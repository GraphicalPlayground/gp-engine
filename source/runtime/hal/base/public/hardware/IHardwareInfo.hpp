// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "hardware/CPUInfo.hpp"
#include "hardware/DiskInfo.hpp"
#include "hardware/GPUInfo.hpp"
#include "hardware/MemoryInfo.hpp"
#include "hardware/OSInfo.hpp"
#include "hardware/PowerStateInfo.hpp"
#include <optional>

namespace gp::hal
{

/// @brief Interface for querying detailed hardware information about the system, including CPU, GPU, memory, storage,
/// and power state.
class IHardwareInfo
{
public:
    /// @brief Virtual destructor for proper cleanup in derived classes.
    virtual ~IHardwareInfo() = default;

public:
    /// @brief Initializes the hardware info system. Must be called before any other methods.
    /// @return True if initialization succeeded, false otherwise.
    [[nodiscard]] virtual bool initialize() noexcept = 0;

    /// @brief Get detailed information about the CPU, including core counts, cache sizes, and SIMD capabilities.
    /// @return A reference to a CPUInfo struct containing the CPU details.
    [[nodiscard]] virtual const CPUInfo& getCPUInfo() const noexcept = 0;

    /// @brief Get detailed information about the GPU, including vendor, device name, memory sizes, and driver version.
    /// @return A reference to a GPUInfo struct containing the GPU details.
    [[nodiscard]] virtual const GPUInfo& getGPUInfo() const noexcept = 0;

    /// @brief Get detailed information about the operating system, including name, version, and architecture.
    /// @return A reference to an OSInfo struct containing the OS details.
    [[nodiscard]] virtual const OSInfo& getOSInfo() const noexcept = 0;

    /// @brief Get detailed information about the system's memory, including total and available physical memory and
    /// page size.
    /// @return An optional MemoryInfo struct containing the memory details, or std::nullopt if the information could
    /// not be queried.
    /// @todo Replace `std::optional` with a custom Result type that can provide error details on failure.
    [[nodiscard]] virtual std::optional<MemoryInfo> getMemoryStatus() const noexcept = 0;

    /// @brief Get detailed information about a specific storage device, such as a hard drive or SSD.
    /// @param path The file system path to the storage device (e.g., "C:\\" on Windows or "/dev/sda" on Linux).
    /// @return An optional DiskInfo struct containing the storage device details, or std::nullopt if the device could
    /// not be queried.
    /// @todo Replace `std::optional` with a custom Result type that can provide error details on failure.
    /// @todo Replace `const char*` with a more robust path type that can handle platform-specific path formats and
    /// encodings.
    [[nodiscard]] virtual std::optional<DiskInfo> getDiskStatus(const char* path) const noexcept = 0;

    /// @brief Get information about the current power state of the system, including power source, battery level, and
    /// low power mode status.
    /// @return An optional PowerStateInfo struct containing the power state details, or std::nullopt if the information
    /// could not be queried.
    /// @todo Replace `std::optional` with a custom Result type that can provide error details on failure.
    [[nodiscard]] virtual std::optional<PowerStateInfo> getPowerState() const noexcept = 0;
};

}   // namespace gp::hal
