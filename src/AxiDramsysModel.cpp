#include "AxiDramsysModel.h"

#include <stdexcept>
#include <utility>

AxiDramsysModel::AxiDramsysModel(std::string name, sc_core::sc_time clk_period)
    : name_(std::move(name))
    , clock_period_(clk_period)
    , step_time_(clk_period)
    , clock_(std::make_unique<sc_core::sc_clock>((name_ + "_clk").c_str(), clock_period_))
    , initiator_(std::make_unique<BlockingInitiator>((name_ + "_initiator").c_str()))
    , dramsys_(std::make_unique<AxiDramsysSystem>((name_ + "_dramsys").c_str())) {
    if (clock_period_ <= sc_core::SC_ZERO_TIME) {
        throw std::invalid_argument("Clock period must be positive");
    }

    initiator_->initiator_socket.bind(dramsys_->axi_target_socket);
    dramsys_->clk_i(*clock_);
}

AxiDramsysModel::~AxiDramsysModel() = default;

void AxiDramsysModel::set_config_path(const std::filesystem::path& path) {
    embedded_config_.reset();
    config_path_ = path;
    if (dramsys_) {
        dramsys_->set_config_path(config_path_);
    }
}

void AxiDramsysModel::set_embedded_config(DRAMSys::Config::EmbeddedConfiguration config) {
    config_path_.clear();
    embedded_config_ = config;
    if (dramsys_) {
        dramsys_->set_embedded_config(config);
    }
}

void AxiDramsysModel::initialize() {
    if (initialized_) {
        return;
    }

    if (embedded_config_) {
        dramsys_->set_embedded_config(*embedded_config_);
    } else {
        if (config_path_.empty()) {
            throw std::runtime_error("Configuration path must be set before initialize()");
        }
        dramsys_->set_config_path(config_path_);
    }
    sc_core::sc_start(sc_core::SC_ZERO_TIME);
    initialized_ = true;
}

axi_helper::AXIResponse AxiDramsysModel::write(const axi_helper::AXIRequest& request,
                                               sc_core::sc_time* latency) {
    auto handle = submit_request(request, /*is_write=*/true);
    wait_for_completion(handle);
    return collect_response(handle, nullptr, latency);
}

axi_helper::AXIResponse AxiDramsysModel::read(axi_helper::AXIRequest& request,
                                              sc_core::sc_time* latency) {
    auto handle = submit_request(request, /*is_write=*/false);
    wait_for_completion(handle);
    return collect_response(handle, &request, latency);
}

auto AxiDramsysModel::post_write(const axi_helper::AXIRequest& request) -> RequestHandle {
    return submit_request(request, /*is_write=*/true);
}

auto AxiDramsysModel::post_read(const axi_helper::AXIRequest& request) -> RequestHandle {
    return submit_request(request, /*is_write=*/false);
}

bool AxiDramsysModel::is_request_done(const RequestHandle& handle) const {
    if (!handle) {
        return false;
    }
    std::scoped_lock lock(handle->mutex);
    return handle->completed;
}

axi_helper::AXIResponse AxiDramsysModel::collect_response(const RequestHandle& handle,
                                                          axi_helper::AXIRequest* out_request,
                                                          sc_core::sc_time* latency) const {
    if (!handle) {
        throw std::invalid_argument("Invalid request handle");
    }

    std::scoped_lock lock(handle->mutex);
    if (!handle->completed) {
        throw std::runtime_error("Request is not completed yet");
    }

    if (out_request) {
        *out_request = handle->request;
    }
    if (latency) {
        *latency = handle->latency;
    }
    return handle->response;
}

void AxiDramsysModel::advance_for(const sc_core::sc_time& duration) {
    if (duration < sc_core::SC_ZERO_TIME) {
        throw std::invalid_argument("advance_for duration must be non-negative");
    }
    if (!initialized_) {
        initialize();
    }
    sc_core::sc_start(duration);
}

void AxiDramsysModel::set_step_time(const sc_core::sc_time& step) {
    if (step <= sc_core::SC_ZERO_TIME) {
        throw std::invalid_argument("Step time must be positive");
    }
    step_time_ = step;
}

AxiDramsysModel::RequestHandle AxiDramsysModel::submit_request(const axi_helper::AXIRequest& request,
                                                               bool is_write) {
    if (!initialized_) {
        initialize();
    }
    return initiator_->enqueue_request(request, is_write);
}

void AxiDramsysModel::wait_for_completion(const RequestHandle& handle) const {
    if (!handle) {
        throw std::invalid_argument("Invalid request handle");
    }

    sc_core::sc_start(sc_core::SC_ZERO_TIME);
    while (true) {
        {
            std::scoped_lock lock(handle->mutex);
            if (handle->completed) {
                break;
            }
        }
        sc_core::sc_start(step_time_);
    }
}

AxiDramsysModel::BlockingInitiator::BlockingInitiator(sc_core::sc_module_name name)
    : sc_core::sc_module(name) {
    initiator_socket(*this);
    payload_ = nullptr;
    status_ = tlm::TLM_INCOMPLETE_RESPONSE;
    SC_THREAD(process_requests);
}

AxiDramsysModel::RequestHandle AxiDramsysModel::BlockingInitiator::enqueue_request(
    const axi_helper::AXIRequest& request, bool is_write) {
    auto handle = std::make_shared<PendingRequest>();
    handle->request = request;
    handle->is_write = is_write;

    {
        std::scoped_lock lock(pending_mutex_);
        pending_.push_back(handle);
    }

    request_event_.notify(sc_core::SC_ZERO_TIME);
    return handle;
}

tlm::tlm_sync_enum AxiDramsysModel::BlockingInitiator::nb_transport_bw(
    axi::axi_protocol_types::tlm_payload_type& trans,
    axi::axi_protocol_types::tlm_phase_type& phase,
    sc_core::sc_time& delay) {
    payload_ = &trans;
    status_ = trans.get_response_status();
    if (phase == tlm::BEGIN_RESP) {
        axi_helper::g_response_event.notify(delay);
        return tlm::TLM_COMPLETED;
    }
    return tlm::TLM_ACCEPTED;
}

template <typename Func>
decltype(auto) AxiDramsysModel::BlockingInitiator::with_response_handler(Func&& func) {
    struct HandlerGuard {
        axi_helper::AXIResponseHandler** slot;
        axi_helper::AXIResponseHandler* previous;
        ~HandlerGuard() { *slot = previous; }
    } guard{&axi_helper::g_response_handler, axi_helper::g_response_handler};

    axi_helper::g_response_handler = this;
    payload_ = nullptr;
    status_ = tlm::TLM_INCOMPLETE_RESPONSE;
    return std::forward<Func>(func)();
}

void AxiDramsysModel::BlockingInitiator::process_requests() {
    while (true) {
        RequestHandle handle;
        {
            std::scoped_lock lock(pending_mutex_);
            if (!pending_.empty()) {
                handle = pending_.front();
                pending_.pop_front();
            }
        }

        if (!handle) {
            wait(request_event_);
            continue;
        }

        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        axi_helper::AXIResponse response;

        if (handle->is_write) {
            response = with_response_handler([&]() {
                return axi_helper::AXIHelper::sendBlockingWrite(initiator_socket, handle->request, delay);
            });
        } else {
            response = with_response_handler([&]() {
                return axi_helper::AXIHelper::sendBlockingRead(initiator_socket, handle->request, delay);
            });
        }

        {
            std::scoped_lock lock(handle->mutex);
            handle->response = response;
            handle->latency = delay;
            handle->completed = true;
        }
    }
}
