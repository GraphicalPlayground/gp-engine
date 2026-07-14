// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "memory/backends/Malloc.hpp"

namespace gp::memory
{

/// @brief
/// @details
/// @see
class GP_CORE_API MallocAnsi final : public Malloc
{
public:
    /// @brief
    /// @param[in] size
    /// @param[in] alignment
    /// @return
    [[nodiscard]] void* allocate(USize size, UInt32 alignment) override;

    /// @brief
    /// @param[in] size
    /// @param[in] alignment
    /// @return
    [[nodiscard]] void* tryAllocate(USize size, UInt32 alignment) noexcept override;

    /// @brief
    /// @param[in] ptr
    /// @param[in] newSize
    /// @param[in] alignment
    /// @return
    [[nodiscard]] void* reallocate(void* ptr, USize newSize, UInt32 alignment) override;

    /// @brief
    /// @param[in] ptr
    /// @param[in] newSize
    /// @param[in] alignment
    /// @return
    [[nodiscard]] void* tryReallocate(void* ptr, USize newSize, UInt32 alignment) noexcept override;

    /// @brief
    /// @param[in] ptr
    void deallocate(void* ptr) override;

    /// @brief
    /// @param[in] ptr
    /// @return
    USize getAllocationSize(void* ptr) override;

    /// @brief
    /// @return
    bool canGetAllocationSize() override;
};

}   // namespace gp::memory
