#ifndef AXI_TO_TLM_BRIDGE_H
#define AXI_TO_TLM_BRIDGE_H

#include <systemc>
#include <tlm>
#include <cstddef>
#include <deque>
#include <vector>
#include <axi/axi_tlm.h>
#include <unordered_map>

class AxiToTlmBridge : public sc_core::sc_module,
                       public axi::axi_fw_transport_if<axi::axi_protocol_types>,
                       public tlm::tlm_bw_transport_if<> {
public:
    // AXI target socket (AT-style, 1024-bit = 128B)
    axi::axi_target_socket<1024, axi::axi_protocol_types, 1, sc_core::SC_ZERO_OR_MORE_BOUND> axi_target_socket;

    // Downstream TLM initiator socket (towards memory/DRAMSys)
    tlm::tlm_initiator_socket<> tlm_initiator_socket;

    SC_HAS_PROCESS(AxiToTlmBridge);
    explicit AxiToTlmBridge(sc_core::sc_module_name name, std::size_t data_width_bytes = 128);

    // Optional clock port (unused internally; provided for compatibility)
    sc_core::sc_in<bool> clk_i{"clk_i"};

    // Configuration helpers
    std::size_t get_data_width_bytes() const { return data_width_bytes_; }
    void set_verbose(bool v) { verbose_ = v; }
    bool get_verbose() const { return verbose_; }
    void set_base_latency(const sc_core::sc_time& t) { base_latency_ = t; }
    void set_beat_latency(const sc_core::sc_time& t) { beat_latency_ = t; }
    sc_core::sc_time get_base_latency() const { return base_latency_; }
    sc_core::sc_time get_beat_latency() const { return beat_latency_; }
    void set_dump_bytes(std::size_t n) { dump_bytes_ = n; }
    std::size_t get_dump_bytes() const { return dump_bytes_; }
    void set_downstream_beat_bytes(std::size_t n) { downstream_beat_bytes_ = n; }
    std::size_t get_downstream_beat_bytes() const { return downstream_beat_bytes_; }

private:
    using payload_type = axi::axi_protocol_types::tlm_payload_type;
    using phase_type = axi::axi_protocol_types::tlm_phase_type;

    // Simple memory manager for nb_transport payloads
    struct SimpleMM : tlm::tlm_mm_interface {
        tlm::tlm_generic_payload* allocate() {
            auto* p = new tlm::tlm_generic_payload();
            p->set_mm(this);
            p->acquire();
            return p;
        }
        void free(tlm::tlm_generic_payload* trans) override { delete trans; }
    } mm_;

    std::size_t data_width_bytes_{};
    bool verbose_{true};
    sc_core::sc_time base_latency_{sc_core::SC_ZERO_TIME};
    sc_core::sc_time beat_latency_{sc_core::SC_ZERO_TIME};
    std::size_t dump_bytes_{128};
    std::size_t downstream_beat_bytes_{32}; // e.g. 32B per DRAM beat

    // Internal default clock (if clk_i not bound)
    sc_core::sc_clock clk_gen_{"bridge_clk", sc_core::sc_time(1, sc_core::SC_NS)};
    // Queue to process nb_transport requests
    sc_core::sc_fifo<payload_type*> req_fifo_{16};
    void process_axi_reqs();

    // tlm_fw_transport_if implementation
    void b_transport(payload_type& trans, sc_core::sc_time& delay) override;
    tlm::tlm_sync_enum nb_transport_fw(payload_type& trans, phase_type& phase, sc_core::sc_time& delay) override;
    bool get_direct_mem_ptr(payload_type& trans, tlm::tlm_dmi& dmi) override { (void)trans; (void)dmi; return false; }
    unsigned int transport_dbg(payload_type& trans) override { (void)trans; return 0; }

    // tlm_bw_transport_if implementation for downstream target callbacks
    tlm::tlm_sync_enum nb_transport_bw(tlm::tlm_generic_payload& trans,
                                       tlm::tlm_phase& phase,
                                       sc_core::sc_time& delay) override;
    void invalidate_direct_mem_ptr(sc_dt::uint64, sc_dt::uint64) override {}

    // Track outstanding downstream AT transactions
    std::unordered_map<tlm::tlm_generic_payload*, sc_core::sc_event*> pending_resp_{};
};

#endif // AXI_TO_TLM_BRIDGE_H
