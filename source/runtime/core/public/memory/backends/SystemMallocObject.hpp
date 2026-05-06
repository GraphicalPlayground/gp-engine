// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"

namespace gp::memory
{

/// @brief A class that provides overloaded new and delete operators to use the system allocator for memory management.
class SystemMallocObject
{
public:
    /// @brief Overloaded new operator to use the system allocator for memory allocation.
    /// @param[in] size The size of the memory block to allocate.
    /// @return A pointer to the allocated memory block.
    void* operator new(gp::USize size);

    /// @brief Overloaded delete operator to use the system allocator for memory deallocation.
    /// @param[in] ptr A pointer to the memory block to deallocate.
    void operator delete(void* ptr);

    /// @brief Overloaded new[] operator to use the system allocator for memory allocation of arrays.
    /// @param[in] size The size of the memory block to allocate for the array.
    /// @return A pointer to the allocated memory block for the array.
    void* operator new[](gp::USize size);

    /// @brief Overloaded delete[] operator to use the system allocator for memory deallocation of arrays.
    /// @param[in] ptr A pointer to the memory block to deallocate for the array.
    void operator delete[](void* ptr);
};

}   // namespace gp::memory
