// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "hardware/GenericHardwareInfo.hpp"

namespace gp::hal::generic
{

[[nodiscard]] bool HardwareInfo::initialize() noexcept
{
    return m_initialized;
}

[[nodiscard]] const CPUInfo& HardwareInfo::getCPUInfo() const noexcept
{
    return m_cpuInfo;
}

[[nodiscard]] const GPUInfo& HardwareInfo::getGPUInfo() const noexcept
{
    return m_gpuInfo;
}

[[nodiscard]] const OSInfo& HardwareInfo::getOSInfo() const noexcept
{
    return m_osInfo;
}

[[nodiscard]] std::optional<MemoryInfo> HardwareInfo::getMemoryStatus() const noexcept
{
    return std::nullopt;
}

[[nodiscard]] std::optional<DiskInfo> HardwareInfo::getDiskStatus(const char* /* path */) const noexcept
{
    return std::nullopt;
}

[[nodiscard]] std::optional<PowerStateInfo> HardwareInfo::getPowerState() const noexcept
{
    return std::nullopt;
}

}   // namespace gp::hal::generic
