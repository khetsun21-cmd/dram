#include "AxiDramsysSystem.h"

#include <sstream>

AxiDramsysSystem::AxiDramsysSystem(sc_core::sc_module_name name)
    : sc_module(name)
    , bridge_("axi_bridge")
    , axi_target_socket(bridge_.axi_target_socket)
    , clk_i(bridge_.clk_i) {}

void AxiDramsysSystem::set_config_path(const std::filesystem::path& config_path) {
    config_path_ = config_path;
}

void AxiDramsysSystem::before_end_of_elaboration() {
    sc_module::before_end_of_elaboration();
    instantiate_dramsys();
}

void AxiDramsysSystem::end_of_elaboration() {
    sc_module::end_of_elaboration();
    instantiate_dramsys();
}

void AxiDramsysSystem::instantiate_dramsys() {
    if (dramsys_) {
        return;
    }

    if (config_path_.empty()) {
        SC_REPORT_FATAL("AxiDramsysSystem", "Configuration path not set before elaboration.");
    }

    auto absolute_path = std::filesystem::absolute(config_path_);
    if (!std::filesystem::exists(absolute_path)) {
        std::ostringstream oss;
        oss << "Configuration file does not exist: " << absolute_path.string();
        SC_REPORT_FATAL("AxiDramsysSystem", oss.str().c_str());
    }

    config_path_ = absolute_path;
    configuration_ = DRAMSys::Config::from_path(config_path_);
    dramsys_ = std::make_unique<DRAMSys::DRAMSys>("DRAMSys", *configuration_);
    bridge_.tlm_initiator_socket.bind(dramsys_->tSocket);
}
