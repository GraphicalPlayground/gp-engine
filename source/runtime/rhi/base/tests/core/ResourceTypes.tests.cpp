// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "core/ResourceTypes.hpp"
#include "core/ResourceEnums.hpp"
#include <gtest/gtest.h>

namespace gp::rhi::resource::tests
{

static_assert(kResourceTypeCount > 0u, "kResourceTypeCount must be non-zero.");
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

}   // namespace gp::rhi::resource::tests
