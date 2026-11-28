// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Vector.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"                // IWYU pragma: keep
#include "shaders/ShaderTypes.hpp"
#include "shaders/VulkanShaderHeader.hpp"

namespace gp::rhi::vulkan
{

/// @brief Magic number at the start of every serialized Vulkan shader blob (`'GPVS'` in little-endian order).
inline constexpr gp::UInt32 kVulkanShaderCodeMagic = 0x53565047u;

/// @brief Version of the serialized blob layout; bump it whenever the byte layout changes.
inline constexpr gp::UInt32 kVulkanShaderCodeVersion = 1u;

/// @brief First word of every SPIR-V module.
inline constexpr gp::UInt32 kSpirvMagicNumber = 0x07230203u;

/// @brief Fully decoded content of a serialized Vulkan shader blob.
/// @details Byte layout (all integers little-endian):
///          1. `kVulkanShaderCodeMagic`, `kVulkanShaderCodeVersion`.
///          2. `VulkanShaderHeader`.
///          3. `shader::ShaderResourceTable`.
///          4. Main SPIR-V: 32-bit byte size (non-zero multiple of 4) followed by the words.
///          5. Any-hit SPIR-V, present only when `header.rayGroupAnyHit == SeparateBlob`.
///          6. Intersection SPIR-V, present only when `header.rayGroupIntersection == SeparateBlob`.
///          No trailing bytes are allowed.
struct VulkanShaderCode
{
    /// @brief Descriptor metadata.
    VulkanShaderHeader header{};

    /// @brief Uniform buffer resource table handed to the RHI shader base.
    shader::ShaderResourceTable resourceTable{};

    /// @brief Main SPIR-V module, stored as words so it is always 4-byte aligned.
    gp::Vector<gp::UInt32> spirv{};

    /// @brief Separate any-hit SPIR-V module, empty when not present.
    gp::Vector<gp::UInt32> anyHitSpirv{};

    /// @brief Separate intersection SPIR-V module, empty when not present.
    gp::Vector<gp::UInt32> intersectionSpirv{};
};

}   // namespace gp::rhi::vulkan
