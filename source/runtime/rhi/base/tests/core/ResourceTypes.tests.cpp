// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "core/ResourceTypes.hpp"
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "core/ResourceEnums.hpp"
#include "hashing/Fnv1a.hpp"
#include <gtest/gtest.h>

namespace gp::rhi::resource::tests
{

static_assert(kResourceTypeCount == 41u, "kResourceTypeCount must cover all 41 ResourceType enumerators.");
static_assert(kResourceLockModeCount == 3u, "kResourceLockModeCount must cover all 3 ResourceLockMode enumerators.");
static_assert(
    isPipelineStateResourceType(ResourceType::GraphicsPipelineState),
    "GraphicsPipelineState must classify as a pipeline state."
);
static_assert(
    isPipelineStateResourceType(ResourceType::ComputePipelineState),
    "ComputePipelineState must classify as a pipeline state."
);
static_assert(
    isPipelineStateResourceType(ResourceType::RayTracingPipelineState),
    "RayTracingPipelineState must classify as a pipeline state."
);
static_assert(
    isPipelineStateResourceType(ResourceType::WorkGraphPipelineState),
    "WorkGraphPipelineState must classify as a pipeline state."
);
static_assert(!isPipelineStateResourceType(ResourceType::None), "None is not a pipeline state.");
static_assert(!isPipelineStateResourceType(ResourceType::Buffer), "Buffer is not a pipeline state.");
static_assert(!isPipelineStateResourceType(ResourceType::Texture), "Texture is not a pipeline state.");
static_assert(!isPipelineStateResourceType(ResourceType::VertexShader), "VertexShader is not a pipeline state.");

static_assert(!VRamAllocation{}.isValid(), "Default VRamAllocation must be invalid.");
static_assert(VRamAllocation{ 0u, 256u }.isValid(), "VRamAllocation with non-zero size must be valid.");
static_assert(!VRamAllocation{ 4096u, 0u }.isValid(), "VRamAllocation with zero size must be invalid.");
static_assert(
    SizeAndStride{ 1024u, 16u } == SizeAndStride{ 1024u, 16u },
    "SizeAndStride equality must compare both size and stride."
);
static_assert(
    SizeAndStride{ 1024u, 16u } != SizeAndStride{ 1024u, 32u },
    "SizeAndStride inequality must detect stride differences."
);

TEST(ResourceTypesTest, SizeAndStrideDefaultsAndEquality)
{
    const SizeAndStride empty{};
    EXPECT_EQ(empty.size, 0u);
    EXPECT_EQ(empty.stride, 0u);

    const SizeAndStride vertexBuffer{ 4096u, 32u };
    const SizeAndStride sameVertexBuffer{ 4096u, 32u };
    const SizeAndStride otherSize{ 8192u, 32u };
    const SizeAndStride otherStride{ 4096u, 64u };

    EXPECT_EQ(vertexBuffer, sameVertexBuffer);
    EXPECT_NE(vertexBuffer, otherSize);
    EXPECT_NE(vertexBuffer, otherStride);
}

TEST(ResourceTypesTest, VRamAllocationValidityAndEquality)
{
    const VRamAllocation defaultAlloc{};
    EXPECT_EQ(defaultAlloc.allocationStart, 0u);
    EXPECT_EQ(defaultAlloc.allocationSize, 0u);
    EXPECT_FALSE(defaultAlloc.isValid());

    const VRamAllocation validAlloc{ 65536u, 1048576u };
    EXPECT_EQ(validAlloc.allocationStart, 65536u);
    EXPECT_EQ(validAlloc.allocationSize, 1048576u);
    EXPECT_TRUE(validAlloc.isValid());
    EXPECT_EQ(validAlloc, VRamAllocation(65536u, 1048576u));
    EXPECT_NE(validAlloc, defaultAlloc);
}

TEST(ResourceTypesTest, ResourceInfoDefaultsAndFields)
{
    const ResourceInfo defaultInfo{};
    EXPECT_TRUE(defaultInfo.name.isNone());
    EXPECT_EQ(defaultInfo.type, ResourceType::None);
    EXPECT_FALSE(defaultInfo.vramAllocation.isValid());
    EXPECT_FALSE(defaultInfo.isTransient);
    EXPECT_TRUE(defaultInfo.isValid);
    EXPECT_TRUE(defaultInfo.isResident);

    ResourceInfo customInfo{};
    customInfo.name = gp::Name("SceneDepthZ");
    customInfo.type = ResourceType::Texture;
    customInfo.vramAllocation = VRamAllocation(1024u, 8388608u);
    customInfo.isTransient = true;
    customInfo.isValid = true;
    customInfo.isResident = false;

    EXPECT_EQ(customInfo.name, gp::Name("SceneDepthZ"));
    EXPECT_EQ(customInfo.type, ResourceType::Texture);
    EXPECT_TRUE(customInfo.vramAllocation.isValid());
    EXPECT_TRUE(customInfo.isTransient);
    EXPECT_FALSE(customInfo.isResident);
    EXPECT_NE(customInfo, defaultInfo);
}

TEST(ResourceTypesTest, GeometryPrimitivesRangesEnumsAndUploadAdapter)
{
    EXPECT_NE(ResourceType::RayTracingOpacityMicromapArray, ResourceType::RayTracingScene);
    EXPECT_NE(ResourceLockMode::ReadOnly, ResourceLockMode::WriteOnly);
    EXPECT_NE(ResourceLockMode::WriteOnly, ResourceLockMode::WriteOnlyNoOverwrite);

    // IntPoint
    const IntPoint ptDefault{};
    const IntPoint ptA(64, 128);
    const IntPoint ptB(64, 128);
    const IntPoint ptC(64, 256);
    EXPECT_EQ(ptDefault.x, 0);
    EXPECT_EQ(ptDefault.y, 0);
    EXPECT_EQ(ptA, ptB);
    EXPECT_NE(ptA, ptC);

    gp::hashing::Fnv1a64 ptHashA{};
    gp::hashing::Fnv1a64 ptHashB{};
    gp::hashing::Fnv1a64 ptHashC{};
    ptA.hashAppend(ptHashA);
    ptB.hashAppend(ptHashB);
    ptC.hashAppend(ptHashC);
    EXPECT_EQ(ptHashA.finalize(), ptHashB.finalize());
    EXPECT_NE(ptHashA.finalize(), ptHashC.finalize());

    // IntVector
    const IntVector vecDefault{};
    const IntVector vecA(16, 32, 64);
    const IntVector vecB(16, 32, 64);
    const IntVector vecC(16, 32, 128);
    EXPECT_EQ(vecDefault.x, 0);
    EXPECT_EQ(vecDefault.y, 0);
    EXPECT_EQ(vecDefault.z, 0);
    EXPECT_EQ(vecA, vecB);
    EXPECT_NE(vecA, vecC);

    gp::hashing::Fnv1a64 vecHashA{};
    gp::hashing::Fnv1a64 vecHashB{};
    gp::hashing::Fnv1a64 vecHashC{};
    vecA.hashAppend(vecHashA);
    vecB.hashAppend(vecHashB);
    vecC.hashAppend(vecHashC);
    EXPECT_EQ(vecHashA.finalize(), vecHashB.finalize());
    EXPECT_NE(vecHashA.finalize(), vecHashC.finalize());

    // IntRect
    const IntRect rectDefault{};
    EXPECT_EQ(rectDefault.width(), 0);
    EXPECT_EQ(rectDefault.height(), 0);

    const IntRect rectFromCoords(10, 20, 110, 220);
    const IntRect rectFromPoints(IntPoint(10, 20), IntPoint(110, 220));
    EXPECT_EQ(rectFromCoords, rectFromPoints);
    EXPECT_EQ(rectFromCoords.width(), 100);
    EXPECT_EQ(rectFromCoords.height(), 200);
    EXPECT_NE(rectFromCoords, rectDefault);

    // Range8 & Range16
    const Range8 mipRange(1u, 4u);
    EXPECT_EQ(mipRange.exclusiveLast(), 5u);
    EXPECT_EQ(mipRange.inclusiveLast(), 4u);
    EXPECT_FALSE(mipRange.isInRange(0u));
    EXPECT_TRUE(mipRange.isInRange(1u));
    EXPECT_TRUE(mipRange.isInRange(4u));
    EXPECT_FALSE(mipRange.isInRange(5u));
    EXPECT_EQ(mipRange, Range8(1u, 4u));
    EXPECT_NE(mipRange, Range8(1u, 3u));

    const Range16 emptySliceRange{};
    EXPECT_EQ(emptySliceRange.exclusiveLast(), 0u);
    EXPECT_FALSE(emptySliceRange.isInRange(0u));

    // ResourceArrayUploadView
    const gp::UInt32 data[3]{ 7u, 8u, 9u };
    ResourceArrayUploadView rawUpload(data, static_cast<gp::UInt32>(sizeof(data)));
    EXPECT_EQ(rawUpload.getResourceData(), static_cast<const void*>(data));
    EXPECT_EQ(rawUpload.getResourceDataSize(), 12u);
    rawUpload.discard();
    EXPECT_EQ(rawUpload.getResourceData(), static_cast<const void*>(data));

    ResourceArrayUploadView spanUpload(gp::ConstVectorView<gp::UInt32>(data, 3u));
    EXPECT_EQ(spanUpload.getResourceData(), static_cast<const void*>(data));
    EXPECT_EQ(spanUpload.getResourceDataSize(), 12u);
}

}   // namespace gp::rhi::resource::tests
