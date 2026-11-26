// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "descriptors/VulkanDescriptorSetsLayout.hpp"
#include "../resources/VulkanResourceTestFakes.hpp"
#include "containers/arrays/Vector.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "descriptors/VulkanDescriptorSetLayoutCache.hpp"
#include "descriptors/VulkanDescriptorSetsEnums.hpp"
#include "descriptors/VulkanDescriptorSetsLayoutInfo.hpp"
#include "descriptors/VulkanDescriptorSetsTypes.hpp"
#include "descriptors/VulkanDescriptorSettings.hpp"
#include "descriptors/VulkanPipelineLayout.hpp"
#include "device/DeviceEnums.hpp"
#include "device/VulkanDevice.hpp"
#include "shaders/VulkanShaderHeader.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>
#include <initializer_list>

namespace gp::rhi::vulkan::tests
{

namespace
{

/// @brief Graphics stage index of the vertex shader.
constexpr gp::USize kVertexStage = 0u;

/// @brief Graphics stage index of the pixel shader.
constexpr gp::USize kPixelStage = 1u;

/// @brief Builds a shader header from its binding types.
/// @param[in] types Descriptor type of each binding, in binding order.
/// @param[in] numBoundUniformBuffers Number of leading uniform buffers bound as buffers.
/// @param[in] packedGlobalsSize Non-zero when binding 0 is the packed globals buffer.
/// @return Shader header.
[[nodiscard]] VulkanShaderHeader makeHeader(
    const std::initializer_list<VkDescriptorType> types,
    const gp::UInt32 numBoundUniformBuffers = 0u,
    const gp::UInt32 packedGlobalsSize = 0u
)
{
    VulkanShaderHeader header{};
    for (const VkDescriptorType type: types)
    {
        header.bindings.pushBack(VulkanShaderBindingInfo{ static_cast<gp::UInt32>(type) });
    }
    header.numBoundUniformBuffers = numBoundUniformBuffers;
    header.packedGlobalsSize = packedGlobalsSize;
    return header;
}

/// @brief Returns limits generous enough for every test layout.
/// @param[in] maxDynamicUniformBuffers Dynamic uniform buffer limit.
/// @return Device limits.
[[nodiscard]] VkPhysicalDeviceLimits makeLimits(const gp::UInt32 maxDynamicUniformBuffers = 8u)
{
    VkPhysicalDeviceLimits limits{};
    limits.maxDescriptorSetSamplers = 16u;
    limits.maxDescriptorSetUniformBuffers = 16u;
    limits.maxDescriptorSetUniformBuffersDynamic = maxDynamicUniformBuffers;
    limits.maxDescriptorSetStorageBuffers = 16u;
    limits.maxDescriptorSetStorageBuffersDynamic = 4u;
    limits.maxDescriptorSetSampledImages = 16u;
    limits.maxDescriptorSetStorageImages = 16u;
    limits.maxDescriptorSetInputAttachments = 4u;
    return limits;
}

/// @brief Layout info exposing nothing more; named for readability.
using LayoutInfo = VulkanDescriptorSetsLayoutInfo;

/// @brief Finalizes a graphics layout from a vertex and a pixel header.
/// @param[out] outInfo Layout info to finalize.
/// @param[in] vertex Vertex header, or null.
/// @param[in] pixel Pixel header, or null.
/// @param[in] mode Dynamic uniform buffer mode.
/// @param[in] limits Device limits.
/// @return Result of `finalizeBindings()`.
bool finalizeGraphics(
    LayoutInfo& outInfo,
    const VulkanShaderHeader* const vertex,
    const VulkanShaderHeader* const pixel,
    const VulkanDynamicUniformBufferMode mode,
    const VkPhysicalDeviceLimits& limits = makeLimits()
)
{
    VulkanUniformBufferGatherInfo gatherInfo{};
    gatherInfo.codeHeaders[kVertexStage] = vertex;
    gatherInfo.codeHeaders[kPixelStage] = pixel;
    return outInfo.finalizeBindings(VK_PIPELINE_BIND_POINT_GRAPHICS, limits, gatherInfo, false, mode);
}

/// @brief Live fixture compiling layouts on a real device.
class VulkanDescriptorSetsLayoutLiveTest : public VulkanResourceLiveTest
{};

}   // namespace

static_assert(getVulkanDescriptorTypeSlot(VK_DESCRIPTOR_TYPE_SAMPLER) == 0u);
static_assert(getVulkanDescriptorTypeSlot(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT) == 10u);
static_assert(getVulkanDescriptorTypeSlot(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR) == 11u);
static_assert(getVulkanDescriptorTypeSlot(VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK) == kVulkanInvalidDescriptorTypeSlot);
static_assert(getVulkanDescriptorTypeForSlot(11u) == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR);
static_assert(sizeof(VulkanHashableDescriptorInfo) == 12u);

TEST(VulkanDescriptorTypeSlotTest, EverySlotMapsBackToItsType)
{
    for (gp::USize slot = 0u; slot < kVulkanNumDescriptorTypeSlots; ++slot)
    {
        EXPECT_EQ(getVulkanDescriptorTypeSlot(getVulkanDescriptorTypeForSlot(slot)), slot);
    }
}

TEST(VulkanDescriptorSettingsTest, DefaultsMatchTheConsoleVariables)
{
    EXPECT_EQ(getVulkanDynamicUniformBufferMode(), VulkanDynamicUniformBufferMode::All);
    EXPECT_EQ(getVulkanNumFramesToWaitForDescriptorDelete(), 2u);
}

TEST(VulkanDescriptorSetsLayoutInfoTest, ModeNoneKeepsStaticUniformBuffers)
{
    const VulkanShaderHeader vertex =
        makeHeader({ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE }, 1u, 16u);
    const VulkanShaderHeader pixel = makeHeader({ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_DESCRIPTOR_TYPE_SAMPLER }, 1u);
    LayoutInfo info;

    ASSERT_TRUE(finalizeGraphics(info, &vertex, &pixel, VulkanDynamicUniformBufferMode::None));

    EXPECT_EQ(info.getTypesUsed(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER), 2u);
    EXPECT_EQ(info.getTypesUsed(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC), 0u);
    EXPECT_EQ(info.getTypesUsed(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE), 1u);
    EXPECT_EQ(info.getTypesUsed(VK_DESCRIPTOR_TYPE_SAMPLER), 1u);
    EXPECT_EQ(info.getBindPoint(), VK_PIPELINE_BIND_POINT_GRAPHICS);
    ASSERT_EQ(info.getLayouts().size(), 2u);

    const VkDescriptorSetLayoutBinding& pixelSampler = info.getLayouts()[kPixelStage].layoutBindings[1];
    EXPECT_EQ(pixelSampler.binding, 1u);
    EXPECT_EQ(pixelSampler.descriptorType, VK_DESCRIPTOR_TYPE_SAMPLER);
    EXPECT_EQ(pixelSampler.stageFlags, static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_FRAGMENT_BIT));
    EXPECT_EQ(
        info.getLayouts()[kVertexStage].layoutBindings[0].stageFlags,
        static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_VERTEX_BIT)
    );
}

TEST(VulkanDescriptorSetsLayoutInfoTest, PackedGlobalsModePromotesOnlyThePackedBuffer)
{
    const VulkanShaderHeader vertex =
        makeHeader({ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER }, 2u, 16u);
    LayoutInfo info;

    ASSERT_TRUE(finalizeGraphics(info, &vertex, nullptr, VulkanDynamicUniformBufferMode::PackedGlobals));

    const LayoutInfo::StageInfo& stage = info.getStageInfo(kVertexStage);
    ASSERT_EQ(stage.types.size(), 2u);
    EXPECT_EQ(stage.types[0], VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC);
    EXPECT_EQ(stage.types[1], VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    EXPECT_EQ(stage.packedGlobalsSize, 16u);
    EXPECT_EQ(stage.numBufferInfos, 2u);
}

TEST(VulkanDescriptorSetsLayoutInfoTest, AllModePromotesUpToTheDeviceLimit)
{
    const VulkanShaderHeader vertex = makeHeader(
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER }, 3u
    );
    LayoutInfo info;

    ASSERT_TRUE(finalizeGraphics(info, &vertex, nullptr, VulkanDynamicUniformBufferMode::All, makeLimits(2u)));

    EXPECT_EQ(info.getTypesUsed(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC), 2u);
    EXPECT_EQ(info.getTypesUsed(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER), 1u);
    EXPECT_EQ(info.getStageInfo(kVertexStage).types[2], VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
}

TEST(VulkanDescriptorSetsLayoutInfoTest, BindlessNeverPromotes)
{
    const VulkanShaderHeader vertex = makeHeader({ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER }, 1u, 16u);
    VulkanUniformBufferGatherInfo gatherInfo{};
    gatherInfo.codeHeaders[kVertexStage] = &vertex;
    LayoutInfo info;

    ASSERT_TRUE(info.finalizeBindings(
        VK_PIPELINE_BIND_POINT_GRAPHICS, makeLimits(), gatherInfo, true, VulkanDynamicUniformBufferMode::All
    ));

    EXPECT_EQ(info.getTypesUsed(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC), 0u);
}

TEST(VulkanDescriptorSetsLayoutInfoTest, ResourceOnlyUniformBuffersAreSkipped)
{
    const VulkanShaderHeader vertex =
        makeHeader({ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER }, 1u);
    LayoutInfo info;

    ASSERT_TRUE(finalizeGraphics(info, &vertex, nullptr, VulkanDynamicUniformBufferMode::None));

    EXPECT_EQ(info.getStageInfo(kVertexStage).types.size(), 1u);
    EXPECT_EQ(info.getStageInfo(kVertexStage).numBoundUniformBuffers, 1u);
    EXPECT_EQ(info.getTypesUsed(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER), 1u);
}

TEST(VulkanDescriptorSetsLayoutInfoTest, SkippedBufferBeforeABindingIsRejected)
{
    const VulkanShaderHeader vertex =
        makeHeader({ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE }, 0u);
    LayoutInfo info;

    EXPECT_FALSE(finalizeGraphics(info, &vertex, nullptr, VulkanDynamicUniformBufferMode::None));
}

TEST(VulkanDescriptorSetsLayoutInfoTest, UnsupportedTypeIsRejected)
{
    const VulkanShaderHeader vertex = makeHeader({ VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK });
    LayoutInfo info;

    EXPECT_FALSE(finalizeGraphics(info, &vertex, nullptr, VulkanDynamicUniformBufferMode::None));
}

TEST(VulkanDescriptorSetsLayoutInfoTest, SecondFinalizeIsRejected)
{
    const VulkanShaderHeader vertex = makeHeader({ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE });
    LayoutInfo info;

    ASSERT_TRUE(finalizeGraphics(info, &vertex, nullptr, VulkanDynamicUniformBufferMode::None));
    EXPECT_FALSE(finalizeGraphics(info, &vertex, nullptr, VulkanDynamicUniformBufferMode::None));
}

TEST(VulkanDescriptorSetsLayoutInfoTest, RayTracingBindPointIsRejected)
{
    const VulkanShaderHeader header = makeHeader({ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE });
    VulkanUniformBufferGatherInfo gatherInfo{};
    gatherInfo.codeHeaders[0] = &header;
    LayoutInfo info;

    EXPECT_FALSE(info.finalizeBindings(
        VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, makeLimits(), gatherInfo, false, VulkanDynamicUniformBufferMode::None
    ));
}

TEST(VulkanDescriptorSetsLayoutInfoTest, PixelOnlyLayoutKeepsAnEmptyVertexSet)
{
    const VulkanShaderHeader pixel = makeHeader({ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT });
    LayoutInfo info;

    ASSERT_TRUE(finalizeGraphics(info, nullptr, &pixel, VulkanDynamicUniformBufferMode::None));

    ASSERT_EQ(info.getLayouts().size(), 2u);
    EXPECT_TRUE(info.getLayouts()[kVertexStage].layoutBindings.isEmpty());
    EXPECT_TRUE(info.getStageInfo(kVertexStage).isEmpty());
    EXPECT_TRUE(info.hasInputAttachments());
    EXPECT_EQ(info.getStageInfo(kPixelStage).numImageInfos, 1u);
}

TEST(VulkanDescriptorSetsLayoutInfoTest, ComputeAndGraphicsHashesDiffer)
{
    const VulkanShaderHeader header = makeHeader({ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER });
    VulkanUniformBufferGatherInfo gatherInfo{};
    gatherInfo.codeHeaders[0] = &header;
    LayoutInfo compute;
    LayoutInfo graphics;

    ASSERT_TRUE(compute.finalizeBindings(
        VK_PIPELINE_BIND_POINT_COMPUTE, makeLimits(), gatherInfo, false, VulkanDynamicUniformBufferMode::None
    ));
    ASSERT_TRUE(graphics.finalizeBindings(
        VK_PIPELINE_BIND_POINT_GRAPHICS, makeLimits(), gatherInfo, false, VulkanDynamicUniformBufferMode::None
    ));

    EXPECT_EQ(
        compute.getLayouts()[0].layoutBindings[0].stageFlags,
        static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_COMPUTE_BIT)
    );
    EXPECT_NE(compute.getHash(), graphics.getHash());
    EXPECT_FALSE(compute == graphics);
}

TEST(VulkanDescriptorSetsLayoutInfoTest, IdenticalLayoutsCompareEqual)
{
    const VulkanShaderHeader vertex = makeHeader({ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_DESCRIPTOR_TYPE_SAMPLER }, 1u);
    LayoutInfo first;
    LayoutInfo second;

    ASSERT_TRUE(finalizeGraphics(first, &vertex, nullptr, VulkanDynamicUniformBufferMode::All));
    ASSERT_TRUE(finalizeGraphics(second, &vertex, nullptr, VulkanDynamicUniformBufferMode::All));

    EXPECT_EQ(first.getHash(), second.getHash());
    EXPECT_TRUE(first == second);
    EXPECT_TRUE(first.getLayouts()[0] == second.getLayouts()[0]);
}

TEST(VulkanDescriptorSetsLayoutInfoTest, TypesUsageIdIsSharedByEqualTypeCounts)
{
    // Same counts in different stages: different layouts, same pool shape.
    const VulkanShaderHeader sampled = makeHeader({ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE });
    const VulkanShaderHeader storage = makeHeader({ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE });
    LayoutInfo vertexOnly;
    LayoutInfo pixelOnly;
    LayoutInfo different;

    ASSERT_TRUE(finalizeGraphics(vertexOnly, &sampled, nullptr, VulkanDynamicUniformBufferMode::None));
    ASSERT_TRUE(finalizeGraphics(pixelOnly, nullptr, &sampled, VulkanDynamicUniformBufferMode::None));
    ASSERT_TRUE(finalizeGraphics(different, &storage, nullptr, VulkanDynamicUniformBufferMode::None));

    EXPECT_NE(vertexOnly.getTypesUsageID(), ~0u);
    EXPECT_GT(vertexOnly.getTypesUsageID(), 0u);
    EXPECT_EQ(vertexOnly.getTypesUsageID(), pixelOnly.getTypesUsageID());
    EXPECT_NE(vertexOnly.getTypesUsageID(), different.getTypesUsageID());
    EXPECT_NE(vertexOnly.getHash(), pixelOnly.getHash());
}

TEST(VulkanDescriptorSetsLayoutValidationTest, LayoutWithinLimitsIsValid)
{
    VulkanDescriptorTypeCounts counts{};
    counts[getVulkanDescriptorTypeSlot(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE)] = 4u;
    counts[getVulkanDescriptorTypeSlot(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)] = 8u;

    EXPECT_TRUE(VulkanDescriptorSetsLayout::validateLimits(counts, makeLimits(), GpuVendorId::Nvidia, 0u));
}

TEST(VulkanDescriptorSetsLayoutValidationTest, ExceededLimitsAreReported)
{
    VulkanDescriptorTypeCounts samplers{};
    samplers[getVulkanDescriptorTypeSlot(VK_DESCRIPTOR_TYPE_SAMPLER)] = 10u;
    samplers[getVulkanDescriptorTypeSlot(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)] = 10u;
    VulkanDescriptorTypeCounts attachments{};
    attachments[getVulkanDescriptorTypeSlot(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT)] = 5u;

    EXPECT_FALSE(VulkanDescriptorSetsLayout::validateLimits(samplers, makeLimits(), GpuVendorId::Intel, 0u));
    EXPECT_FALSE(VulkanDescriptorSetsLayout::validateLimits(attachments, makeLimits(), GpuVendorId::Intel, 0u));
}

TEST(VulkanDescriptorSetsLayoutValidationTest, AmdIsExemptFromTheDynamicUniformBufferLimit)
{
    VulkanDescriptorTypeCounts counts{};
    counts[getVulkanDescriptorTypeSlot(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)] = 9u;

    EXPECT_FALSE(VulkanDescriptorSetsLayout::validateLimits(counts, makeLimits(), GpuVendorId::Nvidia, 0u));
    EXPECT_TRUE(VulkanDescriptorSetsLayout::validateLimits(counts, makeLimits(), GpuVendorId::Amd, 0u));
}

TEST(VulkanDescriptorSetsLayoutValidationTest, AccelerationStructuresNeedARayTracingLimit)
{
    VulkanDescriptorTypeCounts counts{};
    counts[getVulkanDescriptorTypeSlot(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)] = 1u;

    EXPECT_FALSE(VulkanDescriptorSetsLayout::validateLimits(counts, makeLimits(), GpuVendorId::Nvidia, 0u));
    EXPECT_TRUE(VulkanDescriptorSetsLayout::validateLimits(counts, makeLimits(), GpuVendorId::Nvidia, 1u));
}

TEST(VulkanDescriptorSetsLayoutOfflineTest, CompileFailsWithoutADevice)
{
    VulkanDevice offlineDevice(VK_NULL_HANDLE, VK_NULL_HANDLE, VK_API_VERSION_1_3, false);
    VulkanDescriptorSetLayoutCache cache(offlineDevice);
    VulkanPipelineLayout pipelineLayout(offlineDevice, true, false);
    const VulkanShaderHeader vertex = makeHeader({ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE });
    VulkanUniformBufferGatherInfo gatherInfo{};
    gatherInfo.codeHeaders[kVertexStage] = &vertex;

    ASSERT_TRUE(pipelineLayout.getDescriptorSetsLayout().finalizeBindings(
        VK_PIPELINE_BIND_POINT_GRAPHICS, makeLimits(), gatherInfo, false, VulkanDynamicUniformBufferMode::None
    ));

    EXPECT_FALSE(pipelineLayout.compile(cache));
    EXPECT_EQ(pipelineLayout.getPipelineLayout(), VK_NULL_HANDLE);
    EXPECT_EQ(cache.getNumLayouts(), 0u);
    EXPECT_TRUE(pipelineLayout.hasDescriptors());
    EXPECT_TRUE(pipelineLayout.isGfxLayout());
}

TEST(VulkanDescriptorSetsLayoutOfflineTest, BindlessPipelineLayoutIsNotCompiledYet)
{
    VulkanDevice offlineDevice(VK_NULL_HANDLE, VK_NULL_HANDLE, VK_API_VERSION_1_3, false);
    VulkanDescriptorSetLayoutCache cache(offlineDevice);
    VulkanPipelineLayout pipelineLayout(offlineDevice, false, true);

    EXPECT_FALSE(pipelineLayout.compile(cache));
    EXPECT_TRUE(pipelineLayout.usesBindless());
}

TEST_F(VulkanDescriptorSetsLayoutLiveTest, CacheDeduplicatesIdenticalSetLayouts)
{
    const VulkanShaderHeader vertex =
        makeHeader({ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE }, 1u);
    const VulkanShaderHeader pixel = makeHeader({ VK_DESCRIPTOR_TYPE_SAMPLER });
    VulkanDescriptorSetLayoutCache cache(*m_device);
    VulkanDescriptorSetsLayout first(*m_device);
    VulkanDescriptorSetsLayout second(*m_device);

    ASSERT_TRUE(finalizeGraphics(first, &vertex, &pixel, VulkanDynamicUniformBufferMode::All, m_device->getLimits()));
    ASSERT_TRUE(finalizeGraphics(second, &vertex, &pixel, VulkanDynamicUniformBufferMode::All, m_device->getLimits()));
    ASSERT_TRUE(first.compile(cache));
    ASSERT_TRUE(second.compile(cache));

    EXPECT_TRUE(first.isCompiled());
    EXPECT_EQ(cache.getNumLayouts(), 2u);
    ASSERT_EQ(first.getHandles().size(), 2u);
    EXPECT_EQ(first.getHandles()[0], second.getHandles()[0]);
    EXPECT_EQ(first.getHandles()[1], second.getHandles()[1]);
    EXPECT_NE(first.getHandles()[0], first.getHandles()[1]);
    EXPECT_NE(first.getHandleIds()[0], first.getHandleIds()[1]);
    EXPECT_EQ(first.getAllocateInfo().descriptorSetCount, 2u);
    EXPECT_EQ(first.getAllocateInfo().pSetLayouts, first.getHandles().data());
}

TEST_F(VulkanDescriptorSetsLayoutLiveTest, LayoutCompilesOnlyOnce)
{
    const VulkanShaderHeader header = makeHeader({ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER });
    VulkanUniformBufferGatherInfo gatherInfo{};
    gatherInfo.codeHeaders[0] = &header;
    VulkanDescriptorSetLayoutCache cache(*m_device);
    VulkanDescriptorSetsLayout layout(*m_device);

    ASSERT_TRUE(layout.finalizeBindings(
        VK_PIPELINE_BIND_POINT_COMPUTE, m_device->getLimits(), gatherInfo, false, VulkanDynamicUniformBufferMode::None
    ));
    ASSERT_TRUE(layout.compile(cache));
    EXPECT_FALSE(layout.compile(cache));
}

TEST_F(VulkanDescriptorSetsLayoutLiveTest, PipelineLayoutCompiles)
{
    const VulkanShaderHeader vertex = makeHeader({ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER }, 1u, 16u);
    const VulkanShaderHeader pixel = makeHeader({ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_DESCRIPTOR_TYPE_SAMPLER });
    VulkanUniformBufferGatherInfo gatherInfo{};
    gatherInfo.codeHeaders[kVertexStage] = &vertex;
    gatherInfo.codeHeaders[kPixelStage] = &pixel;
    VulkanDescriptorSetLayoutCache cache(*m_device);
    VulkanPipelineLayout pipelineLayout(*m_device, true, false);

    ASSERT_TRUE(pipelineLayout.getDescriptorSetsLayout().finalizeBindings(
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        m_device->getLimits(),
        gatherInfo,
        false,
        VulkanDynamicUniformBufferMode::PackedGlobals
    ));
    ASSERT_TRUE(pipelineLayout.compile(cache));

    EXPECT_NE(pipelineLayout.getPipelineLayout(), VK_NULL_HANDLE);
    EXPECT_EQ(pipelineLayout.getDescriptorSetLayoutHash(), pipelineLayout.getDescriptorSetsLayout().getHash());
    EXPECT_TRUE(pipelineLayout.getDescriptorSetsLayout().isCompiled());
}

}   // namespace gp::rhi::vulkan::tests
