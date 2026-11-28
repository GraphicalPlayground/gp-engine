// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "contexts/VulkanUploadContext.hpp"
#include "commands/VulkanCommandEnums.hpp"
#include "containers/arrays/Vector.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "diagnostics/AssertionMacros.hpp"
#include "memory/pointers/UniquePtr.hpp"
#include "resources/VulkanCommandBufferTransferRecorder.hpp"
#include "threading/sync/Mutex.hpp"
#include "threading/sync/UniqueLock.hpp"
#include <utility>   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

VulkanUploadContext::VulkanUploadContext(const VulkanContextServices& services) noexcept
    : VulkanContextCommon(services, VulkanCommandBufferType::Primary)
{}

VulkanUploadContext::~VulkanUploadContext() noexcept
{
    m_recorder.reset();
    m_recorderOwner = nullptr;
}

IVulkanTransferCommandRecorder* VulkanUploadContext::getTransferRecorder() noexcept
{
    VulkanCommandBuffer* const commandBuffer = getCommandBuffer();
    if (commandBuffer == nullptr)
    {
        return nullptr;
    }
    if (!m_recorder || (m_recorderOwner != commandBuffer))
    {
        m_recorder = gp::makeUnique<VulkanCommandBufferTransferRecorder>(*commandBuffer, getContextSyncPoint());
        m_recorderOwner = commandBuffer;
    }
    return m_recorder.get();
}

void VulkanUploadContext::onFinalize(VulkanPayload& signalPayload) noexcept
{
    static_cast<void>(signalPayload);
    m_recorder.reset();
    m_recorderOwner = nullptr;
}

VulkanUploadContextPool::VulkanUploadContextPool(const VulkanContextServices& services) noexcept
    : m_services(services)
{}

VulkanUploadContextPool::~VulkanUploadContextPool() noexcept
{
    m_freeContexts.clear();
    m_contexts.clear();
}

VulkanUploadContext& VulkanUploadContextPool::acquire() noexcept
{
    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
    if (!m_freeContexts.isEmpty())
    {
        VulkanUploadContext* const context = m_freeContexts.back();
        m_freeContexts.popBack();
        return *context;
    }
    m_contexts.pushBack(gp::makeUnique<VulkanUploadContext>(m_services));
    return *m_contexts.back();
}

void VulkanUploadContextPool::release(VulkanUploadContext& context) noexcept
{
    GP_CHECK_SLOW(context.getNumPayloads() == 0u);
    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
    GP_CHECK_SLOW(!m_freeContexts.contains(&context));
    m_freeContexts.pushBack(&context);
}

gp::USize VulkanUploadContextPool::getNumContexts() const noexcept
{
    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
    return m_contexts.size();
}

gp::USize VulkanUploadContextPool::getNumFreeContexts() const noexcept
{
    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
    return m_freeContexts.size();
}

}   // namespace gp::rhi::vulkan
