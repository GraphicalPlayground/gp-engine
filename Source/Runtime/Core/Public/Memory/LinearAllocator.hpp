// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include "Memory/MemoryUtilities.hpp"

namespace GP::Memory
{

/// \brief A linear (bump/arena) allocator that allocates sequentially from a contiguous buffer.
///
/// Allocations are O(1) — a pointer bumps forward. Individual deallocations are not supported;
/// the entire allocator is reset at once via Reset(). This makes it ideal for:
///   - Per-frame temporary allocations (reset each frame)
///   - Command buffer building
///   - Scratch memory for algorithms
///
/// **How AAA engines use this**:
///   - Frostbite: "Frame allocator" — reset every frame, zero fragmentation
///   - Unreal: FMemStackBase — linear stack for per-frame temporaries
///   - idTech: "hunk allocator" — linear allocation for level data
///
/// \note This is the simplest useful allocator. It demonstrates the key insight: if you know the lifetime pattern of
/// your allocations (e.g., "all freed at end of frame"), you can beat general-purpose malloc by 100x. No free-list, no
/// coalescing, no headers — just a pointer bump.
///
/// \tparam BufferSize  Fixed buffer capacity in bytes. Set to 0 for dynamic/external buffer.
/// \tparam Alignment   Default alignment for allocations.
template <SizeT BufferSize = 0, SizeT Alignment = alignof(std::max_align_t)>
class TLinearAllocator
{
public:
    using SizeType = SizeT;   //<! Size type for allocations.

public:
    static constexpr SizeT DefaultAlignment = Alignment;          //<! Default alignment for allocations.
    static constexpr bool HasInternalBuffer = (BufferSize > 0);   //<! Whether the allocator has an internal buffer.

private:
    /// \brief Buffer storage type. An aligned byte array if we have an internal buffer, or an empty struct if we don't.
    struct BufferStorage
    {
        alignas(Alignment) Byte data[HasInternalBuffer ? BufferSize : 1];   //<! Aligned buffer storage
    };

private:
    GP_MAYBE_UNUSED BufferStorage m_internalBuffer;   //<! Internal buffer storage (only if BufferSize > 0).
    Byte* m_bufferStart;                              //<! Start of the buffer.
    Byte* m_bufferEnd;                                //<! End of the buffer (one past last byte).
    Byte* m_current;                                  //<! Current allocation pointer (bump).
    SizeType m_peakUsage;                             //<! Peak usage for statistics.

public:
    /// \brief Construct a linear allocator with an internal fixed-size buffer. Only available when BufferSize > 0.
    TLinearAllocator() requires(HasInternalBuffer)
        : m_bufferStart(m_internalBuffer.data)
        , m_bufferEnd(m_internalBuffer.data + BufferSize)
        , m_current(m_internalBuffer.data)
        , m_peakUsage(0)
    {}

    /// \brief Construct a linear allocator from an externally-owned buffer. The caller is responsible for the lifetime
    /// of the buffer.
    /// \param buffer Pointer to the start of the buffer.
    /// \param sizeBytes Size of the buffer in bytes.
    TLinearAllocator(void* buffer, SizeType sizeBytes) requires(!HasInternalBuffer)
        : m_bufferStart(static_cast<Byte*>(buffer))
        , m_bufferEnd(static_cast<Byte*>(buffer) + sizeBytes)
        , m_current(static_cast<Byte*>(buffer))
        , m_peakUsage(0)
    {
        GP_ASSERT(buffer != nullptr, "External buffer cannot be null");
        GP_ASSERT(sizeBytes > 0, "Buffer size must be > 0");
    }

    /// \brief Non-copyable, as copying would lead to double-free issues and invalid pointers.
    TLinearAllocator(const TLinearAllocator&) = delete;

    /// \brief Non-movabable, as moving would also lead to double-free issues and invalid pointers. If move semantics
    /// are desired, consider
    TLinearAllocator(TLinearAllocator&&) = delete;

    /// \brief Destructor. Does not free any memory (caller is responsible for external buffer if used).
    ~TLinearAllocator() = default;

public:
    /// \brief Copy assignment operator is deleted to prevent copying the allocator, which could lead to double-free
    /// issues and
    TLinearAllocator& operator=(const TLinearAllocator&) = delete;

    /// \brief Move assignment operator is deleted to prevent moving the allocator, which could lead to double-free
    /// issues and invalid pointers.
    TLinearAllocator& operator=(TLinearAllocator&&) = delete;

public:
    /// \brief Allocate size bytes with the given alignment. Returns nullptr if the allocator is full. O(1) time.
    /// \param size Number of bytes to allocate.
    /// \param align Alignment for the allocation (must be a power of two). Defaults to DefaultAlignment.
    /// \return Pointer to the allocated memory, or nullptr if out of memory.
    GP_NODISCARD void* Allocate(SizeType size, SizeType align = DefaultAlignment)
    {
        GP_ASSERT(size > 0, "Cannot allocate zero bytes");
        GP_ASSERT(IsPowerOfTwo(align), "Alignment must be a power of two");

        Byte* aligned = reinterpret_cast<Byte*>(AlignUp(reinterpret_cast<UIntPtr>(m_current), align));
        Byte* newCurrent = aligned + size;

        if (newCurrent > m_bufferEnd)
        {
            GP_ASSERT(
                false, "LinearAllocator: out of memory (requested %zu, remaining %zu)", size, GetRemainingBytes()
            );
            return nullptr;
        }

        m_current = newCurrent;

        SizeType used = GetUsedBytes();
        if (used > m_peakUsage) { m_peakUsage = used; }

        return aligned;
    }

    /// \brief Individual deallocation is a no-op for linear allocators. All memory is freed at once via Reset().
    /// \param ptr Pointer to the memory to "deallocate". Ignored.
    void Deallocate(void* /*ptr*/) noexcept
    {
        // Intentionally empty. Linear allocators do not support individual free.
        // This satisfies the IsAllocator concept without doing anything.
    }

    /// \brief Reset the allocator, freeing all allocations at once. O(1). Does NOT call destructors, the caller is
    /// responsible for that.
    void Reset() noexcept { m_current = m_bufferStart; }

    /// \brief Number of bytes currently allocated.
    /// \return Number of bytes currently allocated.
    GP_NODISCARD SizeType GetUsedBytes() const noexcept { return static_cast<SizeType>(m_current - m_bufferStart); }

    /// \brief Number of bytes remaining in the buffer.
    /// \return Number of bytes remaining in the buffer.
    GP_NODISCARD SizeType GetRemainingBytes() const noexcept { return static_cast<SizeType>(m_bufferEnd - m_current); }

    /// \brief Total capacity of the buffer in bytes.
    /// \return Total capacity of the buffer in bytes.
    GP_NODISCARD SizeType GetCapacityBytes() const noexcept
    {
        return static_cast<SizeType>(m_bufferEnd - m_bufferStart);
    }

    /// \brief Peak usage since last Reset() (high watermark).
    /// \return Peak usage in bytes.
    GP_NODISCARD SizeType GetPeakUsage() const noexcept { return m_peakUsage; }

    /// \brief Check if the allocator owns a given pointer.
    /// \param ptr Pointer to check.
    /// \return True if the pointer is within the allocator's buffer, false otherwise.
    GP_NODISCARD bool Owns(const void* ptr) const noexcept
    {
        auto p = static_cast<const Byte*>(ptr);
        return p >= m_bufferStart && p < m_bufferEnd;
    }
};

}   // namespace GP::Memory
