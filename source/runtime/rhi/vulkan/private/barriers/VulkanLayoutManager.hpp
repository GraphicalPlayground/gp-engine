// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "barriers/VulkanBarrierForward.hpp"   // IWYU pragma: keep
#include "barriers/VulkanBarrierTypes.hpp"
#include "barriers/VulkanImageLayout.hpp"
#include "containers/arrays/Vector.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "RHIVulkan.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Tracks the current `VkImageLayout` of every image touched by a command list context.
/// @details Each command list context owns one manager. A manager created write-only only collects the layouts it
///          writes so they can be merged into a central manager later with `transferTo()`, which is how parallel
///          command lists avoid reading layouts another thread may be changing. A read/write manager can name a
///          fallback manager (typically the queue's) to look up images it has not seen yet; layouts found there are
///          copied locally so later changes stay private until transferred.
///
///          Storage is a flat structure of arrays: image handles in one vector, layouts in a parallel vector. Lookups
///          scan the 8-byte handles linearly, which stays in cache for the handful of images a context touches.
/// @note Not thread-safe. Pointers returned by the lookup methods are invalidated by any later insertion.
class GP_RHI_VULKAN_API VulkanLayoutManager final
{
public:
    /// @brief Constructs an empty manager.
    /// @param[in] inWriteOnly True when layouts must never be read back from this instance.
    /// @param[in] inFallback Manager consulted when an image is not tracked locally, or `nullptr`.
    VulkanLayoutManager(bool inWriteOnly, VulkanLayoutManager* inFallback) noexcept;

    VulkanLayoutManager(const VulkanLayoutManager&) = delete;
    VulkanLayoutManager& operator=(const VulkanLayoutManager&) = delete;
    VulkanLayoutManager(VulkanLayoutManager&&) = delete;
    VulkanLayoutManager& operator=(VulkanLayoutManager&&) = delete;

    /// @brief Forgets the layout of an image that is being destroyed.
    /// @param[in] image Image handle.
    void notifyDeletedImage(VkImage image) noexcept;

    /// @brief Looks up the layout of `image` locally, then in the fallback manager.
    /// @note Fails a check on a write-only manager.
    /// @param[in] image Image handle.
    /// @return Tracked layout, or `nullptr` when the image is unknown.
    [[nodiscard]] const VulkanImageLayout* getFullLayout(VkImage image) const noexcept;

    /// @brief Looks up the layout of `texture`, importing it from the fallback or creating it when allowed.
    /// @details A layout found in the fallback manager is copied into this manager so it can be tracked locally.
    /// @note Fails a check on a write-only manager.
    /// @param[in] texture Resolved texture (`image`, `numMips`, `numArrayLayers` and `fullAspectMask` are used).
    /// @param[in] addIfNotFound When true, an unknown image is added in `layoutIfNotFound`.
    /// @param[in] layoutIfNotFound Layout given to a newly added image.
    /// @return Tracked layout, or `nullptr` when the image is unknown and `addIfNotFound` is false.
    [[nodiscard]] const VulkanImageLayout* getFullLayout(
        const VulkanBarrierResource& texture,
        bool addIfNotFound = false,
        VkImageLayout layoutIfNotFound = VK_IMAGE_LAYOUT_UNDEFINED
    ) noexcept;

    /// @brief Replaces the whole layout table of `image`.
    /// @note Prefer the texture overloads; this one cannot verify mip and layer counts, but images without a texture
    ///       object (the swapchain back buffers) need it.
    /// @param[in] image Image handle.
    /// @param[in] newLayout New layout table.
    void setFullLayout(VkImage image, const VulkanImageLayout& newLayout) noexcept;

    /// @brief Moves every subresource of `texture` to `layout`.
    /// @param[in] texture Resolved texture.
    /// @param[in] layout New layout.
    /// @param[in] onlyIfNotFound When true, an image that is already tracked keeps its current layout.
    void
        setFullLayout(const VulkanBarrierResource& texture, VkImageLayout layout, bool onlyIfNotFound = false) noexcept;

    /// @brief Moves the subresources in `subresourceRange` of `texture` to `layout`.
    /// @details An untracked image starts with every subresource in `VK_IMAGE_LAYOUT_UNDEFINED`.
    /// @param[in] texture Resolved texture.
    /// @param[in] subresourceRange Subresources to update.
    /// @param[in] layout New layout.
    void setLayout(
        const VulkanBarrierResource& texture, const VkImageSubresourceRange& subresourceRange, VkImageLayout layout
    ) noexcept;

    /// @brief Moves every tracked layout into `destination`, overwriting entries for the same images, then clears
    ///        this manager.
    /// @param[in,out] destination Manager receiving the layouts.
    void transferTo(VulkanLayoutManager& destination) noexcept;

    /// @brief Returns the number of tracked images.
    /// @return Tracked image count.
    [[nodiscard]] gp::USize getTrackedImageCount() const noexcept
    {
        return m_images.size();
    }

    /// @brief Reports whether this manager is write-only.
    /// @return True when layouts must not be read back.
    [[nodiscard]] bool isWriteOnly() const noexcept
    {
        return m_isWriteOnly;
    }

    /// @brief Returns the fallback manager.
    /// @return Fallback manager, or `nullptr`.
    [[nodiscard]] VulkanLayoutManager* getFallback() const noexcept
    {
        return m_fallback;
    }

private:
    /// @brief Returns the slot index of `image`, or `kInvalidIndex` when untracked.
    /// @param[in] image Image handle.
    /// @return Slot index.
    [[nodiscard]] gp::USize findIndex(VkImage image) const noexcept;

    /// @brief Returns a mutable pointer to the layout of `image`, or `nullptr` when untracked.
    /// @param[in] image Image handle.
    /// @return Layout pointer.
    [[nodiscard]] VulkanImageLayout* findLayout(VkImage image) noexcept;

    /// @brief Appends a new tracked image.
    /// @param[in] image Image handle.
    /// @param[in] layout Layout table.
    /// @return Reference to the stored layout.
    VulkanImageLayout& addLayout(VkImage image, const VulkanImageLayout& layout) noexcept;

    /// @brief Inserts or overwrites the layout of `image`.
    /// @param[in] image Image handle.
    /// @param[in] layout Layout table.
    void addOrReplaceLayout(VkImage image, const VulkanImageLayout& layout) noexcept;

private:
    static constexpr gp::USize kInvalidIndex = ~static_cast<gp::USize>(0u);

    gp::Vector<VkImage> m_images{};
    gp::Vector<VulkanImageLayout> m_layouts{};

    /// @brief Layouts are never read from a write-only manager; it only collects them for a central manager.
    const bool m_isWriteOnly{ false };

    /// @brief Manager used to fetch earlier layouts when parallel command list creation is not supported.
    VulkanLayoutManager* m_fallback{ nullptr };
};

}   // namespace gp::rhi::vulkan
