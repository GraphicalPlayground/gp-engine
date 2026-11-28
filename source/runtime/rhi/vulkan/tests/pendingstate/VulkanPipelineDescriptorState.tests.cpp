// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "../resources/VulkanResourceTestFakes.hpp"
#include "containers/arrays/Vector.hpp"
#include "core/ResourceDeletionQueue.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "descriptors/VulkanDescriptorPoolsManager.hpp"
#include "descriptors/VulkanDescriptorSetCache.hpp"
#include "descriptors/VulkanDescriptorSetLayoutCache.hpp"
#include "descriptors/VulkanDescriptorSetsEnums.hpp"
#include "descriptors/VulkanDescriptorSetsLayout.hpp"
#include "descriptors/VulkanDescriptorSetsTypes.hpp"
#include "device/VulkanDevice.hpp"
#include "memory/Memory.hpp"
#include "memory/pointers/RefCountedPtr.hpp"   // IWYU pragma: keep
#include "memory/pointers/UniquePtr.hpp"
#include "pendingstate/VulkanComputePipelineDescriptorState.hpp"
#include "pendingstate/VulkanGraphicsPipelineDescriptorState.hpp"
#include "pendingstate/VulkanPendingStateTypes.hpp"
#include "pipeline/SamplerState.hpp"   // IWYU pragma: keep
#include "shaders/VulkanShaderHeader.hpp"
#include "shaders/VulkanShaderHeaderEnums.hpp"
#include "states/VulkanSamplerState.hpp"
#include "views/VulkanViewTypes.hpp"
#include "VulkanCommon.hpp"
#include "VulkanPendingStateTestFakes.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

namespace
{

/// @brief Creates a sampler state with default parameters.
/// @param[in] device Device; may have no native handle.
/// @return Sampler reference.
[[nodiscard]] pipeline::SamplerStateRef makeSampler(VulkanDevice& device)
{
    VkSamplerCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    createInfo.maxLod = 1.0f;
    return pipeline::SamplerStateRef(new VulkanSamplerState(device, createInfo, nullptr));
}

/// @brief Returns the Vulkan sampler behind a reference.
/// @param[in] sampler Sampler reference.
/// @return Sampler state.
[[nodiscard]] const VulkanSamplerState& asVulkan(const pipeline::SamplerStateRef& sampler) noexcept
{
    return *static_cast<const VulkanSamplerState*>(sampler.get());
}

/// @brief Offline fixture: descriptor states over uncompiled layouts on a device without native handles.
/// @details Offline, pool allocations succeed without producing sets and no descriptor is written to a driver, so
///          every test observes the CPU side only: write infos, dirty tracking, uploads and what the context was asked.
class VulkanPipelineDescriptorStateTest : public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        m_manager.init(m_device);
        m_defaultSampler = makeSampler(m_device);
        m_otherSampler = makeSampler(m_device);
        m_defaultImageView.viewId = 1u;
        m_defaults.defaultSampler = &asVulkan(m_defaultSampler);
        m_defaults.defaultImageView = &m_defaultImageView;
    }

    virtual void TearDown() override
    {
        m_defaultSampler = pipeline::SamplerStateRef();
        m_otherSampler = pipeline::SamplerStateRef();
        resource::ResourceDeletionQueue::flushPendingDeletes(true);
    }

    /// @brief Finalizes a graphics layout from per-stage headers.
    /// @param[in,out] layout Layout to finalize.
    /// @param[in] vertexHeader Vertex header, or null.
    /// @param[in] pixelHeader Pixel header, or null.
    /// @param[in] dynamicMode Dynamic uniform buffer promotion.
    /// @return Result of `finalizeBindings()`.
    [[nodiscard]] bool finalizeGraphics(
        VulkanDescriptorSetsLayout& layout,
        const VulkanShaderHeader* const vertexHeader,
        const VulkanShaderHeader* const pixelHeader,
        const VulkanDynamicUniformBufferMode dynamicMode = VulkanDynamicUniformBufferMode::None
    )
    {
        m_gatherInfo = VulkanUniformBufferGatherInfo{};
        m_gatherInfo.codeHeaders[static_cast<gp::USize>(shader_stage::Stage::Vertex)] = vertexHeader;
        m_gatherInfo.codeHeaders[static_cast<gp::USize>(shader_stage::Stage::Pixel)] = pixelHeader;
        return layout.finalizeBindings(VK_PIPELINE_BIND_POINT_GRAPHICS, m_limits, m_gatherInfo, false, dynamicMode);
    }

    /// @brief Creates a graphics state over the last layout finalized by `finalizeGraphics()`.
    /// @param[in] layout Finalized layout; must outlive the state.
    /// @param[in] useCache True for the descriptor set cache path (resource ids are compared).
    /// @return State.
    [[nodiscard]] gp::UniquePtr<VulkanGraphicsPipelineDescriptorState>
        makeGraphicsState(const VulkanDescriptorSetsLayout& layout, const bool useCache)
    {
        return gp::makeUnique<VulkanGraphicsPipelineDescriptorState>(
            m_device, layout, VK_NULL_HANDLE, m_gatherInfo, m_defaults, useCache
        );
    }

    /// @brief Device without native handles.
    VulkanDevice m_device{ VK_NULL_HANDLE, VK_NULL_HANDLE, VK_API_VERSION_1_3, false };

    /// @brief Device limits.
    VkPhysicalDeviceLimits m_limits{ makePendingLimits() };

    /// @brief Pool manager behind the context; declared before it so it outlives it.
    VulkanDescriptorPoolsManager m_manager{};

    /// @brief Command context services.
    FakePendingStateContext m_context{ m_manager };

    /// @brief Default sampler of every sampler binding.
    pipeline::SamplerStateRef m_defaultSampler{};

    /// @brief Second sampler with a distinct id.
    pipeline::SamplerStateRef m_otherSampler{};

    /// @brief Default image view of every image binding.
    VulkanTextureView m_defaultImageView{};

    /// @brief Defaults handed to every state.
    VulkanDescriptorDefaults m_defaults{};

    /// @brief Headers of the last finalized graphics layout.
    VulkanUniformBufferGatherInfo m_gatherInfo{};
};

}   // namespace

TEST_F(VulkanPipelineDescriptorStateTest, UsedSetsMaskSkipsStagesWithoutBindings)
{
    const VulkanShaderHeader pixel = makePendingHeader({ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE });
    VulkanDescriptorSetsLayout layout(m_device);
    ASSERT_TRUE(finalizeGraphics(layout, nullptr, &pixel));

    const gp::UniquePtr<VulkanGraphicsPipelineDescriptorState> state = makeGraphicsState(layout, false);

    EXPECT_EQ(state->getBindPoint(), VK_PIPELINE_BIND_POINT_GRAPHICS);
    EXPECT_EQ(state->getNumSets(), 2u);
    EXPECT_EQ(state->getUsedSetsMask(), 0b10u);
    EXPECT_EQ(state->getDescriptorSetHandles().size(), 2u);
    EXPECT_TRUE(state->isResourcesDirty());
    EXPECT_FALSE(state->usesDescriptorCache());
    EXPECT_EQ(state->getPipeline(), nullptr);
}

TEST_F(VulkanPipelineDescriptorStateTest, StateWithoutBindingsHasNothingToUpdate)
{
    const VulkanShaderHeader vertex = makePendingHeader({});
    VulkanDescriptorSetsLayout layout(m_device);
    ASSERT_TRUE(finalizeGraphics(layout, &vertex, nullptr));
    const gp::UniquePtr<VulkanGraphicsPipelineDescriptorState> state = makeGraphicsState(layout, false);

    EXPECT_EQ(state->getUsedSetsMask(), 0u);
    EXPECT_FALSE(state->updateDescriptorSets(m_context));
    EXPECT_EQ(m_context.numAcquireCalls, 0u);
    EXPECT_EQ(m_context.numUniformAllocations, 0u);
}

TEST_F(VulkanPipelineDescriptorStateTest, FirstUpdateAcquiresAndClearsTheDirtyFlag)
{
    const VulkanShaderHeader vertex = makePendingHeader({ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER }, 1u);
    VulkanDescriptorSetsLayout layout(m_device);
    ASSERT_TRUE(finalizeGraphics(layout, &vertex, nullptr));
    const gp::UniquePtr<VulkanGraphicsPipelineDescriptorState> state = makeGraphicsState(layout, false);
    const VkBuffer buffer = makeFakePendingHandle<VkBuffer>(0xBEEFu);

    EXPECT_TRUE(state->setUniformBuffer(shader_stage::Stage::Vertex, 0u, buffer, 5u, 64u, 256u));
    ASSERT_TRUE(state->updateDescriptorSets(m_context));

    EXPECT_EQ(m_context.numAcquireCalls, 1u);
    EXPECT_TRUE(m_context.lastNeedDescriptors);
    EXPECT_FALSE(state->isResourcesDirty());
    const VkDescriptorBufferInfo& info = state->getWriteContainer().descriptorBufferInfo[0];
    EXPECT_EQ(info.buffer, buffer);
    EXPECT_EQ(info.offset, 64u);
    EXPECT_EQ(info.range, 256u);
}

TEST_F(VulkanPipelineDescriptorStateTest, UnchangedResourcesKeepTheirSets)
{
    const VulkanShaderHeader vertex = makePendingHeader({ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER }, 1u);
    VulkanDescriptorSetsLayout layout(m_device);
    ASSERT_TRUE(finalizeGraphics(layout, &vertex, nullptr));
    const gp::UniquePtr<VulkanGraphicsPipelineDescriptorState> state = makeGraphicsState(layout, false);
    const VkBuffer buffer = makeFakePendingHandle<VkBuffer>(0xBEEFu);
    ASSERT_TRUE(state->setUniformBuffer(shader_stage::Stage::Vertex, 0u, buffer, 5u, 64u, 256u));
    ASSERT_TRUE(state->updateDescriptorSets(m_context));

    EXPECT_FALSE(state->setUniformBuffer(shader_stage::Stage::Vertex, 0u, buffer, 5u, 64u, 256u));
    EXPECT_FALSE(state->isResourcesDirty());

    // The context is still asked, so a new frame can force new sets, but without a write request.
    EXPECT_TRUE(state->updateDescriptorSets(m_context));
    EXPECT_EQ(m_context.numAcquireCalls, 2u);
    EXPECT_FALSE(m_context.lastNeedDescriptors);
}

TEST_F(VulkanPipelineDescriptorStateTest, ChangedResourceMarksTheStateDirty)
{
    const VulkanShaderHeader vertex = makePendingHeader({ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER }, 1u);
    VulkanDescriptorSetsLayout layout(m_device);
    ASSERT_TRUE(finalizeGraphics(layout, &vertex, nullptr));
    const gp::UniquePtr<VulkanGraphicsPipelineDescriptorState> state = makeGraphicsState(layout, false);
    const VkBuffer buffer = makeFakePendingHandle<VkBuffer>(0xBEEFu);
    ASSERT_TRUE(state->setUniformBuffer(shader_stage::Stage::Vertex, 0u, buffer, 5u, 64u, 256u));
    ASSERT_TRUE(state->updateDescriptorSets(m_context));

    EXPECT_TRUE(state->setUniformBuffer(shader_stage::Stage::Vertex, 0u, buffer, 5u, 128u, 256u));
    EXPECT_TRUE(state->isResourcesDirty());
    ASSERT_TRUE(state->updateDescriptorSets(m_context));
    EXPECT_TRUE(m_context.lastNeedDescriptors);
}

TEST_F(VulkanPipelineDescriptorStateTest, DynamicUniformBufferOffsetTravelsWithTheBind)
{
    const VulkanShaderHeader vertex = makePendingHeader({ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER }, 1u);
    VulkanDescriptorSetsLayout layout(m_device);
    ASSERT_TRUE(finalizeGraphics(layout, &vertex, nullptr, VulkanDynamicUniformBufferMode::All));
    const gp::UniquePtr<VulkanGraphicsPipelineDescriptorState> state = makeGraphicsState(layout, false);
    const VkBuffer buffer = makeFakePendingHandle<VkBuffer>(0xBEEFu);
    ASSERT_EQ(state->getDynamicOffsets().size(), 1u);

    EXPECT_TRUE(state->setUniformBuffer(shader_stage::Stage::Vertex, 0u, buffer, 5u, 128u, 256u));
    EXPECT_EQ(state->getDynamicOffsets()[0], 128u);
    EXPECT_EQ(state->getWriteContainer().descriptorBufferInfo[0].offset, 0u);
    ASSERT_TRUE(state->updateDescriptorSets(m_context));

    // A new offset alone needs no descriptor rewrite.
    EXPECT_FALSE(state->setUniformBuffer(shader_stage::Stage::Vertex, 0u, buffer, 5u, 512u, 256u));
    EXPECT_EQ(state->getDynamicOffsets()[0], 512u);
    EXPECT_FALSE(state->isResourcesDirty());
}

TEST_F(VulkanPipelineDescriptorStateTest, PackedGlobalsAreUploadedAtFlush)
{
    const VulkanShaderHeader vertex = makePendingHeader({ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER }, 0u, 16u);
    VulkanDescriptorSetsLayout layout(m_device);
    ASSERT_TRUE(finalizeGraphics(layout, &vertex, nullptr));
    const gp::UniquePtr<VulkanGraphicsPipelineDescriptorState> state = makeGraphicsState(layout, false);
    ASSERT_TRUE(state->getPackedUniformBuffer(shader_stage::Stage::Vertex).hasData());
    const gp::UInt32 value = 0xCAFEu;

    state->setPackedGlobalParameter(shader_stage::Stage::Vertex, 4u, sizeof(value), &value);
    ASSERT_TRUE(state->updateDescriptorSets(m_context));

    EXPECT_EQ(m_context.numUniformAllocations, 1u);
    EXPECT_EQ(m_context.lastUniformSize, 16u);
    EXPECT_FALSE(state->getPackedUniformBuffer(shader_stage::Stage::Vertex).isDirty());
    const VkDescriptorBufferInfo& info = state->getWriteContainer().descriptorBufferInfo[0];
    EXPECT_EQ(info.buffer, m_context.uniformBuffer);
    EXPECT_EQ(info.offset, static_cast<VkDeviceSize>(FakePendingStateContext::kFirstUniformOffset));
    EXPECT_EQ(info.range, 16u);
    gp::UInt32 uploaded = 0u;
    gp::memory::copyMemory(
        &uploaded, m_context.getUniformBytes(FakePendingStateContext::kFirstUniformOffset) + 4u, sizeof(uploaded)
    );
    EXPECT_EQ(uploaded, value);

    // Nothing changed: no second upload.
    ASSERT_TRUE(state->updateDescriptorSets(m_context));
    EXPECT_EQ(m_context.numUniformAllocations, 1u);
}

TEST_F(VulkanPipelineDescriptorStateTest, DynamicPackedGlobalsUseTheUploadOffsetAsDynamicOffset)
{
    const VulkanShaderHeader vertex = makePendingHeader({ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER }, 0u, 16u);
    VulkanDescriptorSetsLayout layout(m_device);
    ASSERT_TRUE(finalizeGraphics(layout, &vertex, nullptr, VulkanDynamicUniformBufferMode::PackedGlobals));
    const gp::UniquePtr<VulkanGraphicsPipelineDescriptorState> state = makeGraphicsState(layout, false);
    ASSERT_EQ(state->getDynamicOffsets().size(), 1u);

    ASSERT_TRUE(state->updateDescriptorSets(m_context));

    EXPECT_EQ(state->getDynamicOffsets()[0], FakePendingStateContext::kFirstUniformOffset);
    EXPECT_EQ(state->getWriteContainer().descriptorBufferInfo[0].offset, 0u);
}

TEST_F(VulkanPipelineDescriptorStateTest, UniformAllocationFailureFailsTheUpdate)
{
    const VulkanShaderHeader vertex = makePendingHeader({ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER }, 0u, 16u);
    VulkanDescriptorSetsLayout layout(m_device);
    ASSERT_TRUE(finalizeGraphics(layout, &vertex, nullptr));
    const gp::UniquePtr<VulkanGraphicsPipelineDescriptorState> state = makeGraphicsState(layout, false);
    m_context.shouldFailUniformAllocations = true;

    EXPECT_FALSE(state->updateDescriptorSets(m_context));

    EXPECT_EQ(m_context.numAcquireCalls, 0u);
    EXPECT_TRUE(state->getPackedUniformBuffer(shader_stage::Stage::Vertex).isDirty());
}

TEST_F(VulkanPipelineDescriptorStateTest, ResetUploadsThePackedGlobalsAgain)
{
    const VulkanShaderHeader vertex = makePendingHeader({ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER }, 0u, 16u);
    VulkanDescriptorSetsLayout layout(m_device);
    ASSERT_TRUE(finalizeGraphics(layout, &vertex, nullptr));
    const gp::UniquePtr<VulkanGraphicsPipelineDescriptorState> state = makeGraphicsState(layout, false);
    ASSERT_TRUE(state->updateDescriptorSets(m_context));

    state->reset();

    EXPECT_TRUE(state->isResourcesDirty());
    EXPECT_TRUE(state->getPackedUniformBuffer(shader_stage::Stage::Vertex).isDirty());
    ASSERT_TRUE(state->updateDescriptorSets(m_context));
    EXPECT_EQ(m_context.numUniformAllocations, 2u);
    EXPECT_EQ(
        state->getWriteContainer().descriptorBufferInfo[0].offset,
        static_cast<VkDeviceSize>(
            FakePendingStateContext::kFirstUniformOffset + FakePendingStateContext::kUniformAlignment
        )
    );
}

TEST_F(VulkanPipelineDescriptorStateTest, SamplersAreTrackedById)
{
    const VulkanShaderHeader pixel = makePendingHeader({ VK_DESCRIPTOR_TYPE_SAMPLER });
    VulkanDescriptorSetsLayout layout(m_device);
    ASSERT_TRUE(finalizeGraphics(layout, nullptr, &pixel));
    // The cache path compares ids, so offline samplers (all null handles) still differ. No cache is provided, so
    // the flush itself stays on the pool path.
    const gp::UniquePtr<VulkanGraphicsPipelineDescriptorState> state = makeGraphicsState(layout, true);
    ASSERT_TRUE(state->updateDescriptorSets(m_context));

    EXPECT_TRUE(state->setSampler(shader_stage::Stage::Pixel, 0u, asVulkan(m_otherSampler)));
    EXPECT_TRUE(state->isResourcesDirty());
    EXPECT_FALSE(state->setSampler(shader_stage::Stage::Pixel, 0u, asVulkan(m_otherSampler)));
    // Entry 0 is the layout entry of the empty vertex set; the pixel binding follows it.
    EXPECT_EQ(
        state->getWriteContainer().hashableDescriptorInfo[1].image.samplerId, asVulkan(m_otherSampler).getSamplerId()
    );
}

TEST_F(VulkanPipelineDescriptorStateTest, CacheKeyEndsEachSetWithItsLayoutIdentity)
{
    const VulkanShaderHeader pixel = makePendingHeader({ VK_DESCRIPTOR_TYPE_SAMPLER });
    VulkanDescriptorSetsLayout layout(m_device);
    ASSERT_TRUE(finalizeGraphics(layout, nullptr, &pixel));

    const gp::UniquePtr<VulkanGraphicsPipelineDescriptorState> state = makeGraphicsState(layout, true);

    // Set 0 has no binding: only its layout entry. Set 1 has one binding then its layout entry.
    const gp::Vector<VulkanHashableDescriptorInfo>& infos = state->getWriteContainer().hashableDescriptorInfo;
    ASSERT_EQ(infos.size(), 3u);
    EXPECT_EQ(infos[0].layout.max0, ~0u);
    EXPECT_EQ(infos[0].layout.layoutId, 0u);
    EXPECT_EQ(infos[1].image.samplerId, asVulkan(m_defaultSampler).getSamplerId());
    EXPECT_EQ(infos[2].layout.max1, ~0u);
}

TEST_F(VulkanPipelineDescriptorStateTest, GlobalSamplersAreWrittenAtConstruction)
{
    VulkanShaderHeader pixel = makePendingHeader({ VK_DESCRIPTOR_TYPE_SAMPLER, VK_DESCRIPTOR_TYPE_SAMPLER });
    VulkanShaderGlobalSamplerInfo samplerInfo{};
    samplerInfo.bindingIndex = 1u;
    samplerInfo.type = VulkanGlobalSamplerType::PointClampedSampler;
    pixel.globalSamplerInfos.pushBack(samplerInfo);
    m_defaults.globalSamplers[static_cast<gp::USize>(VulkanGlobalSamplerType::PointClampedSampler)] =
        &asVulkan(m_otherSampler);
    VulkanDescriptorSetsLayout layout(m_device);
    ASSERT_TRUE(finalizeGraphics(layout, nullptr, &pixel));

    const gp::UniquePtr<VulkanGraphicsPipelineDescriptorState> state = makeGraphicsState(layout, true);

    // Hashable layout: [set 0 layout][set 1 binding 0][set 1 binding 1][set 1 layout].
    const gp::Vector<VulkanHashableDescriptorInfo>& infos = state->getWriteContainer().hashableDescriptorInfo;
    EXPECT_EQ(infos[1].image.samplerId, asVulkan(m_defaultSampler).getSamplerId());
    EXPECT_EQ(infos[2].image.samplerId, asVulkan(m_otherSampler).getSamplerId());
}

TEST_F(VulkanPipelineDescriptorStateTest, TextureViewsAreTrackedPerBinding)
{
    const VulkanShaderHeader pixel = makePendingHeader({ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE });
    VulkanDescriptorSetsLayout layout(m_device);
    ASSERT_TRUE(finalizeGraphics(layout, nullptr, &pixel));
    const gp::UniquePtr<VulkanGraphicsPipelineDescriptorState> state = makeGraphicsState(layout, false);
    VulkanTextureView view{};
    view.view = makeFakePendingHandle<VkImageView>(0x1A0u);
    view.viewId = 5u;

    EXPECT_TRUE(state->setTextureView(shader_stage::Stage::Pixel, 0u, view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
    EXPECT_FALSE(state->setTextureView(shader_stage::Stage::Pixel, 0u, view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));

    const VkDescriptorImageInfo& info = state->getWriteContainer().descriptorImageInfo[0];
    EXPECT_EQ(info.imageView, view.view);
    EXPECT_EQ(info.imageLayout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

TEST_F(VulkanPipelineDescriptorStateTest, InputAttachmentsComeFromTheContext)
{
    VulkanShaderHeader pixel = makePendingHeader({ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT });
    VulkanShaderInputAttachmentInfo attachmentInfo{};
    attachmentInfo.bindingIndex = 0u;
    attachmentInfo.type = VulkanShaderAttachmentType::Color0;
    pixel.inputAttachmentInfos.pushBack(attachmentInfo);
    VulkanDescriptorSetsLayout layout(m_device);
    ASSERT_TRUE(finalizeGraphics(layout, nullptr, &pixel));
    const gp::UniquePtr<VulkanGraphicsPipelineDescriptorState> state = makeGraphicsState(layout, false);
    ASSERT_TRUE(state->hasInputAttachments());

    // Outside a render pass the binding keeps its default view.
    EXPECT_FALSE(state->updateInputAttachments(m_context));
    EXPECT_EQ(m_context.lastAttachmentType, VulkanShaderAttachmentType::Color0);

    m_context.hasInputAttachment = true;
    m_context.inputAttachmentView.view = makeFakePendingHandle<VkImageView>(0x1B0u);
    m_context.inputAttachmentView.viewId = 9u;
    m_context.inputAttachmentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    EXPECT_TRUE(state->updateInputAttachments(m_context));
    EXPECT_EQ(state->getWriteContainer().descriptorImageInfo[0].imageView, m_context.inputAttachmentView.view);
    EXPECT_FALSE(state->updateInputAttachments(m_context));
}

TEST_F(VulkanPipelineDescriptorStateTest, LayoutWithoutInputAttachmentsSkipsTheQuery)
{
    const VulkanShaderHeader pixel = makePendingHeader({ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE });
    VulkanDescriptorSetsLayout layout(m_device);
    ASSERT_TRUE(finalizeGraphics(layout, nullptr, &pixel));
    const gp::UniquePtr<VulkanGraphicsPipelineDescriptorState> state = makeGraphicsState(layout, false);
    m_context.hasInputAttachment = true;

    EXPECT_FALSE(state->hasInputAttachments());
    EXPECT_FALSE(state->updateInputAttachments(m_context));
    EXPECT_EQ(m_context.lastAttachmentType, VulkanShaderAttachmentType::Count);
}

TEST_F(VulkanPipelineDescriptorStateTest, CachePathFailsWithoutANativePool)
{
    const VulkanShaderHeader pixel = makePendingHeader({ VK_DESCRIPTOR_TYPE_SAMPLER });
    VulkanDescriptorSetsLayout layout(m_device);
    ASSERT_TRUE(finalizeGraphics(layout, nullptr, &pixel));
    const gp::UniquePtr<VulkanGraphicsPipelineDescriptorState> state = makeGraphicsState(layout, true);
    VulkanDescriptorSetCache cache(m_device);
    m_context.descriptorSetCache = &cache;

    EXPECT_FALSE(state->updateDescriptorSets(m_context));

    EXPECT_EQ(m_context.numAcquireCalls, 0u);
    EXPECT_TRUE(state->isResourcesDirty());
}

TEST_F(VulkanPipelineDescriptorStateTest, BindingWithoutACommandBufferIsANoOp)
{
    const VulkanShaderHeader pixel = makePendingHeader({ VK_DESCRIPTOR_TYPE_SAMPLER });
    VulkanDescriptorSetsLayout layout(m_device);
    ASSERT_TRUE(finalizeGraphics(layout, nullptr, &pixel));
    const gp::UniquePtr<VulkanGraphicsPipelineDescriptorState> state = makeGraphicsState(layout, false);
    ASSERT_TRUE(state->updateDescriptorSets(m_context));

    state->bindDescriptorSets(VK_NULL_HANDLE);

    EXPECT_FALSE(state->isResourcesDirty());
}

TEST_F(VulkanPipelineDescriptorStateTest, ComputeStateUsesTheComputeSet)
{
    const VulkanShaderHeader compute = makePendingHeader({ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE });
    VulkanUniformBufferGatherInfo gatherInfo{};
    gatherInfo.codeHeaders[static_cast<gp::USize>(shader_stage::Stage::Compute)] = &compute;
    VulkanDescriptorSetsLayout layout(m_device);
    ASSERT_TRUE(layout.finalizeBindings(
        VK_PIPELINE_BIND_POINT_COMPUTE, m_limits, gatherInfo, false, VulkanDynamicUniformBufferMode::None
    ));

    VulkanComputePipelineDescriptorState state(m_device, layout, VK_NULL_HANDLE, compute, m_defaults, false);

    EXPECT_EQ(state.getBindPoint(), VK_PIPELINE_BIND_POINT_COMPUTE);
    EXPECT_EQ(state.getNumSets(), 1u);
    EXPECT_EQ(state.getUsedSetsMask(), 1u);
    EXPECT_EQ(state.getPipeline(), nullptr);
    EXPECT_TRUE(state.updateDescriptorSets(m_context));
    EXPECT_EQ(m_context.numAcquireCalls, 1u);
}

namespace
{

/// @brief Live fixture: compiled compute layouts allocated on a real device.
class VulkanPipelineDescriptorStateLiveTest : public VulkanResourceLiveTest
{
protected:
    virtual void TearDown() override
    {
        m_otherSampler = pipeline::SamplerStateRef();
        m_defaultSampler = pipeline::SamplerStateRef();
        resource::ResourceDeletionQueue::flushPendingDeletes(true);
        VulkanResourceLiveTest::TearDown();
    }

    /// @brief Creates the samplers and defaults; call once the device exists.
    void createDefaults()
    {
        m_defaultSampler = makeSampler(*m_device);
        m_otherSampler = makeSampler(*m_device);
        m_defaultImageView.viewId = 1u;
        m_defaults.defaultSampler = &asVulkan(m_defaultSampler);
        m_defaults.defaultImageView = &m_defaultImageView;
    }

    /// @brief Finalizes and compiles a compute layout with one sampler binding.
    /// @param[in,out] layout Layout to build.
    /// @param[in,out] cache Set layout cache.
    /// @return True on success.
    [[nodiscard]] bool buildSamplerLayout(VulkanDescriptorSetsLayout& layout, VulkanDescriptorSetLayoutCache& cache)
    {
        VulkanUniformBufferGatherInfo gatherInfo{};
        gatherInfo.codeHeaders[static_cast<gp::USize>(shader_stage::Stage::Compute)] = &m_header;
        return layout.finalizeBindings(
                   VK_PIPELINE_BIND_POINT_COMPUTE,
                   m_device->getLimits(),
                   gatherInfo,
                   false,
                   VulkanDynamicUniformBufferMode::None
               ) &&
               layout.compile(cache);
    }

    /// @brief Compute header with one sampler.
    VulkanShaderHeader m_header{ makePendingHeader({ VK_DESCRIPTOR_TYPE_SAMPLER }) };

    /// @brief Default sampler.
    pipeline::SamplerStateRef m_defaultSampler{};

    /// @brief Second sampler.
    pipeline::SamplerStateRef m_otherSampler{};

    /// @brief Default image view (unused by sampler bindings).
    VulkanTextureView m_defaultImageView{};

    /// @brief Defaults handed to every state.
    VulkanDescriptorDefaults m_defaults{};
};

}   // namespace

TEST_F(VulkanPipelineDescriptorStateLiveTest, PoolPathAllocatesAndWritesRealSets)
{
    createDefaults();
    VulkanDescriptorSetLayoutCache layoutCache(*m_device);
    VulkanDescriptorSetsLayout layout(*m_device);
    ASSERT_TRUE(buildSamplerLayout(layout, layoutCache));
    VulkanDescriptorPoolsManager manager;
    manager.init(*m_device);
    {
        FakePendingStateContext context(manager);
        VulkanComputePipelineDescriptorState state(*m_device, layout, VK_NULL_HANDLE, m_header, m_defaults, false);

        ASSERT_TRUE(state.updateDescriptorSets(context));
        const VkDescriptorSet first = state.getDescriptorSetHandles()[0];
        EXPECT_NE(first, VK_NULL_HANDLE);

        // Unchanged: the same set is kept.
        ASSERT_TRUE(state.updateDescriptorSets(context));
        EXPECT_EQ(state.getDescriptorSetHandles()[0], first);
    }
}

TEST_F(VulkanPipelineDescriptorStateLiveTest, CachePathSharesSetsBetweenEqualStates)
{
    createDefaults();
    VulkanDescriptorSetLayoutCache layoutCache(*m_device);
    VulkanDescriptorSetsLayout layout(*m_device);
    ASSERT_TRUE(buildSamplerLayout(layout, layoutCache));
    VulkanDescriptorPoolsManager manager;
    manager.init(*m_device);
    {
        VulkanDescriptorSetCache cache(*m_device);
        FakePendingStateContext context(manager);
        context.descriptorSetCache = &cache;
        VulkanComputePipelineDescriptorState first(*m_device, layout, VK_NULL_HANDLE, m_header, m_defaults, true);
        VulkanComputePipelineDescriptorState second(*m_device, layout, VK_NULL_HANDLE, m_header, m_defaults, true);
        ASSERT_TRUE(first.setSampler(shader_stage::Stage::Compute, 0u, asVulkan(m_otherSampler)));
        ASSERT_TRUE(second.setSampler(shader_stage::Stage::Compute, 0u, asVulkan(m_otherSampler)));

        ASSERT_TRUE(first.updateDescriptorSets(context));
        ASSERT_TRUE(second.updateDescriptorSets(context));

        EXPECT_NE(first.getDescriptorSetHandles()[0], VK_NULL_HANDLE);
        EXPECT_EQ(first.getDescriptorSetHandles()[0], second.getDescriptorSetHandles()[0]);
        EXPECT_EQ(context.numAcquireCalls, 0u);
        EXPECT_EQ(cache.getCachedPool(0u).getNumSets(), 1u);
    }
}

}   // namespace gp::rhi::vulkan::tests
