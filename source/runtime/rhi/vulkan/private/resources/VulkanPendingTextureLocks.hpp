// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Vector.hpp"          // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "resources/VulkanResourceForward.hpp"   // IWYU pragma: keep
#include "resources/VulkanTextureTypes.hpp"
#include "RHIVulkan.hpp"
#include "threading/sync/Mutex.hpp"

namespace gp::rhi::vulkan
{

/// @brief Registry of staging-backed texture locks waiting for their unlock.
/// @details Each texture subresource (mip, combined layer) can be locked independently, so records are keyed by the
///          triple `(texture, mip, layer)`. Only textures currently locked appear, which keeps the table tiny: a
///          linear scan beats any hash map.
/// @note Thread-safe; every method takes the internal mutex.
class GP_RHI_VULKAN_API VulkanPendingTextureLockRegistry final
{
public:
    /// @brief Constructs an empty registry.
    VulkanPendingTextureLockRegistry() noexcept = default;

    VulkanPendingTextureLockRegistry(const VulkanPendingTextureLockRegistry&) = delete;
    VulkanPendingTextureLockRegistry& operator=(const VulkanPendingTextureLockRegistry&) = delete;
    VulkanPendingTextureLockRegistry(VulkanPendingTextureLockRegistry&&) = delete;
    VulkanPendingTextureLockRegistry& operator=(VulkanPendingTextureLockRegistry&&) = delete;

    /// @brief Records `pendingLock` for the subresource `(texture, pendingLock.mipIndex, pendingLock.arrayLayer)`.
    /// @note Asserts that the subresource has no record yet: it can only be locked once at a time.
    /// @param[in] texture Locked texture.
    /// @param[in] pendingLock How to resolve the lock on unlock.
    void add(const VulkanTexture* texture, const VulkanPendingTextureLock& pendingLock) noexcept;

    /// @brief Removes the record of a subresource and copies it into `outPendingLock`.
    /// @param[in] texture Texture being unlocked.
    /// @param[in] mipIndex Locked mip.
    /// @param[in] arrayLayer Locked combined layer.
    /// @param[out] outPendingLock Receives the record when found.
    /// @return True when a record existed.
    [[nodiscard]] bool take(
        const VulkanTexture* texture,
        gp::UInt32 mipIndex,
        gp::UInt32 arrayLayer,
        VulkanPendingTextureLock& outPendingLock
    ) noexcept;

    /// @brief Reports whether a subresource has a pending record.
    /// @param[in] texture Texture to look up.
    /// @param[in] mipIndex Mip to look up.
    /// @param[in] arrayLayer Combined layer to look up.
    /// @return True when a record exists.
    [[nodiscard]] bool
        contains(const VulkanTexture* texture, gp::UInt32 mipIndex, gp::UInt32 arrayLayer) const noexcept;

    /// @brief Returns the number of pending records.
    /// @return Record count.
    [[nodiscard]] gp::USize size() const noexcept;

private:
    /// @brief One registry row.
    struct Entry final
    {
        const VulkanTexture* texture{ nullptr };
        VulkanPendingTextureLock pendingLock{};
    };

    /// @brief Finds the row of a subresource (caller holds `m_mutex`).
    /// @param[in] texture Texture to look up.
    /// @param[in] mipIndex Mip to look up.
    /// @param[in] arrayLayer Combined layer to look up.
    /// @return Row index, or `m_entries.size()` when absent.
    [[nodiscard]] gp::USize
        findLocked(const VulkanTexture* texture, gp::UInt32 mipIndex, gp::UInt32 arrayLayer) const noexcept;

    mutable gp::thread::Mutex m_mutex{};
    gp::Vector<Entry> m_entries{};
};

/// @brief Returns the process-wide pending texture lock registry.
/// @return Registry shared by every `VulkanTexture`.
[[nodiscard]] GP_RHI_VULKAN_API VulkanPendingTextureLockRegistry& getVulkanPendingTextureLocks() noexcept;

}   // namespace gp::rhi::vulkan
