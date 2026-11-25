// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "containers/arrays/Vector.hpp"      // IWYU pragma: keep
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "extensions/VulkanDeviceExtension.hpp"
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
    OptionalVulkanDeviceExtensions devFlags;
    supported[static_cast<gp::USize>(timelineIdx)]->postPhysicalDeviceFeatures(devFlags);
    EXPECT_EQ(devFlags.hasKHRTimelineSemaphore, 1u);

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

}   // namespace gp::rhi::vulkan::tests
