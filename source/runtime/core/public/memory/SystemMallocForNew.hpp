// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"

namespace gp::memory
{

/// @brief A class that overrides the global new and delete operators to use the system allocator for memory allocation
/// and deallocation.
/// @details Inheriting from this class allows derived classes to use the system allocator for memory management, which
/// can be useful in certain scenarios where the default memory allocator is not suitable or desired.
/// @see gp::memory::Malloc
class UseSystemMallocForNew
{
public:
    /// @brief Overrides the global new operator to use the system allocator.
    /// @param[in] size The size of the memory block to allocate.
    /// @return A pointer to the allocated memory block.
    GP_CORE_API void* operator new(std::size_t size) GP_OPERATOR_NEW_THROW_SPEC;

    /// @brief Overrides the global new[] operator to use the system allocator
    /// @param[in] size The size of the memory block to allocate.
    /// @return A pointer to the allocated memory block.
    GP_CORE_API void* operator new[](std::size_t size) GP_OPERATOR_NEW_THROW_SPEC;

    /// @brief Overrides the global delete operator to use the system allocator for deallocation.
    /// @param[in] ptr A pointer to the memory block to deallocate.
    GP_CORE_API void operator delete(void* ptr) GP_OPERATOR_DELETE_THROW_SPEC;

    /// @brief Overrides the global delete[] operator to use the system allocator for deallocation.
    /// @param[in] ptr A pointer to the memory block to deallocate.
    GP_CORE_API void operator delete[](void* ptr) GP_OPERATOR_DELETE_THROW_SPEC;
};

}   // namespace gp::memory
