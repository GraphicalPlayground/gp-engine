// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "resources/VulkanUniformBufferData.hpp"
#include "../resources/VulkanUniformBufferTestLayouts.hpp"
#include "core/ResourceDeletionQueue.hpp"
#include "core/ResourceForward.hpp"
#include "CoreMinimal.hpp"                     // IWYU pragma: keep
#include "memory/pointers/RefCountedPtr.hpp"   // IWYU pragma: keep
#include "pipeline/PipelineForward.hpp"
#include "pipeline/SamplerState.hpp"
#include "resources/UniformBufferLayout.hpp"   // IWYU pragma: keep
#include "resources/UniformBufferLayoutDesc.hpp"
#include "shaders/ShaderEnums.hpp"
#include "views/DescriptorHandle.hpp"
#include <cstring>
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

TEST(VulkanUniformBufferDataTest, OnlyBindableMembersAreResources)
{
    EXPECT_TRUE(isVulkanUniformBufferResourceMember(shader::UniformBufferBaseType::Texture));
    EXPECT_TRUE(isVulkanUniformBufferResourceMember(shader::UniformBufferBaseType::SRV));
    EXPECT_TRUE(isVulkanUniformBufferResourceMember(shader::UniformBufferBaseType::UAV));
    EXPECT_TRUE(isVulkanUniformBufferResourceMember(shader::UniformBufferBaseType::Sampler));
    EXPECT_TRUE(isVulkanUniformBufferResourceMember(shader::UniformBufferBaseType::UniformBlock));

    EXPECT_FALSE(isVulkanUniformBufferResourceMember(shader::UniformBufferBaseType::Invalid));
    EXPECT_FALSE(isVulkanUniformBufferResourceMember(shader::UniformBufferBaseType::Float32));
    EXPECT_FALSE(isVulkanUniformBufferResourceMember(shader::UniformBufferBaseType::UInt32));
    EXPECT_FALSE(isVulkanUniformBufferResourceMember(shader::UniformBufferBaseType::NestedStruct));
    EXPECT_FALSE(isVulkanUniformBufferResourceMember(shader::UniformBufferBaseType::RenderTargetBindingSlots));
}

TEST(VulkanUniformBufferDataTest, ResourcePointersAreReadFromTheirMemberOffset)
{
    {
        const pipeline::SamplerStateRef sampler(new pipeline::SamplerState());
        TestUniformBufferContents contents{};
        contents.sampler = sampler.get();

        const resource::UniformBufferResource samplerMember{ kTestSamplerOffset,
                                                             shader::UniformBufferBaseType::Sampler };
        EXPECT_EQ(getVulkanUniformBufferResource(&contents, samplerMember), sampler.get());

        // Constant members and missing contents carry no resource.
        const resource::UniformBufferResource constantMember{ 0u, shader::UniformBufferBaseType::Float32 };
        EXPECT_EQ(getVulkanUniformBufferResource(&contents, constantMember), nullptr);
        EXPECT_EQ(getVulkanUniformBufferResource(nullptr, samplerMember), nullptr);

        // A null texture pointer reads back as null too.
        const resource::UniformBufferResource textureMember{ kTestSamplerOffset,
                                                             shader::UniformBufferBaseType::Texture };
        contents.sampler = nullptr;
        EXPECT_EQ(getVulkanUniformBufferResource(&contents, textureMember), nullptr);
    }
    resource::ResourceDeletionQueue::flushPendingDeletes(true);
}

TEST(VulkanUniformBufferDataTest, BindlessHandlesAreInvalidForNonBindlessResources)
{
    {
        const pipeline::SamplerStateRef sampler(new pipeline::SamplerState());
        TestUniformBufferContents contents{};
        contents.sampler = sampler.get();

        const resource::UniformBufferResource samplerMember{ kTestSamplerOffset,
                                                             shader::UniformBufferBaseType::Sampler };
        EXPECT_FALSE(getVulkanUniformBufferBindlessHandle(&contents, samplerMember).isValid());

        contents.sampler = nullptr;
        EXPECT_FALSE(getVulkanUniformBufferBindlessHandle(&contents, samplerMember).isValid());

        const resource::UniformBufferResource constantMember{ 0u, shader::UniformBufferBaseType::Float32 };
        EXPECT_FALSE(getVulkanUniformBufferBindlessHandle(&contents, constantMember).isValid());
    }
    resource::ResourceDeletionQueue::flushPendingDeletes(true);
}

TEST(VulkanUniformBufferDataTest, ConstantsAreCopiedVerbatim)
{
    {
        const resource::UniformBufferLayoutRef layout = createTestUniformBufferLayout(true, true);
        const pipeline::SamplerStateRef sampler(new pipeline::SamplerState());

        TestUniformBufferContents source{};
        source.constants[0] = 1.0f;
        source.constants[1] = 2.0f;
        source.constants[2] = 3.0f;
        source.constants[3] = 4.0f;
        source.sampler = sampler.get();
        source.padding = 0x0123456789ABCDEFull;

        // Without bindless patching, and with no bindless handles to patch, the bytes match the source.
        const bool bindlessModes[2] = { false, true };
        for (const bool allowBindless: bindlessModes)
        {
            TestUniformBufferContents destination{};
            updateVulkanUniformBufferConstants(&destination, &source, *layout, allowBindless);
            EXPECT_EQ(std::memcmp(&destination, &source, sizeof(source)), 0);
        }
    }
    resource::ResourceDeletionQueue::flushPendingDeletes(true);
}

TEST(VulkanUniformBufferDataTest, ValidationReportsMissingResources)
{
    {
        const resource::UniformBufferLayoutRef layout = createTestUniformBufferLayout(true, true);
        const pipeline::SamplerStateRef sampler(new pipeline::SamplerState());

        TestUniformBufferContents contents{};
        contents.sampler = sampler.get();
        EXPECT_TRUE(validateVulkanUniformBufferResources(*layout, &contents));

        contents.sampler = nullptr;
        EXPECT_FALSE(validateVulkanUniformBufferResources(*layout, &contents));

        // Layouts without resources always validate.
        const resource::UniformBufferLayoutRef constantsOnly = createTestUniformBufferLayout(true, false);
        EXPECT_TRUE(validateVulkanUniformBufferResources(*constantsOnly, &contents));
    }
    resource::ResourceDeletionQueue::flushPendingDeletes(true);
}

}   // namespace gp::rhi::vulkan::tests
