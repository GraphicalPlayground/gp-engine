// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include "Memory/Allocator.hpp"
#include "Memory/MemoryUtilities.hpp"

namespace GP::Memory
{

/// \brief An allocator with inlined stack storage that falls back to a heap allocator.
///
/// Stores up to InlineCount elements of T in a stack-local buffer. If more space is needed, seamlessly falls back to
/// the FallbackAllocator (typically the default heap allocator).
///
/// **How AAA engines use this**:
///   - Unreal: TInlineAllocator<N> for TArray — avoids heap allocation for small arrays
///   - EASTL: fixed_vector, fixed_string — inline storage with overflow to heap
///   - Frostbite: SmallVector pattern for temporary buffers
///
/// \note Most arrays in a game engine hold fewer than 8 elements. By keeping a small buffer inside the container itself
/// (on the stack or inside the owning object), we avoid heap allocation entirely for the common case. This eliminates
/// malloc overhead and cache misses — the data is right next to the container's metadata.
///
/// \tparam T Type of elements stored.
/// \tparam InlineCount Number of T elements in the inline buffer.
/// \tparam FallbackAllocator Allocator to use when inline buffer is exceeded.
template <typename T, SizeT InlineCount, typename FallbackAllocator = TAllocator<T>>
class TInlineAllocator
{
    static_assert(InlineCount > 0, "InlineCount must be > 0");

public:
    using ValueType = T;      //<! Alias for the type this allocator is associated with.
    using SizeType = SizeT;   //<! Alias for size type, typically std::size_t.

public:
    /// \brief Inline capacity of the allocator (number of T elements that can be stored in the inline buffer).
    static constexpr SizeT InlineCapacity = InlineCount;

    /// \brief Size of the inline buffer in bytes.
    static constexpr SizeT InlineBufferSize = InlineCount * sizeof(T);

private:
    alignas(T) Byte m_inlineBuffer[InlineBufferSize];   //<! The buffer for storing up to InlineCount elements of T.
    GP_MAYBE_UNUSED FallbackAllocator m_fallback;   //<! The fallback for allocations when inline buffer is exceeded.

public:
    /// \brief Default constructor. Initializes the inline buffer and the fallback allocator.
    TInlineAllocator()
        : m_inlineBuffer{}
        , m_fallback()
    {}

    /// \brief Destructor. Defaulted since the inline buffer is trivially destructible and the fallback allocator is
    /// also default-constructible and doesn't require special cleanup.
    ~TInlineAllocator() = default;

    /// \brief Copy constructor. Defaulted since the inline buffer is trivially copyable and the fallback allocator is
    /// also
    TInlineAllocator(const TInlineAllocator&) = default;

public:
    /// \brief Copy assignment operator. Defaulted since the inline buffer is trivially copyable and the fallback
    /// allocator is also copyable.
    TInlineAllocator& operator=(const TInlineAllocator&) = default;

public:
    /// \brief Allocate memory. If it fits in the inline buffer, return the buffer. Otherwise, fall back to the heap
    /// allocator.
    /// \param size Number of bytes to allocate. Must be > 0.
    /// \param align Alignment requirement in bytes. Must be a power of two. Defaults to alignof(T).
    /// \return Pointer to the allocated memory block, or nullptr on failure.
    GP_NODISCARD void* Allocate(SizeType size, SizeType align = alignof(T))
    {
        if (size <= InlineBufferSize && align <= alignof(T)) { return static_cast<void*>(m_inlineBuffer); }

        return m_fallback.Allocate(size, align);
    }

    /// \brief Deallocate memory. No-op if the pointer is in the inline buffer.
    /// \param ptr Pointer to the memory block to deallocate. Must have been returned by this allocator.
    void Deallocate(void* ptr)
    {
        if (IsInlinePointer(ptr))
        {
            return;   // Inline buffer doesn't need freeing.
        }

        m_fallback.Deallocate(ptr);
    }

    /// \brief Check if a pointer points to the inline buffer.
    /// \param ptr Pointer to check.
    /// \return True if the pointer is within the inline buffer, false otherwise.
    GP_NODISCARD bool IsInlinePointer(const void* ptr) const noexcept
    {
        auto p = static_cast<const Byte*>(ptr);
        auto start = reinterpret_cast<const Byte*>(m_inlineBuffer);
        return p >= start && p < start + InlineBufferSize;
    }

    /// \brief Get the inline buffer as a typed pointer.
    /// \return Pointer to the inline buffer cast to T*.
    GP_NODISCARD T* GetInlineBuffer() noexcept { return reinterpret_cast<T*>(m_inlineBuffer); }

    /// \brief Get the inline buffer as a const typed pointer.
    /// \return Pointer to the inline buffer cast to const T*.
    GP_NODISCARD const T* GetInlineBuffer() const noexcept { return reinterpret_cast<const T*>(m_inlineBuffer); }

    /// \brief Get the inline capacity (in elements, not bytes).
    /// \return The number of T elements that can be stored in the inline buffer.
    GP_NODISCARD constexpr SizeType GetInlineCapacity() const noexcept { return InlineCount; }
};

}   // namespace GP::Memory
