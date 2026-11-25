// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Vector.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "resources/VulkanBufferTypes.hpp"
#include "resources/VulkanResourceForward.hpp"   // IWYU pragma: keep
#include "RHIVulkan.hpp"
#include "threading/sync/Mutex.hpp"
#include "VulkanForward.hpp"

namespace gp::rhi::vulkan
{

/// @brief Registry of staging-backed buffer locks waiting for their unlock.
/// @details A lock that hands out a staging pointer records how to resolve it here, keyed by buffer. The matching
///          unlock takes the record back out. Only buffers locked through staging appear, so the table stays tiny
///          and a linear scan beats any hash map.
/// @note Thread-safe; every method takes the internal mutex.
class GP_RHI_VULKAN_API VulkanPendingBufferLockRegistry final
{
public:
    /// @brief Constructs an empty registry.
    VulkanPendingBufferLockRegistry() noexcept = default;

    VulkanPendingBufferLockRegistry(const VulkanPendingBufferLockRegistry&) = delete;
    VulkanPendingBufferLockRegistry& operator=(const VulkanPendingBufferLockRegistry&) = delete;
    VulkanPendingBufferLockRegistry(VulkanPendingBufferLockRegistry&&) = delete;
    VulkanPendingBufferLockRegistry& operator=(VulkanPendingBufferLockRegistry&&) = delete;

    /// @brief Records `pendingLock` for `buffer`.
    /// @note Asserts that `buffer` has no record yet: a buffer can only be locked once at a time.
    /// @param[in] buffer Locked buffer.
    /// @param[in] pendingLock How to resolve the lock on unlock.
    void add(const VulkanBuffer* buffer, const VulkanPendingBufferLock& pendingLock) noexcept;

    /// @brief Removes the record of `buffer` and copies it into `outPendingLock`.
    /// @param[in] buffer Buffer being unlocked.
    /// @param[out] outPendingLock Receives the record when found.
    /// @return True when a record existed.
    [[nodiscard]] bool take(const VulkanBuffer* buffer, VulkanPendingBufferLock& outPendingLock) noexcept;

    /// @brief Reports whether `buffer` has a pending record.
    /// @param[in] buffer Buffer to look up.
    /// @return True when a record exists.
    [[nodiscard]] bool contains(const VulkanBuffer* buffer) const noexcept;

    /// @brief Returns the number of pending records.
    /// @return Record count.
    [[nodiscard]] gp::USize size() const noexcept;

private:
    /// @brief One registry row.
    struct Entry final
    {
        const VulkanBuffer* buffer{ nullptr };
        VulkanPendingBufferLock pendingLock{};
    };

    /// @brief Finds the row of `buffer` (caller holds `m_mutex`).
    /// @param[in] buffer Buffer to look up.
    /// @return Row index, or `m_entries.size()` when absent.
    [[nodiscard]] gp::USize findLocked(const VulkanBuffer* buffer) const noexcept;

    mutable gp::thread::Mutex m_mutex{};
    gp::Vector<Entry> m_entries{};
};

/// @brief Returns the process-wide pending buffer lock registry.
/// @return Registry shared by every `VulkanBuffer`.
[[nodiscard]] GP_RHI_VULKAN_API VulkanPendingBufferLockRegistry& getVulkanPendingBufferLocks() noexcept;

}   // namespace gp::rhi::vulkan

