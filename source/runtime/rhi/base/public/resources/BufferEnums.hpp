// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "templates/Enums.hpp"

namespace gp::rhi::resource
{

/// @brief Usage, lifetime, and binding flags for GPU buffers.
enum class BufferUsageFlags : gp::UInt32
{
    None = 0u,
    /// @brief Written once at or shortly after creation.
    Static = 1u << 0,
    /// @brief Updated occasionally by the CPU; data lasts until the next update or destruction.
    Dynamic = 1u << 1,
    /// @brief Valid for a single frame; must be rewritten or recreated every frame.
    Volatile = 1u << 2,
    UnorderedAccess = 1u << 3,
    ByteAddressBuffer = 1u << 4,
    SourceCopy = 1u << 5,
    DrawIndirect = 1u << 7,
    ShaderResource = 1u << 8,
    KeepCPUAccessible = 1u << 9,
    FastVRAM = 1u << 10,
    Shared = 1u << 12,
    /// @brief Holds opaque ray tracing acceleration data; cannot bind directly to standard shader stages.
    AccelerationStructure = 1u << 13,
    VertexBuffer = 1u << 14,
    IndexBuffer = 1u << 15,
    StructuredBuffer = 1u << 16,
    MultiGPUAllocate = 1u << 17,
    MultiGPUGraphIgnore = 1u << 18,
    /// @brief Scratch buffer for acceleration structure builds; implies UnorderedAccess.
    RayTracingScratch = (1u << 19) | (1u << 3),
    /// @brief Placeholder without backing GPU memory for streaming.
    NullResource = 1u << 20,
    UniformBuffer = 1u << 21,
    /// @brief Tiled or sparse resource allocated without initial physical memory backing.
    ReservedResource = 1u << 22,

    AnyDynamic = (1u << 1) | (1u << 2)
};

}   // namespace gp::rhi::resource

GP_ENABLE_ENUM_BITWISE_OPERATIONS(gp::rhi::resource::BufferUsageFlags);
