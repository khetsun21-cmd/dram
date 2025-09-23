#include "AxiDramsysSystem.h"
#include "TestAXIMaster.h"

#include <systemc>

#include <filesystem>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <sstream>
#include <vector>

namespace {

class BridgeSmokeBench : public sc_core::sc_module {
public:
    TestAXIMaster master{"master"};
    AxiDramsysSystem dramsys{"dramsys"};

    SC_HAS_PROCESS(BridgeSmokeBench);

    BridgeSmokeBench(sc_core::sc_module_name name, std::filesystem::path config_path)
        : sc_core::sc_module(name)
        , config_path_(std::move(config_path)) {
        dramsys.set_config_path(config_path_);
        master.initiator_socket.bind(dramsys.axi_target_socket);
        SC_THREAD(run);
    }

private:
    void run() {
        wait(sc_core::SC_ZERO_TIME);

        const sc_dt::uint64 base_address = 0x1000;
        std::vector<unsigned char> pattern(32);
        std::iota(pattern.begin(), pattern.end(), static_cast<unsigned char>(0xA0));

        sc_core::sc_time write_delay = sc_core::SC_ZERO_TIME;
        axi_helper::AXIRequest write_req(base_address, pattern.size());
        write_req.data = pattern;
        auto write_resp = master.with_response_handler([&]() {
            return axi_helper::AXIHelper::sendBlockingWrite(master.initiator_socket, write_req, write_delay);
        });
        if (!write_resp.success) {
            SC_REPORT_FATAL("bridge_test", "AXI write request failed");
        }

        sc_core::sc_time read_delay = sc_core::SC_ZERO_TIME;
        axi_helper::AXIRequest read_req(base_address, pattern.size());
        auto read_resp = master.with_response_handler([&]() {
            return axi_helper::AXIHelper::sendBlockingRead(master.initiator_socket, read_req, read_delay);
        });
        if (!read_resp.success) {
            SC_REPORT_FATAL("bridge_test", "AXI read request failed");
        }

        if (read_req.data != pattern) {
            std::ostringstream oss;
            oss << "Readback mismatch at 0x" << std::hex << base_address;
            SC_REPORT_FATAL("bridge_test", oss.str().c_str());
        }

        std::ostringstream oss;
        oss << "Read back " << read_req.data.size() << " bytes from 0x" << std::hex << base_address << std::dec << ": ";
        oss << std::hex << std::setfill('0');
        for (unsigned char byte : read_req.data) {
            oss << std::setw(2) << static_cast<int>(byte) << ' ';
        }
        SC_REPORT_INFO("bridge_test", oss.str().c_str());

        sc_core::sc_stop();
    }

    std::filesystem::path config_path_;
};

} // namespace

int sc_main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    std::filesystem::path config = std::filesystem::path(SRC_ROOT_DIR) / "src" / "DRAMSys" / "configs" / "lpddr4-example.json";
    if (!std::filesystem::exists(config)) {
        std::cerr << "Configuration file not found: " << config << std::endl;
        return 1;
    }

    BridgeSmokeBench tb("tb", config);
    sc_core::sc_start();
    return 0;
}
