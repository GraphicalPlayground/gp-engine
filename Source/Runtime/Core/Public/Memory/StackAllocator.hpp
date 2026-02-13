// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include "Memory/MemoryUtilities.hpp"

namespace GP::Memory
{

/// \brief A stack allocator that supports LIFO deallocation via markers.
///
/// Like a linear allocator but supports partial rollback using saved markers.
/// Useful for nested scopes where inner allocations are freed before outer ones.
///
/// **How AAA engines use this**:
///   - Unreal: FMemStack — push/pop for scope-based temporaries
///   - idTech: Hunk system with high/low watermarks
///   - Frostbite: Scope allocators for render job submission
///
/// **Educational note**: This allocator teaches the concept of allocation scoping.
/// You save a marker, allocate freely within a scope, then rewind to the marker
/// which frees everything allocated after it — in O(1). This is the backbone
/// of per-scope temporary allocations in real-time engines.
///
/// \tparam BufferSize Fixed buffer capacity in bytes. 0 = use external buffer.
/// \tparam Alignment Default alignment for allocations.
template <SizeT BufferSize = 0, SizeT Alignment = alignof(std::max_align_t)>
class TStackAllocator
{
public:
    using SizeType = SizeT;   //<! Type for sizes and offsets.

public:
    /// \brief An opaque marker representing a position in the stack. Save a marker before allocations, then rewind to
    /// it to free them all.
    struct FMarker
    {
        SizeType offset = 0;   //<! Offset from the base of the stack to the marker position.
    };

    static constexpr SizeT DefaultAlignment = Alignment;
    static constexpr bool HasInternalBuffer = (BufferSize > 0);

private:
    /// \brief Internal buffer storage type. If BufferSize > 0, we use an aligned byte array. Otherwise, we use an empty
    /// struct.
    struct Empty
    {};

    // If we have an internal buffer, this will be an aligned byte array of the specified size. Otherwise, it's just an
    // empty struct.
    using BufferStorage = std::conditional_t<HasInternalBuffer, alignas(Alignment) Byte[BufferSize], Empty>;

private:
    Byte* m_bufferStart;   //<! Start of the buffer (internal or external).
    Byte* m_bufferEnd;     //<! End of the buffer (one past the last byte).
    Byte* m_current;       //<! Current allocation position (moves forward with each allocation).
    GP_MAYBE_UNUSED BufferStorage m_internalBuffer;   //<! Internal buffer storage (only used if BufferSize > 0).

public:
    /// \brief Construct with internal fixed-size buffer.
    TStackAllocator() requires(HasInternalBuffer)
        : m_bufferStart(m_internalBuffer)
        , m_bufferEnd(m_internalBuffer + BufferSize)
        , m_current(m_internalBuffer)
    {}

    /// \brief Construct from an externally-owned buffer.
    /// \param buffer Pointer to the external buffer memory.
    /// \param sizeBytes Size of the external buffer in bytes.
    TStackAllocator(void* buffer, SizeType sizeBytes) requires(!HasInternalBuffer)
        : m_bufferStart(static_cast<Byte*>(buffer))
        , m_bufferEnd(static_cast<Byte*>(buffer) + sizeBytes)
        , m_current(static_cast<Byte*>(buffer))
    {
        GP_ASSERT(buffer != nullptr, "External buffer cannot be null");
        GP_ASSERT(sizeBytes > 0, "Buffer size must be > 0");
    }

    /// \brief Non-copyable and non-movable to avoid accidental misuse. If you need to transfer ownership, consider
    /// using a pointer or reference wrapper.
    TStackAllocator(const TStackAllocator&) = delete;

    /// \brief Move constructor deleted to prevent accidental moves. This allocator manages a specific memory region,
    /// and moving it could lead to double frees or dangling pointers if not handled carefully.
    TStackAllocator(TStackAllocator&&) = delete;

    /// \brief Destructor is defaulted. Since this allocator does not manage dynamic memory (it either uses a fixed
    /// internal buffer or an external buffer that it does not own).
    ~TStackAllocator() = default;

public:
    /// \brief Copy assignment operator deleted to prevent copying.
    TStackAllocator& operator=(const TStackAllocator&) = delete;

    /// \brief Move assignment operator deleted to prevent accidental moves.
    TStackAllocator& operator=(TStackAllocator&&) = delete;

public:
    /// \brief Allocate size bytes with the given alignment.
    /// \param size Number of bytes to allocate.
    /// \param align Alignment requirement (must be a power of two). Defaults to the allocator's default alignment.
    /// \return Pointer to the allocated memory, or nullptr if out of memory.
    GP_NODISCARD void* Allocate(SizeType size, SizeType align = DefaultAlignment)
    {
        GP_ASSERT(size > 0, "Cannot allocate zero bytes");
        GP_ASSERT(IsPowerOfTwo(align), "Alignment must be a power of two");

        Byte* aligned = reinterpret_cast<Byte*>(AlignUp(reinterpret_cast<UIntPtr>(m_current), align));
        Byte* newCurrent = aligned + size;

        if (newCurrent > m_bufferEnd)
        {
            GP_ASSERT(false, "StackAllocator: out of memory");
            return nullptr;
        }

        m_current = newCurrent;
        return aligned;
    }

    /// \brief Deallocate is a no-op, use FreeToMarker() for bulk deallocation.
    void Deallocate(void* /*ptr*/) noexcept
    {
        // Individual free not supported. Use FreeToMarker().
    }

    /// \brief Save the current stack position as a marker. Everything allocated after this marker can be freed with
    /// FreeToMarker().
    /// \return A marker representing the current position in the stack.
    GP_NODISCARD FMarker SaveMarker() const noexcept { return FMarker{ GetUsedBytes() }; }

    /// \brief Rewind the stack to a previously saved marker, freeing all allocations made after it. O(1). Does NOT call
    /// destructors.
    /// \param marker The marker to rewind to. Must be a valid marker previously returned by SaveMarker() and not
    /// already rewound past.
    void FreeToMarker(FMarker marker) noexcept
    {
        GP_ASSERT(
            marker.Offset <= GetUsedBytes(),
            "Invalid marker: offset %zu exceeds current usage %zu",
            marker.Offset,
            GetUsedBytes()
        );
        m_current = m_bufferStart + marker.Offset;
    }

    /// \brief Reset to empty (equivalent to FreeToMarker at position 0).
    void Reset() noexcept { m_current = m_bufferStart; }

    /// \brief Get the number of bytes currently allocated (used) in the stack.
    /// \return Number of bytes allocated since the last reset or marker.
    GP_NODISCARD SizeType GetUsedBytes() const noexcept { return static_cast<SizeType>(m_current - m_bufferStart); }

    /// \brief Get the number of bytes remaining (free) in the stack.
    /// \return Number of bytes available for allocation before hitting capacity.
    GP_NODISCARD SizeType GetRemainingBytes() const noexcept { return static_cast<SizeType>(m_bufferEnd - m_current); }

    /// \brief Get the total capacity of the stack in bytes.
    /// \return Total size of the buffer managed by this allocator.
    GP_NODISCARD SizeType GetCapacityBytes() const noexcept
    {
        return static_cast<SizeType>(m_bufferEnd - m_bufferStart);
    }

    /// \brief Check if a pointer is owned by this allocator (i.e., within the buffer range).
    /// \param ptr Pointer to check.
    /// \return True if the pointer is within the buffer managed by this allocator, false otherwise.
    GP_NODISCARD bool Owns(const void* ptr) const noexcept
    {
        auto p = static_cast<const Byte*>(ptr);
        return p >= m_bufferStart && p < m_bufferEnd;
    }
};

}   // namespace GP::Memory
