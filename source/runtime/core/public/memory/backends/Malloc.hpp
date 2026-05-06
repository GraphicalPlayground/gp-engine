// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "memory/backends/MemoryStatistics.hpp"
#include "memory/backends/SystemMallocObject.hpp"

namespace gp::memory
{

/// @brief
class Malloc : public SystemMallocObject
{
public:
    /// @brief
    /// @param[in] size
    /// @param[in] alignment
    /// @return
    virtual void* malloc(gp::USize size, gp::USize alignment) = 0;

    /// @brief
    /// @param[in] size
    /// @param[in] alignment
    /// @return
    virtual void* tryMalloc(gp::USize size, gp::USize alignment);

    /// @brief
    /// @param[in] ptr
    /// @param[in] newSize
    /// @param[in] alignment
    /// @return
    virtual void* realloc(void* ptr, gp::USize newSize, gp::USize alignment) = 0;

    /// @brief
    /// @param[in] ptr
    /// @param[in] newSize
    /// @param[in] alignment
    /// @return
    virtual void* tryRealloc(void* ptr, gp::USize newSize, gp::USize alignment);

    /// @brief
    /// @param[in] ptr
    virtual void free(void* ptr) = 0;

    /// @brief
    /// @param[in] size
    /// @param[in] alignment
    /// @return
    virtual void* mallocZeroed(gp::USize size, gp::USize alignment);

    /// @brief
    /// @param[in] size
    /// @param[in] alignment
    /// @return
    virtual void* tryMallocZeroed(gp::USize size, gp::USize alignment);

    /// @brief
    /// @param[in] size
    /// @param[in] alignment
    /// @return
    virtual gp::USize adjustSize(gp::USize size, gp::USize alignment) const;

    /// @brief
    /// @param[in] ptr
    /// @param[out] outSize
    /// @return
    virtual gp::USize getAllocationSize(void* ptr, gp::USize& outSize) const;

    /// @brief
    virtual void trim();

    /// @brief
    virtual void updateStatistics();

    /// @brief
    /// @return
    virtual MemoryStatistics getAllocatorStatistics() const;

    /// @brief
    /// @param[out] outStats
    virtual void getAllocatorStatistics(MemoryStatistics& outStats) const;

    /// @brief
    /// @return
    virtual bool isInternallyThreadSafe() const;

    /// @brief
    /// @return
    virtual bool validateHeap() const;

    /// @brief
    /// @return
    virtual const char* getDisplayName() const;
};

}   // namespace gp::memory
