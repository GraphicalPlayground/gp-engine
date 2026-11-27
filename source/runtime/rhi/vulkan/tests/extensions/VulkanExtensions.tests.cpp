// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "console/IConsoleManager.hpp"
#include "console/IConsoleVariable.hpp"
#include "containers/arrays/Vector.hpp"      // IWYU pragma: keep
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "extensions/VulkanDeviceExtension.hpp"
#include "extensions/VulkanDeviceExtensions.hpp"
#include "extensions/VulkanExtensionBase.hpp"
#include "extensions/VulkanExtensionTypes.hpp"
#include "extensions/VulkanInstanceExtension.hpp"
#include "memory/Memory.hpp"
#include "memory/pointers/SharedPtr.hpp"
#include "memory/pointers/UniquePtr.hpp"
#include "miscellaneous/CommandLine.hpp"
#include "VulkanCommon.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <cstring>
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

namespace
{

VkExtensionProperties makeExtensionProperties(const char* const name, const gp::UInt32 specVersion = 1u) noexcept
{
    VkExtensionProperties props{};
    gp::memory::zeroMemory(&props, sizeof(props));
    const gp::USize length = std::strlen(name);
    const gp::USize copyLen =
        (length < VK_MAX_EXTENSION_NAME_SIZE - 1u) ? length : static_cast<gp::USize>(VK_MAX_EXTENSION_NAME_SIZE - 1u);
    gp::memory::copyMemory(props.extensionName, name, copyLen);
    props.specVersion = specVersion;
    return props;
}

class TestExternalDeviceExtension final : public VulkanExternalDeviceExtensionBase
{
public:
    TestExternalDeviceExtension(
        const char* const inName,
        const gp::UInt32 inPromotedVersion = kVulkanExtensionNotPromoted,
        const bool inPassFeatures = true,
        const bool inPassProperties = true
    ) noexcept
        : VulkanExternalDeviceExtensionBase(inName, inPromotedVersion)
        , m_passFeatures(inPassFeatures)
        , m_passProperties(inPassProperties)
    {
        m_featuresStruct =
            zeroVulkanStruct<VkPhysicalDeviceVulkan11Features>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES);
        m_propertiesStruct = zeroVulkanStruct<VkPhysicalDeviceVulkan11Properties>(
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES
        );
    }

    [[nodiscard]] virtual VkBaseOutStructure* getFeaturesStruct() noexcept override
    {
        return reinterpret_cast<VkBaseOutStructure*>(&m_featuresStruct);
    }

    [[nodiscard]] virtual bool featuresCallback() noexcept override
    {
        ++m_featuresCallbackCount;
        return m_passFeatures;
    }

    [[nodiscard]] virtual VkBaseOutStructure* getPropertiesStruct() noexcept override
    {
        return reinterpret_cast<VkBaseOutStructure*>(&m_propertiesStruct);
    }

    [[nodiscard]] virtual bool propertiesCallback() noexcept override
    {
        ++m_propertiesCallbackCount;
        return m_passProperties;
    }

    virtual void duplicateCallback() noexcept override
    {
        ++m_duplicateCallbackCount;
    }

    virtual void coreCallback(const VulkanCoreFeaturesAndProperties& coreFeaturesAndProperties) noexcept override
    {
        static_cast<void>(coreFeaturesAndProperties);
        ++m_coreCallbackCount;
    }

    gp::Int32 m_featuresCallbackCount{ 0 };
    gp::Int32 m_propertiesCallbackCount{ 0 };
    gp::Int32 m_duplicateCallbackCount{ 0 };
    gp::Int32 m_coreCallbackCount{ 0 };

private:
    bool m_passFeatures{ true };
    bool m_passProperties{ true };
    VkPhysicalDeviceVulkan11Features m_featuresStruct{};
    VkPhysicalDeviceVulkan11Properties m_propertiesStruct{};
};

}   // namespace

TEST(VulkanExtensionsTest, OptionalExtensionBitfieldsAndHelpers)
{
    OptionalVulkanInstanceExtensions instanceExts;
    EXPECT_EQ(instanceExts.hasKHRGetSurfaceCapabilities2, 0u);
    instanceExts.hasKHRGetSurfaceCapabilities2 = 1u;
    EXPECT_EQ(instanceExts.hasKHRGetSurfaceCapabilities2, 1u);

    OptionalVulkanDeviceExtensions deviceExts;
    EXPECT_FALSE(deviceExts.hasGPUCrashDumpExtensions());
    EXPECT_FALSE(deviceExts.hasRaytracingExtensions());
    EXPECT_FALSE(deviceExts.hasAnyExtendedDynamicState());

    deviceExts.hasAMDBufferMarker = 1u;
    EXPECT_TRUE(deviceExts.hasGPUCrashDumpExtensions());
    deviceExts.hasAMDBufferMarker = 0u;
    deviceExts.hasNVDiagnosticCheckpoints = 1u;
    EXPECT_TRUE(deviceExts.hasGPUCrashDumpExtensions());

    // Ray tracing requires acceleration structure, descriptor indexing, BDA, deferred host ops, SPIR-V 1.4,
    // float controls, and either (ray tracing pipeline + pipeline library) or ray query.
    deviceExts.hasAccelerationStructure = 1u;
    deviceExts.hasEXTDescriptorIndexing = 1u;
    deviceExts.hasBufferDeviceAddress = 1u;
    deviceExts.hasDeferredHostOperations = 1u;
    deviceExts.hasSPIRV_14 = 1u;
    deviceExts.hasShaderFloatControls = 1u;
    EXPECT_FALSE(deviceExts.hasRaytracingExtensions());

    deviceExts.hasRayTracingPipeline = 1u;
    EXPECT_FALSE(deviceExts.hasRaytracingExtensions());
    deviceExts.hasKHRPipelineLibrary = 1u;
    EXPECT_TRUE(deviceExts.hasRaytracingExtensions());

    deviceExts.hasRayTracingPipeline = 0u;
    deviceExts.hasKHRPipelineLibrary = 0u;
    deviceExts.hasRayQuery = 1u;
    EXPECT_TRUE(deviceExts.hasRaytracingExtensions());

    deviceExts.hasEXTExtendedDynamicState2 = 1u;
    EXPECT_TRUE(deviceExts.hasAnyExtendedDynamicState());
}

TEST(VulkanExtensionsTest, ExtensionBaseLifecycleAndPromotion)
{
    VulkanInstanceExtension autoExt(
        "VK_TEST_auto_ext",
        kVulkanExtensionEnabled,
        VK_API_VERSION_1_3,
        nullptr,
        VulkanExtensionActivation::AutoActivate
    );

    EXPECT_STREQ(autoExt.getExtensionName(), "VK_TEST_auto_ext");
    EXPECT_EQ(autoExt.getPromotedVersion(), VK_API_VERSION_1_3);
    EXPECT_TRUE(autoExt.isEnabled());
    EXPECT_TRUE(autoExt.isActivated());
    EXPECT_FALSE(autoExt.isSupported());
    EXPECT_FALSE(autoExt.isCore());
    EXPECT_FALSE(autoExt.usedAsExtension());
    EXPECT_FALSE(autoExt.usedAsCore());

    autoExt.setSupported();
    EXPECT_TRUE(autoExt.isSupported());
    EXPECT_TRUE(autoExt.usedAsExtension());
    EXPECT_FALSE(autoExt.usedAsCore());

    // Below promotion version: stays extension.
    EXPECT_FALSE(autoExt.setCore(VK_API_VERSION_1_2));
    EXPECT_TRUE(autoExt.usedAsExtension());
    EXPECT_FALSE(autoExt.usedAsCore());

    // At promotion version: transitions to core.
    EXPECT_TRUE(autoExt.setCore(VK_API_VERSION_1_3));
    EXPECT_TRUE(autoExt.isCore());
    EXPECT_TRUE(autoExt.isSupported());
    EXPECT_FALSE(autoExt.usedAsExtension());
    EXPECT_TRUE(autoExt.usedAsCore());

    // Manually activated extension stays inactive until setActivated() is called.
    VulkanInstanceExtension manualExt(
        "VK_TEST_manual_ext",
        kVulkanExtensionEnabled,
        kVulkanExtensionNotPromoted,
        nullptr,
        VulkanExtensionActivation::ManuallyActivate
    );
    manualExt.setSupported();
    EXPECT_FALSE(manualExt.isActivated());
    EXPECT_FALSE(manualExt.usedAsExtension());
    manualExt.setActivated();
    EXPECT_TRUE(manualExt.isActivated());
    EXPECT_TRUE(manualExt.usedAsExtension());

    // Compile-time disabled extension is never used even if supported and activated.
    VulkanInstanceExtension disabledExt("VK_TEST_disabled_ext", kVulkanExtensionDisabled);
    disabledExt.setSupported();
    disabledExt.setActivated();
    EXPECT_FALSE(disabledExt.isEnabled());
    EXPECT_FALSE(disabledExt.usedAsExtension());
    EXPECT_FALSE(disabledExt.usedAsCore());
}

TEST(VulkanExtensionsTest, FindExtensionAndFlagExtensionSupport)
{
    VulkanInstanceExtensionArray extensions;
    extensions.pushBack(
        gp::makeUnique<VulkanInstanceExtension>("VK_TEST_promoted_1_2", kVulkanExtensionEnabled, VK_API_VERSION_1_2)
    );
    extensions.pushBack(
        gp::makeUnique<VulkanInstanceExtension>(
            "VK_TEST_optional", kVulkanExtensionEnabled, kVulkanExtensionNotPromoted
        )
    );

    EXPECT_EQ(VulkanExtensionBase::findExtension(extensions, nullptr), -1);
    EXPECT_EQ(VulkanExtensionBase::findExtension(extensions, "VK_TEST_missing"), -1);
    EXPECT_EQ(VulkanExtensionBase::findExtension(extensions, "VK_TEST_promoted_1_2"), 0);
    EXPECT_EQ(VulkanExtensionBase::findExtension(extensions, "VK_TEST_optional"), 1);

    gp::Vector<VkExtensionProperties> driverExts;
    driverExts.pushBack(makeExtensionProperties("VK_TEST_optional"));
    driverExts.pushBack(makeExtensionProperties("VK_TEST_untracked"));

    // With useCoreStructs = false, promoted extension is not flagged as core.
    VulkanExtensionBase::flagExtensionSupport(
        gp::ConstVectorView<VkExtensionProperties>(driverExts.data(), driverExts.size()),
        extensions,
        VK_API_VERSION_1_3,
        false,
        "instance"
    );
    EXPECT_FALSE(extensions[0]->isCore());
    EXPECT_FALSE(extensions[0]->isSupported());
    EXPECT_TRUE(extensions[1]->isSupported());
    EXPECT_TRUE(extensions[1]->usedAsExtension());

    // Vulkan 1.1 does not have core feature/property structs: must stay non-core even when useCoreStructs = true.
    VulkanInstanceExtensionArray api11Exts;
    api11Exts.pushBack(
        gp::makeUnique<VulkanInstanceExtension>("VK_TEST_promoted_1_1", kVulkanExtensionEnabled, VK_API_VERSION_1_1)
    );
    VulkanExtensionBase::flagExtensionSupport(
        gp::ConstVectorView<VkExtensionProperties>(driverExts.data(), driverExts.size()),
        api11Exts,
        VK_API_VERSION_1_1,
        true,
        "instance"
    );
    EXPECT_FALSE(api11Exts[0]->isCore());
    EXPECT_FALSE(api11Exts[0]->usedAsCore());

    // With useCoreStructs = true at API 1.3, promoted extension becomes core.
    VulkanExtensionBase::flagExtensionSupport(
        gp::ConstVectorView<VkExtensionProperties>(driverExts.data(), driverExts.size()),
        extensions,
        VK_API_VERSION_1_3,
        true,
        "instance"
    );
    EXPECT_TRUE(extensions[0]->isCore());
    EXPECT_TRUE(extensions[0]->usedAsCore());
    EXPECT_FALSE(extensions[0]->usedAsExtension());

    // Verify OptionalVulkanDeviceExtensionProperties preserves VK_DRIVER_ID_MAX_ENUM default.
    const OptionalVulkanDeviceExtensionProperties extProps;
    EXPECT_EQ(extProps.driverId, VK_DRIVER_ID_MAX_ENUM);
}

TEST(VulkanExtensionsTest, InstanceExtensionDiscoveryAndExternalRegistration)
{
    VulkanInstanceExtension::clearExternalInstanceExtensions();

    const char* const externalNames[] = { VK_KHR_SURFACE_EXTENSION_NAME, "VK_EXT_custom_plugin_instance_ext" };
    VulkanInstanceExtension::addExternalInstanceExtensions(gp::ConstVectorView<const char*>(externalNames, 2u));
    EXPECT_EQ(VulkanInstanceExtension::getExternalInstanceExtensions().size(), 2u);

    gp::Vector<VkExtensionProperties> driverExts;
    driverExts.pushBack(makeExtensionProperties(VK_KHR_SURFACE_EXTENSION_NAME));
    driverExts.pushBack(makeExtensionProperties(VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME));
    driverExts.pushBack(makeExtensionProperties("VK_EXT_custom_plugin_instance_ext"));

    VulkanInstanceExtensionArray supported = VulkanInstanceExtension::getSupportedInstanceExtensions(
        VK_API_VERSION_1_3, gp::ConstVectorView<VkExtensionProperties>(driverExts.data(), driverExts.size())
    );

    const gp::Int32 surfaceIdx = VulkanExtensionBase::findExtension(supported, VK_KHR_SURFACE_EXTENSION_NAME);
    ASSERT_GE(surfaceIdx, 0);
    EXPECT_TRUE(supported[static_cast<gp::USize>(surfaceIdx)]->usedAsExtension());

    const gp::Int32 cap2Idx =
        VulkanExtensionBase::findExtension(supported, VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME);
    ASSERT_GE(cap2Idx, 0);
    EXPECT_TRUE(supported[static_cast<gp::USize>(cap2Idx)]->usedAsExtension());

    VkInstanceCreateInfo createInfo = zeroVulkanStruct<VkInstanceCreateInfo>(VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO);
    OptionalVulkanInstanceExtensions extFlags;
    supported[static_cast<gp::USize>(cap2Idx)]->preCreateInstance(createInfo, extFlags);
    EXPECT_EQ(extFlags.hasKHRGetSurfaceCapabilities2, 1u);

    const gp::Int32 customIdx = VulkanExtensionBase::findExtension(supported, "VK_EXT_custom_plugin_instance_ext");
    ASSERT_GE(customIdx, 0);
    EXPECT_TRUE(supported[static_cast<gp::USize>(customIdx)]->usedAsExtension());

    VulkanInstanceExtension::clearExternalInstanceExtensions();
    EXPECT_EQ(VulkanInstanceExtension::getExternalInstanceExtensions().size(), 0u);
}

TEST(VulkanExtensionsTest, ValidationFeaturesExtensionCollectsAndChainsFeatures)
{
    const gp::Vector<VkValidationFeatureEnableEXT> none =
        VulkanEXTValidationFeaturesExtension::collectValidationFeatures(0, false, false);
    EXPECT_EQ(none.size(), 0u);

    const gp::Vector<VkValidationFeatureEnableEXT> gpuLevel1 =
        VulkanEXTValidationFeaturesExtension::collectValidationFeatures(1, false, false);
    ASSERT_EQ(gpuLevel1.size(), 1u);
    EXPECT_EQ(gpuLevel1[0], VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT);

    const gp::Vector<VkValidationFeatureEnableEXT> gpuLevel2All =
        VulkanEXTValidationFeaturesExtension::collectValidationFeatures(2, true, true);
    ASSERT_EQ(gpuLevel2All.size(), 4u);
    EXPECT_EQ(gpuLevel2All[0], VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT);
    EXPECT_EQ(gpuLevel2All[1], VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_RESERVE_BINDING_SLOT_EXT);
    EXPECT_EQ(gpuLevel2All[2], VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT);
    EXPECT_EQ(gpuLevel2All[3], VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT);

    VulkanEXTValidationFeaturesExtension ext;
    EXPECT_STREQ(ext.getExtensionName(), VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME);
    EXPECT_FALSE(ext.isActivated());
    EXPECT_EQ(ext.getEnabledFeatures().size(), 0u);

    // Verify preCreateInstance chains VkValidationFeaturesEXT while preserving existing pNext.
    gp::CommandLine::shutdown();
    gp::CommandLine::initialize("app.exe -vulkanbestpractices -vulkandebugsync");

    VkBaseOutStructure sentinel{};
    sentinel.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    sentinel.pNext = nullptr;

    VkInstanceCreateInfo createInfo = zeroVulkanStruct<VkInstanceCreateInfo>(VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO);
    createInfo.pNext = &sentinel;
    OptionalVulkanInstanceExtensions extFlags;
    ext.preCreateInstance(createInfo, extFlags);

    ASSERT_NE(createInfo.pNext, nullptr);
    const VkValidationFeaturesEXT* const chained = static_cast<const VkValidationFeaturesEXT*>(createInfo.pNext);
    EXPECT_EQ(chained->sType, VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT);
    EXPECT_EQ(chained->pNext, &sentinel);
    EXPECT_GE(chained->enabledValidationFeatureCount, 2u);
    EXPECT_NE(chained->pEnabledValidationFeatures, nullptr);
    EXPECT_EQ(ext.getEnabledFeatures().size(), chained->enabledValidationFeatureCount);

    gp::CommandLine::shutdown();
}

TEST(VulkanExtensionsTest, DeviceExtensionDiscoveryAndExternalWrapper)
{
    VulkanDeviceExtension::clearExternalDeviceExtensions();

    gp::SharedPtr<TestExternalDeviceExtension> passingExternal =
        gp::makeShared<TestExternalDeviceExtension>("VK_EXT_custom_passing_device_ext");
    gp::SharedPtr<TestExternalDeviceExtension> failingExternal = gp::makeShared<TestExternalDeviceExtension>(
        "VK_EXT_custom_failing_device_ext", kVulkanExtensionNotPromoted, false, true
    );
    gp::SharedPtr<TestExternalDeviceExtension> duplicateSwapchain =
        gp::makeShared<TestExternalDeviceExtension>(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

    gp::SharedPtr<VulkanExternalDeviceExtensionBase> externals[] = { passingExternal,
                                                                     failingExternal,
                                                                     duplicateSwapchain };
    VulkanDeviceExtension::addExternalDeviceExtensions(
        gp::ConstVectorView<gp::SharedPtr<VulkanExternalDeviceExtensionBase>>(externals, 3u)
    );
    EXPECT_EQ(VulkanDeviceExtension::getExternalDeviceExtensions().size(), 3u);

    gp::Vector<VkExtensionProperties> driverExts;
    driverExts.pushBack(makeExtensionProperties(VK_KHR_SWAPCHAIN_EXTENSION_NAME));
    driverExts.pushBack(makeExtensionProperties(VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME));
    driverExts.pushBack(makeExtensionProperties("VK_EXT_custom_passing_device_ext"));
    driverExts.pushBack(makeExtensionProperties("VK_EXT_custom_failing_device_ext"));

    VulkanDeviceExtensionArray supported = VulkanDeviceExtension::getSupportedDeviceExtensions(
        nullptr, VK_API_VERSION_1_3, gp::ConstVectorView<VkExtensionProperties>(driverExts.data(), driverExts.size())
    );

    // Duplicate registration of VK_KHR_swapchain must trigger duplicateCallback().
    EXPECT_EQ(duplicateSwapchain->m_duplicateCallbackCount, 1);

    const gp::Int32 timelineIdx =
        VulkanExtensionBase::findExtension(supported, VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME);
    ASSERT_GE(timelineIdx, 0);
    VulkanDeviceExtension& timelineExt = *supported[static_cast<gp::USize>(timelineIdx)];
    VkPhysicalDeviceFeatures2 timelineFeats2 =
        zeroVulkanStruct<VkPhysicalDeviceFeatures2>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2);
    timelineExt.prePhysicalDeviceFeatures(timelineFeats2);
    ASSERT_NE(timelineFeats2.pNext, nullptr);
    VkPhysicalDeviceTimelineSemaphoreFeatures* const timelineFeats =
        static_cast<VkPhysicalDeviceTimelineSemaphoreFeatures*>(timelineFeats2.pNext);
    EXPECT_EQ(timelineFeats->sType, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES);
    timelineFeats->timelineSemaphore = VK_TRUE;

    OptionalVulkanDeviceExtensions devFlags;
    timelineExt.postPhysicalDeviceFeatures(devFlags);
    EXPECT_TRUE(timelineExt.didRequirementsPass());
    EXPECT_EQ(devFlags.hasKHRTimelineSemaphore, 1u);

    VkDeviceCreateInfo timelineDevCreateInfo =
        zeroVulkanStruct<VkDeviceCreateInfo>(VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO);
    timelineExt.preCreateDevice(timelineDevCreateInfo);
    EXPECT_EQ(timelineDevCreateInfo.pNext, timelineFeats);

    // Passing external wrapper chains structs and passes requirements.
    const gp::Int32 passingIdx = VulkanExtensionBase::findExtension(supported, "VK_EXT_custom_passing_device_ext");
    ASSERT_GE(passingIdx, 0);
    VulkanDeviceExtension& passingExt = *supported[static_cast<gp::USize>(passingIdx)];
    EXPECT_TRUE(passingExt.usedAsExtension());

    VkPhysicalDeviceProperties2 props2 =
        zeroVulkanStruct<VkPhysicalDeviceProperties2>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2);
    passingExt.prePhysicalDeviceProperties(props2);
    EXPECT_EQ(props2.pNext, passingExternal->getPropertiesStruct());
    passingExt.postPhysicalDeviceProperties(devFlags);
    EXPECT_EQ(passingExternal->m_propertiesCallbackCount, 1);
    EXPECT_TRUE(passingExt.didRequirementsPass());

    VkPhysicalDeviceFeatures2 feats2 =
        zeroVulkanStruct<VkPhysicalDeviceFeatures2>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2);
    passingExt.prePhysicalDeviceFeatures(feats2);
    EXPECT_EQ(feats2.pNext, passingExternal->getFeaturesStruct());
    passingExt.postPhysicalDeviceFeatures(devFlags);
    EXPECT_EQ(passingExternal->m_featuresCallbackCount, 1);
    EXPECT_TRUE(passingExt.didRequirementsPass());

    VkDeviceCreateInfo devCreateInfo = zeroVulkanStruct<VkDeviceCreateInfo>(VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO);
    passingExt.preCreateDevice(devCreateInfo);
    EXPECT_EQ(devCreateInfo.pNext, passingExternal->getFeaturesStruct());

    VulkanCoreFeaturesAndProperties coreData{};
    passingExt.handleCoreFeaturesAndProperties(coreData, devFlags);
    EXPECT_EQ(passingExternal->m_coreCallbackCount, 1);

    // Failing external wrapper marks requirements failed and skips device create chaining.
    const gp::Int32 failingIdx = VulkanExtensionBase::findExtension(supported, "VK_EXT_custom_failing_device_ext");
    ASSERT_GE(failingIdx, 0);
    VulkanDeviceExtension& failingExt = *supported[static_cast<gp::USize>(failingIdx)];
    failingExt.postPhysicalDeviceFeatures(devFlags);
    EXPECT_FALSE(failingExt.didRequirementsPass());

    VkDeviceCreateInfo failCreateInfo = zeroVulkanStruct<VkDeviceCreateInfo>(VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO);
    failingExt.preCreateDevice(failCreateInfo);
    EXPECT_EQ(failCreateInfo.pNext, nullptr);

    // External device extension storage is automatically drained on getSupportedDeviceExtensions().
    EXPECT_EQ(VulkanDeviceExtension::getExternalDeviceExtensions().size(), 0u);

    // Failing propertiesCallback() short-circuits subsequent feature and device creation hooks,
    // and pre-existing pNext pointers are preserved when chaining.
    gp::SharedPtr<TestExternalDeviceExtension> propFailExternal =
        gp::makeShared<TestExternalDeviceExtension>("VK_EXT_prop_fail_ext", kVulkanExtensionNotPromoted, true, false);
    VulkanExternalDeviceExtensionWrapper propFailWrapper(nullptr, propFailExternal);

    VkBaseOutStructure sentinel{};
    sentinel.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    sentinel.pNext = nullptr;

    VkPhysicalDeviceProperties2 chainProps =
        zeroVulkanStruct<VkPhysicalDeviceProperties2>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2);
    chainProps.pNext = &sentinel;
    propFailWrapper.prePhysicalDeviceProperties(chainProps);
    EXPECT_EQ(chainProps.pNext, propFailExternal->getPropertiesStruct());
    EXPECT_EQ(propFailExternal->getPropertiesStruct()->pNext, &sentinel);

    propFailWrapper.postPhysicalDeviceProperties(devFlags);
    EXPECT_FALSE(propFailWrapper.didRequirementsPass());

    VkPhysicalDeviceFeatures2 skippedFeats =
        zeroVulkanStruct<VkPhysicalDeviceFeatures2>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2);
    propFailWrapper.prePhysicalDeviceFeatures(skippedFeats);
    EXPECT_EQ(skippedFeats.pNext, nullptr);
    propFailWrapper.postPhysicalDeviceFeatures(devFlags);
    EXPECT_EQ(propFailExternal->m_featuresCallbackCount, 0);

    VulkanDeviceExtension::clearExternalDeviceExtensions();
    EXPECT_EQ(VulkanDeviceExtension::getExternalDeviceExtensions().size(), 0u);
}

namespace
{

/// @brief Searches a `pNext` chain for a structure matching `targetType`.
/// @param[in] head First node in the `pNext` chain.
/// @param[in] targetType Expected `VkStructureType`.
/// @return Matching structure pointer, or `nullptr` if absent.
[[nodiscard]] VkBaseOutStructure* findInPNextChainMutable(void* const head, const VkStructureType targetType) noexcept
{
    VkBaseOutStructure* current = static_cast<VkBaseOutStructure*>(head);
    while (current != nullptr)
    {
        if (current->sType == targetType)
        {
            return current;
        }
        current = current->pNext;
    }
    return nullptr;
}

/// @brief Searches a const `pNext` chain for a structure matching `targetType`.
/// @param[in] head First node in the `pNext` chain.
/// @param[in] targetType Expected `VkStructureType`.
/// @return Matching structure pointer, or `nullptr` if absent.
[[nodiscard]] const VkBaseInStructure*
    findInPNextChainConst(const void* const head, const VkStructureType targetType) noexcept
{
    const VkBaseInStructure* current = static_cast<const VkBaseInStructure*>(head);
    while (current != nullptr)
    {
        if (current->sType == targetType)
        {
            return current;
        }
        current = current->pNext;
    }
    return nullptr;
}

/// @brief Counts the number of nodes chained onto `head` (detecting cycles up to `maxNodes`).
/// @param[in] head First node in the `pNext` chain.
/// @param[in] maxNodes Cycle safety cap.
/// @return Node count.
[[nodiscard]] gp::UInt32 countPNextNodes(const void* const head, const gp::UInt32 maxNodes = 64u) noexcept
{
    gp::UInt32 count = 0u;
    const VkBaseInStructure* current = static_cast<const VkBaseInStructure*>(head);
    while (current != nullptr && count < maxNodes)
    {
        ++count;
        current = current->pNext;
    }
    return count;
}

/// @brief RAII helper restoring an integer console variable on scope exit.
struct ScopedConsoleIntOverride final
{
    gp::console::IConsoleVariable* m_variable{ nullptr };
    gp::Int32 m_originalValue{ 0 };

    ScopedConsoleIntOverride(const char* const name, const gp::Int32 tempValue) noexcept
    {
        gp::console::IConsoleManager& manager = gp::console::IConsoleManager::get();
        if (!manager.isInitialized())
        {
            static_cast<void>(manager.initialize());
        }
        m_variable = manager.findConsoleVariable(name);
        if (m_variable != nullptr)
        {
            m_originalValue = m_variable->getInt();
            (void)m_variable->set(tempValue);
        }
    }

    ~ScopedConsoleIntOverride() noexcept
    {
        if (m_variable != nullptr)
        {
            (void)m_variable->set(m_originalValue);
        }
    }
};

}   // namespace

TEST(VulkanExtensionsTest, FeatureGatedExtensionsChainIntoFeatures2AndDeviceCreateInfo)
{
    VulkanKHRTimelineSemaphoreExtension timelineExt(nullptr);
    VulkanKHRBufferDeviceAddressExtension bdaExt(nullptr);
    VulkanKHRSynchronization2Extension sync2Ext(nullptr);
    VulkanKHRSeparateDepthStencilLayoutsExtension separateDepthStencilExt(nullptr);
    VulkanKHRDynamicRenderingExtension dynamicRenderingExt(nullptr);
    VulkanEXTExtendedDynamicStateExtension eds1Ext(nullptr);
    VulkanEXTExtendedDynamicState2Extension eds2Ext(nullptr);
    VulkanEXTExtendedDynamicState3Extension eds3Ext(nullptr);
    VulkanEXTVertexInputDynamicStateExtension vertexInputDynExt(nullptr);
    VulkanEXTDescriptorIndexingExtension descriptorIndexingExt(nullptr);
    VulkanKHRMaintenance4Extension maint4Ext(nullptr);
    VulkanKHRMaintenance5Extension maint5Ext(nullptr);
    VulkanKHRMaintenance6Extension maint6Ext(nullptr);

    VulkanDeviceExtension* const extensions[] = {
        &timelineExt, &bdaExt,    &sync2Ext, &separateDepthStencilExt, &dynamicRenderingExt,
        &eds1Ext,     &eds2Ext,   &eds3Ext,  &vertexInputDynExt,       &descriptorIndexingExt,
        &maint4Ext,   &maint5Ext, &maint6Ext
    };

    VkPhysicalDeviceFeatures2 features2 =
        zeroVulkanStruct<VkPhysicalDeviceFeatures2>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2);
    for (VulkanDeviceExtension* const ext: extensions)
    {
        ext->prePhysicalDeviceFeatures(features2);
    }
    EXPECT_EQ(countPNextNodes(features2.pNext), 13u);

    // Populate simulated hardware support for every chained feature struct.
    reinterpret_cast<VkPhysicalDeviceTimelineSemaphoreFeatures*>(
        findInPNextChainMutable(features2.pNext, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES)
    )
        ->timelineSemaphore = VK_TRUE;

    reinterpret_cast<VkPhysicalDeviceBufferDeviceAddressFeatures*>(
        findInPNextChainMutable(features2.pNext, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES)
    )
        ->bufferDeviceAddress = VK_TRUE;

    reinterpret_cast<VkPhysicalDeviceSynchronization2Features*>(
        findInPNextChainMutable(features2.pNext, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES)
    )
        ->synchronization2 = VK_TRUE;

    reinterpret_cast<VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures*>(
        findInPNextChainMutable(
            features2.pNext, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SEPARATE_DEPTH_STENCIL_LAYOUTS_FEATURES
        )
    )
        ->separateDepthStencilLayouts = VK_TRUE;

    reinterpret_cast<VkPhysicalDeviceDynamicRenderingFeatures*>(
        findInPNextChainMutable(features2.pNext, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES)
    )
        ->dynamicRendering = VK_TRUE;

    reinterpret_cast<VkPhysicalDeviceExtendedDynamicStateFeaturesEXT*>(
        findInPNextChainMutable(features2.pNext, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT)
    )
        ->extendedDynamicState = VK_TRUE;

    reinterpret_cast<VkPhysicalDeviceExtendedDynamicState2FeaturesEXT*>(
        findInPNextChainMutable(
            features2.pNext, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_2_FEATURES_EXT
        )
    )
        ->extendedDynamicState2 = VK_TRUE;

    VkPhysicalDeviceExtendedDynamicState3FeaturesEXT* const eds3Features =
        reinterpret_cast<VkPhysicalDeviceExtendedDynamicState3FeaturesEXT*>(findInPNextChainMutable(
            features2.pNext, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_FEATURES_EXT
        ));
    ASSERT_NE(eds3Features, nullptr);
    eds3Features->extendedDynamicState3PolygonMode = VK_TRUE;
    eds3Features->extendedDynamicState3ColorBlendEnable = VK_TRUE;
    eds3Features->extendedDynamicState3ColorBlendEquation = VK_TRUE;
    eds3Features->extendedDynamicState3ColorWriteMask = VK_TRUE;
    eds3Features->extendedDynamicState3RasterizationSamples = VK_TRUE;
    eds3Features->extendedDynamicState3AlphaToCoverageEnable = VK_TRUE;
    // Set an unenabled vendor-extension subfeature to confirm preCreateDevice sanitizes it to VK_FALSE.
    eds3Features->extendedDynamicState3ShadingRateImageEnable = VK_TRUE;

    reinterpret_cast<VkPhysicalDeviceVertexInputDynamicStateFeaturesEXT*>(
        findInPNextChainMutable(
            features2.pNext, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_INPUT_DYNAMIC_STATE_FEATURES_EXT
        )
    )
        ->vertexInputDynamicState = VK_TRUE;

    VkPhysicalDeviceDescriptorIndexingFeatures* const descIndexingFeatures =
        reinterpret_cast<VkPhysicalDeviceDescriptorIndexingFeatures*>(
            findInPNextChainMutable(features2.pNext, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES)
        );
    ASSERT_NE(descIndexingFeatures, nullptr);
    descIndexingFeatures->runtimeDescriptorArray = VK_TRUE;
    descIndexingFeatures->descriptorBindingPartiallyBound = VK_TRUE;
    descIndexingFeatures->descriptorBindingUpdateUnusedWhilePending = VK_TRUE;
    descIndexingFeatures->descriptorBindingVariableDescriptorCount = VK_TRUE;

    reinterpret_cast<VkPhysicalDeviceMaintenance4Features*>(
        findInPNextChainMutable(features2.pNext, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_4_FEATURES)
    )
        ->maintenance4 = VK_TRUE;

    reinterpret_cast<VkPhysicalDeviceMaintenance5FeaturesKHR*>(
        findInPNextChainMutable(features2.pNext, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_5_FEATURES_KHR)
    )
        ->maintenance5 = VK_TRUE;

    reinterpret_cast<VkPhysicalDeviceMaintenance6FeaturesKHR*>(
        findInPNextChainMutable(features2.pNext, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_6_FEATURES_KHR)
    )
        ->maintenance6 = VK_TRUE;

    OptionalVulkanDeviceExtensions flags;
    for (VulkanDeviceExtension* const ext: extensions)
    {
        ext->postPhysicalDeviceFeatures(flags);
        EXPECT_TRUE(ext->didRequirementsPass()) << ext->getExtensionName();
    }

    EXPECT_EQ(flags.hasKHRTimelineSemaphore, 1u);
    EXPECT_EQ(flags.hasBufferDeviceAddress, 1u);
    EXPECT_EQ(flags.hasKHRSynchronization2, 1u);
    EXPECT_EQ(flags.hasSeparateDepthStencilLayouts, 1u);
    EXPECT_EQ(flags.hasKHRDynamicRendering, 1u);
    EXPECT_EQ(flags.hasEXTExtendedDynamicState1, 1u);
    EXPECT_EQ(flags.hasEXTExtendedDynamicState2, 1u);
    EXPECT_EQ(flags.hasEXTExtendedDynamicState3, 1u);
    EXPECT_EQ(flags.hasEXTVertexInputDynamicState, 1u);
    EXPECT_EQ(flags.hasEXTDescriptorIndexing, 1u);
    EXPECT_EQ(flags.hasKHRMaintenance4, 1u);
    EXPECT_EQ(flags.hasKHRMaintenance5, 1u);
    EXPECT_EQ(flags.hasKHRMaintenance6, 1u);

    VkDeviceCreateInfo deviceInfo = zeroVulkanStruct<VkDeviceCreateInfo>(VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO);
    for (VulkanDeviceExtension* const ext: extensions)
    {
        ext->preCreateDevice(deviceInfo);
    }
    EXPECT_EQ(countPNextNodes(deviceInfo.pNext), 13u);

    const VkPhysicalDeviceTimelineSemaphoreFeatures* const chainedTimeline =
        reinterpret_cast<const VkPhysicalDeviceTimelineSemaphoreFeatures*>(
            findInPNextChainConst(deviceInfo.pNext, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES)
        );
    ASSERT_NE(chainedTimeline, nullptr);
    EXPECT_EQ(chainedTimeline->timelineSemaphore, VK_TRUE);

    const VkPhysicalDeviceBufferDeviceAddressFeatures* const chainedBda =
        reinterpret_cast<const VkPhysicalDeviceBufferDeviceAddressFeatures*>(
            findInPNextChainConst(deviceInfo.pNext, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES)
        );
    ASSERT_NE(chainedBda, nullptr);
    EXPECT_EQ(chainedBda->bufferDeviceAddress, VK_TRUE);

    const VkPhysicalDeviceSynchronization2Features* const chainedSync2 =
        reinterpret_cast<const VkPhysicalDeviceSynchronization2Features*>(
            findInPNextChainConst(deviceInfo.pNext, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES)
        );
    ASSERT_NE(chainedSync2, nullptr);
    EXPECT_EQ(chainedSync2->synchronization2, VK_TRUE);

    const VkPhysicalDeviceExtendedDynamicState3FeaturesEXT* const chainedEds3 =
        reinterpret_cast<const VkPhysicalDeviceExtendedDynamicState3FeaturesEXT*>(findInPNextChainConst(
            deviceInfo.pNext, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_FEATURES_EXT
        ));
    ASSERT_NE(chainedEds3, nullptr);
    EXPECT_EQ(chainedEds3->extendedDynamicState3PolygonMode, VK_TRUE);
    EXPECT_EQ(chainedEds3->extendedDynamicState3ColorBlendEnable, VK_TRUE);
    EXPECT_EQ(chainedEds3->extendedDynamicState3ShadingRateImageEnable, VK_FALSE);

    const VkPhysicalDeviceVertexInputDynamicStateFeaturesEXT* const chainedVertexInput =
        reinterpret_cast<const VkPhysicalDeviceVertexInputDynamicStateFeaturesEXT*>(findInPNextChainConst(
            deviceInfo.pNext, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_INPUT_DYNAMIC_STATE_FEATURES_EXT
        ));
    ASSERT_NE(chainedVertexInput, nullptr);
    EXPECT_EQ(chainedVertexInput->vertexInputDynamicState, VK_TRUE);
}

TEST(VulkanExtensionsTest, FeatureGatedExtensionsRejectMissingFeaturesAndSkipCreateDeviceChain)
{
    VulkanKHRTimelineSemaphoreExtension timelineExt(nullptr);
    VulkanKHRBufferDeviceAddressExtension bdaExt(nullptr);
    VulkanKHRSynchronization2Extension sync2Ext(nullptr);
    VulkanEXTExtendedDynamicStateExtension eds1Ext(nullptr);
    VulkanEXTExtendedDynamicState2Extension eds2Ext(nullptr);
    VulkanEXTExtendedDynamicState3Extension eds3Ext(nullptr);
    VulkanEXTVertexInputDynamicStateExtension vertexInputDynExt(nullptr);
    VulkanEXTMeshShaderExtension meshShaderExt(nullptr);
    VulkanKHRAccelerationStructureExtension accelStructExt(nullptr);
    VulkanKHRRayTracingPipelineExtension rtPipelineExt(nullptr);
    VulkanKHRRayQueryExtension rayQueryExt(nullptr);

    VulkanDeviceExtension* const extensions[] = { &timelineExt,    &bdaExt,        &sync2Ext,          &eds1Ext,
                                                  &eds2Ext,        &eds3Ext,       &vertexInputDynExt, &meshShaderExt,
                                                  &accelStructExt, &rtPipelineExt, &rayQueryExt };

    VkPhysicalDeviceFeatures2 features2 =
        zeroVulkanStruct<VkPhysicalDeviceFeatures2>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2);
    OptionalVulkanDeviceExtensions flags;
    VkDeviceCreateInfo deviceInfo = zeroVulkanStruct<VkDeviceCreateInfo>(VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO);

    for (VulkanDeviceExtension* const ext: extensions)
    {
        ext->prePhysicalDeviceFeatures(features2);
        ext->postPhysicalDeviceFeatures(flags);
        EXPECT_FALSE(ext->didRequirementsPass()) << ext->getExtensionName();
        ext->preCreateDevice(deviceInfo);
    }

    EXPECT_EQ(deviceInfo.pNext, nullptr);
    EXPECT_EQ(flags.hasKHRTimelineSemaphore, 0u);
    EXPECT_EQ(flags.hasBufferDeviceAddress, 0u);
    EXPECT_EQ(flags.hasKHRSynchronization2, 0u);
    EXPECT_FALSE(flags.hasAnyExtendedDynamicState());
    EXPECT_EQ(flags.hasEXTMeshShader, 0u);
    EXPECT_EQ(flags.hasAccelerationStructure, 0u);
    EXPECT_EQ(flags.hasRayTracingPipeline, 0u);
    EXPECT_EQ(flags.hasRayQuery, 0u);
}

TEST(VulkanExtensionsTest, AllowDynamicStatesBitmaskControlsEachDynamicStateExtension)
{
    {
        const ScopedConsoleIntOverride maskOverride("r.Vulkan.AllowDynamicStates", 0);
        EXPECT_FALSE(VulkanEXTExtendedDynamicStateExtension(nullptr).isEnabled());
        EXPECT_FALSE(VulkanEXTExtendedDynamicState2Extension(nullptr).isEnabled());
        EXPECT_FALSE(VulkanEXTExtendedDynamicState3Extension(nullptr).isEnabled());
        EXPECT_FALSE(VulkanEXTVertexInputDynamicStateExtension(nullptr).isEnabled());
    }
    {
        const ScopedConsoleIntOverride maskOverride("r.Vulkan.AllowDynamicStates", 5);
        EXPECT_TRUE(VulkanEXTExtendedDynamicStateExtension(nullptr).isEnabled());
        EXPECT_FALSE(VulkanEXTExtendedDynamicState2Extension(nullptr).isEnabled());
        EXPECT_TRUE(VulkanEXTExtendedDynamicState3Extension(nullptr).isEnabled());
        EXPECT_FALSE(VulkanEXTVertexInputDynamicStateExtension(nullptr).isEnabled());
    }
    {
        const ScopedConsoleIntOverride maskOverride("r.Vulkan.AllowDynamicStates", 10);
        EXPECT_FALSE(VulkanEXTExtendedDynamicStateExtension(nullptr).isEnabled());
        EXPECT_TRUE(VulkanEXTExtendedDynamicState2Extension(nullptr).isEnabled());
        EXPECT_FALSE(VulkanEXTExtendedDynamicState3Extension(nullptr).isEnabled());
        EXPECT_TRUE(VulkanEXTVertexInputDynamicStateExtension(nullptr).isEnabled());
    }
}

TEST(VulkanExtensionsTest, PromotedExtensionsPopulateFlagsFromCoreFeaturesAndOrderingIsPreserved)
{
    VkPhysicalDeviceVulkan11Features core11 =
        zeroVulkanStruct<VkPhysicalDeviceVulkan11Features>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES);
    core11.multiview = VK_TRUE;
    core11.storageBuffer16BitAccess = VK_TRUE;
    core11.uniformAndStorageBuffer16BitAccess = VK_TRUE;
    core11.storagePushConstant16 = VK_TRUE;

    VkPhysicalDeviceVulkan12Features core12 =
        zeroVulkanStruct<VkPhysicalDeviceVulkan12Features>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES);
    core12.timelineSemaphore = VK_TRUE;
    core12.bufferDeviceAddress = VK_TRUE;
    core12.separateDepthStencilLayouts = VK_TRUE;
    core12.runtimeDescriptorArray = VK_TRUE;
    core12.descriptorBindingPartiallyBound = VK_TRUE;
    core12.descriptorBindingUpdateUnusedWhilePending = VK_TRUE;
    core12.descriptorBindingVariableDescriptorCount = VK_TRUE;

    VkPhysicalDeviceVulkan13Features core13 =
        zeroVulkanStruct<VkPhysicalDeviceVulkan13Features>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES);
    core13.synchronization2 = VK_TRUE;
    core13.dynamicRendering = VK_TRUE;
    core13.maintenance4 = VK_TRUE;
    core13.subgroupSizeControl = VK_TRUE;

    VkPhysicalDeviceVulkan14Features core14 =
        zeroVulkanStruct<VkPhysicalDeviceVulkan14Features>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES);
    core14.maintenance5 = VK_TRUE;
    core14.maintenance6 = VK_TRUE;

    VulkanCoreFeaturesAndProperties coreData{};
    coreData.features11 = &core11;
    coreData.features12 = &core12;
    coreData.features13 = &core13;
    coreData.features14 = &core14;

    OptionalVulkanDeviceExtensions flags;
    VulkanKHRMultiviewExtension(nullptr).handleCoreFeaturesAndProperties(coreData, flags);
    VulkanKHR16BitStorageExtension(nullptr).handleCoreFeaturesAndProperties(coreData, flags);
    VulkanKHRTimelineSemaphoreExtension(nullptr).handleCoreFeaturesAndProperties(coreData, flags);
    VulkanKHRBufferDeviceAddressExtension(nullptr).handleCoreFeaturesAndProperties(coreData, flags);
    VulkanKHRSeparateDepthStencilLayoutsExtension(nullptr).handleCoreFeaturesAndProperties(coreData, flags);
    VulkanEXTDescriptorIndexingExtension(nullptr).handleCoreFeaturesAndProperties(coreData, flags);
    VulkanKHRSynchronization2Extension(nullptr).handleCoreFeaturesAndProperties(coreData, flags);
    VulkanKHRDynamicRenderingExtension(nullptr).handleCoreFeaturesAndProperties(coreData, flags);
    VulkanKHRMaintenance4Extension(nullptr).handleCoreFeaturesAndProperties(coreData, flags);
    VulkanEXTSubgroupSizeControlExtension(nullptr).handleCoreFeaturesAndProperties(coreData, flags);
    VulkanEXTExtendedDynamicStateExtension(nullptr).handleCoreFeaturesAndProperties(coreData, flags);
    VulkanEXTExtendedDynamicState2Extension(nullptr).handleCoreFeaturesAndProperties(coreData, flags);
    VulkanKHRMaintenance5Extension(nullptr).handleCoreFeaturesAndProperties(coreData, flags);
    VulkanKHRMaintenance6Extension(nullptr).handleCoreFeaturesAndProperties(coreData, flags);

    EXPECT_EQ(flags.hasKHRMultiview, 1u);
    EXPECT_EQ(flags.hasKHR16bitStorage, 1u);
    EXPECT_EQ(flags.hasKHRTimelineSemaphore, 1u);
    EXPECT_EQ(flags.hasBufferDeviceAddress, 1u);
    EXPECT_EQ(flags.hasSeparateDepthStencilLayouts, 1u);
    EXPECT_EQ(flags.hasEXTDescriptorIndexing, 1u);
    EXPECT_EQ(flags.hasKHRSynchronization2, 1u);
    EXPECT_EQ(flags.hasKHRDynamicRendering, 1u);
    EXPECT_EQ(flags.hasKHRMaintenance4, 1u);
    EXPECT_EQ(flags.hasEXTSubgroupSizeControl, 1u);
    EXPECT_EQ(flags.hasEXTExtendedDynamicState1, 1u);
    EXPECT_EQ(flags.hasEXTExtendedDynamicState2, 1u);
    EXPECT_EQ(flags.hasKHRMaintenance5, 1u);
    EXPECT_EQ(flags.hasKHRMaintenance6, 1u);

    // Verify dependency ordering in getSupportedDeviceExtensions and KHR calibrated timestamps precedence over EXT.
    gp::Vector<VkExtensionProperties> driverExts;
    driverExts.pushBack(makeExtensionProperties(VK_KHR_CALIBRATED_TIMESTAMPS_EXTENSION_NAME));
    driverExts.pushBack(makeExtensionProperties(VK_EXT_CALIBRATED_TIMESTAMPS_EXTENSION_NAME));

    VulkanDeviceExtensionArray supported = VulkanDeviceExtension::getSupportedDeviceExtensions(
        nullptr, VK_API_VERSION_1_3, gp::ConstVectorView<VkExtensionProperties>(driverExts.data(), driverExts.size())
    );
    const gp::Int32 fsrIdx = VulkanExtensionBase::findExtension(supported, VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME);
    const gp::Int32 fdmIdx = VulkanExtensionBase::findExtension(supported, VK_EXT_FRAGMENT_DENSITY_MAP_EXTENSION_NAME);
    const gp::Int32 khrCalIdx =
        VulkanExtensionBase::findExtension(supported, VK_KHR_CALIBRATED_TIMESTAMPS_EXTENSION_NAME);
    const gp::Int32 extCalIdx =
        VulkanExtensionBase::findExtension(supported, VK_EXT_CALIBRATED_TIMESTAMPS_EXTENSION_NAME);
    const gp::Int32 storage16Idx = VulkanExtensionBase::findExtension(supported, VK_KHR_16BIT_STORAGE_EXTENSION_NAME);
    const gp::Int32 float16Idx =
        VulkanExtensionBase::findExtension(supported, VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME);

    EXPECT_LT(fsrIdx, fdmIdx);
    EXPECT_LT(khrCalIdx, extCalIdx);
    EXPECT_LT(storage16Idx, float16Idx);

    OptionalVulkanDeviceExtensions calFlags;
    supported[static_cast<gp::USize>(khrCalIdx)]->postPhysicalDeviceFeatures(calFlags);
    supported[static_cast<gp::USize>(extCalIdx)]->postPhysicalDeviceFeatures(calFlags);
    EXPECT_EQ(calFlags.hasKHRCalibratedTimestamps, 1u);
    EXPECT_EQ(calFlags.hasEXTCalibratedTimestamps, 0u);
    EXPECT_FALSE(supported[static_cast<gp::USize>(extCalIdx)]->didRequirementsPass());
}

}   // namespace gp::rhi::vulkan::tests
