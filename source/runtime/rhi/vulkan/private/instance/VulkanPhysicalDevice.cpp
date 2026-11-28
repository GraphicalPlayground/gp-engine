// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "instance/VulkanPhysicalDevice.hpp"
#include "console/AutoConsoleVariable.hpp"
#include "console/ConsoleEnums.hpp"
#include "console/IConsoleManager.hpp"
#include "console/IConsoleVariable.hpp"
#include "containers/arrays/Vector.hpp"      // IWYU pragma: keep
#include "containers/strings/String.hpp"     // IWYU pragma: keep
#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "device/DeviceEnums.hpp"
#include "device/RHIGlobals.hpp"
#include "loader/VulkanCommandWrappers.hpp"
#include "logging/LogMacros.hpp"
#include "memory/Memory.hpp"
#include "miscellaneous/CommandLine.hpp"
#include "templates/Optional.hpp"
#include "utils/VulkanResult.hpp"
#include "VulkanCommon.hpp"
#include <cstdio>

namespace gp::rhi::vulkan
{

namespace
{

gp::console::AutoConsoleVariable<bool> g_cvarVulkanSupportsTimestampQueries(
    "r.Vulkan.SupportsTimestampQueries",
    true,
    "State of Vulkan timestamp render query support on the current device (0=unsupported, 1=supported).",
    gp::console::ConsoleVarFlags::RenderThreadSafe
);

void stableSortCandidateIndices(
    gp::Vector<gp::USize>& indices, const gp::ConstVectorView<VulkanPhysicalDeviceInfo> candidates
) noexcept
{
    // Insertion sort preserves relative order for equal device types (small N: typically 1..4 GPUs).
    for (gp::USize i = 1u; i < indices.size(); ++i)
    {
        const gp::USize key = indices[i];
        const VkPhysicalDeviceType keyType = candidates[key].properties2.properties.deviceType;
        const gp::UInt32 keyOrig = candidates[key].originalIndex;

        gp::USize j = i;
        while (j > 0u)
        {
            const gp::USize prev = indices[j - 1u];
            const VkPhysicalDeviceType prevType = candidates[prev].properties2.properties.deviceType;
            const gp::UInt32 prevOrig = candidates[prev].originalIndex;

            bool keyBeforePrev = false;
            if (keyType == prevType)
            {
                keyBeforePrev = (keyOrig < prevOrig);
            }
            else
            {
                // Prefer discrete GPUs first, then integrated/virtual/other, then CPU last.
                keyBeforePrev =
                    (keyType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) || (prevType == VK_PHYSICAL_DEVICE_TYPE_CPU);
            }

            if (!keyBeforePrev)
            {
                break;
            }

            indices[j] = prev;
            --j;
        }
        indices[j] = key;
    }
}

}   // namespace

gp::rhi::GpuVendorId resolveVulkanVendorId(const gp::UInt32 rawVendorId) noexcept
{
    if (rawVendorId > 0xFFFFu)
    {
        switch (static_cast<VkVendorId>(rawVendorId))
        {
        case VK_VENDOR_ID_VIV:
            return gp::rhi::GpuVendorId::Vivante;
        case VK_VENDOR_ID_VSI:
            return gp::rhi::GpuVendorId::VeriSilicon;
        case VK_VENDOR_ID_KAZAN:
            return gp::rhi::GpuVendorId::Kazan;
        case VK_VENDOR_ID_CODEPLAY:
            return gp::rhi::GpuVendorId::Codeplay;
        case VK_VENDOR_ID_MESA:
            return gp::rhi::GpuVendorId::Mesa;
        default:
            GP_LOG_WARNING(LogVulkanRHI, "Unhandled Khronos VkVendorId {}.", rawVendorId);
            return gp::rhi::convertToGpuVendorId(rawVendorId);
        }
    }

    return gp::rhi::convertToGpuVendorId(rawVendorId);
}

gp::rhi::GpuVendorId resolvePreferredAdapterVendor() noexcept
{
    if (gp::CommandLine::hasArgument("prefernvidia"))
    {
        return gp::rhi::GpuVendorId::Nvidia;
    }
    if (gp::CommandLine::hasArgument("preferamd"))
    {
        return gp::rhi::GpuVendorId::Amd;
    }
    if (gp::CommandLine::hasArgument("preferintel"))
    {
        return gp::rhi::GpuVendorId::Intel;
    }
    if (gp::CommandLine::hasArgument("preferarm"))
    {
        return gp::rhi::GpuVendorId::Arm;
    }
    if (gp::CommandLine::hasArgument("preferqualcomm"))
    {
        return gp::rhi::GpuVendorId::Qualcomm;
    }
    if (gp::CommandLine::hasArgument("preferapple"))
    {
        return gp::rhi::GpuVendorId::Apple;
    }

    return gp::rhi::GpuVendorId::Unknown;
}

VulkanPhysicalDeviceSelectionDesc resolvePhysicalDeviceSelectionDesc() noexcept
{
    VulkanPhysicalDeviceSelectionDesc desc{};
    desc.explicitAdapterIndex = -1;
    desc.adapterOriginLabel = "'r.GraphicsAdapter'";

    if (gp::console::IConsoleManager::isGlobalInitialized())
    {
        gp::console::IConsoleVariable* const cvar =
            gp::console::IConsoleManager::get().findConsoleVariable(gp::StringView("r.GraphicsAdapter"));
        if (cvar != nullptr)
        {
            desc.explicitAdapterIndex = cvar->getInt();
        }
    }

    const gp::Optional<gp::Int32> cliAdapter = gp::CommandLine::tryGetInt32("graphicsadapter");
    if (cliAdapter.hasValue())
    {
        desc.explicitAdapterIndex = cliAdapter.value();
        desc.adapterOriginLabel = "command line";
    }

    desc.preferredVendor = resolvePreferredAdapterVendor();
    desc.allowCpuDevices =
        gp::CommandLine::hasArgument("AllowCPUDevices") || gp::CommandLine::hasArgument("AllowSoftwareRendering");

    return desc;
}

gp::UInt64 computeDedicatedVideoMemory(const VkPhysicalDeviceMemoryProperties& memoryProperties) noexcept
{
    gp::UInt64 totalDeviceLocalBytes = 0u;
    const gp::UInt32 heapCount = (memoryProperties.memoryHeapCount <= VK_MAX_MEMORY_HEAPS)
                                     ? memoryProperties.memoryHeapCount
                                     : static_cast<gp::UInt32>(VK_MAX_MEMORY_HEAPS);

    for (gp::UInt32 i = 0u; i < heapCount; ++i)
    {
        if ((memoryProperties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) != 0u)
        {
            totalDeviceLocalBytes += static_cast<gp::UInt64>(memoryProperties.memoryHeaps[i].size);
        }
    }

    return totalDeviceLocalBytes;
}

gp::String formatVulkanDriverVersion(const gp::rhi::GpuVendorId vendorId, const gp::UInt32 rawDriverVersion) noexcept
{
    char buffer[64]{};
    if (vendorId == gp::rhi::GpuVendorId::Nvidia)
    {
        const VulkanNvidiaDriverVersion nvVersion = VulkanNvidiaDriverVersion::unpack(rawDriverVersion);
        std::snprintf(buffer, sizeof(buffer), "%u.%02u", nvVersion.major, nvVersion.minor);
    }
    else
    {
        std::snprintf(
            buffer,
            sizeof(buffer),
            "%u.%u.%u",
            VK_VERSION_MAJOR(rawDriverVersion),
            VK_VERSION_MINOR(rawDriverVersion),
            VK_VERSION_PATCH(rawDriverVersion)
        );
    }
    return gp::String(buffer);
}

gp::String formatVulkanApiVersion(const gp::UInt32 apiVersion) noexcept
{
    char buffer[64]{};
    std::snprintf(
        buffer,
        sizeof(buffer),
        "%u.%u.%u",
        VK_API_VERSION_MAJOR(apiVersion),
        VK_API_VERSION_MINOR(apiVersion),
        VK_API_VERSION_PATCH(apiVersion)
    );
    return gp::String(buffer);
}

void populatePhysicalDeviceInfo(VulkanPhysicalDeviceInfo& info) noexcept
{
    const VkPhysicalDeviceProperties& props = info.properties2.properties;

    info.vendorId = resolveVulkanVendorId(props.vendorID);
    info.dedicatedVideoMemoryBytes = computeDedicatedVideoMemory(info.memoryProperties);
    info.deviceIsIntegrated = (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU);

    gp::memory::copyMemory(info.deviceUuid.data(), info.idProperties.deviceUUID, VK_UUID_SIZE);
    gp::memory::copyMemory(info.driverUuid.data(), info.idProperties.driverUUID, VK_UUID_SIZE);

    info.hasValidLuid = (info.idProperties.deviceLUIDValid == VK_TRUE);
    if (info.hasValidLuid)
    {
        static_assert(sizeof(info.luid) == VK_LUID_SIZE, "VulkanPhysicalDeviceInfo::luid must match VK_LUID_SIZE.");
        gp::memory::copyMemory(&info.luid, info.idProperties.deviceLUID, VK_LUID_SIZE);
    }
    else
    {
        info.luid = 0u;
    }

    info.formattedDriverVersion = formatVulkanDriverVersion(info.vendorId, props.driverVersion);
    info.formattedApiVersion = formatVulkanApiVersion(props.apiVersion);
}

VulkanPhysicalDeviceInfo inspectPhysicalDevice(
    const VkPhysicalDevice physicalDevice, const gp::UInt32 originalIndex, const gp::UInt32 instanceApiVersion
) noexcept
{
    VulkanPhysicalDeviceInfo info{};
    info.originalIndex = originalIndex;
    info.physicalDevice = physicalDevice;

    info.properties2 = zeroVulkanStruct<VkPhysicalDeviceProperties2>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2);
    info.idProperties = zeroVulkanStruct<VkPhysicalDeviceIDProperties>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES);
    info.driverProperties =
        zeroVulkanStruct<VkPhysicalDeviceDriverProperties>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES);
    gp::memory::zeroMemory(&info.memoryProperties, sizeof(info.memoryProperties));

    if (physicalDevice == VK_NULL_HANDLE)
    {
        return info;
    }

    if (dynamic_api::vkGetPhysicalDeviceProperties2 != nullptr)
    {
        info.properties2.pNext = &info.idProperties;
        if (instanceApiVersion >= VK_API_VERSION_1_2)
        {
            info.idProperties.pNext = &info.driverProperties;
        }
        vkGetPhysicalDeviceProperties2(physicalDevice, &info.properties2);

        // Detach stack-local pNext pointers before returning by value so copies never dangle.
        info.properties2.pNext = nullptr;
        info.idProperties.pNext = nullptr;

        info.hasValidDriverProperties = (instanceApiVersion >= VK_API_VERSION_1_2) &&
                                        (info.properties2.properties.apiVersion >= VK_API_VERSION_1_2) &&
                                        (info.driverProperties.driverID != static_cast<VkDriverId>(0));
    }
    else if (dynamic_api::vkGetPhysicalDeviceProperties != nullptr)
    {
        vkGetPhysicalDeviceProperties(physicalDevice, &info.properties2.properties);
    }

    if (dynamic_api::vkGetPhysicalDeviceMemoryProperties != nullptr)
    {
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &info.memoryProperties);
    }

    populatePhysicalDeviceInfo(info);
    return info;
}

gp::Vector<VulkanPhysicalDeviceInfo>
    enumeratePhysicalDevices(const VkInstance instance, const gp::UInt32 instanceApiVersion) noexcept
{
    gp::Vector<VulkanPhysicalDeviceInfo> deviceInfos;

    if (instance == VK_NULL_HANDLE || dynamic_api::vkEnumeratePhysicalDevices == nullptr)
    {
        return deviceInfos;
    }

    gp::UInt32 physicalDeviceCount = 0u;
    VkResult result = vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, nullptr);
    if (result != VK_SUCCESS || physicalDeviceCount == 0u)
    {
        GP_LOG_INFO(
            LogVulkanRHI,
            "enumeratePhysicalDevices found no compatible Vulkan device (returned '{}' and {} devices).",
            vulkanResultToString(result),
            physicalDeviceCount
        );
        return deviceInfos;
    }

    gp::Vector<VkPhysicalDevice> handles(physicalDeviceCount, VK_NULL_HANDLE);
    result = vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, handles.data());
    if ((result != VK_SUCCESS && result != VK_INCOMPLETE) || physicalDeviceCount == 0u)
    {
        GP_LOG_ERROR(
            LogVulkanRHI, "vkEnumeratePhysicalDevices failed on second attempt with '{}'.", vulkanResultToString(result)
        );
        return deviceInfos;
    }

    deviceInfos.reserve(physicalDeviceCount);
    for (gp::UInt32 index = 0u; index < physicalDeviceCount; ++index)
    {
        deviceInfos.pushBack(inspectPhysicalDevice(handles[index], index, instanceApiVersion));
    }

    return deviceInfos;
}

gp::Int32 selectPhysicalDeviceIndex(
    const gp::ConstVectorView<VulkanPhysicalDeviceInfo> candidates, const VulkanPhysicalDeviceSelectionDesc& desc
) noexcept
{
    if (candidates.isEmpty())
    {
        return -1;
    }

    const gp::UInt32 candidateCount = static_cast<gp::UInt32>(candidates.size());
    const char* const originLabel =
        (desc.adapterOriginLabel != nullptr) ? desc.adapterOriginLabel : "'r.GraphicsAdapter'";

    // 1. Preferred LUID override (e.g., HMD-attached GPU).
    if (desc.hasPreferredLuid)
    {
        for (gp::USize index = 0u; index < candidates.size(); ++index)
        {
            if (candidates[index].hasValidLuid && candidates[index].luid == desc.preferredLuid)
            {
                GP_LOG_INFO(
                    LogVulkanRHI,
                    "Preferred LUID device at index {} of {} selected.",
                    static_cast<gp::UInt32>(index),
                    candidateCount
                );
                return static_cast<gp::Int32>(index);
            }
        }
    }

    // 2. Explicit adapter index from CVar or command line.
    if (desc.explicitAdapterIndex >= 0)
    {
        gp::Int32 targetIndex = desc.explicitAdapterIndex;
        if (static_cast<gp::USize>(targetIndex) >= candidates.size())
        {
            GP_LOG_WARNING(
                LogVulkanRHI,
                "Requested graphics adapter index {} from {}, but only {} adapter(s) found. Falling back to index 0.",
                targetIndex,
                originLabel,
                candidateCount
            );
            targetIndex = 0;
        }

        GP_LOG_INFO(
            LogVulkanRHI, "Using device at index {} of {} as specified by {}.", targetIndex, candidateCount, originLabel
        );
        return targetIndex;
    }

    // 3. Explicit unsorted first device (-2).
    if (desc.explicitAdapterIndex == -2)
    {
        GP_LOG_INFO(
            LogVulkanRHI, "Using first device (of {}) without sorting as specified by {}.", candidateCount, originLabel
        );
        return 0;
    }

    // 4. Build index ordering (discrete-first stable sort when explicitAdapterIndex == -1).
    gp::Vector<gp::USize> orderedIndices;
    orderedIndices.reserve(candidates.size());
    for (gp::USize index = 0u; index < candidates.size(); ++index)
    {
        orderedIndices.pushBack(index);
    }

    if (desc.explicitAdapterIndex == -1)
    {
        stableSortCandidateIndices(orderedIndices, candidates);
    }

    // 5. Preferred vendor filter.
    if (desc.preferredVendor != gp::rhi::GpuVendorId::Unknown)
    {
        for (const gp::USize candidateIdx: orderedIndices)
        {
            const gp::rhi::GpuVendorId vendor =
                resolveVulkanVendorId(candidates[candidateIdx].properties2.properties.vendorID);
            if (vendor == desc.preferredVendor)
            {
                GP_LOG_INFO(
                    LogVulkanRHI,
                    "Using preferred vendor device at index {} of {}.",
                    static_cast<gp::UInt32>(candidateIdx),
                    candidateCount
                );
                return static_cast<gp::Int32>(candidateIdx);
            }
        }
    }

    // 6. Skip CPU software devices unless explicitly permitted.
    for (const gp::USize candidateIdx: orderedIndices)
    {
        if (!desc.allowCpuDevices &&
            candidates[candidateIdx].properties2.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU)
        {
            continue;
        }

        return static_cast<gp::Int32>(candidateIdx);
    }

    GP_LOG_WARNING(LogVulkanRHI, "None of the {} Vulkan physical device(s) met selection criteria.", candidateCount);
    return -1;
}

VkPhysicalDevice selectPhysicalDevice(
    const VkInstance instance,
    const gp::UInt32 instanceApiVersion,
    VulkanPhysicalDeviceInfo& outSelectedInfo,
    const VulkanPhysicalDeviceSelectionDesc* const overrideDesc
) noexcept
{
    outSelectedInfo = VulkanPhysicalDeviceInfo{};

    const gp::Vector<VulkanPhysicalDeviceInfo> candidates = enumeratePhysicalDevices(instance, instanceApiVersion);
    if (candidates.isEmpty())
    {
        return VK_NULL_HANDLE;
    }

    const VulkanPhysicalDeviceSelectionDesc selectionDesc =
        (overrideDesc != nullptr) ? *overrideDesc : resolvePhysicalDeviceSelectionDesc();

    const gp::Int32 selectedIndex = selectPhysicalDeviceIndex(
        gp::ConstVectorView<VulkanPhysicalDeviceInfo>(candidates.data(), candidates.size()), selectionDesc
    );
    if (selectedIndex < 0)
    {
        return VK_NULL_HANDLE;
    }

    outSelectedInfo = candidates[static_cast<gp::USize>(selectedIndex)];
    return outSelectedInfo.physicalDevice;
}

gp::UInt64 getGraphicsAdapterLuid(const VkPhysicalDevice physicalDevice) noexcept
{
    if (physicalDevice == VK_NULL_HANDLE || dynamic_api::vkGetPhysicalDeviceProperties2 == nullptr)
    {
        return 0u;
    }

    VkPhysicalDeviceProperties2 gpuProps2 =
        zeroVulkanStruct<VkPhysicalDeviceProperties2>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2);
    VkPhysicalDeviceIDProperties gpuIdProps =
        zeroVulkanStruct<VkPhysicalDeviceIDProperties>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES);
    gpuProps2.pNext = &gpuIdProps;

    vkGetPhysicalDeviceProperties2(physicalDevice, &gpuProps2);
    if (gpuIdProps.deviceLUIDValid == VK_TRUE)
    {
        gp::UInt64 adapterLuid = 0u;
        static_assert(sizeof(adapterLuid) == VK_LUID_SIZE, "Adapter LUID size must match VK_LUID_SIZE.");
        gp::memory::copyMemory(&adapterLuid, gpuIdProps.deviceLUID, VK_LUID_SIZE);
        return adapterLuid;
    }

    return 0u;
}

bool doesAdapterMatchDevice(const VulkanPhysicalDeviceInfo& info, const void* const adapterId) noexcept
{
    if (adapterId == nullptr || !info.hasValidLuid)
    {
        return true;
    }

    return gp::memory::compareMemory(adapterId, info.idProperties.deviceLUID, VK_LUID_SIZE) == 0;
}

void populateRHIGlobalsGpuInfo(const VulkanPhysicalDeviceInfo& info, gp::rhi::GPUInfo& outGpuInfo) noexcept
{
    const VkPhysicalDeviceProperties& props = info.properties2.properties;

    outGpuInfo.adapterName = gp::String(props.deviceName);
#if defined(GP_PLATFORM_ANDROID) && GP_PLATFORM_ANDROID
    outGpuInfo.adapterName.append(" Vulkan");
#endif

    outGpuInfo.vendorId = info.vendorId;
    outGpuInfo.deviceId = props.deviceID;
    outGpuInfo.deviceRevision = 0u;
    outGpuInfo.dedicatedVideoMemory = info.dedicatedVideoMemoryBytes;
    outGpuInfo.deviceIsIntegrated = info.deviceIsIntegrated;

    if (!info.formattedDriverVersion.isEmpty())
    {
        outGpuInfo.driverVersion = info.formattedDriverVersion;
    }
    else
    {
        outGpuInfo.driverVersion = formatVulkanDriverVersion(info.vendorId, props.driverVersion);
    }

#if GP_PLATFORM_LINUX
    outGpuInfo.driverDate = gp::String("01-01-01");
#else
    outGpuInfo.driverDate.clear();
#endif

    GP_LOG_INFO(
        LogVulkanRHI,
        "Selected Vulkan Adapter: '{}' (DeviceId: 0x{:X}, API Version: {}, Driver: {}, Dedicated VRAM: {} MB).",
        outGpuInfo.adapterName.data(),
        outGpuInfo.deviceId,
        info.formattedApiVersion.data(),
        outGpuInfo.driverVersion.data(),
        static_cast<gp::UInt64>(outGpuInfo.dedicatedVideoMemory / (1024u * 1024u))
    );
}

void applySelectedPhysicalDeviceToRHIGlobals(const VulkanPhysicalDeviceInfo& info) noexcept
{
    gp::rhi::RHIGlobals& globals = gp::rhi::getRHIGlobals();
    populateRHIGlobalsGpuInfo(info, globals.gpuInfo);

    const bool cvarAllowsTimestamps = g_cvarVulkanSupportsTimestampQueries.getValueOnAnyThread(true);
    globals.supportsTimestampQueries =
        cvarAllowsTimestamps && (info.properties2.properties.limits.timestampPeriod > 0.0f);
}

}   // namespace gp::rhi::vulkan
