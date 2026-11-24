// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Vector.hpp"      // IWYU pragma: keep
#include "containers/strings/String.hpp"     // IWYU pragma: keep
#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "device/DeviceEnums.hpp"
#include "platforms/base/system/Time.hpp"
#include "RHIVulkan.hpp"
#include "VulkanForward.hpp"      // IWYU pragma: keep
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include "window/NativeWindow.hpp"

namespace gp::rhi::vulkan
{

/// @brief Base native window context carried through surface and swapchain creation.
/// @details Wraps either a raw OS window pointer or a `gp::hal::NativeWindowHandle` so platform layers can extract
///          WSI handles (such as `HWND`/`HINSTANCE`, X11 `Display`/`Window`, or Wayland `wl_display`/`wl_surface`)
///          without leaking platform SDK headers into shared RHI interfaces.
class GP_RHI_VULKAN_API VulkanGenericPlatformWindowContext
{
public:
    /// @brief Constructs a window context from an opaque native window pointer.
    /// @param[in] inWindowHandle Platform window handle pointer (may be `nullptr` for headless surfaces).
    explicit VulkanGenericPlatformWindowContext(void* const inWindowHandle = nullptr) noexcept
        : m_windowHandle(inWindowHandle)
    {}

    /// @brief Constructs a window context from a HAL `NativeWindowHandle` descriptor.
    /// @param[in] inNativeHandle Native window descriptor captured from `gp::hal::IWindow`.
    explicit VulkanGenericPlatformWindowContext(const gp::hal::NativeWindowHandle& inNativeHandle) noexcept
        : m_nativeHandle(inNativeHandle)
        , m_windowHandle(extractRawHandle(inNativeHandle))
    {}

    /// @brief Returns the primary opaque native window pointer.
    /// @return Raw window pointer, or `nullptr` if unavailable.
    [[nodiscard]] GP_FORCEINLINE_HINT void* getWindowHandle() const noexcept
    {
        return m_windowHandle;
    }

    /// @brief Returns the full HAL `NativeWindowHandle` when constructed from one.
    /// @return Const reference to the stored `gp::hal::NativeWindowHandle`.
    [[nodiscard]] GP_FORCEINLINE_HINT const gp::hal::NativeWindowHandle& getNativeWindowHandle() const noexcept
    {
        return m_nativeHandle;
    }

    /// @brief Returns a platform-defined immutable window identifier (`0` if untracked).
    /// @details Platforms whose window pointers may be recycled before deferred destroy override this to carry a
    ///          stable identifier captured while the window is alive.
    /// @return Stable window identifier, or `0u`.
    [[nodiscard]] GP_FORCEINLINE_HINT gp::UInt32 getWindowId() const noexcept
    {
        return 0u;
    }

    /// @brief Reports whether this platform may attempt on-demand swapchain creation during present.
    /// @return True by default.
    [[nodiscard]] static constexpr bool canCreateSwapchainOnDemand() noexcept
    {
        return true;
    }

    /// @brief Reports whether the wrapped window context is valid for surface creation.
    /// @return True by default.
    [[nodiscard]] GP_FORCEINLINE_HINT bool isValid() const noexcept
    {
        return true;
    }

protected:
    /// @brief Extracts the primary raw window pointer from a `gp::hal::NativeWindowHandle`.
    /// @param[in] handle Native window handle union.
    /// @return Extracted raw pointer (for X11, the `Display*` handle), or `nullptr` if `backend` is `Unknown`.
    [[nodiscard]] static void* extractRawHandle(const gp::hal::NativeWindowHandle& handle) noexcept
    {
        switch (handle.backend)
        {
        case gp::hal::NativeWindowBackend::Win32:
            return handle.win32.hwnd;
        case gp::hal::NativeWindowBackend::Cocoa:
            return handle.cocoa.nsWindow;
        case gp::hal::NativeWindowBackend::UIKit:
            return handle.uikit.uiWindow;
        case gp::hal::NativeWindowBackend::Wayland:
            return handle.wayland.surface;
        case gp::hal::NativeWindowBackend::X11:
            return handle.x11.display;
        case gp::hal::NativeWindowBackend::Android:
            return handle.android.window;
        case gp::hal::NativeWindowBackend::KMSDRM:
            return handle.kmsdrm.gbmSurface;
        case gp::hal::NativeWindowBackend::Vivante:
            return handle.vivante.window;
        case gp::hal::NativeWindowBackend::Unknown:
        default:
            return nullptr;
        }
    }

    /// @brief Optional full HAL window descriptor for multi-handle WSI backends.
    gp::hal::NativeWindowHandle m_nativeHandle{};

    /// @brief Primary raw OS window pointer.
    void* m_windowHandle{ nullptr };
};

/// @brief Default platform abstraction layer providing Vulkan capability defaults, hooks, and PSO cache utilities.
/// @details Platform subclasses (`VulkanWindowsPlatform`, `VulkanLinuxPlatform`, `VulkanMacPlatform`,
///          `VulkanAndroidPlatform`) hide or specialize individual static methods as needed.
class GP_RHI_VULKAN_API VulkanGenericPlatform
{
public:
    VulkanGenericPlatform() = delete;

    /// @brief Reports whether the host platform can run the Vulkan backend.
    /// @return True by default.
    [[nodiscard]] static constexpr bool isSupported() noexcept
    {
        return true;
    }

    /// @brief Returns the default dynamic loader library filename for the active OS.
    /// @return Null-terminated shared library name.
    [[nodiscard]] static constexpr const char* getDefaultLibraryName() noexcept
    {
        return "libvulkan.so.1";
    }

    /// @brief Appends secondary platform loader library filenames tried if `getDefaultLibraryName()` is unavailable.
    /// @param[in,out] outFallbacks Destination list of fallback shared library filenames.
    static void getFallbackLibraryNames(gp::Vector<const char*>& outFallbacks) noexcept
    {
        static_cast<void>(outFallbacks);
    }

    /// @brief Loads the platform Vulkan shared library and resolves base loader entry points.
    /// @return True when loading succeeds.
    [[nodiscard]] static bool loadVulkanLibrary() noexcept;

    /// @brief Resolves instance-level Vulkan function pointers for `instance`.
    /// @param[in] instance Newly created `VkInstance`.
    /// @return True when all required instance entry points were resolved.
    [[nodiscard]] static bool loadVulkanInstanceFunctions(VkInstance instance) noexcept;

    /// @brief Clears cached instance-level Vulkan function pointers during shutdown.
    static void clearVulkanInstanceFunctions() noexcept;

    /// @brief Releases the dynamically loaded Vulkan library handle.
    static void freeVulkanLibrary() noexcept;

    /// @brief Platform hook called after `device` creation before queue/resource setup completes.
    /// @param[in] device Newly created `VulkanDevice`.
    static void initDevice(VulkanDevice* const device) noexcept
    {
        static_cast<void>(device);
    }

    /// @brief Platform hook called after full GPU initialization completes.
    /// @param[in] device Initialized `VulkanDevice`.
    static void postInitGPU(const VulkanDevice& device) noexcept
    {
        static_cast<void>(device);
    }

    /// @brief Notification called after enumerating available instance layers and extensions.
    /// @param[in] layers Discovered instance layer names.
    /// @param[in] extensions Discovered instance extension names.
    static void notifyFoundInstanceLayersAndExtensions(
        const gp::ConstVectorView<const char*> layers, const gp::ConstVectorView<const char*> extensions
    ) noexcept
    {
        static_cast<void>(layers);
        static_cast<void>(extensions);
    }

    /// @brief Notification called after enumerating available device layers and extensions.
    /// @param[in] physicalDevice Inspected `VkPhysicalDevice`.
    /// @param[in] layers Discovered device layer names.
    /// @param[in] extensions Discovered device extension names.
    static void notifyFoundDeviceLayersAndExtensions(
        const VkPhysicalDevice physicalDevice,
        const gp::ConstVectorView<const char*> layers,
        const gp::ConstVectorView<const char*> extensions
    ) noexcept
    {
        static_cast<void>(physicalDevice);
        static_cast<void>(layers);
        static_cast<void>(extensions);
    }

    /// @brief Appends platform-required Vulkan instance layer names to `outLayers`.
    /// @param[in,out] outLayers Destination list of null-terminated layer names.
    static void getInstanceLayers(gp::Vector<const char*>& outLayers) noexcept
    {
        static_cast<void>(outLayers);
    }

    /// @brief Appends platform-required Vulkan device layer names to `outLayers`.
    /// @param[in,out] outLayers Destination list of null-terminated layer names.
    static void getDeviceLayers(gp::Vector<const char*>& outLayers) noexcept
    {
        static_cast<void>(outLayers);
    }

    /// @brief Appends platform-required Vulkan instance extension names (such as WSI surface extensions).
    /// @param[in,out] outExtensions Destination list of null-terminated extension names.
    static void getPlatformInstanceExtensionNames(gp::Vector<const char*>& outExtensions) noexcept
    {
        static_cast<void>(outExtensions);
    }

    /// @brief Appends platform-required Vulkan device extension names.
    /// @param[in,out] outExtensions Destination list of null-terminated extension names.
    static void getPlatformDeviceExtensionNames(gp::Vector<const char*>& outExtensions) noexcept
    {
        static_cast<void>(outExtensions);
    }

    /// @brief Platform hook invoked when an unrecoverable GPU crash or `VK_ERROR_DEVICE_LOST` is detected.
    /// @param[in] message Diagnostic description of the crash.
    static void onGPUCrash(const gp::StringView message) noexcept
    {
        static_cast<void>(message);
    }

    /// @brief Destroys a platform `VkSurfaceKHR` via `vkDestroySurfaceKHR`.
    /// @param[in] windowId Immutable platform window identifier (unused on generic platforms).
    /// @param[in] instance Owning `VkInstance`.
    /// @param[in] surface Surface handle to destroy.
    static void destroySurface(gp::UInt32 windowId, VkInstance instance, VkSurfaceKHR surface) noexcept;

    /// @brief Presents queued swapchain images on `queue` via `vkQueuePresentKHR`.
    /// @param[in] queue Presentation `VkQueue`.
    /// @param[in,out] presentInfo Presentation descriptor.
    /// @return `VkResult` returned by `vkQueuePresentKHR`.
    [[nodiscard]] static VkResult present(VkQueue queue, VkPresentInfoKHR& presentInfo) noexcept;

    /// @brief Creates a `VkSwapchainKHR` via `vkCreateSwapchainKHR`.
    /// @param[in,out] windowContext Platform window context associated with the swapchain.
    /// @param[in] physicalDevice Target `VkPhysicalDevice`.
    /// @param[in] device Target logical `VkDevice`.
    /// @param[in] createInfo Swapchain creation descriptor.
    /// @param[in] allocator Optional host allocation callbacks.
    /// @param[out] outSwapchain Destination swapchain handle.
    /// @return `VkResult` returned by `vkCreateSwapchainKHR`.
    [[nodiscard]] static VkResult createSwapchainKHR(
        VulkanGenericPlatformWindowContext& windowContext,
        VkPhysicalDevice physicalDevice,
        VkDevice device,
        const VkSwapchainCreateInfoKHR* createInfo,
        const VkAllocationCallbacks* allocator,
        VkSwapchainKHR* outSwapchain
    ) noexcept;

    /// @brief Destroys a `VkSwapchainKHR` via `vkDestroySwapchainKHR`.
    /// @param[in] device Owning logical `VkDevice`.
    /// @param[in] swapchain Swapchain handle to destroy.
    /// @param[in] allocator Optional host allocation callbacks.
    static void
        destroySwapchainKHR(VkDevice device, VkSwapchainKHR swapchain, const VkAllocationCallbacks* allocator) noexcept;

    /// @brief Reports whether the platform supports BC1..BC7 block-compressed texture formats.
    /// @return True on desktop platforms by default.
    [[nodiscard]] static constexpr bool supportsBCTextureFormats() noexcept
    {
        return true;
    }

    /// @brief Reports whether the platform supports ASTC texture compression formats.
    /// @return False on desktop platforms by default.
    [[nodiscard]] static constexpr bool supportsASTCTextureFormats() noexcept
    {
        return false;
    }

    /// @brief Reports whether the platform supports ETC2 texture compression formats.
    /// @return False on desktop platforms by default.
    [[nodiscard]] static constexpr bool supportsETC2TextureFormats() noexcept
    {
        return false;
    }

    /// @brief Reports whether the platform supports `R16_UNorm` texture sampling and storage.
    /// @return True by default.
    [[nodiscard]] static constexpr bool supportsR16UnormTextureFormat() noexcept
    {
        return true;
    }

    /// @brief Reports whether the platform can query surface capabilities, formats, and present modes.
    /// @return True by default.
    [[nodiscard]] static constexpr bool supportsQuerySurfaceProperties() noexcept
    {
        return true;
    }

    /// @brief Reports whether the platform mandates the mobile rendering path (`FeatureLevel::ES3_1`).
    /// @return False on desktop platforms by default.
    [[nodiscard]] static constexpr bool requiresMobileRenderer() noexcept
    {
        return false;
    }

    /// @brief Resolves the effective `FeatureLevel` given `requestedFeatureLevel`, platform limits, and CLI overrides.
    /// @note Editor builds preserve `requestedFeatureLevel` at compile time via `GP_WITH_EDITOR` until a runtime
    ///       editor-mode query is added to core.
    /// @param[in] requestedFeatureLevel Caller-requested feature level.
    /// @return Resolved `FeatureLevel`.
    [[nodiscard]] static FeatureLevel getFeatureLevel(FeatureLevel requestedFeatureLevel) noexcept;

    /// @brief Allows the platform to install or remove OS-level handlers at RHI init and shutdown.
    /// @param[in] init True during RHI initialization; false during teardown.
    static void overridePlatformHandlers(const bool init) noexcept
    {
        static_cast<void>(init);
    }

    /// @brief Reports whether the platform driver needs an access-flag workaround for
    /// `VK_IMAGE_LAYOUT_PRESENT_SRC_KHR`.
    /// @return False by default.
    [[nodiscard]] static constexpr bool requiresPresentLayoutFix() noexcept
    {
        return false;
    }

    /// @brief Reports whether the platform has unified CPU/GPU physical memory.
    /// @return False by default.
    [[nodiscard]] static constexpr bool hasUnifiedMemory() noexcept
    {
        return false;
    }

    /// @brief Reports whether parallel command-buffer recording tasks are beneficial on this platform.
    /// @return True by default.
    [[nodiscard]] static constexpr bool supportParallelRenderingTasks() noexcept
    {
        return true;
    }

    /// @brief Reports whether dynamic resolution scaling (requiring GPU timing queries) is supported.
    /// @return True by default.
    [[nodiscard]] static constexpr bool supportsDynamicResolution() noexcept
    {
        return true;
    }

    /// @brief Reports whether rendering directly into 3D volume texture slices is supported.
    /// @return True by default.
    [[nodiscard]] static constexpr bool supportsVolumeTextureRendering() noexcept
    {
        return true;
    }

    /// @brief Reports whether swapchain images must transition from `VK_IMAGE_LAYOUT_GENERAL` on first use.
    /// @return False by default.
    [[nodiscard]] static constexpr bool requiresSwapchainGeneralInitialLayout() noexcept
    {
        return false;
    }

    /// @brief Platform frame-pacing hook called prior to `vkQueuePresentKHR`.
    /// @param[in,out] device Active `VulkanDevice`.
    /// @param[in] windowHandle Native window pointer.
    /// @param[in] swapchain Target swapchain handle.
    /// @param[in] presentId Monotonic presentation identifier.
    /// @param[in,out] presentInfo Presentation descriptor that may be augmented via `pNext`.
    /// @return True if the platform handled frame pacing internally; false otherwise.
    [[nodiscard]] static bool framePace(
        VulkanDevice& device,
        void* const windowHandle,
        const VkSwapchainKHR swapchain,
        const gp::UInt32 presentId,
        VkPresentInfoKHR& presentInfo
    ) noexcept
    {
        static_cast<void>(device);
        static_cast<void>(windowHandle);
        static_cast<void>(swapchain);
        static_cast<void>(presentId);
        static_cast<void>(presentInfo);
        return false;
    }

    /// @brief Notifies the platform whether `VK_EXT_present_timing` is active on a newly created swapchain.
    /// @param[in] device Owning logical device.
    /// @param[in] swapchain Newly created swapchain.
    /// @param[in] presentTimingEnabled True when `VK_EXT_present_timing` is enabled.
    static void notifySwapchainPresentTiming(
        const VkDevice device, const VkSwapchainKHR swapchain, const bool presentTimingEnabled
    ) noexcept
    {
        static_cast<void>(device);
        static_cast<void>(swapchain);
        static_cast<void>(presentTimingEnabled);
    }

    /// @brief Reports whether swapchain recreation should be attempted after acquire or present failures.
    /// @return True by default.
    [[nodiscard]] static constexpr bool recreateSwapchainOnFail() noexcept
    {
        return true;
    }

    /// @brief Reports whether a native window handle is still valid for presentation.
    /// @param[in] windowHandle Native window handle pointer.
    /// @return True when `windowHandle` is non-null.
    [[nodiscard]] static bool isWindowValid(void* const windowHandle) noexcept
    {
        return windowHandle != nullptr;
    }

    /// @brief Tests and clears a pending platform-side swapchain recreation request for `windowHandle`.
    /// @param[in] windowHandle Native window handle pointer.
    /// @return False by default.
    [[nodiscard]] static bool tryConsumeSwapchainRecreateRequest(void* const windowHandle) noexcept
    {
        static_cast<void>(windowHandle);
        return false;
    }

    /// @brief Reports whether the backend must wait on the previous frame's completion event before proceeding.
    /// @return True by default.
    [[nodiscard]] static constexpr bool requiresWaitingForFrameCompletionEvent() noexcept
    {
        return true;
    }

    /// @brief Reports whether graphics pipelines may omit a fragment (pixel) shader.
    /// @return True by default.
    [[nodiscard]] static constexpr bool supportsNullPixelShader() noexcept
    {
        return true;
    }

    /// @brief Reports whether the platform requires depth writes to be enabled during stencil clears.
    /// @return False by default.
    [[nodiscard]] static constexpr bool requiresDepthStencilFullWrite() noexcept
    {
        return false;
    }

    /// @brief Reports whether separate `VkAttachmentDescriptionStencilLayout` must be avoided in favor of merged
    ///        depth-stencil layouts.
    /// @return False by default.
    [[nodiscard]] static constexpr bool requiresMergedDepthStencilLayout() noexcept
    {
        return false;
    }

    /// @brief Validates whether a serialized Vulkan pipeline cache header matches `deviceProperties`.
    /// @details Verifies header size (`16 + VK_UUID_SIZE`), `VK_PIPELINE_CACHE_HEADER_VERSION_ONE`, `vendorID`,
    ///          `deviceID`, and `pipelineCacheUUID`.
    /// @param[in] deviceProperties Active physical device properties.
    /// @param[in] deviceCache Raw serialized pipeline cache bytes.
    /// @return True when the binary cache header matches the physical device.
    [[nodiscard]] static bool psoBinaryCacheMatches(
        const VkPhysicalDeviceProperties& deviceProperties, gp::ConstVectorView<gp::UInt8> deviceCache
    ) noexcept;

    /// @brief Appends `.<vendorID>.<deviceID>` in lowercase hexadecimal to `cacheFilename` if not already present.
    /// @param[in] deviceProperties Active physical device properties.
    /// @param[in] cacheFilename Base PSO cache filename.
    /// @return Device-qualified PSO binary cache filename.
    [[nodiscard]] static gp::String createPSOBinaryCacheFilename(
        const VkPhysicalDeviceProperties& deviceProperties, gp::StringView cacheFilename
    ) noexcept;

    /// @brief Returns the top-level directory path used to store compiled Vulkan pipeline caches.
    /// @return Relative folder path (`Saved/VulkanProgramBinaryCache`).
    [[nodiscard]] static gp::String getCompiledPSOCacheTopFolderPath() noexcept;

    /// @brief Gathers candidate precompiled PSO cache filenames staged for the active platform.
    /// @return List of discovered PSO cache paths (empty when no staged caches are present).
    [[nodiscard]] static gp::Vector<gp::String> getPSOCacheFilenames() noexcept;

    /// @brief Platform hook invoked to inspect or augment `deviceCreateInfo` prior to `vkCreateDevice`.
    /// @param[in] physicalDeviceProperties Selected physical device properties.
    /// @param[in,out] deviceCreateInfo Logical device creation descriptor.
    static void serializeDeviceCreateInfo(
        const VkPhysicalDeviceProperties& physicalDeviceProperties, VkDeviceCreateInfo& deviceCreateInfo
    ) noexcept
    {
        static_cast<void>(physicalDeviceProperties);
        static_cast<void>(deviceCreateInfo);
    }

    /// @brief Platform filter invoked before logging a legacy `VK_EXT_debug_report` validation message.
    /// @param[in] msgFlags Debug report severity flags.
    /// @param[in] objType Reporting Vulkan object type.
    /// @param[in] srcObject Handle of the reporting object.
    /// @param[in] location Layer-specific location code.
    /// @param[in] msgCode Layer-specific message code.
    /// @param[in] layerPrefix Reporting layer name.
    /// @param[in] msg Diagnostic text.
    /// @param[in] userData Callback user data pointer.
    /// @return `VK_TRUE` to print the report, or `VK_FALSE` to suppress it.
    [[nodiscard]] static VkBool32 debugReportFunction(
        const VkDebugReportFlagsEXT msgFlags,
        const VkDebugReportObjectTypeEXT objType,
        const gp::UInt64 srcObject,
        const gp::USize location,
        const gp::Int32 msgCode,
        const char* const layerPrefix,
        const char* const msg,
        void* const userData
    ) noexcept
    {
        static_cast<void>(msgFlags);
        static_cast<void>(objType);
        static_cast<void>(srcObject);
        static_cast<void>(location);
        static_cast<void>(msgCode);
        static_cast<void>(layerPrefix);
        static_cast<void>(msg);
        static_cast<void>(userData);
        return VK_TRUE;
    }

    /// @brief Applies platform-specific adjustments to `imageCreateInfo` to reduce texture memory overhead.
    /// @param[in,out] imageCreateInfo Image creation descriptor to adjust.
    static void setImageMemoryRequirementWorkaround(VkImageCreateInfo& imageCreateInfo) noexcept
    {
        static_cast<void>(imageCreateInfo);
    }

    /// @brief Reports whether Vulkan profile compatibility checks are enabled (`r.Vulkan.UseProfileCheck` and CLI).
    /// @return True when profile checks should run.
    [[nodiscard]] static bool supportsProfileChecks() noexcept;

    /// @brief Builds the Vulkan profile identifier string for `featureLevel` and optional ray tracing support.
    /// @param[in] featureLevel Target rendering feature level.
    /// @param[in] raytracing True to append the `_RT` profile suffix.
    /// @return Profile name string (e.g., `"VP_GP_Vulkan_SM6_RT"`).
    [[nodiscard]] static gp::String
        getVulkanProfileNameForFeatureLevel(FeatureLevel featureLevel, bool raytracing) noexcept;

    /// @brief Filters or returns the shader stage flags required to support subgroup wave operations.
    /// @param[in] deviceShaderStageFlags Subgroup stage flags reported by the physical device.
    /// @return Required `VkShaderStageFlags`.
    [[nodiscard]] static constexpr VkShaderStageFlags
        requiredWaveOpsShaderStageFlags(const VkShaderStageFlags deviceShaderStageFlags) noexcept
    {
        return deviceShaderStageFlags;
    }

    /// @brief Returns the `VkTimeDomainKHR` used for calibrated CPU/GPU timestamp correlation.
    /// @return `VK_TIME_DOMAIN_DEVICE_KHR` by default.
    [[nodiscard]] static constexpr VkTimeDomainKHR getTimeDomain() noexcept
    {
        return VK_TIME_DOMAIN_DEVICE_KHR;
    }

    /// @brief Converts a calibrated timestamp sampled in `getTimeDomain()` into CPU performance counter ticks.
    /// @details `VK_TIME_DOMAIN_DEVICE_KHR` has no direct CPU clock mapping, so the generic fallback samples
    ///          `gp::platform::Time::rawPerformanceCounter()`.
    /// @param[in] timestamp Raw timestamp value returned by `vkGetCalibratedTimestampsKHR`.
    /// @return Timestamp in CPU raw performance counter units.
    [[nodiscard]] static gp::UInt64 calibratedTimestampToCycles64(const gp::UInt64 timestamp) noexcept
    {
        static_cast<void>(timestamp);
        return gp::platform::Time::rawPerformanceCounter();
    }
};

}   // namespace gp::rhi::vulkan
