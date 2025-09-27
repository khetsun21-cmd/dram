#ifndef AXI_DRAMSYS_SYSTEM_H
#define AXI_DRAMSYS_SYSTEM_H

#include "AxiToTlmBridge.h"

#include <DRAMSys/config/DRAMSysConfiguration.h>
#include <DRAMSys/simulation/DRAMSys.h>

#include <axi/axi_tlm.h>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <systemc>

class AxiDramsysSystem : public sc_core::sc_module {
public:
    SC_HAS_PROCESS(AxiDramsysSystem);

    explicit AxiDramsysSystem(sc_core::sc_module_name name);

    void set_config_path(const std::filesystem::path& config_path);
    void set_config_path(const std::string& config_path) { set_config_path(std::filesystem::path(config_path)); }
    void set_embedded_config(DRAMSys::Config::EmbeddedConfiguration config);

    const std::filesystem::path& get_config_path() const { return config_path_; }
    std::optional<DRAMSys::Config::EmbeddedConfiguration> get_embedded_config() const
    {
        return embedded_config_;
    }

protected:
    void before_end_of_elaboration() override;
    void end_of_elaboration() override;

private:
    void instantiate_dramsys();

    AxiToTlmBridge bridge_;
    std::filesystem::path config_path_{};
    std::optional<DRAMSys::Config::EmbeddedConfiguration> embedded_config_{};
    std::optional<DRAMSys::Config::Configuration> configuration_{};
    std::unique_ptr<DRAMSys::DRAMSys> dramsys_{};

public:
    axi::axi_target_socket<1024, axi::axi_protocol_types, 1, sc_core::SC_ZERO_OR_MORE_BOUND>& axi_target_socket;
    sc_core::sc_in<bool>& clk_i;
};

#endif // AXI_DRAMSYS_SYSTEM_H
