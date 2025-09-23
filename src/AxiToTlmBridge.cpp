#include "AxiToTlmBridge.h"

#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cassert>

AxiToTlmBridge::AxiToTlmBridge(sc_core::sc_module_name name, std::size_t data_width_bytes)
    : sc_module(name)
    , axi_target_socket("axi_target_socket")
    , tlm_initiator_socket("tlm_initiator_socket")
    , data_width_bytes_(data_width_bytes) {
    // Bind forward transport interface for AXI target socket (no scc dependency)
    axi_target_socket.bind(*this);
    // Bind backward interface for downstream target (required by initiator socket)
    tlm_initiator_socket(*this);
    // Provide default clock binding to avoid unbound port error
    clk_i(clk_gen_);
    // Worker for nb_transport requests
    SC_THREAD(process_axi_reqs);
}

void AxiToTlmBridge::process_axi_reqs() {
    while (true) {
        auto* gp = req_fifo_.read();
        assert(gp != nullptr);

        // Accept any AXI burst type; if extensions are absent or burst is FIXED/WRAP, fall back to data_length
        std::size_t total_bytes = gp->get_data_length();
        if (total_bytes == 0) {
            // As a fallback, try derive from AXI helpers (if extension present)
            auto total_beats = axi::get_burst_length(*gp);
            auto beat_bytes = axi::get_burst_size(*gp);
            total_bytes = static_cast<std::size_t>(total_beats) * static_cast<std::size_t>(beat_bytes);
        }

        const auto base_addr = gp->get_address();
        auto* base_ptr = gp->get_data_ptr();

        if (verbose_) {
            std::ostringstream oss;
            oss << "AXI REQ(cmd=" << (gp->is_write() ? "W" : "R")
                << ") addr=0x" << std::hex << base_addr << std::dec
                << " total=" << total_bytes
                << " at " << sc_core::sc_time_stamp();

            // Print AXI extension details if available
            auto* ext = gp->get_extension<axi::axi4_extension>();
            if (ext) {
                oss << " id=" << ext->get_id()
                    << " burst=" << static_cast<int>(ext->get_burst())
                    << " size=" << static_cast<int>(ext->get_size())
                    << " len=" << static_cast<int>(ext->get_length())
                    << " cache=" << static_cast<int>(ext->get_cache())
                    << " prot=" << static_cast<int>(ext->get_prot());
            }
            SC_REPORT_INFO("AxiToTlmBridge", oss.str().c_str());
        }

        if (get_base_latency() != sc_core::SC_ZERO_TIME) wait(get_base_latency());

        const std::size_t dr_beat = std::max<std::size_t>(1, get_downstream_beat_bytes());
        std::size_t done = 0;
        while (done < total_bytes) {
            const auto chunk = std::min<std::size_t>(dr_beat, total_bytes - done);

            // Create persistent sub-transaction for AT interaction downstream (with memory manager)
            auto* sub = mm_.allocate();
            sub->deep_copy_from(*gp);
            sub->set_address(base_addr + done);
            sub->set_data_ptr(base_ptr ? base_ptr + done : nullptr);
            sub->set_data_length(chunk);
            sub->set_streaming_width(chunk);
            sub->set_byte_enable_ptr(nullptr);
            sub->set_dmi_allowed(false);
            sub->set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

            // Register event and start AT request
            auto* ev = new sc_core::sc_event();
            pending_resp_[sub] = ev;
            tlm::tlm_phase sub_ph = tlm::BEGIN_REQ;
            sc_core::sc_time sub_dly = sc_core::SC_ZERO_TIME;
            auto stat = tlm_initiator_socket->nb_transport_fw(*sub, sub_ph, sub_dly);
            (void)stat;

            // Wait until BEGIN_RESP observed in nb_transport_bw
            wait(*ev);

            if (get_beat_latency() != sc_core::SC_ZERO_TIME) wait(get_beat_latency());

            if (!sub->is_response_ok()) {
                gp->set_response_status(sub->get_response_status());
                // cleanup
                pending_resp_.erase(sub);
                delete ev;
                sub->release();
                break;
            }

            // cleanup
            pending_resp_.erase(sub);
            delete ev;
            sub->release();

            done += chunk;
        }

        if (done == total_bytes) gp->set_response_status(tlm::TLM_OK_RESPONSE);

        // Notify initiator via backward path (BEGIN_RESP)
        tlm::tlm_phase ph = tlm::BEGIN_RESP;
        sc_core::sc_time bw_delay = sc_core::SC_ZERO_TIME;
        (void)axi_target_socket->nb_transport_bw(*gp, ph, bw_delay);
    }
}

// Blocking path: segment and forward synchronously
void AxiToTlmBridge::b_transport(payload_type& trans, sc_core::sc_time& delay) {
    // Accept any burst; prefer data_length for total bytes
    std::size_t total_bytes = trans.get_data_length();
    if (total_bytes == 0) {
        auto total_beats = axi::get_burst_length(trans);
        auto beat_bytes = axi::get_burst_size(trans);
        total_bytes = static_cast<std::size_t>(total_beats) * static_cast<std::size_t>(beat_bytes);
    }

    const auto base_addr = trans.get_address();
    auto* base_ptr = trans.get_data_ptr();

    if (get_base_latency() != sc_core::SC_ZERO_TIME) delay += get_base_latency();

    const std::size_t dr_beat = std::max<std::size_t>(1, get_downstream_beat_bytes());
    std::size_t done = 0;
    while (done < total_bytes) {
        const auto chunk = std::min<std::size_t>(dr_beat, total_bytes - done);

        tlm::tlm_generic_payload sub;
        sub.deep_copy_from(trans);
        sub.set_address(base_addr + done);
        sub.set_data_ptr(base_ptr ? base_ptr + done : nullptr);
        sub.set_data_length(chunk);
        sub.set_streaming_width(chunk);
        sub.set_byte_enable_ptr(nullptr);
        sub.set_dmi_allowed(false);
        sub.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

        sc_core::sc_time dly = sc_core::SC_ZERO_TIME;
        tlm_initiator_socket->b_transport(sub, dly);
        if (get_beat_latency() != sc_core::SC_ZERO_TIME) dly += get_beat_latency();
        delay += dly;

        if (!sub.is_response_ok()) {
            trans.set_response_status(sub.get_response_status());
            return;
        }
        done += chunk;
    }
    trans.set_response_status(tlm::TLM_OK_RESPONSE);
}

// Non-blocking path: accept BEGIN_REQ and process in worker, then send BEGIN_RESP
tlm::tlm_sync_enum AxiToTlmBridge::nb_transport_fw(payload_type& trans, phase_type& phase, sc_core::sc_time& delay) {
    if (phase == tlm::BEGIN_REQ) {
        // Accept request, enqueue for worker, and immediately ACK with END_REQ
        (void)delay; // model in worker
        req_fifo_.write(&trans);
        phase = tlm::END_REQ;
        return tlm::TLM_UPDATED;
    }
    return tlm::TLM_COMPLETED;
}

// Minimal BW callback implementation (downstream may call it in AT mode)
tlm::tlm_sync_enum AxiToTlmBridge::nb_transport_bw(tlm::tlm_generic_payload& trans,
                                                   tlm::tlm_phase& phase,
                                                   sc_core::sc_time& delay) {
    // Handle downstream response: notify waiting thread and ack END_RESP
    if (phase == tlm::BEGIN_RESP) {
        if (verbose_) {
            std::ostringstream oss;
            oss << "AXI RESP(cmd=" << (trans.is_write() ? "W" : "R")
                << ") at " << sc_core::sc_time_stamp();
            SC_REPORT_INFO("AxiToTlmBridge", oss.str().c_str());
        }
        auto it = pending_resp_.find(&trans);
        if (it != pending_resp_.end() && it->second) {
            it->second->notify(delay);
        }
        phase = tlm::END_RESP;
        return tlm::TLM_UPDATED;
    }
    return tlm::TLM_ACCEPTED;
}
