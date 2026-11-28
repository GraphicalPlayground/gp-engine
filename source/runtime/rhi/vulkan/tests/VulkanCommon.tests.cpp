// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "VulkanCommon.hpp"
#include "CoreMinimal.hpp"
#include "memory/Memory.hpp"
#include "shaders/ShaderEnums.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

TEST(VulkanCommonTest, ZeroVulkanStructInPlaceAndReturningOverloads)
{
    // In-place overload zeroes all bytes after sType, including pNext and flags.
    {
        VkBufferCreateInfo info{};
        gp::memory::setMemory(&info, 0xAB, sizeof(info));

        zeroVulkanStruct(info, VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO);

        EXPECT_EQ(info.sType, VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO);
        EXPECT_EQ(info.pNext, nullptr);
        EXPECT_EQ(info.flags, 0u);
        EXPECT_EQ(info.size, 0u);
        EXPECT_EQ(info.usage, 0u);
        EXPECT_EQ(info.sharingMode, VK_SHARING_MODE_EXCLUSIVE);
        EXPECT_EQ(info.queueFamilyIndexCount, 0u);
        EXPECT_EQ(info.pQueueFamilyIndices, nullptr);

        const gp::UInt8* const tailBytes = reinterpret_cast<const gp::UInt8*>(&info) + sizeof(info.sType);
        EXPECT_TRUE(gp::memory::isMemoryZeroed(tailBytes, sizeof(info) - sizeof(info.sType)));
    }

    // Returning factory overload produces an identically zero-initialized struct with sType populated.
    {
        const VkImageCreateInfo imageInfo = zeroVulkanStruct<VkImageCreateInfo>(VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO);

        EXPECT_EQ(imageInfo.sType, VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO);
        EXPECT_EQ(imageInfo.pNext, nullptr);
        EXPECT_EQ(imageInfo.flags, 0u);
        EXPECT_EQ(imageInfo.extent.width, 0u);
        EXPECT_EQ(imageInfo.mipLevels, 0u);
        EXPECT_EQ(imageInfo.arrayLayers, 0u);

        const gp::UInt8* const tailBytes = reinterpret_cast<const gp::UInt8*>(&imageInfo) + sizeof(imageInfo.sType);
        EXPECT_TRUE(gp::memory::isMemoryZeroed(tailBytes, sizeof(imageInfo) - sizeof(imageInfo.sType)));
    }

    // Raw 32-bit integer structure type overload works without requiring VkStructureType directly.
    {
        VkSamplerCreateInfo samplerInfo{};
        gp::memory::setMemory(&samplerInfo, 0xCD, sizeof(samplerInfo));
        zeroVulkanStruct(samplerInfo, static_cast<gp::Int32>(VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO));
        EXPECT_EQ(samplerInfo.sType, VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO);
        EXPECT_EQ(samplerInfo.pNext, nullptr);
    }
}

TEST(VulkanCommonTest, AddToPNextCoversAllConstAndMutablePointerCombinations)
{
    // 1. (void*, void*): mutable root + mutable extensions.
    {
        VkPhysicalDeviceFeatures2 root =
            zeroVulkanStruct<VkPhysicalDeviceFeatures2>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2);
        VkPhysicalDeviceVulkan11Features feat11 =
            zeroVulkanStruct<VkPhysicalDeviceVulkan11Features>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES);
        VkPhysicalDeviceVulkan12Features feat12 =
            zeroVulkanStruct<VkPhysicalDeviceVulkan12Features>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES);

        addToPNext(root, feat11);
        EXPECT_EQ(root.pNext, &feat11);
        EXPECT_EQ(feat11.pNext, nullptr);

        addToPNext(root, feat12);
        EXPECT_EQ(root.pNext, &feat12);
        EXPECT_EQ(feat12.pNext, &feat11);
        EXPECT_EQ(feat11.pNext, nullptr);
    }

    // 2. (const void*, void*): const root + mutable extensions.
    {
        VkDeviceCreateInfo createInfo = zeroVulkanStruct<VkDeviceCreateInfo>(VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO);
        VkPhysicalDeviceVulkan13Features feat13 =
            zeroVulkanStruct<VkPhysicalDeviceVulkan13Features>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES);
        VkPhysicalDeviceDescriptorIndexingFeatures descIdx =
            zeroVulkanStruct<VkPhysicalDeviceDescriptorIndexingFeatures>(
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES
            );

        addToPNext(createInfo, feat13);
        addToPNext(createInfo, descIdx);

        EXPECT_EQ(createInfo.pNext, &descIdx);
        EXPECT_EQ(descIdx.pNext, &feat13);
        EXPECT_EQ(feat13.pNext, nullptr);
    }

    // 3. (const void*, const void*): const root + const extensions.
    {
        VkDeviceCreateInfo createInfo = zeroVulkanStruct<VkDeviceCreateInfo>(VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO);
        VkDevicePrivateDataCreateInfo privA =
            zeroVulkanStruct<VkDevicePrivateDataCreateInfo>(VK_STRUCTURE_TYPE_DEVICE_PRIVATE_DATA_CREATE_INFO);
        VkDevicePrivateDataCreateInfo privB =
            zeroVulkanStruct<VkDevicePrivateDataCreateInfo>(VK_STRUCTURE_TYPE_DEVICE_PRIVATE_DATA_CREATE_INFO);

        addToPNext(createInfo, privA);
        addToPNext(createInfo, privB);

        EXPECT_EQ(createInfo.pNext, &privB);
        EXPECT_EQ(privB.pNext, &privA);
        EXPECT_EQ(privA.pNext, nullptr);
    }

    // 4. (void*, const void*): mutable root + const extension.
    {
        VkPhysicalDeviceFeatures2 root =
            zeroVulkanStruct<VkPhysicalDeviceFeatures2>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2);
        VkDevicePrivateDataCreateInfo privExt =
            zeroVulkanStruct<VkDevicePrivateDataCreateInfo>(VK_STRUCTURE_TYPE_DEVICE_PRIVATE_DATA_CREATE_INFO);

        addToPNext(root, privExt);

        EXPECT_EQ(root.pNext, &privExt);
        EXPECT_EQ(privExt.pNext, nullptr);
    }
}

TEST(VulkanCommonTest, CommonDefinesLogExportAndShaderStageMappings)
{
    EXPECT_EQ(GP_VULKAN_SUPPORTS_GEOMETRY_SHADERS, GP_PLATFORM_SUPPORTS_GEOMETRY_SHADERS);
    EXPECT_EQ(GP_VULKAN_ENABLE_BINDING_DEBUG_NAMES, 0);
    EXPECT_EQ(gp::LogVulkan.getName(), "LogVulkan");

    EXPECT_EQ(shader_stage::kMaxActiveGraphicStages, 3u);
    EXPECT_EQ(static_cast<gp::Int32>(shader_stage::Stage::NumGraphicsStages), 5);
    EXPECT_EQ(static_cast<gp::Int32>(shader_stage::Stage::NumRayTracingStages), 4);
    EXPECT_EQ(static_cast<gp::Int32>(shader_stage::Stage::NumComputeStages), 1);
    EXPECT_EQ(static_cast<gp::Int32>(shader_stage::Stage::MaxNumStages), 6);
    EXPECT_EQ(static_cast<gp::Int32>(shader_stage::Stage::Invalid), -1);

    // Verify intentional overlapping stage indices across mutually exclusive pipeline families.
    EXPECT_EQ(static_cast<gp::Int32>(shader_stage::Stage::Vertex), 0);
    EXPECT_EQ(static_cast<gp::Int32>(shader_stage::Stage::RayGen), 0);
    EXPECT_EQ(static_cast<gp::Int32>(shader_stage::Stage::Compute), 0);
    EXPECT_EQ(static_cast<gp::Int32>(shader_stage::Stage::Pixel), 1);
    EXPECT_EQ(static_cast<gp::Int32>(shader_stage::Stage::RayMiss), 1);
    EXPECT_EQ(static_cast<gp::Int32>(shader_stage::Stage::Geometry), 2);
    EXPECT_EQ(static_cast<gp::Int32>(shader_stage::Stage::RayHitGroup), 2);
    EXPECT_EQ(static_cast<gp::Int32>(shader_stage::Stage::Mesh), 3);
    EXPECT_EQ(static_cast<gp::Int32>(shader_stage::Stage::RayCallable), 3);
    EXPECT_EQ(static_cast<gp::Int32>(shader_stage::Stage::Task), 4);

    EXPECT_EQ(shader_stage::getStageForFrequency(shader::ShaderFrequency::Vertex), shader_stage::Stage::Vertex);
    EXPECT_EQ(shader_stage::getStageForFrequency(shader::ShaderFrequency::Pixel), shader_stage::Stage::Pixel);
    EXPECT_EQ(shader_stage::getStageForFrequency(shader::ShaderFrequency::Geometry), shader_stage::Stage::Geometry);
    EXPECT_EQ(shader_stage::getStageForFrequency(shader::ShaderFrequency::Mesh), shader_stage::Stage::Mesh);
    EXPECT_EQ(shader_stage::getStageForFrequency(shader::ShaderFrequency::Amplification), shader_stage::Stage::Task);
    EXPECT_EQ(shader_stage::getStageForFrequency(shader::ShaderFrequency::Compute), shader_stage::Stage::Compute);
    EXPECT_EQ(shader_stage::getStageForFrequency(shader::ShaderFrequency::RayGen), shader_stage::Stage::RayGen);
    EXPECT_EQ(shader_stage::getStageForFrequency(shader::ShaderFrequency::RayMiss), shader_stage::Stage::RayMiss);
    EXPECT_EQ(
        shader_stage::getStageForFrequency(shader::ShaderFrequency::RayHitGroup), shader_stage::Stage::RayHitGroup
    );
    EXPECT_EQ(
        shader_stage::getStageForFrequency(shader::ShaderFrequency::RayCallable), shader_stage::Stage::RayCallable
    );

    EXPECT_EQ(shader_stage::getFrequencyForGfxStage(shader_stage::Stage::Vertex), shader::ShaderFrequency::Vertex);
    EXPECT_EQ(shader_stage::getFrequencyForGfxStage(shader_stage::Stage::Pixel), shader::ShaderFrequency::Pixel);
    EXPECT_EQ(shader_stage::getFrequencyForGfxStage(shader_stage::Stage::Geometry), shader::ShaderFrequency::Geometry);
    EXPECT_EQ(shader_stage::getFrequencyForGfxStage(shader_stage::Stage::Mesh), shader::ShaderFrequency::Mesh);
    EXPECT_EQ(shader_stage::getFrequencyForGfxStage(shader_stage::Stage::Task), shader::ShaderFrequency::Amplification);
}

TEST(VulkanCommonTest, BindlessDescriptorSetsAndUniformBufferOffsets)
{
    EXPECT_EQ(bindless::kMaxUniformBuffersPerStage, 16u);
    EXPECT_EQ(bindless::kMaxUniformBuffersTotal, 32u);

    EXPECT_EQ(static_cast<gp::UInt32>(bindless::DescriptorSet::BindlessSamplerSet), 0u);
    EXPECT_EQ(static_cast<gp::UInt32>(bindless::DescriptorSet::BindlessResourceSet), 1u);
    EXPECT_EQ(bindless::DescriptorSet::BindlessStorageBufferSet, bindless::DescriptorSet::BindlessResourceSet);
    EXPECT_EQ(bindless::DescriptorSet::BindlessUniformBufferSet, bindless::DescriptorSet::BindlessResourceSet);
    EXPECT_EQ(bindless::DescriptorSet::BindlessStorageImageSet, bindless::DescriptorSet::BindlessResourceSet);
    EXPECT_EQ(bindless::DescriptorSet::BindlessSampledImageSet, bindless::DescriptorSet::BindlessResourceSet);
    EXPECT_EQ(bindless::DescriptorSet::BindlessStorageTexelBufferSet, bindless::DescriptorSet::BindlessResourceSet);
    EXPECT_EQ(bindless::DescriptorSet::BindlessUniformTexelBufferSet, bindless::DescriptorSet::BindlessResourceSet);
    EXPECT_EQ(bindless::DescriptorSet::BindlessAccelerationStructureSet, bindless::DescriptorSet::BindlessResourceSet);
    EXPECT_EQ(static_cast<gp::UInt32>(bindless::DescriptorSet::NumBindlessSets), 2u);
    EXPECT_EQ(static_cast<gp::UInt32>(bindless::DescriptorSet::BindlessSingleUseUniformBufferSet), 2u);
    EXPECT_EQ(static_cast<gp::UInt32>(bindless::DescriptorSet::MaxNumSets), 3u);

    EXPECT_EQ(bindless::getOffsetForFrequency(shader::ShaderFrequency::Pixel), 0);
    EXPECT_EQ(bindless::getOffsetForFrequency(shader::ShaderFrequency::Vertex), 16);
    EXPECT_EQ(bindless::getOffsetForFrequency(shader::ShaderFrequency::Geometry), -1);
    EXPECT_EQ(bindless::getOffsetForFrequency(shader::ShaderFrequency::Mesh), 16);
    EXPECT_EQ(bindless::getOffsetForFrequency(shader::ShaderFrequency::Amplification), -1);
    EXPECT_EQ(bindless::getOffsetForFrequency(shader::ShaderFrequency::Compute), 0);
    EXPECT_EQ(bindless::getOffsetForFrequency(shader::ShaderFrequency::RayGen), 0);
    EXPECT_EQ(bindless::getOffsetForFrequency(shader::ShaderFrequency::RayMiss), 0);
    EXPECT_EQ(bindless::getOffsetForFrequency(shader::ShaderFrequency::RayHitGroup), 0);
    EXPECT_EQ(bindless::getOffsetForFrequency(shader::ShaderFrequency::RayCallable), 0);
}

}   // namespace gp::rhi::vulkan::tests
