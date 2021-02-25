/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021 Advanced Micro Devices, Inc. All Rights Reserved.
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

#pragma once

#include "palPipelineAbi.h"
#include "palInlineFuncs.h"
#include "palMsgPack.h"

namespace Util
{
namespace Abi
{

using MsgPackOffset = uint32;

struct BinaryData
{
    const void*  pBuffer;
    uint32       sizeInBytes;
};

/// Per-API shader metadata.
struct ShaderMetadata
{
    /// Input shader hash, typically passed in from the client.
    uint64 apiShaderHash[2];
    /// Flags indicating the HW stages this API shader maps to.
    uint32 hardwareMapping;

    union
    {
        struct
        {
            uint8 apiShaderHash   : 1;
            uint8 hardwareMapping : 1;
            uint8 reserved        : 6;
        };
        uint8 uAll;
    } hasEntry;
};

/// Per-hardware stage metadata.
struct HardwareStageMetadata
{
    /// The ELF symbol pointing to this pipeline's stage entry point.
    PipelineSymbolType entryPoint;
    /// Scratch memory size in bytes.
    uint32             scratchMemorySize;
    /// Local Data Share size in bytes.
    uint32             ldsSize;
    /// Performance data buffer size in bytes.
    uint32             perfDataBufferSize;
    /// Number of VGPRs used.
    uint32             vgprCount;
    /// Number of SGPRs used.
    uint32             sgprCount;
    /// If non-zero, indicates the shader was compiled with a directive to instruct the compiler to limit the VGPR usage
    /// to be less than or equal to the specified value (only set if different from HW default).
    uint32             vgprLimit;
    /// SGPR count upper limit (only set if different from HW default).
    uint32             sgprLimit;
    /// Thread-group X/Y/Z dimensions (Compute only).
    uint32             threadgroupDimensions[3];
    /// Wavefront size (only set if different from HW default).
    uint32             wavefrontSize;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 619
    /// Deprecated, unused.
    uint32             maxPrimsPerWave;
#endif

    union
    {
        struct
        {
            /// The shader reads or writes UAVs.
            uint8 usesUavs          : 1;
            /// The shader reads or writes ROVs.
            uint8 usesRovs          : 1;
            /// The shader writes to one or more UAVs.
            uint8 writesUavs        : 1;
            /// The shader writes out a depth value.
            uint8 writesDepth       : 1;
            /// The shader uses append and/or consume operations, either memory or GDS.
            uint8 usesAppendConsume : 1;
            /// The shader uses PrimID.
            uint8 usesPrimId        : 1;
            uint8 reserved          : 2;
        };
        uint8 uAll;
    } flags;

    union
    {
        struct
        {
            uint32 entryPoint            : 1;
            uint32 scratchMemorySize     : 1;
            uint32 ldsSize               : 1;
            uint32 perfDataBufferSize    : 1;
            uint32 vgprCount             : 1;
            uint32 sgprCount             : 1;
            uint32 vgprLimit             : 1;
            uint32 sgprLimit             : 1;
            uint32 threadgroupDimensions : 1;
            uint32 wavefrontSize         : 1;
            uint32 usesUavs              : 1;
            uint32 usesRovs              : 1;
            uint32 writesUavs            : 1;
            uint32 writesDepth           : 1;
            uint32 usesAppendConsume     : 1;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 619
            uint32 maxPrimsPerWave       : 1;
#else
            uint32 placeholder0          : 1;
#endif
            uint32 usesPrimId            : 1;
            uint32 reserved              : 15;
        };
        uint32 uAll;
    } hasEntry;
};

/// Per-pipeline metadata.
struct PipelineMetadata
{
    /// Source name of the pipeline.
    char                  name[256];
    /// Pipeline type, e.g. VsPs.
    PipelineType          type;
    /// Internal compiler hash for this pipeline. Lower 64 bits is the "stable" portion of the hash, used for e.g.
    /// shader replacement lookup. Upper 64 bits is the "unique" portion of the hash, used for e.g. pipeline cache
    /// lookup.
    uint64                internalPipelineHash[2];
    /// Per-API shader metadata.
    ShaderMetadata        shader[static_cast<uint32>(ApiShaderType::Count)];
    /// Per-hardware stage metadata.
    HardwareStageMetadata hardwareStage[static_cast<uint32>(HardwareStage::Count)];
    /// Per-shader function metadata (offset in bytes into the msgpack blob to map of map).
    MsgPackOffset         shaderFunctions;
    /// Hardware register configuration (offset in bytes into the msgpack blob to map).
    MsgPackOffset         registers;
    /// Number of user data entries accessed by this pipeline.
    uint32                userDataLimit;
    /// The user data spill threshold.  0xFFFF for NoUserDataSpilling.
    uint32                spillThreshold;
    /// Size in bytes of LDS space used internally for handling data-passing between the ES and GS shader stages. This
    /// can be zero if the data is passed using off-chip buffers. This value should be used to program all user-SGPRs
    /// which have been marked with "UserDataMapping::EsGsLdsSize" (typically only the GS and VS HW stages will ever
    /// have a user-SGPR so marked).
    uint32                esGsLdsSize;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 619
    /// Address of stream out table entry.
    uint32                streamOutTableAddress;
    /// Address(es) of indirect user data tables. 3 for VK, else 1.
    uint32                indirectUserDataTableAddresses[3];
#endif
    /// Explicit maximum subgroup size for NGG shaders (maximum number of threads in a subgroup).
    uint32                nggSubgroupSize;
    /// Graphics only. Number of PS interpolants.
    uint32                numInterpolants;
    /// Max mesh shader scratch memory used.
    uint32                meshScratchMemorySize;
    /// Name of the client graphics API.
    char                  api[16];
    /// Graphics API shader create info binary blob. Can be defined by the driver using the compiler if they want to be
    /// able to correlate API-specific information used during creation at a later time.
    BinaryData            apiCreateInfo;

    union
    {
        struct
        {
            /// Indicates whether or not the pipeline uses the viewport array index feature. Pipelines which use this
            /// feature can render into all 16 viewports, whereas pipelines which do not use it are restricted to
            /// viewport #0.
            uint8 usesViewportArrayIndex      : 1;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 619
            /// GFX10 only. Determines if wave break size should be calculated at draw time.
            uint8 calcWaveBreakSizeAtDrawTime : 1;
#else
            uint8 placeholder0                : 1;
#endif
            uint8 reserved                    : 6;
        };
        uint8 uAll;
    } flags;

    union
    {
        struct
        {
            uint32 name                           : 1;
            uint32 type                           : 1;
            uint32 internalPipelineHash           : 1;
            uint32 shaderFunctions                : 1;
            uint32 registers                      : 1;
            uint32 userDataLimit                  : 1;
            uint32 spillThreshold                 : 1;
            uint32 usesViewportArrayIndex         : 1;
            uint32 esGsLdsSize                    : 1;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 619
            uint32 streamOutTableAddress          : 1;
            uint32 indirectUserDataTableAddresses : 1;
#else
            uint32 placeholder0                   : 1;
            uint32 placeholder1                   : 1;
#endif
            uint32 nggSubgroupSize                : 1;
            uint32 numInterpolants                : 1;
            uint32 meshScratchMemorySize          : 1;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 619
            uint32 calcWaveBreakSizeAtDrawTime    : 1;
#else
            uint32 placeholder2                   : 1;
#endif
            uint32 placeholder3                   : 1;
            uint32 placeholder4                   : 1;
            uint32 api                            : 1;
            uint32 apiCreateInfo                  : 1;
            uint32 reserved                       : 13;
        };
        uint32 uAll;
    } hasEntry;
};

/// PAL code object metadata.
struct PalCodeObjectMetadata
{
    /// PAL code object metadata (major, minor) version.
    uint32           version[2];
    /// Per-pipeline metadata.
    PipelineMetadata pipeline;

    union
    {
        struct
        {
            uint8 version  : 1;
            uint8 reserved : 7;
        };
        uint8 uAll;
    } hasEntry;
};

namespace PalCodeObjectMetadataKey
{
    static constexpr char Version[]   = "amdpal.version";
    static constexpr char Pipelines[] = "amdpal.pipelines";
};

namespace PipelineMetadataKey
{
    static constexpr char Name[]                           = ".name";
    static constexpr char Type[]                           = ".type";
    static constexpr char InternalPipelineHash[]           = ".internal_pipeline_hash";
    static constexpr char Shaders[]                        = ".shaders";
    static constexpr char HardwareStages[]                 = ".hardware_stages";
    static constexpr char ShaderFunctions[]                = ".shader_functions";
    static constexpr char Registers[]                      = ".registers";
    static constexpr char UserDataLimit[]                  = ".user_data_limit";
    static constexpr char SpillThreshold[]                 = ".spill_threshold";
    static constexpr char UsesViewportArrayIndex[]         = ".uses_viewport_array_index";
    static constexpr char EsGsLdsSize[]                    = ".es_gs_lds_size";
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 619
    static constexpr char StreamOutTableAddress[]          = ".stream_out_table_address";
    static constexpr char IndirectUserDataTableAddresses[] = ".indirect_user_data_table_addresses";
#endif
    static constexpr char NggSubgroupSize[]                = ".nggSubgroupSize";
    static constexpr char NumInterpolants[]                = ".num_interpolants";
    static constexpr char MeshScratchMemorySize[]          = ".mesh_scratch_memory_size";
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 619
    static constexpr char CalcWaveBreakSizeAtDrawTime[]    = ".calc_wave_break_size_at_draw_time";
#endif
    static constexpr char Api[]                            = ".api";
    static constexpr char ApiCreateInfo[]                  = ".api_create_info";
};

namespace HardwareStageMetadataKey
{
    static constexpr char EntryPoint[]            = ".entry_point";
    static constexpr char ScratchMemorySize[]     = ".scratch_memory_size";
    static constexpr char LdsSize[]               = ".lds_size";
    static constexpr char PerfDataBufferSize[]    = ".perf_data_buffer_size";
    static constexpr char VgprCount[]             = ".vgpr_count";
    static constexpr char SgprCount[]             = ".sgpr_count";
    static constexpr char VgprLimit[]             = ".vgpr_limit";
    static constexpr char SgprLimit[]             = ".sgpr_limit";
    static constexpr char ThreadgroupDimensions[] = ".threadgroup_dimensions";
    static constexpr char WavefrontSize[]         = ".wavefront_size";
    static constexpr char UsesUavs[]              = ".uses_uavs";
    static constexpr char UsesRovs[]              = ".uses_rovs";
    static constexpr char WritesUavs[]            = ".writes_uavs";
    static constexpr char WritesDepth[]           = ".writes_depth";
    static constexpr char UsesAppendConsume[]     = ".uses_append_consume";
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 619
    static constexpr char MaxPrimsPerWave[]       = ".max_prims_per_wave";
#endif
    static constexpr char UsesPrimId[]            = ".uses_prim_id";
};

namespace ShaderMetadataKey
{
    static constexpr char ApiShaderHash[]   = ".api_shader_hash";
    static constexpr char HardwareMapping[] = ".hardware_mapping";
};

namespace Metadata
{

Result DeserializePalCodeObjectMetadata(
    MsgPackReader*  pReader,
    PalCodeObjectMetadata*  pMetadata);

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 580
PAL_INLINE Result DeserializePalCodeObjectMetadata(
    MsgPackReader*          pReader,
    PalCodeObjectMetadata*  pMetadata,
    uint32*                 pRegistersOffset)
{
    const Result result = DeserializePalCodeObjectMetadata(pReader, pMetadata);
    if ((result == Result::Success) && (pRegistersOffset != nullptr))
    {
        *pRegistersOffset = (pMetadata->pipeline.hasEntry.registers != 0) ? pMetadata->pipeline.registers : 0xffffffff;
    }
    return result;
}
#endif

Result SerializeEnum(MsgPackWriter* pWriter, PipelineType value);
Result SerializeEnum(MsgPackWriter* pWriter, ApiShaderType value);
Result SerializeEnum(MsgPackWriter* pWriter, ApiShaderSubType value);
Result SerializeEnum(MsgPackWriter* pWriter, HardwareStage value);
Result SerializeEnum(MsgPackWriter* pWriter, PipelineSymbolType value);

template <typename EnumType>
Result SerializeEnumBitflags(MsgPackWriter* pWriter, uint32 bitflags);

} // Metadata

} // Abi
} // Util
