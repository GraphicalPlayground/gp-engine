// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "memory/Allocator.hpp"
#include "memory/Utilities.hpp"

#if GP_BUILD_DEBUG
    #include <atomic>
#endif

namespace gp::memory
{

/// @brief Global allocator backed by the platform's aligned malloc/free.
/// @details
/// Thread-safe by virtue of OS-level guarantees on malloc/free.
/// Allocation statistics are tracked only in debug builds to avoid introducing atomic contention
/// on the hot path in release.
///
/// This is a singleton-style allocator. Use get() to obtain the global instance.
class DefaultAllocator final : public Allocator
{
private:
#if GP_BUILD_DEBUG
    std::atomic<USize> m_debugAllocatedSize{ 0 };
    std::atomic<USize> m_debugAllocationCount{ 0 };
#endif

public:
    DefaultAllocator() = default;
    DefaultAllocator(const DefaultAllocator&) = delete;
    DefaultAllocator& operator=(const DefaultAllocator&) = delete;
    ~DefaultAllocator() override = default;

public:
    /// @brief Returns the global default allocator instance.
    /// @return Reference to the singleton default_allocator.
    static DefaultAllocator& get() noexcept
    {
        static DefaultAllocator s_instance;
        return s_instance;
    }

public:
    /// @brief Allocates memory using the platform's aligned allocation.
    /// @param[in] size The number of bytes to allocate.
    /// @param[in] alignment The required alignment (must be a power of two).
    /// @return Pointer to the allocated memory, or nullptr on failure.
    GP_NODISCARD void* allocate(USize size, USize alignment = alignof(gp::MaxAlign)) noexcept override
    {
        void* ptr = gp::memory::malloc(size, alignment);
#if GP_BUILD_DEBUG
        if (ptr)
        {
            m_debugAllocatedSize.fetch_add(size, std::memory_order_relaxed);
            m_debugAllocationCount.fetch_add(1, std::memory_order_relaxed);
        }
#endif
        return ptr;
    }

    /// @brief Deallocates memory previously allocated by this allocator.
    /// @param[in] ptr Pointer to the memory block to free.
    /// @param[in] size The size in bytes of the block being freed.
    void deallocate(void* ptr, USize size) noexcept override
    {
#if GP_BUILD_DEBUG
        if (ptr)
        {
            m_debugAllocatedSize.fetch_sub(size, std::memory_order_relaxed);
            m_debugAllocationCount.fetch_sub(1, std::memory_order_relaxed);
        }
#else
        GP_UNUSED(size);
#endif
        gp::memory::free(ptr);
    }

#if GP_BUILD_DEBUG
    GP_NODISCARD USize getAllocatedSize() const noexcept override
    {
        return m_debugAllocatedSize.load(std::memory_order_relaxed);
    }

    GP_NODISCARD USize getAllocationCount() const noexcept override
    {
        return m_debugAllocationCount.load(std::memory_order_relaxed);
    }
#endif

    GP_NODISCARD const char* getDebugName() const noexcept override { return "DefaultAllocator"; }
};

}   // namespace gp::memory
