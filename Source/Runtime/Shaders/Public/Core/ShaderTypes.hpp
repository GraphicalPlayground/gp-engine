// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "ShadersBuild.hpp"
#include "Templates/Enums.hpp"

namespace GP::Shaders
{

/// \brief Stage of the GPU pipeline this shader executes in.
/// Maps directly to Vulkan VkShaderStageFlagBits / D3D12 shader visibility.
enum class EShaderStage : UInt32
{
    None = 0,
    Vertex = 1 << 0,
    Hull = 1 << 1,       //<! Tessellation Control (Vulkan)
    Domain = 1 << 2,     //<! Tessellation Evaluation (Vulkan)
    Geometry = 1 << 3,
    Fragment = 1 << 4,   //<! Pixel (D3D12)
    Compute = 1 << 5,
    Mesh = 1 << 6,
    Amplification = 1 << 7,   //<! Task (Vulkan)
    RayGeneration = 1 << 8,
    RayMiss = 1 << 9,
    RayClosestHit = 1 << 10,
    RayAnyHit = 1 << 11,
    RayIntersection = 1 << 12,
    Callable = 1 << 13,

    AllGraphics = Vertex | Hull | Domain | Geometry | Fragment,
    All = 0xFFFFFFFF,
};

/// \brief Shader source language.
enum class EShaderLanguage : UInt8
{
    GLSL,    //<! OpenGL Shading Language
    HLSL,    //<! High-Level Shading Language (DirectX)
    SPIRV,   //<! SPIR-V binary (pre-compiled)
    MSL,     //<! Metal Shading Language
    WGSL,    //<! WebGPU Shading Language
};

/// \brief Target compilation output format.
enum class EShaderTarget : UInt8
{
    SPIRV_1_0,
    SPIRV_1_3,
    SPIRV_1_5,
    SPIRV_1_6,
    DXIL,       //<! DirectX Intermediate Language
    MSL_2_0,    //<! Metal Shading Language 2.0+
    MSL_3_0,
    GLSL_460,   //<! For OpenGL fallback
};

/// \brief Shader model / feature level.
enum class EShaderModel : UInt8
{
    SM_5_0,   //<! DX11 baseline
    SM_6_0,   //<! DX12 wave intrinsics
    SM_6_5,   //<! DX12 ray tracing tier 1.1
    SM_6_6,   //<! DX12 dynamic resources
    SM_6_7,   //<! DX12 work graphs
};

/// \brief Optimization level for shader compilation.
enum class EShaderOptimization : UInt8
{
    None,          //<! -O0 — fastest compile, debug info preserved
    Size,          //<! -Os — minimize code size
    Performance,   //<! -O2 — maximize runtime performance (default)
    Full,          //<! -O3 — aggressive, may change numerical precision
};

/// \brief Severity of a compilation diagnostic message.
enum class EShaderDiagnosticSeverity : UInt8
{
    Info,
    Warning,
    Error,
    Fatal,
};

/// \brief Type of a shader resource.
enum class EShaderResourceType : UInt8
{
    UniformBuffer,           //<! UBO / CBV
    StorageBuffer,           //<! SSBO / UAV
    SampledImage,            //<! Combined image+sampler / SRV
    StorageImage,            //<! Image load/store / UAV
    Sampler,                 //<! Standalone sampler
    UniformTexelBuffer,      //<! Buffer texture / SRV
    StorageTexelBuffer,      //<! Buffer image / UAV
    InputAttachment,         //<! Vulkan subpass input
    AccelerationStructure,   //<! Ray tracing TLAS / BLAS
    PushConstant,            //<! Push constant / root constant
};

/// \brief Scalar types found in shader reflection.
enum class EShaderDataType : UInt8
{
    Float,
    Float2,
    Float3,
    Float4,
    Int,
    Int2,
    Int3,
    Int4,
    UInt,
    UInt2,
    UInt3,
    UInt4,
    Bool,
    Mat2,
    Mat3,
    Mat4,
    Struct,
};

}   // namespace GP::Shaders
