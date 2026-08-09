// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "memory/MemoryBase.hpp"
#include "memory/SystemMallocForNew.hpp"

namespace gp::memory
{

/// @brief Base class for memory allocation backends.
/// @details This class serves as the base interface for the memory allocation backends used in the engine.
/// @see UseSystemMallocForNew, MallocAnsi, MallocBinned
class GP_CORE_API Malloc : public UseSystemMallocForNew
{
public:
    /// @brief Default constructor for the Malloc class.
    virtual ~Malloc() = default;

public:
    /// @brief Allocate memory of the specified size and alignment.
    /// @param[in] size The size of the memory block to allocate, in bytes.
    /// @param[in] alignment The alignment requirement for the allocated memory block, in bytes.
    /// @return A pointer to the allocated memory block, or nullptr if the allocation fails.
    [[nodiscard]] virtual void* allocate(USize size, UInt32 alignment = kDefaultAlignment) = 0;

    /// @brief Try to allocate memory of the specified size and alignment without throwing exceptions.
    /// @param[in] size The size of the memory block to allocate, in bytes.
    /// @param[in] alignment The alignment requirement for the allocated memory block, in bytes.
    /// @return A pointer to the allocated memory block, or nullptr if the allocation fails.
    [[nodiscard]] virtual void* tryAllocate(USize size, UInt32 alignment = kDefaultAlignment) noexcept;

    /// @brief Allocate zero-initialized memory of the specified size and alignment.
    /// @param[in] size The size of the memory block to allocate, in bytes.
    /// @param[in] alignment The alignment requirement for the allocated memory block, in bytes.
    /// @return A pointer to the allocated memory block, or nullptr if the allocation fails.
    [[nodiscard]] virtual void* allocateZeroed(USize size, UInt32 alignment = kDefaultAlignment);

    /// @brief Try to allocate zero-initialized memory of the specified size and alignment without throwing exceptions.
    /// @param[in] size The size of the memory block to allocate, in bytes.
    /// @param[in] alignment The alignment requirement for the allocated memory block, in bytes.
    /// @return A pointer to the allocated memory block, or nullptr if the allocation fails.
    [[nodiscard]] virtual void* tryAllocateZeroed(USize size, UInt32 alignment = kDefaultAlignment) noexcept;

    /// @brief Reallocate memory to a new size and alignment.
    /// @details The ptr block must have been allocated by this allocator.
    /// @param[in] ptr A pointer to the previously allocated memory block.
    /// @param[in] newSize The new size of the memory block to allocate, in bytes.
    /// @param[in] alignment The alignment requirement for the allocated memory block, in bytes.
    /// @return A pointer to the reallocated memory block, or nullptr if the reallocation fails.
    [[nodiscard]] virtual void* reallocate(void* ptr, USize newSize, UInt32 alignment = kDefaultAlignment) = 0;

    /// @brief Try to reallocate memory to a new size and alignment without throwing exceptions.
    /// @details The ptr block must have been allocated by this allocator.
    /// @param[in] ptr A pointer to the previously allocated memory block.
    /// @param[in] newSize The new size of the memory block to allocate, in bytes.
    /// @param[in] alignment The alignment requirement for the allocated memory block, in bytes.
    /// @return A pointer to the reallocated memory block, or nullptr if the reallocation fails.
    [[nodiscard]] virtual void* tryReallocate(void* ptr, USize newSize, UInt32 alignment = kDefaultAlignment) noexcept;

    /// @brief Deallocate memory previously allocated by this allocator.
    /// @details The ptr block must have been allocated by this allocator.
    /// @param[in] ptr A pointer to the memory block to deallocate.
    virtual void deallocate(void* ptr) = 0;

    /// @brief Get the size of an allocated memory block.
    /// @param[in] ptr A pointer to the allocated memory block.
    /// @return The size of the memory block, in bytes.
    virtual USize getAllocationSize(void* ptr);

    /// @brief Check if the allocator can get the size of an allocated memory block.
    /// @return true if the allocator can get the size, false otherwise.
    virtual bool canGetAllocationSize();
};

}   // namespace gp::memory
