#ifndef AXI_DRAMSYS_MODEL_H
#define AXI_DRAMSYS_MODEL_H

#include "AXIHelper.h"
#include "AxiDramsysSystem.h"

#include <axi/axi_tlm.h>
#include <deque>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

/**
 * @brief C++ 友好的 DRAMSys 封装，允许在非 SystemC 顶层中以同步/异步方式
 *        触发 AXI 事务并通过显式调用 sc_start() 驱动仿真时间。
 */
class AxiDramsysModel {
public:
    class PendingRequest;
    using RequestHandle = std::shared_ptr<PendingRequest>;

    /**
     * @param name        内部模块前缀，用于生成唯一的 SystemC 对象名。
     * @param clk_period  默认时钟周期，同时作为阻塞等待时推进仿真的步长。
     */
    explicit AxiDramsysModel(std::string name = "axi_dramsys_model",
                             sc_core::sc_time clk_period = sc_core::sc_time(1, sc_core::SC_NS));
    ~AxiDramsysModel();

    void set_config_path(const std::filesystem::path& path);
    void set_embedded_config(DRAMSys::Config::EmbeddedConfiguration config);
    const std::filesystem::path& get_config_path() const { return config_path_; }
    std::optional<DRAMSys::Config::EmbeddedConfiguration> get_embedded_config() const
    {
        return embedded_config_;
    }

    /**
     * @brief 完成 DRAMSys 实例化并执行零时间启动，必须在发起事务之前调用。
     */
    void initialize();
    bool is_initialized() const { return initialized_; }

    /**
     * @brief 提交并等待写事务完成，内部会在需要时循环调用 sc_start() 推进时间。
     */
    axi_helper::AXIResponse write(const axi_helper::AXIRequest& request,
                                  sc_core::sc_time* latency = nullptr);

    /**
     * @brief 提交并等待读事务完成，返回的数据将写回 request.data。
     */
    axi_helper::AXIResponse read(axi_helper::AXIRequest& request,
                                 sc_core::sc_time* latency = nullptr);

    /**
     * @brief 以异步方式提交写请求，调用者可在外部循环中驱动 sc_start() 并轮询状态。
     */
    RequestHandle post_write(const axi_helper::AXIRequest& request);

    /**
     * @brief 以异步方式提交读请求，完成后可通过 collect_response() 获取数据。
     */
    RequestHandle post_read(const axi_helper::AXIRequest& request);

    /**
     * @brief 查询异步请求是否已经完成。
     */
    bool is_request_done(const RequestHandle& handle) const;

    /**
     * @brief 获取异步请求的响应信息，若提供 out_request 将一并返回更新后的请求。
     * @throws std::runtime_error 若请求尚未完成。
     */
    axi_helper::AXIResponse collect_response(const RequestHandle& handle,
                                             axi_helper::AXIRequest* out_request = nullptr,
                                             sc_core::sc_time* latency = nullptr) const;

    /**
     * @brief 手动推进仿真一段时间，便于在外部统一驱动 SystemC。
     */
    void advance_for(const sc_core::sc_time& duration);
    void advance_cycle() { advance_for(step_time_); }
    void advance_cycles(unsigned cycles) { advance_for(step_time_ * cycles); }

    /**
     * @brief 设置阻塞等待时用于自动推进仿真的步长，默认等于时钟周期。
     */
    void set_step_time(const sc_core::sc_time& step);
    sc_core::sc_time get_step_time() const { return step_time_; }

private:
    class BlockingInitiator;

    RequestHandle submit_request(const axi_helper::AXIRequest& request, bool is_write);
    void wait_for_completion(const RequestHandle& handle) const;

    std::string name_;
    sc_core::sc_time clock_period_;
    sc_core::sc_time step_time_;
    std::filesystem::path config_path_{};
    std::optional<DRAMSys::Config::EmbeddedConfiguration> embedded_config_{};
    bool initialized_{false};

    std::unique_ptr<sc_core::sc_clock> clock_;
    std::unique_ptr<BlockingInitiator> initiator_;
    std::unique_ptr<AxiDramsysSystem> dramsys_;

    // -------- 内部请求结构体定义 --------
    // -------- AXI 主设备模块，用于在 SystemC 线程中执行阻塞事务 --------
    class BlockingInitiator : public sc_core::sc_module,
                              public axi::axi_bw_transport_if<axi::axi_protocol_types>,
                              public axi_helper::AXIResponseHandler {
    public:
        axi::axi_initiator_socket<1024> initiator_socket{"initiator_socket"};

        SC_HAS_PROCESS(BlockingInitiator);
        explicit BlockingInitiator(sc_core::sc_module_name name);

        RequestHandle enqueue_request(const axi_helper::AXIRequest& request, bool is_write);

        // axi_bw_transport_if
        tlm::tlm_sync_enum nb_transport_bw(axi::axi_protocol_types::tlm_payload_type& trans,
                                           axi::axi_protocol_types::tlm_phase_type& phase,
                                           sc_core::sc_time& delay) override;
        void invalidate_direct_mem_ptr(sc_dt::uint64, sc_dt::uint64) override {}

    private:
        template <typename Func>
        decltype(auto) with_response_handler(Func&& func);
        void process_requests();

        sc_core::sc_event request_event_{"request_event"};
        std::deque<RequestHandle> pending_{};
        mutable std::mutex pending_mutex_;
    };
};

class AxiDramsysModel::PendingRequest {
    friend class AxiDramsysModel;
    friend class AxiDramsysModel::BlockingInitiator;

public:
    PendingRequest() = default;

private:
    axi_helper::AXIRequest request;
    axi_helper::AXIResponse response;
    sc_core::sc_time latency{sc_core::SC_ZERO_TIME};
    bool is_write{false};
    bool completed{false};
    mutable std::mutex mutex;
};

#endif // AXI_DRAMSYS_MODEL_H
