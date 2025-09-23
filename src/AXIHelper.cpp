#include "AXIHelper.h"
#include <cstring>
#include <algorithm>

namespace axi_helper {

// 定义全局变量
sc_core::sc_event g_response_event;
AXIResponseHandler* g_response_handler = nullptr;

// 静态内存管理器
class SimpleMemoryManager : public tlm::tlm_mm_interface {
public:
    SimpleMemoryManager() = default;

    tlm::tlm_generic_payload* allocate() {
        tlm::tlm_generic_payload* p = new tlm::tlm_generic_payload();
        p->set_mm(this);
        p->acquire();
        return p;
    }

    void free(tlm::tlm_generic_payload* trans) override {
        delete trans;
    }
};

static SimpleMemoryManager g_memory_manager;

tlm::tlm_mm_interface* AXIHelper::getMemoryManager() {
    return &g_memory_manager;
}

// AXI事务构建器实现
AXITransactionBuilder::AXITransactionBuilder() : request_() {}

AXITransactionBuilder& AXITransactionBuilder::setAddress(sc_dt::uint64 addr) {
    request_.address = addr;
    return *this;
}

AXITransactionBuilder& AXITransactionBuilder::setDataLength(std::size_t len) {
    request_.data_length = len;
    request_.data.resize(len);
    return *this;
}

AXITransactionBuilder& AXITransactionBuilder::setID(unsigned int id) {
    request_.id = id;
    return *this;
}

AXITransactionBuilder& AXITransactionBuilder::setData(const std::vector<unsigned char>& data) {
    request_.data = data;
    request_.data_length = data.size();
    return *this;
}

AXITransactionBuilder& AXITransactionBuilder::setData(const unsigned char* data, std::size_t len) {
    request_.data.assign(data, data + len);
    request_.data_length = len;
    return *this;
}

AXITransactionBuilder& AXITransactionBuilder::setBurstType(axi::burst_e type) {
    request_.burst_type = type;
    return *this;
}

AXITransactionBuilder& AXITransactionBuilder::setBurstLength(unsigned int len) {
    request_.burst_length = len;
    return *this;
}

AXITransactionBuilder& AXITransactionBuilder::setBurstSize(unsigned int size) {
    request_.burst_size = size;
    return *this;
}

AXITransactionBuilder& AXITransactionBuilder::setCache(unsigned int cache) {
    request_.cache = cache;
    return *this;
}

AXITransactionBuilder& AXITransactionBuilder::setProt(unsigned int prot) {
    request_.prot = prot;
    return *this;
}

AXITransactionBuilder& AXITransactionBuilder::setQoS(unsigned int qos) {
    request_.qos = qos;
    return *this;
}

AXITransactionBuilder& AXITransactionBuilder::setRegion(unsigned int region) {
    request_.region = region;
    return *this;
}

AXIRequest AXITransactionBuilder::build() const {
    return request_;
}

AXIResponse AXITransactionBuilder::sendBlockingWrite(axi::axi_initiator_socket<1024>& socket, sc_core::sc_time& delay) {
    return AXIHelper::sendBlockingWrite(socket, request_, delay);
}

AXIResponse AXITransactionBuilder::sendBlockingRead(axi::axi_initiator_socket<1024>& socket, sc_core::sc_time& delay) {
    return AXIHelper::sendBlockingRead(socket, request_, delay);
}

// AXI Helper类实现
tlm::tlm_generic_payload* AXIHelper::createWritePayload(const AXIRequest& req) {
    tlm::tlm_generic_payload* payload = static_cast<SimpleMemoryManager*>(getMemoryManager())->allocate();
    payload->set_command(tlm::TLM_WRITE_COMMAND);
    payload->set_address(req.address);
    // 为写操作创建数据副本，因为payload需要non-const指针
    unsigned char* data_copy = new unsigned char[req.data_length];
    std::memcpy(data_copy, req.data.data(), req.data_length);
    payload->set_data_ptr(data_copy);
    payload->set_data_length(req.data_length);
    payload->set_streaming_width(req.data_length);
    payload->set_byte_enable_ptr(nullptr);
    payload->set_dmi_allowed(false);
    payload->set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    setupAXIExtension(payload, req);
    return payload;
}

tlm::tlm_generic_payload* AXIHelper::createReadPayload(const AXIRequest& req) {
    tlm::tlm_generic_payload* payload = static_cast<SimpleMemoryManager*>(getMemoryManager())->allocate();
    payload->set_command(tlm::TLM_READ_COMMAND);
    payload->set_address(req.address);
    // 为读操作分配缓冲区
    unsigned char* data_buffer = new unsigned char[req.data_length];
    payload->set_data_ptr(data_buffer);
    payload->set_data_length(req.data_length);
    payload->set_streaming_width(req.data_length);
    payload->set_byte_enable_ptr(nullptr);
    payload->set_dmi_allowed(false);
    payload->set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    setupAXIExtension(payload, req);
    return payload;
}

void AXIHelper::setupAXIExtension(tlm::tlm_generic_payload* payload, const AXIRequest& req) {
    auto* ext = new axi::axi4_extension();
    ext->set_id(req.id);
    ext->set_length(static_cast<uint8_t>(req.burst_length - 1)); // AXI length = burst_length - 1
    ext->set_size(static_cast<uint8_t>(req.burst_size));
    ext->set_burst(req.burst_type);
    ext->set_cache(static_cast<uint8_t>(req.cache));
    ext->set_prot(static_cast<uint8_t>(req.prot));
    ext->set_qos(static_cast<uint8_t>(req.qos));
    ext->set_region(static_cast<uint8_t>(req.region));

    payload->set_extension(ext);
}

AXIResponse AXIHelper::extractResponse(const tlm::tlm_generic_payload* payload) {
    AXIResponse response;
    response.status = payload->get_response_status();
    response.success = payload->is_response_ok();

    // 提取AXI响应
    auto* ext = payload->get_extension<axi::axi4_extension>();
    if (ext) {
        response.axi_resp = ext->get_resp();
    }

    return response;
}

AXIResponse AXIHelper::sendBlockingWrite(axi::axi_initiator_socket<1024>& socket,
                                       const AXIRequest& req,
                                       sc_core::sc_time& delay) {
    auto* payload = createWritePayload(req);
    tlm::tlm_phase phase = tlm::BEGIN_REQ;
    sc_core::sc_time transport_delay = sc_core::SC_ZERO_TIME;

    // 使用非阻塞传输
    auto result = socket->nb_transport_fw(*payload, phase, transport_delay);

    if (result == tlm::TLM_UPDATED && phase == tlm::END_REQ) {
        // 等待响应
        sc_core::wait(g_response_event);
        AXIResponse response = extractResponse(g_response_handler->payload_);
        response.latency = transport_delay;
        delay += transport_delay;
        releasePayload(payload);
        return response;
    }

    // 如果传输未完成，返回错误
    AXIResponse response;
    response.success = false;
    response.status = tlm::TLM_GENERIC_ERROR_RESPONSE;
    releasePayload(payload);
    return response;
}

AXIResponse AXIHelper::sendBlockingRead(axi::axi_initiator_socket<1024>& socket,
                                      AXIRequest& req,
                                      sc_core::sc_time& delay) {
    auto* payload = createReadPayload(req);
    tlm::tlm_phase phase = tlm::BEGIN_REQ;
    sc_core::sc_time transport_delay = sc_core::SC_ZERO_TIME;

    // 使用非阻塞传输
    auto result = socket->nb_transport_fw(*payload, phase, transport_delay);

    if (result == tlm::TLM_UPDATED && phase == tlm::END_REQ) {
        // 等待响应
        sc_core::wait(g_response_event);
        AXIResponse response = extractResponse(g_response_handler->payload_);
        response.latency = transport_delay;
        delay += transport_delay;

        // 如果读取成功，复制数据到请求结构体
        if (response.success && g_response_handler->payload_->get_data_ptr()) {
            req.data.assign(g_response_handler->payload_->get_data_ptr(),
                           g_response_handler->payload_->get_data_ptr() + g_response_handler->payload_->get_data_length());
        }

        releasePayload(payload);
        return response;
    }

    // 如果传输未完成，返回错误
    AXIResponse response;
    response.success = false;
    response.status = tlm::TLM_GENERIC_ERROR_RESPONSE;
    releasePayload(payload);
    return response;
}

tlm::tlm_sync_enum AXIHelper::sendNonBlockingWrite(
    axi::axi_initiator_socket<1024>& socket,
    const AXIRequest& req,
    sc_core::sc_time& delay,
    std::function<void(const AXIResponse&)> callback) {
    // 非阻塞实现需要更复杂的回调机制，这里提供基本框架
    auto* payload = createWritePayload(req);
    tlm::tlm_phase phase = tlm::BEGIN_REQ;
    auto result = socket->nb_transport_fw(*payload, phase, delay);

    // 注意：实际的非阻塞实现需要配合bw_transport_if来处理响应
    // 这里只是基本框架，完整实现需要额外的回调管理

    if (callback) {
        // 简化处理，实际应该在响应到达时调用
        AXIResponse response = extractResponse(payload);
        callback(response);
    }

    releasePayload(payload);
    return result;
}

tlm::tlm_sync_enum AXIHelper::sendNonBlockingRead(
    axi::axi_initiator_socket<1024>& socket,
    AXIRequest& req,
    sc_core::sc_time& delay,
    std::function<void(const AXIResponse&)> callback) {
    auto* payload = createReadPayload(req);
    tlm::tlm_phase phase = tlm::BEGIN_REQ;
    auto result = socket->nb_transport_fw(*payload, phase, delay);

    // 类似write的非阻塞处理
    if (callback) {
        AXIResponse response = extractResponse(payload);
        if (response.success && payload->get_data_ptr()) {
            req.data.assign(payload->get_data_ptr(),
                           payload->get_data_ptr() + payload->get_data_length());
        }
        callback(response);
    }

    releasePayload(payload);
    return result;
}

bool AXIHelper::write(axi::axi_initiator_socket<1024>& socket,
                     sc_dt::uint64 address,
                     const unsigned char* data,
                     std::size_t length,
                     unsigned int id,
                     sc_core::sc_time* delay) {
    sc_core::sc_time local_delay = delay ? *delay : sc_core::SC_ZERO_TIME;

    AXIRequest req(address, length, id);
    req.data.assign(data, data + length);

    AXIResponse response = sendBlockingWrite(socket, req, local_delay);

    if (delay) {
        *delay = local_delay;
    }

    return response.success;
}

bool AXIHelper::read(axi::axi_initiator_socket<1024>& socket,
                    sc_dt::uint64 address,
                    unsigned char* data,
                    std::size_t length,
                    unsigned int id,
                    sc_core::sc_time* delay) {
    sc_core::sc_time local_delay = delay ? *delay : sc_core::SC_ZERO_TIME;

    AXIRequest req(address, length, id);
    AXIResponse response = sendBlockingRead(socket, req, local_delay);

    if (response.success && !req.data.empty()) {
        std::memcpy(data, req.data.data(), std::min(length, req.data.size()));
    }

    if (delay) {
        *delay = local_delay;
    }

    return response.success;
}

bool AXIHelper::writeVector(axi::axi_initiator_socket<1024>& socket,
                           sc_dt::uint64 address,
                           const std::vector<unsigned char>& data,
                           unsigned int id,
                           sc_core::sc_time* delay) {
    return write(socket, address, data.data(), data.size(), id, delay);
}

bool AXIHelper::readVector(axi::axi_initiator_socket<1024>& socket,
                          sc_dt::uint64 address,
                          std::vector<unsigned char>& data,
                          std::size_t length,
                          unsigned int id,
                          sc_core::sc_time* delay) {
    sc_core::sc_time local_delay = delay ? *delay : sc_core::SC_ZERO_TIME;

    AXIRequest req(address, length, id);
    AXIResponse response = sendBlockingRead(socket, req, local_delay);

    if (response.success) {
        data = req.data;
    }

    if (delay) {
        *delay = local_delay;
    }

    return response.success;
}

bool AXIHelper::writeString(axi::axi_initiator_socket<1024>& socket,
                           sc_dt::uint64 address,
                           const std::string& str,
                           unsigned int id,
                           sc_core::sc_time* delay) {
    const unsigned char* data = reinterpret_cast<const unsigned char*>(str.c_str());
    std::size_t length = str.size() + 1; // 包含null终止符
    return write(socket, address, data, length, id, delay);
}

bool AXIHelper::readString(axi::axi_initiator_socket<1024>& socket,
                          sc_dt::uint64 address,
                          std::string& str,
                          std::size_t max_length,
                          unsigned int id,
                          sc_core::sc_time* delay) {
    sc_core::sc_time local_delay = delay ? *delay : sc_core::SC_ZERO_TIME;

    AXIRequest req(address, max_length, id);
    AXIResponse response = sendBlockingRead(socket, req, local_delay);

    if (response.success && !req.data.empty()) {
        // 查找null终止符
        auto null_pos = std::find(req.data.begin(), req.data.end(), '\0');
        std::size_t len = (null_pos != req.data.end()) ?
                         (null_pos - req.data.begin()) :
                         req.data.size();
        str.assign(reinterpret_cast<const char*>(req.data.data()), len);
    }

    if (delay) {
        *delay = local_delay;
    }

    return response.success;
}

void AXIHelper::releasePayload(tlm::tlm_generic_payload* payload) {
    if (payload) {
        // 清理扩展
        auto* ext = payload->get_extension<axi::axi4_extension>();
        if (ext) {
            payload->clear_extension(ext);
            delete ext;
        }

        payload->release();
    }
}

} // namespace axi_helper
