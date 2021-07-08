/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2021 Advanced Micro Devices, Inc. All Rights Reserved.
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 *
 **********************************************************************************************************************/

#include "core/platform.h"
#include "core/hw/gfxip/gfx9/gfx9CmdStream.h"
#include "core/hw/gfxip/gfx9/gfx9CmdUtil.h"
#include "core/hw/gfxip/gfx9/gfx9Device.h"
#include "core/hw/gfxip/gfx9/gfx9GraphicsPipeline.h"
#include "core/hw/gfxip/gfx9/gfx9PipelineChunkHs.h"

using namespace Util;

namespace Pal
{
namespace Gfx9
{

// =====================================================================================================================
PipelineChunkHs::PipelineChunkHs(
    const Device&       device,
    const PerfDataInfo* pPerfDataInfo)
    :
    m_device(device),
    m_regs{},
    m_pHsPerfDataInfo(pPerfDataInfo),
    m_stageInfo{}
{
    m_stageInfo.stageId = Abi::HardwareStage::Hs;
}

// =====================================================================================================================
// Late initialization for this pipeline chunk.  Responsible for fetching register values from the pipeline binary and
// determining the values of other registers.
void PipelineChunkHs::LateInit(
    const AbiReader&        abiReader,
    const RegisterVector&   registers,
    PipelineUploader*       pUploader,
    MetroHash64*            pHasher)
{
    const GpuChipProperties& chipProps    = m_device.Parent()->ChipProperties();
    const RegisterInfo&      registerInfo = m_device.CmdUtil().GetRegInfo();

    const uint16 mmSpiShaderUserDataHs0 = registerInfo.mmUserDataStartHsShaderStage;
    const uint16 mmSpiShaderPgmLoLs     = registerInfo.mmSpiShaderPgmLoLs;

    GpuSymbol symbol = { };
    if (pUploader->GetPipelineGpuSymbol(Abi::PipelineSymbolType::HsMainEntry, &symbol) == Result::Success)
    {
        m_stageInfo.codeLength     = static_cast<size_t>(symbol.size);
        PAL_ASSERT(IsPow2Aligned(symbol.gpuVirtAddr, 256));

        m_regs.sh.spiShaderPgmLoLs.bits.MEM_BASE = Get256BAddrLo(symbol.gpuVirtAddr);
        PAL_ASSERT(Get256BAddrHi(symbol.gpuVirtAddr) == 0);
    }

    regSPI_SHADER_USER_DATA_LS_0 spiShaderUserDataLoHs = { };
    if (pUploader->GetPipelineGpuSymbol(Abi::PipelineSymbolType::HsShdrIntrlTblPtr, &symbol) == Result::Success)
    {
        m_regs.sh.userDataInternalTable = LowPart(symbol.gpuVirtAddr);
    }

    const Elf::SymbolTableEntry* pElfSymbol = abiReader.GetPipelineSymbol(Abi::PipelineSymbolType::HsDisassembly);
    if (pElfSymbol != nullptr)
    {
        m_stageInfo.disassemblyLength = static_cast<size_t>(pElfSymbol->st_size);
    }

    m_regs.sh.spiShaderPgmRsrc1Hs.u32All = registers.At(mmSPI_SHADER_PGM_RSRC1_HS);
    m_regs.sh.spiShaderPgmRsrc2Hs.u32All = registers.At(mmSPI_SHADER_PGM_RSRC2_HS);
    registers.HasEntry(mmSPI_SHADER_PGM_RSRC3_HS, &m_regs.dynamic.spiShaderPgmRsrc3Hs.u32All);

    // NOTE: The Pipeline ABI doesn't specify CU enable masks for each shader stage, so it should be safe to
    // always use the ones PAL prefers.
    m_regs.dynamic.spiShaderPgmRsrc3Hs.bits.CU_EN = m_device.GetCuEnableMask(0, UINT_MAX);

    if (IsGfx10Plus(chipProps.gfxLevel))
    {
        m_regs.dynamic.spiShaderPgmRsrc4Hs.gfx10Plus.CU_EN = m_device.GetCuEnableMaskHi(0, UINT_MAX);

#if PAL_ENABLE_PRINTS_ASSERTS
        m_device.AssertUserAccumRegsDisabled(registers, Gfx10Plus::mmSPI_SHADER_USER_ACCUM_LSHS_0);
#endif
    }

    if (chipProps.gfx9.supportSpp != 0)
    {
        registers.HasEntry(Apu09_1xPlus::mmSPI_SHADER_PGM_CHKSUM_HS, &m_regs.sh.spiShaderPgmChksumHs.u32All);
    }

    m_regs.context.vgtHosMinTessLevel.u32All = registers.At(mmVGT_HOS_MIN_TESS_LEVEL);
    m_regs.context.vgtHosMaxTessLevel.u32All = registers.At(mmVGT_HOS_MAX_TESS_LEVEL);

    pHasher->Update(m_regs.context);
}

// =====================================================================================================================
// Copies this pipeline chunk's sh commands into the specified command space. Returns the next unused DWORD in
// pCmdSpace.
uint32* PipelineChunkHs::WriteShCommands(
    CmdStream*              pCmdStream,
    uint32*                 pCmdSpace,
    const DynamicStageInfo& hsStageInfo
    ) const
{
    const GpuChipProperties& chipProps = m_device.Parent()->ChipProperties();

    const RegisterInfo& registerInfo = m_device.CmdUtil().GetRegInfo();

    const uint16 mmSpiShaderUserDataHs0 = registerInfo.mmUserDataStartHsShaderStage;
    const uint16 mmSpiShaderPgmLoLs     = registerInfo.mmSpiShaderPgmLoLs;

    pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderGraphics>(mmSpiShaderPgmLoLs,
                                                             m_regs.sh.spiShaderPgmLoLs.u32All,
                                                             pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetSeqShRegs(mmSPI_SHADER_PGM_RSRC1_HS,
                                              mmSPI_SHADER_PGM_RSRC2_HS,
                                              ShaderGraphics,
                                              &m_regs.sh.spiShaderPgmRsrc1Hs,
                                              pCmdSpace);

    pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderGraphics>(mmSpiShaderUserDataHs0 + ConstBufTblStartReg,
                                                             m_regs.sh.userDataInternalTable,
                                                             pCmdSpace);

    if (chipProps.gfx9.supportSpp != 0)
    {
        pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderGraphics>(Apu09_1xPlus::mmSPI_SHADER_PGM_CHKSUM_HS,
                                                                 m_regs.sh.spiShaderPgmChksumHs.u32All,
                                                                 pCmdSpace);
    }

    auto dynamic = m_regs.dynamic;

    if (hsStageInfo.wavesPerSh > 0)
    {
        dynamic.spiShaderPgmRsrc3Hs.bits.WAVE_LIMIT = hsStageInfo.wavesPerSh;
    }
#if PAL_AMDGPU_BUILD
    else if (IsGfx9(chipProps.gfxLevel) && (dynamic.spiShaderPgmRsrc3Hs.bits.WAVE_LIMIT == 0))
    {
        // GFX9 GPUs have a HW bug where a wave limit size of 0 does not correctly map to "no limit",
        // potentially breaking high-priority compute.
        dynamic.spiShaderPgmRsrc3Hs.bits.WAVE_LIMIT = m_device.GetMaxWavesPerSh(chipProps, false);
    }
#endif

    if (hsStageInfo.cuEnableMask != 0)
    {
        dynamic.spiShaderPgmRsrc3Hs.bits.CU_EN &= hsStageInfo.cuEnableMask;
        dynamic.spiShaderPgmRsrc4Hs.gfx10Plus.CU_EN =
            Device::AdjustCuEnHi(dynamic.spiShaderPgmRsrc4Hs.gfx10Plus.CU_EN, hsStageInfo.cuEnableMask);
    }

    pCmdSpace = pCmdStream->WriteSetOneShRegIndex(mmSPI_SHADER_PGM_RSRC3_HS,
                                                  dynamic.spiShaderPgmRsrc3Hs.u32All,
                                                  ShaderGraphics,
                                                  index__pfp_set_sh_reg_index__apply_kmd_cu_and_mask,
                                                  pCmdSpace);

    if (IsGfx10Plus(chipProps.gfxLevel))
    {
        pCmdSpace = pCmdStream->WriteSetOneShRegIndex(mmSPI_SHADER_PGM_RSRC4_HS,
                                                      dynamic.spiShaderPgmRsrc4Hs.u32All,
                                                      ShaderGraphics,
                                                      index__pfp_set_sh_reg_index__apply_kmd_cu_and_mask,
                                                      pCmdSpace);
    }

    if (m_pHsPerfDataInfo->regOffset != UserDataNotMapped)
    {
        pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderGraphics>(m_pHsPerfDataInfo->regOffset,
                                                                 m_pHsPerfDataInfo->gpuVirtAddr,
                                                                 pCmdSpace);
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Copies this pipeline chunk's context commands into the specified command space. Returns the next unused DWORD in
// pCmdSpace.
uint32* PipelineChunkHs::WriteContextCommands(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    return pCmdStream->WriteSetSeqContextRegs(mmVGT_HOS_MAX_TESS_LEVEL,
                                              mmVGT_HOS_MIN_TESS_LEVEL,
                                              &m_regs.context.vgtHosMaxTessLevel,
                                              pCmdSpace);
}

} // Gfx9
} // Pal
