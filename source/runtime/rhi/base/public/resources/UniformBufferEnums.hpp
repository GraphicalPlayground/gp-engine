// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "RHIDefinitions.hpp"
#include "templates/Enums.hpp"

namespace gp::rhi::resource
{

/// @brief Index type for global static uniform buffer slots.
using UniformBufferStaticSlot = gp::UInt8;

/// @brief Expected update frequency and lifetime of a uniform buffer.
enum class UniformBufferUsage : gp::UInt8
{
    /// @brief Used for one draw call and then discarded.
    SingleDraw = 0,
    /// @brief Reused across multiple draw calls within the current frame only.
    SingleFrame,
    /// @brief Persisted across multiple frames.
    MultiFrame
};

/// @brief Validation policy applied when constructing or updating a uniform buffer.
enum class UniformBufferValidation : gp::UInt8
{
    None = 0,
    ValidateResources
};

/// @brief Behavior and reflection flags for uniform buffer creation.
enum class UniformBufferFlags : gp::UInt8
{
    None = 0x00u,
    /// @brief Forces a hardware uniform buffer even when the backend uses emulated uniform buffers.
    NoEmulatedUniformBuffer = 1u << 0,
    NeedsReflectedMembers = 1u << 1,
    HasNonGraphOutputs = 1u << 2,
    /// @brief Exposes the uniform buffer to bindless descriptor tables when bindless is active.
    BindlessAccessible = 1u << 4
};

/// @brief Checks whether a static uniform buffer slot index is within valid bounds.
/// @param[in] slot Static slot index to test.
/// @return True when `slot` is less than `kMaxUniformBufferStaticSlots`.
[[nodiscard]] inline constexpr bool isUniformBufferStaticSlotValid(const UniformBufferStaticSlot slot) noexcept
{
    return slot < kMaxUniformBufferStaticSlots;
}

}   // namespace gp::rhi::resource

GP_ENABLE_ENUM_BITWISE_OPERATIONS(gp::rhi::resource::UniformBufferFlags);
