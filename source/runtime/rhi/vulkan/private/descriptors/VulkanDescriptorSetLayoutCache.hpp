// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Vector.hpp"                  // IWYU pragma: keep
#include "CoreMinimal.hpp"                               // IWYU pragma: keep
#include "descriptors/VulkanDescriptorSetsForward.hpp"   // IWYU pragma: keep
#include "descriptors/VulkanDescriptorSetsLayoutInfo.hpp"
#include "descriptors/VulkanDescriptorSetsTypes.hpp"
#include "RHIVulkan.hpp"
#include "threading/sync/Mutex.hpp"
#include "VulkanForward.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Owner of every `VkDescriptorSetLayout` of a device, deduplicated by binding contents.
/// @details Pipelines with different shaders very often share the bindings of one stage (every opaque pixel shader of a
///          material family, for example). Deduplicating set layouts keeps their number small and, more importantly,
///          makes descriptor sets allocated for one pipeline compatible with another: Vulkan only lets a set be bound
///          to a pipeline whose layout uses an identical set layout.
///
///          Entries are kept sorted by binding hash and searched with a binary search; equal hashes are then compared
///          byte for byte. Each created layout gets a non-zero id, used instead of the handle in descriptor set cache
///          keys because a destroyed handle value can be reused by the driver.
/// @note Thread-safe. The cache must outlive every `VulkanDescriptorSetsLayout` compiled against it; it destroys the
///       layouts in its destructor. Owned by the pipeline state cache once that exists.
class GP_RHI_VULKAN_API VulkanDescriptorSetLayoutCache
{
public:
    /// @brief Creates an empty cache.
    /// @param[in] device Logical device that creates the layouts.
    /// @param[in] deferredDeletionQueue Queue used to release layouts; null destroys them immediately.
    explicit VulkanDescriptorSetLayoutCache(
        VulkanDevice& device, VulkanDeferredDeletionQueue* deferredDeletionQueue = nullptr
    ) noexcept;

    /// @brief Releases every layout.
    ~VulkanDescriptorSetLayoutCache() noexcept;

    VulkanDescriptorSetLayoutCache(const VulkanDescriptorSetLayoutCache&) = delete;
    VulkanDescriptorSetLayoutCache& operator=(const VulkanDescriptorSetLayoutCache&) = delete;
    VulkanDescriptorSetLayoutCache(VulkanDescriptorSetLayoutCache&&) = delete;
    VulkanDescriptorSetLayoutCache& operator=(VulkanDescriptorSetLayoutCache&&) = delete;

    /// @brief Returns the layout of `setLayout`, creating it on first use.
    /// @param[in] setLayout Bindings of the set; its hash must be current.
    /// @param[out] outEntry Receives the handle and its id.
    /// @return False when `vkCreateDescriptorSetLayout` fails or the device is offline.
    [[nodiscard]] bool findOrCreate(
        const VulkanDescriptorSetsLayoutInfo::SetLayout& setLayout, VulkanDescriptorSetLayoutEntry& outEntry
    ) noexcept;

    /// @brief Returns the number of cached layouts.
    /// @return Layout count.
    [[nodiscard]] gp::USize getNumLayouts() const noexcept;

    /// @brief Returns the device that owns the layouts.
    /// @return Logical device.
    [[nodiscard]] VulkanDevice& getDevice() const noexcept
    {
        return m_device;
    }

private:
    /// @brief One cached layout.
    struct Entry
    {
        /// @brief Bindings the layout was created from.
        VulkanDescriptorSetsLayoutInfo::SetLayout setLayout{};

        /// @brief Native layout and its id.
        VulkanDescriptorSetLayoutEntry layout{};
    };

private:
    /// @brief Logical device.
    VulkanDevice& m_device;

    /// @brief Queue used to release layouts; may be null.
    VulkanDeferredDeletionQueue* m_deferredDeletionQueue{ nullptr };

    /// @brief Guards every member below.
    mutable gp::thread::Mutex m_mutex{};

    /// @brief Cached layouts, sorted by `setLayout.hash`.
    gp::Vector<Entry> m_entries{};

    /// @brief Last id handed out.
    gp::UInt32 m_lastHandleId{ 0u };
};

}   // namespace gp::rhi::vulkan
