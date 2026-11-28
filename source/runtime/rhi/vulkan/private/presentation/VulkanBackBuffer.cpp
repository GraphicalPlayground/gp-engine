// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "presentation/VulkanBackBuffer.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "presentation/VulkanViewport.hpp"

namespace gp::rhi::vulkan
{

VulkanBackBuffer::VulkanBackBuffer(
    const VulkanResourceServices& services, VulkanViewport& viewport, const resource::TextureCreateDesc& createDesc
) noexcept
    : VulkanTexture(services, createDesc)
    , m_viewport(&viewport)
{}

VulkanBackBuffer::~VulkanBackBuffer() noexcept
{
    unbindBorrowedImage();
}

void VulkanBackBuffer::onGetBackBufferImage(command::CommandListBase& executingCmdList) noexcept
{
    if (hasAcquiredImage() || (m_viewport == nullptr))
    {
        return;
    }
    m_viewport->acquireBackBufferImage(executingCmdList, *this);
}

void VulkanBackBuffer::bindAcquiredImage(VulkanTexture& source) noexcept
{
    bindBorrowedImage(source);
}

void VulkanBackBuffer::releaseAcquiredImage() noexcept
{
    unbindBorrowedImage();
}

}   // namespace gp::rhi::vulkan
