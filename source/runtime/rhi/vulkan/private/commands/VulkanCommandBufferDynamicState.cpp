// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "commands/VulkanCommandBufferDynamicState.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "pipeline/VulkanGraphicsPipelineDesc.hpp"

namespace gp::rhi::vulkan
{

namespace
{

/// @brief Clamps a vector size to a fixed capacity.
/// @param[in] size Number of elements.
/// @param[in] capacity Capacity of the destination array.
/// @return `min(size, capacity)`.
[[nodiscard]] gp::UInt32 clampCount(const gp::USize size, const gp::UInt32 capacity) noexcept
{
    return (size < static_cast<gp::USize>(capacity)) ? static_cast<gp::UInt32>(size) : capacity;
}

/// @brief Compares two viewports field by field.
/// @param[in] lhs First viewport.
/// @param[in] rhs Second viewport.
/// @return True when every field matches.
[[nodiscard]] bool isSameViewport(const VkViewport& lhs, const VkViewport& rhs) noexcept
{
    return (lhs.x == rhs.x) && (lhs.y == rhs.y) && (lhs.width == rhs.width) && (lhs.height == rhs.height) &&
           (lhs.minDepth == rhs.minDepth) && (lhs.maxDepth == rhs.maxDepth);
}

/// @brief Compares two rectangles field by field.
/// @param[in] lhs First rectangle.
/// @param[in] rhs Second rectangle.
/// @return True when every field matches.
[[nodiscard]] bool isSameRect(const VkRect2D& lhs, const VkRect2D& rhs) noexcept
{
    return (lhs.offset.x == rhs.offset.x) && (lhs.offset.y == rhs.offset.y) && (lhs.extent.width == rhs.extent.width) &&
           (lhs.extent.height == rhs.extent.height);
}

}   // namespace

bool VulkanDynamicPipelineStateSnapshot::hasSameColorAttachments(const VulkanGraphicsPipelineDesc& desc) const noexcept
{
    if (static_cast<gp::USize>(desc.colorAttachmentStates.size()) != static_cast<gp::USize>(numColorAttachments))
    {
        return false;
    }
    for (gp::UInt32 index = 0u; index < numColorAttachments; ++index)
    {
        if (!(colorAttachmentStates[index] == desc.colorAttachmentStates[index]))
        {
            return false;
        }
    }
    return true;
}

void VulkanDynamicPipelineStateSnapshot::captureColorAttachments(const VulkanGraphicsPipelineDesc& desc) noexcept
{
    numColorAttachments = clampCount(desc.colorAttachmentStates.size(), kMaxSimultaneousRenderTargets);
    for (gp::UInt32 index = 0u; index < numColorAttachments; ++index)
    {
        colorAttachmentStates[index] = desc.colorAttachmentStates[index];
    }
}

bool VulkanDynamicPipelineStateSnapshot::hasSameVertexInput(const VulkanGraphicsPipelineDesc& desc) const noexcept
{
    if ((static_cast<gp::USize>(desc.vertexBindings.size()) != static_cast<gp::USize>(numVertexBindings)) ||
        (static_cast<gp::USize>(desc.vertexAttributes.size()) != static_cast<gp::USize>(numVertexAttributes)))
    {
        return false;
    }
    for (gp::UInt32 index = 0u; index < numVertexBindings; ++index)
    {
        if (!(vertexBindings[index] == desc.vertexBindings[index]))
        {
            return false;
        }
    }
    for (gp::UInt32 index = 0u; index < numVertexAttributes; ++index)
    {
        if (!(vertexAttributes[index] == desc.vertexAttributes[index]))
        {
            return false;
        }
    }
    return true;
}

void VulkanDynamicPipelineStateSnapshot::captureVertexInput(const VulkanGraphicsPipelineDesc& desc) noexcept
{
    numVertexBindings = clampCount(desc.vertexBindings.size(), kMaxVertexElementCount);
    for (gp::UInt32 index = 0u; index < numVertexBindings; ++index)
    {
        vertexBindings[index] = desc.vertexBindings[index];
    }
    numVertexAttributes = clampCount(desc.vertexAttributes.size(), kMaxVertexElementCount);
    for (gp::UInt32 index = 0u; index < numVertexAttributes; ++index)
    {
        vertexAttributes[index] = desc.vertexAttributes[index];
    }
}

void VulkanCommandBufferDynamicState::reset() noexcept
{
    numViewports = 0u;
    numScissors = 0u;
    stencilRef = 0u;
    blendConstants = gp::Array<gp::Float32, 4u>{};
    minDepthBounds = 0.0f;
    maxDepthBounds = 1.0f;
    lastPipelineState = VulkanDynamicPipelineStateSnapshot{};
    hasViewport = false;
    hasScissor = false;
    hasStencilRef = false;
    hasBlendConstants = false;
    hasDepthBounds = false;
    needsFullUpdate = true;
}

bool VulkanCommandBufferDynamicState::updateViewports(
    const VkViewport* const newViewports, const gp::UInt32 count
) noexcept
{
    const gp::UInt32 clampedCount = (count < kVulkanMaxPendingViewports) ? count : kVulkanMaxPendingViewports;
    bool isDifferent = !hasViewport || (clampedCount != numViewports);
    for (gp::UInt32 index = 0u; !isDifferent && (index < clampedCount); ++index)
    {
        isDifferent = !isSameViewport(viewports[index], newViewports[index]);
    }
    if (!isDifferent)
    {
        return false;
    }

    for (gp::UInt32 index = 0u; index < clampedCount; ++index)
    {
        viewports[index] = newViewports[index];
    }
    numViewports = clampedCount;
    hasViewport = true;
    return true;
}

bool VulkanCommandBufferDynamicState::updateScissors(const VkRect2D* const newScissors, const gp::UInt32 count) noexcept
{
    const gp::UInt32 clampedCount = (count < kVulkanMaxPendingViewports) ? count : kVulkanMaxPendingViewports;
    bool isDifferent = !hasScissor || (clampedCount != numScissors);
    for (gp::UInt32 index = 0u; !isDifferent && (index < clampedCount); ++index)
    {
        isDifferent = !isSameRect(scissors[index], newScissors[index]);
    }
    if (!isDifferent)
    {
        return false;
    }

    for (gp::UInt32 index = 0u; index < clampedCount; ++index)
    {
        scissors[index] = newScissors[index];
    }
    numScissors = clampedCount;
    hasScissor = true;
    return true;
}

bool VulkanCommandBufferDynamicState::updateStencilRef(const gp::UInt32 newStencilRef) noexcept
{
    if (hasStencilRef && (stencilRef == newStencilRef))
    {
        return false;
    }
    stencilRef = newStencilRef;
    hasStencilRef = true;
    return true;
}

bool VulkanCommandBufferDynamicState::updateBlendConstants(const gp::Array<gp::Float32, 4u>& newBlendConstants) noexcept
{
    if (hasBlendConstants && (blendConstants[0] == newBlendConstants[0]) &&
        (blendConstants[1] == newBlendConstants[1]) && (blendConstants[2] == newBlendConstants[2]) &&
        (blendConstants[3] == newBlendConstants[3]))
    {
        return false;
    }
    blendConstants = newBlendConstants;
    hasBlendConstants = true;
    return true;
}

bool VulkanCommandBufferDynamicState::updateDepthBounds(
    const gp::Float32 newMinDepthBounds, const gp::Float32 newMaxDepthBounds
) noexcept
{
    if (hasDepthBounds && (minDepthBounds == newMinDepthBounds) && (maxDepthBounds == newMaxDepthBounds))
    {
        return false;
    }
    minDepthBounds = newMinDepthBounds;
    maxDepthBounds = newMaxDepthBounds;
    hasDepthBounds = true;
    return true;
}

}   // namespace gp::rhi::vulkan
