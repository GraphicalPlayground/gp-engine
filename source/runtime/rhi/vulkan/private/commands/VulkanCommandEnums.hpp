// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Vector.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "device/VulkanQueueFamilySelection.hpp"
#include "RHIVulkan.hpp"
#include "VulkanForward.hpp"

namespace gp::rhi::vulkan
{

/// @brief Execution role classifying a `VulkanCommandBuffer` and its owning `VulkanCommandBufferPool`.
enum class VulkanCommandBufferType : gp::UInt8
{
    Primary = 0u,
    Secondary,
    Parallel,
    Count
};

/// @brief Number of distinct `VulkanCommandBufferType` categories.
inline constexpr gp::USize kVulkanCommandBufferTypeCount = static_cast<gp::USize>(VulkanCommandBufferType::Count);

/// @brief Recording and submission lifecycle state of a `VulkanCommandBuffer`.
enum class VulkanCommandBufferState : gp::UInt8
{
    ReadyForBegin = 0u,
    IsInsideBegin,
    IsInsideRenderPass,
    HasEnded,
    Submitted,
    NotAllocated,
    NeedReset
};

/// @brief Returns a human-readable ASCII name for `queueType` (used in diagnostics and debug labels).
/// @param[in] queueType Hardware queue role.
/// @return Static null-terminated queue name string.
[[nodiscard]] constexpr const char* getVulkanQueueTypeName(const VulkanQueueType queueType) noexcept
{
    switch (queueType)
    {
    case VulkanQueueType::Graphics:
        return "Graphics";
    case VulkanQueueType::AsyncCompute:
        return "AsyncCompute";
    case VulkanQueueType::Transfer:
        return "Transfer";
    default:
        return "Unknown";
    }
}

/// @brief Metadata recorded when a binary `VulkanSemaphore` is signaled by a queue submission.
struct VulkanBinarySemaphoreSignalInfo final
{
    /// @brief Timeline counter value associated with the signaling submission (or `0` when fences are used).
    gp::UInt64 timelineValue{ 0u };

    /// @brief Queue role that signaled the binary semaphore.
    VulkanQueueType queueType{ VulkanQueueType::Graphics };
};

/// @brief Lightweight linear tracker recording in-flight binary semaphores signaled by prior queue submissions.
/// @details Cross-queue binary semaphore counts per submission pass are tiny (typically 0 to 8 entries), so a flat
///          contiguous array avoids hash table allocation overhead while keeping lookups cache-resident.
class GP_RHI_VULKAN_API VulkanSignaledSemaphoreTracker final
{
public:
    /// @brief Key-value record pairing a raw `VulkanSemaphore` pointer with its signaling metadata.
    struct Entry final
    {
        /// @brief Non-owning pointer to the signaled binary semaphore.
        const VulkanSemaphore* semaphore{ nullptr };

        /// @brief Timeline counter value and queue role associated with the signal.
        VulkanBinarySemaphoreSignalInfo info{};
    };

    /// @brief Default constructor.
    VulkanSignaledSemaphoreTracker() noexcept = default;

    /// @brief Records or updates signaling metadata for `semaphore`.
    /// @param[in] semaphore Signaled binary semaphore (ignored if null).
    /// @param[in] info Signaling timeline value and queue role.
    void addOrUpdate(const VulkanSemaphore* const semaphore, const VulkanBinarySemaphoreSignalInfo info) noexcept
    {
        if (semaphore == nullptr)
        {
            return;
        }

        for (gp::USize index = 0u; index < m_entries.size(); ++index)
        {
            if (m_entries[index].semaphore == semaphore)
            {
                m_entries[index].info = info;
                return;
            }
        }

        m_entries.pushBack(Entry{ semaphore, info });
    }

    /// @brief Reports whether `semaphore` is currently tracked as signaled.
    /// @param[in] semaphore Semaphore to query.
    /// @return True when present; false otherwise.
    [[nodiscard]] bool contains(const VulkanSemaphore* const semaphore) const noexcept
    {
        return find(semaphore) != nullptr;
    }

    /// @brief Looks up signaling metadata for `semaphore`.
    /// @param[in] semaphore Semaphore to query.
    /// @return Pointer to `VulkanBinarySemaphoreSignalInfo`, or `nullptr` if not tracked.
    [[nodiscard]] const VulkanBinarySemaphoreSignalInfo* find(const VulkanSemaphore* const semaphore) const noexcept
    {
        if (semaphore == nullptr)
        {
            return nullptr;
        }

        for (gp::USize index = 0u; index < m_entries.size(); ++index)
        {
            if (m_entries[index].semaphore == semaphore)
            {
                return &m_entries[index].info;
            }
        }

        return nullptr;
    }

    /// @brief Removes `semaphore` from the tracker after a consuming wait submission has been issued.
    /// @param[in] semaphore Semaphore to remove.
    /// @return True if an entry was removed; false if not present.
    bool remove(const VulkanSemaphore* const semaphore) noexcept
    {
        if (semaphore == nullptr)
        {
            return false;
        }

        for (gp::USize index = 0u; index < m_entries.size(); ++index)
        {
            if (m_entries[index].semaphore == semaphore)
            {
                const gp::USize lastIndex = m_entries.size() - 1u;
                if (index != lastIndex)
                {
                    m_entries[index] = m_entries[lastIndex];
                }
                m_entries.popBack();
                return true;
            }
        }

        return false;
    }

    /// @brief Clears all tracked entries.
    void clear() noexcept
    {
        m_entries.clear();
    }

    /// @brief Returns the number of currently tracked signaled binary semaphores.
    /// @return Entry count.
    [[nodiscard]] gp::USize size() const noexcept
    {
        return m_entries.size();
    }

    /// @brief Reports whether no signaled binary semaphores are currently tracked.
    /// @return True when empty.
    [[nodiscard]] bool isEmpty() const noexcept
    {
        return m_entries.isEmpty();
    }

private:
    gp::Vector<Entry> m_entries{};
};

}   // namespace gp::rhi::vulkan
