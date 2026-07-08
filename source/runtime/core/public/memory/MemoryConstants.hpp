// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"

namespace gp::memory
{

/// @brief Struct used to hold common memory constants for all platforms.
/// These values generally don't change over the life of the executable, except for memory hotplugging and the addition
/// or removal of swap space.
struct PlatformConstants
{
    /// @brief THe amount of physical memory available on the system in bytes.
    UInt64 physicalMemoryBytes{ 0ull };

    /// @brief The amount of virtual memory available on the system in bytes.
    UInt64 virtualMemoryBytes{ 0ull };

    /// @brief The size of a physical memory page in bytes. This is the size of a single page of physical memory, which
    /// is typically 4KB on most systems.
    USize standardPageSize{ 0u };

    /// @brief The size of a binned memory page in bytes. This is the size of a single page of memory that is used for
    /// binned allocations, which may be larger than the standard page size.
    USize binnedPageSize{ 0u };

    /// @brief The granularity of standard memory allocations in bytes. This is the minimum size of a standard memory
    /// allocation, which is typically 16 bytes on most systems.
    USize standardAllocationGranularity{ 0u };

    /// @brief The granularity of binned memory allocations in bytes. This is the minimum size of a binned memory
    /// allocation, which is typically larger than the standard allocation granularity.
    USize binnedAllocationGranularity{ 0u };

    /// @brief Starting address for the available virtual address space. This is the lowest address that can be used for
    /// virtual memory allocations.
    UInt64 addressSpaceStart{ 0ull };

    /// @brief An estimate of the total size of the virtual address space available to the process in bytes.
    UInt64 addressSpaceSizeBytes{ static_cast<UInt64>(0xffffffff + 1) };

    /// @brief Apporximate physical RAM size in GB. This is a rough estimate of the total amount of physical RAM
    /// available on the system, which may be used for memory management decisions.
    UInt32 totalPhysicalMemoryGB{ 1u };
};

}   // namespace gp::memory
