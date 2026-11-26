// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "views/VulkanShaderResourceView.hpp"
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

VulkanShaderResourceView::VulkanShaderResourceView(
    const VulkanResourceServices& services,
    resource::ViewableResource* const targetResource,
    const view::ViewDesc& viewDesc
) noexcept
    : view::ShaderResourceView(targetResource, viewDesc)
    , VulkanView(services, getVulkanSRVDescriptorType(viewDesc))
{}

VulkanShaderResourceView::~VulkanShaderResourceView() noexcept = default;

VulkanViewableResource* VulkanShaderResourceView::getBaseResource() const noexcept
{
    if (isBuffer())
    {
        return static_cast<VulkanBuffer*>(getBuffer());
    }

    return static_cast<VulkanTexture*>(getTexture());
}

void VulkanShaderResourceView::updateView(const allocator::ContextArray& contexts) noexcept
{
    invalidate();

    if (isBuffer())
    {
        VulkanBuffer* const buffer = static_cast<VulkanBuffer*>(getBuffer());
        const view::ViewDesc::BufferSRV::ViewInfo info = m_viewDesc.buffer.srv.getViewInfo(buffer);
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
                (buffer->getBufferUsageFlags() & VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT) != 0u,
                "Typed buffer SRVs need a buffer created with ShaderResource usage."
            );
            initAsTypedBufferView(contexts, *buffer, info.format, info.offsetInBytes, info.sizeInBytes);
            break;

        case view::BufferViewType::AccelerationStructure:
            initAsAccelerationStructureView(contexts, *buffer, info.offsetInBytes, info.sizeInBytes);
            break;

        case view::BufferViewType::Unknown:
        default:
            GP_CHECKF(false, "Unhandled BufferViewType in VulkanShaderResourceView::updateView().");
            break;
        }
        return;
    }

    VulkanTexture* const texture = static_cast<VulkanTexture*>(getTexture());
    const view::ViewDesc::TextureSRV::ViewInfo info = m_viewDesc.texture.srv.getViewInfo(texture);
    const VulkanTextureViewDesc viewDesc = translateVulkanTextureViewDesc(*texture, info);
    initAsTextureView(contexts, texture->getImage(), viewDesc);
}

view::DescriptorHandle VulkanShaderResourceView::getBindlessHandle() const noexcept
{
    return VulkanView::getBindlessHandle();
}

void VulkanShaderResourceView::enqueueLinkAndUpdate(command::CommandListBase& cmdList) noexcept
{
    gp::RefCountedPtr<VulkanShaderResourceView> heldView(this);
    cmdList.enqueueLambda([heldView = std::move(heldView)](command::CommandListBase& executingCmdList) noexcept
    {
        static_cast<void>(executingCmdList);
        heldView->getBaseResource()->linkView(*heldView);
        heldView->updateView(allocator::ContextArray{});
    });
}

view::ShaderResourceViewRef createVulkanShaderResourceView(
    command::CommandListBase& cmdList, resource::Buffer* const buffer, const view::ViewDesc& desc
) noexcept
{
    GP_CHECK(buffer != nullptr);
    GP_CHECKF(desc.isSRV() && desc.isBuffer(), "createVulkanShaderResourceView(buffer) needs a buffer SRV desc.");

    VulkanBuffer* const vulkanBuffer = static_cast<VulkanBuffer*>(buffer);
    VulkanShaderResourceView* const shaderResourceView =
        new VulkanShaderResourceView(vulkanBuffer->getServices(), vulkanBuffer, desc);
    view::ShaderResourceViewRef viewRef(shaderResourceView);
    shaderResourceView->enqueueLinkAndUpdate(cmdList);
    return viewRef;
}

view::ShaderResourceViewRef createVulkanShaderResourceView(
    command::CommandListBase& cmdList, resource::Texture* const texture, const view::ViewDesc& desc
) noexcept
{
    GP_CHECK(texture != nullptr);
    GP_CHECKF(desc.isSRV() && desc.isTexture(), "createVulkanShaderResourceView(texture) needs a texture SRV desc.");

    VulkanTexture* const vulkanTexture = static_cast<VulkanTexture*>(texture);
    VulkanShaderResourceView* const shaderResourceView =
        new VulkanShaderResourceView(vulkanTexture->getServices(), vulkanTexture, desc);
    view::ShaderResourceViewRef viewRef(shaderResourceView);
    shaderResourceView->enqueueLinkAndUpdate(cmdList);
    return viewRef;
}

}   // namespace gp::rhi::vulkan
