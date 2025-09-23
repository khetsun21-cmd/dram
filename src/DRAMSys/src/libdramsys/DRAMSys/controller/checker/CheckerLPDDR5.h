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

#ifndef CHECKERLPDDR5_H
#define CHECKERLPDDR5_H

#include "DRAMSys/controller/checker/CheckerIF.h"
#include "DRAMSys/configuration/memspec/MemSpecLPDDR5.h"

#include <queue>
#include <vector>

namespace DRAMSys
{

class CheckerLPDDR5 final : public CheckerIF
{
public:
    explicit CheckerLPDDR5(const MemSpecLPDDR5& memSpec);
    [[nodiscard]] sc_core::sc_time timeToSatisfyConstraints(Command command, const tlm::tlm_generic_payload& payload) const override;
    void insert(Command command, const tlm::tlm_generic_payload& payload) override;

private:
    const MemSpecLPDDR5& memSpec;

    // Pre-calculated timing values for LPDDR5
    sc_core::sc_time tBURST;
    sc_core::sc_time tRDWR_S; // Read to Write, Same Bank Group
    sc_core::sc_time tRDWR_L; // Read to Write, Different Bank Group
    sc_core::sc_time tRDWR_R; // Read to Write, Different Rank
    sc_core::sc_time tWRRD_S; // Write to Read, Same Bank Group
    sc_core::sc_time tWRRD_L; // Write to Read, Different Bank Group
    sc_core::sc_time tWRRD_R; // Write to Read, Different Rank
    sc_core::sc_time tRDPRE;
    sc_core::sc_time tWRPRE;
    sc_core::sc_time tWRAACT;
    sc_core::sc_time tRDAACT;

    template<typename T>
    using CommandArray = std::array<T, Command::END_ENUM>;
    template<typename T>
    using BankVector = ControllerVector<Bank, T>;
    template<typename T>
    using BankGroupVector = ControllerVector<BankGroup, T>;
    template<typename T>
    using RankVector = ControllerVector<Rank, T>;

    // Timing constraint states
    CommandArray<BankVector<sc_core::sc_time>> nextCommandByBank;
    CommandArray<BankGroupVector<sc_core::sc_time>> nextCommandByBankGroup;
    CommandArray<RankVector<sc_core::sc_time>> nextCommandByRank;

    RankVector<std::queue<sc_core::sc_time>> last4ActivatesOnRank;
    sc_core::sc_time nextCommandOnBus = sc_core::SC_ZERO_TIME;
};

} // namespace DRAMSys

#endif // CHECKERLPDDR5_H