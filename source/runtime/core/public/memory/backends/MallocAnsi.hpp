// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "memory/backends/Malloc.hpp"

namespace gp::memory
{

/// @brief Memory allocation backend that uses ANSI C standard library functions for memory management.
/// @details This allocator uses the standard C library functions, but using their aligned versions when possible.
/// @see Malloc, UseSystemMallocForNew
class GP_CORE_API MallocAnsi final : public Malloc
{
public:
    /// @brief Allocate memory of the specified size and alignment.
    /// @param[in] size The size of the memory block to allocate, in bytes.
    /// @param[in] alignment The alignment requirement for the allocated memory block, in bytes.
    /// @return A pointer to the allocated memory block, or nullptr if the allocation fails.
    [[nodiscard]] void* allocate(USize size, UInt32 alignment) override;

    /// @brief Try to allocate memory of the specified size and alignment without throwing exceptions.
    /// @param[in] size The size of the memory block to allocate, in bytes.
    /// @param[in] alignment The alignment requirement for the allocated memory block, in bytes.
    /// @return A pointer to the allocated memory block, or nullptr if the allocation fails.
    [[nodiscard]] void* tryAllocate(USize size, UInt32 alignment) noexcept override;

    /// @brief Reallocate memory to a new size and alignment.
    /// @details The ptr block must have been allocated by this allocator.
    /// @param[in] ptr A pointer to the previously allocated memory block.
    /// @param[in] newSize The new size of the memory block to allocate, in bytes.
    /// @param[in] alignment The alignment requirement for the allocated memory block, in bytes.
    /// @return A pointer to the reallocated memory block, or nullptr if the reallocation fails.
    [[nodiscard]] void* reallocate(void* ptr, USize newSize, UInt32 alignment) override;

    /// @brief Try to reallocate memory to a new size and alignment without throwing exceptions.
    /// @details The ptr block must have been allocated by this allocator.
    /// @param[in] ptr A pointer to the previously allocated memory block.
    /// @param[in] newSize The new size of the memory block to allocate, in bytes.
    /// @param[in] alignment The alignment requirement for the allocated memory block, in bytes.
    /// @return A pointer to the reallocated memory block, or nullptr if the reallocation fails.
    [[nodiscard]] void* tryReallocate(void* ptr, USize newSize, UInt32 alignment) noexcept override;

    /// @brief Deallocate memory previously allocated by this allocator.
    /// @details The ptr block must have been allocated by this allocator.
    /// @param[in] ptr A pointer to the memory block to deallocate.
    void deallocate(void* ptr) override;

    /// @brief Get the size of an allocated memory block.
    /// @param[in] ptr A pointer to the allocated memory block.
    /// @return The size of the memory block, in bytes.
    USize getAllocationSize(void* ptr) override;

    /// @brief Check if the allocator can get the size of an allocated memory block.
    /// @return true if the allocator can get the size, false otherwise
    bool canGetAllocationSize() override;
};

}   // namespace gp::memory
