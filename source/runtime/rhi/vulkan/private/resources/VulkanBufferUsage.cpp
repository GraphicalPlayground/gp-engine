// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "resources/VulkanBufferUsage.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "device/VulkanDevice.hpp"
#include "extensions/VulkanExtensionTypes.hpp"
#include "loader/VulkanCommandWrappers.hpp"
#include "resources/BufferEnums.hpp"
#include "VulkanCommon.hpp"

namespace gp::rhi::vulkan
{

VkBufferUsageFlags translateBufferUsageFlags(
    const OptionalVulkanDeviceExtensions& extensions, const resource::BufferUsageFlags usage, const bool isZeroSize
) noexcept
{
    using resource::BufferUsageFlags;

    // Always include TRANSFER_SRC: vendors confirmed it has no cost and debug features read buffers back.
    VkBufferUsageFlags outUsage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    const auto translateFlag = [&outUsage, usage](
                                   const BufferUsageFlags searchFlag,
                                   const VkBufferUsageFlags addedIfFound,
                                   const VkBufferUsageFlags addedIfNotFound = 0u
                               ) -> void
    {
        outUsage |= hasAnyBufferUsage(usage, searchFlag) ? addedIfFound : addedIfNotFound;
    };

    translateFlag(BufferUsageFlags::VertexBuffer, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    translateFlag(BufferUsageFlags::IndexBuffer, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    translateFlag(
        BufferUsageFlags::ByteAddressBuffer | BufferUsageFlags::StructuredBuffer, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
    );
    translateFlag(BufferUsageFlags::UniformBuffer, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    translateFlag(BufferUsageFlags::AccelerationStructure, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR);

    if (!isZeroSize)
    {
        translateFlag(BufferUsageFlags::UnorderedAccess, VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT);
        translateFlag(BufferUsageFlags::DrawIndirect, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
        translateFlag(
            BufferUsageFlags::KeepCPUAccessible, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
        );
        translateFlag(BufferUsageFlags::ShaderResource, VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT);

        // Volatile buffers are rewritten through their mapping and are never copy destinations.
        translateFlag(BufferUsageFlags::Volatile, 0u, VK_BUFFER_USAGE_TRANSFER_DST_BIT);

        if (extensions.hasRaytracingExtensions())
        {
            outUsage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

            translateFlag(
                BufferUsageFlags::AccelerationStructure,
                0u,
                VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
            );

            // Hit groups bind vertex and index buffers as storage buffers to fetch geometry attributes.
            if (hasAnyBufferUsage(usage, BufferUsageFlags::VertexBuffer | BufferUsageFlags::IndexBuffer) &&
                !hasAnyBufferUsage(usage, BufferUsageFlags::AccelerationStructure))
            {
                outUsage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
            }
        }

        // Descriptor buffers address their resources by GPU virtual address.
        if (extensions.hasBufferDeviceAddress != 0u)
        {
            outUsage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        }
    }

    return outUsage;
}

VkDeviceAddress getVulkanBufferDeviceAddress(const VulkanDevice& device, const VkBuffer buffer) noexcept
{
    if ((device.getOptionalExtensions().hasBufferDeviceAddress == 0u) || (buffer == VK_NULL_HANDLE) ||
        (dynamic_api::vkGetBufferDeviceAddressKHR == nullptr))
    {
        return 0u;
    }

    VkBufferDeviceAddressInfo addressInfo =
        zeroVulkanStruct<VkBufferDeviceAddressInfo>(VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO);
    addressInfo.buffer = buffer;
    return vkGetBufferDeviceAddressKHR(device.getHandle(), &addressInfo);
}

}   // namespace gp::rhi::vulkan

