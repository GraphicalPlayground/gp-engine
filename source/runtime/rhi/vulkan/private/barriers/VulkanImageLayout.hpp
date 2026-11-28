// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "barriers/VulkanBarrierForward.hpp"   // IWYU pragma: keep
#include "containers/arrays/Vector.hpp"        // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "RHIVulkan.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Per-subresource `VkImageLayout` table for a single image.
/// @details While every subresource shares one layout, only `mainLayout` is stored and `subresLayouts` stays empty.
///          The first partial update expands the table to `numPlanes * numLayers * numMips` entries laid out plane
///          major, then layer, then mip:
///          `index = plane * (numLayers * numMips) + layer * numMips + mip`.
///          Once every entry matches again, the table collapses back to `mainLayout`.
/// @note Depth/stencil images have two planes; the stencil plane is plane `numPlanes - 1`.
struct GP_RHI_VULKAN_API VulkanImageLayout
{
public:
    /// @brief Constructs a table where every subresource is in `initialLayout`.
    /// @param[in] initialLayout Layout of every subresource.
    /// @param[in] inNumMips Mip level count of the image.
    /// @param[in] inNumLayers Array layer count of the image.
    /// @param[in] aspect Full aspect mask of the image; depth plus stencil yields two planes.
    VulkanImageLayout(
        VkImageLayout initialLayout, gp::UInt32 inNumMips, gp::UInt32 inNumLayers, VkImageAspectFlags aspect
    ) noexcept;

    /// @brief Reports whether every subresource shares `mainLayout`.
    /// @return True when no per-subresource table is stored.
    [[nodiscard]] bool areAllSubresourcesSameLayout() const noexcept
    {
        return subresLayouts.isEmpty();
    }

    /// @brief Returns the layout of one subresource addressed by aspect.
    /// @param[in] layer Array layer, or `~0u` for layer zero.
    /// @param[in] mip Mip level.
    /// @param[in] aspect `VK_IMAGE_ASPECT_STENCIL_BIT` selects the stencil plane; any other aspect selects plane zero.
    /// @return Layout of that subresource.
    [[nodiscard]] VkImageLayout
        getSubresLayout(gp::UInt32 layer, gp::UInt32 mip, VkImageAspectFlagBits aspect) const noexcept;

    /// @brief Returns the layout of one subresource addressed by plane index.
    /// @param[in] layer Array layer, or `~0u` for layer zero.
    /// @param[in] mip Mip level.
    /// @param[in] plane Plane index (`0` for color or depth, `numPlanes - 1` for stencil).
    /// @return Layout of that subresource.
    [[nodiscard]] VkImageLayout getSubresLayout(gp::UInt32 layer, gp::UInt32 mip, gp::UInt32 plane) const noexcept;

    /// @brief Reports whether every subresource in `subresourceRange` is in `layout`.
    /// @param[in] layout Layout to compare against.
    /// @param[in] subresourceRange Subresources to test.
    /// @return True when all of them match.
    [[nodiscard]] bool
        areSubresourcesSameLayout(VkImageLayout layout, const VkImageSubresourceRange& subresourceRange) const noexcept;

    /// @brief Resolves the layer count of `subresourceRange`, expanding `VK_REMAINING_ARRAY_LAYERS`.
    /// @param[in] subresourceRange Range to resolve.
    /// @return Number of layers covered.
    [[nodiscard]] gp::UInt32 getSubresRangeLayerCount(const VkImageSubresourceRange& subresourceRange) const noexcept;

    /// @brief Resolves the mip count of `subresourceRange`, expanding `VK_REMAINING_MIP_LEVELS`.
    /// @param[in] subresourceRange Range to resolve.
    /// @return Number of mips covered.
    [[nodiscard]] gp::UInt32 getSubresRangeMipCount(const VkImageSubresourceRange& subresourceRange) const noexcept;

    /// @brief Drops the per-subresource table when every entry holds the same layout.
    void collapseSubresLayoutsIfSame() noexcept;

    /// @brief Sets the layout of every subresource in `subresourceRange`.
    /// @details Covering the whole image resets the table to `mainLayout` in O(1); otherwise the table is expanded
    ///          and the range is written, then collapsed if it became uniform.
    /// @note `VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL` and `VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL` are rejected: they are
    ///       per-plane `synchronization2` layouts and cannot be tracked for a whole image.
    /// @param[in] layout New layout.
    /// @param[in] subresourceRange Subresources to update.
    void set(VkImageLayout layout, const VkImageSubresourceRange& subresourceRange) noexcept;

public:
    /// @brief Mip level count.
    gp::UInt32 numMips{ 1u };

    /// @brief Array layer count.
    gp::UInt32 numLayers{ 1u };

    /// @brief Plane count (`2` for depth plus stencil, otherwise `1`).
    gp::UInt32 numPlanes{ 1u };

    /// @brief Layout of every subresource while `subresLayouts` is empty.
    VkImageLayout mainLayout{ VK_IMAGE_LAYOUT_UNDEFINED };

    /// @brief Explicit per-subresource layouts; either empty or `numPlanes * numLayers * numMips` entries.
    gp::Vector<VkImageLayout> subresLayouts{};

private:
    /// @brief Returns the first plane touched by `aspectMask`.
    /// @param[in] aspectMask Aspect mask of a subresource range.
    /// @return Plane index.
    [[nodiscard]] gp::UInt32 getFirstPlane(VkImageAspectFlags aspectMask) const noexcept;

    /// @brief Returns one past the last plane touched by `aspectMask`.
    /// @param[in] aspectMask Aspect mask of a subresource range.
    /// @return Plane end index.
    [[nodiscard]] gp::UInt32 getLastPlane(VkImageAspectFlags aspectMask) const noexcept;
};

}   // namespace gp::rhi::vulkan
