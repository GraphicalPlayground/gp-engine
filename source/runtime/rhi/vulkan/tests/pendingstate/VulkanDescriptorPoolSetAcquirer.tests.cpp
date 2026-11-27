// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "pendingstate/VulkanDescriptorPoolSetAcquirer.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "descriptors/VulkanDescriptorPoolSetContainer.hpp"
#include "descriptors/VulkanDescriptorPoolsManager.hpp"
#include "descriptors/VulkanDescriptorSetsEnums.hpp"
#include "descriptors/VulkanDescriptorSetsLayout.hpp"
#include "descriptors/VulkanDescriptorSetsTypes.hpp"
#include "device/VulkanDevice.hpp"
#include "shaders/VulkanShaderHeader.hpp"
#include "VulkanPendingStateTestFakes.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

namespace
{

/// @brief Offline fixture: a pool manager over a device without native handles and two distinct layouts.
/// @details Offline, a typed pool set "allocates" an uncompiled layout trivially, so the acquirer's decision (allocate
///          or keep the old sets) is observable through its return value alone.
class VulkanDescriptorPoolSetAcquirerTest : public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        m_manager.init(m_device);
        ASSERT_TRUE(finalize(m_bufferLayout, makePendingHeader({ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER }, 1u)));
        ASSERT_TRUE(finalize(m_imageLayout, makePendingHeader({ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE })));
    }

    /// @brief Finalizes a compute layout from one header.
    /// @param[in,out] layout Layout to finalize.
    /// @param[in] header Compute header.
    /// @return Result of `finalizeBindings()`.
    [[nodiscard]] bool finalize(VulkanDescriptorSetsLayout& layout, const VulkanShaderHeader& header) const
    {
        VulkanUniformBufferGatherInfo gatherInfo{};
        gatherInfo.codeHeaders[0] = &header;
        return layout.finalizeBindings(
            VK_PIPELINE_BIND_POINT_COMPUTE, m_limits, gatherInfo, false, VulkanDynamicUniformBufferMode::None
        );
    }

    /// @brief Device without native handles.
    VulkanDevice m_device{ VK_NULL_HANDLE, VK_NULL_HANDLE, VK_API_VERSION_1_3, false };

    /// @brief Device limits.
    VkPhysicalDeviceLimits m_limits{ makePendingLimits() };

    /// @brief Pool manager; declared before the acquirers so it outlives them.
    VulkanDescriptorPoolsManager m_manager{};

    /// @brief Layout with one uniform buffer.
    VulkanDescriptorSetsLayout m_bufferLayout{ m_device };

    /// @brief Layout with one storage image.
    VulkanDescriptorSetsLayout m_imageLayout{ m_device };

    /// @brief Output sets; never read offline.
    VkDescriptorSet m_sets[kVulkanMaxNumDescriptorStages] = {};
};

}   // namespace

TEST_F(VulkanDescriptorPoolSetAcquirerTest, NewAcquirerHoldsNoContainer)
{
    const VulkanDescriptorPoolSetAcquirer acquirer(m_manager);

    EXPECT_FALSE(acquirer.hasPoolSetContainer());
    EXPECT_EQ(acquirer.getPoolSetContainer(), nullptr);
}

TEST_F(VulkanDescriptorPoolSetAcquirerTest, FirstAcquisitionAlwaysAllocates)
{
    VulkanDescriptorPoolSetAcquirer acquirer(m_manager);

    // Unchanged resources still need sets: the previous ones came from another frame's container.
    EXPECT_TRUE(acquirer.acquirePoolSetAndDescriptorsIfNeeded(m_bufferLayout, false, 5u, m_sets));

    ASSERT_TRUE(acquirer.hasPoolSetContainer());
    EXPECT_EQ(acquirer.getPoolSetContainer()->getLastFrameUsed(), 5u);
    EXPECT_FALSE(acquirer.getPoolSetContainer()->isUnused());
    EXPECT_EQ(acquirer.getPoolSetContainer()->getNumTypedPoolSets(), 1u);
}

TEST_F(VulkanDescriptorPoolSetAcquirerTest, UnchangedResourcesKeepTheirSets)
{
    VulkanDescriptorPoolSetAcquirer acquirer(m_manager);
    ASSERT_TRUE(acquirer.acquirePoolSetAndDescriptorsIfNeeded(m_bufferLayout, false, 1u, m_sets));

    EXPECT_FALSE(acquirer.acquirePoolSetAndDescriptorsIfNeeded(m_bufferLayout, false, 1u, m_sets));
    EXPECT_TRUE(acquirer.acquirePoolSetAndDescriptorsIfNeeded(m_bufferLayout, true, 1u, m_sets));
}

TEST_F(VulkanDescriptorPoolSetAcquirerTest, NewTypedPoolSetForcesAnAllocation)
{
    VulkanDescriptorPoolSetAcquirer acquirer(m_manager);
    ASSERT_TRUE(acquirer.acquirePoolSetAndDescriptorsIfNeeded(m_bufferLayout, false, 1u, m_sets));

    EXPECT_TRUE(acquirer.acquirePoolSetAndDescriptorsIfNeeded(m_imageLayout, false, 1u, m_sets));
    ASSERT_TRUE(acquirer.hasPoolSetContainer());
    EXPECT_EQ(acquirer.getPoolSetContainer()->getNumTypedPoolSets(), 2u);
    EXPECT_FALSE(acquirer.acquirePoolSetAndDescriptorsIfNeeded(m_imageLayout, false, 1u, m_sets));
}

TEST_F(VulkanDescriptorPoolSetAcquirerTest, ReleaseHandsTheContainerBack)
{
    VulkanDescriptorPoolSetAcquirer acquirer(m_manager);
    ASSERT_TRUE(acquirer.acquirePoolSetAndDescriptorsIfNeeded(m_bufferLayout, false, 1u, m_sets));
    VulkanDescriptorPoolSetContainer* const container = acquirer.getPoolSetContainer();
    ASSERT_NE(container, nullptr);

    acquirer.release();

    EXPECT_FALSE(acquirer.hasPoolSetContainer());
    EXPECT_TRUE(container->isUnused());
    acquirer.release();
    EXPECT_EQ(m_manager.getNumPoolSetContainers(), 1u);
}

TEST_F(VulkanDescriptorPoolSetAcquirerTest, NextFrameRecyclesTheContainerAndReallocates)
{
    VulkanDescriptorPoolSetAcquirer acquirer(m_manager);
    ASSERT_TRUE(acquirer.acquirePoolSetAndDescriptorsIfNeeded(m_bufferLayout, false, 1u, m_sets));
    VulkanDescriptorPoolSetContainer* const first = acquirer.getPoolSetContainer();
    acquirer.release();

    EXPECT_TRUE(acquirer.acquirePoolSetAndDescriptorsIfNeeded(m_bufferLayout, false, 2u, m_sets));

    EXPECT_EQ(acquirer.getPoolSetContainer(), first);
    EXPECT_EQ(first->getLastFrameUsed(), 2u);
    EXPECT_EQ(m_manager.getNumPoolSetContainers(), 1u);
}

TEST_F(VulkanDescriptorPoolSetAcquirerTest, DestructionReleasesTheContainer)
{
    VulkanDescriptorPoolSetContainer* container = nullptr;
    {
        VulkanDescriptorPoolSetAcquirer acquirer(m_manager);
        ASSERT_TRUE(acquirer.acquirePoolSetAndDescriptorsIfNeeded(m_bufferLayout, false, 1u, m_sets));
        container = acquirer.getPoolSetContainer();
    }

    ASSERT_NE(container, nullptr);
    EXPECT_TRUE(container->isUnused());
}

TEST_F(VulkanDescriptorPoolSetAcquirerTest, TwoAcquirersHoldDistinctContainers)
{
    VulkanDescriptorPoolSetAcquirer first(m_manager);
    VulkanDescriptorPoolSetAcquirer second(m_manager);

    ASSERT_TRUE(first.acquirePoolSetAndDescriptorsIfNeeded(m_bufferLayout, false, 1u, m_sets));
    ASSERT_TRUE(second.acquirePoolSetAndDescriptorsIfNeeded(m_bufferLayout, false, 1u, m_sets));

    EXPECT_NE(first.getPoolSetContainer(), second.getPoolSetContainer());
    EXPECT_EQ(m_manager.getNumPoolSetContainers(), 2u);
}

}   // namespace gp::rhi::vulkan::tests
