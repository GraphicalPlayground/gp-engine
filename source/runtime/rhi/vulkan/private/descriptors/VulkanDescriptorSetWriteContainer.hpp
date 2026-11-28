// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Vector.hpp"                  // IWYU pragma: keep
#include "CoreMinimal.hpp"                               // IWYU pragma: keep
#include "descriptors/VulkanDescriptorSetsForward.hpp"   // IWYU pragma: keep
#include "descriptors/VulkanDescriptorSetsTypes.hpp"
#include "VulkanThirdParty.hpp"                          // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Storage behind every `VulkanDescriptorSetWriter` of one pipeline.
/// @details A compute pipeline uses the arrays as they are. A graphics pipeline sizes them for all its stages at once
///          and hands each stage's writer a sub-range, so the descriptor writes of a draw live in a few contiguous
///          arrays (one `vkUpdateDescriptorSets` call) instead of many small allocations. The arrays must not be
///          resized after the writers are set up: the writers and the write structs point into them.
struct VulkanDescriptorSetWriteContainer
{
    /// @brief Cache keys, one per write plus one layout entry per set; empty when the descriptor set cache is off.
    gp::Vector<VulkanHashableDescriptorInfo> hashableDescriptorInfo{};

    /// @brief Image infos referenced by image, sampler and input attachment writes.
    gp::Vector<VkDescriptorImageInfo> descriptorImageInfo{};

    /// @brief Buffer infos referenced by uniform and storage buffer writes.
    gp::Vector<VkDescriptorBufferInfo> descriptorBufferInfo{};

    /// @brief Write structs, one per binding.
    gp::Vector<VkWriteDescriptorSet> descriptorWrites{};

    /// @brief Acceleration structure handles referenced by `accelerationStructureWrites`.
    gp::Vector<VkAccelerationStructureKHR> accelerationStructures{};

    /// @brief Acceleration structure extension structs chained to their write.
    gp::Vector<VkWriteDescriptorSetAccelerationStructureKHR> accelerationStructureWrites{};

    /// @brief Dynamic offset index of every binding; only meaningful for dynamic uniform buffers.
    gp::Vector<gp::UInt8> bindingToDynamicOffsetMap{};
};

}   // namespace gp::rhi::vulkan
