// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "hardware/GenericHardwareInfo.hpp"

namespace gp::hal::generic
{

GP_NODISCARD bool HardwareInfo::initialize() noexcept
{
    return m_initialized;
}

GP_NODISCARD const CPUInfo& HardwareInfo::getCPUInfo() const noexcept
{
    return m_cpuInfo;
}

GP_NODISCARD const GPUInfo& HardwareInfo::getGPUInfo() const noexcept
{
    return m_gpuInfo;
}

GP_NODISCARD const OSInfo& HardwareInfo::getOSInfo() const noexcept
{
    return m_osInfo;
}

GP_NODISCARD std::optional<MemoryInfo> HardwareInfo::getMemoryStatus() const noexcept
{
    return std::nullopt;
}

GP_NODISCARD std::optional<DiskInfo> HardwareInfo::getDiskStatus(const char* /* path */) const noexcept
{
    return std::nullopt;
}

GP_NODISCARD std::optional<PowerStateInfo> HardwareInfo::getPowerState() const noexcept
{
    return std::nullopt;
}

}   // namespace gp::hal::generic
