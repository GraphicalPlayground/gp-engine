// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "contexts/VulkanDrawUtils.hpp"
#include "commands/CommandTypes.hpp"
#include "CoreMinimal.hpp"
#include "diagnostics/AssertionMacros.hpp"
#include <cstddef>

namespace gp::rhi::vulkan
{

// Indirect argument buffers are written by the RHI and consumed verbatim by the GPU, so the RHI structs must be
// bit-compatible with the Vulkan command layouts.
static_assert(sizeof(command::DrawIndirectParameters) == sizeof(VkDrawIndirectCommand));
static_assert(
    offsetof(command::DrawIndirectParameters, vertexCountPerInstance) == offsetof(VkDrawIndirectCommand, vertexCount)
);
static_assert(
    offsetof(command::DrawIndirectParameters, instanceCount) == offsetof(VkDrawIndirectCommand, instanceCount)
);
static_assert(
    offsetof(command::DrawIndirectParameters, startVertexLocation) == offsetof(VkDrawIndirectCommand, firstVertex)
);
static_assert(
    offsetof(command::DrawIndirectParameters, startInstanceLocation) == offsetof(VkDrawIndirectCommand, firstInstance)
);

static_assert(sizeof(command::DrawIndexedIndirectParameters) == sizeof(VkDrawIndexedIndirectCommand));
static_assert(
    offsetof(command::DrawIndexedIndirectParameters, indexCountPerInstance) ==
    offsetof(VkDrawIndexedIndirectCommand, indexCount)
);
static_assert(
    offsetof(command::DrawIndexedIndirectParameters, instanceCount) ==
    offsetof(VkDrawIndexedIndirectCommand, instanceCount)
);
static_assert(
    offsetof(command::DrawIndexedIndirectParameters, startIndexLocation) ==
    offsetof(VkDrawIndexedIndirectCommand, firstIndex)
);
static_assert(
    offsetof(command::DrawIndexedIndirectParameters, baseVertexLocation) ==
    offsetof(VkDrawIndexedIndirectCommand, vertexOffset)
);
static_assert(
    offsetof(command::DrawIndexedIndirectParameters, startInstanceLocation) ==
    offsetof(VkDrawIndexedIndirectCommand, firstInstance)
);

static_assert(sizeof(command::DispatchIndirectParameters) == sizeof(VkDispatchIndirectCommand));
static_assert(
    offsetof(command::DispatchIndirectParameters, threadGroupCountX) == offsetof(VkDispatchIndirectCommand, x)
);
static_assert(
    offsetof(command::DispatchIndirectParameters, threadGroupCountY) == offsetof(VkDispatchIndirectCommand, y)
);
static_assert(
    offsetof(command::DispatchIndirectParameters, threadGroupCountZ) == offsetof(VkDispatchIndirectCommand, z)
);

gp::UInt32
    getVulkanVertexCountForPrimitiveCount(const gp::UInt32 numPrimitives, const VkPrimitiveTopology topology) noexcept
{
    if (numPrimitives == 0u)
    {
        return 0u;
    }

    switch (topology)
    {
    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
        return numPrimitives * 3u;
    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
        return numPrimitives + 2u;
    case VK_PRIMITIVE_TOPOLOGY_LINE_LIST:
        return numPrimitives * 2u;
    case VK_PRIMITIVE_TOPOLOGY_POINT_LIST:
        return numPrimitives;
    default:
        GP_CHECK_SLOW(false);
        break;
    }
    return numPrimitives;
}

gp::UInt32 getVulkanIndirectCommandCapacity(
    const gp::UInt64 bufferSize, const gp::UInt64 offset, const gp::UInt32 stride
) noexcept
{
    if ((stride == 0u) || (offset >= bufferSize))
    {
        return 0u;
    }
    return static_cast<gp::UInt32>((bufferSize - offset) / static_cast<gp::UInt64>(stride));
}

}   // namespace gp::rhi::vulkan
