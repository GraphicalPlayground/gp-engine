// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"                               // IWYU pragma: keep
#include "descriptors/VulkanDescriptorSetsForward.hpp"   // IWYU pragma: keep
#include "RHIVulkan.hpp"
#include "VulkanThirdParty.hpp"                          // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Per-context view of the pool set container that serves the descriptor sets of one frame.
/// @details A command context records into one container per frame. The container is acquired lazily, on the first
///          descriptor allocation, and handed back to the manager when the context finishes the frame; the manager
///          resets it once the GPU has consumed every set allocated from it.
///
///          The acquirer also decides when sets must be reallocated: a descriptor state that did not change keeps
///          its sets, unless they come from an earlier container (a new frame) or from a typed pool set created just
///          now (the state's old sets came from somewhere else).
/// @note Not thread-safe; owned by one command context on the recording thread.
class GP_RHI_VULKAN_API VulkanDescriptorPoolSetAcquirer
{
public:
    /// @brief Creates an acquirer drawing from `manager`.
    /// @param[in] manager Pool manager; must outlive the acquirer.
    explicit VulkanDescriptorPoolSetAcquirer(VulkanDescriptorPoolsManager& manager) noexcept;

    /// @brief Hands the current container back to the manager.
    ~VulkanDescriptorPoolSetAcquirer() noexcept;

    VulkanDescriptorPoolSetAcquirer(const VulkanDescriptorPoolSetAcquirer&) = delete;
    VulkanDescriptorPoolSetAcquirer& operator=(const VulkanDescriptorPoolSetAcquirer&) = delete;
    VulkanDescriptorPoolSetAcquirer(VulkanDescriptorPoolSetAcquirer&&) = delete;
    VulkanDescriptorPoolSetAcquirer& operator=(VulkanDescriptorPoolSetAcquirer&&) = delete;

    /// @brief Allocates sets for `layout` when they must be rewritten.
    /// @param[in] layout Compiled set layouts.
    /// @param[in] needDescriptors True when the caller's resources changed.
    /// @param[in] currentFrame Frame number; stamps a newly acquired container.
    /// @param[out] outSets Receives one set per set layout when the function returns true.
    /// @return True when new sets were allocated; false when the old sets stay valid or allocation failed.
    [[nodiscard]] bool acquirePoolSetAndDescriptorsIfNeeded(
        const VulkanDescriptorSetsLayout& layout,
        bool needDescriptors,
        gp::UInt64 currentFrame,
        VkDescriptorSet* outSets
    ) noexcept;

    /// @brief Hands the current container back to the manager; the next allocation acquires a new one.
    void release() noexcept;

    /// @brief Hands the current container to the caller without releasing it.
    /// @details Used when a context finalizes: the container must outlive the GPU work that reads its sets, so its
    ///          ownership moves into the payload that releases it on completion.
    /// @return Detached container, or `nullptr` when none was acquired.
    [[nodiscard]] VulkanDescriptorPoolSetContainer* detachPoolSetContainer() noexcept
    {
        VulkanDescriptorPoolSetContainer* const container = m_container;
        m_container = nullptr;
        return container;
    }

    /// @brief Reports whether a container is currently held.
    /// @return True between the first allocation of a frame and `release()`.
    [[nodiscard]] bool hasPoolSetContainer() const noexcept
    {
        return m_container != nullptr;
    }

    /// @brief Returns the container currently held.
    /// @return Container, or null.
    [[nodiscard]] VulkanDescriptorPoolSetContainer* getPoolSetContainer() const noexcept
    {
        return m_container;
    }

private:
    /// @brief Manager the containers come from.
    VulkanDescriptorPoolsManager& m_manager;

    /// @brief Container of the current frame; null until the first allocation.
    VulkanDescriptorPoolSetContainer* m_container{ nullptr };
};

}   // namespace gp::rhi::vulkan
