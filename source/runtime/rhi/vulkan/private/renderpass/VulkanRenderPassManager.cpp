// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "renderpass/VulkanRenderPassManager.hpp"
#include "commands/RenderPassInfo.hpp"
#include "commands/VulkanCommandBuffer.hpp"
#include "containers/arrays/Array.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "hashing/Crc32.hpp"
#include "renderpass/VulkanBeginRenderPassInfo.hpp"
#include "renderpass/VulkanFramebuffer.hpp"
#include "renderpass/VulkanRenderPass.hpp"
#include "renderpass/VulkanRenderTargetLayout.hpp"
#include "resources/VulkanTextureUsage.hpp"
#include "RHIDefinitions.hpp"
#include "threading/sync/UniqueLock.hpp"

namespace gp::rhi::vulkan
{

VulkanRenderPassManager::VulkanRenderPassManager(
    VulkanDevice& device,
    VulkanDeferredDeletionQueue* const deferredDeletionQueue,
    const VulkanRenderTargetCaps& caps,
    const bool useDynamicRendering
) noexcept
    : m_device(device)
    , m_deferredDeletionQueue(deferredDeletionQueue)
    , m_caps(caps)
    , m_useDynamicRendering(useDynamicRendering && caps.hasKHRDynamicRendering)
{}

VulkanRenderPassManager::~VulkanRenderPassManager() noexcept
{
    // Framebuffers first: they were created against the render passes.
    {
        gp::thread::UniqueLock<gp::thread::Mutex> lock(m_framebufferMutex);
        m_framebuffers.clear();
    }
    {
        gp::thread::UniqueLock<gp::thread::Mutex> lock(m_renderPassMutex);
        m_renderPasses.clear();
    }
}

VulkanRenderPass* VulkanRenderPassManager::getOrCreateRenderPass(const VulkanRenderTargetLayout& layout) noexcept
{
    const gp::UInt32 hash = layout.getRenderPassFullHash();
    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_renderPassMutex);
    for (const RenderPassEntry& entry: m_renderPasses)
    {
        if (entry.hash == hash)
        {
            return entry.renderPass.get();
        }
    }

    RenderPassEntry& entry = m_renderPasses.emplaceBack(
        RenderPassEntry{ hash, gp::makeUnique<VulkanRenderPass>(m_device, m_deferredDeletionQueue, m_caps, layout) }
    );
    return entry.renderPass.get();
}

VulkanFramebuffer* VulkanRenderPassManager::getOrCreateFramebuffer(
    const command::SetRenderTargetsInfo& rtInfo,
    const VulkanRenderTargetLayout& layout,
    const VulkanRenderPass& renderPass
) noexcept
{
    const gp::UInt32 hash = computeFramebufferHash(rtInfo, layout);
    const VkOffset2D offset = layout.getOffset2D();
    const VkExtent2D extent = layout.getExtent2D();

    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_framebufferMutex);
    for (const FramebufferEntry& entry: m_framebuffers)
    {
        if (entry.hash != hash)
        {
            continue;
        }
        const VkRect2D& area = entry.framebuffer->getRenderArea();
        if (entry.framebuffer->matches(rtInfo) && (area.extent.width == extent.width) &&
            (area.extent.height == extent.height) && (area.offset.x == offset.x) && (area.offset.y == offset.y))
        {
            return entry.framebuffer.get();
        }
    }

    FramebufferEntry& entry = m_framebuffers.emplaceBack(
        FramebufferEntry{
          hash, gp::makeUnique<VulkanFramebuffer>(m_device, m_deferredDeletionQueue, rtInfo, layout, renderPass) }
    );
    return entry.framebuffer.get();
}

void VulkanRenderPassManager::notifyDeletedImage(const VkImage image, const resource::TextureCreateFlags flags) noexcept
{
    constexpr resource::TextureCreateFlags kAttachmentFlags =
        resource::TextureCreateFlags::RenderTargetable | resource::TextureCreateFlags::ResolveTargetable |
        resource::TextureCreateFlags::DepthStencilTargetable | resource::TextureCreateFlags::DepthStencilResolveTarget |
        resource::TextureCreateFlags::Foveation;
    if (m_useDynamicRendering || (image == VK_NULL_HANDLE) || !hasAnyTextureFlags(flags, kAttachmentFlags))
    {
        return;
    }

    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_framebufferMutex);
    for (gp::USize index = m_framebuffers.size(); index > 0u; --index)
    {
        FramebufferEntry& entry = m_framebuffers[index - 1u];
        if (entry.framebuffer->containsImage(image))
        {
            entry.framebuffer->destroy(false);
            m_framebuffers.removeAtSwap(index - 1u);
        }
    }
}

bool VulkanRenderPassManager::beginRenderPass(
    VulkanCommandBuffer& commandBuffer,
    const command::RenderPassInfo& rpInfo,
    const VulkanRenderPass& renderPass,
    const VulkanFramebuffer& framebuffer,
    const bool isParallelRenderPass
) noexcept
{
    if ((renderPass.getHandle() == VK_NULL_HANDLE) || (framebuffer.getHandle() == VK_NULL_HANDLE))
    {
        return false;
    }

    VulkanBeginRenderPassInfo beginInfo;
    beginInfo.isParallelRenderPass = isParallelRenderPass;
    beginInfo.build(rpInfo, renderPass, framebuffer);
    return commandBuffer.beginRenderPass(beginInfo.beginInfo, beginInfo.getSubpassContents());
}

bool VulkanRenderPassManager::endRenderPass(VulkanCommandBuffer& commandBuffer) noexcept
{
    return commandBuffer.endRenderPass();
}

gp::USize VulkanRenderPassManager::getNumRenderPasses() const noexcept
{
    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_renderPassMutex);
    return m_renderPasses.size();
}

gp::USize VulkanRenderPassManager::getNumFramebuffers() const noexcept
{
    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_framebufferMutex);
    return m_framebuffers.size();
}

gp::UInt32 VulkanRenderPassManager::computeFramebufferHash(
    const command::SetRenderTargetsInfo& rtInfo, const VulkanRenderTargetLayout& layout
) noexcept
{
    // Two passes over the same textures but different mips or slices need different views, hence framebuffers.
    gp::Array<gp::UInt64, kMaxSimultaneousRenderTargets> mipsAndSlices{};
    for (gp::UInt32 index = 0u; index < kMaxSimultaneousRenderTargets; ++index)
    {
        const command::RenderTargetView& view = rtInfo.colorRenderTarget[index];
        mipsAndSlices[index] =
            (static_cast<gp::UInt64>(view.arraySliceIndex) << 32u) | static_cast<gp::UInt64>(view.mipIndex);
    }

    gp::hashing::Crc32 hasher(layout.getRenderPassCompatibleHash());
    hasher.update(static_cast<const void*>(mipsAndSlices.data()), sizeof(gp::UInt64) * mipsAndSlices.size());
    return hasher.finalize();
}

}   // namespace gp::rhi::vulkan
