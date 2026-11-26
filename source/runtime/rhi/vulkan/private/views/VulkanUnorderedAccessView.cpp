// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "views/VulkanUnorderedAccessView.hpp"
#include "commands/CommandListBase.hpp"
#include "CoreMinimal.hpp"
#include "diagnostics/AssertionMacros.hpp"
#include "memory/pointers/RefCountedPtr.hpp"
#include "resources/Buffer.hpp"
#include "resources/Texture.hpp"
#include "resources/VulkanBuffer.hpp"
#include "resources/VulkanTexture.hpp"
#include "resources/VulkanViewableResource.hpp"
#include "views/ViewEnums.hpp"
#include "views/VulkanTextureViewDesc.hpp"
#include "views/VulkanViewTranslation.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <utility>

namespace gp::rhi::vulkan
{

VulkanUnorderedAccessView::VulkanUnorderedAccessView(
    const VulkanResourceServices& services,
    resource::ViewableResource* const targetResource,
    const view::ViewDesc& viewDesc
) noexcept
    : view::UnorderedAccessView(targetResource, viewDesc)
    , VulkanView(services, getVulkanUAVDescriptorType(viewDesc))
{}

VulkanUnorderedAccessView::~VulkanUnorderedAccessView() noexcept = default;

VulkanViewableResource* VulkanUnorderedAccessView::getBaseResource() const noexcept
{
    if (isBuffer())
    {
        return static_cast<VulkanBuffer*>(getBuffer());
    }

    return static_cast<VulkanTexture*>(getTexture());
}

void VulkanUnorderedAccessView::updateView(const allocator::ContextArray& contexts) noexcept
{
    invalidate();

    if (isBuffer())
    {
        VulkanBuffer* const buffer = static_cast<VulkanBuffer*>(getBuffer());
        const view::ViewDesc::BufferUAV::ViewInfo info = m_viewDesc.buffer.uav.getViewInfo(buffer);
        GP_CHECKF(!info.atomicCounter && !info.appendBuffer, "UAV counters are not supported by the Vulkan RHI.");
        if (info.isNullView)
        {
            return;
        }

        switch (info.bufferType)
        {
        case view::BufferViewType::Raw:
        case view::BufferViewType::Structured:
            initAsStructuredBufferView(contexts, *buffer, info.offsetInBytes, info.sizeInBytes);
            break;

        case view::BufferViewType::Typed:
            GP_CHECKF(
                (buffer->getBufferUsageFlags() & VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT) != 0u,
                "Typed buffer UAVs need a buffer created with UnorderedAccess usage."
            );
            initAsTypedBufferView(contexts, *buffer, info.format, info.offsetInBytes, info.sizeInBytes);
            break;

        case view::BufferViewType::AccelerationStructure:
            GP_CHECKF(false, "Acceleration structures cannot be bound as UAVs.");
            break;

        case view::BufferViewType::Unknown:
        default:
            GP_CHECKF(false, "Unhandled BufferViewType in VulkanUnorderedAccessView::updateView().");
            break;
        }
        return;
    }

    VulkanTexture* const texture = static_cast<VulkanTexture*>(getTexture());
    const view::ViewDesc::TextureUAV::ViewInfo info = m_viewDesc.texture.uav.getViewInfo(texture);
    const VulkanTextureViewDesc viewDesc = translateVulkanTextureViewDesc(*texture, info);
    initAsTextureView(contexts, texture->getImage(), viewDesc);
}

view::DescriptorHandle VulkanUnorderedAccessView::getBindlessHandle() const noexcept
{
    return VulkanView::getBindlessHandle();
}

void VulkanUnorderedAccessView::enqueueLinkAndUpdate(command::CommandListBase& cmdList) noexcept
{
    gp::RefCountedPtr<VulkanUnorderedAccessView> heldView(this);
    cmdList.enqueueLambda([heldView = std::move(heldView)](command::CommandListBase& executingCmdList) noexcept
    {
        static_cast<void>(executingCmdList);
        heldView->getBaseResource()->linkView(*heldView);
        heldView->updateView(allocator::ContextArray{});
    });
}

view::UnorderedAccessViewRef createVulkanUnorderedAccessView(
    command::CommandListBase& cmdList, resource::Buffer* const buffer, const view::ViewDesc& desc
) noexcept
{
    GP_CHECK(buffer != nullptr);
    GP_CHECKF(desc.isUAV() && desc.isBuffer(), "createVulkanUnorderedAccessView(buffer) needs a buffer UAV desc.");

    VulkanBuffer* const vulkanBuffer = static_cast<VulkanBuffer*>(buffer);
    VulkanUnorderedAccessView* const unorderedAccessView =
        new VulkanUnorderedAccessView(vulkanBuffer->getServices(), vulkanBuffer, desc);
    view::UnorderedAccessViewRef viewRef(unorderedAccessView);
    unorderedAccessView->enqueueLinkAndUpdate(cmdList);
    return viewRef;
}

view::UnorderedAccessViewRef createVulkanUnorderedAccessView(
    command::CommandListBase& cmdList, resource::Texture* const texture, const view::ViewDesc& desc
) noexcept
{
    GP_CHECK(texture != nullptr);
    GP_CHECKF(desc.isUAV() && desc.isTexture(), "createVulkanUnorderedAccessView(texture) needs a texture UAV desc.");

    VulkanTexture* const vulkanTexture = static_cast<VulkanTexture*>(texture);
    VulkanUnorderedAccessView* const unorderedAccessView =
        new VulkanUnorderedAccessView(vulkanTexture->getServices(), vulkanTexture, desc);
    view::UnorderedAccessViewRef viewRef(unorderedAccessView);
    unorderedAccessView->enqueueLinkAndUpdate(cmdList);
    return viewRef;
}

}   // namespace gp::rhi::vulkan
