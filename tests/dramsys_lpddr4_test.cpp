#include "AxiDramsysSystem.h"
#include "TestAXIMaster.h"

#include <systemc>

#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {

class DramsysLpddr4Bench : public sc_core::sc_module {
public:
    TestAXIMaster master{"master"};
    AxiDramsysSystem dramsys{"dramsys"};

    SC_HAS_PROCESS(DramsysLpddr4Bench);

    DramsysLpddr4Bench(sc_core::sc_module_name name, std::filesystem::path config_path)
        : sc_core::sc_module(name)
        , config_path_(std::move(config_path)) {
        dramsys.set_config_path(config_path_);
        master.initiator_socket.bind(dramsys.axi_target_socket);
        SC_THREAD(run);
    }

private:
    void run() {
        wait(sc_core::SC_ZERO_TIME);

        perform_roundtrip(0x2000, 64, 0x10);
        perform_roundtrip(0x4000, 96, 0x30);
        perform_string_roundtrip(0x6000, "DRAMSys AXI bridge test payload");

        sc_core::sc_stop();
    }

    void perform_roundtrip(sc_dt::uint64 address, std::size_t length, unsigned char start_value) {
        std::vector<unsigned char> pattern(length);
        for (std::size_t i = 0; i < length; ++i) {
            pattern[i] = static_cast<unsigned char>(start_value + static_cast<unsigned char>(i));
        }

        sc_core::sc_time write_delay = sc_core::SC_ZERO_TIME;
        axi_helper::AXIRequest write_req(address, pattern.size());
        write_req.data = pattern;
        auto write_resp = master.with_response_handler([&]() {
            return axi_helper::AXIHelper::sendBlockingWrite(master.initiator_socket, write_req, write_delay);
        });
        if (!write_resp.success) {
            std::ostringstream oss;
            oss << "Write transaction failed at 0x" << std::hex << address;
            SC_REPORT_FATAL("dramsys_lpddr4_test", oss.str().c_str());
        }

        sc_core::sc_time read_delay = sc_core::SC_ZERO_TIME;
        axi_helper::AXIRequest read_req(address, pattern.size());
        auto read_resp = master.with_response_handler([&]() {
            return axi_helper::AXIHelper::sendBlockingRead(master.initiator_socket, read_req, read_delay);
        });
        if (!read_resp.success) {
            std::ostringstream oss;
            oss << "Read transaction failed at 0x" << std::hex << address;
            SC_REPORT_FATAL("dramsys_lpddr4_test", oss.str().c_str());
        }

        if (read_req.data != pattern) {
            std::ostringstream oss;
            oss << "Data mismatch at 0x" << std::hex << address;
            SC_REPORT_FATAL("dramsys_lpddr4_test", oss.str().c_str());
        }

        report_bytes("roundtrip", address, read_req.data);
    }

    void perform_string_roundtrip(sc_dt::uint64 address, const std::string& message) {
        sc_core::sc_time write_delay = sc_core::SC_ZERO_TIME;
        bool write_ok = master.with_response_handler([&]() {
            return axi_helper::AXIHelper::writeString(master.initiator_socket, address, message, 0, &write_delay);
        });
        if (!write_ok) {
            std::ostringstream oss;
            oss << "writeString failed at 0x" << std::hex << address;
            SC_REPORT_FATAL("dramsys_lpddr4_test", oss.str().c_str());
        }

        std::string read_back;
        sc_core::sc_time read_delay = sc_core::SC_ZERO_TIME;
        bool read_ok = master.with_response_handler([&]() {
            return axi_helper::AXIHelper::readString(master.initiator_socket, address, read_back, message.size() + 1, 0, &read_delay);
        });
        if (!read_ok) {
            std::ostringstream oss;
            oss << "readString failed at 0x" << std::hex << address;
            SC_REPORT_FATAL("dramsys_lpddr4_test", oss.str().c_str());
        }

        if (read_back != message) {
            std::ostringstream oss;
            oss << "String mismatch at 0x" << std::hex << address;
            SC_REPORT_FATAL("dramsys_lpddr4_test", oss.str().c_str());
        }

        std::ostringstream oss;
        oss << "String roundtrip at 0x" << std::hex << address << std::dec << ": " << read_back;
        SC_REPORT_INFO("dramsys_lpddr4_test", oss.str().c_str());
    }

    void report_bytes(const char* label, sc_dt::uint64 address, const std::vector<unsigned char>& bytes) {
        std::ostringstream oss;
        oss << label << " at 0x" << std::hex << address << std::dec << " (" << bytes.size() << " bytes): ";
        oss << std::hex << std::setfill('0');
        for (unsigned char byte : bytes) {
            oss << std::setw(2) << static_cast<int>(byte) << ' ';
        }
        SC_REPORT_INFO("dramsys_lpddr4_test", oss.str().c_str());
    }

    std::filesystem::path config_path_;
};

} // namespace

int sc_main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    std::filesystem::path config = std::filesystem::path(SRC_ROOT_DIR) / "src" / "DRAMSys" / "configs" / "lpddr4.json";
    if (!std::filesystem::exists(config)) {
        config = std::filesystem::path(SRC_ROOT_DIR) / "src" / "DRAMSys" / "configs" / "lpddr4-example.json";
    }
    if (!std::filesystem::exists(config)) {
        std::cerr << "DRAMSys configuration not found: " << config << std::endl;
        return 1;
    }

    DramsysLpddr4Bench tb("tb", config);
    sc_core::sc_start();
    return 0;
}
