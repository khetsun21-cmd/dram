/*
 * Copyright (c) 2024, RPTU Kaiserslautern-Landau
 * All rights reserved.
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
 * Lukas Steiner
 * Derek Christ
 * Marco Mörz
 * (Adapted for LPDDR5 by Gemini)
 */

#include "MemSpecLPDDR5.h"

#include "DRAMSys/common/utils.h"
#include <DRAMPower/standards/lpddr5/LPDDR5.h>
#include <iostream>

using namespace sc_core;
using namespace tlm;

namespace DRAMSys
{

MemSpecLPDDR5::MemSpecLPDDR5(const DRAMUtils::MemSpec::MemSpecLPDDR5& memSpec) :
    MemSpec(memSpec,
            // numberOfChannels
            memSpec.memarchitecturespec.nbrOfChannels,
            // ranksPerChannel
            memSpec.memarchitecturespec.nbrOfRanks,
            // banksPerRank (total banks per rank)
            memSpec.memarchitecturespec.nbrOfBanks,
            // groupsPerRank (bank groups per rank)
            memSpec.memarchitecturespec.nbrOfBankGroups,
            // banksPerGroup (banks per bank group)
            (memSpec.memarchitecturespec.nbrOfBankGroups > 0)
                ? (memSpec.memarchitecturespec.nbrOfBanks /
                   memSpec.memarchitecturespec.nbrOfBankGroups)
                : memSpec.memarchitecturespec.nbrOfBanks,
            // banksPerChannel = banksPerRank * ranksPerChannel
            memSpec.memarchitecturespec.nbrOfBanks *
                memSpec.memarchitecturespec.nbrOfRanks,
            // bankGroupsPerChannel = groupsPerRank * ranksPerChannel
            memSpec.memarchitecturespec.nbrOfBankGroups *
                memSpec.memarchitecturespec.nbrOfRanks,
            // devicesPerRank
            memSpec.memarchitecturespec.nbrOfDevices),
    memSpec(memSpec),
    tREFI(tCK * memSpec.memtimingspec.REFI),
    tREFIpb(tCK * memSpec.memtimingspec.REFIpb),
    tRFCab(tCK * memSpec.memtimingspec.RFCab),
    tRFCpb(tCK * memSpec.memtimingspec.RFCpb),
    tRAS(tCK * memSpec.memtimingspec.RAS),
    tRPab(tCK * memSpec.memtimingspec.RPab),
    tRPpb(tCK * memSpec.memtimingspec.RPpb),
    tRCab(tCK * memSpec.memtimingspec.RCab),
    tRCpb(tCK * memSpec.memtimingspec.RCpb),
    tPPD(tCK * memSpec.memtimingspec.PPD),
    tFAW(tCK * memSpec.memtimingspec.FAW),
    tRRD(tCK * memSpec.memtimingspec.RRD),
    tRL(tCK * memSpec.memtimingspec.RL),
    tWL(tCK * memSpec.memtimingspec.WL),
    tWCK2DQO(tCK * memSpec.memtimingspec.WCK2DQO),
    tWR(tCK * memSpec.memtimingspec.WR),
    tWTR_L(tCK * memSpec.memtimingspec.WTR_L),
    tWTR_S(tCK * memSpec.memtimingspec.WTR_S),
    tRTRS(tCK * memSpec.memtimingspec.RTRS)
{
    commandLengthInCycles[Command::ACT] = 4;
    commandLengthInCycles[Command::PREPB] = 2;
    commandLengthInCycles[Command::PREAB] = 2;
    commandLengthInCycles[Command::RD] = 4;
    commandLengthInCycles[Command::RDA] = 4;
    commandLengthInCycles[Command::WR] = 4;
    commandLengthInCycles[Command::WRA] = 4;
    commandLengthInCycles[Command::REFAB] = 2;
    commandLengthInCycles[Command::REFPB] = 2;
    commandLengthInCycles[Command::SREFEN] = 2;
    commandLengthInCycles[Command::SREFEX] = 2;

    uint64_t deviceSizeBits =
        static_cast<uint64_t>(banksPerRank) * rowsPerBank * columnsPerRow * bitWidth;
    uint64_t deviceSizeBytes = deviceSizeBits / 8;
    memorySizeBytes = deviceSizeBytes * ranksPerChannel * numberOfChannels;

    std::cout << headline << std::endl;
    std::cout << "Memory Configuration:" << std::endl << std::endl;
    std::cout << " Memory type:           "
              << "LPDDR5" << std::endl;
    std::cout << " Memory size in bytes:  " << memorySizeBytes << std::endl;
    std::cout << " Channels:              " << numberOfChannels << std::endl;
    std::cout << " Ranks per channel:     " << ranksPerChannel << std::endl;
    std::cout << " Bank Groups per rank:  " << groupsPerRank << std::endl;
    std::cout << " Banks per group:       " << banksPerGroup << std::endl;
    std::cout << " Banks per rank:        " << banksPerRank << std::endl;
    std::cout << " Rows per bank:         " << rowsPerBank << std::endl;
    std::cout << " Columns per row:       " << columnsPerRow << std::endl;
    std::cout << " Device width in bits:  " << bitWidth << std::endl;
    std::cout << " Device size in bits:   " << deviceSizeBits << std::endl;
    std::cout << " Device size in bytes:  " << deviceSizeBytes << std::endl;
    std::cout << " Devices per rank:      " << devicesPerRank << std::endl;
    std::cout << std::endl;
}

sc_time MemSpecLPDDR5::getRefreshIntervalAB() const
{
    return tREFI;
}

sc_time MemSpecLPDDR5::getRefreshIntervalPB() const
{
    return tREFIpb;
}

sc_time MemSpecLPDDR5::getExecutionTime(Command command,
                                        [[maybe_unused]] const tlm::tlm_generic_payload& payload) const
{
    if (command == Command::PREPB)
        return tRPpb;

    if (command == Command::PREAB)
        return tRPab;

    if (command == Command::ACT)
    {
        // NOTE: tRCD is missing from DRAMUtils. Returning placeholder.
        // For a correct simulation, DRAMUtils needs to be updated.
        return SC_ZERO_TIME;
    }

    if (command == Command::RD)
        return tRL + burstDuration;

    if (command == Command::RDA)
    {
        // NOTE: tRTP is missing from DRAMUtils. Returning placeholder.
        // For a correct simulation, DRAMUtils needs to be updated.
        return tRL + burstDuration;
    }

    if (command == Command::WR)
        return tWL + burstDuration;

    if (command == Command::WRA)
        return tWL + burstDuration + tWR + tRPpb;

    if (command == Command::REFAB)
        return tRFCab;

    if (command == Command::REFPB)
        return tRFCpb;

    SC_REPORT_FATAL("getExecutionTime",
                    "command not known or command doesn't have a fixed execution time");
    throw;
}

TimeInterval
MemSpecLPDDR5::getIntervalOnDataStrobe(Command command,
                                       [[maybe_unused]] const tlm::tlm_generic_payload& payload) const
{
    if (command == Command::RD || command == Command::RDA)
        return {tRL, tRL + burstDuration};

    if (command == Command::WR || command == Command::WRA)
        return {tWL + tWCK2DQO, tWL + tWCK2DQO + burstDuration};

    SC_REPORT_FATAL("MemSpecLPDDR5", "Method was called with invalid argument");
    throw;
}

std::unique_ptr<DRAMPower::dram_base<DRAMPower::CmdType>> MemSpecLPDDR5::toDramPowerObject() const
{
    return std::make_unique<DRAMPower::LPDDR5>(DRAMPower::MemSpecLPDDR5(memSpec));
}

bool MemSpecLPDDR5::requiresMaskedWrite(const tlm::tlm_generic_payload& payload) const
{
    return !allBytesEnabled(payload);
}

} // namespace DRAMSys
