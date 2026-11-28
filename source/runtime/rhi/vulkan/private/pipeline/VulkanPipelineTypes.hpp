// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Array.hpp"
#include "CoreMinimal.hpp"                      // IWYU pragma: keep
#include "pipeline/VulkanPipelineForward.hpp"   // IWYU pragma: keep
#include "VulkanCommon.hpp"
#include "VulkanForward.hpp"                    // IWYU pragma: keep
#include "VulkanThirdParty.hpp"                 // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Number of graphics pipeline stage slots (vertex, pixel, geometry, mesh, task).
inline constexpr gp::UInt32 kNumGraphicsPipelineStages =
    static_cast<gp::UInt32>(shader_stage::Stage::NumGraphicsStages);

/// @brief Number of compute pipeline stage slots.
inline constexpr gp::UInt32 kNumComputePipelineStages = static_cast<gp::UInt32>(shader_stage::Stage::NumComputeStages);

/// @brief Vulkan shader per graphics stage, indexed by `shader_stage::Stage`; null for an unused stage.
using VulkanGraphicsShaderArray = gp::Array<VulkanShader*, kNumGraphicsPipelineStages>;

/// @brief Largest number of specialization constants one shader stage may receive.
/// @details Every constant is a 32-bit word; constant `i` lives at byte offset `4 * i` of the stage's constant block.
inline constexpr gp::UInt32 kMaxNumSpecializationConstants = 32u;

/// @brief Byte size of one specialization constant.
inline constexpr gp::UInt32 kSpecializationConstantSize = 4u;

/// @brief Upper bound on the dynamic states one graphics pipeline enables.
/// @details 3 core states, 10 from extended dynamic state 1 (or 2 viewport/scissor without it), 2 from extended
///          dynamic state 2, 6 from extended dynamic state 3 and 1 vertex input state.
inline constexpr gp::UInt32 kMaxPipelineDynamicStates = 22u;

/// @brief Fallback size, in bytes, charged to the pipeline LRU when the driver reports an empty cache blob.
inline constexpr gp::UInt32 kDefaultPipelineCacheSize = 20u * 1024u;

/// @brief Number of frames after its last use before an evicted pipeline may be destroyed without deferral.
inline constexpr gp::UInt64 kPipelineImmediateDeleteFrameDelay = 3u;

/// @brief Largest number of pipelines `VulkanPipelineStateCacheManager::tickLRU` evicts in one call.
inline constexpr gp::UInt32 kMaxPipelineEvictsPerTick = 5u;

/// @brief Builds the identity specialization map: constant `i` is read from offset `4 * i`, 4 bytes wide.
/// @details One table serves every stage and every pipeline, because the specialization data of a stage is always a
///          packed array of 32-bit words. The map entry count passed to Vulkan is the stage's constant count.
/// @return Map entries for `kMaxNumSpecializationConstants` constants.
[[nodiscard]] inline constexpr gp::Array<VkSpecializationMapEntry, kMaxNumSpecializationConstants>
    makeVulkanSpecializationMapEntries() noexcept
{
    gp::Array<VkSpecializationMapEntry, kMaxNumSpecializationConstants> entries{};
    for (gp::UInt32 index = 0u; index < kMaxNumSpecializationConstants; ++index)
    {
        entries[index].constantID = index;
        entries[index].offset = index * kSpecializationConstantSize;
        entries[index].size = kSpecializationConstantSize;
    }
    return entries;
}

/// @brief Process-wide identity specialization map shared by every pipeline.
inline constexpr gp::Array<VkSpecializationMapEntry, kMaxNumSpecializationConstants> kVulkanSpecializationMapEntries =
    makeVulkanSpecializationMapEntries();

/// @brief Device features that decide which dynamic states a graphics pipeline enables.
/// @details A plain snapshot of four extension bits, so the dynamic state list can be built (and tested) without a
///          device. Build it with `makeVulkanDynamicStateFeatures(device)`.
struct VulkanDynamicStateFeatures
{
    /// @brief `VK_EXT_extended_dynamic_state` is enabled.
    bool hasExtendedDynamicState1{ false };

    /// @brief `VK_EXT_extended_dynamic_state2` is enabled.
    bool hasExtendedDynamicState2{ false };

    /// @brief `VK_EXT_extended_dynamic_state3` is enabled.
    bool hasExtendedDynamicState3{ false };

    /// @brief `VK_EXT_vertex_input_dynamic_state` is enabled.
    bool hasVertexInputDynamicState{ false };
};

/// @brief Fixed-capacity list of `VkDynamicState` values enabled on a graphics pipeline.
/// @details Built on the stack during pipeline creation; no heap allocation on the pipeline creation path.
struct VulkanDynamicStateList
{
    /// @brief Enabled states; only the first `count` entries are meaningful.
    gp::Array<VkDynamicState, kMaxPipelineDynamicStates> states{};

    /// @brief Number of enabled states.
    gp::UInt32 count{ 0u };

    /// @brief Appends one dynamic state.
    /// @param[in] state State to enable.
    void add(const VkDynamicState state) noexcept
    {
        states[count] = state;
        ++count;
    }

    /// @brief Reports whether `state` is in the list.
    /// @param[in] state State to look for.
    /// @return True when the state is enabled.
    [[nodiscard]] bool contains(const VkDynamicState state) const noexcept
    {
        for (gp::UInt32 index = 0u; index < count; ++index)
        {
            if (states[index] == state)
            {
                return true;
            }
        }
        return false;
    }
};

}   // namespace gp::rhi::vulkan
