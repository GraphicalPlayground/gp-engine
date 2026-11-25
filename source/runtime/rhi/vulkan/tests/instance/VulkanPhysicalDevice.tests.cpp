// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "instance/VulkanPhysicalDevice.hpp"
#include "containers/arrays/Vector.hpp"      // IWYU pragma: keep
#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "device/DeviceEnums.hpp"
#include "device/RHIGlobals.hpp"
#include "instance/VulkanInstanceDesc.hpp"
#include "memory/Memory.hpp"
#include "miscellaneous/CommandLine.hpp"
#include "VulkanCommon.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <cstring>
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

namespace
{

class ScopedTestCommandLine final
{
public:
    explicit ScopedTestCommandLine(const gp::StringView args) noexcept
    {
        gp::CommandLine::shutdown();
        gp::CommandLine::initialize(args);
    }

    ~ScopedTestCommandLine() noexcept
    {
        gp::CommandLine::shutdown();
    }

    ScopedTestCommandLine(const ScopedTestCommandLine&) = delete;
    ScopedTestCommandLine& operator=(const ScopedTestCommandLine&) = delete;
};

VulkanPhysicalDeviceInfo makeCandidate(
    const gp::UInt32 originalIndex,
    const char* const name,
    const VkPhysicalDeviceType deviceType,
    const gp::UInt32 vendorId,
    const gp::UInt32 deviceId,
    const gp::UInt64 vramBytes,
    const gp::UInt64 luid = 0u,
    const bool hasLuid = false
) noexcept
{
    VulkanPhysicalDeviceInfo info{};
    info.originalIndex = originalIndex;
    info.physicalDevice = reinterpret_cast<VkPhysicalDevice>(static_cast<gp::UIntPtr>(0x1000u + originalIndex));
    info.properties2 = zeroVulkanStruct<VkPhysicalDeviceProperties2>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2);
    info.idProperties = zeroVulkanStruct<VkPhysicalDeviceIDProperties>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES);

    const gp::USize nameLen = std::strlen(name);
    const gp::USize copyLen = (nameLen < VK_MAX_PHYSICAL_DEVICE_NAME_SIZE - 1u)
                                  ? nameLen
                                  : static_cast<gp::USize>(VK_MAX_PHYSICAL_DEVICE_NAME_SIZE - 1u);
    gp::memory::copyMemory(info.properties2.properties.deviceName, name, copyLen);

    info.properties2.properties.deviceType = deviceType;
    info.properties2.properties.vendorID = vendorId;
    info.properties2.properties.deviceID = deviceId;
    info.properties2.properties.apiVersion = VK_API_VERSION_1_3;
    info.properties2.properties.driverVersion = VK_MAKE_VERSION(24u, 2u, 1u);
    info.properties2.properties.limits.timestampPeriod = 1.0f;

    info.memoryProperties.memoryHeapCount = 2u;
    info.memoryProperties.memoryHeaps[0].size = vramBytes;
    info.memoryProperties.memoryHeaps[0].flags = VK_MEMORY_HEAP_DEVICE_LOCAL_BIT;
    info.memoryProperties.memoryHeaps[1].size = 16ull * 1024ull * 1024ull * 1024ull;
    info.memoryProperties.memoryHeaps[1].flags = 0u;

    for (gp::USize i = 0u; i < VK_UUID_SIZE; ++i)
    {
        info.idProperties.deviceUUID[i] = static_cast<gp::UInt8>(originalIndex + i);
        info.idProperties.driverUUID[i] = static_cast<gp::UInt8>(0xA0u + i);
    }

    if (hasLuid)
    {
        info.idProperties.deviceLUIDValid = VK_TRUE;
        gp::memory::copyMemory(info.idProperties.deviceLUID, &luid, VK_LUID_SIZE);
    }

    populatePhysicalDeviceInfo(info);
    return info;
}

}   // namespace

TEST(VulkanPhysicalDeviceTest, NvidiaDriverVersionPackAndUnpack)
{
    const gp::UInt32 packed = VulkanNvidiaDriverVersion::pack(560u, 81u, 3u, 5u);
    const VulkanNvidiaDriverVersion unpacked = VulkanNvidiaDriverVersion::unpack(packed);
    EXPECT_EQ(unpacked.major, 560u);
    EXPECT_EQ(unpacked.minor, 81u);
    EXPECT_EQ(unpacked.secondary, 3u);
    EXPECT_EQ(unpacked.tertiary, 5u);

    const gp::String nvStr = formatVulkanDriverVersion(gp::rhi::GpuVendorId::Nvidia, packed);
    EXPECT_STREQ(nvStr.data(), "560.81");

    const gp::String nvZeroPad =
        formatVulkanDriverVersion(gp::rhi::GpuVendorId::Nvidia, VulkanNvidiaDriverVersion::pack(555u, 7u));
    EXPECT_STREQ(nvZeroPad.data(), "555.07");

    const gp::String stdStr = formatVulkanDriverVersion(gp::rhi::GpuVendorId::Amd, VK_MAKE_VERSION(24u, 10u, 2u));
    EXPECT_STREQ(stdStr.data(), "24.10.2");

    const gp::String apiStr = formatVulkanApiVersion(VK_API_VERSION_1_3);
    EXPECT_STREQ(apiStr.data(), "1.3.0");
}

TEST(VulkanPhysicalDeviceTest, ResolveVendorIdHandlesPciAndKhronosVendorIds)
{
    EXPECT_EQ(resolveVulkanVendorId(0x10DEu), gp::rhi::GpuVendorId::Nvidia);
    EXPECT_EQ(resolveVulkanVendorId(0x1002u), gp::rhi::GpuVendorId::Amd);
    EXPECT_EQ(resolveVulkanVendorId(0x8086u), gp::rhi::GpuVendorId::Intel);
    EXPECT_EQ(resolveVulkanVendorId(0x13B5u), gp::rhi::GpuVendorId::Arm);
    EXPECT_EQ(resolveVulkanVendorId(0x5143u), gp::rhi::GpuVendorId::Qualcomm);
    EXPECT_EQ(resolveVulkanVendorId(0x106Bu), gp::rhi::GpuVendorId::Apple);
    EXPECT_EQ(resolveVulkanVendorId(0u), gp::rhi::GpuVendorId::NotQueried);
    EXPECT_EQ(resolveVulkanVendorId(0xBEEFu), gp::rhi::GpuVendorId::Unknown);

    EXPECT_EQ(resolveVulkanVendorId(static_cast<gp::UInt32>(VK_VENDOR_ID_VIV)), gp::rhi::GpuVendorId::Vivante);
    EXPECT_EQ(resolveVulkanVendorId(static_cast<gp::UInt32>(VK_VENDOR_ID_VSI)), gp::rhi::GpuVendorId::VeriSilicon);
    EXPECT_EQ(resolveVulkanVendorId(static_cast<gp::UInt32>(VK_VENDOR_ID_KAZAN)), gp::rhi::GpuVendorId::Kazan);
    EXPECT_EQ(resolveVulkanVendorId(static_cast<gp::UInt32>(VK_VENDOR_ID_CODEPLAY)), gp::rhi::GpuVendorId::Codeplay);
    EXPECT_EQ(resolveVulkanVendorId(static_cast<gp::UInt32>(VK_VENDOR_ID_MESA)), gp::rhi::GpuVendorId::Mesa);
    EXPECT_EQ(resolveVulkanVendorId(0x20000u), gp::rhi::GpuVendorId::Unknown);
}

TEST(VulkanPhysicalDeviceTest, ResolveSelectionDescAndPreferredVendorFromCommandLine)
{
    {
        ScopedTestCommandLine cmd("app.exe");
        const VulkanPhysicalDeviceSelectionDesc desc = resolvePhysicalDeviceSelectionDesc();
        EXPECT_EQ(desc.explicitAdapterIndex, -1);
        EXPECT_EQ(desc.preferredVendor, gp::rhi::GpuVendorId::Unknown);
        EXPECT_FALSE(desc.allowCpuDevices);
    }

    {
        ScopedTestCommandLine cmd("app.exe -graphicsadapter=2 -preferamd -AllowCPUDevices");
        const VulkanPhysicalDeviceSelectionDesc desc = resolvePhysicalDeviceSelectionDesc();
        EXPECT_EQ(desc.explicitAdapterIndex, 2);
        EXPECT_STREQ(desc.adapterOriginLabel, "command line");
        EXPECT_EQ(desc.preferredVendor, gp::rhi::GpuVendorId::Amd);
        EXPECT_TRUE(desc.allowCpuDevices);
    }

    {
        ScopedTestCommandLine cmd("app.exe -prefernvidia -AllowSoftwareRendering");
        const VulkanPhysicalDeviceSelectionDesc desc = resolvePhysicalDeviceSelectionDesc();
        EXPECT_EQ(desc.preferredVendor, gp::rhi::GpuVendorId::Nvidia);
        EXPECT_TRUE(desc.allowCpuDevices);
    }

    {
        ScopedTestCommandLine cmd("app.exe -preferintel");
        EXPECT_EQ(resolvePreferredAdapterVendor(), gp::rhi::GpuVendorId::Intel);
    }
    {
        ScopedTestCommandLine cmd("app.exe -preferarm");
        EXPECT_EQ(resolvePreferredAdapterVendor(), gp::rhi::GpuVendorId::Arm);
    }
    {
        ScopedTestCommandLine cmd("app.exe -preferqualcomm");
        EXPECT_EQ(resolvePreferredAdapterVendor(), gp::rhi::GpuVendorId::Qualcomm);
    }
    {
        ScopedTestCommandLine cmd("app.exe -preferapple");
        EXPECT_EQ(resolvePreferredAdapterVendor(), gp::rhi::GpuVendorId::Apple);
    }
}

TEST(VulkanPhysicalDeviceTest, ComputeDedicatedVideoMemorySumsDeviceLocalHeapsOnly)
{
    VkPhysicalDeviceMemoryProperties memProps{};
    gp::memory::zeroMemory(&memProps, sizeof(memProps));
    memProps.memoryHeapCount = 3u;
    memProps.memoryHeaps[0].size = 8ull * 1024ull * 1024ull * 1024ull;
    memProps.memoryHeaps[0].flags = VK_MEMORY_HEAP_DEVICE_LOCAL_BIT;
    memProps.memoryHeaps[1].size = 32ull * 1024ull * 1024ull * 1024ull;
    memProps.memoryHeaps[1].flags = 0u;
    memProps.memoryHeaps[2].size = 256ull * 1024ull * 1024ull;
    memProps.memoryHeaps[2].flags = VK_MEMORY_HEAP_DEVICE_LOCAL_BIT | VK_MEMORY_HEAP_MULTI_INSTANCE_BIT;

    const gp::UInt64 expected = (8ull * 1024ull * 1024ull * 1024ull) + (256ull * 1024ull * 1024ull);
    EXPECT_EQ(computeDedicatedVideoMemory(memProps), expected);

    // Zero heaps returns 0.
    VkPhysicalDeviceMemoryProperties emptyMemProps{};
    gp::memory::zeroMemory(&emptyMemProps, sizeof(emptyMemProps));
    EXPECT_EQ(computeDedicatedVideoMemory(emptyMemProps), 0u);

    // Heap count exceeding VK_MAX_MEMORY_HEAPS is clamped safely.
    memProps.memoryHeapCount = VK_MAX_MEMORY_HEAPS + 10u;
    EXPECT_EQ(computeDedicatedVideoMemory(memProps), expected);
}

TEST(VulkanPhysicalDeviceTest, SelectPhysicalDeviceIndexEnforcesPriorityRules)
{
    // Empty candidates -> -1.
    VulkanPhysicalDeviceSelectionDesc defaultDesc{};
    EXPECT_EQ(selectPhysicalDeviceIndex({}, defaultDesc), -1);

    gp::Vector<VulkanPhysicalDeviceInfo> candidates;
    candidates.pushBack(makeCandidate(
        0u, "Intel Integrated", VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU, 0x8086u, 0x9A49u, 2ull << 30u, 0x1111u, true
    ));
    candidates.pushBack(makeCandidate(
        1u, "AMD Discrete", VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, 0x1002u, 0x744Cu, 16ull << 30u, 0x2222u, true
    ));
    candidates.pushBack(makeCandidate(
        2u, "Lavapipe CPU", VK_PHYSICAL_DEVICE_TYPE_CPU, static_cast<gp::UInt32>(VK_VENDOR_ID_MESA), 0x0001u, 0u
    ));
    candidates.pushBack(makeCandidate(
        3u, "NVIDIA Discrete", VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, 0x10DEu, 0x2684u, 24ull << 30u, 0x3333u, true
    ));

    const gp::ConstVectorView<VulkanPhysicalDeviceInfo> view(candidates.data(), candidates.size());

    // Default (-1): discrete GPUs sorted first preserving original order -> AMD Discrete (index 1).
    EXPECT_EQ(selectPhysicalDeviceIndex(view, defaultDesc), 1);

    // Preferred vendor overrides order among sorted devices -> NVIDIA Discrete (index 3).
    VulkanPhysicalDeviceSelectionDesc preferNvidia{};
    preferNvidia.preferredVendor = gp::rhi::GpuVendorId::Nvidia;
    EXPECT_EQ(selectPhysicalDeviceIndex(view, preferNvidia), 3);

    // Explicit adapter index selects exact index; out-of-range clamps to 0.
    VulkanPhysicalDeviceSelectionDesc explicitIdx{};
    explicitIdx.explicitAdapterIndex = 2;
    EXPECT_EQ(selectPhysicalDeviceIndex(view, explicitIdx), 2);
    explicitIdx.explicitAdapterIndex = 99;
    EXPECT_EQ(selectPhysicalDeviceIndex(view, explicitIdx), 0);

    // Explicit -2 picks index 0 without sorting.
    VulkanPhysicalDeviceSelectionDesc unsortedFirst{};
    unsortedFirst.explicitAdapterIndex = -2;
    EXPECT_EQ(selectPhysicalDeviceIndex(view, unsortedFirst), 0);

    // Preferred LUID override takes highest priority over explicit index and discrete sort.
    VulkanPhysicalDeviceSelectionDesc luidOverride{};
    luidOverride.explicitAdapterIndex = 1;
    luidOverride.hasPreferredLuid = true;
    luidOverride.preferredLuid = 0x3333u;
    EXPECT_EQ(selectPhysicalDeviceIndex(view, luidOverride), 3);

    // Non-matching LUID falls through to discrete-first selection.
    VulkanPhysicalDeviceSelectionDesc missingLuid{};
    missingLuid.hasPreferredLuid = true;
    missingLuid.preferredLuid = 0x9999u;
    EXPECT_EQ(selectPhysicalDeviceIndex(view, missingLuid), 1);

    // Non-matching preferred vendor falls through to discrete-first selection.
    VulkanPhysicalDeviceSelectionDesc missingVendor{};
    missingVendor.preferredVendor = gp::rhi::GpuVendorId::Apple;
    EXPECT_EQ(selectPhysicalDeviceIndex(view, missingVendor), 1);

    // [CPU, Integrated] ordering with allowCpuDevices=true still prefers Integrated over CPU.
    gp::Vector<VulkanPhysicalDeviceInfo> cpuAndIntegrated;
    cpuAndIntegrated.pushBack(makeCandidate(
        0u, "Lavapipe CPU", VK_PHYSICAL_DEVICE_TYPE_CPU, static_cast<gp::UInt32>(VK_VENDOR_ID_MESA), 0x0001u, 0u
    ));
    cpuAndIntegrated.pushBack(
        makeCandidate(1u, "Intel Integrated", VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU, 0x8086u, 0x9A49u, 2ull << 30u)
    );
    VulkanPhysicalDeviceSelectionDesc allowCpuSort{};
    allowCpuSort.allowCpuDevices = true;
    EXPECT_EQ(
        selectPhysicalDeviceIndex(
            gp::ConstVectorView<VulkanPhysicalDeviceInfo>(cpuAndIntegrated.data(), cpuAndIntegrated.size()),
            allowCpuSort
        ),
        1
    );

    // CPU-only candidate list rejected unless allowCpuDevices == true.
    gp::Vector<VulkanPhysicalDeviceInfo> cpuOnly;
    cpuOnly.pushBack(makeCandidate(
        0u, "Lavapipe CPU", VK_PHYSICAL_DEVICE_TYPE_CPU, static_cast<gp::UInt32>(VK_VENDOR_ID_MESA), 0x0001u, 0u
    ));
    const gp::ConstVectorView<VulkanPhysicalDeviceInfo> cpuView(cpuOnly.data(), cpuOnly.size());
    EXPECT_EQ(selectPhysicalDeviceIndex(cpuView, defaultDesc), -1);

    VulkanPhysicalDeviceSelectionDesc allowCpu{};
    allowCpu.allowCpuDevices = true;
    EXPECT_EQ(selectPhysicalDeviceIndex(cpuView, allowCpu), 0);
}

TEST(VulkanPhysicalDeviceTest, AdapterLuidMatchingAndRHIGlobalsPopulation)
{
    const gp::UInt64 expectedLuid = 0x0123456789ABCDEFull;
    const gp::UInt64 wrongLuid = 0xFEDCBA9876543210ull;

    const VulkanPhysicalDeviceInfo discreteInfo = makeCandidate(
        0u,
        "Test Discrete GPU",
        VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU,
        0x10DEu,
        0x2684u,
        12ull << 30u,
        expectedLuid,
        true
    );
    EXPECT_TRUE(discreteInfo.hasValidLuid);
    EXPECT_EQ(discreteInfo.luid, expectedLuid);
    EXPECT_EQ(discreteInfo.deviceUuid[0], 0u);
    EXPECT_EQ(discreteInfo.deviceUuid[VK_UUID_SIZE - 1u], static_cast<gp::UInt8>(VK_UUID_SIZE - 1u));
    EXPECT_EQ(discreteInfo.driverUuid[0], 0xA0u);
    EXPECT_TRUE(doesAdapterMatchDevice(discreteInfo, &expectedLuid));
    EXPECT_FALSE(doesAdapterMatchDevice(discreteInfo, &wrongLuid));
    EXPECT_TRUE(doesAdapterMatchDevice(discreteInfo, nullptr));

    // Null-handle guards safely return empty / zero results without dereferencing Vulkan handles.
    const VulkanPhysicalDeviceInfo nullInspect = inspectPhysicalDevice(VK_NULL_HANDLE, 0u, VK_API_VERSION_1_3);
    EXPECT_EQ(nullInspect.physicalDevice, VK_NULL_HANDLE);
    EXPECT_EQ(enumeratePhysicalDevices(VK_NULL_HANDLE, VK_API_VERSION_1_3).size(), 0u);
    EXPECT_EQ(getGraphicsAdapterLuid(VK_NULL_HANDLE), 0u);
    VulkanPhysicalDeviceInfo unusedSelected{};
    EXPECT_EQ(selectPhysicalDevice(VK_NULL_HANDLE, VK_API_VERSION_1_3, unusedSelected), VK_NULL_HANDLE);

    // Device without valid LUID always returns true.
    const VulkanPhysicalDeviceInfo noLuidInfo = makeCandidate(
        1u, "No LUID GPU", VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU, 0x8086u, 0x1234u, 4ull << 30u, 0u, false
    );
    EXPECT_FALSE(noLuidInfo.hasValidLuid);
    EXPECT_TRUE(doesAdapterMatchDevice(noLuidInfo, &wrongLuid));
    EXPECT_TRUE(noLuidInfo.deviceIsIntegrated);

    // Verify GPUInfo and RHIGlobals population (and restore RHIGlobals cleanly afterwards).
    const gp::rhi::GPUInfo previousGpuInfo = gp::rhi::getRHIGlobals().gpuInfo;
    const bool previousTimestampSupport = gp::rhi::getRHIGlobals().supportsTimestampQueries;

    applySelectedPhysicalDeviceToRHIGlobals(discreteInfo);
    const gp::rhi::GPUInfo& gpuInfo = gp::rhi::getRHIGlobals().gpuInfo;
    EXPECT_STREQ(gpuInfo.adapterName.data(), "Test Discrete GPU");
    EXPECT_EQ(gpuInfo.vendorId, gp::rhi::GpuVendorId::Nvidia);
    EXPECT_EQ(gpuInfo.deviceId, 0x2684u);
    EXPECT_EQ(gpuInfo.dedicatedVideoMemory, 12ull << 30u);
    EXPECT_FALSE(gpuInfo.deviceIsIntegrated);
    EXPECT_FALSE(gpuInfo.driverVersion.isEmpty());
    EXPECT_TRUE(gp::rhi::getRHIGlobals().supportsTimestampQueries);

    // Zero timestampPeriod disables timestamp queries.
    VulkanPhysicalDeviceInfo noTimestampInfo = discreteInfo;
    noTimestampInfo.properties2.properties.limits.timestampPeriod = 0.0f;
    applySelectedPhysicalDeviceToRHIGlobals(noTimestampInfo);
    EXPECT_FALSE(gp::rhi::getRHIGlobals().supportsTimestampQueries);

    gp::rhi::getRHIGlobals().gpuInfo = previousGpuInfo;
    gp::rhi::getRHIGlobals().supportsTimestampQueries = previousTimestampSupport;
}

}   // namespace gp::rhi::vulkan::tests
