// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "core/ResourceForward.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "diagnostics/AssertionMacros.hpp"
#include "memory/GlobalMemory.hpp"

namespace gp::rhi::resource
{

/// @brief Move-only RAII wrapper owning an aligned heap allocation used during staged resource initialization.
/// @details Intended for backend initializer lambdas that allocate staging memory upfront and release it automatically
///          when the finalization closure finishes or is destroyed.
struct InitializerScopedMemory final
{
public:
    /// @brief Raw pointer to the owned heap allocation, or `nullptr` when empty or moved from.
    void* pointer{ nullptr };

public:
    /// @brief Adopts ownership of a raw allocation obtained from `gp::memory::getGlobalMalloc()`.
    /// @param[in] inPointer Heap pointer to own, or `nullptr`.
    explicit InitializerScopedMemory(void* const inPointer) noexcept
        : pointer(inPointer)
    {}

    /// @brief Allocates `size` bytes aligned to `alignment` from the global engine allocator.
    /// @param[in] size Requested allocation size in bytes (`0` produces a null allocation).
    /// @param[in] alignment Byte alignment passed to the global allocator.
    InitializerScopedMemory(const gp::USize size, const gp::UInt32 alignment) noexcept
        : pointer(allocateMemory(size, alignment))
    {}

    InitializerScopedMemory(const InitializerScopedMemory&) = delete;
    InitializerScopedMemory& operator=(const InitializerScopedMemory&) = delete;

    /// @brief Move constructor transferring ownership of the raw pointer from `other`.
    /// @param[in,out] other Source wrapper left holding `nullptr`.
    InitializerScopedMemory(InitializerScopedMemory&& other) noexcept
        : pointer(other.pointer)
    {
        other.pointer = nullptr;
    }

    /// @brief Move assignment operator freeing any currently owned pointer before adopting `other`.
    /// @param[in,out] other Source wrapper left holding `nullptr`.
    /// @return Reference to `*this`.
    InitializerScopedMemory& operator=(InitializerScopedMemory&& other) noexcept
    {
        if (this != &other)
        {
            release();
            pointer = other.pointer;
            other.pointer = nullptr;
        }
        return *this;
    }

    /// @brief Releases the owned heap allocation if non-null.
    ~InitializerScopedMemory() noexcept
    {
        release();
    }

    /// @brief Frees the owned allocation immediately and resets `pointer` to `nullptr`.
    void release() noexcept
    {
        if (pointer != nullptr)
        {
            if (gp::memory::Malloc* const allocator = gp::memory::getGlobalMalloc(); allocator != nullptr)
            {
                allocator->deallocate(pointer);
            }
            pointer = nullptr;
        }
    }

private:
    /// @brief Allocates aligned memory from the global engine allocator when `size > 0`.
    /// @param[in] size Allocation size in bytes.
    /// @param[in] alignment Required byte alignment.
    /// @return Allocated pointer, or `nullptr` when `size == 0`.
    [[nodiscard]] static void* allocateMemory(const gp::USize size, const gp::UInt32 alignment) noexcept
    {
        if (size == 0u)
        {
            return nullptr;
        }

        gp::memory::Malloc* const allocator = gp::memory::getGlobalMalloc();
        GP_CHECKF(allocator != nullptr, "Global engine allocator must be valid for InitializerScopedMemory.");
        return allocator->allocate(size, alignment);
    }
};

}   // namespace gp::rhi::resource
