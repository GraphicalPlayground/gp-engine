// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "device/VulkanDynamicRHI.hpp"
#include "commands/CommandListExecutor.hpp"
#include "commands/ICommandContext.hpp"
#include "commands/IComputeContext.hpp"
#include "commands/VulkanQueue.hpp"
#include "commands/VulkanSubmission.hpp"
#include "contexts/VulkanCommandContextProvider.hpp"
#include "contexts/VulkanCommandListContext.hpp"
#include "contexts/VulkanImmediatePayloadSubmitter.hpp"
#include "contexts/VulkanUploadContext.hpp"
#include "CoreMinimal.hpp"
#include "descriptors/VulkanDescriptorPoolsManager.hpp"
#include "descriptors/VulkanDescriptorSetCache.hpp"
#include "device/VulkanDevice.hpp"
#include "device/VulkanQueueFamilySelection.hpp"
#include "extensions/VulkanExtensionTypes.hpp"
#include "instance/VulkanInstanceDesc.hpp"
#include "instance/VulkanPhysicalDevice.hpp"
#include "loader/VulkanLoader.hpp"
#include "logging/LogMacros.hpp"
#include "memory/VulkanDeferredDeletionQueue.hpp"
#include "memory/VulkanDeviceMemory.hpp"
#include "memory/VulkanMemoryManager.hpp"
#include "memory/VulkanMemoryStatsTranslation.hpp"
#include "memory/VulkanStagingManager.hpp"
#include "memory/VulkanTempBlockAllocator.hpp"
#include "pendingstate/VulkanPendingComputeState.hpp"
#include "pendingstate/VulkanPendingGfxState.hpp"
#include "pipeline/PipelineEnums.hpp"
#include "pipeline/SamplerStateDesc.hpp"
#include "pipeline/VulkanPipelineDeletionNotifier.hpp"
#include "pipeline/VulkanPipelineSettings.hpp"
#include "pipeline/VulkanPipelineStateCacheManager.hpp"
#include "presentation/VulkanViewportRegistry.hpp"
#include "profiling/RHIStats.hpp"
#include "queries/VulkanQueryPoolManager.hpp"
#include "renderpass/VulkanRenderPassManager.hpp"
#include "renderpass/VulkanRenderPassSettings.hpp"
#include "renderpass/VulkanRenderTargetCaps.hpp"
#include "resources/TextureEnums.hpp"
#include "resources/VulkanBuffer.hpp"
#include "resources/VulkanTexture.hpp"
#include "shaders/VulkanShaderFactory.hpp"
#include "shaders/VulkanShaderHeaderEnums.hpp"
#include "states/VulkanSamplerState.hpp"
#include "states/VulkanSamplerStateCache.hpp"
#include "states/VulkanVertexDeclaration.hpp"
#include "sync/VulkanFence.hpp"
#include "threading/sync/UniqueLock.hpp"
#include "views/VulkanTextureViewDesc.hpp"
#include "views/VulkanView.hpp"
#include "views/VulkanViewEnums.hpp"

namespace gp::rhi::vulkan
{

namespace
{

/// @brief Debug name of the texture bound where a shader reads an image nobody set.
constexpr const char* kDefaultTextureName = "VulkanDefaultTexture";

/// @brief Builds the description of one global sampler.
/// @details Global samplers come in (clamped, wrapped) pairs per filter, in `VulkanGlobalSamplerType` order: point,
///          bilinear, trilinear.
/// @param[in] index Global sampler index, below `kVulkanNumGlobalSamplers`.
/// @return Sampler description.
[[nodiscard]] pipeline::SamplerStateDesc makeGlobalSamplerDesc(const gp::USize index) noexcept
{
    static constexpr pipeline::SamplerFilter kFilters[] = {
        pipeline::SamplerFilter::Point,
        pipeline::SamplerFilter::Bilinear,
        pipeline::SamplerFilter::Trilinear,
    };

    const pipeline::SamplerAddressMode addressMode =
        ((index % 2u) == 0u) ? pipeline::SamplerAddressMode::Clamp : pipeline::SamplerAddressMode::Wrap;

    pipeline::SamplerStateDesc desc{};
    desc.filter = kFilters[(index / 2u) % 3u];
    desc.addressU = addressMode;
    desc.addressV = addressMode;
    desc.addressW = addressMode;
    return desc;
}

}   // namespace

static_assert(
    static_cast<gp::USize>(VulkanGlobalSamplerType::PointClampedSampler) == 0u &&
        static_cast<gp::USize>(VulkanGlobalSamplerType::PointWrappedSampler) == 1u &&
        static_cast<gp::USize>(VulkanGlobalSamplerType::BilinearClampedSampler) == 2u &&
        static_cast<gp::USize>(VulkanGlobalSamplerType::BilinearWrappedSampler) == 3u &&
        static_cast<gp::USize>(VulkanGlobalSamplerType::TrilinearClampedSampler) == 4u &&
        static_cast<gp::USize>(VulkanGlobalSamplerType::TrilinearWrappedSampler) == 5u,
    "makeGlobalSamplerDesc() assumes (clamped, wrapped) pairs ordered point, bilinear, trilinear."
);

VulkanDynamicRHI::VulkanDynamicRHI() noexcept = default;

VulkanDynamicRHI::~VulkanDynamicRHI() noexcept
{
    shutdown();
}

void VulkanDynamicRHI::init() noexcept
{
    if (m_isInitialized)
    {
        return;
    }

    if (!initInstance() || !initDevice() || !initServices())
    {
        GP_LOG_ERROR(LogVulkanRHI, "Vulkan RHI initialization failed; releasing the partially created device.");
        releaseAll();
        return;
    }

    initDescriptorDefaults();
    initContexts();

    static_cast<void>(setVulkanPipelineDeletionListener(this));
    m_isPipelineListener = true;
    m_isInitialized = true;
    GP_LOG_INFO(LogVulkanRHI, "Vulkan RHI initialized.");
}

void VulkanDynamicRHI::postInit() noexcept
{
    if (m_isInitialized)
    {
        flushPendingUploads(true);
    }
}

void VulkanDynamicRHI::shutdown() noexcept
{
    m_isInitialized = false;
    releaseAll();
}

void* VulkanDynamicRHI::getNativeDevice() noexcept
{
    return getVulkanDeviceHandle();
}

void VulkanDynamicRHI::tick(const gp::Float32 deltaTime) noexcept
{
    static_cast<void>(deltaTime);
    if (!m_isInitialized)
    {
        return;
    }

    pollQueues();
    static_cast<void>(m_deferredDeletionQueue->processCompletedBatches(false));
}

void VulkanDynamicRHI::processDeleteQueue() noexcept
{
    if (!m_isInitialized)
    {
        return;
    }

    // New handles are harvested at frame end, gated on the frame's last sync point; here only batches the GPU already
    // finished are released, so a mid-frame call never stalls.
    pollQueues();
    static_cast<void>(m_deferredDeletionQueue->processCompletedBatches(false));
}

gp::UInt64 VulkanDynamicRHI::getMinimumAlignmentForBufferBackedSRV(const PixelFormat format) noexcept
{
    if (m_device == nullptr)
    {
        return DynamicRHI::getMinimumAlignmentForBufferBackedSRV(format);
    }
    return static_cast<gp::UInt64>(m_device->getLimits().minTexelBufferOffsetAlignment);
}

gp::UInt32 VulkanDynamicRHI::getPlatformTextureMaxSampleCount() const noexcept
{
    if (m_device == nullptr)
    {
        return 1u;
    }

    const VkSampleCountFlags counts = m_device->getLimits().framebufferColorSampleCounts;
    for (gp::UInt32 sampleCount = static_cast<gp::UInt32>(VK_SAMPLE_COUNT_64_BIT); sampleCount > 1u; sampleCount >>= 1u)
    {
        if ((counts & sampleCount) != 0u)
        {
            return sampleCount;
        }
    }
    return 1u;
}

void VulkanDynamicRHI::beginFrame() noexcept
{
    if (m_isInitialized)
    {
        pollQueues();
    }
}

void VulkanDynamicRHI::endFrame(const EndFrameArgs& args) noexcept
{
    if (!m_isInitialized)
    {
        return;
    }

    m_currentFrame = args.frameNumber;
    const gp::UInt32 frame32 = static_cast<gp::UInt32>(args.frameNumber);

    // Uploads recorded outside any command list (resource creation from the render thread) must reach the GPU before
    // the frame's deletions are gated on its last sync point.
    flushPendingUploads(false);
    pollQueues();

    m_stagingManager->processPendingFree(false, true, frame32);
    m_memoryManager->releaseFreedPages(frame32);
    m_deferredDeletionQueue->releaseResources(false, m_lastSubmittedSyncPoint, frame32);
    m_descriptorSetCache->gc(args.frameNumber);
    static_cast<void>(m_descriptorPoolsManager->gc(args.frameNumber));
    m_queryPoolManager->removeStaleQueryPools();
    m_pipelineCache->setFrameNumber(args.frameNumber);
    m_pipelineCache->tickLRU(args.frameNumber);
    m_tempBlockAllocator->updateBlocks();
    m_deviceMemoryManager->updateMemoryProperties();

    m_defaultContext->setCurrentFrame(args.frameNumber);
    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_contextMutex);
    for (gp::UniquePtr<VulkanCommandListContext>& context: m_parallelContexts)
    {
        context->setCurrentFrame(args.frameNumber);
    }
}

void VulkanDynamicRHI::blockUntilGPUIdle() noexcept
{
    if (!m_isInitialized)
    {
        return;
    }

    flushPendingUploads(false);
    m_device->waitUntilIdle();
    pollQueues();
    static_cast<void>(m_deferredDeletionQueue->processCompletedBatches(false));
}

void VulkanDynamicRHI::bindDebugLabelName(
    command::CommandListBase& cmdList, resource::Texture* const texture, const char* const name
) noexcept
{
    DynamicRHI::bindDebugLabelName(cmdList, texture, name);
    if ((m_device == nullptr) || (texture == nullptr) || (name == nullptr))
    {
        return;
    }

    const VkImage image = static_cast<VulkanTexture*>(texture)->getImage();
    if (image != VK_NULL_HANDLE)
    {
        m_device->setObjectName(VK_OBJECT_TYPE_IMAGE, reinterpret_cast<gp::UInt64>(image), name);
    }
}

void VulkanDynamicRHI::bindDebugLabelName(
    command::CommandListBase& cmdList, resource::Buffer* const buffer, const char* const name
) noexcept
{
    DynamicRHI::bindDebugLabelName(cmdList, buffer, name);
    if ((m_device == nullptr) || (buffer == nullptr) || (name == nullptr))
    {
        return;
    }

    // Pooled buffers share one `VkBuffer`; the name then labels the whole pool, which is still useful in captures.
    const VkBuffer handle = static_cast<VulkanBuffer*>(buffer)->getHandle();
    if (handle != VK_NULL_HANDLE)
    {
        m_device->setObjectName(VK_OBJECT_TYPE_BUFFER, reinterpret_cast<gp::UInt64>(handle), name);
    }
}

command::ICommandContext* VulkanDynamicRHI::getDefaultContext() noexcept
{
    return m_defaultContext.get();
}

command::IComputeContext* VulkanDynamicRHI::getDefaultAsyncComputeContext() noexcept
{
    return m_defaultContext.get();
}

command::IComputeContext* VulkanDynamicRHI::getCommandContext(const sync::PipelineStage pipeline) noexcept
{
    if (!m_isInitialized || (pipeline != sync::PipelineStage::Graphics))
    {
        return nullptr;
    }

    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_contextMutex);
    if (!m_freeParallelContexts.isEmpty())
    {
        VulkanCommandListContext* const context = m_freeParallelContexts[m_freeParallelContexts.size() - 1u];
        m_freeParallelContexts.popBack();
        return context;
    }

    m_parallelContexts.pushBack(gp::makeUnique<VulkanCommandListContext>(m_contextServices));
    VulkanCommandListContext* const context = m_parallelContexts[m_parallelContexts.size() - 1u].get();
    context->setCurrentFrame(m_currentFrame);
    return context;
}

void VulkanDynamicRHI::finalizeContexts(FinalizeContextsArgs&& args, FinalizeContextsResult& outResult) noexcept
{
    outResult.commandLists.clear();
    outResult.uploadCommandList = nullptr;
    if (!m_isInitialized)
    {
        // The executor flushes once more after `shutdown()`; the contexts are gone and there is nothing to submit.
        return;
    }

    // An upload context handed out by `acquireUploadContext()`; the RHI owns it, the caller only recorded into it.
    VulkanUploadContext* const callerUploadContext = static_cast<VulkanUploadContext*>(args.uploadContext);
    args.uploadContext = nullptr;

    // Uploads were recorded before any of these contexts' commands were translated, so they run first. Keeping them
    // in the first list (rather than a separate upload list, which the executor does not submit) preserves the index
    // mapping wrappers rely on.
    const auto finalizeUploadsInto = [this, callerUploadContext](VulkanPlatformCommandList& commandList) -> void
    {
        static_cast<void>(m_contextProvider->finalizeUploads(commandList));
        if (callerUploadContext != nullptr)
        {
            callerUploadContext->finalize(commandList);
            m_uploadPool->release(*callerUploadContext);
        }
    };

    outResult.commandLists.reserve(args.contexts.size() + 1u);
    for (gp::USize index = 0u; index < args.contexts.size(); ++index)
    {
        command::IComputeContext* const context = args.contexts[index];
        if (context == nullptr)
        {
            continue;
        }

        VulkanCommandListContext& vulkanContext = toVulkanContext(*context);
        VulkanPlatformCommandList* const commandList = new VulkanPlatformCommandList();
        if (outResult.commandLists.isEmpty())
        {
            finalizeUploadsInto(*commandList);
        }

        m_lastSubmittedSyncPoint = vulkanContext.getContextSyncPoint();
        vulkanContext.finalize(*commandList);
        outResult.commandLists.pushBack(commandList);
        releaseParallelContext(vulkanContext);
    }

    // No context recorded anything, but the caller's upload context must still be submitted and returned to the pool.
    if (outResult.commandLists.isEmpty() && (callerUploadContext != nullptr))
    {
        VulkanPlatformCommandList* const commandList = new VulkanPlatformCommandList();
        finalizeUploadsInto(*commandList);
        outResult.commandLists.pushBack(commandList);
    }
}

command::IUploadContext* VulkanDynamicRHI::acquireUploadContext() noexcept
{
    if (!m_isInitialized || (m_uploadPool == nullptr))
    {
        return nullptr;
    }
    return &m_uploadPool->acquire();
}

void VulkanDynamicRHI::getMemoryStats(profiling::RHIMemoryStats& outStats) noexcept
{
    if (!m_isInitialized || (m_deviceMemoryManager == nullptr))
    {
        outStats = profiling::RHIMemoryStats{};
        return;
    }
    outStats = makeVulkanRHIMemoryStats(
        m_deviceMemoryManager->getMemoryStats(),
        m_deviceMemoryManager->getTotalMemory(true),
        m_deviceMemoryManager->getTotalMemory(false)
    );
}

void VulkanDynamicRHI::submitCommandLists(SubmitCommandListsArgs&& args) noexcept
{
    for (command::IPlatformCommandList* const platformCommandList: args.commandLists)
    {
        if (platformCommandList == nullptr)
        {
            continue;
        }

        VulkanPlatformCommandList* const commandList = static_cast<VulkanPlatformCommandList*>(platformCommandList);
        if (m_isInitialized)
        {
            // The submitter takes the payloads; the list destructor would otherwise delete unsubmitted ones.
            m_submitter->submitPayloads(commandList->payloads);
            commandList->payloads.clear();
        }
        delete commandList;
    }
    args.commandLists.clear();
}

gp::UInt32 VulkanDynamicRHI::getVulkanApiVersion() const noexcept
{
    return (m_device != nullptr) ? m_device->getApiVersion() : 0u;
}

void* VulkanDynamicRHI::getVulkanInstanceHandle() const noexcept
{
    return m_hasInstance ? static_cast<void*>(m_instance.getHandle()) : nullptr;
}

void* VulkanDynamicRHI::getVulkanPhysicalDeviceHandle() const noexcept
{
    return (m_device != nullptr) ? static_cast<void*>(m_device->getPhysicalHandle()) : nullptr;
}

void* VulkanDynamicRHI::getVulkanDeviceHandle() const noexcept
{
    return (m_device != nullptr) ? static_cast<void*>(m_device->getHandle()) : nullptr;
}

void* VulkanDynamicRHI::getVulkanGraphicsQueueHandle() const noexcept
{
    return (m_graphicsQueue != nullptr) ? static_cast<void*>(m_graphicsQueue->getHandle()) : nullptr;
}

gp::UInt32 VulkanDynamicRHI::getVulkanGraphicsQueueIndex() const noexcept
{
    return (m_graphicsQueue != nullptr) ? m_graphicsQueue->getQueueIndex() : 0u;
}

gp::UInt32 VulkanDynamicRHI::getVulkanGraphicsQueueFamilyIndex() const noexcept
{
    return (m_graphicsQueue != nullptr) ? m_graphicsQueue->getFamilyIndex() : ~0u;
}

const gp::UInt8* VulkanDynamicRHI::getVulkanDeviceUUID() const noexcept
{
    return m_deviceUuid.data();
}

void* VulkanDynamicRHI::getVulkanInstanceProcAddress(const char* const functionName) const noexcept
{
    if (!m_hasInstance || (functionName == nullptr))
    {
        return nullptr;
    }
    return reinterpret_cast<void*>(VulkanLoader::getInstanceProcAddr(m_instance.getHandle(), functionName));
}

void* VulkanDynamicRHI::getVulkanDeviceProcAddress(const char* const functionName) const noexcept
{
    if ((m_device == nullptr) || (functionName == nullptr))
    {
        return nullptr;
    }
    return reinterpret_cast<void*>(VulkanLoader::getDeviceProcAddr(m_device->getHandle(), functionName));
}

void VulkanDynamicRHI::onGraphicsPipelineDeleted(const VulkanGraphicsPipelineState& pipeline) noexcept
{
    if (m_defaultContext != nullptr)
    {
        m_defaultContext->getPendingGfxState().notifyDeletedPipeline(pipeline);
    }

    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_contextMutex);
    for (gp::UniquePtr<VulkanCommandListContext>& context: m_parallelContexts)
    {
        context->getPendingGfxState().notifyDeletedPipeline(pipeline);
    }
}

void VulkanDynamicRHI::onComputePipelineDeleted(const VulkanComputePipelineState& pipeline) noexcept
{
    if (m_defaultContext != nullptr)
    {
        m_defaultContext->getPendingComputeState().notifyDeletedPipeline(pipeline);
    }

    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_contextMutex);
    for (gp::UniquePtr<VulkanCommandListContext>& context: m_parallelContexts)
    {
        context->getPendingComputeState().notifyDeletedPipeline(pipeline);
    }
}

gp::USize VulkanDynamicRHI::getNumParallelContexts() const noexcept
{
    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_contextMutex);
    return m_parallelContexts.size();
}

gp::USize VulkanDynamicRHI::getNumFreeParallelContexts() const noexcept
{
    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_contextMutex);
    return m_freeParallelContexts.size();
}

bool VulkanDynamicRHI::initInstance() noexcept
{
    m_ownsLoaderLibrary = !VulkanLoader::isLibraryLoaded();
    if (!VulkanLoader::loadLibrary())
    {
        GP_LOG_ERROR(LogVulkanRHI, "Failed to load the Vulkan loader library.");
        m_ownsLoaderLibrary = false;
        return false;
    }

    VulkanInstanceCreateDesc desc{};
    desc.allowRetryWithoutLayers = true;
    desc.allowMissingSurfaceFunctions = true;
    if (!m_instance.initialize(desc))
    {
        GP_LOG_ERROR(LogVulkanRHI, "Failed to create the Vulkan instance.");
        return false;
    }
    m_hasInstance = true;
    return true;
}

bool VulkanDynamicRHI::initDevice() noexcept
{
    VulkanPhysicalDeviceInfo physicalDeviceInfo{};
    const VkPhysicalDevice physicalDevice =
        selectPhysicalDevice(m_instance.getHandle(), m_instance.getApiVersion(), physicalDeviceInfo);
    if (physicalDevice == VK_NULL_HANDLE)
    {
        GP_LOG_ERROR(LogVulkanRHI, "No Vulkan physical device satisfies the selection policy.");
        return false;
    }

    applySelectedPhysicalDeviceToRHIGlobals(physicalDeviceInfo);
    m_deviceUuid = physicalDeviceInfo.deviceUuid;

    const bool supportsDebugUtils =
        (m_instance.getActiveDebugLayerExtension() == ActiveDebugLayerExtension::DebugUtilsExtension);
    m_device = gp::makeUnique<VulkanDevice>(
        m_instance.getHandle(), physicalDevice, m_instance.getApiVersion(), supportsDebugUtils
    );
    if (!m_device->initialize(true))
    {
        GP_LOG_ERROR(LogVulkanRHI, "Failed to create the Vulkan logical device.");
        m_device.reset();
        return false;
    }
    return true;
}

bool VulkanDynamicRHI::initServices() noexcept
{
    const VulkanQueueFamilyIndices& families = m_device->getQueueFamilyIndices();
    if (families.graphicsFamily < 0)
    {
        GP_LOG_ERROR(LogVulkanRHI, "The Vulkan device exposes no graphics queue family.");
        return false;
    }

    m_deviceMemoryManager = gp::makeUnique<VulkanDeviceMemoryManager>();
    m_deviceMemoryManager->init(m_device.get());

    m_memoryManager = gp::makeUnique<VulkanMemoryManager>(*m_device, *m_deviceMemoryManager);
    m_memoryManager->init();

    m_deferredDeletionQueue =
        gp::makeUnique<VulkanDeferredDeletionQueue>(*m_device, *m_deviceMemoryManager, *m_memoryManager);
    m_memoryManager->setDeferredDeletionQueue(m_deferredDeletionQueue.get());

    m_stagingManager = gp::makeUnique<VulkanStagingManager>();
    m_stagingManager->init(m_device.get(), m_memoryManager.get());

    m_fenceManager = gp::makeUnique<VulkanFenceManager>(*m_device);
    m_graphicsQueue = gp::makeUnique<VulkanQueue>(
        *m_device, *m_fenceManager, static_cast<gp::UInt32>(families.graphicsFamily), VulkanQueueType::Graphics, 0u
    );
    if (m_device->hasAsyncComputeQueue() && (families.asyncComputeFamily >= 0))
    {
        m_computeQueue = gp::makeUnique<VulkanQueue>(
            *m_device,
            *m_fenceManager,
            static_cast<gp::UInt32>(families.asyncComputeFamily),
            VulkanQueueType::AsyncCompute,
            0u
        );
    }
    m_submitter = gp::makeUnique<VulkanImmediatePayloadSubmitter>();

    m_queryPoolManager = gp::makeUnique<VulkanQueryPoolManager>(*m_device);
    m_descriptorPoolsManager = gp::makeUnique<VulkanDescriptorPoolsManager>();
    m_descriptorPoolsManager->init(*m_device);
    m_descriptorSetCache = gp::makeUnique<VulkanDescriptorSetCache>(*m_device);

    const bool useDynamicRendering =
        getVulkanAllowDynamicRendering() && (m_device->getOptionalExtensions().hasKHRDynamicRendering != 0u);
    m_renderPassManager = gp::makeUnique<VulkanRenderPassManager>(
        *m_device, m_deferredDeletionQueue.get(), makeVulkanRenderTargetCaps(*m_device), useDynamicRendering
    );
    m_samplerCache = gp::makeUnique<VulkanSamplerStateCache>(*m_device, m_deferredDeletionQueue.get());
    m_shaderFactory = gp::makeUnique<VulkanShaderFactory>();
    m_pipelineCache = gp::makeUnique<VulkanPipelineStateCacheManager>(
        *m_device, m_deferredDeletionQueue.get(), m_renderPassManager.get(), getVulkanPipelineLRUSettings()
    );
    m_tempBlockAllocator = gp::makeUnique<VulkanTempBlockAllocator>(
        *m_memoryManager,
        VulkanTempBlockAllocator::getConfiguredBlockSize(),
        VulkanTempBlockAllocator::computeDefaultBlockAlignment(*m_device),
        VulkanTempBlockAllocator::computeDefaultBufferUsage(*m_device)
    );

    m_contextServices.device = m_device.get();
    m_contextServices.queue = m_graphicsQueue.get();
    m_contextServices.submitter = m_submitter.get();
    m_contextServices.queryPoolManager = m_queryPoolManager.get();
    m_contextServices.renderPassManager = m_renderPassManager.get();
    m_contextServices.descriptorPoolsManager = m_descriptorPoolsManager.get();
    m_contextServices.descriptorDefaults = &m_descriptorDefaults;
    m_contextServices.descriptorSetCache = m_descriptorSetCache.get();
    m_contextServices.tempBlockAllocator = m_tempBlockAllocator.get();

    m_uploadPool = gp::makeUnique<VulkanUploadContextPool>(m_contextServices);
    m_contextProvider = gp::makeUnique<VulkanCommandContextProvider>(m_uploadPool.get());

    m_resourceServices.device = m_device.get();
    m_resourceServices.memoryManager = m_memoryManager.get();
    m_resourceServices.stagingManager = m_stagingManager.get();
    m_resourceServices.deferredDeletionQueue = m_deferredDeletionQueue.get();
    m_resourceServices.contextProvider = m_contextProvider.get();
    m_resourceServices.tempBlockAllocator = m_tempBlockAllocator.get();
    m_resourceServices.renderPassManager = m_renderPassManager.get();

    m_viewportServices.resourceServices = m_resourceServices;
    m_viewportServices.fenceManager = m_fenceManager.get();
    m_viewportServices.graphicsQueue = m_graphicsQueue.get();
    m_viewportServices.computeQueue = m_computeQueue.get();
    m_viewportServices.submitter = m_submitter.get();
    m_viewportServices.renderPassManager = m_renderPassManager.get();
    m_viewportRegistry = gp::makeUnique<VulkanViewportRegistry>();
    m_viewportServices.viewportRegistry = m_viewportRegistry.get();
    return true;
}

void VulkanDynamicRHI::initDescriptorDefaults() noexcept
{
    resource::TextureCreateDesc desc =
        resource::TextureCreateDesc::create2D(kDefaultTextureName, 1, 1, PixelFormat::R8G8B8A8_UNorm);
    desc.setFlags(resource::TextureCreateFlags::ShaderResource);

    // The initial layout transition goes through the shared upload context, which `postInit()` (or the first
    // finalized command list) submits; the immediate command list is only the recording entry point.
    VulkanTexture* const texture = new VulkanTexture(m_resourceServices, command::getImmediateCommandList(), desc);
    m_defaultTexture = resource::TextureRef(texture);
    if (texture->isValid())
    {
        VulkanTextureViewDesc viewDesc{};
        viewDesc.viewType = texture->getViewType();
        viewDesc.aspectFlags = texture->getFullAspectMask();
        viewDesc.pixelFormat = PixelFormat::R8G8B8A8_UNorm;
        viewDesc.format = texture->getViewFormat();
        // View creation requires resolved ranges; the default texture is a single 1x1 mip with one layer.
        viewDesc.firstMip = 0u;
        viewDesc.numMips = 1u;
        viewDesc.arraySliceIndex = 0u;
        viewDesc.numArraySlices = 1u;
        viewDesc.useIdentitySwizzle = true;

        const VulkanView* const view = texture->findOrAddInternalView(viewDesc);
        if ((view != nullptr) && (view->getViewKind() == VulkanViewKind::Texture))
        {
            m_descriptorDefaults.defaultImageView = &view->getTextureView();
        }
    }
    else
    {
        GP_LOG_WARNING(LogVulkanRHI, "Failed to create the default descriptor texture; unbound images stay null.");
    }

    m_defaultSampler = m_samplerCache->findOrCreate(pipeline::SamplerStateDesc{});
    m_descriptorDefaults.defaultSampler = static_cast<const VulkanSamplerState*>(m_defaultSampler.get());

    for (gp::USize index = 0u; index < kVulkanNumGlobalSamplers; ++index)
    {
        m_globalSamplers[index] = m_samplerCache->findOrCreate(makeGlobalSamplerDesc(index));
        m_descriptorDefaults.globalSamplers[index] =
            static_cast<const VulkanSamplerState*>(m_globalSamplers[index].get());
    }
}

void VulkanDynamicRHI::initContexts() noexcept
{
    // Contexts copy the descriptor defaults when they are built, so this runs after `initDescriptorDefaults()`.
    m_defaultContext = gp::makeUnique<VulkanCommandListContext>(m_contextServices);
}

void VulkanDynamicRHI::releaseAll() noexcept
{
    // 1. Drain the GPU: submit what is still recorded, wait, and retire every in-flight payload.
    if ((m_contextProvider != nullptr) && (m_submitter != nullptr))
    {
        flushPendingUploads(true);
    }
    if ((m_device != nullptr) && m_device->isValid())
    {
        m_device->waitUntilIdle();
    }
    pollQueues();

    // Viewports are owned by the application and may outlive the RHI. Their swapchains, surfaces and image views
    // must not: strip them now, while the device, the instance and the deletion queue still exist.
    if (m_viewportRegistry != nullptr)
    {
        const gp::USize numLiveViewports = m_viewportRegistry->releaseAll();
        if (numLiveViewports > 0u)
        {
            GP_LOG_WARNING(
                LogVulkanRHI,
                "{} viewport(s) still referenced at RHI shutdown; their swapchains were released.",
                numLiveViewports
            );
        }
    }

    // 2. Contexts reference pipelines and descriptor pools; they go first, after the listener stops pointing at them.
    if (m_isPipelineListener)
    {
        if (getVulkanPipelineDeletionListener() == this)
        {
            static_cast<void>(setVulkanPipelineDeletionListener(nullptr));
        }
        m_isPipelineListener = false;
    }
    m_freeParallelContexts.clear();
    m_parallelContexts.clear();
    m_defaultContext.reset();
    m_contextProvider.reset();
    m_uploadPool.reset();

    // 3. Engine-owned resources: their destructors enqueue native handles into the deletion queue, so it is drained
    //    while every service the handles need is still alive.
    m_descriptorDefaults = VulkanDescriptorDefaults{};
    for (pipeline::SamplerStateRef& sampler: m_globalSamplers)
    {
        sampler.reset();
    }
    m_defaultSampler.reset();
    m_defaultTexture.reset();
    m_lastSubmittedSyncPoint.reset();

    // Dropping the last reference only marks an RHI wrapper for deletion; the destructor runs when the RHI deletion
    // queue is flushed. Flush it now, otherwise the wrappers would be destroyed after the device is gone.
    VulkanDeferredDeletionQueue::flushRHIDeletionQueue(true);

    VulkanVertexDeclaration::emptyCache();
    if (m_pipelineCache != nullptr)
    {
        m_pipelineCache->destroyCache();
    }
    m_samplerCache.reset();
    if (m_deferredDeletionQueue != nullptr)
    {
        m_deferredDeletionQueue->releaseResources(true);
    }
    m_pipelineCache.reset();
    m_shaderFactory.reset();

    // 4. Queues delete their completed payloads, which return descriptor pools and command buffers.
    m_computeQueue.reset();
    m_graphicsQueue.reset();
    m_submitter.reset();
    VulkanDeferredDeletionQueue::flushRHIDeletionQueue(true);
    if (m_deferredDeletionQueue != nullptr)
    {
        m_deferredDeletionQueue->releaseResources(true);
    }

    // 5. Device-level caches and allocators.
    m_renderPassManager.reset();
    m_descriptorSetCache.reset();
    m_descriptorPoolsManager.reset();
    m_queryPoolManager.reset();
    m_tempBlockAllocator.reset();
    if (m_deferredDeletionQueue != nullptr)
    {
        m_deferredDeletionQueue->releaseResources(true);
    }

    // 6. Memory, in reverse dependency order.
    if (m_stagingManager != nullptr)
    {
        m_stagingManager->deinit();
        m_stagingManager.reset();
    }
    if (m_memoryManager != nullptr)
    {
        m_memoryManager->setDeferredDeletionQueue(nullptr);
    }
    m_deferredDeletionQueue.reset();
    if (m_memoryManager != nullptr)
    {
        m_memoryManager->deinit();
        m_memoryManager.reset();
    }
    if (m_deviceMemoryManager != nullptr)
    {
        m_deviceMemoryManager->deinit();
        m_deviceMemoryManager.reset();
    }
    if (m_fenceManager != nullptr)
    {
        m_fenceManager->deinit();
        m_fenceManager.reset();
    }

    m_resourceServices = VulkanResourceServices{};
    m_viewportServices = VulkanViewportServices{};
    m_viewportRegistry.reset();
    m_contextServices = VulkanContextServices{};

    // 7. Device, instance and loader.
    if (m_device != nullptr)
    {
        m_device->destroy();
        m_device.reset();
    }
    if (m_hasInstance)
    {
        m_instance.shutdown();
        m_hasInstance = false;
    }
    if (m_ownsLoaderLibrary)
    {
        VulkanLoader::freeLibrary();
        m_ownsLoaderLibrary = false;
    }
    m_deviceUuid = gp::Array<gp::UInt8, VK_UUID_SIZE>{};
}

void VulkanDynamicRHI::flushPendingUploads(const bool waitForCompletion) noexcept
{
    if ((m_contextProvider == nullptr) || (m_submitter == nullptr) ||
        (m_contextProvider->getActiveUploadContext() == nullptr))
    {
        return;
    }

    VulkanPlatformCommandList commandList{};
    static_cast<void>(m_contextProvider->finalizeUploads(commandList));
    m_submitter->submitPayloads(commandList.payloads);
    commandList.payloads.clear();

    if (waitForCompletion && (m_device != nullptr))
    {
        m_device->waitUntilIdle();
        pollQueues();
    }
}

void VulkanDynamicRHI::pollQueues() noexcept
{
    if (m_graphicsQueue != nullptr)
    {
        static_cast<void>(m_graphicsQueue->processInterruptQueue(0u));
    }
    if (m_computeQueue != nullptr)
    {
        static_cast<void>(m_computeQueue->processInterruptQueue(0u));
    }
}

VulkanCommandListContext& VulkanDynamicRHI::toVulkanContext(command::IComputeContext& context) noexcept
{
    return static_cast<VulkanCommandListContext&>(context.getLowestLevelContext());
}

void VulkanDynamicRHI::releaseParallelContext(VulkanCommandListContext& context) noexcept
{
    if (&context == m_defaultContext.get())
    {
        return;
    }

    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_contextMutex);
    for (const gp::UniquePtr<VulkanCommandListContext>& owned: m_parallelContexts)
    {
        if (owned.get() == &context)
        {
            m_freeParallelContexts.pushBack(&context);
            return;
        }
    }
}

}   // namespace gp::rhi::vulkan
