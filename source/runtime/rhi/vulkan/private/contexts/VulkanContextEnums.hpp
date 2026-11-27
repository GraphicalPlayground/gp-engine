// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "templates/Enums.hpp"

namespace gp::rhi::vulkan
{

/// @brief Recording phase of the payload a context currently appends to.
/// @details Phases are strictly ordered inside one payload: semaphore waits come first, then sparse binding, then
///          command buffers, then signals. Asking for an earlier phase than the current one closes the payload and
///          opens a new one, so a wait recorded after a signal never executes before that signal.
enum class VulkanContextPhase : gp::UInt8
{
    Wait = 0,
    UpdateReservedResources,
    Execute,
    Signal
};

/// @brief Bitmask controlling how far `VulkanContextCommon::flushCommands()` blocks.
enum class VulkanFlushFlags : gp::UInt8
{
    None = 0x00,
    WaitForSubmission = 0x01,
    WaitForCompletion = 0x02
};

}   // namespace gp::rhi::vulkan

GP_ENABLE_ENUM_BITWISE_OPERATIONS(gp::rhi::vulkan::VulkanFlushFlags);
