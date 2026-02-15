// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include "Templates/Concepts.hpp"
#include <cstring>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

namespace GP::Memory
{

/// \brief Align a value up to the next multiple of the given alignment. Alignment must be a power of two.
/// \param value The value to align up.
/// \param alignment The alignment to align to (must be a power of two).
/// \return The aligned value. If the input value is already aligned, it is returned unchanged.
/// \note GPUs and CPUs access memory most efficiently at aligned addresses. Misaligned access can cause performance
/// penalties or crashes on some architectures (ARM). This function rounds up to the nearest aligned address:
/// AlignUp(7, 4) = 8, AlignUp(8, 4) = 8, AlignUp(9, 16) = 16.
GP_NODISCARD GP_FORCEINLINE constexpr SizeT AlignUp(SizeT value, SizeT alignment) noexcept
{
    GP_ASSERT((alignment & (alignment - 1)) == 0, "Alignment must be a power of two");
    return (value + alignment - 1) & ~(alignment - 1);
}

/// \brief Align a value down to the previous multiple of the given alignment.
/// \param value The value to align down.
/// \param alignment The alignment to align to (must be a power of two).
/// \return The aligned value. If the input value is already aligned, it is returned unchanged.
GP_NODISCARD GP_FORCEINLINE constexpr SizeT AlignDown(SizeT value, SizeT alignment) noexcept
{
    GP_ASSERT((alignment & (alignment - 1)) == 0, "Alignment must be a power of two");
    return value & ~(alignment - 1);
}

/// \brief Check if a value is a power of two.
/// \param value The value to check.
/// \return True if the value is a power of two, false otherwise. Zero is not considered a power of two.
GP_NODISCARD GP_FORCEINLINE constexpr bool IsPowerOfTwo(SizeT value) noexcept
{
    return value > 0 && (value & (value - 1)) == 0;
}

/// \brief Check if a pointer is aligned to the given alignment.
/// \param ptr The pointer to check.
/// \param alignment The alignment to check against (must be a power of two).
/// \return True if the pointer is aligned, false otherwise.
GP_NODISCARD GP_FORCEINLINE bool IsAligned(const void* ptr, SizeT alignment) noexcept
{
    return (reinterpret_cast<UIntPtr>(ptr) & (alignment - 1)) == 0;
}

/// \brief Construct an object in-place at the given memory address.
/// \tparam T Type to construct.
/// \tparam Args Constructor argument types.
/// \param ptr Raw memory where the object will be constructed.
/// \param args Arguments forwarded to the constructor.
/// \return Pointer to the constructed object.
/// \note This is placement new, it constructs an object at a specific memory address without allocating new memory.
/// Game engines use this extensively: the allocator provides raw bytes, then Construct() initializes the object at that
/// address. This separates allocation from initialization, which is critical for pool allocators and arena allocators.
template <typename T, typename... Args>
GP_FORCEINLINE T* Construct(void* ptr, Args&&... args)
{
    GP_ASSERT(ptr != nullptr, "Cannot construct at nullptr");
    return ::new (ptr) T(std::forward<Args>(args)...);
}

/// \brief Destroy an object without freeing its memory.
/// \tparam T Type to destroy.
/// \param ptr Pointer to the object to destroy.
/// \note This calls the destructor without freeing memory. Paired with Construct(), the allocator manages memory
/// lifetime, while Construct/Destroy manage object lifetime.
template <typename T>
GP_FORCEINLINE void Destroy(T* ptr) noexcept
{
    if constexpr (!Concepts::IsTriviallyDestructible<T>)
    {
        if (ptr) { ptr->~T(); }
    }
}

/// \brief Construct a range of objects using default construction. Optimized: skips construction for trivially
/// default-constructible types.
/// \tparam T Type to construct.
/// \param first Pointer to the first object in the range.
/// \param count Number of objects to construct.
template <typename T>
GP_FORCEINLINE void ConstructRange(T* first, SizeT count)
{
    if constexpr (Concepts::IsTriviallyDefaultConstructible<T>) { std::memset(first, 0, count * sizeof(T)); }
    else
    {
        for (SizeT i = 0; i < count; ++i) { Construct<T>(first + i); }
    }
}

/// \brief Copy-construct a range of objects from a source range. Optimized: uses memcpy for trivially copyable types.
/// \tparam T Type to construct.
/// \param dest Pointer to the first object in the destination range.
/// \param src Pointer to the first object in the source range.
/// \param count Number of objects to construct.
template <typename T>
GP_FORCEINLINE void CopyConstructRange(T* dest, const T* src, SizeT count)
{
    if constexpr (Concepts::IsTriviallyCopyable<T>) { std::memcpy(dest, src, count * sizeof(T)); }
    else
    {
        for (SizeT i = 0; i < count; ++i) { Construct<T>(dest + i, src[i]); }
    }
}

/// \brief Move-construct a range of objects from a source range. Optimized: uses memcpy for trivially copyable types
/// (move == copy).
/// \tparam T Type to construct.
/// \param dest Pointer to the first object in the destination range.
/// \param src Pointer to the first object in the source range.
/// \param count Number of objects to construct.
template <typename T>
GP_FORCEINLINE void MoveConstructRange(T* dest, T* src, SizeT count)
{
    if constexpr (Concepts::IsTriviallyCopyable<T>) { std::memcpy(dest, src, count * sizeof(T)); }
    else
    {
        for (SizeT i = 0; i < count; ++i) { Construct<T>(dest + i, std::move(src[i])); }
    }
}

/// \brief Destroy a range of objects without freeing memory. Optimized: no-op for trivially destructible types.
/// \tparam T Type to destroy.
/// \param first Pointer to the first object in the range.
/// \param count Number of objects to destroy.
template <typename T>
GP_FORCEINLINE void DestroyRange(T* first, SizeT count) noexcept
{
    if constexpr (!Concepts::IsTriviallyDestructible<T>)
    {
        for (SizeT i = 0; i < count; ++i) { Destroy(first + i); }
    }
}

/// \brief Relocate a range of objects from src to dest. Relocate = move-construct at dest + destroy at src. Optimized:
/// single memcpy for trivially copyable types.
/// \tparam T Type to relocate.
/// \param dest Pointer to the first object in the destination range.
/// \param src Pointer to the first object in the source range.
/// \param count Number of objects to relocate.
/// \note "Relocate" is a concept. For trivial types, moving an object is just a memcpy, no need to
/// call the move constructor then the destructor. This can be 10x faster for large arrays of complex objects that
/// happen to be trivially relocatable.
template <typename T>
GP_FORCEINLINE void RelocateRange(T* dest, T* src, SizeT count)
{
    if constexpr (Concepts::IsTriviallyCopyable<T>) { std::memcpy(dest, src, count * sizeof(T)); }
    else
    {
        MoveConstructRange(dest, src, count);
        DestroyRange(src, count);
    }
}

/// \brief Copy-assign a range of objects from a source range.
/// \tparam T Type to assign.
/// \param dest Pointer to the first object in the destination range.
/// \param src Pointer to the first object in the source range.
/// \param count Number of objects to assign.
template <typename T>
GP_FORCEINLINE void CopyAssignRange(T* dest, const T* src, SizeT count)
{
    if constexpr (Concepts::IsTriviallyCopyable<T>) { std::memcpy(dest, src, count * sizeof(T)); }
    else
    {
        for (SizeT i = 0; i < count; ++i) { dest[i] = src[i]; }
    }
}

/// \brief Move-assign a range of objects from a source range.
/// \tparam T Type to assign.
/// \param dest Pointer to the first object in the destination range.
/// \param src Pointer to the first object in the source range.
/// \param count Number of objects to assign.
template <typename T>
GP_FORCEINLINE void MoveAssignRange(T* dest, T* src, SizeT count)
{
    if constexpr (Concepts::IsTriviallyCopyable<T>) { std::memcpy(dest, src, count * sizeof(T)); }
    else
    {
        for (SizeT i = 0; i < count; ++i) { dest[i] = std::move(src[i]); }
    }
}

/// \brief Uninitialized fill: construct N copies of a value in raw memory.
/// \tparam T Type to construct.
/// \param first Pointer to the first object in the range.
/// \param count Number of objects to construct.
/// \param value The value to copy-construct into each object.
template <typename T>
GP_FORCEINLINE void UninitializedFill(T* first, SizeT count, const T& value)
{
    for (SizeT i = 0; i < count; ++i) { Construct<T>(first + i, value); }
}

}   // namespace GP::Memory
