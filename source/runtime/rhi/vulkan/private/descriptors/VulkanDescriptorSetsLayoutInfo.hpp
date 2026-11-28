// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Array.hpp"
#include "containers/arrays/Vector.hpp"                  // IWYU pragma: keep
#include "CoreMinimal.hpp"                               // IWYU pragma: keep
#include "descriptors/VulkanDescriptorSetsEnums.hpp"
#include "descriptors/VulkanDescriptorSetsForward.hpp"   // IWYU pragma: keep
#include "descriptors/VulkanDescriptorSetsTypes.hpp"
#include "RHIVulkan.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Descriptor layout of one pipeline, built from its shader headers; holds no Vulkan objects.
/// @details This is the CPU description that later becomes `VkDescriptorSetLayout`s and a `VkPipelineLayout`.
///          The backend uses **one descriptor set per shader stage**: set 0 holds the vertex (or compute) bindings,
///          set 1 the pixel bindings, and so on, indexed by `shader_stage::Stage`. A binding index inside a set is the
///          binding index the shader compiler emitted, so no remapping table is needed at draw time.
///
///          `finalizeBindings()` walks the bindings of every stage once and:
///          1. Promotes uniform buffers to dynamic uniform buffers according to `VulkanDynamicUniformBufferMode`.
///          2. Skips uniform buffers that carry no constant data (resource-only uniform buffers).
///          3. Counts descriptors per type (pool sizing) and image/buffer infos per stage (write container sizing).
///          4. Assigns a *types usage id*: layouts with the same per-type counts share an id, and descriptor pools are
///             keyed by it so pipelines with equivalent needs draw from the same pools.
///          5. Hashes everything, bind point included, so graphics and compute layouts never collide.
class GP_RHI_VULKAN_API VulkanDescriptorSetsLayoutInfo
{
public:
    /// @brief Bindings of one descriptor set.
    struct SetLayout
    {
        /// @brief Bindings in emission order; their `binding` fields are the shader binding indices.
        gp::Vector<VkDescriptorSetLayoutBinding> layoutBindings{};

        /// @brief CRC-32 of `layoutBindings`; refreshed by `generateHash()`.
        gp::UInt32 hash{ 0u };

        /// @brief Recomputes `hash` from the raw bytes of `layoutBindings`.
        /// @note Bindings are zero-initialized before use, so the bytes, padding included, are deterministic.
        void generateHash() noexcept;

        /// @brief Compares hashes first, then every binding byte, so CRC-32 collisions cannot alias two layouts.
        /// @param[in] rhs Layout to compare against.
        /// @return True when both layouts hold the same bindings.
        [[nodiscard]] bool operator==(const SetLayout& rhs) const noexcept;
    };

    /// @brief Per-stage summary used to size write containers and to validate bound resources.
    struct StageInfo
    {
        /// @brief Descriptor type of every binding of the stage, indexed by binding.
        gp::Vector<VkDescriptorType> types{};

        /// @brief Byte size of the packed globals uniform buffer; zero when the stage has none.
        gp::UInt32 packedGlobalsSize{ 0u };

        /// @brief Number of uniform buffers with constant data.
        gp::UInt32 numBoundUniformBuffers{ 0u };

        /// @brief Number of `VkDescriptorImageInfo`s the stage writes.
        gp::UInt16 numImageInfos{ 0u };

        /// @brief Number of `VkDescriptorBufferInfo`s the stage writes.
        gp::UInt16 numBufferInfos{ 0u };

        /// @brief Number of acceleration structure writes.
        gp::UInt16 numAccelerationStructures{ 0u };

        /// @brief Reports whether the stage declares nothing.
        /// @return True when there are no bindings, no packed globals and no bound uniform buffers.
        [[nodiscard]] bool isEmpty() const noexcept;

        /// @brief Memberwise comparison.
        /// @param[in] rhs Stage to compare against.
        /// @return True when every field and every type matches.
        [[nodiscard]] bool operator==(const StageInfo& rhs) const noexcept;
    };

    /// @brief Creates an empty, unfinalized layout description.
    VulkanDescriptorSetsLayoutInfo() noexcept = default;

    /// @brief Builds the bindings of every stage from the gathered shader headers.
    /// @details Must be called once, on an empty description. Uniform buffer promotion is capped by
    ///          `limits.maxDescriptorSetUniformBuffersDynamic` across the whole layout. Bindless layouts never promote
    ///          uniform buffers: bindless uniform buffers are fetched through the descriptor heap instead.
    /// @param[in] bindPoint `VK_PIPELINE_BIND_POINT_GRAPHICS` or `VK_PIPELINE_BIND_POINT_COMPUTE`.
    /// @param[in] limits Device limits.
    /// @param[in] gatherInfo Shader header per stage.
    /// @param[in] usesBindless True when the pipeline reads resources through bindless heaps.
    /// @param[in] dynamicMode Which uniform buffers become dynamic descriptors.
    /// @return False when already finalized, when the bind point is unsupported, or when a header is inconsistent
    ///         (binding indices that do not follow emission order, unsupported descriptor types, counter overflow).
    [[nodiscard]] bool finalizeBindings(
        VkPipelineBindPoint bindPoint,
        const VkPhysicalDeviceLimits& limits,
        const VulkanUniformBufferGatherInfo& gatherInfo,
        bool usesBindless,
        VulkanDynamicUniformBufferMode dynamicMode
    ) noexcept;

    /// @brief Same as the full overload, with the promotion mode read from `r.Vulkan.DynamicGlobalUBs`.
    /// @param[in] bindPoint `VK_PIPELINE_BIND_POINT_GRAPHICS` or `VK_PIPELINE_BIND_POINT_COMPUTE`.
    /// @param[in] limits Device limits.
    /// @param[in] gatherInfo Shader header per stage.
    /// @param[in] usesBindless True when the pipeline reads resources through bindless heaps.
    /// @return See the full overload.
    [[nodiscard]] bool finalizeBindings(
        VkPipelineBindPoint bindPoint,
        const VkPhysicalDeviceLimits& limits,
        const VulkanUniformBufferGatherInfo& gatherInfo,
        bool usesBindless
    ) noexcept;

    /// @brief Recomputes the set hashes and the layout hash.
    /// @param[in] bindPoint Bind point stored in, and hashed into, the layout.
    void generateHash(VkPipelineBindPoint bindPoint) noexcept;

    /// @brief Returns how many descriptors of `type` the layout declares, across every set.
    /// @param[in] type Descriptor type.
    /// @return Descriptor count; zero for untracked types.
    [[nodiscard]] gp::UInt32 getTypesUsed(VkDescriptorType type) const noexcept;

    /// @brief Returns the descriptor counts of every tracked type.
    /// @return Counts indexed by `getVulkanDescriptorTypeSlot()`.
    [[nodiscard]] const VulkanDescriptorTypeCounts& getLayoutTypes() const noexcept
    {
        return m_layoutTypes;
    }

    /// @brief Returns the set layouts, indexed by stage.
    /// @return Set layouts; trailing stages without bindings have no entry.
    [[nodiscard]] const gp::Vector<SetLayout>& getLayouts() const noexcept
    {
        return m_setLayouts;
    }

    /// @brief Returns the per-stage summary.
    /// @param[in] stage Stage index in `[0, kVulkanMaxNumDescriptorStages)`.
    /// @return Stage summary.
    [[nodiscard]] const StageInfo& getStageInfo(const gp::USize stage) const noexcept
    {
        return m_stageInfos[stage];
    }

    /// @brief Returns the layout hash.
    /// @return Hash computed by `generateHash()`, or zero before it.
    [[nodiscard]] gp::UInt32 getHash() const noexcept
    {
        return m_hash;
    }

    /// @brief Returns the id shared by every layout with the same per-type descriptor counts.
    /// @return Types usage id, or `~0u` before `finalizeBindings()`.
    [[nodiscard]] gp::UInt32 getTypesUsageID() const noexcept
    {
        return m_typesUsageID;
    }

    /// @brief Returns the bind point the layout was hashed for.
    /// @return Bind point, or `VK_PIPELINE_BIND_POINT_MAX_ENUM` before `generateHash()`.
    [[nodiscard]] VkPipelineBindPoint getBindPoint() const noexcept
    {
        return m_bindPoint;
    }

    /// @brief Reports whether any stage reads input attachments.
    /// @return True when the layout declares at least one input attachment.
    [[nodiscard]] bool hasInputAttachments() const noexcept
    {
        return getTypesUsed(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT) > 0u;
    }

    /// @brief Full comparison: hash, bind point, types usage id, every set and every stage.
    /// @param[in] rhs Layout to compare against.
    /// @return True when both layouts are interchangeable.
    [[nodiscard]] bool operator==(const VulkanDescriptorSetsLayoutInfo& rhs) const noexcept;

protected:
    /// @brief Assigns `m_typesUsageID` from the per-type counts.
    /// @details Uses a process-wide registry, guarded by a mutex, mapping a CRC of the counts to a dense id.
    void compileTypesUsageID() noexcept;

    /// @brief Appends one binding to a set and updates every counter.
    /// @param[in] setIndex Set (stage) index.
    /// @param[in] binding Binding to append; its type must already be recorded in the stage types.
    /// @return False for an untracked descriptor type, a type mismatch or a counter overflow.
    [[nodiscard]] bool addDescriptor(gp::USize setIndex, const VkDescriptorSetLayoutBinding& binding) noexcept;

protected:
    /// @brief Descriptor count per tracked type.
    VulkanDescriptorTypeCounts m_layoutTypes{};

    /// @brief Set layouts indexed by stage.
    gp::Vector<SetLayout> m_setLayouts{};

    /// @brief Per-stage summary.
    gp::Array<StageInfo, kVulkanMaxNumDescriptorStages> m_stageInfos{};

    /// @brief Layout hash.
    gp::UInt32 m_hash{ 0u };

    /// @brief Types usage id, or `~0u` when not compiled.
    gp::UInt32 m_typesUsageID{ ~0u };

    /// @brief Bind point the layout was hashed for.
    VkPipelineBindPoint m_bindPoint{ VK_PIPELINE_BIND_POINT_MAX_ENUM };
};

}   // namespace gp::rhi::vulkan
