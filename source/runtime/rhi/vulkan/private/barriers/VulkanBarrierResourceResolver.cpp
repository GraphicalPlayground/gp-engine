// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "barriers/VulkanBarrierResourceResolver.hpp"
#include "barriers/VulkanBarrierEnums.hpp"
#include "CoreMinimal.hpp"
#include "resources/TextureDesc.hpp"
#include "resources/TextureEnums.hpp"
#include "resources/VulkanBuffer.hpp"
#include "resources/VulkanTexture.hpp"
#include "resources/VulkanTextureTypes.hpp"
#include "sync/SyncEnums.hpp"
#include "sync/TransitionDesc.hpp"
#include "views/UnorderedAccessView.hpp"

namespace gp::rhi::vulkan
{

VulkanBarrierResource VulkanBarrierResourceResolver::resolve(const sync::TransitionInfo& info) const noexcept
{
    if (info.resource == nullptr)
    {
        return VulkanBarrierResource{};
    }

    switch (info.type)
    {
    case sync::TransitionResourceType::Texture:
        return resolveTexture(*static_cast<const VulkanTexture*>(info.texture));
    case sync::TransitionResourceType::Buffer:
        return resolveBuffer(*static_cast<const VulkanBuffer*>(info.buffer));
    case sync::TransitionResourceType::UAV:
    {
        const view::UnorderedAccessView* const uav = info.uav;
        if (uav->isTexture())
        {
            return resolveTexture(*static_cast<const VulkanTexture*>(uav->getTexture()));
        }
        if (uav->isBuffer())
        {
            return resolveBuffer(*static_cast<const VulkanBuffer*>(uav->getBuffer()));
        }
        return VulkanBarrierResource{};
    }
    case sync::TransitionResourceType::BVH:
        // Acceleration structures need no handle: the processor orders them with global memory barriers.
    case sync::TransitionResourceType::Unknown:
    default:
        break;
    }
    return VulkanBarrierResource{};
}

VulkanBarrierResource VulkanBarrierResourceResolver::resolveTexture(const VulkanTexture& texture) noexcept
{
    VulkanBarrierResource resource{};
    const VulkanCpuReadbackBuffer* const readback = texture.getCpuReadbackBuffer();
    if (readback != nullptr)
    {
        // Readback textures live in a host buffer; only memory dependencies apply to them.
        resource.type = VulkanBarrierResourceType::ReadbackTexture;
        resource.buffer = readback->buffer;
        return resource;
    }

    const resource::TextureDesc& desc = texture.getDesc();
    resource.type = VulkanBarrierResourceType::Texture;
    resource.image = texture.getImage();
    resource.fullAspectMask = texture.getFullAspectMask();
    resource.isDepthOrStencilAspect = texture.isDepthOrStencilAspect();
    resource.supportsSampling = texture.supportsSampling();
    resource.is3DTexture = (desc.dimension == resource::TextureDimension::Texture3D);
    resource.isReservedResource =
        (desc.flags & resource::TextureCreateFlags::ReservedResource) != resource::TextureCreateFlags::None;
    resource.numMips = static_cast<gp::UInt32>(desc.numMips);
    resource.numArrayLayers = texture.getNumberOfArrayLevels();
    return resource;
}

VulkanBarrierResource VulkanBarrierResourceResolver::resolveBuffer(const VulkanBuffer& buffer) noexcept
{
    VulkanBarrierResource resource{};
    resource.type = VulkanBarrierResourceType::Buffer;
    resource.buffer = buffer.getHandle();
    resource.bufferOffset = static_cast<VkDeviceSize>(buffer.getOffset());
    resource.bufferSize = static_cast<VkDeviceSize>(buffer.getCurrentSize());
    resource.bufferUsageFlags = buffer.getBufferUsageFlags();
    resource.isReservedResource = buffer.isReservedResource();
    return resource;
}

}   // namespace gp::rhi::vulkan
