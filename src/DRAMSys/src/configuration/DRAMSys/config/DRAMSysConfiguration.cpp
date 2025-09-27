/*
 * Copyright (c) 2021, RPTU Kaiserslautern-Landau
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Authors:
 *    Derek Christ
 */

#include "DRAMSysConfiguration.h"

#include "DRAMSys/config/MemSpec.h"

#include <fstream>
#include <optional>
#include <stdexcept>
#include <string_view>

namespace
{

using DRAMSys::Config::Configuration;
using DRAMSys::Config::EmbeddedConfiguration;

constexpr std::string_view LPDDR4_EMBEDDED_CONFIG = R"json(
{
    "simulation": {
        "addressmapping": {
            "BANK_BIT": [22, 23, 24],
            "BYTE_BIT": [0],
            "COLUMN_BIT": [1, 2, 3, 4, 5, 6],
            "ROW_BIT": [7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21]
        },
        "mcconfig": {
            "PagePolicy": "Open",
            "Scheduler": "FrFcfs",
            "SchedulerBuffer": "Bankwise",
            "RequestBufferSize": 8,
            "CmdMux": "Oldest",
            "RespQueue": "Fifo",
            "RefreshPolicy": "AllBank",
            "RefreshMaxPostponed": 0,
            "RefreshMaxPulledin": 0,
            "PowerDownPolicy": "NoPowerDown",
            "Arbiter": "Simple",
            "MaxActiveTransactions": 128,
            "RefreshManagement": false
        },
        "memspec": {
            "memarchitecturespec": {
                "burstLength": 16,
                "dataRate": 2,
                "nbrOfBanks": 8,
                "nbrOfColumns": 64,
                "nbrOfRanks": 1,
                "nbrOfRows": 32768,
                "width": 256,
                "nbrOfDevices": 1,
                "nbrOfChannels": 1,
                "nbrOfBankGroups": 1,
                "maxBurstLength": 16
            },
            "memoryId": "JEDEC_8Gb_LPDDR4-3200_16bit",
            "memoryType": "LPDDR4",
            "mempowerspec": {
                "idd01": 3.5e-3,
                "idd02": 45.0e-3,
                "idd0ql": 0.75e-3,
                "idd2n1": 2.0e-3,
                "idd2n2": 27.0e-3,
                "idd2nQ": 0.75e-3,
                "idd2ns1": 2.0e-3,
                "idd2ns2": 23.0e-3,
                "idd2nsq": 0.75e-3,
                "idd2p1": 1.2e-3,
                "idd2p2": 3.0e-3,
                "idd2pQ": 0.75e-3,
                "idd2ps1": 1.2e-3,
                "idd2ps2": 3.0e-3,
                "idd2psq": 0.75e-3,
                "idd3n1": 2.25e-3,
                "idd3n2": 30.0e-3,
                "idd3nQ": 0.75e-3,
                "idd3ns1": 2.25e-3,
                "idd3ns2": 30.0e-3,
                "idd3nsq": 0.75e-3,
                "idd3p1": 1.2e-3,
                "idd3p2": 9.0e-3,
                "idd3pQ": 0.75e-3,
                "idd3ps1": 1.2e-3,
                "idd3ps2": 9.0e-3,
                "idd3psq": 0.75e-3,
                "idd4r1": 2.25e-3,
                "idd4r2": 275.0e-3,
                "idd4rq": 150.0e-3,
                "idd4w1": 2.25e-3,
                "idd4w2": 210.0e-3,
                "idd4wq": 55.0e-3,
                "idd51": 10.0e-3,
                "idd52": 90.0e-3,
                "idd5ab1": 2.5e-3,
                "idd5ab2": 30.0e-3,
                "idd5abq": 0.75e-3,
                "idd5pb1": 2.5e-3,
                "idd5pb2": 30.0e-3,
                "idd5pbq": 0.75e-3,
                "idd5q": 0.75e-3,
                "idd61": 0.3e-3,
                "idd62": 0.5e-3,
                "idd6q": 0.1e-3,
                "vdd1": 1.8,
                "vdd2": 1.1,
                "vddq": 1.1,
                "iBeta_vdd1": 3.5e-3,
                "iBeta_vdd2": 45.0e-3
            },
            "memtimingspec": {
                "CCD": 2.5,
                "CCDMW": 32,
                "CKE": 12,
                "CMDCKE": 3,
                "DQS2DQ": 2,
                "DQSCK": 6,
                "DQSS": 1,
                "ESCKE": 3,
                "FAW": 64,
                "PPD": 4,
                "RCD": 29,
                "REFI": 6246,
                "REFIpb": 780,
                "RFCab": 448,
                "RFCpb": 224,
                "RL": 5,
                "RAS": 34.7,
                "RPab": 34,
                "RPpb": 29,
                "RCab": 102,
                "RCpb": 97,
                "RPST": 0,
                "RRD": 16,
                "RTP": 12,
                "SR": 24,
                "WL": 14,
                "WPRE": 2,
                "WR": 20,
                "WTR": 16,
                "XP": 12,
                "XSR": 460,
                "RTRS": 1,
                "tCK": 5e-9
            },
            "memimpedancespec": {
                "ck_termination": true,
                "ck_R_eq": 1e6,
                "ck_dyn_E": 1e-12,
                "ca_termination": true,
                "ca_R_eq": 1e6,
                "ca_dyn_E": 1e-12,
                "rdq_termination": true,
                "rdq_R_eq": 1e6,
                "rdq_dyn_E": 1e-12,
                "wdq_termination": true,
                "wdq_R_eq": 1e6,
                "wdq_dyn_E": 1e-12,
                "wdqs_termination": true,
                "wdqs_R_eq": 1e6,
                "wdqs_dyn_E": 1e-12,
                "rdqs_termination": true,
                "rdqs_R_eq": 1e6,
                "rdqs_dyn_E": 1e-12,
                "rdbi_termination": true,
                "rdbi_R_eq": 1e6,
                "rdbi_dyn_E": 1e-12,
                "wdbi_termination": true,
                "wdbi_R_eq": 1e6,
                "wdbi_dyn_E": 1e-12
            },
            "bankwisespec": {
                "factRho": 1,
                "factSigma": 1,
                "pasrMode": 0,
                "hasPASR": false
            }
        },
        "simconfig": {
            "AddressOffset": 0,
            "CheckTLM2Protocol": false,
            "DatabaseRecording": true,
            "Debug": false,
            "EnableWindowing": true,
            "PowerAnalysis": false,
            "SimulationName": "gem5_se",
            "SimulationProgressBar": true,
            "StoreMode": "Store",
            "UseMalloc": false,
            "WindowSize": 1000
        },
        "simulationid": "lpddr4-example",
        "tracesetup": [
            {
                "type": "player",
                "clkMhz": 200,
                "name": "traces/example.stl"
            }
        ]
    }
}
)json";

bool matches_lpddr4(const std::filesystem::path& path)
{
    return path.filename() == std::filesystem::path{"lpddr4.json"};
}

Configuration parse_lpddr4_embedded()
{
    json_t simulation = json_t::parse(LPDDR4_EMBEDDED_CONFIG, nullptr, true, true).at(Configuration::KEY);
    return simulation.get<Configuration>();
}

} // namespace

namespace DRAMSys::Config
{

Configuration from_embedded(EmbeddedConfiguration config)
{
    switch (config)
    {
    case EmbeddedConfiguration::Lpddr4:
        return parse_lpddr4_embedded();
    }

    throw std::runtime_error("Unsupported embedded configuration");
}

std::optional<Configuration> try_from_embedded(const std::filesystem::path& baseConfig)
{
    if (matches_lpddr4(baseConfig))
        return parse_lpddr4_embedded();

    return std::nullopt;
}

Configuration from_path(std::filesystem::path baseConfig)
{
    if (auto embedded = try_from_embedded(baseConfig))
        return *embedded;

    std::ifstream file(baseConfig);
    std::filesystem::path baseDir = baseConfig.parent_path();

    enum class SubConfig
    {
        MemSpec,
        AddressMapping,
        McConfig,
        SimConfig,
        TraceSetup,
        Unkown
    } current_sub_config;

    // This custom parser callback is responsible to swap out the paths to the sub-config json files
    // with the actual json data.
    std::function<bool(int depth, nlohmann::detail::parse_event_t event, json_t& parsed)>
        parser_callback;
    parser_callback = [&parser_callback, &current_sub_config, baseDir](
                          int depth, nlohmann::detail::parse_event_t event, json_t& parsed) -> bool
    {
        using nlohmann::detail::parse_event_t;

        if (depth != 2)
            return true;

        if (event == parse_event_t::key)
        {
            assert(parsed.is_string());

            if (parsed == MemSpecConstants::KEY)
                current_sub_config = SubConfig::MemSpec;
            else if (parsed == AddressMapping::KEY)
                current_sub_config = SubConfig::AddressMapping;
            else if (parsed == McConfig::KEY)
                current_sub_config = SubConfig::McConfig;
            else if (parsed == SimConfig::KEY)
                current_sub_config = SubConfig::SimConfig;
            else if (parsed == TraceSetupConstants::KEY)
                current_sub_config = SubConfig::TraceSetup;
            else
                current_sub_config = SubConfig::Unkown;
        }

        // In case we have an value (string) instead of an object, replace the value with the loaded
        // json object.
        if (event == parse_event_t::value && current_sub_config != SubConfig::Unkown)
        {
            // Replace name of json file with actual json data
            auto parse_json = [&parser_callback, baseDir](std::string_view sub_config_key,
                                                          const std::string& filename) -> json_t
            {
                std::filesystem::path path{baseDir};
                path /= filename;

                std::ifstream json_file(path);

                if (!json_file.is_open())
                    throw std::runtime_error("Failed to open file " + std::string(path));

                json_t json =
                    json_t::parse(json_file, parser_callback, true, true).at(sub_config_key);
                return json;
            };

            if (current_sub_config == SubConfig::MemSpec)
                parsed = parse_json(MemSpecConstants::KEY, parsed);
            else if (current_sub_config == SubConfig::AddressMapping)
                parsed = parse_json(AddressMapping::KEY, parsed);
            else if (current_sub_config == SubConfig::McConfig)
                parsed = parse_json(McConfig::KEY, parsed);
            else if (current_sub_config == SubConfig::SimConfig)
                parsed = parse_json(SimConfig::KEY, parsed);
            else if (current_sub_config == SubConfig::TraceSetup)
                parsed = parse_json(TraceSetupConstants::KEY, parsed);
        }

        return true;
    };

    if (file.is_open())
    {
        json_t simulation = json_t::parse(file, parser_callback, true, true).at(Configuration::KEY);
        return simulation.get<Config::Configuration>();
    }
    throw std::runtime_error("Failed to open file " + std::string(baseConfig));
}

} // namespace DRAMSys::Config
