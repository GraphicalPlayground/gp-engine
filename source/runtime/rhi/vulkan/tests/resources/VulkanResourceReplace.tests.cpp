// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "resources/VulkanResourceReplace.hpp"
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "memory/pointers/RefCountedPtr.hpp"
#include "resources/BufferCreateDesc.hpp"
#include "resources/BufferEnums.hpp"
#include "resources/TextureInitializer.hpp"
#include "resources/VulkanBuffer.hpp"
#include "resources/VulkanLinkedView.hpp"
#include "VulkanResourceTestFakes.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

namespace
{

using VulkanBufferRef = gp::RefCountedPtr<VulkanBuffer>;

class CountingView final : public VulkanLinkedView
{
public:
    CountingView() noexcept = default;
    virtual ~CountingView() noexcept override = default;

    virtual void updateView([[maybe_unused]] const allocator::ContextArray& contexts) noexcept override
    {
        ++updateCount;
    }

    gp::Int32 updateCount{ 0 };
};

}   // namespace

class VulkanResourceReplaceLiveTest : public VulkanResourceLiveTest
{
protected:
    [[nodiscard]] VulkanBufferRef makeBuffer() noexcept
    {
        const resource::BufferCreateDesc desc = resource::BufferCreateDesc::create(
            "ReplaceBuffer", 128u, 0u, resource::BufferUsageFlags::VertexBuffer | resource::BufferUsageFlags::Static
        );
        return VulkanBufferRef(new VulkanBuffer(m_services, desc));
    }
};

TEST_F(VulkanResourceReplaceLiveTest, SourceAllocationMovesIntoDestination)
{
    const VulkanBufferRef destination = makeBuffer();
    const VulkanBufferRef source = makeBuffer();
    CountingView destinationView{};
    destination->linkView(destinationView);

    const VkBuffer sourceHandle = source->getHandle();
    const gp::UInt32 sourceOffset = source->getOffset();

    const resource::ResourceReplaceInfo info(destination.get(), source.get());
    replaceVulkanResources(cmdList(), gp::ConstVectorView<resource::ResourceReplaceInfo>(&info, 1u));

    EXPECT_EQ(destination->getHandle(), sourceHandle);
    EXPECT_EQ(destination->getOffset(), sourceOffset);
    EXPECT_EQ(source->getHandle(), VK_NULL_HANDLE);
    EXPECT_EQ(destinationView.updateCount, 1);

    destinationView.unlink();
}

TEST_F(VulkanResourceReplaceLiveTest, NullSourceReleasesDestination)
{
    const VulkanBufferRef destination = makeBuffer();
    CountingView destinationView{};
    destination->linkView(destinationView);
    ASSERT_TRUE(destination->getCurrentBufferAlloc().alloc.hasAllocation());

    const resource::ResourceReplaceInfo info(destination.get(), nullptr);
    replaceVulkanResources(cmdList(), gp::ConstVectorView<resource::ResourceReplaceInfo>(&info, 1u));

    EXPECT_FALSE(destination->getCurrentBufferAlloc().alloc.hasAllocation());
    EXPECT_EQ(destinationView.updateCount, 1);

    destinationView.unlink();
}

TEST_F(VulkanResourceReplaceLiveTest, ProcessesEveryEntryInOrder)
{
    const VulkanBufferRef first = makeBuffer();
    const VulkanBufferRef second = makeBuffer();
    const VulkanBufferRef sourceA = makeBuffer();
    const VulkanBufferRef sourceB = makeBuffer();

    const VkBuffer handleA = sourceA->getHandle();
    const gp::UInt32 offsetA = sourceA->getOffset();
    const VkBuffer handleB = sourceB->getHandle();
    const gp::UInt32 offsetB = sourceB->getOffset();

    const resource::ResourceReplaceInfo infos[2] = {
        resource::ResourceReplaceInfo(first.get(), sourceA.get()),
        resource::ResourceReplaceInfo(second.get(), sourceB.get()),
    };
    replaceVulkanResources(cmdList(), gp::ConstVectorView<resource::ResourceReplaceInfo>(infos, 2u));

    EXPECT_EQ(first->getHandle(), handleA);
    EXPECT_EQ(first->getOffset(), offsetA);
    EXPECT_EQ(second->getHandle(), handleB);
    EXPECT_EQ(second->getOffset(), offsetB);
}

TEST_F(VulkanResourceReplaceLiveTest, EmptyBatchIsNoOp)
{
    replaceVulkanResources(cmdList(), gp::ConstVectorView<resource::ResourceReplaceInfo>());
    SUCCEED();
}

}   // namespace gp::rhi::vulkan::tests
