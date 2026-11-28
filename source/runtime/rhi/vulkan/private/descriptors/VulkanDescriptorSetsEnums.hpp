// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Which uniform buffer bindings become `VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC`.
/// @details A dynamic uniform buffer descriptor stores the buffer and range once; the byte offset is supplied to
///          `vkCmdBindDescriptorSets` every time. Constant data that changes per draw can then be sub-allocated from a
///          ring buffer without allocating or rewriting a descriptor set per draw.
enum class VulkanDynamicUniformBufferMode : gp::UInt8
{
    /// @brief Every uniform buffer stays a regular descriptor.
    None = 0u,

    /// @brief Only the packed globals buffer (binding 0 when a shader has loose parameters) is dynamic.
    PackedGlobals = 1u,

    /// @brief Every uniform buffer is dynamic while `maxDescriptorSetUniformBuffersDynamic` allows it.
    All = 2u
};

}   // namespace gp::rhi::vulkan
