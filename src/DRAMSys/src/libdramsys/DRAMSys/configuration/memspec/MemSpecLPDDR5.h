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

#ifndef MEMSPECLPDDR5_H
#define MEMSPECLPDDR5_H

#include "DRAMSys/configuration/memspec/MemSpec.h"
#include <DRAMUtils/memspec/standards/MemSpecLPDDR5.h>
#include <systemc>

namespace DRAMSys
{

class MemSpecLPDDR5 final : public MemSpec
{
public:
    explicit MemSpecLPDDR5(const DRAMUtils::MemSpec::MemSpecLPDDR5& memSpec);

    // Memspec Variables:
    const DRAMUtils::MemSpec::MemSpecLPDDR5& memSpec;
    const sc_core::sc_time tREFI;
    const sc_core::sc_time tREFIpb;
    const sc_core::sc_time tRFCab;
    const sc_core::sc_time tRFCpb;
    const sc_core::sc_time tRAS;
    const sc_core::sc_time tRPab;
    const sc_core::sc_time tRPpb;
    const sc_core::sc_time tRCab;
    const sc_core::sc_time tRCpb;
    const sc_core::sc_time tPPD;
    const sc_core::sc_time tFAW;
    const sc_core::sc_time tRRD;
    const sc_core::sc_time tRL;
    const sc_core::sc_time tWL;
    const sc_core::sc_time tWCK2DQO;
    const sc_core::sc_time tWR;
    const sc_core::sc_time tWTR_L;
    const sc_core::sc_time tWTR_S;
    const sc_core::sc_time tRTRS;

    // Currents and Voltages:
    // TODO: to be completed

    [[nodiscard]] sc_core::sc_time getRefreshIntervalAB() const override;
    [[nodiscard]] sc_core::sc_time getRefreshIntervalPB() const override;

    [[nodiscard]] sc_core::sc_time
    getExecutionTime(Command command, const tlm::tlm_generic_payload& payload) const override;
    [[nodiscard]] TimeInterval
    getIntervalOnDataStrobe(Command command,
                            const tlm::tlm_generic_payload& payload) const override;

    [[nodiscard]] bool requiresMaskedWrite(const tlm::tlm_generic_payload& payload) const override;

    [[nodiscard]] std::unique_ptr<DRAMPower::dram_base<DRAMPower::CmdType>> toDramPowerObject() const override;
};

} // namespace DRAMSys

#endif // MEMSPECLPDDR5_H