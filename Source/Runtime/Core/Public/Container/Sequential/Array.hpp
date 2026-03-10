// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Container/Utility/Iterator.hpp"
#include "Container/Utility/ReverseIterator.hpp"
#include "Container/Views/ArrayView.hpp"
#include "CoreTypes.hpp"
#include "Math/Scalar/Max.hpp"
#include "Memory/Allocators/DefaultAllocator.hpp"
#include "Memory/Allocators/IAllocator.hpp"
#include "Templates/Concepts/ComparisonConcepts.hpp"
#include "Templates/Concepts/CoreConcepts.hpp"
#include "Templates/Concepts/ObjectConcepts.hpp"
#include "Templates/Core/Conditional.hpp"
#include "Templates/Core/InitializerList.hpp"
#include "Templates/Traits/TypeProperties.hpp"

namespace GP
{

/// @brief Dynamic resizable array with Small Buffer Optimization (SBO).
///
/// Stores up to @p InlineCapacity elements inline (stack / in-object storage) before spilling to the heap. Growth
/// factor of 2× gives O(1) amortised PushBack. Trivially copyable types use memcpy for all bulk operations.
///
/// @tparam T
/// @tparam InlineCapacity
template <typename T, SizeT InlineCapacity = 8>
class TArray
{
private:
    static_assert(!CReference<T>, "TArray cannot store reference types");
    static_assert(CDestructible<T>, "TArray element type must be destructible");
    static_assert(
        CTriviallyCopyable<T> || TIsNothrowConstructible_V<T, T&&>,
        "TArray<T>: T must be trivially copyable or nothrow move-constructible; " "a throwing move during Reallocate corrupts the container."
    );

private:
    /// @brief Empty storage type used when InlineCapacity is 0 to avoid defining the inline storage member, which would
    ///        be wasteful and potentially cause issues with zero-sized arrays.
    struct EmptyStorage
    {};

    /// @brief Storage for inline elements when InlineCapacity > 0. Uses TConditional_T to avoid defining this member
    ///        when InlineCapacity is 0, which would be wasteful and potentially cause issues with zero-sized arrays.
    struct GP_ALIGN(alignof(T)) Storage
    {
        Byte data[sizeof(T) * (InlineCapacity > 0 ? InlineCapacity : 1)];
    };

public:
    using ValueType = T;
    using SizeType = SizeT;
    using DiffType = SSizeT;
    using Pointer = T*;
    using ConstPointer = const T*;
    using Reference = T&;
    using ConstReference = const T&;
    using Iterator = TIterator<T>;
    using ConstIterator = TConstIterator<T>;
    using ReverseIterator = TReverseIteratorT<T>;
    using ConstReverseIterator = TConstReverseIterator<T>;

public:
    static constexpr SizeType NPOS = static_cast<SizeType>(-1);   //<! Sentinel value representing "not found".

private:
    IAllocator* m_alloc{ &GetDefaultAllocator() };   //<! Allocator used for heap operations.
    Pointer m_data{ nullptr };                       //<! Pointer to the array data (either inline or heap).
    SizeT m_size{ 0 };                               //<! Current number of elements in the array.
    SizeT m_capacity{ 0 };                           //<! Current capacity of the array.
    SizeT m_heapBytes{ 0 };                          //<! Number of bytes currently allocated on the heap.

    /// @brief Storage for inline elements when InlineCapacity > 0. Uses TConditional_T to avoid defining this member
    ///        when InlineCapacity is 0, which would be wasteful and potentially cause issues with zero-sized arrays.
    GP_NO_UNIQUE_ADDRESS TConditional_T<(InlineCapacity > 0), Storage, EmptyStorage> m_inline;

public:
    static constexpr SizeT kInlineCapacity = InlineCapacity;
    static constexpr SizeT kMinHeapCapacity = 16;

    /// @brief Minimum alignment used for all heap allocations.
    ///        Guarantees at least 16-byte SIMD-safe alignment regardless of T's natural alignment.
    static constexpr SizeT kAllocAlignment = (alignof(T) > SizeT(16)) ? alignof(T) : SizeT(16);

    /// @brief Profiling tag forwarded to IAllocator::AllocateTagged for engine memory accounting.
    static constexpr const char kStatTag[] = "TArray";

public:
    /// @brief Constructs an empty TArray using the provided allocator.
    /// @param alloc Allocator used for any required heap allocations.
    explicit TArray(IAllocator& alloc = GetDefaultAllocator()) noexcept
        : m_alloc(&alloc)
    {
        SetupInline();
    }

    /// @brief Constructs a TArray with the given size, default-initialized elements.
    /// @param count Number of elements to create.
    /// @param alloc Allocator used for heap allocations if needed.
    explicit TArray(SizeT count, IAllocator& alloc = GetDefaultAllocator())
        : m_alloc(&alloc)
    {
        SetupInline();
        Resize(count);
    }

    /// @brief Constructs a TArray with the given size and fills it with `value`.
    /// @param count Number of elements to create.
    /// @param value Value to copy-construct into each element.
    /// @param alloc Allocator used for heap allocations if needed.
    TArray(SizeT count, const T& value, IAllocator& alloc = GetDefaultAllocator())
        : m_alloc(&alloc)
    {
        SetupInline();
        Resize(count, value);
    }

    /// @brief Constructs a TArray from an initializer list.
    /// @param list Elements to copy into the new array.
    /// @param alloc Allocator used for heap allocations if the list exceeds inline capacity.
    TArray(TInitializerList<T> list, IAllocator& alloc = GetDefaultAllocator())
        : m_alloc(&alloc)
    {
        SetupInline();
        const SizeT count = list.size();
        Reserve(count);
        DoConstruct(m_data, list.begin(), count);
        m_size = count;
    }

    /// @brief Constructs a TArray from a pointer range [first, last).
    /// @param first Pointer to first element (inclusive).
    /// @param last Pointer one-past-last element (exclusive).
    /// @param alloc Allocator used for heap allocations if the range exceeds inline capacity.
    TArray(const T* first, const T* last, IAllocator& alloc = GetDefaultAllocator())
        : m_alloc(&alloc)
    {
        GP_ASSERT(first <= last);
        SetupInline();
        const SizeT count = static_cast<SizeT>(last - first);
        Reserve(count);
        DoConstruct(m_data, first, count);
        m_size = count;
    }

    /// @brief Copy-constructs a TArray from `other`.
    /// @param other The array to copy from.
    TArray(const TArray& other)
        : m_alloc(other.m_alloc)
    {
        SetupInline();
        CopyFrom(other);
    }

    /// @brief Move-constructs a TArray, taking ownership of `other`'s heap storage when applicable.
    /// @param other The array to move from.
    TArray(TArray&& other) noexcept
        : m_alloc(other.m_alloc)
    {
        SetupInline();
        MoveFrom(GP::Move(other));
    }

    /// @brief Destroys all elements and releases any heap storage.
    ~TArray()
    {
        DestroyN(m_data, m_size);
        FreeHeap();
    }

public:
    /// @brief Copy-assigns from another TArray, replacing current contents.
    /// @param other Source array to copy.
    /// @return Reference to this.
    TArray& operator=(const TArray& other)
    {
        if (this != &other)
        {
            Clear();
            CopyFrom(other);
        }
        return *this;
    }

    /// @brief Move-assigns from another TArray, taking ownership of resources when possible.
    /// @param other Source array to move from.
    /// @return Reference to this.
    TArray& operator=(TArray&& other) noexcept
    {
        if (this != &other)
        {
            Clear();
            FreeHeap();
            MoveFrom(GP::Move(other));
        }
        return *this;
    }

    /// @brief Assigns contents from an initializer list.
    /// @param list Elements to copy into this array.
    /// @return Reference to this.
    TArray& operator=(TInitializerList<T> list)
    {
        Clear();
        const SizeT count = list.size();
        Reserve(count);
        DoConstruct(m_data, list.begin(), count);
        m_size = count;
        return *this;
    }

    /// @brief Returns a mutable reference to the element at `index`.
    /// @param index Index of the element to access.
    /// @return Reference to the element.
    GP_NODISCARD constexpr Reference operator[](SizeT index) noexcept
    {
        GP_ASSERT(index < m_size);
        return m_data[index];
    }

    /// @brief Returns a const reference to the element at `index`.
    /// @param index Index of the element to access.
    /// @return Const reference to the element.
    GP_NODISCARD constexpr ConstReference operator[](SizeT index) const noexcept
    {
        GP_ASSERT(index < m_size);
        return m_data[index];
    }

    /// @brief Compares equality with another array (element-wise).
    /// @param other Array to compare against.
    /// @return true if sizes and all elements are equal.
    GP_NODISCARD bool operator==(const TArray& other) const noexcept requires CEqualityComparable<T>
    {
        if (m_size != other.m_size) { return false; }
        for (SizeT i = 0; i < m_size; ++i)
        {
            if (m_data[i] != other.m_data[i]) { return false; }
        }
        return true;
    }

    /// @brief Inequality comparison with another array.
    /// @param other Array to compare against.
    /// @return true if arrays differ.
    GP_NODISCARD bool operator!=(const TArray& other) const noexcept requires CEqualityComparable<T>
    {
        return !(*this == other);
    }

    /// @brief Implicit conversion to a mutable non-owning view over this array's elements.
    /// @return TArrayView<T> covering [data, data + size).
    operator TArrayView<T>() noexcept { return TArrayView<T>(m_data, m_size); }

    /// @brief Implicit conversion to an immutable non-owning view over this array's elements.
    /// @return TArrayView<const T> covering [data, data + size).
    operator TArrayView<const T>() const noexcept { return TArrayView<const T>(m_data, m_size); }

public:
    /// @brief Returns the number of elements in the array.
    /// @return Current size.
    GP_NODISCARD constexpr SizeT Size() const noexcept { return m_size; }

    /// @brief Returns the current capacity of the array.
    /// @return Capacity (number of elements that can be stored without reallocating).
    GP_NODISCARD constexpr SizeT Capacity() const noexcept { return m_capacity; }

    /// @brief Returns whether the array is empty.
    /// @return true if size is zero.
    GP_NODISCARD constexpr bool IsEmpty() const noexcept { return m_size == 0; }

    /// @brief Returns whether the storage is currently using the inline buffer.
    /// @return true if data points to inline storage.
    GP_NODISCARD constexpr bool IsInline() const noexcept { return m_data == InlineBuffer(); }

    /// @brief Ensures the array can hold at least `count` elements without reallocating.
    /// @param count Minimum desired capacity.
    void Reserve(SizeT count)
    {
        if (count > m_capacity) { Reallocate(count); }
    }

    /// @brief Increments the size by `count` and returns a pointer to the first new uninitialised slot.
    ///        No constructors are called. The caller must initialise every returned element before any
    ///        operation that could trigger destruction (e.g. Clear, Resize, or the destructor itself).
    /// @param count Number of uninitialised element slots to claim.
    /// @return Pointer to the first newly claimed (uninitialised) element.
    /// @warning The returned memory is uninitialised. Reading it without prior construction is UB.
    GP_NODISCARD T* AddUninitialized(SizeT count)
    {
        GP_ASSERT(count > 0);
        const SizeT newSize = m_size + count;
        if (newSize > m_capacity)
        {
            const SizeT grown = m_capacity + (m_capacity / 2);
            Reallocate(grown > newSize ? grown : newSize);
        }
        T* ptr = m_data + m_size;
        m_size = newSize;
        return ptr;
    }

    /// @brief Resizes the array to `count`, default-constructing new elements if expanding.
    /// @param count New size.
    void Resize(SizeT count)
    {
        if (count > m_size)
        {
            Reserve(count);
            ConstructDefault(m_data + m_size, count - m_size);
        }
        else { DestroyN(m_data + count, m_size - count); }
        m_size = count;
    }

    /// @brief Resizes the array to `count`, filling new slots with `value` if expanding.
    /// @param count New size.
    /// @param value Value to copy into new elements.
    void Resize(SizeT count, const T& value)
    {
        if (count > m_size)
        {
            Reserve(count);
            for (SizeT i = m_size; i < count; ++i) { ::new (m_data + i) T(value); }
        }
        else { DestroyN(m_data + count, m_size - count); }
        m_size = count;
    }

    /// @brief Reduces capacity to exactly fit the current size.
    ///        Moves elements back into inline storage when the size fits.
    ///        Prefer Trim() in production code; see its documentation for guidance.
    void ShrinkToFit()
    {
        if (m_size == m_capacity) { return; }
        if (m_size <= InlineCapacity && !IsInline())
        {
            T* source = m_data;
            SizeT sourceSize = m_size;
            SizeT bytes = m_heapBytes;
            m_data = InlineBuffer();
            m_capacity = InlineCapacity > 0 ? InlineCapacity : 0;
            DoMoveConstruct(m_data, source, sourceSize);
            DestroyN(source, sourceSize);
            m_alloc->Deallocate(source, bytes);
            m_heapBytes = 0;
        }
        else if (m_size > InlineCapacity) { Reallocate(m_size); }
    }

    /// @brief Explicitly releases excess capacity to fit the current size.
    ///        Equivalent to ShrinkToFit(). Use sparingly; repeated trimming causes heap fragmentation.
    void Trim() { ShrinkToFit(); }

    /// @brief Appends `count` copies from the raw pointer `data` to the back of this array.
    ///        Safely handles the case where `data` points into this array's own storage
    ///        (reallocation would otherwise invalidate the source pointer).
    /// @param data  Pointer to the first element to copy.
    /// @param count Number of elements to append.
    void Append(const T* data, SizeT count)
    {
        if GP_UNLIKELY (count == 0) { return; }
        // Detect self-overlap: source is inside our buffer; reallocation would invalidate `data`.
        const bool selfOverlap = (data >= m_data && data < m_data + m_size);
        const SizeT srcOffset = selfOverlap ? static_cast<SizeT>(data - m_data) : SizeT(0);
        const SizeT newSize = m_size + count;
        if (newSize > m_capacity)
        {
            const SizeT grown = m_capacity + (m_capacity / 2);
            Reallocate(grown > newSize ? grown : newSize);
        }
        // After potential reallocation, recompute the source pointer when it was aliased.
        const T* src = selfOverlap ? m_data + srcOffset : data;
        DoConstruct(m_data + m_size, src, count);
        m_size = newSize;
    }

    /// @brief Appends all elements of `other` to the back of this array.
    ///        Safe when `other` is the same array instance (self-append).
    /// @param other Source array whose elements are appended.
    void Append(const TArray& other) { Append(other.m_data, other.m_size); }

    /// @brief Appends all elements of an initializer list to the back of this array.
    /// @param list Initializer list of elements to append.
    void Append(TInitializerList<T> list) { Append(list.begin(), static_cast<SizeT>(list.size())); }

    /// @brief Appends a copy of `value` to the end of the array.
    /// @param value Value to copy into the new element.
    void PushBack(const T& value)
    {
        // Guard against self-reference: capture source index before a potential reallocation
        // invalidates the reference (e.g. arr.PushBack(arr[0])).
        const bool selfRef = IsInternalPtr(&value);
        const SizeT selfIdx = selfRef ? static_cast<SizeT>(&value - m_data) : SizeT(0);
        GrowIfNeeded();
        ::new (m_data + m_size++) T(selfRef ? static_cast<const T&>(m_data[selfIdx]) : value);
    }

    /// @brief Appends `value` to the end of the array by moving it.
    /// @param value Rvalue to move into the new element.
    void PushBack(T&& value)
    {
        // Guard against self-move: capture source index before a potential reallocation
        // invalidates the reference (e.g. arr.PushBack(GP::Move(arr[0]))).
        const bool selfRef = IsInternalPtr(&value);
        const SizeT selfIdx = selfRef ? static_cast<SizeT>(&value - m_data) : SizeT(0);
        GrowIfNeeded();
        ::new (m_data + m_size++) T(GP::Move(selfRef ? m_data[selfIdx] : value));
    }

    /// @brief Constructs a new element in-place at the end using forwarded arguments.
    /// @tparam Args Parameter pack forwarded to `T`'s constructor.
    /// @param args Constructor arguments for the new element.
    /// @return Reference to the newly created element.
    template <typename... Args>
    T& EmplaceBack(Args&&... args)
    {
        GrowIfNeeded();
        return *::new (m_data + m_size++) T(GP::Forward<Args>(args)...);
    }

    /// @brief Removes and returns the last element by moving it out.
    /// @return Moved value of the last element.
    T PopBack() noexcept
    {
        GP_ASSERT(!IsEmpty());
        T value = GP::Move(m_data[m_size - 1]);
        m_data[--m_size].~T();
        return value;
    }

    /// @brief Inserts `value` at `index`, shifting subsequent elements right (copy).
    /// @param index Position to insert at (may equal `Size()`).
    /// @param value Value to copy into the inserted element.
    void Insert(SizeT index, const T& value)
    {
        GP_ASSERT(index <= m_size);
        // Guard against self-reference: capture index before reallocation or ShiftRight invalidates &value.
        const bool selfRef = IsInternalPtr(&value);
        const SizeT selfIdx = selfRef ? static_cast<SizeT>(&value - m_data) : SizeT(0);
        GrowIfNeeded();
        ShiftRight(index, 1);
        if (selfRef)
        {
            // ShiftRight displaced the element at selfIdx rightward by 1 if it was at or past `index`.
            const SizeT srcIdx = (selfIdx >= index) ? selfIdx + 1 : selfIdx;
            ::new (m_data + index) T(m_data[srcIdx]);
        }
        else { ::new (m_data + index) T(value); }
        ++m_size;
    }

    /// @brief Inserts `value` at `index`, shifting subsequent elements right (move).
    /// @param index Position to insert at (may equal `Size()`).
    /// @param value Rvalue to move into the inserted element.
    void Insert(SizeT index, T&& value)
    {
        GP_ASSERT(index <= m_size);
        // Guard against self-move: capture index before reallocation or ShiftRight invalidates &value.
        const bool selfRef = IsInternalPtr(&value);
        const SizeT selfIdx = selfRef ? static_cast<SizeT>(&value - m_data) : SizeT(0);
        GrowIfNeeded();
        ShiftRight(index, 1);
        if (selfRef)
        {
            const SizeT srcIdx = (selfIdx >= index) ? selfIdx + 1 : selfIdx;
            ::new (m_data + index) T(GP::Move(m_data[srcIdx]));
        }
        else { ::new (m_data + index) T(GP::Move(value)); }
        ++m_size;
    }

    /// @brief Removes element at `index` by replacing it with the last element (order not preserved).
    /// @param index Index of the element to remove.
    void RemoveAtSwap(SizeT index) noexcept
    {
        GP_ASSERT(index < m_size);
        if (index != m_size - 1) { m_data[index] = GP::Move(m_data[m_size - 1]); }
        m_data[--m_size].~T();
    }

    /// @brief Removes element at `index`, shifting later elements left to preserve order.
    /// @param index Index of the element to remove.
    void RemoveAt(SizeT index) noexcept
    {
        GP_ASSERT(index < m_size);
        m_data[index].~T();
        for (SizeT i = index; i < m_size - 1; ++i)
        {
            ::new (m_data + i) T(GP::Move(m_data[i + 1]));
            m_data[i + 1].~T();
        }
        --m_size;
    }

    /// @brief Removes the first occurrence of `value` (preserving order) if present.
    /// @param value Value to remove.
    /// @return true if an element was removed.
    bool Remove(const T& value) noexcept
    {
        SizeT idx = Find(value);
        if (idx == NPOS) { return false; }
        RemoveAt(idx);
        return true;
    }

    /// @brief Destroys all elements and resets size to zero. Capacity is unchanged.
    void Clear() noexcept
    {
        DestroyN(m_data, m_size);
        m_size = 0;
    }

    /// @brief Finds the first index of `value` using equality comparison.
    /// @param value Value to search for.
    /// @return Index of found element or `NPOS` if not found.
    GP_NODISCARD SizeT Find(const T& value) const noexcept
    {
        for (SizeT i = 0; i < m_size; ++i)
        {
            if (m_data[i] == value) { return i; }
        }
        return NPOS;
    }

    /// @brief Finds the first index where `pred(element)` is true.
    /// @tparam Pred Predicate invocable with `const T&` returning `bool`.
    /// @param pred Predicate to evaluate each element.
    /// @return Index of the first matching element or `NPOS` if none match.
    template <CPredicate<const T&> Pred>
    GP_NODISCARD SizeT FindIf(Pred&& pred) const noexcept
    {
        for (SizeT i = 0; i < m_size; ++i)
        {
            if (pred(m_data[i])) { return i; }
        }
        return NPOS;
    }

    /// @brief Returns whether the array contains `value`.
    /// @param value Value to check.
    /// @return true if present.
    GP_NODISCARD bool Contains(const T& value) const noexcept { return Find(value) != NPOS; }

    /// @brief Returns a reference to the first element.
    /// @return Reference to the first element.
    GP_NODISCARD constexpr Reference Front() noexcept
    {
        GP_ASSERT(!IsEmpty());
        return m_data[0];
    }

    /// @brief Returns a const reference to the first element.
    /// @return Const reference to the first element.
    GP_NODISCARD constexpr ConstReference Front() const noexcept
    {
        GP_ASSERT(!IsEmpty());
        return m_data[0];
    }

    /// @brief Returns a reference to the last element.
    /// @return Reference to the last element.
    GP_NODISCARD constexpr Reference Back() noexcept
    {
        GP_ASSERT(!IsEmpty());
        return m_data[m_size - 1];
    }

    /// @brief Returns a const reference to the last element.
    /// @return Const reference to the last element.
    GP_NODISCARD constexpr ConstReference Back() const noexcept
    {
        GP_ASSERT(!IsEmpty());
        return m_data[m_size - 1];
    }

    /// @brief Issues CPU prefetch hints for every cache line that covers the array's data.
    ///        Call this immediately before a hot loop to warm the L1 cache and reduce load-use stalls.
    ///        No-op on empty arrays or when the data pointer is null.
    /// @note  The prefetch distance covers all sizeof(T) * Size() bytes, one GP_CACHE_LINE_SIZE at a time.
    void Prefetch() const noexcept
    {
        if GP_UNLIKELY (m_data == nullptr || m_size == 0) { return; }
        const Byte* ptr = reinterpret_cast<const Byte*>(m_data);
        const SizeT total = m_size * sizeof(T);
        for (SizeT offset = 0; offset < total; offset += GP_CACHE_LINE_SIZE) { GP_PREFETCH_R(ptr + offset); }
    }

    /// @brief Erases the element pointed to by `pos`.
    ///        Preserves element order by shifting subsequent elements left.
    /// @param pos Valid, dereferenceable iterator into this array.
    /// @return Iterator to the element that immediately followed the erased one, or end() if it was the last.
    Iterator Erase(ConstIterator pos) noexcept
    {
        GP_ASSERT(pos >= cbegin() && pos < cend());
        const SizeT index = static_cast<SizeT>(pos - cbegin());
        RemoveAt(index);
        return Iterator(m_data + index);
    }

    /// @brief Erases all elements in the half-open range [first, last).
    ///        Preserves element order. A no-op when first == last.
    /// @param first Iterator to the first element to erase.
    /// @param last Iterator one-past the last element to erase.
    /// @return Iterator to the element that followed the last erased element, or end().
    Iterator Erase(ConstIterator first, ConstIterator last) noexcept
    {
        GP_ASSERT(first >= cbegin() && last <= cend() && first <= last);
        const SizeT from = static_cast<SizeT>(first - cbegin());
        const SizeT count = static_cast<SizeT>(last - first);
        if (count == 0) { return Iterator(m_data + from); }

        DestroyN(m_data + from, count);

        const SizeT remaining = m_size - from - count;
        if (remaining > 0)
        {
            if constexpr (CTriviallyCopyable<T>)
            {
                // Safe overlap: destination is always before source when shifting left.
                ::memmove(m_data + from, m_data + from + count, sizeof(T) * remaining);
            }
            else
            {
                for (SizeT i = 0; i < remaining; ++i)
                {
                    ::new (m_data + from + i) T(GP::Move(m_data[from + count + i]));
                    m_data[from + count + i].~T();
                }
            }
        }

        m_size -= count;
        return Iterator(m_data + from);
    }

    /// @brief Removes all elements for which `pred` returns true, preserving the relative order of survivors.
    ///        Single-pass write-pointer compaction, equivalent to the erase-remove idiom but with no redundant
    ///        iterator invalidation step. O(n) moves in the worst case.
    /// @tparam Pred Predicate invocable with `const T&`, returning bool.
    /// @param pred Predicate to evaluate each element; returning true marks the element for removal.
    /// @return Number of elements removed.
    template <CPredicate<const T&> Pred>
    SizeT RemoveIf(Pred&& pred) noexcept
    {
        SizeT write = 0;
        for (SizeT read = 0; read < m_size; ++read)
        {
            if (pred(m_data[read]))
            {
                // Element is condemned, destroy it in-place.
                if constexpr (!CTriviallyDestructible<T>) { m_data[read].~T(); }
            }
            else
            {
                // Element survives, relocate it to the write cursor if compaction is active.
                if (write != read)
                {
                    if constexpr (CTriviallyCopyable<T>) { ::memcpy(m_data + write, m_data + read, sizeof(T)); }
                    else
                    {
                        ::new (m_data + write) T(GP::Move(m_data[read]));
                        m_data[read].~T();
                    }
                }
                ++write;
            }
        }

        const SizeT removed = m_size - write;
        m_size = write;
        return removed;
    }

    /// @brief Removes all elements for which `pred` returns true by swapping each condemned element
    ///        with the current tail (order NOT preserved). Preferred over RemoveIf when element order
    ///        does not matter and the removal rate is expected to be low.
    /// @tparam Pred Predicate invocable with `const T&`, returning bool.
    /// @param pred Predicate to evaluate each element; returning true marks the element for removal.
    /// @return Number of elements removed.
    template <CPredicate<const T&> Pred>
    SizeT RemoveIfSwap(Pred&& pred) noexcept
    {
        SizeT removed = 0;
        for (SizeT i = 0; i < m_size;)
        {
            if (pred(m_data[i]))
            {
                RemoveAtSwap(i);
                ++removed;
                // Do NOT advance i: the element swapped in from the tail has not been evaluated yet.
            }
            else { ++i; }
        }
        return removed;
    }

    /// @brief Returns a pointer to the underlying element storage.
    /// @return Mutable pointer to element data.
    GP_NODISCARD constexpr Pointer Data() noexcept { return m_data; }

    /// @brief Returns a const pointer to the underlying element storage.
    /// @return Const pointer to element data.
    GP_NODISCARD constexpr ConstPointer Data() const noexcept { return m_data; }

    /// @brief Returns an iterator to the first element.
    /// @return Mutable iterator to begin.
    GP_NODISCARD constexpr Iterator begin() noexcept { return Iterator(m_data); }

    /// @brief Returns an iterator to one-past-last element.
    /// @return Mutable iterator to end.
    GP_NODISCARD constexpr Iterator end() noexcept { return Iterator(m_data + m_size); }

    /// @brief Returns a const iterator to the first element.
    /// @return Const iterator to begin.
    GP_NODISCARD constexpr ConstIterator begin() const noexcept { return ConstIterator(m_data); }

    /// @brief Returns a const iterator to one-past-last element.
    /// @return Const iterator to end.
    GP_NODISCARD constexpr ConstIterator end() const noexcept { return ConstIterator(m_data + m_size); }

    /// @brief Returns a const iterator to the first element.
    /// @return Const iterator to begin.
    GP_NODISCARD constexpr ConstIterator cbegin() const noexcept { return begin(); }

    /// @brief Returns a const iterator to one-past-last element.
    /// @return Const iterator to end.
    GP_NODISCARD constexpr ConstIterator cend() const noexcept { return end(); }

    /// @brief Returns a reverse iterator to the last element.
    /// @return Mutable reverse iterator to rbegin.
    GP_NODISCARD constexpr ReverseIterator rbegin() noexcept { return ReverseIterator(end()); }

    /// @brief Returns a reverse iterator to one-before-first element.
    /// @return Mutable reverse iterator to rend.
    GP_NODISCARD constexpr ReverseIterator rend() noexcept { return ReverseIterator(begin()); }

    /// @brief Returns a const reverse iterator to the last element.
    /// @return Const reverse iterator to rbegin.
    GP_NODISCARD constexpr ConstReverseIterator rbegin() const noexcept { return ConstReverseIterator(cend()); }

    /// @brief Returns a const reverse iterator to one-before-first element.
    /// @return Const reverse iterator to rend.
    GP_NODISCARD constexpr ConstReverseIterator rend() const noexcept { return ConstReverseIterator(cbegin()); }

    /// @brief Returns the allocator used by this array.
    /// @return Reference to the allocator.
    GP_NODISCARD IAllocator& GetAllocator() const noexcept { return *m_alloc; }

    /// @brief Returns a mutable non-owning view over the array's elements.
    /// @return TArrayView<T> covering [data, data + size).
    GP_NODISCARD TArrayView<T> AsView() noexcept { return TArrayView<T>(m_data, m_size); }

    /// @brief Returns an immutable non-owning view over the array's elements.
    /// @return TArrayView<const T> covering [data, data + size).
    GP_NODISCARD TArrayView<const T> AsView() const noexcept { return TArrayView<const T>(m_data, m_size); }

private:
    /// @brief Returns true if `ptr` falls within the currently occupied element range [m_data, m_data+m_size).
    ///        Used to detect self-aliased references before grow or shift operations invalidate them.
    /// @param ptr Pointer to test for containment.
    /// @return true when ptr aliases an existing element in this array.
    GP_NODISCARD GP_FORCEINLINE bool IsInternalPtr(const T* ptr) const noexcept
    {
        return ptr >= m_data && ptr < m_data + m_size;
    }

    /// @brief Returns pointer to the inline storage buffer, or nullptr when inline capacity is zero.
    /// @return Pointer to inline buffer.
    GP_NODISCARD T* InlineBuffer() noexcept
    {
        if constexpr (InlineCapacity > 0) { return reinterpret_cast<T*>(m_inline.data); }
        else { return nullptr; }
    }

    /// @brief Returns const pointer to inline buffer or nullptr when inline capacity is zero.
    /// @return Const pointer to inline buffer.
    GP_NODISCARD const T* InlineBuffer() const noexcept
    {
        if constexpr (InlineCapacity > 0) { return reinterpret_cast<const T*>(m_inline.data); }
        else { return nullptr; }
    }

    /// @brief Initializes data and capacity to use the inline buffer when available.
    void SetupInline() noexcept
    {
        if constexpr (InlineCapacity > 0)
        {
            m_data = InlineBuffer();
            m_capacity = InlineCapacity;
        }
    }

    /// @brief Checks size against capacity and grows storage if required (fast inline check).
    GP_FORCEINLINE void GrowIfNeeded()
    {
        if GP_UNLIKELY (m_size >= m_capacity) { Grow(); }
    }

    /// @brief Increases capacity by a 1.5x factor and reallocates storage.
    ///        A 1.5x growth rate (vs 2x) allows the allocator to satisfy future allocations from
    ///        previously freed blocks once the cumulative freed space exceeds the new size.
    GP_NOINLINE void Grow()
    {
        const SizeT newCapacity = m_capacity == 0
                                      ? GP::Max(InlineCapacity ? InlineCapacity : SizeT(0), kMinHeapCapacity)
                                      : m_capacity + (m_capacity / 2);
        Reallocate(newCapacity);
    }

    /// @brief Reallocates storage to `newCapacity` elements and migrates existing elements.
    ///        For TIsBitwiseRelocatable types a single memcpy transfers all elements without any
    ///        constructor or destructor calls. For other types the standard move+destroy path is used.
    ///        All allocations use kAllocAlignment (>= 16 bytes) for SIMD-safe data pointers.
    /// @param newCapacity Desired new capacity (must be >= m_size).
    void Reallocate(SizeT newCapacity)
    {
        GP_ASSERT(newCapacity >= m_size);
        const SizeT newBytes = sizeof(T) * newCapacity;
        T* newData = static_cast<T*>(m_alloc->AllocateTagged(newBytes, kAllocAlignment, kStatTag));
        GP_ASSERT(newData);
        if (m_size)
        {
            if constexpr (TIsBitwiseRelocatable_V<T>)
            {
                // Bitwise-relocatable types can be transferred with a single memcpy.
                // No move-constructor or destructor calls are required on the source bytes.
                ::memcpy(newData, m_data, sizeof(T) * m_size);
            }
            else
            {
                DoMoveConstruct(newData, m_data, m_size);
                DestroyN(m_data, m_size);
            }
        }
        if (m_data && m_data != InlineBuffer()) { m_alloc->Deallocate(m_data, m_heapBytes); }
        m_data = newData;
        m_capacity = newCapacity;
        m_heapBytes = newBytes;
    }

    /// @brief Frees heap allocation (if any) and resets storage to inline buffer.
    void FreeHeap() noexcept
    {
        if (m_data && m_data != InlineBuffer()) { m_alloc->Deallocate(m_data, m_heapBytes); }
        m_heapBytes = 0;
        SetupInline();
        if constexpr (InlineCapacity == 0)
        {
            m_data = nullptr;
            m_capacity = 0;
        }
    }

    /// @brief Copies contents from `other` into this array, allocating if necessary.
    /// @param other Source array to copy.
    void CopyFrom(const TArray& other)
    {
        Reserve(other.m_size);
        DoConstruct(m_data, other.m_data, other.m_size);
        m_size = other.m_size;
    }

    /// @brief Moves contents from `other` into this array, taking ownership of heap storage when possible.
    /// @param other Source array to move from.
    void MoveFrom(TArray&& other) noexcept
    {
        m_size = other.m_size;
        if (other.IsInline())
        {
            m_data = InlineBuffer();
            m_capacity = InlineCapacity > 0 ? InlineCapacity : 0;
            DoMoveConstruct(m_data, other.m_data, other.m_size);
            DestroyN(other.m_data, other.m_size);
        }
        else
        {
            m_data = other.m_data;
            m_capacity = other.m_capacity;
            m_heapBytes = other.m_heapBytes;
            other.m_data = other.InlineBuffer();
        }
        other.m_size = 0;
        other.m_capacity = InlineCapacity > 0 ? InlineCapacity : 0;
        other.m_heapBytes = 0;
    }

    /// @brief Opens a gap of `count` slots at `from` by shifting elements in [from, m_size) rightward.
    ///        Elements whose destination index >= m_size (uninitialised memory) are move-constructed;
    ///        those whose destination index < m_size are move-assigned.
    ///        The source slots in [from, min(from+count, m_size)) are left destroyed (ready for
    ///        in-place construction by the caller).  The caller is responsible for reserving sufficient
    ///        capacity before calling ShiftRight.
    /// @param from  First index of the gap to open.
    /// @param count Width of the gap in element slots.
    void ShiftRight(SizeT from, SizeT count) noexcept
    {
        if (m_size <= from || count == 0) { return; }
        // Iterate from the top downward so we never overwrite a source before it has been copied.
        for (SSizeT i = static_cast<SSizeT>(m_size + count) - 1; i >= static_cast<SSizeT>(from + count); --i)
        {
            const SizeT src = static_cast<SizeT>(i) - count;
            if (static_cast<SizeT>(i) >= m_size) { ::new (m_data + i) T(GP::Move(m_data[src])); }
            else { m_data[i] = GP::Move(m_data[src]); }
        }
        // Destroy only the elements that were moved from valid (initialised) source slots.
        const SizeT gapEnd = (from + count < m_size) ? from + count : m_size;
        for (SizeT i = from; i < gapEnd; ++i) { m_data[i].~T(); }
    }

private:
    /// @brief Default-constructs `count` elements into `destination`.
    /// @param destination Pointer to uninitialized element storage.
    /// @param count Number of elements to construct.
    static void ConstructDefault(T* destination, SizeT count)
    {
        if constexpr (CTriviallyDefaultConstructible<T>) { ::memset(destination, 0, sizeof(T) * count); }
        else
        {
            for (SizeT i = 0; i < count; ++i) { ::new (destination + i) T(); }
        }
    }

    /// @brief Constructs `count` copies from `source` into `destination`.
    /// @param destination Destination memory for constructed elements.
    /// @param source Source elements to copy from.
    /// @param count Number of elements to construct.
    static void DoConstruct(T* GP_RESTRICT destination, const T* GP_RESTRICT source, SizeT count)
    {
        if constexpr (CTriviallyCopyable<T>) { ::memcpy(destination, source, sizeof(T) * count); }
        else
        {
            for (SizeT i = 0; i < count; ++i) { ::new (destination + i) T(source[i]); }
        }
    }

    /// @brief Move-constructs `count` elements from `source` into `destination`.
    /// @param destination Destination memory for constructed elements.
    /// @param source Source elements to move from.
    /// @param count Number of elements to construct.
    static void DoMoveConstruct(T* GP_RESTRICT destination, T* GP_RESTRICT source, SizeT count)
    {
        if constexpr (CTriviallyCopyable<T>) { ::memcpy(destination, source, sizeof(T) * count); }
        else
        {
            for (SizeT i = 0; i < count; ++i) { ::new (destination + i) T(GP::Move(source[i])); }
        }
    }

    /// @brief Destroys `count` elements starting at `data` if destruction is required for T.
    /// @param data Pointer to elements to destroy.
    /// @param count Number of elements to destroy.
    static void DestroyN(T* data, SizeT count) noexcept
    {
        if constexpr (!CTriviallyDestructible<T>)
        {
            for (SizeT i = 0; i < count; ++i) { data[i].~T(); }
        }
    }
};

/// @brief
/// @tparam T
template <typename T>
TArray(TInitializerList<T>) -> TArray<T>;

}   // namespace GP
