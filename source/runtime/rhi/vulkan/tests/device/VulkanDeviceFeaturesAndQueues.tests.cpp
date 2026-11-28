// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "console/ConsoleEnums.hpp"
#include "console/IConsoleManager.hpp"
#include "console/IConsoleVariable.hpp"
#include "containers/arrays/Vector.hpp"
#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "device/VulkanPhysicalDeviceFeatures.hpp"
#include "device/VulkanPhysicalDeviceProperties.hpp"
#include "device/VulkanQueueFamilySelection.hpp"
#include "extensions/VulkanExtensionTypes.hpp"
#include "miscellaneous/CommandLine.hpp"
#include "VulkanCommon.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

namespace
{

gp::UInt32 countPNextChain(const void* head) noexcept
{
    gp::UInt32 count = 0u;
    const VkBaseInStructure* current = static_cast<const VkBaseInStructure*>(head);
    while (current != nullptr)
    {
        ++count;
        current = current->pNext;
    }
    return count;
}

bool pNextChainContains(const void* head, const VkStructureType targetType) noexcept
{
    const VkBaseInStructure* current = static_cast<const VkBaseInStructure*>(head);
    while (current != nullptr)
    {
        if (current->sType == targetType)
        {
            return true;
        }
        current = current->pNext;
    }
    return false;
}

VkQueueFamilyProperties makeQueueFamily(const VkQueueFlags flags, const gp::UInt32 count) noexcept
{
    VkQueueFamilyProperties props{};
    props.queueFlags = flags;
    props.queueCount = count;
    props.timestampValidBits = 64u;
    props.minImageTransferGranularity = { 1u, 1u, 1u };
    return props;
}

}   // namespace

TEST(VulkanDeviceFeaturesTest, RemoveUnusedFeaturesStripsUnusedAndHonorsCvars)
{
    VulkanPhysicalDeviceFeatures features;
    features.setApiVersion(VK_API_VERSION_1_4);

    features.getCore10Mutable().robustBufferAccess = VK_TRUE;
    features.getCore10Mutable().sparseBinding = VK_TRUE;
    features.getCore10Mutable().sparseResidencyBuffer = VK_TRUE;
    features.getCore10Mutable().sparseResidencyImage2D = VK_TRUE;
    features.getCore10Mutable().sparseResidencyAliased = VK_TRUE;
    features.getCore11Mutable().multiviewTessellationShader = VK_TRUE;
    features.getCore13Mutable().robustImageAccess = VK_TRUE;
    features.getCore13Mutable().inlineUniformBlock = VK_TRUE;
    features.getCore13Mutable().descriptorBindingInlineUniformBlockUpdateAfterBind = VK_TRUE;
    features.getCore13Mutable().privateData = VK_TRUE;
    features.getCore14Mutable().rectangularLines = VK_TRUE;
    features.getCore14Mutable().bresenhamLines = VK_TRUE;
    features.getCore14Mutable().smoothLines = VK_TRUE;
    features.getCore14Mutable().stippledRectangularLines = VK_TRUE;
    features.getCore14Mutable().stippledBresenhamLines = VK_TRUE;
    features.getCore14Mutable().stippledSmoothLines = VK_TRUE;

    features.removeUnusedFeatures();

    // Unused features are always stripped.
    EXPECT_EQ(features.getCore11().multiviewTessellationShader, VK_FALSE);
    EXPECT_EQ(features.getCore13().inlineUniformBlock, VK_FALSE);
    EXPECT_EQ(features.getCore13().descriptorBindingInlineUniformBlockUpdateAfterBind, VK_FALSE);
    EXPECT_EQ(features.getCore13().privateData, VK_FALSE);
    EXPECT_EQ(features.getCore14().rectangularLines, VK_FALSE);
    EXPECT_EQ(features.getCore14().bresenhamLines, VK_FALSE);
    EXPECT_EQ(features.getCore14().smoothLines, VK_FALSE);
    EXPECT_EQ(features.getCore14().stippledRectangularLines, VK_FALSE);
    EXPECT_EQ(features.getCore14().stippledBresenhamLines, VK_FALSE);
    EXPECT_EQ(features.getCore14().stippledSmoothLines, VK_FALSE);
    // Default CVars allow robust buffer access, image robustness, and sparse residency.
    EXPECT_EQ(features.getCore10().robustBufferAccess, VK_TRUE);
    EXPECT_EQ(features.getCore13().robustImageAccess, VK_TRUE);
    EXPECT_EQ(features.getCore10().sparseBinding, VK_TRUE);

    // Disabling CVars strips robustBufferAccess, robustImageAccess, and all sparse features.
    gp::console::IConsoleVariable* const robustCvar =
        gp::console::IConsoleManager::get().findConsoleVariable(gp::StringView("r.Vulkan.RobustBufferAccess"));
    gp::console::IConsoleVariable* const imgRobustCvar =
        gp::console::IConsoleManager::get().findConsoleVariable(gp::StringView("r.Vulkan.AllowImageRobustness"));
    gp::console::IConsoleVariable* const sparseCvar =
        gp::console::IConsoleManager::get().findConsoleVariable(gp::StringView("r.Vulkan.AllowSparseResidency"));

    ASSERT_NE(imgRobustCvar, nullptr);

    const gp::Int32 prevRobust = (robustCvar != nullptr) ? robustCvar->getInt() : 1;
    const gp::Int32 prevImgRobust = imgRobustCvar->getInt();
    const gp::Int32 prevSparse = (sparseCvar != nullptr) ? sparseCvar->getInt() : 1;

    if (robustCvar != nullptr)
    {
        static_cast<void>(robustCvar->set(0, gp::console::ConsoleVarSetBy::Console));
    }
    static_cast<void>(imgRobustCvar->set(0, gp::console::ConsoleVarSetBy::Console));
    if (sparseCvar != nullptr)
    {
        static_cast<void>(sparseCvar->set(0, gp::console::ConsoleVarSetBy::Console));
    }

    features.removeUnusedFeatures();
    if (robustCvar != nullptr)
    {
        EXPECT_EQ(features.getCore10().robustBufferAccess, VK_FALSE);
        static_cast<void>(robustCvar->set(prevRobust, gp::console::ConsoleVarSetBy::Console));
    }
    EXPECT_EQ(features.getCore13().robustImageAccess, VK_FALSE);
    static_cast<void>(imgRobustCvar->set(prevImgRobust, gp::console::ConsoleVarSetBy::Console));
    if (sparseCvar != nullptr)
    {
        EXPECT_EQ(features.getCore10().sparseBinding, VK_FALSE);
        EXPECT_EQ(features.getCore10().sparseResidencyBuffer, VK_FALSE);
        EXPECT_EQ(features.getCore10().sparseResidencyImage2D, VK_FALSE);
        EXPECT_EQ(features.getCore10().sparseResidencyAliased, VK_FALSE);
        static_cast<void>(sparseCvar->set(prevSparse, gp::console::ConsoleVarSetBy::Console));
    }

    // Null physical device query safely no-ops.
    features.query(VK_NULL_HANDLE, VK_API_VERSION_1_3);
    EXPECT_EQ(features.getApiVersion(), VK_API_VERSION_1_3);
}

TEST(VulkanDeviceFeaturesTest, AddToCreateDeviceChainsByApiVersionAndCoreStructsFlag)
{
    VulkanPhysicalDeviceFeatures features;

    // useCoreStructs = false uses legacy pEnabledFeatures.
    features.setApiVersion(VK_API_VERSION_1_4);
    VkDeviceCreateInfo legacyInfo = zeroVulkanStruct<VkDeviceCreateInfo>(VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO);
    features.addToCreateDevice(legacyInfo, false);
    EXPECT_EQ(legacyInfo.pEnabledFeatures, &features.getCore10());
    EXPECT_EQ(legacyInfo.pNext, nullptr);

    // API 1.1 with useCoreStructs = true still uses pEnabledFeatures.
    features.setApiVersion(VK_API_VERSION_1_1);
    VkDeviceCreateInfo info11 = zeroVulkanStruct<VkDeviceCreateInfo>(VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO);
    features.addToCreateDevice(info11, true);
    EXPECT_EQ(info11.pEnabledFeatures, &features.getCore10());
    EXPECT_EQ(info11.pNext, nullptr);

    // API 1.2 chains Core10, Core11, Core12 (3 structs).
    features.setApiVersion(VK_API_VERSION_1_2);
    VkDeviceCreateInfo info12 = zeroVulkanStruct<VkDeviceCreateInfo>(VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO);
    features.addToCreateDevice(info12, true);
    EXPECT_EQ(info12.pEnabledFeatures, nullptr);
    EXPECT_EQ(countPNextChain(info12.pNext), 3u);
    EXPECT_TRUE(pNextChainContains(info12.pNext, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2));
    EXPECT_TRUE(pNextChainContains(info12.pNext, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES));
    EXPECT_TRUE(pNextChainContains(info12.pNext, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES));
    EXPECT_FALSE(pNextChainContains(info12.pNext, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES));

    // API 1.3 chains Core10..Core13 (4 structs).
    features.setApiVersion(VK_API_VERSION_1_3);
    VkDeviceCreateInfo info13 = zeroVulkanStruct<VkDeviceCreateInfo>(VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO);
    features.addToCreateDevice(info13, true);
    EXPECT_EQ(countPNextChain(info13.pNext), 4u);
    EXPECT_TRUE(pNextChainContains(info13.pNext, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES));

    // API 1.4 chains Core10..Core14 (5 structs) while preserving pre-existing pNext nodes.
    features.setApiVersion(VK_API_VERSION_1_4);
    VkBaseOutStructure sentinel{};
    sentinel.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    sentinel.pNext = nullptr;
    VkDeviceCreateInfo info14 = zeroVulkanStruct<VkDeviceCreateInfo>(VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO);
    info14.pNext = &sentinel;
    features.addToCreateDevice(info14, true);
    EXPECT_EQ(countPNextChain(info14.pNext), 6u);
    EXPECT_TRUE(pNextChainContains(info14.pNext, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES));
    EXPECT_TRUE(pNextChainContains(info14.pNext, VK_STRUCTURE_TYPE_APPLICATION_INFO));
}

TEST(VulkanDevicePropertiesTest, MakeCoreFeaturesAndPropertiesGatesPointersByVersion)
{
    VulkanPhysicalDeviceFeatures features;
    VulkanPhysicalDeviceProperties props;

    props.setApiVersion(VK_API_VERSION_1_1);
    const VulkanCoreFeaturesAndProperties core11 = props.makeCoreFeaturesAndProperties(features);
    EXPECT_EQ(core11.features11, nullptr);
    EXPECT_EQ(core11.properties12, nullptr);

    props.setApiVersion(VK_API_VERSION_1_2);
    const VulkanCoreFeaturesAndProperties core12 = props.makeCoreFeaturesAndProperties(features);
    EXPECT_NE(core12.features11, nullptr);
    EXPECT_NE(core12.features12, nullptr);
    EXPECT_NE(core12.properties11, nullptr);
    EXPECT_NE(core12.properties12, nullptr);
    EXPECT_EQ(core12.features13, nullptr);
    EXPECT_EQ(core12.properties14, nullptr);

    props.setApiVersion(VK_API_VERSION_1_3);
    const VulkanCoreFeaturesAndProperties core13 = props.makeCoreFeaturesAndProperties(features);
    EXPECT_NE(core13.features13, nullptr);
    EXPECT_NE(core13.properties13, nullptr);
    EXPECT_EQ(core13.features14, nullptr);

    props.setApiVersion(VK_API_VERSION_1_4);
    const VulkanCoreFeaturesAndProperties core14 = props.makeCoreFeaturesAndProperties(features);
    EXPECT_NE(core14.features14, nullptr);
    EXPECT_NE(core14.properties14, nullptr);
}

TEST(VulkanQueueFamilySelectionTest, SelectsGraphicsAsyncComputeAndDedicatedTransfer)
{
    gp::Vector<VkQueueFamilyProperties> families;
    families.pushBack(makeQueueFamily(VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT, 4u));
    families.pushBack(makeQueueFamily(VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT, 2u));
    families.pushBack(makeQueueFamily(VK_QUEUE_TRANSFER_BIT, 1u));
    families.pushBack(makeQueueFamily(VK_QUEUE_TRANSFER_BIT, 0u));   // Zero-count family ignored.

    const gp::ConstVectorView<VkQueueFamilyProperties> view(families.data(), families.size());

    // Without parallel rendering + maintenance9, only graphics family is selected.
    VulkanQueueFamilySelectionConfig basicConfig{};
    basicConfig.allowAsyncCompute = true;
    basicConfig.allowTransferQueue = true;
    basicConfig.supportsParallelRendering = false;
    basicConfig.hasMaintenance9 = false;

    const VulkanQueueFamilyIndices basicIndices = selectQueueFamilies(view, basicConfig);
    EXPECT_EQ(basicIndices.graphicsFamily, 0);
    EXPECT_EQ(basicIndices.asyncComputeFamily, -1);
    EXPECT_EQ(basicIndices.transferFamily, -1);
    ASSERT_EQ(basicIndices.activeQueueFamilies.size(), 1u);
    EXPECT_EQ(basicIndices.activeQueueFamilies[0], 0u);

    // With parallel rendering + maintenance9, graphics, async compute, and dedicated transfer are all selected.
    VulkanQueueFamilySelectionConfig fullConfig{};
    fullConfig.allowAsyncCompute = true;
    fullConfig.allowTransferQueue = true;
    fullConfig.supportsParallelRendering = true;
    fullConfig.hasMaintenance9 = true;

    const VulkanQueueFamilyIndices fullIndices = selectQueueFamilies(view, fullConfig);
    EXPECT_EQ(fullIndices.graphicsFamily, 0);
    EXPECT_EQ(fullIndices.asyncComputeFamily, 1);
    EXPECT_EQ(fullIndices.transferFamily, 2);
    ASSERT_EQ(fullIndices.activeQueueFamilies.size(), 3u);

    // Build VkDeviceQueueCreateInfo and verify priority slices.
    gp::Vector<VkDeviceQueueCreateInfo> createInfos;
    gp::Vector<gp::Float32> priorities;
    buildDeviceQueueCreateInfos(view, fullIndices, createInfos, priorities);
    ASSERT_EQ(createInfos.size(), 3u);
    EXPECT_EQ(priorities.size(), 7u);   // 4 + 2 + 1
    EXPECT_EQ(createInfos[0].queueFamilyIndex, 0u);
    EXPECT_EQ(createInfos[0].queueCount, 4u);
    EXPECT_EQ(createInfos[0].pQueuePriorities, &priorities[0]);
    EXPECT_EQ(createInfos[1].queueFamilyIndex, 1u);
    EXPECT_EQ(createInfos[1].queueCount, 2u);
    EXPECT_EQ(createInfos[1].pQueuePriorities, &priorities[4]);
    EXPECT_EQ(createInfos[2].queueFamilyIndex, 2u);
    EXPECT_EQ(createInfos[2].queueCount, 1u);
    EXPECT_EQ(createInfos[2].pQueuePriorities, &priorities[6]);
    EXPECT_FLOAT_EQ(priorities[0], 1.0f);
    EXPECT_FLOAT_EQ(priorities[6], 1.0f);

    // -DisableAsyncCompute overrides forceAsyncCompute.
    fullConfig.forceAsyncCompute = true;
    fullConfig.disableAsyncCompute = true;
    const VulkanQueueFamilyIndices disabledAsync = selectQueueFamilies(view, fullConfig);
    EXPECT_EQ(disabledAsync.asyncComputeFamily, -1);
    EXPECT_EQ(disabledAsync.transferFamily, 2);

    // Verify CLI flag parsing in resolveQueueFamilySelectionConfig().
    gp::CommandLine::shutdown();
    gp::CommandLine::initialize("app.exe -ForceAsyncCompute -DisableAsyncCompute");
    const VulkanQueueFamilySelectionConfig cliConfig = resolveQueueFamilySelectionConfig(true, true);
    EXPECT_TRUE(cliConfig.forceAsyncCompute);
    EXPECT_TRUE(cliConfig.disableAsyncCompute);
    EXPECT_TRUE(cliConfig.supportsParallelRendering);
    EXPECT_TRUE(cliConfig.hasMaintenance9);
    gp::CommandLine::shutdown();
}

}   // namespace gp::rhi::vulkan::tests
