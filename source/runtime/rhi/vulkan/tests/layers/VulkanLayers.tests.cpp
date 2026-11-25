// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "layers/VulkanLayers.hpp"
#include "console/ConsoleEnums.hpp"
#include "console/IConsoleManager.hpp"
#include "console/IConsoleVariable.hpp"
#include "containers/arrays/Vector.hpp"
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "extensions/VulkanDeviceExtension.hpp"
#include "extensions/VulkanExtensionBase.hpp"
#include "extensions/VulkanExtensionTypes.hpp"
#include "extensions/VulkanInstanceExtension.hpp"
#include "memory/Memory.hpp"
#include "memory/pointers/UniquePtr.hpp"
#include "miscellaneous/CommandLine.hpp"
#include "VulkanConfiguration.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <cstring>
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

namespace
{

VkExtensionProperties makeLayerExtension(const char* const name) noexcept
{
    VkExtensionProperties props{};
    gp::memory::zeroMemory(&props, sizeof(props));
    const gp::USize length = std::strlen(name);
    const gp::USize copyLen =
        (length < VK_MAX_EXTENSION_NAME_SIZE - 1u) ? length : static_cast<gp::USize>(VK_MAX_EXTENSION_NAME_SIZE - 1u);
    gp::memory::copyMemory(props.extensionName, name, copyLen);
    props.specVersion = 1u;
    return props;
}

VulkanLayerWithExtensions
    makeLayer(const char* const layerName, gp::ConstVectorView<const char*> extensionNames = {}) noexcept
{
    VulkanLayerWithExtensions layer;
    gp::memory::zeroMemory(&layer.layerProperties, sizeof(layer.layerProperties));
    const gp::USize length = std::strlen(layerName);
    const gp::USize copyLen =
        (length < VK_MAX_EXTENSION_NAME_SIZE - 1u) ? length : static_cast<gp::USize>(VK_MAX_EXTENSION_NAME_SIZE - 1u);
    gp::memory::copyMemory(layer.layerProperties.layerName, layerName, copyLen);
    layer.layerProperties.specVersion = VK_API_VERSION_1_3;
    layer.layerProperties.implementationVersion = 1u;

    for (const char* const extName: extensionNames)
    {
        layer.extensionProperties.pushBack(makeLayerExtension(extName));
    }
    return layer;
}

bool containsString(const gp::Vector<const char*>& list, const char* const target) noexcept
{
    for (const char* const item: list)
    {
        if (item != nullptr && std::strcmp(item, target) == 0)
        {
            return true;
        }
    }
    return false;
}

class ScopedCommandLine final
{
public:
    explicit ScopedCommandLine(const gp::StringView args) noexcept
    {
        gp::CommandLine::shutdown();
        gp::CommandLine::initialize(args);
    }

    ~ScopedCommandLine() noexcept
    {
        gp::CommandLine::shutdown();
    }

    ScopedCommandLine(const ScopedCommandLine&) = delete;
    ScopedCommandLine& operator=(const ScopedCommandLine&) = delete;
};

}   // namespace

TEST(VulkanLayersTest, FindLayerAndAddRequestedLayerPropagatesExtensionSupport)
{
    const char* const validationExts[] = { VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
                                           VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME };

    gp::Vector<VulkanLayerWithExtensions> availableLayers;
    availableLayers.pushBack(
        makeLayer(kKhronosStandardValidationLayerName, gp::ConstVectorView<const char*>(validationExts, 2u))
    );
    availableLayers.pushBack(makeLayer(kRenderDocLayerName));

    const gp::ConstVectorView<VulkanLayerWithExtensions> layersView(availableLayers.data(), availableLayers.size());

    EXPECT_EQ(findLayerIndexInList(nullptr, layersView), -1);
    EXPECT_EQ(findLayerIndexInList("VK_LAYER_missing", layersView), -1);
    EXPECT_EQ(findLayerIndexInList(kKhronosStandardValidationLayerName, layersView), 0);
    EXPECT_EQ(findLayerIndexInList(kRenderDocLayerName, layersView), 1);

    VulkanInstanceExtensionArray extensions;
    extensions.pushBack(
        gp::makeUnique<VulkanInstanceExtension>(
            VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
            kVulkanExtensionEnabled,
            kVulkanExtensionNotPromoted,
            nullptr,
            VulkanExtensionActivation::ManuallyActivate
        )
    );
    extensions.pushBack(gp::makeUnique<VulkanEXTValidationFeaturesExtension>());

    gp::Vector<const char*> enabledLayers;
    EXPECT_FALSE(addRequestedLayer("VK_LAYER_missing", layersView, extensions, enabledLayers));
    EXPECT_EQ(enabledLayers.size(), 0u);

    EXPECT_TRUE(addRequestedLayer(kKhronosStandardValidationLayerName, layersView, extensions, enabledLayers));
    ASSERT_EQ(enabledLayers.size(), 1u);
    EXPECT_STREQ(enabledLayers[0], kKhronosStandardValidationLayerName);
    EXPECT_TRUE(extensions[0]->isSupported());
    EXPECT_TRUE(extensions[1]->isSupported());
}

TEST(VulkanLayersTest, SetupValidationRequestsParsesCommandLineFlags)
{
#if GP_VULKAN_HAS_DEBUGGING_ENABLED
    gp::console::IConsoleVariable* const valCvar =
        gp::console::IConsoleManager::get().findConsoleVariable("r.Vulkan.EnableValidation");
    gp::console::IConsoleVariable* const gpuCvar =
        gp::console::IConsoleManager::get().findConsoleVariable("r.Vulkan.GPUValidation");
    const gp::Int32 origVal = (valCvar != nullptr) ? valCvar->getInt() : 0;
    const gp::Int32 origGpu = (gpuCvar != nullptr) ? gpuCvar->getInt() : 0;

    {
        ScopedCommandLine cmd("app.exe -vulkandebug");
        const VulkanValidationConfig config = setupValidationRequests();
        EXPECT_TRUE(config.isDebugLayerEnabled);
        EXPECT_GE(config.validationLevel, 2);
    }

    {
        ScopedCommandLine cmd("app.exe -vulkanvalidation=4 -gpuvalidation");
        const VulkanValidationConfig config = setupValidationRequests();
        EXPECT_TRUE(config.isDebugLayerEnabled);
        EXPECT_EQ(config.validationLevel, 4);
        EXPECT_EQ(config.gpuValidationLevel, 2);
    }

    // -gpuvalidation alone bumps validationLevel from < 2 to 2.
    {
        if (valCvar != nullptr)
        {
            static_cast<void>(valCvar->set(0, gp::console::ConsoleVarSetBy::Console));
        }
        ScopedCommandLine cmd("app.exe -gpuvalidation");
        const VulkanValidationConfig config = setupValidationRequests();
        EXPECT_TRUE(config.isDebugLayerEnabled);
        EXPECT_EQ(config.validationLevel, 2);
        EXPECT_EQ(config.gpuValidationLevel, 2);
    }

    if (valCvar != nullptr)
    {
        static_cast<void>(valCvar->set(origVal, gp::console::ConsoleVarSetBy::Console));
    }
    if (gpuCvar != nullptr)
    {
        static_cast<void>(gpuCvar->set(origGpu, gp::console::ConsoleVarSetBy::Console));
    }
#else
    const VulkanValidationConfig config = setupValidationRequests();
    EXPECT_FALSE(config.isDebugLayerEnabled);
    EXPECT_EQ(config.validationLevel, 0);
    EXPECT_EQ(config.gpuValidationLevel, 0);
#endif
}

TEST(VulkanLayersTest, SetupInstanceLayersSelectsValidationAndDebugUtils)
{
    clearExternalInstanceLayers();
    ScopedCommandLine cmd("app.exe -vulkanbestpractices -vulkandebugsync");

    const char* const validationExts[] = { VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
                                           VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME };

    gp::Vector<VulkanLayerWithExtensions> availableLayers;
    availableLayers.pushBack(
        makeLayer(kKhronosStandardValidationLayerName, gp::ConstVectorView<const char*>(validationExts, 2u))
    );
    availableLayers.pushBack(makeLayer("VK_LAYER_custom_external"));

    const char* const extLayers[] = { "VK_LAYER_custom_external", kKhronosStandardValidationLayerName };
    addExternalInstanceLayers(gp::ConstVectorView<const char*>(extLayers, 2u));

    VulkanInstanceExtensionArray extensions =
        VulkanInstanceExtension::getSupportedInstanceExtensions(VK_API_VERSION_1_3, {});

    VulkanValidationConfig validationConfig;
    validationConfig.isDebugLayerEnabled = true;
    validationConfig.validationLevel = 3;
    validationConfig.gpuValidationLevel = 1;

    const VulkanInstanceLayerSetupResult result = setupInstanceLayers(
        gp::ConstVectorView<VulkanLayerWithExtensions>(availableLayers.data(), availableLayers.size()),
        extensions,
        validationConfig
    );

    EXPECT_TRUE(containsString(result.enabledLayers, "VK_LAYER_custom_external"));

#if GP_VULKAN_HAS_DEBUGGING_ENABLED
    EXPECT_TRUE(containsString(result.enabledLayers, kKhronosStandardValidationLayerName));
    // Duplicates must be removed and layers sorted lexicographically.
    ASSERT_EQ(result.enabledLayers.size(), 2u);
    EXPECT_LT(std::strcmp(result.enabledLayers[0], result.enabledLayers[1]), 0);
    EXPECT_EQ(result.activeDebugLayerExtension, ActiveDebugLayerExtension::DebugUtilsExtension);

    const gp::Int32 debugUtilsIdx = VulkanExtensionBase::findExtension(extensions, VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    ASSERT_GE(debugUtilsIdx, 0);
    EXPECT_TRUE(extensions[static_cast<gp::USize>(debugUtilsIdx)]->usedAsExtension());

    const gp::Int32 valFeatsIdx =
        VulkanExtensionBase::findExtension(extensions, VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME);
    ASSERT_GE(valFeatsIdx, 0);
    EXPECT_TRUE(extensions[static_cast<gp::USize>(valFeatsIdx)]->usedAsExtension());
#endif

    clearExternalInstanceLayers();
}

TEST(VulkanLayersTest, SetupInstanceLayersPrefersGfxReconstructOverVkTrace)
{
    clearExternalInstanceLayers();
    ScopedCommandLine cmd("app.exe -vktrace");

    gp::Vector<VulkanLayerWithExtensions> availableLayers;
    availableLayers.pushBack(makeLayer(kGfxReconstructLayerName));
    availableLayers.pushBack(makeLayer(kVkTraceLayerName));
    availableLayers.pushBack(makeLayer(kKhronosStandardValidationLayerName));

    VulkanInstanceExtensionArray extensions =
        VulkanInstanceExtension::getSupportedInstanceExtensions(VK_API_VERSION_1_3, {});

    VulkanValidationConfig validationConfig;
    validationConfig.isDebugLayerEnabled = true;
    validationConfig.validationLevel = 2;

    const VulkanInstanceLayerSetupResult result = setupInstanceLayers(
        gp::ConstVectorView<VulkanLayerWithExtensions>(availableLayers.data(), availableLayers.size()),
        extensions,
        validationConfig
    );

#if GP_VULKAN_HAS_DEBUGGING_ENABLED
    EXPECT_EQ(result.activeDebugLayerExtension, ActiveDebugLayerExtension::GfxReconstructLayer);
    EXPECT_TRUE(containsString(result.enabledLayers, kGfxReconstructLayerName));
    EXPECT_FALSE(containsString(result.enabledLayers, kVkTraceLayerName));
    // Trace capture suppresses validation layers.
    EXPECT_FALSE(containsString(result.enabledLayers, kKhronosStandardValidationLayerName));

    // When GfxReconstruct is absent, -vktrace falls back to VK_LAYER_LUNARG_vktrace and suppresses api_dump.
    {
        ScopedCommandLine fallbackCmd("app.exe -vktrace -vulkanapidump");
        gp::Vector<VulkanLayerWithExtensions> fallbackLayers;
        fallbackLayers.pushBack(makeLayer(kVkTraceLayerName));
        fallbackLayers.pushBack(makeLayer(kApiDumpLayerName));

        VulkanInstanceExtensionArray fallbackExts =
            VulkanInstanceExtension::getSupportedInstanceExtensions(VK_API_VERSION_1_3, {});
        const VulkanInstanceLayerSetupResult fallbackResult = setupInstanceLayers(
            gp::ConstVectorView<VulkanLayerWithExtensions>(fallbackLayers.data(), fallbackLayers.size()),
            fallbackExts,
            validationConfig
        );

        EXPECT_EQ(fallbackResult.activeDebugLayerExtension, ActiveDebugLayerExtension::VkTraceLayer);
        EXPECT_TRUE(containsString(fallbackResult.enabledLayers, kVkTraceLayerName));
        EXPECT_FALSE(containsString(fallbackResult.enabledLayers, kApiDumpLayerName));
    }

    // -vulkandebugutils activates VK_EXT_debug_utils even when validation is disabled.
    {
        ScopedCommandLine debugUtilsCmd("app.exe -vulkandebugutils");
        gp::Vector<VkExtensionProperties> driverExts;
        driverExts.pushBack(makeLayerExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME));
        VulkanInstanceExtensionArray debugUtilsExts = VulkanInstanceExtension::getSupportedInstanceExtensions(
            VK_API_VERSION_1_3, gp::ConstVectorView<VkExtensionProperties>(driverExts.data(), driverExts.size())
        );

        VulkanValidationConfig disabledConfig{};
        const VulkanInstanceLayerSetupResult debugUtilsResult = setupInstanceLayers({}, debugUtilsExts, disabledConfig);
        EXPECT_EQ(debugUtilsResult.activeDebugLayerExtension, ActiveDebugLayerExtension::DebugUtilsExtension);
    }
#endif
}

TEST(VulkanLayersTest, SetupInstanceLayersApiDumpPopulatesLayerSettings)
{
    clearExternalInstanceLayers();
    ScopedCommandLine cmd("app.exe -vulkanapidump");

    gp::Vector<VulkanLayerWithExtensions> availableLayers;
    availableLayers.pushBack(makeLayer(kApiDumpLayerName));

    VulkanInstanceExtensionArray extensions =
        VulkanInstanceExtension::getSupportedInstanceExtensions(VK_API_VERSION_1_3, {});

    VulkanValidationConfig validationConfig;
    const VulkanInstanceLayerSetupResult result = setupInstanceLayers(
        gp::ConstVectorView<VulkanLayerWithExtensions>(availableLayers.data(), availableLayers.size()),
        extensions,
        validationConfig
    );

#if GP_VULKAN_HAS_DEBUGGING_ENABLED
    EXPECT_TRUE(containsString(result.enabledLayers, kApiDumpLayerName));
    ASSERT_EQ(result.layerSettings.size(), 4u);
    EXPECT_STREQ(result.layerSettings[0].pLayerName, kApiDumpLayerName);
    EXPECT_STREQ(result.layerSettings[0].pSettingName, "file");
#endif
}

TEST(VulkanLayersTest, SetupDeviceLayersDetectsRenderDocAndLegacyDebugMarkerFallback)
{
    clearExternalDeviceLayers();

    const char* const renderDocExts[] = { VK_EXT_DEBUG_MARKER_EXTENSION_NAME };
    gp::Vector<VulkanLayerWithExtensions> availableLayers;
    availableLayers.pushBack(makeLayer(kRenderDocLayerName, gp::ConstVectorView<const char*>(renderDocExts, 1u)));
    availableLayers.pushBack(makeLayer("VK_LAYER_custom_device"));

    const char* const externalDevLayers[] = { "VK_LAYER_custom_device", "VK_LAYER_custom_device" };
    addExternalDeviceLayers(gp::ConstVectorView<const char*>(externalDevLayers, 2u));

    gp::Vector<VkExtensionProperties> driverExts;
    driverExts.pushBack(makeLayerExtension(VK_EXT_DEBUG_MARKER_EXTENSION_NAME));
    const gp::ConstVectorView<VkExtensionProperties> driverExtsView(driverExts.data(), driverExts.size());

    // Case 1: VK_EXT_debug_utils is NOT active -> legacy VK_EXT_debug_marker fallback activates.
    {
        VulkanDeviceExtensionArray extensions =
            VulkanDeviceExtension::getSupportedDeviceExtensions(nullptr, VK_API_VERSION_1_3, driverExtsView);

        const VulkanDeviceLayerSetupResult result = setupDeviceLayers(
            gp::ConstVectorView<VulkanLayerWithExtensions>(availableLayers.data(), availableLayers.size()),
            false,
            extensions
        );

#if GP_VULKAN_HAS_DEBUGGING_ENABLED && GP_VULKAN_ENABLE_DRAW_MARKERS
        EXPECT_TRUE(result.renderDocFound);
#else
        EXPECT_FALSE(result.renderDocFound);
#endif
        ASSERT_EQ(result.enabledLayers.size(), 1u);
        EXPECT_STREQ(result.enabledLayers[0], "VK_LAYER_custom_device");

#if GP_VULKAN_ENABLE_DRAW_MARKERS
        EXPECT_TRUE(result.useLegacyDebugMarkerExt);
        const gp::Int32 markerIdx = VulkanExtensionBase::findExtension(extensions, VK_EXT_DEBUG_MARKER_EXTENSION_NAME);
        ASSERT_GE(markerIdx, 0);
        EXPECT_TRUE(extensions[static_cast<gp::USize>(markerIdx)]->usedAsExtension());
#endif
    }

    // Case 2: VK_EXT_debug_utils IS active -> legacy VK_EXT_debug_marker is not activated.
    {
        VulkanDeviceExtensionArray extensions =
            VulkanDeviceExtension::getSupportedDeviceExtensions(nullptr, VK_API_VERSION_1_3, driverExtsView);

        const VulkanDeviceLayerSetupResult result = setupDeviceLayers(
            gp::ConstVectorView<VulkanLayerWithExtensions>(availableLayers.data(), availableLayers.size()),
            true,
            extensions
        );

#if GP_VULKAN_HAS_DEBUGGING_ENABLED && GP_VULKAN_ENABLE_DRAW_MARKERS
        EXPECT_TRUE(result.renderDocFound);
#else
        EXPECT_FALSE(result.renderDocFound);
#endif
        EXPECT_FALSE(result.useLegacyDebugMarkerExt);
    }

    clearExternalDeviceLayers();
}

}   // namespace gp::rhi::vulkan::tests
