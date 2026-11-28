// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "resources/VulkanUniformBuffer.hpp"
#include "../resources/VulkanResourceTestFakes.hpp"
#include "../resources/VulkanUniformBufferTestLayouts.hpp"
#include "core/ResourceDeletionQueue.hpp"
#include "core/ResourceForward.hpp"
#include "CoreMinimal.hpp"
#include "device/VulkanDevice.hpp"
#include "extensions/VulkanExtensionTypes.hpp"
#include "memory/pointers/RefCountedPtr.hpp"   // IWYU pragma: keep
#include "memory/VulkanTempBlockAllocator.hpp"
#include "memory/VulkanUniformBufferMemory.hpp"
#include "pipeline/PipelineForward.hpp"
#include "pipeline/SamplerState.hpp"
#include "resources/UniformBuffer.hpp"
#include "resources/UniformBufferEnums.hpp"
#include "resources/UniformBufferLayout.hpp"   // IWYU pragma: keep
#include "resources/VulkanResourceServices.hpp"
#include "VulkanThirdParty.hpp"                // IWYU pragma: keep
#include <cstring>
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

/// @brief Live fixture with an optional temp block allocator for volatile uniform buffers.
class VulkanUniformBufferLiveTest : public VulkanResourceLiveTest
{
protected:
    virtual void SetUp() override
    {
        VulkanResourceLiveTest::SetUp();
        if (IsSkipped() || HasFatalFailure())
        {
            return;
        }

        m_tempBlockAllocator = new VulkanTempBlockAllocator(
            *m_memoryManager,
            256u * 1024u,
            VulkanTempBlockAllocator::computeDefaultBlockAlignment(*m_device),
            VulkanTempBlockAllocator::computeDefaultBufferUsage(*m_device)
        );
        ASSERT_TRUE(m_tempBlockAllocator->isValid());

        m_contents.constants[0] = 1.5f;
        m_contents.constants[1] = -2.0f;
        m_contents.constants[2] = 8.25f;
        m_contents.constants[3] = 0.125f;
        m_contents.padding = 0xFEEDFACECAFEBEEFull;
    }

    virtual void TearDown() override
    {
        // Uniform buffers release their temp ranges on deletion, so flush them before the allocator goes away.
        resource::ResourceDeletionQueue::flushPendingDeletes(true);
        delete m_tempBlockAllocator;
        m_tempBlockAllocator = nullptr;
        VulkanResourceLiveTest::TearDown();
    }

    /// @brief Returns services that sub-allocate volatile uniform buffers from the temp allocator.
    /// @return Services copy.
    [[nodiscard]] VulkanResourceServices tempServices() const noexcept
    {
        VulkanResourceServices services = m_services;
        services.tempBlockAllocator = m_tempBlockAllocator;
        return services;
    }

    /// @brief Reports whether the mapped constants of `uniformBuffer` equal `expected`.
    [[nodiscard]] static bool hasConstants(
        const resource::UniformBufferRef& uniformBuffer, const TestUniformBufferContents& expected
    ) noexcept
    {
        const VulkanUniformBuffer* const vulkanBuffer = static_cast<const VulkanUniformBuffer*>(uniformBuffer.get());
        return (vulkanBuffer->getMappedConstants() != nullptr) &&
               (std::memcmp(vulkanBuffer->getMappedConstants(), &expected, sizeof(expected)) == 0);
    }

    VulkanTempBlockAllocator* m_tempBlockAllocator{ nullptr };
    TestUniformBufferContents m_contents{};
};

TEST_F(VulkanUniformBufferLiveTest, RejectsMissingLayoutsAndServices)
{
    const resource::UniformBufferLayoutRef layout = createTestUniformBufferLayout(true, false);

    EXPECT_FALSE(
        createVulkanUniformBuffer(m_services, &m_contents, nullptr, resource::UniformBufferUsage::MultiFrame).isValid()
    );
    EXPECT_FALSE(createVulkanUniformBuffer(
                     VulkanResourceServices{}, &m_contents, layout.get(), resource::UniformBufferUsage::MultiFrame
    )
                     .isValid());
}

TEST_F(VulkanUniformBufferLiveTest, MultiFrameBuffersUsePooledMemory)
{
    const resource::UniformBufferLayoutRef layout = createTestUniformBufferLayout(true, false);
    const resource::UniformBufferRef uniformBuffer =
        createVulkanUniformBuffer(tempServices(), &m_contents, layout.get(), resource::UniformBufferUsage::MultiFrame);
    ASSERT_TRUE(uniformBuffer.isValid());
    const VulkanUniformBuffer* const vulkanBuffer = static_cast<const VulkanUniformBuffer*>(uniformBuffer.get());

    EXPECT_EQ(vulkanBuffer->getUsage(), resource::UniformBufferUsage::MultiFrame);
    EXPECT_FALSE(vulkanBuffer->isTempBlockAllocation());
    EXPECT_NE(vulkanBuffer->getBufferHandle(), VK_NULL_HANDLE);
    EXPECT_EQ(vulkanBuffer->getOffset() % getVulkanUniformBufferAlignment(*m_device), 0u);
    EXPECT_EQ(uniformBuffer->getSize(), static_cast<gp::UInt32>(sizeof(TestUniformBufferContents)));
    EXPECT_TRUE(uniformBuffer->getResourceTable().isEmpty());
    EXPECT_FALSE(uniformBuffer->getBindlessHandle().isValid());
    EXPECT_TRUE(hasConstants(uniformBuffer, m_contents));

    // Persistent buffers never touch the temp allocator.
    EXPECT_EQ(m_tempBlockAllocator->getCurrentBlockReferences(), 0u);
}

TEST_F(VulkanUniformBufferLiveTest, VolatileBuffersSubAllocateFromTempBlocks)
{
    const resource::UniformBufferLayoutRef layout = createTestUniformBufferLayout(true, false);
    const resource::UniformBufferUsage volatileUsages[2] = { resource::UniformBufferUsage::SingleDraw,
                                                             resource::UniformBufferUsage::SingleFrame };

    for (const resource::UniformBufferUsage usage: volatileUsages)
    {
        resource::UniformBufferRef uniformBuffer =
            createVulkanUniformBuffer(tempServices(), &m_contents, layout.get(), usage);
        ASSERT_TRUE(uniformBuffer.isValid());
        const VulkanUniformBuffer* const vulkanBuffer = static_cast<const VulkanUniformBuffer*>(uniformBuffer.get());

        EXPECT_TRUE(vulkanBuffer->isTempBlockAllocation());
        EXPECT_NE(vulkanBuffer->getBufferHandle(), VK_NULL_HANDLE);
        EXPECT_EQ(vulkanBuffer->getOffset() % getVulkanUniformBufferAlignment(*m_device), 0u);
        EXPECT_EQ(m_tempBlockAllocator->getCurrentBlockReferences(), 1u);
        EXPECT_TRUE(hasConstants(uniformBuffer, m_contents));

        if (m_device->getOptionalExtensions().hasBufferDeviceAddress != 0u)
        {
            EXPECT_NE(vulkanBuffer->getDeviceAddress(), 0u);
        }

        // Deleting the buffer returns its temp range.
        uniformBuffer = nullptr;
        resource::ResourceDeletionQueue::flushPendingDeletes(true);
        EXPECT_EQ(m_tempBlockAllocator->getCurrentBlockReferences(), 0u);
    }
}

TEST_F(VulkanUniformBufferLiveTest, VolatileBuffersFallBackToPooledMemoryWithoutATempAllocator)
{
    const resource::UniformBufferLayoutRef layout = createTestUniformBufferLayout(true, false);
    const resource::UniformBufferRef uniformBuffer =
        createVulkanUniformBuffer(m_services, &m_contents, layout.get(), resource::UniformBufferUsage::SingleDraw);
    ASSERT_TRUE(uniformBuffer.isValid());
    const VulkanUniformBuffer* const vulkanBuffer = static_cast<const VulkanUniformBuffer*>(uniformBuffer.get());

    EXPECT_FALSE(vulkanBuffer->isTempBlockAllocation());
    EXPECT_NE(vulkanBuffer->getBufferHandle(), VK_NULL_HANDLE);
    EXPECT_TRUE(hasConstants(uniformBuffer, m_contents));
}

TEST_F(VulkanUniformBufferLiveTest, ResourceTableHoldsReferencesToBoundResources)
{
    const resource::UniformBufferLayoutRef layout = createTestUniformBufferLayout(true, true);
    const pipeline::SamplerStateRef sampler(new pipeline::SamplerState());
    m_contents.sampler = sampler.get();
    const gp::UInt32 samplerRefs = sampler->getRefCount();

    {
        const resource::UniformBufferRef uniformBuffer =
            createVulkanUniformBuffer(m_services, &m_contents, layout.get(), resource::UniformBufferUsage::MultiFrame);
        ASSERT_TRUE(uniformBuffer.isValid());
        ASSERT_EQ(uniformBuffer->getResourceTable().size(), 1u);
        EXPECT_EQ(uniformBuffer->getResourceTable()[0].get(), sampler.get());
        EXPECT_EQ(sampler->getRefCount(), samplerRefs + 1u);
        EXPECT_TRUE(hasConstants(uniformBuffer, m_contents));
    }
    resource::ResourceDeletionQueue::flushPendingDeletes(true);
    EXPECT_EQ(sampler->getRefCount(), samplerRefs);
}

TEST_F(VulkanUniformBufferLiveTest, ResourceOnlyLayoutsAllocateNoConstantMemory)
{
    const resource::UniformBufferLayoutRef layout = createTestUniformBufferLayout(false, true);
    const pipeline::SamplerStateRef sampler(new pipeline::SamplerState());
    m_contents.sampler = sampler.get();

    const resource::UniformBufferRef uniformBuffer =
        createVulkanUniformBuffer(tempServices(), &m_contents, layout.get(), resource::UniformBufferUsage::SingleDraw);
    ASSERT_TRUE(uniformBuffer.isValid());
    const VulkanUniformBuffer* const vulkanBuffer = static_cast<const VulkanUniformBuffer*>(uniformBuffer.get());

    EXPECT_EQ(uniformBuffer->getSize(), 0u);
    EXPECT_EQ(vulkanBuffer->getBufferHandle(), VK_NULL_HANDLE);
    EXPECT_EQ(vulkanBuffer->getMappedConstants(), nullptr);
    EXPECT_EQ(m_tempBlockAllocator->getCurrentBlockReferences(), 0u);
    ASSERT_EQ(uniformBuffer->getResourceTable().size(), 1u);
    EXPECT_EQ(uniformBuffer->getResourceTable()[0].get(), sampler.get());
}

TEST_F(VulkanUniformBufferLiveTest, NullContentsAllocateWithoutCopying)
{
    const resource::UniformBufferLayoutRef layout = createTestUniformBufferLayout(true, true);
    const resource::UniformBufferRef uniformBuffer =
        createVulkanUniformBuffer(m_services, nullptr, layout.get(), resource::UniformBufferUsage::MultiFrame);
    ASSERT_TRUE(uniformBuffer.isValid());
    const VulkanUniformBuffer* const vulkanBuffer = static_cast<const VulkanUniformBuffer*>(uniformBuffer.get());

    EXPECT_NE(vulkanBuffer->getMappedConstants(), nullptr);
    ASSERT_EQ(uniformBuffer->getResourceTable().size(), 1u);
    EXPECT_FALSE(uniformBuffer->getResourceTable()[0].isValid());
}

TEST_F(VulkanUniformBufferLiveTest, MissingResourcesAreToleratedWithoutValidation)
{
    const resource::UniformBufferLayoutRef layout = createTestUniformBufferLayout(true, true);
    m_contents.sampler = nullptr;

    const resource::UniformBufferUsage usage = resource::UniformBufferUsage::MultiFrame;
    const resource::UniformBufferRef validated =
        createVulkanUniformBuffer(m_services, &m_contents, layout.get(), usage);
    const resource::UniformBufferRef unvalidated = createVulkanUniformBuffer(
        m_services, &m_contents, layout.get(), usage, resource::UniformBufferValidation::None
    );
    ASSERT_TRUE(validated.isValid());
    ASSERT_TRUE(unvalidated.isValid());
    EXPECT_FALSE(validated->getResourceTable()[0].isValid());
    EXPECT_FALSE(unvalidated->getResourceTable()[0].isValid());
}

TEST_F(VulkanUniformBufferLiveTest, UpdateReplacesConstantsAndResources)
{
    const resource::UniformBufferLayoutRef layout = createTestUniformBufferLayout(true, true);
    const pipeline::SamplerStateRef firstSampler(new pipeline::SamplerState());
    const pipeline::SamplerStateRef secondSampler(new pipeline::SamplerState());
    m_contents.sampler = firstSampler.get();

    const resource::UniformBufferUsage usages[2] = { resource::UniformBufferUsage::SingleFrame,
                                                     resource::UniformBufferUsage::MultiFrame };
    for (const resource::UniformBufferUsage usage: usages)
    {
        TestUniformBufferContents initial = m_contents;
        initial.sampler = firstSampler.get();
        const resource::UniformBufferRef uniformBuffer =
            createVulkanUniformBuffer(tempServices(), &initial, layout.get(), usage);
        ASSERT_TRUE(uniformBuffer.isValid());

        TestUniformBufferContents updated = initial;
        updated.constants[0] = 42.0f;
        updated.constants[3] = -7.5f;
        updated.sampler = secondSampler.get();

        ASSERT_TRUE(cmdList().bypass());
        updateVulkanUniformBuffer(cmdList(), uniformBuffer.get(), &updated);

        EXPECT_TRUE(hasConstants(uniformBuffer, updated));
        EXPECT_EQ(uniformBuffer->getResourceTable()[0].get(), secondSampler.get());

        // A volatile buffer moves to a new temp range and drops the old one.
        if (usage == resource::UniformBufferUsage::SingleFrame)
        {
            EXPECT_TRUE(static_cast<const VulkanUniformBuffer*>(uniformBuffer.get())->isTempBlockAllocation());
            EXPECT_EQ(m_tempBlockAllocator->getCurrentBlockReferences(), 1u);
        }
    }
    resource::ResourceDeletionQueue::flushPendingDeletes(true);
    EXPECT_EQ(m_tempBlockAllocator->getCurrentBlockReferences(), 0u);
}

}   // namespace gp::rhi::vulkan::tests
