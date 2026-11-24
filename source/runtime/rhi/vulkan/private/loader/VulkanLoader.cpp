// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "loader/VulkanLoader.hpp"
#include "containers/arrays/Vector.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "logging/LogMacros.hpp"
#include "miscellaneous/CommandLine.hpp"
#include "platform/VulkanPlatform.hpp"
#include "platforms/base/system/SharedLibrary.hpp"
#include "RHIDefinitions.hpp"
#include "RHIVulkan.hpp"

namespace gp::rhi::vulkan::dynamic_api
{

#define GP_DEFINE_VK_ENTRYPOINT(Type, Func) GP_RHI_VULKAN_API Type Func = nullptr;
GP_ENUM_VK_ENTRYPOINTS_ALL(GP_DEFINE_VK_ENTRYPOINT)
#undef GP_DEFINE_VK_ENTRYPOINT

}   // namespace gp::rhi::vulkan::dynamic_api

namespace gp::rhi::vulkan
{

namespace
{

/// @brief Active OS handle for the dynamically loaded Vulkan shared library.
void* g_libraryHandle = nullptr;

/// @brief Tracks whether default library probing has already run since startup or last `freeLibrary()`.
bool g_attemptedLoad = false;

/// @brief Reinterprets a raw shared-library export address as a typed Vulkan function pointer.
template <typename InPfnType>
[[nodiscard]] InPfnType castExportToPfn(void* const symbol) noexcept
{
    return reinterpret_cast<InPfnType>(reinterpret_cast<gp::UIntPtr>(symbol));
}

/// @brief Reinterprets a generic `PFN_vkVoidFunction` as a typed Vulkan function pointer.
template <typename InPfnType>
[[nodiscard]] InPfnType castVoidFuncToPfn(const PFN_vkVoidFunction func) noexcept
{
    return reinterpret_cast<InPfnType>(reinterpret_cast<gp::UIntPtr>(func));
}

/// @brief Opens either an explicit Vulkan library path or the active platform's primary/fallback library candidates.
[[nodiscard]] void* openPlatformVulkanLibrary(const gp::StringView explicitLibraryPath) noexcept
{
    if (!explicitLibraryPath.isEmpty())
    {
        return gp::platform::SharedLibrary::getHandle(explicitLibraryPath);
    }

    void* handle = gp::platform::SharedLibrary::getHandle(VulkanPlatform::getDefaultLibraryName());
    if (handle != nullptr)
    {
        return handle;
    }

    gp::Vector<const char*> fallbacks;
    VulkanPlatform::getFallbackLibraryNames(fallbacks);
    for (const char* const candidate: fallbacks)
    {
        handle = gp::platform::SharedLibrary::getHandle(candidate);
        if (handle != nullptr)
        {
            return handle;
        }
    }

    return nullptr;
}

}   // namespace

bool VulkanLoader::loadLibrary(const gp::StringView explicitLibraryPath) noexcept
{
    if (explicitLibraryPath.isEmpty() && g_attemptedLoad)
    {
        return g_libraryHandle != nullptr;
    }

    if (g_libraryHandle != nullptr)
    {
        freeLibrary();
    }

    if (explicitLibraryPath.isEmpty())
    {
        g_attemptedLoad = true;
    }

    g_libraryHandle = openPlatformVulkanLibrary(explicitLibraryPath);
    if (g_libraryHandle == nullptr)
    {
        return false;
    }

    bool foundAllRequired = true;

#define GP_LOAD_VK_BASE_EXPORT(Type, Func) \
    dynamic_api::Func = castExportToPfn<Type>(gp::platform::SharedLibrary::getExport(g_libraryHandle, #Func));
#define GP_CHECK_VK_BASE_EXPORT(Type, Func)                                         \
    if (dynamic_api::Func == nullptr)                                               \
    {                                                                               \
        foundAllRequired = false;                                                   \
        GP_LOG_WARNING(gp::LogRHI, "Failed to find Vulkan entry point: {}", #Func); \
    }

    GP_ENUM_VK_ENTRYPOINTS_BASE(GP_LOAD_VK_BASE_EXPORT)
    GP_ENUM_VK_ENTRYPOINTS_BASE(GP_CHECK_VK_BASE_EXPORT)

    GP_ENUM_VK_ENTRYPOINTS_PLATFORM_BASE(GP_LOAD_VK_BASE_EXPORT)
    GP_ENUM_VK_ENTRYPOINTS_PLATFORM_BASE(GP_CHECK_VK_BASE_EXPORT)

    if (!foundAllRequired)
    {
        freeLibrary();
        if (explicitLibraryPath.isEmpty())
        {
            g_attemptedLoad = true;
        }
        return false;
    }

    GP_ENUM_VK_ENTRYPOINTS_OPTIONAL_BASE(GP_LOAD_VK_BASE_EXPORT)
#if GP_BUILD_DEBUG
    #define GP_CHECK_VK_OPTIONAL_EXPORT(Type, Func)                                            \
        if (dynamic_api::Func == nullptr)                                                      \
        {                                                                                      \
            GP_LOG_WARNING(gp::LogRHI, "Optional Vulkan base entry point missing: {}", #Func); \
        }
    GP_ENUM_VK_ENTRYPOINTS_OPTIONAL_BASE(GP_CHECK_VK_OPTIONAL_EXPORT)
    #undef GP_CHECK_VK_OPTIONAL_EXPORT
#endif

#undef GP_CHECK_VK_BASE_EXPORT
#undef GP_LOAD_VK_BASE_EXPORT

    return true;
}

void VulkanLoader::freeLibrary() noexcept
{
    clearAllFunctions();

    if (g_libraryHandle != nullptr)
    {
        gp::platform::SharedLibrary::freeHandle(g_libraryHandle);
        g_libraryHandle = nullptr;
    }

    g_attemptedLoad = false;
}

bool VulkanLoader::isLibraryLoaded() noexcept
{
    return g_libraryHandle != nullptr;
}

void* VulkanLoader::getLibraryHandle() noexcept
{
    return g_libraryHandle;
}

bool VulkanLoader::loadInstanceFunctions(const VkInstance instance, const bool allowMissingSurfaceFunctions) noexcept
{
    if (instance == VK_NULL_HANDLE || dynamic_api::vkGetInstanceProcAddr == nullptr)
    {
        return false;
    }

    bool foundAllCoreInstance = true;
    bool foundAllSurfaceInstance = true;
    bool foundAllPlatformInstance = true;

#define GP_LOAD_VK_INSTANCE_PROC(Type, Func) \
    dynamic_api::Func = castVoidFuncToPfn<Type>(dynamic_api::vkGetInstanceProcAddr(instance, #Func));
#define GP_CHECK_VK_INSTANCE_FLAG(Flag, Type, Func)                                          \
    if (dynamic_api::Func == nullptr)                                                        \
    {                                                                                        \
        Flag = false;                                                                        \
        GP_LOG_WARNING(gp::LogRHI, "Failed to find Vulkan instance entry point: {}", #Func); \
    }
#define GP_CHECK_VK_CORE_INSTANCE(Type, Func) GP_CHECK_VK_INSTANCE_FLAG(foundAllCoreInstance, Type, Func)
#define GP_CHECK_VK_SURFACE_INSTANCE(Type, Func) GP_CHECK_VK_INSTANCE_FLAG(foundAllSurfaceInstance, Type, Func)
#define GP_CHECK_VK_PLATFORM_INSTANCE(Type, Func) GP_CHECK_VK_INSTANCE_FLAG(foundAllPlatformInstance, Type, Func)

    GP_ENUM_VK_ENTRYPOINTS_INSTANCE(GP_LOAD_VK_INSTANCE_PROC)
    GP_ENUM_VK_ENTRYPOINTS_INSTANCE(GP_CHECK_VK_CORE_INSTANCE)

    GP_ENUM_VK_ENTRYPOINTS_SURFACE_INSTANCE(GP_LOAD_VK_INSTANCE_PROC)
    GP_ENUM_VK_ENTRYPOINTS_SURFACE_INSTANCE(GP_CHECK_VK_SURFACE_INSTANCE)

    const bool renderOffScreen = allowMissingSurfaceFunctions ||
                                 (gp::CommandLine::isInitialized() && gp::CommandLine::hasArgument("RenderOffScreen"));

    if (!foundAllCoreInstance || (!foundAllSurfaceInstance && !renderOffScreen))
    {
        clearInstanceFunctions();
        return false;
    }

    GP_ENUM_VK_ENTRYPOINTS_OPTIONAL_INSTANCE(GP_LOAD_VK_INSTANCE_PROC)
    GP_ENUM_VK_ENTRYPOINTS_OPTIONAL_PLATFORM_INSTANCE(GP_LOAD_VK_INSTANCE_PROC)

#if GP_BUILD_DEBUG
    #define GP_CHECK_VK_OPTIONAL_INSTANCE(Type, Func)                                               \
        if (dynamic_api::Func == nullptr)                                                           \
        {                                                                                           \
            GP_LOG_WARNING(gp::LogRHI, "Optional Vulkan instance entry point missing: {}", #Func); \
        }
    GP_ENUM_VK_ENTRYPOINTS_OPTIONAL_INSTANCE(GP_CHECK_VK_OPTIONAL_INSTANCE)
    GP_ENUM_VK_ENTRYPOINTS_OPTIONAL_PLATFORM_INSTANCE(GP_CHECK_VK_OPTIONAL_INSTANCE)
    #undef GP_CHECK_VK_OPTIONAL_INSTANCE
#endif

    if (!checkRayTracingInstanceFunctions())
    {
        GP_LOG_WARNING(
            gp::LogVulkanRHI, "Vulkan ray tracing entry points are incomplete for the active Vulkan instance."
        );
    }

    GP_ENUM_VK_ENTRYPOINTS_PLATFORM_INSTANCE(GP_LOAD_VK_INSTANCE_PROC)
    GP_ENUM_VK_ENTRYPOINTS_PLATFORM_INSTANCE(GP_CHECK_VK_PLATFORM_INSTANCE)

#undef GP_CHECK_VK_PLATFORM_INSTANCE
#undef GP_CHECK_VK_SURFACE_INSTANCE
#undef GP_CHECK_VK_CORE_INSTANCE
#undef GP_CHECK_VK_INSTANCE_FLAG
#undef GP_LOAD_VK_INSTANCE_PROC

    if (!foundAllPlatformInstance && !renderOffScreen)
    {
        clearInstanceFunctions();
        return false;
    }

    return true;
}

bool VulkanLoader::loadDeviceFunctions(const VkDevice device) noexcept
{
    if (device == VK_NULL_HANDLE || dynamic_api::vkGetDeviceProcAddr == nullptr)
    {
        return false;
    }

#define GP_UPGRADE_VK_DEVICE_PROC(Type, Func)                                                  \
    if (const PFN_vkVoidFunction deviceProc = dynamic_api::vkGetDeviceProcAddr(device, #Func); \
        deviceProc != nullptr)                                                                 \
    {                                                                                          \
        dynamic_api::Func = castVoidFuncToPfn<Type>(deviceProc);                               \
    }

    GP_ENUM_VK_ENTRYPOINTS_INSTANCE(GP_UPGRADE_VK_DEVICE_PROC)
    GP_ENUM_VK_ENTRYPOINTS_OPTIONAL_INSTANCE(GP_UPGRADE_VK_DEVICE_PROC)
    GP_ENUM_VK_ENTRYPOINTS_PLATFORM_INSTANCE(GP_UPGRADE_VK_DEVICE_PROC)
    GP_ENUM_VK_ENTRYPOINTS_OPTIONAL_PLATFORM_INSTANCE(GP_UPGRADE_VK_DEVICE_PROC)

#undef GP_UPGRADE_VK_DEVICE_PROC

    return true;
}

bool VulkanLoader::checkRayTracingInstanceFunctions() noexcept
{
    bool foundAllRayTracing = true;

#define GP_CHECK_VK_RT_PROC(Type, Func) \
    if (dynamic_api::Func == nullptr)   \
    {                                   \
        foundAllRayTracing = false;     \
    }

    GP_ENUM_VK_ENTRYPOINTS_RAYTRACING(GP_CHECK_VK_RT_PROC)

#undef GP_CHECK_VK_RT_PROC

    return foundAllRayTracing;
}

void VulkanLoader::clearInstanceFunctions() noexcept
{
#define GP_CLEAR_VK_ENTRYPOINT(Type, Func) dynamic_api::Func = nullptr;
    GP_ENUM_VK_ENTRYPOINTS_INSTANCE(GP_CLEAR_VK_ENTRYPOINT)
    GP_ENUM_VK_ENTRYPOINTS_SURFACE_INSTANCE(GP_CLEAR_VK_ENTRYPOINT)
    GP_ENUM_VK_ENTRYPOINTS_OPTIONAL_INSTANCE(GP_CLEAR_VK_ENTRYPOINT)
    GP_ENUM_VK_ENTRYPOINTS_OPTIONAL_PLATFORM_INSTANCE(GP_CLEAR_VK_ENTRYPOINT)
    GP_ENUM_VK_ENTRYPOINTS_PLATFORM_INSTANCE(GP_CLEAR_VK_ENTRYPOINT)
#undef GP_CLEAR_VK_ENTRYPOINT
}

void VulkanLoader::clearAllFunctions() noexcept
{
#define GP_CLEAR_VK_ENTRYPOINT(Type, Func) dynamic_api::Func = nullptr;
    GP_ENUM_VK_ENTRYPOINTS_ALL(GP_CLEAR_VK_ENTRYPOINT)
#undef GP_CLEAR_VK_ENTRYPOINT
}

PFN_vkVoidFunction VulkanLoader::getInstanceProcAddr(const VkInstance instance, const char* const procName) noexcept
{
    if (dynamic_api::vkGetInstanceProcAddr == nullptr || procName == nullptr)
    {
        return nullptr;
    }

    return dynamic_api::vkGetInstanceProcAddr(instance, procName);
}

PFN_vkVoidFunction VulkanLoader::getDeviceProcAddr(const VkDevice device, const char* const procName) noexcept
{
    if (device == VK_NULL_HANDLE || dynamic_api::vkGetDeviceProcAddr == nullptr || procName == nullptr)
    {
        return nullptr;
    }

    return dynamic_api::vkGetDeviceProcAddr(device, procName);
}

}   // namespace gp::rhi::vulkan
