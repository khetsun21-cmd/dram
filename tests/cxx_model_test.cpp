#include "AxiDramsysModel.h"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <numeric>

namespace {

std::vector<unsigned char> make_pattern(unsigned base, std::size_t length) {
    std::vector<unsigned char> data(length);
    std::iota(data.begin(), data.end(), static_cast<unsigned char>(base));
    return data;
}

void dump_bytes(const std::string& tag, const std::vector<unsigned char>& data) {
    std::cout << tag << " (" << data.size() << " bytes): ";
    std::cout << std::hex << std::setfill('0');
    for (unsigned char byte : data) {
        std::cout << std::setw(2) << static_cast<unsigned>(byte) << ' ';
    }
    std::cout << std::dec << '\n';
}

bool check_success(const axi_helper::AXIResponse& resp, const char* what) {
    if (!resp.success) {
        std::cerr << what << " failed: TLM status=" << resp.status << '\n';
        return false;
    }
    return true;
}

} // namespace

int main() {
    AxiDramsysModel model{"cxx_model"};
    model.set_embedded_config(DRAMSys::Config::EmbeddedConfiguration::Lpddr4);
    model.initialize();

    const sc_dt::uint64 base_address = 0x2000;
    auto pattern = make_pattern(0x10, 64);

    axi_helper::AXIRequest write_req(base_address, pattern.size());
    write_req.data = pattern;
    sc_core::sc_time write_latency = sc_core::SC_ZERO_TIME;
    auto write_resp = model.write(write_req, &write_latency);
    if (!check_success(write_resp, "Blocking write")) {
        return 1;
    }

    axi_helper::AXIRequest read_req(base_address, pattern.size());
    sc_core::sc_time read_latency = sc_core::SC_ZERO_TIME;
    auto read_resp = model.read(read_req, &read_latency);
    if (!check_success(read_resp, "Blocking read")) {
        return 1;
    }

    if (read_req.data != pattern) {
        std::cerr << "Blocking readback mismatch" << '\n';
        dump_bytes("expected", pattern);
        dump_bytes("actual", read_req.data);
        return 1;
    }

    // Demonstrate asynchronous workflow with manual stepping.
    const sc_dt::uint64 async_addr = base_address + 0x100;
    auto async_pattern = make_pattern(0x80, 32);

    axi_helper::AXIRequest async_write(async_addr, async_pattern.size());
    async_write.data = async_pattern;
    auto write_handle = model.post_write(async_write);

    while (!model.is_request_done(write_handle)) {
        model.advance_cycle();
    }
    sc_core::sc_time async_write_latency = sc_core::SC_ZERO_TIME;
    auto async_write_resp = model.collect_response(write_handle, nullptr, &async_write_latency);
    if (!check_success(async_write_resp, "Async write")) {
        return 1;
    }

    axi_helper::AXIRequest async_read(async_addr, async_pattern.size());
    auto read_handle = model.post_read(async_read);
    while (!model.is_request_done(read_handle)) {
        model.advance_cycles(4);
    }

    sc_core::sc_time async_read_latency = sc_core::SC_ZERO_TIME;
    auto async_read_resp = model.collect_response(read_handle, &async_read, &async_read_latency);
    if (!check_success(async_read_resp, "Async read")) {
        return 1;
    }

    if (async_read.data != async_pattern) {
        std::cerr << "Async readback mismatch" << '\n';
        dump_bytes("expected", async_pattern);
        dump_bytes("actual", async_read.data);
        return 1;
    }

    std::cout << "All C++ model transactions completed successfully." << std::endl;
    return 0;
}
