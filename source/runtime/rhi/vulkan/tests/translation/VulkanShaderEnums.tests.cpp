// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "translation/VulkanShaderEnums.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "shaders/ShaderEnums.hpp"
#include "VulkanCommon.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

namespace
{

static_assert(vkHasAllFlags(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, VK_SHADER_STAGE_VERTEX_BIT));
static_assert(!vkHasAllFlags(VK_SHADER_STAGE_VERTEX_BIT, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT));
static_assert(vkHasAnyFlags(VK_SHADER_STAGE_VERTEX_BIT, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT));
static_assert(!vkHasAnyFlags(VK_SHADER_STAGE_COMPUTE_BIT, VK_SHADER_STAGE_VERTEX_BIT));
static_assert(vkHasAllFlags(
    static_cast<VkFlags64>(VK_PIPELINE_STAGE_2_COPY_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT),
    VK_PIPELINE_STAGE_2_COPY_BIT
));
static_assert(shaderFrequencyToVkStageBit(shader::ShaderFrequency::Vertex) == VK_SHADER_STAGE_VERTEX_BIT);
static_assert(vkStageBitToShaderFrequency(VK_SHADER_STAGE_FRAGMENT_BIT) == shader::ShaderFrequency::Pixel);
static_assert(gfxStageToVkStageBit(shader_stage::Stage::Task) == VK_SHADER_STAGE_TASK_BIT_EXT);
static_assert(getNumStagesForBindPoint(VK_PIPELINE_BIND_POINT_GRAPHICS) == 5);

}   // namespace

TEST(VulkanShaderEnumsTest, FlagBitPredicates)
{
    const VkFlags combined = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;

    EXPECT_TRUE(vkHasAllFlags(combined, VK_ACCESS_SHADER_READ_BIT));
    EXPECT_TRUE(vkHasAllFlags(combined, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT));
    EXPECT_FALSE(vkHasAllFlags(combined, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT));
    EXPECT_TRUE(vkHasAllFlags(static_cast<VkFlags>(0u), static_cast<VkFlags>(0u)));
    EXPECT_TRUE(vkHasAllFlags(combined, static_cast<VkFlags>(0u)));

    EXPECT_TRUE(vkHasAnyFlags(combined, VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_HOST_READ_BIT));
    EXPECT_FALSE(vkHasAnyFlags(combined, VK_ACCESS_HOST_READ_BIT | VK_ACCESS_HOST_WRITE_BIT));
    EXPECT_FALSE(vkHasAnyFlags(static_cast<VkFlags>(0u), VK_ACCESS_SHADER_READ_BIT));
    EXPECT_FALSE(vkHasAnyFlags(combined, static_cast<VkFlags>(0u)));

    // 64-bit synchronization2 flag masks must not truncate upper bits.
    const VkFlags64 combined64 = VK_PIPELINE_STAGE_2_COPY_BIT | VK_PIPELINE_STAGE_2_BLIT_BIT;
    EXPECT_TRUE(vkHasAllFlags(combined64, VK_PIPELINE_STAGE_2_COPY_BIT));
    EXPECT_TRUE(vkHasAllFlags(combined64, VK_PIPELINE_STAGE_2_COPY_BIT | VK_PIPELINE_STAGE_2_BLIT_BIT));
    EXPECT_FALSE(vkHasAllFlags(combined64, VK_PIPELINE_STAGE_2_CLEAR_BIT));
    EXPECT_TRUE(vkHasAnyFlags(combined64, VK_PIPELINE_STAGE_2_BLIT_BIT | VK_PIPELINE_STAGE_2_CLEAR_BIT));
    EXPECT_FALSE(vkHasAnyFlags(combined64, VK_PIPELINE_STAGE_2_CLEAR_BIT));
}

TEST(VulkanShaderEnumsTest, ShaderFrequencyAndVkStageBitConversions)
{
    EXPECT_EQ(shaderFrequencyToVkStageBit(shader::ShaderFrequency::Vertex), VK_SHADER_STAGE_VERTEX_BIT);
    EXPECT_EQ(shaderFrequencyToVkStageBit(shader::ShaderFrequency::Mesh), VK_SHADER_STAGE_MESH_BIT_EXT);
    EXPECT_EQ(shaderFrequencyToVkStageBit(shader::ShaderFrequency::Amplification), VK_SHADER_STAGE_TASK_BIT_EXT);
    EXPECT_EQ(shaderFrequencyToVkStageBit(shader::ShaderFrequency::Pixel), VK_SHADER_STAGE_FRAGMENT_BIT);
    EXPECT_EQ(shaderFrequencyToVkStageBit(shader::ShaderFrequency::Geometry), VK_SHADER_STAGE_GEOMETRY_BIT);
    EXPECT_EQ(shaderFrequencyToVkStageBit(shader::ShaderFrequency::Compute), VK_SHADER_STAGE_COMPUTE_BIT);
    EXPECT_EQ(shaderFrequencyToVkStageBit(shader::ShaderFrequency::RayGen), VK_SHADER_STAGE_RAYGEN_BIT_KHR);
    EXPECT_EQ(shaderFrequencyToVkStageBit(shader::ShaderFrequency::RayMiss), VK_SHADER_STAGE_MISS_BIT_KHR);
    EXPECT_EQ(shaderFrequencyToVkStageBit(shader::ShaderFrequency::RayHitGroup), VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
    EXPECT_EQ(shaderFrequencyToVkStageBit(shader::ShaderFrequency::RayCallable), VK_SHADER_STAGE_CALLABLE_BIT_KHR);

    // Work-graph and out-of-range frequencies return VK_SHADER_STAGE_ALL.
    EXPECT_EQ(shaderFrequencyToVkStageBit(shader::ShaderFrequency::WorkGraphRoot), VK_SHADER_STAGE_ALL);
    EXPECT_EQ(shaderFrequencyToVkStageBit(shader::ShaderFrequency::WorkGraphComputeNode), VK_SHADER_STAGE_ALL);
    EXPECT_EQ(shaderFrequencyToVkStageBit(static_cast<shader::ShaderFrequency>(255u)), VK_SHADER_STAGE_ALL);

    // Reverse lookup
    EXPECT_EQ(vkStageBitToShaderFrequency(VK_SHADER_STAGE_VERTEX_BIT), shader::ShaderFrequency::Vertex);
    EXPECT_EQ(vkStageBitToShaderFrequency(VK_SHADER_STAGE_FRAGMENT_BIT), shader::ShaderFrequency::Pixel);
    EXPECT_EQ(vkStageBitToShaderFrequency(VK_SHADER_STAGE_MESH_BIT_EXT), shader::ShaderFrequency::Mesh);
    EXPECT_EQ(vkStageBitToShaderFrequency(VK_SHADER_STAGE_TASK_BIT_EXT), shader::ShaderFrequency::Amplification);
    EXPECT_EQ(vkStageBitToShaderFrequency(VK_SHADER_STAGE_GEOMETRY_BIT), shader::ShaderFrequency::Geometry);
    EXPECT_EQ(vkStageBitToShaderFrequency(VK_SHADER_STAGE_COMPUTE_BIT), shader::ShaderFrequency::Compute);
    EXPECT_EQ(vkStageBitToShaderFrequency(VK_SHADER_STAGE_RAYGEN_BIT_KHR), shader::ShaderFrequency::RayGen);
    EXPECT_EQ(vkStageBitToShaderFrequency(VK_SHADER_STAGE_MISS_BIT_KHR), shader::ShaderFrequency::RayMiss);
    EXPECT_EQ(vkStageBitToShaderFrequency(VK_SHADER_STAGE_CALLABLE_BIT_KHR), shader::ShaderFrequency::RayCallable);
    EXPECT_EQ(vkStageBitToShaderFrequency(VK_SHADER_STAGE_ANY_HIT_BIT_KHR), shader::ShaderFrequency::RayHitGroup);
    EXPECT_EQ(vkStageBitToShaderFrequency(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR), shader::ShaderFrequency::RayHitGroup);
    EXPECT_EQ(vkStageBitToShaderFrequency(VK_SHADER_STAGE_INTERSECTION_BIT_KHR), shader::ShaderFrequency::RayHitGroup);

    EXPECT_EQ(
        vkStageBitToShaderFrequency(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT),
        static_cast<shader::ShaderFrequency>(shader::kShaderFrequencyCount)
    );
    EXPECT_EQ(
        vkStageBitToShaderFrequency(VK_SHADER_STAGE_ALL),
        static_cast<shader::ShaderFrequency>(shader::kShaderFrequencyCount)
    );
}

TEST(VulkanShaderEnumsTest, CompactGraphicsStageAndBindPointStageCounts)
{
    EXPECT_EQ(gfxStageToVkStageBit(shader_stage::Stage::Vertex), VK_SHADER_STAGE_VERTEX_BIT);
    EXPECT_EQ(gfxStageToVkStageBit(shader_stage::Stage::Pixel), VK_SHADER_STAGE_FRAGMENT_BIT);
    EXPECT_EQ(gfxStageToVkStageBit(shader_stage::Stage::Geometry), VK_SHADER_STAGE_GEOMETRY_BIT);
    EXPECT_EQ(gfxStageToVkStageBit(shader_stage::Stage::Mesh), VK_SHADER_STAGE_MESH_BIT_EXT);
    EXPECT_EQ(gfxStageToVkStageBit(shader_stage::Stage::Task), VK_SHADER_STAGE_TASK_BIT_EXT);
    EXPECT_EQ(gfxStageToVkStageBit(shader_stage::Stage::NumGraphicsStages), VK_SHADER_STAGE_ALL);
    EXPECT_EQ(gfxStageToVkStageBit(shader_stage::Stage::Invalid), VK_SHADER_STAGE_ALL);

    EXPECT_EQ(getNumStagesForBindPoint(VK_PIPELINE_BIND_POINT_GRAPHICS), 5);
    EXPECT_EQ(getNumStagesForBindPoint(VK_PIPELINE_BIND_POINT_COMPUTE), 1);
    EXPECT_EQ(getNumStagesForBindPoint(VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR), 4);
    EXPECT_EQ(getNumStagesForBindPoint(VK_PIPELINE_BIND_POINT_MAX_ENUM), 0);
}

}   // namespace gp::rhi::vulkan::tests
