// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "memory/MemoryBase.hpp"
#include "memory/SystemMallocForNew.hpp"

namespace gp::memory
{

/// @brief
/// @details
/// @see
class GP_CORE_API Malloc : public UseSystemMallocForNew
{
public:
    /// @brief
    virtual ~Malloc() = default;

public:
    /// @brief
    /// @param[in] size
    /// @param[in] alignment
    /// @return
    [[nodiscard]] virtual void* allocate(USize size, UInt32 alignment = kDefaultAlignment) = 0;

    /// @brief
    /// @param[in] size
    /// @param[in] alignment
    /// @return
    [[nodiscard]] virtual void* tryAllocate(USize size, UInt32 alignment = kDefaultAlignment) noexcept;

    /// @brief
    /// @param[in] size
    /// @param[in] alignment
    /// @return
    [[nodiscard]] virtual void* allocateZeroed(USize size, UInt32 alignment = kDefaultAlignment);

    /// @brief
    /// @param[in] size
    /// @param[in] alignment
    /// @return
    [[nodiscard]] virtual void* tryAllocateZeroed(USize size, UInt32 alignment = kDefaultAlignment) noexcept;

    /// @brief
    /// @param[in] ptr
    /// @param[in] newSize
    /// @param[in] alignment
    /// @return
    [[nodiscard]] virtual void* reallocate(void* ptr, USize newSize, UInt32 alignment = kDefaultAlignment) = 0;

    /// @brief
    /// @param[in] ptr
    /// @param[in] newSize
    /// @param[in] alignment
    /// @return
    [[nodiscard]] virtual void* tryReallocate(void* ptr, USize newSize, UInt32 alignment = kDefaultAlignment) noexcept;

    /// @brief
    /// @param[in] ptr
    virtual void deallocate(void* ptr) = 0;

    /// @brief
    /// @param[in] ptr
    /// @return
    virtual USize getAllocationSize(void* ptr);

    /// @brief
    /// @return
    virtual bool canGetAllocationSize();
};

}   // namespace gp::memory
