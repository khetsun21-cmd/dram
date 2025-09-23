#pragma once

#include "AXIHelper.h"

#include <axi/axi_tlm.h>
#include <systemc>
#include <tlm>
#include <utility>

class TestAXIMaster : public sc_core::sc_module,
                      public axi::axi_bw_transport_if<axi::axi_protocol_types>,
                      public axi_helper::AXIResponseHandler {
public:
    axi::axi_initiator_socket<1024> initiator_socket{"initiator_socket"};

    SC_HAS_PROCESS(TestAXIMaster);

    explicit TestAXIMaster(sc_core::sc_module_name name)
        : sc_core::sc_module(name) {
        initiator_socket(*this);
        payload_ = nullptr;
        status_ = tlm::TLM_INCOMPLETE_RESPONSE;
    }

    template <typename Func>
    decltype(auto) with_response_handler(Func&& func) {
        axi_helper::g_response_handler = this;
        payload_ = nullptr;
        status_ = tlm::TLM_INCOMPLETE_RESPONSE;
        return std::forward<Func>(func)();
    }

    tlm::tlm_sync_enum nb_transport_bw(axi::axi_protocol_types::tlm_payload_type& trans,
                                       axi::axi_protocol_types::tlm_phase_type& phase,
                                       sc_core::sc_time& delay) override {
        payload_ = &trans;
        status_ = trans.get_response_status();
        if (phase == tlm::BEGIN_RESP) {
            axi_helper::g_response_event.notify(delay);
            return tlm::TLM_COMPLETED;
        }
        return tlm::TLM_ACCEPTED;
    }

    void invalidate_direct_mem_ptr(sc_dt::uint64, sc_dt::uint64) override {}
};
