// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "pipeline/VulkanPipelineStateCacheManager.hpp"
#include "containers/arrays/Array.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "descriptors/VulkanDescriptorSetsLayoutInfo.hpp"
#include "descriptors/VulkanDescriptorSetsTypes.hpp"
#include "descriptors/VulkanPipelineLayout.hpp"
#include "device/VulkanDevice.hpp"
#include "extensions/VulkanExtensionTypes.hpp"
#include "loader/VulkanCommandWrappers.hpp"
#include "logging/LogMacros.hpp"
#include "pipeline/BoundShaderState.hpp"
#include "pipeline/GraphicsPipelineStateDesc.hpp"
#include "pipeline/VulkanComputePipelineState.hpp"
#include "pipeline/VulkanGraphicsPipelineDesc.hpp"
#include "pipeline/VulkanGraphicsPipelineState.hpp"
#include "pipeline/VulkanPipelineDynamicStates.hpp"
#include "pipeline/VulkanPipelinePrecache.hpp"
#include "pipeline/VulkanPipelineShaders.hpp"
#include "pipeline/VulkanWrappedPipelineStates.hpp"
#include "renderpass/VulkanRenderPass.hpp"
#include "renderpass/VulkanRenderPassManager.hpp"
#include "renderpass/VulkanRenderTargetLayout.hpp"
#include "RHIDefinitions.hpp"
#include "shaders/GeometryShader.hpp"   // IWYU pragma: keep
#include "shaders/MeshShader.hpp"       // IWYU pragma: keep
#include "shaders/PixelShader.hpp"      // IWYU pragma: keep
#include "shaders/VertexShader.hpp"     // IWYU pragma: keep
#include "shaders/VulkanBaseShader.hpp"
#include "shaders/VulkanShader.hpp"
#include "shaders/VulkanShaderHeader.hpp"
#include "shaders/VulkanShaderHeaderEnums.hpp"
#include "shaders/VulkanShaderModule.hpp"
#include "states/VulkanRasterizerState.hpp"
#include "threading/sync/SharedLock.hpp"
#include "threading/sync/UniqueLock.hpp"
#include "translation/VulkanPixelFormat.hpp"
#include "translation/VulkanShaderEnums.hpp"
#include "utils/VulkanResult.hpp"
#include "VulkanCommon.hpp"
#include "VulkanConfiguration.hpp"

namespace gp::rhi::vulkan
{

namespace
{

using MutexLock = gp::thread::UniqueLock<gp::thread::Mutex>;
using ExclusiveLock = gp::thread::UniqueLock<gp::thread::SharedMutex>;
using ReaderLock = gp::thread::SharedLock<gp::thread::SharedMutex>;

/// @brief Points a shader stage at its specialization constants.
/// @param[in] constants Constants of the stage; must outlive pipeline creation.
/// @param[out] outSpecializationInfo Storage for the specialization info.
/// @param[in,out] stageInfo Stage create info that receives the info when there are constants.
void applySpecializationConstants(
    const shader::SpecializationContainer& constants,
    VkSpecializationInfo& outSpecializationInfo,
    VkPipelineShaderStageCreateInfo& stageInfo
) noexcept
{
    if (constants.isEmpty())
    {
        return;
    }
    GP_ASSERT(constants.size() <= kMaxNumSpecializationConstants, "Too many specialization constants for one stage.");
    const gp::UInt32 count = static_cast<gp::UInt32>(
        (constants.size() <= kMaxNumSpecializationConstants) ? constants.size() : kMaxNumSpecializationConstants
    );

    outSpecializationInfo = {};
    outSpecializationInfo.mapEntryCount = count;
    outSpecializationInfo.pMapEntries = kVulkanSpecializationMapEntries.data();
    outSpecializationInfo.dataSize = count * kSpecializationConstantSize;
    outSpecializationInfo.pData = constants.data();
    stageInfo.pSpecializationInfo = &outSpecializationInfo;
}

/// @brief Requests the subgroup size a shader was compiled for, when the device can honor it for that stage.
/// @param[in] device Logical device.
/// @param[in] header Shader header (`waveSize`, 0 when the shader has no requirement).
/// @param[in] stageBit Stage of the shader.
/// @param[out] outSubgroupInfo Storage for the requirement.
/// @param[in,out] stageInfo Stage create info that receives the requirement.
void applyRequiredSubgroupSize(
    const VulkanDevice& device,
    const VulkanShaderHeader& header,
    const VkShaderStageFlagBits stageBit,
    VkPipelineShaderStageRequiredSubgroupSizeCreateInfoEXT& outSubgroupInfo,
    VkPipelineShaderStageCreateInfo& stageInfo
) noexcept
{
    if ((header.waveSize == 0u) || (device.getOptionalExtensions().hasEXTSubgroupSizeControl == 0u))
    {
        return;
    }
    const VkPhysicalDeviceSubgroupSizeControlPropertiesEXT& properties =
        device.getOptionalExtensionProperties().subgroupSizeControlProperties;
    const gp::UInt32 waveSize = header.waveSize;
    if (((properties.requiredSubgroupSizeStages & static_cast<VkShaderStageFlags>(stageBit)) == 0u) ||
        (waveSize < properties.minSubgroupSize) || (waveSize > properties.maxSubgroupSize))
    {
        return;
    }

    outSubgroupInfo = {};
    outSubgroupInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_REQUIRED_SUBGROUP_SIZE_CREATE_INFO_EXT;
    outSubgroupInfo.requiredSubgroupSize = waveSize;
    stageInfo.pNext = &outSubgroupInfo;
}

}   // namespace

VulkanPipelineStateCacheManager::VulkanPipelineStateCacheManager(
    VulkanDevice& device,
    VulkanDeferredDeletionQueue* const deferredDeletionQueue,
    VulkanRenderPassManager* const renderPassManager,
    const VulkanPipelineLRUSettings& lruSettings
) noexcept
    : m_device(device)
    , m_deferredDeletionQueue(deferredDeletionQueue)
    , m_renderPassManager(renderPassManager)
    , m_lruSettings(lruSettings)
    , m_renderTargetCaps(
          (renderPassManager != nullptr) ? renderPassManager->getCaps() : makeVulkanRenderTargetCaps(device)
      )
    , m_dynamicStateFeatures(makeVulkanDynamicStateFeatures(device))
    , m_useDynamicRendering(
          (renderPassManager != nullptr) ? renderPassManager->usesDynamicRendering()
                                         : (device.getOptionalExtensions().hasKHRDynamicRendering != 0u)
      )
    , m_descriptorSetLayoutCache(device, deferredDeletionQueue)
{
    if ((m_device.getHandle() == VK_NULL_HANDLE) || (dynamic_api::vkCreatePipelineCache == nullptr))
    {
        return;
    }

    VkPipelineCacheCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    const VkResult result =
        vkCreatePipelineCache(m_device.getHandle(), &createInfo, GP_VULKAN_CPU_ALLOCATOR, &m_globalPipelineCache);
    if (result != VK_SUCCESS)
    {
        GP_LOG_ERROR(LogVulkanRHI, "vkCreatePipelineCache failed with {}.", vulkanResultToString(result));
        m_globalPipelineCache = VK_NULL_HANDLE;
    }
}

VulkanPipelineStateCacheManager::~VulkanPipelineStateCacheManager() noexcept
{
    destroyCache();

    // Layouts are only destroyed with the manager: wrappers detached by destroyCache() may still reference them.
    {
        MutexLock lock(m_layoutMutex);
        for (VulkanPipelineLayout* const layout: m_layouts)
        {
            delete layout;
        }
        m_layouts.clear();
    }

    if ((m_globalPipelineCache != VK_NULL_HANDLE) && (dynamic_api::vkDestroyPipelineCache != nullptr))
    {
        ExclusiveLock lock(m_globalPipelineCacheMutex);
        vkDestroyPipelineCache(m_device.getHandle(), m_globalPipelineCache, GP_VULKAN_CPU_ALLOCATOR);
        m_globalPipelineCache = VK_NULL_HANDLE;
    }
}

pipeline::GraphicsPipelineStateRef VulkanPipelineStateCacheManager::createGraphicsPipelineState(
    const pipeline::GraphicsPipelineStateInitializer& initializer
) noexcept
{
    const bool isPrecache = initializer.isPSOPrecache();

    // 1. Translate the initializer; the key decides whether a native pipeline already exists.
    VulkanDescriptorSetsLayoutInfo layoutInfo{};
    VulkanGraphicsPipelineDesc desc{};
    if (!buildVulkanGraphicsPipelineDesc(m_device.getLimits(), m_renderTargetCaps, initializer, layoutInfo, desc))
    {
        GP_LOG_ERROR(LogVulkanRHI, "Invalid graphics pipeline initializer: missing shader or fixed-function state.");
        return pipeline::GraphicsPipelineStateRef();
    }
    VulkanPipelineKey key = desc.createKey();
    const gp::UInt64 keyHash = key.getHash();

    // 2. Look up, wait for a concurrent creation of the same key, or insert a placeholder entry.
    gp::RefCountedPtr<VulkanGraphicsPipelineState> state{};
    for (;;)
    {
        MutexLock lock(m_graphicsMutex);
        GraphicsEntry* const entry = findGraphicsEntry(key);
        if (entry != nullptr)
        {
            VulkanGraphicsPipelineState* const found = entry->state.get();
            if (!found->m_isInitializing)
            {
                if (!isPrecache)
                {
                    lruTouch(*found);
                }
                found->m_numWrappers.fetchAdd(1, gp::memory::MemoryOrder::AcqRel);
                return makeGraphicsWrapper(found);
            }

            // Another thread is compiling this pipeline: block on its mutex instead of compiling it twice. The
            // reference keeps the state alive if the creator fails and removes it meanwhile.
            const gp::RefCountedPtr<VulkanGraphicsPipelineState> pending(found, gp::kRetainRef);
            lock.unlock();
            pending->m_initializingMutex.lock();
            pending->m_initializingMutex.unlock();
            continue;
        }

        state = gp::RefCountedPtr<VulkanGraphicsPipelineState>(
            new VulkanGraphicsPipelineState(m_device, m_deferredDeletionQueue, this, std::move(desc), std::move(key)),
            gp::kRetainRef
        );
        state->m_isInitializing = true;
        state->m_initializingMutex.lock();
        // The wrapper of this request is counted now, so a concurrent release can never drop the entry before it.
        state->m_numWrappers.store(1, gp::memory::MemoryOrder::Release);

        GraphicsEntry& newEntry = m_graphicsPipelines.emplaceBack();
        newEntry.hash = keyHash;
        newEntry.state = state;
        break;
    }

    // 3. Compile outside the map lock.
    VulkanGraphicsPipelineState& newState = *state;
    newState.m_precacheKey = computeVulkanPrecachePSOHash(initializer, m_device.getVendorId());
    bool isCreated = prepareGraphicsState(initializer, layoutInfo, newState);

    const bool isRecordOnly = isPrecache && m_lruSettings.isEnabled && m_lruSettings.evictPrecachedImmediately;
    if (isCreated && !isRecordOnly)
    {
        isCreated = createGraphicsVkPipeline(newState);
    }

    {
        MutexLock lock(m_graphicsMutex);
        newState.m_isInitializing = false;
        if (!isCreated)
        {
            removeGraphicsEntry(newState);
            newState.m_numWrappers.store(0, gp::memory::MemoryOrder::Release);
            newState.m_owner.store(nullptr, gp::memory::MemoryOrder::Release);
        }
        else if (m_lruSettings.isEnabled && (newState.m_pipeline != VK_NULL_HANDLE))
        {
            lruTrim(newState.m_pipelineSize);
            lruAdd(newState);
            // A precache only warms the driver cache; the pipeline is rebuilt (cheaply) on its first real use.
            if (isPrecache)
            {
                lruRemove(newState);
            }
        }
    }
    newState.m_initializingMutex.unlock();

    if (!isCreated)
    {
        return pipeline::GraphicsPipelineStateRef();
    }
    return makeGraphicsWrapper(state.get());
}

pipeline::ComputePipelineStateRef VulkanPipelineStateCacheManager::createComputePipelineState(
    const pipeline::ComputePipelineStateInitializer& initializer
) noexcept
{
    if (initializer.computeShader == nullptr)
    {
        return pipeline::ComputePipelineStateRef();
    }

    const VulkanShader* const shader = static_cast<VulkanComputeShader*>(initializer.computeShader);
    const gp::UInt64 key = computeVulkanComputePipelineKey(shader->getShaderKey(), initializer.specializationConstants);

    // 1. Fast path under the shared lock. Wrapper counts only drop under the exclusive lock, so incrementing here is
    //    safe.
    {
        ReaderLock lock(m_computeMutex);
        const ComputeEntry* const entry = findComputeEntry(shader, key);
        if (entry != nullptr)
        {
            entry->state->m_numWrappers.fetchAdd(1, gp::memory::MemoryOrder::AcqRel);
            return makeComputeWrapper(entry->state.get());
        }
    }

    // 2. Compile outside any lock. A failed compilation is cached as well, so it is not retried every frame.
    gp::RefCountedPtr<VulkanComputePipelineState> created(
        new VulkanComputePipelineState(
            m_device, m_deferredDeletionQueue, this, initializer.computeShader, initializer.specializationConstants, key
        ),
        gp::kRetainRef
    );
    created->m_isValid = createComputeVkPipeline(*created);

    // 3. Insert unless another thread won the race; the loser's pipeline is released with `created`.
    ExclusiveLock lock(m_computeMutex);
    const ComputeEntry* const existing = findComputeEntry(shader, key);
    VulkanComputePipelineState* target = nullptr;
    if (existing != nullptr)
    {
        target = existing->state.get();
        created->m_owner.store(nullptr, gp::memory::MemoryOrder::Release);
    }
    else
    {
        ComputeEntry& entry = m_computePipelines.emplaceBack();
        entry.shader = shader;
        entry.key = key;
        entry.state = created;
        target = created.get();
    }
    target->m_numWrappers.fetchAdd(1, gp::memory::MemoryOrder::AcqRel);
    return makeComputeWrapper(target);
}

VulkanPipelineLayout* VulkanPipelineStateCacheManager::findOrAddLayout(
    const VulkanDescriptorSetsLayoutInfo& layoutInfo, const bool isGfxLayout
) noexcept
{
    MutexLock lock(m_layoutMutex);
    const gp::UInt32 hash = layoutInfo.getHash();
    for (VulkanPipelineLayout* const layout: m_layouts)
    {
        const VulkanDescriptorSetsLayoutInfo& existing = layout->getDescriptorSetsLayout();
        if ((layout->isGfxLayout() == isGfxLayout) && (existing.getHash() == hash) && (existing == layoutInfo))
        {
            return layout;
        }
    }

    VulkanPipelineLayout* const layout =
        new VulkanPipelineLayout(m_device, isGfxLayout, false, m_deferredDeletionQueue);
    static_cast<VulkanDescriptorSetsLayoutInfo&>(layout->getDescriptorSetsLayout()) = layoutInfo;
    if (!layout->compile(m_descriptorSetLayoutCache))
    {
        delete layout;
        return nullptr;
    }
    m_layouts.pushBack(layout);
    return layout;
}

void VulkanPipelineStateCacheManager::touchGraphicsPipeline(VulkanGraphicsPipelineState& state) noexcept
{
    if (!m_lruSettings.isEnabled)
    {
        return;
    }
    MutexLock lock(m_graphicsMutex);
    if (!state.m_isInitializing && !state.m_isEvictedFromCache)
    {
        lruTouch(state);
    }
}

void VulkanPipelineStateCacheManager::setFrameNumber(const gp::UInt64 frameNumber) noexcept
{
    m_frameNumber.store(frameNumber, gp::memory::MemoryOrder::Release);
}

gp::UInt64 VulkanPipelineStateCacheManager::getFrameNumber() const noexcept
{
    return m_frameNumber.load(gp::memory::MemoryOrder::Acquire);
}

void VulkanPipelineStateCacheManager::tickLRU(const gp::UInt64 frameNumber) noexcept
{
    setFrameNumber(frameNumber);
    if (!m_lruSettings.isEnabled || (m_lruSettings.evictAfterUnusedFrames == 0u))
    {
        return;
    }

    MutexLock lock(m_graphicsMutex);
    for (gp::UInt32 evicted = 0u; evicted < kMaxPipelineEvictsPerTick; ++evicted)
    {
        VulkanGraphicsPipelineState* const oldest = m_lruTail;
        if ((oldest == nullptr) || (oldest->m_lruFrame + m_lruSettings.evictAfterUnusedFrames >= frameNumber))
        {
            break;
        }
        lruRemove(*oldest);
    }
}

void VulkanPipelineStateCacheManager::destroyCache() noexcept
{
    {
        MutexLock lock(m_graphicsMutex);
        for (GraphicsEntry& entry: m_graphicsPipelines)
        {
            VulkanGraphicsPipelineState& state = *entry.state;
            lruRemove(state);
            state.m_isEvictedFromCache = true;
            state.m_owner.store(nullptr, gp::memory::MemoryOrder::Release);
        }
        m_graphicsPipelines.clear();
    }
    {
        ExclusiveLock lock(m_computeMutex);
        for (ComputeEntry& entry: m_computePipelines)
        {
            entry.state->m_isEvictedFromCache = true;
            entry.state->m_owner.store(nullptr, gp::memory::MemoryOrder::Release);
        }
        m_computePipelines.clear();
    }
}

void VulkanPipelineStateCacheManager::releaseGraphicsWrapper(VulkanGraphicsPipelineState& state) noexcept
{
    MutexLock lock(m_graphicsMutex);
    const gp::Int32 previous = state.m_numWrappers.fetchSub(1, gp::memory::MemoryOrder::AcqRel);
    if (previous > 1)
    {
        return;
    }

    lruRemove(state);
    if (state.m_isEvictedFromCache)
    {
        return;
    }
    removeGraphicsEntry(state);
}

void VulkanPipelineStateCacheManager::releaseComputeWrapper(VulkanComputePipelineState& state) noexcept
{
    ExclusiveLock lock(m_computeMutex);
    const gp::Int32 previous = state.m_numWrappers.fetchSub(1, gp::memory::MemoryOrder::AcqRel);
    if ((previous > 1) || state.m_isEvictedFromCache)
    {
        return;
    }

    for (gp::USize index = 0u; index < m_computePipelines.size(); ++index)
    {
        if (m_computePipelines[index].state.get() == &state)
        {
            m_computePipelines.removeAtSwap(index);
            return;
        }
    }
}

gp::USize VulkanPipelineStateCacheManager::getNumGraphicsPipelines() const noexcept
{
    MutexLock lock(m_graphicsMutex);
    return m_graphicsPipelines.size();
}

gp::USize VulkanPipelineStateCacheManager::getNumComputePipelines() const noexcept
{
    ReaderLock lock(m_computeMutex);
    return m_computePipelines.size();
}

gp::USize VulkanPipelineStateCacheManager::getNumLayouts() const noexcept
{
    MutexLock lock(m_layoutMutex);
    return m_layouts.size();
}

gp::UInt32 VulkanPipelineStateCacheManager::getLRUCount() const noexcept
{
    MutexLock lock(m_graphicsMutex);
    return m_lruCount;
}

gp::UInt64 VulkanPipelineStateCacheManager::getLRUSizeInBytes() const noexcept
{
    MutexLock lock(m_graphicsMutex);
    return m_lruSizeInBytes;
}

VulkanPipelineStateCacheManager::GraphicsEntry*
    VulkanPipelineStateCacheManager::findGraphicsEntry(const VulkanPipelineKey& key) noexcept
{
    const gp::UInt64 hash = key.getHash();
    for (GraphicsEntry& entry: m_graphicsPipelines)
    {
        if ((entry.hash == hash) && (entry.state->m_key == key))
        {
            return &entry;
        }
    }
    return nullptr;
}

void VulkanPipelineStateCacheManager::removeGraphicsEntry(const VulkanGraphicsPipelineState& state) noexcept
{
    for (gp::USize index = 0u; index < m_graphicsPipelines.size(); ++index)
    {
        if (m_graphicsPipelines[index].state.get() == &state)
        {
            m_graphicsPipelines.removeAtSwap(index);
            return;
        }
    }
}

const VulkanPipelineStateCacheManager::ComputeEntry* VulkanPipelineStateCacheManager::findComputeEntry(
    const VulkanShader* const shader, const gp::UInt64 key
) const noexcept
{
    for (const ComputeEntry& entry: m_computePipelines)
    {
        if ((entry.key == key) && (entry.shader == shader))
        {
            return &entry;
        }
    }
    return nullptr;
}

bool VulkanPipelineStateCacheManager::prepareGraphicsState(
    const pipeline::GraphicsPipelineStateInitializer& initializer,
    const VulkanDescriptorSetsLayoutInfo& layoutInfo,
    VulkanGraphicsPipelineState& state
) noexcept
{
    const pipeline::BoundShaderStateInput& bound = initializer.boundShaderState;

    // Shaders and their constants are kept so an evicted pipeline can be rebuilt without the initializer.
    state.m_shaders = getVulkanGraphicsShaders(bound);
    state.m_rhiShaders[static_cast<gp::USize>(shader_stage::Stage::Vertex)] =
        shader::GraphicsShaderRef(bound.getVertexShader(), gp::kRetainRef);
    state.m_rhiShaders[static_cast<gp::USize>(shader_stage::Stage::Pixel)] =
        shader::GraphicsShaderRef(bound.getPixelShader(), gp::kRetainRef);
    state.m_rhiShaders[static_cast<gp::USize>(shader_stage::Stage::Geometry)] =
        shader::GraphicsShaderRef(bound.getGeometryShader(), gp::kRetainRef);
    state.m_rhiShaders[static_cast<gp::USize>(shader_stage::Stage::Mesh)] =
        shader::GraphicsShaderRef(bound.getMeshShader(), gp::kRetainRef);
    state.m_rhiShaders[static_cast<gp::USize>(shader_stage::Stage::Task)] =
        shader::GraphicsShaderRef(bound.getAmplificationShader(), gp::kRetainRef);
    for (gp::UInt32 stage = 0u; stage < kNumGraphicsPipelineStages; ++stage)
    {
        state.m_specializationConstants[stage] =
            getVulkanStageSpecializationConstants(bound, static_cast<shader_stage::Stage>(stage));
    }

    state.m_layout = findOrAddLayout(layoutInfo, true);
    if (state.m_layout == nullptr)
    {
        return false;
    }
    state.m_hasInputAttachments = layoutInfo.hasInputAttachments();

    if (m_useDynamicRendering)
    {
        return true;
    }
    if (m_renderPassManager == nullptr)
    {
        GP_LOG_ERROR(LogVulkanRHI, "Graphics pipelines need a render pass manager without dynamic rendering.");
        return false;
    }
    state.m_renderPass =
        m_renderPassManager->getOrCreateRenderPass(VulkanRenderTargetLayout(m_renderTargetCaps, initializer));
    return state.m_renderPass != nullptr;
}

bool VulkanPipelineStateCacheManager::createGraphicsVkPipeline(VulkanGraphicsPipelineState& state) noexcept
{
    if ((m_device.getHandle() == VK_NULL_HANDLE) || (dynamic_api::vkCreateGraphicsPipelines == nullptr) ||
        (state.m_layout == nullptr))
    {
        return false;
    }

    const VulkanGraphicsPipelineDesc& desc = state.m_desc;
    const gp::UInt32 rasterizationSamples = (desc.rasterizationSamples > 1u) ? desc.rasterizationSamples : 1u;
    const gp::UInt32 layoutHash = state.m_layout->getDescriptorSetLayoutHash();

    // 1. Shader stages. Modules are referenced only until creation returns; the shader keeps its module cache.
    gp::Array<gp::RefCountedPtr<VulkanShaderModule>, kNumGraphicsPipelineStages> modules{};
    gp::Array<VkPipelineShaderStageCreateInfo, kNumGraphicsPipelineStages> stages{};
    gp::Array<VkSpecializationInfo, kNumGraphicsPipelineStages> specializationInfos{};
    gp::Array<VkPipelineShaderStageRequiredSubgroupSizeCreateInfoEXT, kNumGraphicsPipelineStages> subgroupInfos{};
    gp::UInt32 numStages = 0u;
    for (gp::UInt32 stage = 0u; stage < kNumGraphicsPipelineStages; ++stage)
    {
        VulkanShader* const shader = state.m_shaders[stage];
        if (shader == nullptr)
        {
            continue;
        }
        modules[stage] = shader->getOrCreateHandle(rasterizationSamples, layoutHash);
        if (!modules[stage].isValid() || !modules[stage]->isValid())
        {
            GP_LOG_ERROR(LogVulkanRHI, "Missing shader module for graphics pipeline stage {}.", stage);
            return false;
        }

        const VkShaderStageFlagBits stageBit = gfxStageToVkStageBit(static_cast<shader_stage::Stage>(stage));
        VkPipelineShaderStageCreateInfo& stageInfo = stages[numStages];
        stageInfo = {};
        stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stageInfo.stage = stageBit;
        stageInfo.module = modules[stage]->getHandle();
        stageInfo.pName = shader->getEntryPoint();
        applySpecializationConstants(state.m_specializationConstants[stage], specializationInfos[numStages], stageInfo);
        applyRequiredSubgroupSize(m_device, shader->getCodeHeader(), stageBit, subgroupInfos[numStages], stageInfo);
        ++numStages;
    }

    // 2. Color blend: outputs the pixel shader never writes get an empty write mask.
    const VulkanShader* const pixelShader = state.m_shaders[static_cast<gp::USize>(shader_stage::Stage::Pixel)];
    const gp::UInt32 pixelOutputMask = (pixelShader != nullptr) ? pixelShader->getCodeHeader().inOutMask : 0xffffffffu;
    gp::Array<VkPipelineColorBlendAttachmentState, kMaxSimultaneousRenderTargets> blendAttachments{};
    const gp::UInt32 numColorAttachments = static_cast<gp::UInt32>(
        (desc.colorAttachmentStates.size() <= kMaxSimultaneousRenderTargets) ? desc.colorAttachmentStates.size()
                                                                             : kMaxSimultaneousRenderTargets
    );
    for (gp::UInt32 index = 0u; index < numColorAttachments; ++index)
    {
        desc.colorAttachmentStates[index].writeInto(blendAttachments[index]);
        if ((pixelOutputMask & (1u << index)) == 0u)
        {
            blendAttachments[index].colorWriteMask = 0u;
        }
    }
    VkPipelineColorBlendStateCreateInfo colorBlendState{};
    colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlendState.attachmentCount = numColorAttachments;
    colorBlendState.pAttachments = blendAttachments.data();
    colorBlendState.blendConstants[0] = 1.0f;
    colorBlendState.blendConstants[1] = 1.0f;
    colorBlendState.blendConstants[2] = 1.0f;
    colorBlendState.blendConstants[3] = 1.0f;

    // 3. Viewport: with extended dynamic state 1 the counts are dynamic too and must be 0 here.
    const gp::UInt32 viewportCount = m_dynamicStateFeatures.hasExtendedDynamicState1 ? 0u : 1u;
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = viewportCount;
    viewportState.scissorCount = viewportCount;

    VkPipelineMultisampleStateCreateInfo multisampleState{};
    multisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampleState.rasterizationSamples = static_cast<VkSampleCountFlagBits>(rasterizationSamples);
    multisampleState.alphaToCoverageEnable = (desc.useAlphaToCoverage != 0u) ? VK_TRUE : VK_FALSE;

    // 4. Vertex input and input assembly (mesh pipelines have neither).
    gp::Array<VkVertexInputBindingDescription, kMaxVertexElementCount> vertexBindings{};
    gp::Array<VkVertexInputAttributeDescription, kMaxVertexElementCount> vertexAttributes{};
    const gp::UInt32 numBindings = static_cast<gp::UInt32>(
        (desc.vertexBindings.size() <= kMaxVertexElementCount) ? desc.vertexBindings.size() : kMaxVertexElementCount
    );
    const gp::UInt32 numAttributes = static_cast<gp::UInt32>(
        (desc.vertexAttributes.size() <= kMaxVertexElementCount) ? desc.vertexAttributes.size() : kMaxVertexElementCount
    );
    for (gp::UInt32 index = 0u; index < numBindings; ++index)
    {
        desc.vertexBindings[index].writeInto(vertexBindings[index]);
    }
    for (gp::UInt32 index = 0u; index < numAttributes; ++index)
    {
        desc.vertexAttributes[index].writeInto(vertexAttributes[index]);
    }
    VkPipelineVertexInputStateCreateInfo vertexInputState{};
    vertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputState.vertexBindingDescriptionCount = numBindings;
    vertexInputState.pVertexBindingDescriptions = vertexBindings.data();
    vertexInputState.vertexAttributeDescriptionCount = numAttributes;
    vertexInputState.pVertexAttributeDescriptions = vertexAttributes.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState{};
    inputAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssemblyState.topology = static_cast<VkPrimitiveTopology>(desc.topology);

    // 5. Rasterizer and depth-stencil.
    VkPipelineRasterizationStateCreateInfo rasterizationState{};
    VulkanRasterizerState::resetCreateInfo(rasterizationState);
    desc.rasterizer.writeInto(rasterizationState);

    VkPipelineDepthStencilStateCreateInfo depthStencilState{};
    depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    desc.depthStencil.writeInto(depthStencilState);

    // 6. Dynamic states.
    const VulkanDynamicStateList dynamicStates = fillVulkanDynamicStates(m_dynamicStateFeatures, desc);
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = dynamicStates.count;
    dynamicState.pDynamicStates = dynamicStates.states.data();

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = numStages;
    pipelineInfo.pStages = stages.data();
    pipelineInfo.pVertexInputState = desc.hasMeshShader() ? nullptr : &vertexInputState;
    pipelineInfo.pInputAssemblyState = desc.hasMeshShader() ? nullptr : &inputAssemblyState;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizationState;
    pipelineInfo.pMultisampleState = &multisampleState;
    pipelineInfo.pDepthStencilState = &depthStencilState;
    pipelineInfo.pColorBlendState = &colorBlendState;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = state.m_layout->getPipelineLayout();
    pipelineInfo.subpass = desc.subpassIndex;
    pipelineInfo.basePipelineIndex = -1;

    // 7. Attachments: formats for dynamic rendering, or the compatible render pass.
    const VulkanRenderTargetsDesc& renderTargets = desc.renderTargets;
    VkPipelineRenderingCreateInfoKHR renderingInfo{};
    gp::Array<VkFormat, kMaxSimultaneousRenderTargets> colorFormats{};
    VkRenderingInputAttachmentIndexInfoKHR inputAttachmentIndexInfo{};
    gp::Array<gp::UInt32, kMaxSimultaneousRenderTargets> colorInputIndices{};
    const gp::UInt32 depthInputIndex = static_cast<gp::UInt32>(VulkanShaderAttachmentType::Depth);
    if (m_useDynamicRendering)
    {
        const gp::UInt32 numColorFormats = static_cast<gp::UInt32>(
            (renderTargets.colorAttachments.size() <= kMaxSimultaneousRenderTargets)
                ? renderTargets.colorAttachments.size()
                : kMaxSimultaneousRenderTargets
        );
        for (gp::UInt32 index = 0u; index < numColorFormats; ++index)
        {
            const gp::UInt32 attachment = renderTargets.colorAttachments[index].attachment;
            colorFormats[index] = (attachment < renderTargets.descriptions.size())
                                      ? static_cast<VkFormat>(renderTargets.descriptions[attachment].format)
                                      : VK_FORMAT_UNDEFINED;
        }

        renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
        renderingInfo.colorAttachmentCount = numColorFormats;
        renderingInfo.pColorAttachmentFormats = colorFormats.data();
        if (renderTargets.hasDepthStencil && (renderTargets.depth.attachment < renderTargets.descriptions.size()))
        {
            const VkFormat depthFormat =
                static_cast<VkFormat>(renderTargets.descriptions[renderTargets.depth.attachment].format);
            renderingInfo.depthAttachmentFormat = depthFormat;
            renderingInfo.stencilAttachmentFormat = vkFormatHasStencil(depthFormat) ? depthFormat : VK_FORMAT_UNDEFINED;
        }
        pipelineInfo.pNext = &renderingInfo;

        // Subpass fetch with dynamic rendering: input attachment index N reads the attachment of type N.
        if (state.m_hasInputAttachments)
        {
            for (gp::UInt32 index = 0u; index < numColorFormats; ++index)
            {
                colorInputIndices[index] = static_cast<gp::UInt32>(VulkanShaderAttachmentType::Color0) + index;
            }
            inputAttachmentIndexInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INPUT_ATTACHMENT_INDEX_INFO_KHR;
            inputAttachmentIndexInfo.colorAttachmentCount = numColorFormats;
            inputAttachmentIndexInfo.pColorAttachmentInputIndices = colorInputIndices.data();
            inputAttachmentIndexInfo.pDepthInputAttachmentIndex = &depthInputIndex;
            inputAttachmentIndexInfo.pStencilInputAttachmentIndex = &depthInputIndex;
            renderingInfo.pNext = &inputAttachmentIndexInfo;
        }
    }
    else
    {
        if (state.m_renderPass == nullptr)
        {
            return false;
        }
        pipelineInfo.renderPass = state.m_renderPass->getHandle();
    }

    // 8. Compile.
    VkPipeline pipelineHandle = VK_NULL_HANDLE;
    VkResult result = VK_SUCCESS;
    gp::UInt32 pipelineSize = 0u;
    const bool canMeasure = (dynamic_api::vkCreatePipelineCache != nullptr) &&
                            (dynamic_api::vkGetPipelineCacheData != nullptr) &&
                            (dynamic_api::vkDestroyPipelineCache != nullptr);
    if (m_lruSettings.isEnabled && canMeasure)
    {
        // A private cache isolates this pipeline's blob, whose size is the LRU charge.
        VkPipelineCacheCreateInfo cacheInfo{};
        cacheInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
        VkPipelineCache localCache = VK_NULL_HANDLE;
        result = vkCreatePipelineCache(m_device.getHandle(), &cacheInfo, GP_VULKAN_CPU_ALLOCATOR, &localCache);
        if (result == VK_SUCCESS)
        {
            result = vkCreateGraphicsPipelines(
                m_device.getHandle(), localCache, 1u, &pipelineInfo, GP_VULKAN_CPU_ALLOCATOR, &pipelineHandle
            );
            gp::USize dataSize = 0u;
            if ((result == VK_SUCCESS) &&
                (vkGetPipelineCacheData(m_device.getHandle(), localCache, &dataSize, nullptr) == VK_SUCCESS))
            {
                pipelineSize = static_cast<gp::UInt32>(dataSize);
            }
            if ((result == VK_SUCCESS) && (m_globalPipelineCache != VK_NULL_HANDLE) &&
                (dynamic_api::vkMergePipelineCaches != nullptr))
            {
                ExclusiveLock lock(m_globalPipelineCacheMutex);
                static_cast<void>(vkMergePipelineCaches(m_device.getHandle(), m_globalPipelineCache, 1u, &localCache));
            }
            vkDestroyPipelineCache(m_device.getHandle(), localCache, GP_VULKAN_CPU_ALLOCATOR);
        }
        if (pipelineSize == 0u)
        {
            pipelineSize = kDefaultPipelineCacheSize;
        }
    }
    else
    {
        ReaderLock lock(m_globalPipelineCacheMutex);
        result = vkCreateGraphicsPipelines(
            m_device.getHandle(), m_globalPipelineCache, 1u, &pipelineInfo, GP_VULKAN_CPU_ALLOCATOR, &pipelineHandle
        );
    }

    if (result != VK_SUCCESS)
    {
        GP_LOG_ERROR(LogVulkanRHI, "vkCreateGraphicsPipelines failed with {}.", vulkanResultToString(result));
        return false;
    }
    state.m_pipeline = pipelineHandle;
    state.m_pipelineSize = pipelineSize;
    return true;
}

bool VulkanPipelineStateCacheManager::createComputeVkPipeline(VulkanComputePipelineState& state) noexcept
{
    VulkanShader* const shader = state.m_vulkanShader;

    // The layout comes from the compute shader's header alone.
    VulkanUniformBufferGatherInfo gatherInfo{};
    gatherInfo.codeHeaders[0] = &shader->getCodeHeader();
    VulkanDescriptorSetsLayoutInfo layoutInfo{};
    if (!layoutInfo.finalizeBindings(VK_PIPELINE_BIND_POINT_COMPUTE, m_device.getLimits(), gatherInfo, false))
    {
        return false;
    }
    state.m_layout = findOrAddLayout(layoutInfo, false);
    if ((state.m_layout == nullptr) || (m_device.getHandle() == VK_NULL_HANDLE) ||
        (dynamic_api::vkCreateComputePipelines == nullptr))
    {
        return false;
    }

    const gp::RefCountedPtr<VulkanShaderModule> module =
        shader->getOrCreateHandle(state.m_layout->getDescriptorSetLayoutHash());
    if (!module.isValid() || !module->isValid())
    {
        GP_LOG_ERROR(LogVulkanRHI, "Missing shader module for compute pipeline.");
        return false;
    }

    VkSpecializationInfo specializationInfo{};
    VkPipelineShaderStageRequiredSubgroupSizeCreateInfoEXT subgroupInfo{};
    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineInfo.stage.module = module->getHandle();
    pipelineInfo.stage.pName = shader->getEntryPoint();
    applySpecializationConstants(state.m_specializationConstants, specializationInfo, pipelineInfo.stage);
    applyRequiredSubgroupSize(
        m_device, shader->getCodeHeader(), VK_SHADER_STAGE_COMPUTE_BIT, subgroupInfo, pipelineInfo.stage
    );
    pipelineInfo.layout = state.m_layout->getPipelineLayout();
    pipelineInfo.basePipelineIndex = -1;

    VkResult result = VK_SUCCESS;
    {
        ReaderLock lock(m_globalPipelineCacheMutex);
        result = vkCreateComputePipelines(
            m_device.getHandle(), m_globalPipelineCache, 1u, &pipelineInfo, GP_VULKAN_CPU_ALLOCATOR, &state.m_pipeline
        );
    }
    if (result != VK_SUCCESS)
    {
        GP_LOG_ERROR(LogVulkanRHI, "vkCreateComputePipelines failed with {}.", vulkanResultToString(result));
        state.m_pipeline = VK_NULL_HANDLE;
        return false;
    }
    return true;
}

pipeline::GraphicsPipelineStateRef
    VulkanPipelineStateCacheManager::makeGraphicsWrapper(VulkanGraphicsPipelineState* const state) noexcept
{
    return pipeline::GraphicsPipelineStateRef(new VulkanWrappedGraphicsPipelineState(state), gp::kRetainRef);
}

pipeline::ComputePipelineStateRef
    VulkanPipelineStateCacheManager::makeComputeWrapper(VulkanComputePipelineState* const state) noexcept
{
    return pipeline::ComputePipelineStateRef(new VulkanWrappedComputePipelineState(state), gp::kRetainRef);
}

void VulkanPipelineStateCacheManager::lruAdd(VulkanGraphicsPipelineState& state) noexcept
{
    if (state.m_isInLRU)
    {
        return;
    }
    state.m_lruFrame = getFrameNumber();
    state.m_lruPrev = nullptr;
    state.m_lruNext = m_lruHead;
    if (m_lruHead != nullptr)
    {
        m_lruHead->m_lruPrev = &state;
    }
    m_lruHead = &state;
    if (m_lruTail == nullptr)
    {
        m_lruTail = &state;
    }
    state.m_isInLRU = true;
    ++m_lruCount;
    m_lruSizeInBytes += state.m_pipelineSize;
}

void VulkanPipelineStateCacheManager::lruRemove(VulkanGraphicsPipelineState& state) noexcept
{
    if (!state.m_isInLRU)
    {
        return;
    }

    // Unused for a few frames: no command buffer in flight can reference it anymore.
    const bool isImmediate = (state.m_lruFrame + kPipelineImmediateDeleteFrameDelay) < getFrameNumber();

    if (state.m_lruPrev != nullptr)
    {
        state.m_lruPrev->m_lruNext = state.m_lruNext;
    }
    else
    {
        m_lruHead = state.m_lruNext;
    }
    if (state.m_lruNext != nullptr)
    {
        state.m_lruNext->m_lruPrev = state.m_lruPrev;
    }
    else
    {
        m_lruTail = state.m_lruPrev;
    }
    state.m_lruPrev = nullptr;
    state.m_lruNext = nullptr;
    state.m_isInLRU = false;
    --m_lruCount;
    m_lruSizeInBytes -= state.m_pipelineSize;

    state.destroyPipeline(isImmediate);
    if (m_lruSettings.releaseShaderModulesOnEvict)
    {
        for (VulkanShader* const shader: state.m_shaders)
        {
            if (shader != nullptr)
            {
                shader->purgeShaderModules();
            }
        }
    }
}

bool VulkanPipelineStateCacheManager::lruEvictOne() noexcept
{
    if (m_lruTail == nullptr)
    {
        return false;
    }
    lruRemove(*m_lruTail);
    return true;
}

void VulkanPipelineStateCacheManager::lruTrim(const gp::UInt32 neededBytes) noexcept
{
    while (((m_lruSizeInBytes + neededBytes) > m_lruSettings.maxSizeInBytes) || (m_lruCount >= m_lruSettings.capacity))
    {
        if (!lruEvictOne())
        {
            break;
        }
    }
}

void VulkanPipelineStateCacheManager::lruTouch(VulkanGraphicsPipelineState& state) noexcept
{
    if (!m_lruSettings.isEnabled)
    {
        return;
    }

    const gp::UInt64 frameNumber = getFrameNumber();
    if (state.m_isInLRU)
    {
        if (state.m_lruFrame == frameNumber)
        {
            return;
        }
        // Move to the most recently used end.
        if (m_lruHead != &state)
        {
            state.m_lruPrev->m_lruNext = state.m_lruNext;
            if (state.m_lruNext != nullptr)
            {
                state.m_lruNext->m_lruPrev = state.m_lruPrev;
            }
            else
            {
                m_lruTail = state.m_lruPrev;
            }
            state.m_lruPrev = nullptr;
            state.m_lruNext = m_lruHead;
            m_lruHead->m_lruPrev = &state;
            m_lruHead = &state;
        }
        state.m_lruFrame = frameNumber;
        return;
    }

    // Evicted: rebuild from the kept description. This is the hitch the LRU trades for memory.
    if ((state.m_pipeline == VK_NULL_HANDLE) && createGraphicsVkPipeline(state))
    {
        lruTrim(state.m_pipelineSize);
        lruAdd(state);
    }
}

}   // namespace gp::rhi::vulkan
