// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Vector.hpp"                  // IWYU pragma: keep
#include "CoreMinimal.hpp"                               // IWYU pragma: keep
#include "descriptors/VulkanDescriptorSetsForward.hpp"   // IWYU pragma: keep
#include "descriptors/VulkanTypedDescriptorPoolSet.hpp"
#include "memory/pointers/UniquePtr.hpp"
#include "RHIVulkan.hpp"
#include "VulkanForward.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Every typed pool set one command list context allocates from during a frame.
/// @details A context acquires a container from `VulkanDescriptorPoolsManager` when it starts recording and releases
///          it once the GPU has finished with the command buffers that reference its sets. Inside the container, one
///          `VulkanTypedDescriptorPoolSet` exists per layout shape, keyed by the types usage id (or by the layout hash
///          when `GP_VULKAN_HASH_POOLS_WITH_TYPES_USAGE_ID` is off).
/// @note Not thread-safe; used by one context at a time.
class GP_RHI_VULKAN_API VulkanDescriptorPoolSetContainer
{
public:
    /// @brief Creates an empty container, marked used.
    /// @param[in] device Logical device.
    /// @param[in] currentFrame Frame number the container is acquired on.
    VulkanDescriptorPoolSetContainer(VulkanDevice& device, gp::UInt64 currentFrame) noexcept;

    /// @brief Destroys every typed pool set.
    ~VulkanDescriptorPoolSetContainer() noexcept = default;

    VulkanDescriptorPoolSetContainer(const VulkanDescriptorPoolSetContainer&) = delete;
    VulkanDescriptorPoolSetContainer& operator=(const VulkanDescriptorPoolSetContainer&) = delete;
    VulkanDescriptorPoolSetContainer(VulkanDescriptorPoolSetContainer&&) = delete;
    VulkanDescriptorPoolSetContainer& operator=(VulkanDescriptorPoolSetContainer&&) = delete;

    /// @brief Returns the pool set for the shape of `layout`, creating it on first use.
    /// @param[in] layout Compiled layout.
    /// @return Typed pool set; stays valid until the container is destroyed.
    [[nodiscard]] VulkanTypedDescriptorPoolSet& acquireTypedPoolSet(const VulkanDescriptorSetsLayout& layout) noexcept;

    /// @brief Resets every typed pool set; the pools themselves are kept for reuse.
    void reset() noexcept;

    /// @brief Marks the container used or unused.
    /// @param[in] isUsed New state.
    /// @param[in] currentFrame Frame number; recorded as the last use when `isUsed` is true.
    void setUsed(bool isUsed, gp::UInt64 currentFrame) noexcept;

    /// @brief Reports whether no context holds the container.
    /// @return True when unused.
    [[nodiscard]] bool isUnused() const noexcept
    {
        return !m_isUsed;
    }

    /// @brief Returns the frame the container was last acquired on.
    /// @return Frame number.
    [[nodiscard]] gp::UInt64 getLastFrameUsed() const noexcept
    {
        return m_lastFrameUsed;
    }

    /// @brief Returns the number of typed pool sets.
    /// @return Typed pool set count.
    [[nodiscard]] gp::USize getNumTypedPoolSets() const noexcept
    {
        return static_cast<gp::USize>(m_typedPoolSets.size());
    }

    /// @brief Returns the pool key of a layout.
    /// @param[in] layout Compiled layout.
    /// @return Types usage id, or the layout hash when pools are not keyed by types usage.
    [[nodiscard]] static gp::UInt32 computePoolKey(const VulkanDescriptorSetsLayout& layout) noexcept;

private:
    /// @brief One typed pool set and its key.
    struct Entry
    {
        /// @brief Pool key; entries are sorted by it.
        gp::UInt32 key{ 0u };

        /// @brief Pool set.
        gp::UniquePtr<VulkanTypedDescriptorPoolSet> poolSet{};
    };

private:
    /// @brief Logical device.
    VulkanDevice& m_device;

    /// @brief Typed pool sets sorted by key.
    gp::Vector<Entry> m_typedPoolSets{};

    /// @brief Frame the container was last acquired on.
    gp::UInt64 m_lastFrameUsed{ 0u };

    /// @brief True while a context holds the container.
    bool m_isUsed{ true };
};

}   // namespace gp::rhi::vulkan
