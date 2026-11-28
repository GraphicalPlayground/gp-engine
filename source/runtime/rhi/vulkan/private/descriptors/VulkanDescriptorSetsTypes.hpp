// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Array.hpp"
#include "CoreMinimal.hpp"                   // IWYU pragma: keep
#include "shaders/VulkanShaderForward.hpp"   // IWYU pragma: keep
#include "VulkanCommon.hpp"
#include "VulkanThirdParty.hpp"              // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Number of stage slots in every per-stage array (`shader_stage::Stage::MaxNumStages`).
inline constexpr gp::USize kVulkanMaxNumDescriptorStages = static_cast<gp::USize>(shader_stage::Stage::MaxNumStages);

/// @brief Number of core descriptor types, `VK_DESCRIPTOR_TYPE_SAMPLER` through `VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT`.
inline constexpr gp::USize kVulkanNumCoreDescriptorTypes = 11u;

/// @brief Number of tracked descriptor types: the core range plus `VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR`.
inline constexpr gp::USize kVulkanNumDescriptorTypeSlots = kVulkanNumCoreDescriptorTypes + 1u;

/// @brief Slot returned for descriptor types the backend does not track (inline uniform blocks, mutable types).
inline constexpr gp::USize kVulkanInvalidDescriptorTypeSlot = kVulkanNumDescriptorTypeSlots;

/// @brief Descriptor count per tracked type, indexed by `getVulkanDescriptorTypeSlot()`.
/// @details A dense array replaces a type-keyed map: twelve counters fit in one cache line and finding the slot of a
///          type is a range check, not a lookup.
using VulkanDescriptorTypeCounts = gp::Array<gp::UInt32, kVulkanNumDescriptorTypeSlots>;

/// @brief Maps a descriptor type to its slot in `VulkanDescriptorTypeCounts`.
/// @param[in] type Descriptor type.
/// @return Slot in `[0, kVulkanNumDescriptorTypeSlots)`, or `kVulkanInvalidDescriptorTypeSlot`.
[[nodiscard]] inline constexpr gp::USize getVulkanDescriptorTypeSlot(const VkDescriptorType type) noexcept
{
    if ((type >= VK_DESCRIPTOR_TYPE_SAMPLER) && (type <= VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT))
    {
        return static_cast<gp::USize>(type);
    }
    if (type == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
    {
        return kVulkanNumCoreDescriptorTypes;
    }
    return kVulkanInvalidDescriptorTypeSlot;
}

/// @brief Maps a slot of `VulkanDescriptorTypeCounts` back to its descriptor type.
/// @param[in] slot Slot in `[0, kVulkanNumDescriptorTypeSlots)`.
/// @return Descriptor type, or `VK_DESCRIPTOR_TYPE_MAX_ENUM` for an invalid slot.
[[nodiscard]] inline constexpr VkDescriptorType getVulkanDescriptorTypeForSlot(const gp::USize slot) noexcept
{
    if (slot < kVulkanNumCoreDescriptorTypes)
    {
        return static_cast<VkDescriptorType>(slot);
    }
    if (slot == kVulkanNumCoreDescriptorTypes)
    {
        return VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    }
    return VK_DESCRIPTOR_TYPE_MAX_ENUM;
}

/// @brief Twelve bytes that identify the contents of one descriptor write, for descriptor set caching.
/// @details Raw handles are not stable keys (the driver can recycle a destroyed handle value), so each resource
///          carries a monotonically increasing id instead. The array of these infos for a set, plus one trailing
///          `layout` entry, is hashed into a `VulkanDescriptorSetKey`. Every variant is three `UInt32`s so the whole
///          union can be hashed and compared as bytes.
union VulkanHashableDescriptorInfo
{
    /// @brief Trailing entry that identifies the set layout.
    struct LayoutInfo
    {
        /// @brief Reserved; always `~0u`.
        gp::UInt32 max0;

        /// @brief Reserved; always `~0u`.
        gp::UInt32 max1;

        /// @brief Handle id of the `VkDescriptorSetLayout`.
        gp::UInt32 layoutId;
    };

    /// @brief Uniform or storage buffer write.
    struct BufferInfo
    {
        /// @brief Allocation handle id of the buffer.
        gp::UInt32 id;

        /// @brief Byte offset.
        gp::UInt32 offset;

        /// @brief Byte range.
        gp::UInt32 range;
    };

    /// @brief Sampler, sampled image, storage image or input attachment write.
    struct ImageInfo
    {
        /// @brief Sampler id, or the default sampler id.
        gp::UInt32 samplerId;

        /// @brief Image view id.
        gp::UInt32 imageViewId;

        /// @brief `VkImageLayout` of the view.
        gp::UInt32 imageLayout;
    };

    /// @brief Texel buffer write.
    struct BufferViewInfo
    {
        /// @brief Buffer view id.
        gp::UInt32 id;

        /// @brief Always zero.
        gp::UInt32 zero1;

        /// @brief Always zero.
        gp::UInt32 zero2;
    };

    /// @brief Layout variant.
    LayoutInfo layout;

    /// @brief Buffer variant.
    BufferInfo buffer;

    /// @brief Image variant.
    ImageInfo image;

    /// @brief Buffer view variant.
    BufferViewInfo bufferView;
};

static_assert(sizeof(VulkanHashableDescriptorInfo) == 3u * sizeof(gp::UInt32), "Hashable infos are hashed as bytes.");

/// @brief Shader headers of every stage of one pipeline, gathered before its descriptor layout is built.
struct VulkanUniformBufferGatherInfo
{
    /// @brief Header per stage, indexed by `shader_stage::Stage`; null for an unused stage.
    gp::Array<const VulkanShaderHeader*, kVulkanMaxNumDescriptorStages> codeHeaders{};
};

/// @brief One cached `VkDescriptorSetLayout` and its id.
struct VulkanDescriptorSetLayoutEntry
{
    /// @brief Native layout.
    VkDescriptorSetLayout handle{ VK_NULL_HANDLE };

    /// @brief Unique non-zero id used in descriptor set cache keys.
    gp::UInt32 handleId{ 0u };
};

}   // namespace gp::rhi::vulkan
