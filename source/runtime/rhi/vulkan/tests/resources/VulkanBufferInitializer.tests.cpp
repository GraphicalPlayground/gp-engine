// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "resources/VulkanBufferInitializer.hpp"
#include "core/ResourceTypes.hpp"
#include "CoreMinimal.hpp"
#include "resources/Buffer.hpp"   // IWYU pragma: keep
#include "resources/BufferCreateDesc.hpp"
#include "resources/BufferEnums.hpp"
#include "resources/BufferInitializer.hpp"
#include "resources/VulkanBuffer.hpp"
#include "resources/VulkanBufferEnums.hpp"
#include "resources/VulkanPendingBufferLocks.hpp"
#include "VulkanResourceTestFakes.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

namespace
{

constexpr gp::UInt32 kBufferSize = 64u;

/// @brief Upload source that records whether the backend released it.
class TrackingUpload final : public resource::IResourceArrayUpload
{
public:
    TrackingUpload() noexcept
    {
        for (gp::UInt32 index = 0u; index < kBufferSize; ++index)
        {
            bytes[index] = static_cast<gp::UInt8>(index + 1u);
        }
    }

    [[nodiscard]] virtual const void* getResourceData() const noexcept override
    {
        return bytes;
    }

    [[nodiscard]] virtual gp::UInt32 getResourceDataSize() const noexcept override
    {
        return kBufferSize;
    }

    virtual void discard() noexcept override
    {
        isDiscarded = true;
    }

    gp::UInt8 bytes[kBufferSize]{};
    bool isDiscarded{ false };
};

[[nodiscard]] VulkanBuffer* asVulkan(const resource::BufferRef& buffer) noexcept
{
    return static_cast<VulkanBuffer*>(buffer.get());
}

}   // namespace

class VulkanBufferInitializerLiveTest : public VulkanResourceLiveTest
{
protected:
    [[nodiscard]] resource::BufferCreateDesc makeDesc(const resource::BufferUsageFlags usage) const noexcept
    {
        return resource::BufferCreateDesc::create("InitBuffer", kBufferSize, 0u, usage);
    }
};

TEST_F(VulkanBufferInitializerLiveTest, DefaultActionReturnsBufferWithoutWritableData)
{
    const resource::BufferCreateDesc desc =
        makeDesc(resource::BufferUsageFlags::VertexBuffer | resource::BufferUsageFlags::Static);

    resource::BufferInitializer initializer = createVulkanBufferInitializer(m_services, cmdList(), desc);
    ASSERT_TRUE(initializer.isValid());
    EXPECT_EQ(initializer.writableData, nullptr);

    const resource::BufferRef buffer = initializer.finalize();
    ASSERT_NE(buffer.get(), nullptr);
    EXPECT_NE(asVulkan(buffer)->getHandle(), VK_NULL_HANDLE);
    EXPECT_EQ(m_provider.uploadRecorder.totalCommandCount(), 0u);
}

TEST_F(VulkanBufferInitializerLiveTest, NullResourceUsesDefaultPath)
{
    const resource::BufferCreateDesc desc = resource::BufferCreateDesc::createNull("NullBuffer");

    resource::BufferInitializer initializer = createVulkanBufferInitializer(m_services, cmdList(), desc);
    const resource::BufferRef buffer = initializer.finalize();

    ASSERT_NE(buffer.get(), nullptr);
    EXPECT_EQ(asVulkan(buffer)->getHandle(), VK_NULL_HANDLE);
}

TEST_F(VulkanBufferInitializerLiveTest, ZeroedHostVisibleBufferIsClearedOnTheCPU)
{
    resource::BufferCreateDesc desc =
        makeDesc(resource::BufferUsageFlags::VertexBuffer | resource::BufferUsageFlags::Dynamic);
    desc.setInitActionZeroData();

    resource::BufferInitializer initializer = createVulkanBufferInitializer(m_services, cmdList(), desc);
    const resource::BufferRef buffer = initializer.finalize();
    ASSERT_NE(buffer.get(), nullptr);

    const gp::UInt8* const bytes = static_cast<const gp::UInt8*>(asVulkan(buffer)->getCurrentHostPointer());
    ASSERT_NE(bytes, nullptr);
    for (gp::UInt32 index = 0u; index < kBufferSize; ++index)
    {
        EXPECT_EQ(bytes[index], 0u);
    }
    EXPECT_EQ(m_provider.uploadRecorder.fillCount, 0u);
}

TEST_F(VulkanBufferInitializerLiveTest, ZeroedDeviceLocalBufferIsFilledOnUploadRecorder)
{
    if (isUnifiedMemory())
    {
        GTEST_SKIP() << "Unified memory clears static buffers on the CPU.";
    }

    resource::BufferCreateDesc desc =
        makeDesc(resource::BufferUsageFlags::VertexBuffer | resource::BufferUsageFlags::Static);
    desc.setInitActionZeroData();

    resource::BufferInitializer initializer = createVulkanBufferInitializer(m_services, cmdList(), desc);
    EXPECT_EQ(m_provider.uploadRecorder.fillCount, 0u);

    const resource::BufferRef buffer = initializer.finalize();
    ASSERT_NE(buffer.get(), nullptr);

    const FakeTransferRecorder& upload = m_provider.uploadRecorder;
    ASSERT_EQ(upload.fillCount, 1u);
    EXPECT_EQ(upload.fills[0].dstBuffer, asVulkan(buffer)->getHandle());
    EXPECT_EQ(upload.fills[0].offset, static_cast<VkDeviceSize>(asVulkan(buffer)->getOffset()));
    EXPECT_EQ(upload.fills[0].size, static_cast<VkDeviceSize>(asVulkan(buffer)->getCurrentSize()));
    EXPECT_EQ(upload.fills[0].data, 0u);
    ASSERT_EQ(upload.barrierCount, 1u);
    EXPECT_EQ(upload.barriers[0].srcAccessMask, static_cast<VkAccessFlags>(VK_ACCESS_TRANSFER_WRITE_BIT));
}

TEST_F(VulkanBufferInitializerLiveTest, ResourceArrayOnHostVisibleBufferCopiesInPlace)
{
    TrackingUpload upload{};
    resource::BufferCreateDesc desc =
        makeDesc(resource::BufferUsageFlags::VertexBuffer | resource::BufferUsageFlags::Dynamic);
    desc.setInitActionResourceArray(&upload);

    resource::BufferInitializer initializer = createVulkanBufferInitializer(m_services, cmdList(), desc);
    const resource::BufferRef buffer = initializer.finalize();
    ASSERT_NE(buffer.get(), nullptr);

    EXPECT_TRUE(upload.isDiscarded);
    const gp::UInt8* const bytes = static_cast<const gp::UInt8*>(asVulkan(buffer)->getCurrentHostPointer());
    ASSERT_NE(bytes, nullptr);
    for (gp::UInt32 index = 0u; index < kBufferSize; ++index)
    {
        EXPECT_EQ(bytes[index], upload.bytes[index]);
    }

    // The initial write counts as the first lock, so the next write lock renames.
    EXPECT_EQ(asVulkan(buffer)->getLockCounter(), 1u);
    EXPECT_EQ(asVulkan(buffer)->getLockStatus(), VulkanBufferLockStatus::Unlocked);
}

TEST_F(VulkanBufferInitializerLiveTest, ResourceArrayOnDeviceLocalBufferUploadsThroughStaging)
{
    if (isUnifiedMemory())
    {
        GTEST_SKIP() << "Unified memory writes static buffers in place.";
    }

    TrackingUpload upload{};
    resource::BufferCreateDesc desc =
        makeDesc(resource::BufferUsageFlags::VertexBuffer | resource::BufferUsageFlags::Static);
    desc.setInitActionResourceArray(&upload);

    resource::BufferInitializer initializer = createVulkanBufferInitializer(m_services, cmdList(), desc);
    const resource::BufferRef buffer = initializer.finalize();
    ASSERT_NE(buffer.get(), nullptr);

    EXPECT_TRUE(upload.isDiscarded);
    EXPECT_FALSE(getVulkanPendingBufferLocks().contains(asVulkan(buffer)));
    ASSERT_EQ(m_provider.uploadRecorder.copyCount, 1u);
    EXPECT_EQ(m_provider.uploadRecorder.copies[0].dstBuffer, asVulkan(buffer)->getHandle());
    EXPECT_EQ(m_provider.uploadRecorder.copies[0].region.size, kBufferSize);
    EXPECT_EQ(m_provider.commandListRecorder.copyCount, 0u);
}

TEST_F(VulkanBufferInitializerLiveTest, InitializerOnHostVisibleBufferExposesMapping)
{
    resource::BufferCreateDesc desc =
        makeDesc(resource::BufferUsageFlags::VertexBuffer | resource::BufferUsageFlags::Dynamic);
    desc.setInitActionInitializer();

    resource::BufferInitializer initializer = createVulkanBufferInitializer(m_services, cmdList(), desc);
    ASSERT_NE(initializer.writableData, nullptr);
    EXPECT_EQ(initializer.writableDataSize, kBufferSize);
    EXPECT_EQ(initializer.writableData, asVulkan(initializer.buffer)->getCurrentHostPointer());

    const gp::UInt32 value = 0xA5A5A5A5u;
    initializer.writeData(&value, sizeof(value), 8u);

    const resource::BufferRef buffer = initializer.finalize();
    ASSERT_NE(buffer.get(), nullptr);
    const gp::UInt8* const bytes = static_cast<const gp::UInt8*>(asVulkan(buffer)->getCurrentHostPointer());
    EXPECT_EQ(bytes[8], 0xA5u);
    EXPECT_EQ(bytes[11], 0xA5u);
    EXPECT_EQ(m_provider.uploadRecorder.copyCount, 0u);
}

TEST_F(VulkanBufferInitializerLiveTest, InitializerOnDeviceLocalBufferUploadsOnFinalize)
{
    if (isUnifiedMemory())
    {
        GTEST_SKIP() << "Unified memory writes static buffers in place.";
    }

    resource::BufferCreateDesc desc =
        makeDesc(resource::BufferUsageFlags::VertexBuffer | resource::BufferUsageFlags::Static);
    desc.setInitActionInitializer();

    resource::BufferInitializer initializer = createVulkanBufferInitializer(m_services, cmdList(), desc);
    ASSERT_NE(initializer.writableData, nullptr);
    EXPECT_EQ(asVulkan(initializer.buffer)->getLockStatus(), VulkanBufferLockStatus::Locked);
    EXPECT_EQ(m_provider.uploadRecorder.copyCount, 0u);

    initializer.fillWithValue(0x7Fu);
    const resource::BufferRef buffer = initializer.finalize();
    ASSERT_NE(buffer.get(), nullptr);

    EXPECT_EQ(asVulkan(buffer)->getLockStatus(), VulkanBufferLockStatus::Unlocked);
    ASSERT_EQ(m_provider.uploadRecorder.copyCount, 1u);
    EXPECT_EQ(m_provider.uploadRecorder.copies[0].region.size, kBufferSize);
}

}   // namespace gp::rhi::vulkan::tests

