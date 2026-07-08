// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "memory/GlobalMemory.hpp"   // IWYU pragma: keep

/// @brief Allocates a block of memory of the specified size.
/// @param[in] size The size of the memory block to allocate.
/// @return The pointer to the allocated memory block.
GP_NODISCARD void* operator new(std::size_t size);

/// @brief Allocates a block of memory of the specified size.
/// @param[in] size The size of the memory block to allocate.
/// @return The pointer to the allocated memory block.
GP_NODISCARD void* operator new[](std::size_t size);

/// @brief Allocates a block of memory of the specified size with the specified alignment requirement.
/// @param[in] size The size of the memory block to allocate.
/// @param[in] alignment The alignment requirement for the memory block to allocate.
/// @return The pointer to the allocated memory block.
GP_NODISCARD void* operator new(std::size_t size, std::align_val_t alignment);

/// @brief Allocates a block of memory of the specified size with the specified alignment requirement.
/// @param[in] size The size of the memory block to allocate.
/// @param[in] alignment The alignment requirement for the memory block to allocate.
/// @return The pointer to the allocated memory block.
GP_NODISCARD void* operator new[](std::size_t size, std::align_val_t alignment);

/// @brief Deallocates a block of memory that was previously allocated.
/// @param[in] ptr The pointer to the memory block to deallocate.
void operator delete(void* ptr) noexcept;

/// @brief Deallocates a block of memory that was previously allocated.
/// @param[in] ptr The pointer to the memory block to deallocate.
void operator delete[](void* ptr) noexcept;

/// @brief Deallocates a block of memory that was previously allocated.
/// @param[in] ptr The pointer to the memory block to deallocate.
/// @param[in] size The size of the memory block to deallocate.
void operator delete(void* ptr, GP_MAYBE_UNUSED std::size_t size) noexcept;

/// @brief Deallocates a block of memory that was previously allocated.
/// @param[in] ptr The pointer to the memory block to deallocate.
/// @param[in] size The size of the memory block to deallocate.
void operator delete[](void* ptr, GP_MAYBE_UNUSED std::size_t size) noexcept;

/// @brief Deallocates a block of memory that was previously allocated with a specific alignment requirement.
/// @param[in] ptr The pointer to the memory block to deallocate.
/// @param[in] alignment The alignment requirement for the memory block to deallocate.
void operator delete(void* ptr, GP_MAYBE_UNUSED std::align_val_t alignment) noexcept;

/// @brief Deallocates a block of memory that was previously allocated with a specific alignment requirement.
/// @param[in] ptr The pointer to the memory block to deallocate.
/// @param[in] alignment The alignment requirement for the memory block to deallocate.
void operator delete[](void* ptr, GP_MAYBE_UNUSED std::align_val_t alignment) noexcept;
