// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "resources/VulkanBufferUsage.hpp"
#include "CoreMinimal.hpp"
#include "extensions/VulkanExtensionTypes.hpp"
#include "resources/BufferEnums.hpp"
#include "resources/VulkanBufferEnums.hpp"
#include "resources/VulkanBufferStats.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

namespace
{

using resource::BufferUsageFlags;

/// @brief Extensions with every ray tracing requirement enabled.
[[nodiscard]] OptionalVulkanDeviceExtensions makeRayTracingExtensions() noexcept
{
    OptionalVulkanDeviceExtensions extensions{};
    extensions.hasAccelerationStructure = 1u;
    extensions.hasRayTracingPipeline = 1u;
    extensions.hasKHRPipelineLibrary = 1u;
    extensions.hasEXTDescriptorIndexing = 1u;
    extensions.hasBufferDeviceAddress = 1u;
    extensions.hasDeferredHostOperations = 1u;
    extensions.hasSPIRV_14 = 1u;
    extensions.hasShaderFloatControls = 1u;
    return extensions;
}

[[nodiscard]] bool hasBits(const VkBufferUsageFlags flags, const VkBufferUsageFlags bits) noexcept
{
    return (flags & bits) == bits;
}

[[nodiscard]] gp::Int64 getCategoryBytes(const VulkanBufferMemoryStats& stats, const VulkanBufferStatCategory category)
{
    return stats.categoryBytes[static_cast<gp::USize>(category)];
}

}   // namespace

static_assert(hasAnyBufferUsage(BufferUsageFlags::Dynamic | BufferUsageFlags::VertexBuffer, BufferUsageFlags::Dynamic));
static_assert(!hasAnyBufferUsage(BufferUsageFlags::Static, BufferUsageFlags::Dynamic));
static_assert(isDynamicBufferUsage(BufferUsageFlags::Dynamic));
static_assert(isDynamicBufferUsage(BufferUsageFlags::Volatile));
static_assert(!isDynamicBufferUsage(BufferUsageFlags::Static | BufferUsageFlags::UnorderedAccess));

TEST(VulkanBufferUsageTest, AlwaysIncludesTransferSource)
{
    const OptionalVulkanDeviceExtensions extensions{};

    EXPECT_TRUE(
        hasBits(translateBufferUsageFlags(extensions, BufferUsageFlags::None, true), VK_BUFFER_USAGE_TRANSFER_SRC_BIT)
    );
    EXPECT_TRUE(hasBits(
        translateBufferUsageFlags(extensions, BufferUsageFlags::Static, false), VK_BUFFER_USAGE_TRANSFER_SRC_BIT
    ));
}

TEST(VulkanBufferUsageTest, TranslatesBindFlagsRegardlessOfSize)
{
    const OptionalVulkanDeviceExtensions extensions{};
    const BufferUsageFlags usage = BufferUsageFlags::VertexBuffer | BufferUsageFlags::IndexBuffer |
                                   BufferUsageFlags::StructuredBuffer | BufferUsageFlags::UniformBuffer;

    const VkBufferUsageFlags zeroSize = translateBufferUsageFlags(extensions, usage, true);
    const VkBufferUsageFlags sized = translateBufferUsageFlags(extensions, usage, false);

    const VkBufferUsageFlags expected = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                                        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    EXPECT_TRUE(hasBits(zeroSize, expected));
    EXPECT_TRUE(hasBits(sized, expected));
}

TEST(VulkanBufferUsageTest, ByteAddressBufferMapsToStorageBuffer)
{
    const OptionalVulkanDeviceExtensions extensions{};
    EXPECT_TRUE(hasBits(
        translateBufferUsageFlags(extensions, BufferUsageFlags::ByteAddressBuffer, false),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
    ));
}

TEST(VulkanBufferUsageTest, ZeroSizeSkipsViewAndCopyFlags)
{
    const OptionalVulkanDeviceExtensions extensions = makeRayTracingExtensions();
    const BufferUsageFlags usage = BufferUsageFlags::UnorderedAccess | BufferUsageFlags::DrawIndirect |
                                   BufferUsageFlags::KeepCPUAccessible | BufferUsageFlags::ShaderResource;

    const VkBufferUsageFlags zeroSize = translateBufferUsageFlags(extensions, usage, true);

    EXPECT_EQ(zeroSize, static_cast<VkBufferUsageFlags>(VK_BUFFER_USAGE_TRANSFER_SRC_BIT));
}

TEST(VulkanBufferUsageTest, TranslatesViewFlagsForSizedBuffers)
{
    const OptionalVulkanDeviceExtensions extensions{};
    const BufferUsageFlags usage = BufferUsageFlags::UnorderedAccess | BufferUsageFlags::DrawIndirect |
                                   BufferUsageFlags::KeepCPUAccessible | BufferUsageFlags::ShaderResource;

    const VkBufferUsageFlags flags = translateBufferUsageFlags(extensions, usage, false);

    EXPECT_TRUE(hasBits(flags, VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT));
    EXPECT_TRUE(hasBits(flags, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT));
    EXPECT_TRUE(hasBits(flags, VK_BUFFER_USAGE_TRANSFER_DST_BIT));
    EXPECT_TRUE(hasBits(flags, VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT));
}

TEST(VulkanBufferUsageTest, VolatileBuffersAreNotCopyDestinations)
{
    const OptionalVulkanDeviceExtensions extensions{};

    EXPECT_FALSE(hasBits(
        translateBufferUsageFlags(extensions, BufferUsageFlags::Volatile, false), VK_BUFFER_USAGE_TRANSFER_DST_BIT
    ));
    EXPECT_TRUE(hasBits(
        translateBufferUsageFlags(extensions, BufferUsageFlags::Dynamic, false), VK_BUFFER_USAGE_TRANSFER_DST_BIT
    ));
    EXPECT_TRUE(hasBits(
        translateBufferUsageFlags(extensions, BufferUsageFlags::Static, false), VK_BUFFER_USAGE_TRANSFER_DST_BIT
    ));
}

TEST(VulkanBufferUsageTest, BufferDeviceAddressAddsShaderDeviceAddress)
{
    OptionalVulkanDeviceExtensions extensions{};
    EXPECT_FALSE(hasBits(
        translateBufferUsageFlags(extensions, BufferUsageFlags::Static, false),
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
    ));

    extensions.hasBufferDeviceAddress = 1u;
    EXPECT_TRUE(hasBits(
        translateBufferUsageFlags(extensions, BufferUsageFlags::Static, false),
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
    ));
    EXPECT_FALSE(hasBits(
        translateBufferUsageFlags(extensions, BufferUsageFlags::Static, true), VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
    ));
}

TEST(VulkanBufferUsageTest, RayTracingAddsBuildInputAndStorageForGeometry)
{
    const OptionalVulkanDeviceExtensions extensions = makeRayTracingExtensions();
    ASSERT_TRUE(extensions.hasRaytracingExtensions());

    const VkBufferUsageFlags vertexFlags = translateBufferUsageFlags(extensions, BufferUsageFlags::VertexBuffer, false);
    EXPECT_TRUE(hasBits(vertexFlags, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR));
    EXPECT_TRUE(hasBits(vertexFlags, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT));
    EXPECT_TRUE(hasBits(vertexFlags, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT));

    const VkBufferUsageFlags asFlags =
        translateBufferUsageFlags(extensions, BufferUsageFlags::AccelerationStructure, false);
    EXPECT_TRUE(hasBits(asFlags, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR));
    EXPECT_FALSE(hasBits(asFlags, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR));
    EXPECT_FALSE(hasBits(asFlags, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT));
}

TEST(VulkanBufferUsageTest, WithoutRayTracingGeometryIsNotStorage)
{
    const OptionalVulkanDeviceExtensions extensions{};
    const VkBufferUsageFlags vertexFlags = translateBufferUsageFlags(extensions, BufferUsageFlags::VertexBuffer, false);

    EXPECT_FALSE(hasBits(vertexFlags, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT));
    EXPECT_FALSE(hasBits(vertexFlags, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR));
}

TEST(VulkanBufferStatsTest, CategoryFollowsPriorityOrder)
{
    EXPECT_EQ(
        getVulkanBufferStatCategory(BufferUsageFlags::ReservedResource | BufferUsageFlags::VertexBuffer),
        VulkanBufferStatCategory::ReservedUncommitted
    );
    EXPECT_EQ(
        getVulkanBufferStatCategory(BufferUsageFlags::VertexBuffer | BufferUsageFlags::IndexBuffer),
        VulkanBufferStatCategory::Vertex
    );
    EXPECT_EQ(getVulkanBufferStatCategory(BufferUsageFlags::IndexBuffer), VulkanBufferStatCategory::Index);
    EXPECT_EQ(
        getVulkanBufferStatCategory(BufferUsageFlags::AccelerationStructure),
        VulkanBufferStatCategory::AccelerationStructure
    );
    EXPECT_EQ(
        getVulkanBufferStatCategory(BufferUsageFlags::ByteAddressBuffer | BufferUsageFlags::DrawIndirect),
        VulkanBufferStatCategory::ByteAddress
    );
    EXPECT_EQ(getVulkanBufferStatCategory(BufferUsageFlags::DrawIndirect), VulkanBufferStatCategory::DrawIndirect);
    EXPECT_EQ(getVulkanBufferStatCategory(BufferUsageFlags::StructuredBuffer), VulkanBufferStatCategory::Structured);
    EXPECT_EQ(getVulkanBufferStatCategory(BufferUsageFlags::UniformBuffer), VulkanBufferStatCategory::Misc);
}

TEST(VulkanBufferStatsTest, AllocateAndFreeAreSymmetric)
{
    constexpr gp::UInt64 kSize = 4096u;
    const VulkanBufferMemoryStats before = getVulkanBufferMemoryStats();

    updateVulkanBufferStats(BufferUsageFlags::IndexBuffer, kSize, true);
    const VulkanBufferMemoryStats allocated = getVulkanBufferMemoryStats();
    EXPECT_EQ(allocated.totalBufferBytes - before.totalBufferBytes, static_cast<gp::Int64>(kSize));
    EXPECT_EQ(
        getCategoryBytes(allocated, VulkanBufferStatCategory::Index) -
            getCategoryBytes(before, VulkanBufferStatCategory::Index),
        static_cast<gp::Int64>(kSize)
    );

    updateVulkanBufferStats(BufferUsageFlags::IndexBuffer, kSize, false);
    const VulkanBufferMemoryStats freed = getVulkanBufferMemoryStats();
    EXPECT_EQ(freed.totalBufferBytes, before.totalBufferBytes);
    EXPECT_EQ(
        getCategoryBytes(freed, VulkanBufferStatCategory::Index),
        getCategoryBytes(before, VulkanBufferStatCategory::Index)
    );
}

TEST(VulkanBufferStatsTest, ReservedBuffersTrackVirtualAndCommittedBytes)
{
    constexpr gp::UInt64 kVirtualSize = 1u << 20u;
    constexpr gp::UInt64 kCommitSize = 1u << 16u;
    const VulkanBufferMemoryStats before = getVulkanBufferMemoryStats();

    updateVulkanBufferStats(BufferUsageFlags::ReservedResource, kVirtualSize, true);
    updateVulkanReservedBufferStatsOnCommit(kCommitSize, true);

    const VulkanBufferMemoryStats committed = getVulkanBufferMemoryStats();
    EXPECT_EQ(committed.reservedVirtualBytes - before.reservedVirtualBytes, static_cast<gp::Int64>(kVirtualSize));
    EXPECT_EQ(committed.reservedCommittedBytes - before.reservedCommittedBytes, static_cast<gp::Int64>(kCommitSize));
    EXPECT_EQ(
        getCategoryBytes(committed, VulkanBufferStatCategory::ReservedUncommitted) -
            getCategoryBytes(before, VulkanBufferStatCategory::ReservedUncommitted),
        static_cast<gp::Int64>(kVirtualSize - kCommitSize)
    );

    updateVulkanReservedBufferStatsOnCommit(kCommitSize, false);
    updateVulkanBufferStats(BufferUsageFlags::ReservedResource, kVirtualSize, false);

    const VulkanBufferMemoryStats after = getVulkanBufferMemoryStats();
    EXPECT_EQ(after.reservedVirtualBytes, before.reservedVirtualBytes);
    EXPECT_EQ(after.reservedCommittedBytes, before.reservedCommittedBytes);
    EXPECT_EQ(after.totalBufferBytes, before.totalBufferBytes);
    EXPECT_EQ(
        getCategoryBytes(after, VulkanBufferStatCategory::ReservedUncommitted),
        getCategoryBytes(before, VulkanBufferStatCategory::ReservedUncommitted)
    );
}

}   // namespace gp::rhi::vulkan::tests

