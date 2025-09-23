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
 * (Adapted for LPDDR5 by Gemini)
 */

#include "CheckerLPDDR5.h"
#include "DRAMSys/common/DebugManager.h"
#include "DRAMSys/common/utils.h"
#include "DRAMSys/configuration/memspec/MemSpecLPDDR5.h"

#include <algorithm>

using namespace sc_core;
using namespace tlm;

namespace DRAMSys
{

CheckerLPDDR5::CheckerLPDDR5(const MemSpecLPDDR5& memSpec) : memSpec(memSpec)
{
    nextCommandByBank.fill({BankVector<sc_time>(memSpec.banksPerChannel, SC_ZERO_TIME)});
    nextCommandByBankGroup.fill({BankGroupVector<sc_time>(memSpec.bankGroupsPerChannel, SC_ZERO_TIME)});
    nextCommandByRank.fill({RankVector<sc_time>(memSpec.ranksPerChannel, SC_ZERO_TIME)});
    // Initialize queue container per rank (ControllerVector has no fill())
    last4ActivatesOnRank = RankVector<std::queue<sc_time>>(memSpec.ranksPerChannel);

    // Pre-calculate LPDDR5 timing constraints
    tBURST = (memSpec.defaultBurstLength / memSpec.dataRate) * memSpec.tCK;
    
    // Read-to-Write delays
    tRDWR_S = memSpec.tRL + tBURST + memSpec.tWTR_S - memSpec.tWL;
    tRDWR_L = memSpec.tRL + tBURST + memSpec.tWTR_L - memSpec.tWL;
    tRDWR_R = memSpec.tRL + tBURST + memSpec.tRTRS - memSpec.tWL;

    // Write-to-Read delays
    tWRRD_S = memSpec.tWL + tBURST + memSpec.tWTR_S;
    tWRRD_L = memSpec.tWL + tBURST + memSpec.tWTR_L;
    tWRRD_R = memSpec.tWL + tBURST + memSpec.tRTRS - memSpec.tRL;

    // Other command delays (approximate where DRAMUtils lacks parameters)
    // tRTP not available in DRAMUtils for LPDDR5; approximate with read data window length
    tRDPRE = memSpec.tRL + tBURST;
    tWRPRE = memSpec.tWL + tBURST + memSpec.tWR;
    // Use per-bank precharge timing for ACT after precharge
    tWRAACT = tWRPRE + memSpec.tRPpb;
    tRDAACT = tRDPRE + memSpec.tRPpb;
}

sc_time CheckerLPDDR5::timeToSatisfyConstraints(Command command, const tlm_generic_payload& payload) const
{
    Bank bank = ControllerExtension::getBank(payload);
    BankGroup bankGroup = ControllerExtension::getBankGroup(payload);
    Rank rank = ControllerExtension::getRank(payload);

    sc_time earliestTimeToStart = sc_time_stamp();

    earliestTimeToStart = std::max(earliestTimeToStart, nextCommandByBank[command][bank]);
    earliestTimeToStart = std::max(earliestTimeToStart, nextCommandByBankGroup[command][bankGroup]);
    earliestTimeToStart = std::max(earliestTimeToStart, nextCommandByRank[command][rank]);
    earliestTimeToStart = std::max(earliestTimeToStart, nextCommandOnBus);

    return earliestTimeToStart;
}

void CheckerLPDDR5::insert(Command command, const tlm_generic_payload& payload)
{
    const Bank bank = ControllerExtension::getBank(payload);
    const BankGroup bankGroup = ControllerExtension::getBankGroup(payload);
    const Rank rank = ControllerExtension::getRank(payload);

    PRINTDEBUGMESSAGE("CheckerLPDDR5", "Changing state on bank " + std::to_string(static_cast<std::size_t>(bank))
                      + " command is " + command.toString());

    const sc_time& currentTime = sc_time_stamp();
    
    // Most commands need to wait for tBURST time on the bus
    sc_time busBusyUntil = currentTime + tBURST;

    switch (command)
    {
    case Command::RD:
    case Command::RDA:
    {
        // Same rank, different bank group (approximate without tCCD_L)
        nextCommandByRank[Command::RD][rank] = std::max(nextCommandByRank[Command::RD][rank], currentTime + memSpec.tCK);
        nextCommandByRank[Command::RDA][rank] = std::max(nextCommandByRank[Command::RDA][rank], currentTime + memSpec.tCK);
        nextCommandByRank[Command::WR][rank] = std::max(nextCommandByRank[Command::WR][rank], currentTime + tRDWR_L);
        nextCommandByRank[Command::WRA][rank] = std::max(nextCommandByRank[Command::WRA][rank], currentTime + tRDWR_L);

        // Same rank, same bank group (approximate without tCCD_S)
        nextCommandByBankGroup[Command::RD][bankGroup] = std::max(nextCommandByBankGroup[Command::RD][bankGroup], currentTime + memSpec.tCK);
        nextCommandByBankGroup[Command::RDA][bankGroup] = std::max(nextCommandByBankGroup[Command::RDA][bankGroup], currentTime + memSpec.tCK);
        nextCommandByBankGroup[Command::WR][bankGroup] = std::max(nextCommandByBankGroup[Command::WR][bankGroup], currentTime + tRDWR_S);
        nextCommandByBankGroup[Command::WRA][bankGroup] = std::max(nextCommandByBankGroup[Command::WRA][bankGroup], currentTime + tRDWR_S);

        // Different rank
        for (unsigned int i = 0; i < memSpec.ranksPerChannel; i++)
        {
            Rank r{i};
            if (r == rank) continue;
            nextCommandByRank[Command::RD][r] = std::max(nextCommandByRank[Command::RD][r], currentTime + tBURST + memSpec.tRTRS);
            nextCommandByRank[Command::RDA][r] = std::max(nextCommandByRank[Command::RDA][r], currentTime + tBURST + memSpec.tRTRS);
            nextCommandByRank[Command::WR][r] = std::max(nextCommandByRank[Command::WR][r], currentTime + tRDWR_R);
            nextCommandByRank[Command::WRA][r] = std::max(nextCommandByRank[Command::WRA][r], currentTime + tRDWR_R);
        }

        if (command == Command::RDA)
        {
            nextCommandByBank[Command::ACT][bank] = std::max(nextCommandByBank[Command::ACT][bank], currentTime + tRDAACT);
            nextCommandByRank[Command::REFAB][rank] = std::max(nextCommandByRank[Command::REFAB][rank], currentTime + tRDPRE + memSpec.tRPpb);
        }
        else // RD
        {
            nextCommandByBank[Command::PREPB][bank] = std::max(nextCommandByBank[Command::PREPB][bank], currentTime + tRDPRE);
        }
        break;
    }
    case Command::WR:
    case Command::WRA:
    {
        // Same rank, different bank group
        nextCommandByRank[Command::RD][rank] = std::max(nextCommandByRank[Command::RD][rank], currentTime + tWRRD_L);
        nextCommandByRank[Command::RDA][rank] = std::max(nextCommandByRank[Command::RDA][rank], currentTime + tWRRD_L);
        // No tCCD_L available; use tCK baseline
        nextCommandByRank[Command::WR][rank] = std::max(nextCommandByRank[Command::WR][rank], currentTime + memSpec.tCK);
        nextCommandByRank[Command::WRA][rank] = std::max(nextCommandByRank[Command::WRA][rank], currentTime + memSpec.tCK);

        // Same rank, same bank group
        nextCommandByBankGroup[Command::RD][bankGroup] = std::max(nextCommandByBankGroup[Command::RD][bankGroup], currentTime + tWRRD_S);
        nextCommandByBankGroup[Command::RDA][bankGroup] = std::max(nextCommandByBankGroup[Command::RDA][bankGroup], currentTime + tWRRD_S);
        // No tCCD_S available; use tCK baseline
        nextCommandByBankGroup[Command::WR][bankGroup] = std::max(nextCommandByBankGroup[Command::WR][bankGroup], currentTime + memSpec.tCK);
        nextCommandByBankGroup[Command::WRA][bankGroup] = std::max(nextCommandByBankGroup[Command::WRA][bankGroup], currentTime + memSpec.tCK);

        // Different rank
        for (unsigned int i = 0; i < memSpec.ranksPerChannel; i++)
        {
            Rank r{i};
            if (r == rank) continue;
            nextCommandByRank[Command::RD][r] = std::max(nextCommandByRank[Command::RD][r], currentTime + tWRRD_R);
            nextCommandByRank[Command::RDA][r] = std::max(nextCommandByRank[Command::RDA][r], currentTime + tWRRD_R);
            nextCommandByRank[Command::WR][r] = std::max(nextCommandByRank[Command::WR][r], currentTime + tBURST + memSpec.tRTRS);
            nextCommandByRank[Command::WRA][r] = std::max(nextCommandByRank[Command::WRA][r], currentTime + tBURST + memSpec.tRTRS);
        }
        
        if (command == Command::WRA)
        {
            nextCommandByBank[Command::ACT][bank] = std::max(nextCommandByBank[Command::ACT][bank], currentTime + tWRAACT);
        }
        else // WR
        {
            nextCommandByBank[Command::PREPB][bank] = std::max(nextCommandByBank[Command::PREPB][bank], currentTime + tWRPRE);
        }
        break;
    }
    case Command::ACT:
    {
        nextCommandByBank[Command::PREPB][bank] = std::max(nextCommandByBank[Command::PREPB][bank], currentTime + memSpec.tRAS);
        // tRCD not available in DRAMUtils for LPDDR5; no additional constraint here
        nextCommandByBank[Command::RD][bank] = std::max(nextCommandByBank[Command::RD][bank], currentTime + SC_ZERO_TIME);
        nextCommandByBank[Command::RDA][bank] = std::max(nextCommandByBank[Command::RDA][bank], currentTime + SC_ZERO_TIME);
        nextCommandByBank[Command::WR][bank] = std::max(nextCommandByBank[Command::WR][bank], currentTime + SC_ZERO_TIME);
        nextCommandByBank[Command::WRA][bank] = std::max(nextCommandByBank[Command::WRA][bank], currentTime + SC_ZERO_TIME);
        nextCommandByBank[Command::ACT][bank] = std::max(nextCommandByBank[Command::ACT][bank], currentTime + memSpec.tRCpb);

        // Same rank, different bank group
        nextCommandByRank[Command::ACT][rank] = std::max(nextCommandByRank[Command::ACT][rank], currentTime + memSpec.tRRD);

        // Same rank, same bank group
        nextCommandByBankGroup[Command::ACT][bankGroup] = std::max(nextCommandByBankGroup[Command::ACT][bankGroup], currentTime + memSpec.tRRD);
        
        // tFAW handling
        last4ActivatesOnRank[rank].push(currentTime);
        if (last4ActivatesOnRank[rank].size() > 4)
            last4ActivatesOnRank[rank].pop();
        if (last4ActivatesOnRank[rank].size() == 4)
        {
            sc_time fourthLastActivate = last4ActivatesOnRank[rank].front();
            nextCommandByRank[Command::ACT][rank] = std::max(nextCommandByRank[Command::ACT][rank], fourthLastActivate + memSpec.tFAW);
        }
        busBusyUntil = currentTime + memSpec.tCK; // ACT is short
        break;
    }
    case Command::PREPB:
    {
        nextCommandByBank[Command::ACT][bank] = std::max(nextCommandByBank[Command::ACT][bank], currentTime + memSpec.tRPpb);
        busBusyUntil = currentTime + memSpec.tCK; // PRE is short
        break;
    }
    case Command::PREAB:
    {
        // Block ACT on this rank for tRPab
        nextCommandByRank[Command::ACT][rank] = std::max(nextCommandByRank[Command::ACT][rank], currentTime + memSpec.tRPab);
        busBusyUntil = currentTime + memSpec.tCK; // PRE is short
        break;
    }
    case Command::REFAB:
    {
        // Block ACT on this rank for tRFCab
        nextCommandByRank[Command::ACT][rank] = std::max(nextCommandByRank[Command::ACT][rank], currentTime + memSpec.tRFCab);
        nextCommandByRank[Command::REFAB][rank] = std::max(nextCommandByRank[Command::REFAB][rank], currentTime + memSpec.tRFCab);
        busBusyUntil = currentTime + memSpec.tCK; // REF is short
        break;
    }
    case Command::REFPB:
    {
        nextCommandByBank[Command::ACT][bank] = std::max(nextCommandByBank[Command::ACT][bank], currentTime + memSpec.tRFCpb);
        busBusyUntil = currentTime + memSpec.tCK; // REF is short
        break;
    }
    case Command::SREFEN:
    {
        nextCommandByRank[Command::SREFEX][rank] = std::max(nextCommandByRank[Command::SREFEX][rank], currentTime + memSpec.tCK);
        busBusyUntil = currentTime + memSpec.tCK; // SREF is short
        break;
    }
    case Command::SREFEX:
    {
        // Approximate: allow ACT after a short delay on this rank
        nextCommandByRank[Command::ACT][rank] = std::max(nextCommandByRank[Command::ACT][rank], currentTime + memSpec.tCK);
        busBusyUntil = currentTime + memSpec.tCK; // SREF is short
        break;
    }
    default:
        busBusyUntil = currentTime + memSpec.getCommandLength(command);
    }
    nextCommandOnBus = std::max(nextCommandOnBus, busBusyUntil);
}

} // namespace DRAMSys
