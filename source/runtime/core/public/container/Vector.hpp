// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "container/Forward.hpp"
#include "CoreMinimal.hpp"
#include "memory/AllocatorTraits.hpp"
#include "memory/Forward.hpp"
#include <cstring>
#include <initializer_list>
#include <iterator>
#include <type_traits>

namespace gp
{

/// @brief Contiguous dynamic array with custom allocator support and trivial-type fast-paths.
/// @details
/// Implements the standard vector interface for interoperability with <algorithm>.
/// Growth factor is hardcoded at 1.5x to minimize memory spikes and improve allocator
/// reuse in game engine scenarios. For trivially copyable types, reallocation and
/// element shifting use memcpy/memmove instead of per-element construct/destroy.
/// @tparam T The element type.
/// @tparam Allocator The allocator type (defaults to gp::memory::PolymorphicAllocator<T>).
template <typename T, typename Allocator>
class Vector
{
public:
    using ValueType = T;
    using AllocatorType = Allocator;
    using Traits = gp::memory::AllocatorTraits<Allocator>;
    using SizeType = gp::USize;
    using DifferenceType = gp::ISize;
    using Reference = ValueType&;
    using ConstReference = const ValueType&;
    using Pointer = ValueType*;
    using ConstPointer = const ValueType*;
    using Iterator = Pointer;
    using ConstIterator = ConstPointer;
    using ReverseIterator = std::reverse_iterator<Iterator>;
    using ConstReverseIterator = std::reverse_iterator<ConstIterator>;

private:
    Pointer m_data{ nullptr };
    SizeType m_size{ 0 };
    SizeType m_capacity{ 0 };
    GP_NO_UNIQUE_ADDRESS AllocatorType m_allocator{};

public:
    /// @brief Default constructs an empty vector with the default allocator.
    Vector() noexcept = default;

    /// @brief Constructs an empty vector with the given allocator.
    /// @param[in] alloc The allocator instance to use.
    explicit Vector(const AllocatorType& alloc) noexcept
        : m_allocator(alloc)
    {}

    /// @brief Constructs a vector with count default-inserted elements.
    /// @param[in] count Number of elements to default-insert.
    /// @param[in] alloc The allocator instance to use.
    explicit Vector(SizeType count, const AllocatorType& alloc = AllocatorType{})
        : m_allocator(alloc)
    {
        if (count > 0)
        {
            m_data = Traits::allocate(m_allocator, count);
            m_capacity = count;
            _defaultConstructRange(m_data, count);
            m_size = count;
        }
    }

    /// @brief Constructs a vector with count copies of value.
    /// @param[in] count Number of elements.
    /// @param[in] value The value to copy.
    /// @param[in] alloc The allocator instance to use.
    Vector(SizeType count, const T& value, const AllocatorType& alloc = AllocatorType{})
        : m_allocator(alloc)
    {
        if (count > 0)
        {
            m_data = Traits::allocate(m_allocator, count);
            m_capacity = count;
            _fillConstructRange(m_data, count, value);
            m_size = count;
        }
    }

    /// @brief Constructs a vector from an iterator range.
    /// @tparam InputIt The input iterator type.
    /// @param[in] first Iterator to the beginning of the range.
    /// @param[in] last Iterator to the end of the range.
    /// @param[in] alloc The allocator instance to use.
    template <typename InputIt>
    requires(!std::is_integral_v<InputIt>)
    Vector(InputIt first, InputIt last, const AllocatorType& alloc = AllocatorType{})
        : m_allocator(alloc)
    {
        _rangeInit(first, last);
    }

    /// @brief Constructs a vector from an initializer list.
    /// @param[in] init The initializer list.
    /// @param[in] alloc The allocator instance to use.
    Vector(std::initializer_list<T> init, const AllocatorType& alloc = AllocatorType{})
        : m_allocator(alloc)
    {
        if (init.size() > 0)
        {
            m_data = Traits::allocate(m_allocator, init.size());
            m_capacity = init.size();
            _copyConstructRange(m_data, init.begin(), init.size());
            m_size = init.size();
        }
    }

    /// @brief Copy constructor.
    /// @param[in] other The vector to copy from.
    Vector(const Vector& other)
        : m_allocator(Traits::selectOnContainerCopyConstruction(other.m_allocator))
    {
        if (other.m_size > 0)
        {
            m_data = Traits::allocate(m_allocator, other.m_size);
            m_capacity = other.m_size;
            _copyConstructRange(m_data, other.m_data, other.m_size);
            m_size = other.m_size;
        }
    }

    /// @brief Copy constructor with explicit allocator.
    /// @param[in] other The vector to copy from.
    /// @param[in] alloc The allocator instance to use.
    Vector(const Vector& other, const AllocatorType& alloc)
        : m_allocator(alloc)
    {
        if (other.m_size > 0)
        {
            m_data = Traits::allocate(m_allocator, other.m_size);
            m_capacity = other.m_size;
            _copyConstructRange(m_data, other.m_data, other.m_size);
            m_size = other.m_size;
        }
    }

    /// @brief Move constructor.
    /// @param[in] other The vector to move from.
    Vector(Vector&& other) noexcept
        : m_data(other.m_data)
        , m_size(other.m_size)
        , m_capacity(other.m_capacity)
        , m_allocator(static_cast<AllocatorType&&>(other.m_allocator))
    {
        other.m_data = nullptr;
        other.m_size = 0;
        other.m_capacity = 0;
    }

    /// @brief Move constructor with explicit allocator.
    /// @param[in] other The vector to move from.
    /// @param[in] alloc The allocator instance to use.
    Vector(Vector&& other, const AllocatorType& alloc) noexcept
        : m_allocator(alloc)
    {
        if (m_allocator == other.m_allocator)
        {
            m_data = other.m_data;
            m_size = other.m_size;
            m_capacity = other.m_capacity;
            other.m_data = nullptr;
            other.m_size = 0;
            other.m_capacity = 0;
        }
        else if (other.m_size > 0)
        {
            m_data = Traits::allocate(m_allocator, other.m_size);
            m_capacity = other.m_size;
            _moveConstructRange(m_data, other.m_data, other.m_size);
            m_size = other.m_size;
        }
    }

    /// @brief Destructor destroys elements and frees memory.
    ~Vector() { _destroyAndFree(); }

    /// @brief Copy assignment operator.
    /// @param[in] other The vector to copy from.
    Vector& operator=(const Vector& other)
    {
        if (this == &other) { return *this; }

        constexpr bool propagate = Traits::PropagateOnContainerCopyAssignment::value;

        if constexpr (propagate)
        {
            if (m_allocator != other.m_allocator)
            {
                _destroyAndFree();
                _replaceAllocator(other.m_allocator);
                m_data = nullptr;
                m_size = 0;
                m_capacity = 0;
            }
            else { _replaceAllocator(other.m_allocator); }
        }

        _assignFrom(other.m_data, other.m_size);
        return *this;
    }

    /// @brief Move assignment operator.
    /// @param[in] other The vector to move from.
    Vector& operator=(Vector&& other) noexcept
    {
        if (this == &other) { return *this; }

        constexpr bool propagate = Traits::PropagateOnContainerMoveAssignment::value;

        if constexpr (propagate)
        {
            _destroyAndFree();
            _replaceAllocator(static_cast<AllocatorType&&>(other.m_allocator));
            m_data = other.m_data;
            m_size = other.m_size;
            m_capacity = other.m_capacity;
            other.m_data = nullptr;
            other.m_size = 0;
            other.m_capacity = 0;
        }
        else
        {
            if (m_allocator == other.m_allocator)
            {
                _destroyAndFree();
                m_data = other.m_data;
                m_size = other.m_size;
                m_capacity = other.m_capacity;
                other.m_data = nullptr;
                other.m_size = 0;
                other.m_capacity = 0;
            }
            else { _assignFromMove(other.m_data, other.m_size); }
        }
        return *this;
    }

    /// @brief Initializer list assignment.
    /// @param[in] init The initializer list to assign from.
    Vector& operator=(std::initializer_list<T> init)
    {
        _assignFrom(init.begin(), init.size());
        return *this;
    }

public:
    /// @brief Unchecked element access.
    /// @param[in] pos The index of the element to access.
    /// @return Reference to the element at the given index.
    GP_NODISCARD GP_FORCEINLINE Reference operator[](SizeType pos) noexcept { return m_data[pos]; }

    /// @brief Unchecked element access (const).
    /// @param[in] pos The index of the element to access.
    /// @return Const reference to the element at the given index.
    GP_NODISCARD GP_FORCEINLINE ConstReference operator[](SizeType pos) const noexcept { return m_data[pos]; }

    /// @brief Equality comparison.
    /// @param[in] other The vector to compare with.
    /// @return True if the vectors are equal, false otherwise.
    GP_NODISCARD bool operator==(const Vector& other) const noexcept
    {
        if (m_size != other.m_size) { return false; }
        if constexpr (std::is_trivially_copyable_v<T>)
        {
            return m_size == 0 || std::memcmp(m_data, other.m_data, m_size * sizeof(T)) == 0;
        }
        else
        {
            for (SizeType i = 0; i < m_size; ++i)
            {
                if (!(m_data[i] == other.m_data[i])) { return false; }
            }
            return true;
        }
    }

    /// @brief Inequality comparison.
    /// @param[in] other The vector to compare with.
    /// @return True if the vectors are not equal, false otherwise.
    GP_NODISCARD bool operator!=(const Vector& other) const noexcept { return !(*this == other); }

public:
    /// @brief Replaces contents with count copies of value.
    /// @param[in] count Number of elements.
    /// @param[in] value The value to copy.
    void assign(SizeType count, const T& value)
    {
        _destroyRange(m_data, m_size);
        m_size = 0;
        if (count > m_capacity) { _reallocExact(count); }
        _fillConstructRange(m_data, count, value);
        m_size = count;
    }

    /// @brief Replaces contents with the range [first, last).
    /// @tparam InputIt The input iterator type.
    /// @param[in] first Iterator to the beginning of the range.
    /// @param[in] last Iterator to the end of the range.
    template <typename InputIt>
    requires(!std::is_integral_v<InputIt>) void assign(InputIt first, InputIt last)
    {
        _destroyRange(m_data, m_size);
        m_size = 0;
        _rangeInit(first, last);
    }

    /// @brief Replaces contents with an initializer list.
    /// @param[in] init The initializer list.
    void assign(std::initializer_list<T> init) { _assignFrom(init.begin(), init.size()); }

    /// @brief Bounds-checked element access.
    /// @param[in] pos The index of the element to access.
    /// @return Reference to the element at the given index.
    GP_NODISCARD Reference at(SizeType pos)
    {
        GP_ASSERT(pos < m_size);
        return m_data[pos];
    }

    /// @brief Bounds-checked element access (const).
    /// @param[in] pos The index of the element to access.
    /// @return Const reference to the element at the given index.
    GP_NODISCARD ConstReference at(SizeType pos) const
    {
        GP_ASSERT(pos < m_size);
        return m_data[pos];
    }

    /// @brief Returns a reference to the first element.
    /// @return Reference to the first element.
    GP_NODISCARD GP_FORCEINLINE Reference front() noexcept { return m_data[0]; }

    /// @brief Returns a reference to the first element (const).
    /// @return Const reference to the first element.
    GP_NODISCARD GP_FORCEINLINE ConstReference front() const noexcept { return m_data[0]; }

    /// @brief Returns a reference to the last element.
    /// @return Reference to the last element.
    GP_NODISCARD GP_FORCEINLINE Reference back() noexcept { return m_data[m_size - 1]; }

    /// @brief Returns a reference to the last element (const).
    /// @return Const reference to the last element.
    GP_NODISCARD GP_FORCEINLINE ConstReference back() const noexcept { return m_data[m_size - 1]; }

    /// @brief Returns a pointer to the underlying array.
    /// @return Pointer to the underlying array.
    GP_NODISCARD GP_FORCEINLINE Pointer data() noexcept { return m_data; }

    /// @brief Returns a pointer to the underlying array (const).
    /// @return Const pointer to the underlying array.
    GP_NODISCARD GP_FORCEINLINE ConstPointer data() const noexcept { return m_data; }

    /// @brief Returns an iterator to the beginning of the vector.
    /// @return Iterator to the beginning of the vector.
    GP_NODISCARD GP_FORCEINLINE Iterator begin() noexcept { return m_data; }

    /// @brief Returns an iterator to the beginning of the vector (const).
    /// @return Const iterator to the beginning of the vector.
    GP_NODISCARD GP_FORCEINLINE ConstIterator begin() const noexcept { return m_data; }

    /// @brief Returns an iterator to the end of the vector.
    /// @return Iterator to the end of the vector.
    GP_NODISCARD GP_FORCEINLINE Iterator end() noexcept { return m_data + m_size; }

    /// @brief Returns an iterator to the end of the vector (const).
    /// @return Const iterator to the end of the vector.
    GP_NODISCARD GP_FORCEINLINE ConstIterator end() const noexcept { return m_data + m_size; }

    /// @brief Returns a const iterator to the beginning of the vector.
    /// @return Const iterator to the beginning of the vector.
    GP_NODISCARD GP_FORCEINLINE ConstIterator cbegin() const noexcept { return m_data; }

    /// @brief Returns a const iterator to the end of the vector.
    /// @return Const iterator to the end of the vector.
    GP_NODISCARD GP_FORCEINLINE ConstIterator cend() const noexcept { return m_data + m_size; }

    /// @brief Returns a reverse iterator to the beginning of the reversed vector.
    /// @return Reverse iterator to the beginning of the reversed vector.
    GP_NODISCARD GP_FORCEINLINE ReverseIterator rbegin() noexcept { return ReverseIterator(end()); }

    /// @brief Returns a reverse iterator to the beginning of the reversed vector (const).
    /// @return Const reverse iterator to the beginning of the reversed vector.
    GP_NODISCARD GP_FORCEINLINE ConstReverseIterator rbegin() const noexcept { return ConstReverseIterator(end()); }

    /// @brief Returns a reverse iterator to the end of the reversed vector.
    /// @return Reverse iterator to the end of the reversed vector.
    GP_NODISCARD GP_FORCEINLINE ReverseIterator rend() noexcept { return ReverseIterator(begin()); }

    /// @brief Returns a reverse iterator to the end of the reversed vector (const).
    /// @return Const reverse iterator to the end of the reversed vector.
    GP_NODISCARD GP_FORCEINLINE ConstReverseIterator rend() const noexcept { return ConstReverseIterator(begin()); }

    /// @brief Returns a const reverse iterator to the beginning of the reversed vector.
    /// @return Const reverse iterator to the beginning of the reversed vector.
    GP_NODISCARD GP_FORCEINLINE ConstReverseIterator crbegin() const noexcept { return ConstReverseIterator(cend()); }

    /// @brief Returns a const reverse iterator to the end of the reversed vector.
    /// @return Const reverse iterator to the end of the reversed vector.
    GP_NODISCARD GP_FORCEINLINE ConstReverseIterator crend() const noexcept { return ConstReverseIterator(cbegin()); }

    /// @brief Returns the number of elements.
    /// @return The number of elements in the vector.
    GP_NODISCARD GP_FORCEINLINE SizeType size() const noexcept { return m_size; }

    /// @brief Checks if the vector is empty.
    /// @return True if the vector is empty, false otherwise.
    GP_NODISCARD GP_FORCEINLINE bool empty() const noexcept { return m_size == 0; }

    /// @brief Returns the current allocated capacity.
    /// @return The current allocated capacity.
    GP_NODISCARD GP_FORCEINLINE SizeType capacity() const noexcept { return m_capacity; }

    /// @brief Returns the maximum possible number of elements.
    /// @return The maximum possible number of elements.
    GP_NODISCARD constexpr SizeType maxSize() const noexcept { return Traits::maxSize(m_allocator); }

    /// @brief Reserves storage for at least newCap elements.
    /// @param[in] newCap Minimum desired capacity.
    void reserve(SizeType newCap)
    {
        if (newCap > m_capacity) { _reallocExact(newCap); }
    }

    /// @brief Releases unused capacity, reallocating to fit size().
    void shrinkToFit()
    {
        if (m_size == 0)
        {
            if (m_data)
            {
                Traits::deallocate(m_allocator, m_data, m_capacity);
                m_data = nullptr;
                m_capacity = 0;
            }
        }
        else if (m_size < m_capacity) { _reallocExact(m_size); }
    }

    /// @brief Erases all elements. Does not release memory.
    void clear() noexcept
    {
        _destroyRange(m_data, m_size);
        m_size = 0;
    }

    /// @brief Appends an element by copy.
    /// @param[in] value The value to append.
    void pushBack(const T& value)
    {
        if (m_size == m_capacity) { _grow(); }
        Traits::construct(m_allocator, m_data + m_size, value);
        ++m_size;
    }

    /// @brief Appends an element by move.
    /// @param[in] value The value to move-append.
    void pushBack(T&& value)
    {
        if (m_size == m_capacity) { _grow(); }
        Traits::construct(m_allocator, m_data + m_size, static_cast<T&&>(value));
        ++m_size;
    }

    /// @brief Constructs an element in-place at the end.
    /// @tparam Args Constructor argument types.
    /// @param[in] args Arguments forwarded to T's constructor.
    /// @return Reference to the newly constructed element.
    template <typename... Args>
    Reference emplaceBack(Args&&... args)
    {
        if (m_size == m_capacity) { _grow(); }
        Traits::construct(m_allocator, m_data + m_size, static_cast<Args&&>(args)...);
        return m_data[m_size++];
    }

    /// @brief Removes the last element.
    void popBack() noexcept
    {
        GP_ASSERT(m_size > 0);
        --m_size;
        Traits::destroy(m_allocator, m_data + m_size);
    }

    /// @brief Inserts an element at the given position by copy.
    /// @param[in] pos Iterator to the insertion point.
    /// @param[in] value The value to insert.
    /// @return Iterator to the inserted element.
    Iterator insert(ConstIterator pos, const T& value)
    {
        const SizeType idx = static_cast<SizeType>(pos - m_data);
        if (m_size == m_capacity) { _grow(); }
        _shiftRight(m_data + idx, m_size - idx);
        Traits::construct(m_allocator, m_data + idx, value);
        ++m_size;
        return m_data + idx;
    }

    /// @brief Inserts an element at the given position by move.
    /// @param[in] pos Iterator to the insertion point.
    /// @param[in] value The value to move-insert.
    /// @return Iterator to the inserted element.
    Iterator insert(ConstIterator pos, T&& value)
    {
        const SizeType idx = static_cast<SizeType>(pos - m_data);
        if (m_size == m_capacity) { _grow(); }
        _shiftRight(m_data + idx, m_size - idx);
        Traits::construct(m_allocator, m_data + idx, static_cast<T&&>(value));
        ++m_size;
        return m_data + idx;
    }

    /// @brief Inserts count copies of value at the given position.
    /// @param[in] pos Iterator to the insertion point.
    /// @param[in] count Number of copies to insert.
    /// @param[in] value The value to copy.
    /// @return Iterator to the first inserted element.
    Iterator insert(ConstIterator pos, SizeType count, const T& value)
    {
        if (count == 0) { return const_cast<Iterator>(pos); }
        const SizeType idx = static_cast<SizeType>(pos - m_data);
        _ensureCapacity(m_size + count);
        _shiftRight(m_data + idx, m_size - idx, count);
        for (SizeType i = 0; i < count; ++i) { Traits::construct(m_allocator, m_data + idx + i, value); }
        m_size += count;
        return m_data + idx;
    }

    /// @brief Inserts elements from an initializer list at the given position.
    /// @param[in] pos Iterator to the insertion point.
    /// @param[in] init The initializer list.
    /// @return Iterator to the first inserted element.
    Iterator insert(ConstIterator pos, std::initializer_list<T> init)
    {
        if (init.size() == 0) { return const_cast<Iterator>(pos); }
        const SizeType idx = static_cast<SizeType>(pos - m_data);
        const SizeType count = init.size();
        _ensureCapacity(m_size + count);
        _shiftRight(m_data + idx, m_size - idx, count);
        SizeType i = 0;
        for (auto& v: init)
        {
            Traits::construct(m_allocator, m_data + idx + i, v);
            ++i;
        }
        m_size += count;
        return m_data + idx;
    }

    /// @brief Constructs an element in-place at the given position.
    /// @tparam Args Constructor argument types.
    /// @param[in] pos Iterator to the insertion point.
    /// @param[in] args Arguments forwarded to T's constructor.
    /// @return Iterator to the emplaced element.
    template <typename... Args>
    Iterator emplace(ConstIterator pos, Args&&... args)
    {
        const SizeType idx = static_cast<SizeType>(pos - m_data);
        if (m_size == m_capacity) { _grow(); }
        _shiftRight(m_data + idx, m_size - idx);
        Traits::construct(m_allocator, m_data + idx, static_cast<Args&&>(args)...);
        ++m_size;
        return m_data + idx;
    }

    /// @brief Erases the element at the given position.
    /// @param[in] pos Iterator to the element to erase.
    /// @return Iterator to the element following the erased one.
    Iterator erase(ConstIterator pos)
    {
        const SizeType idx = static_cast<SizeType>(pos - m_data);
        Traits::destroy(m_allocator, m_data + idx);
        _shiftLeft(m_data + idx + 1, m_size - idx - 1);
        --m_size;
        return m_data + idx;
    }

    /// @brief Erases elements in the range [first, last).
    /// @param[in] first Iterator to the first element to erase.
    /// @param[in] last Iterator past the last element to erase.
    /// @return Iterator to the element following the last erased one.
    Iterator erase(ConstIterator first, ConstIterator last)
    {
        if (first == last) { return const_cast<Iterator>(first); }
        const SizeType idx = static_cast<SizeType>(first - m_data);
        const SizeType count = static_cast<SizeType>(last - first);
        _destroyRange(m_data + idx, count);
        _shiftLeft(m_data + idx + count, m_size - idx - count, count);
        m_size -= count;
        return m_data + idx;
    }

    /// @brief Resizes the vector to contain count elements.
    /// @param[in] count New size.
    void resize(SizeType count)
    {
        if (count < m_size)
        {
            _destroyRange(m_data + count, m_size - count);
            m_size = count;
        }
        else if (count > m_size)
        {
            _ensureCapacity(count);
            _defaultConstructRange(m_data + m_size, count - m_size);
            m_size = count;
        }
    }

    /// @brief Resizes the vector, filling new elements with value.
    /// @param[in] count New size.
    /// @param[in] value The fill value for new elements.
    void resize(SizeType count, const T& value)
    {
        if (count < m_size)
        {
            _destroyRange(m_data + count, m_size - count);
            m_size = count;
        }
        else if (count > m_size)
        {
            _ensureCapacity(count);
            _fillConstructRange(m_data + m_size, count - m_size, value);
            m_size = count;
        }
    }

    /// @brief Swaps contents with another vector.
    /// @param[in,out] other The vector to swap with.
    void swap(Vector& other) noexcept
    {
        constexpr bool propagate = Traits::PropagateOnContainerSwap::value;
        if constexpr (!propagate) { GP_ASSERT(m_allocator == other.m_allocator); }

        Pointer tmp_data = m_data;
        SizeType tmp_size = m_size;
        SizeType tmp_cap = m_capacity;
        m_data = other.m_data;
        m_size = other.m_size;
        m_capacity = other.m_capacity;
        other.m_data = tmp_data;
        other.m_size = tmp_size;
        other.m_capacity = tmp_cap;

        if constexpr (propagate)
        {
            AllocatorType tmpAlloc(m_allocator);
            _replaceAllocator(other.m_allocator);
            other._replaceAllocator(tmpAlloc);
        }
    }

    /// @brief Returns a copy of the allocator.
    /// @return A copy of the allocator used by the vector.
    GP_NODISCARD AllocatorType getAllocator() const noexcept { return m_allocator; }

private:
    /// @brief Replaces the allocator via destroy + placement new (works even if operator= is deleted).
    /// @param[in] newAlloc The new allocator to set.
    void _replaceAllocator(const AllocatorType& newAlloc) noexcept
    {
        m_allocator.~AllocatorType();
        ::new (static_cast<void*>(&m_allocator)) AllocatorType(newAlloc);
    }

    /// @brief Replaces the allocator from an rvalue via destroy + placement new.
    /// @param[in] newAlloc The new allocator to set.
    void _replaceAllocator(AllocatorType&& newAlloc) noexcept
    {
        m_allocator.~AllocatorType();
        ::new (static_cast<void*>(&m_allocator)) AllocatorType(static_cast<AllocatorType&&>(newAlloc));
    }

    /// @brief 1.5x growth: newCap = old_cap + (old_cap >> 1), minimum growth of 1.
    /// @return The next capacity to grow to.
    GP_NODISCARD GP_FORCEINLINE SizeType _nextCapacity() const noexcept
    {
        if (m_capacity == 0) { return 1; }
        const SizeType growth = m_capacity >> 1;
        return m_capacity + (growth > 0 ? growth : 1);
    }

    /// @brief Grows the backing store by the 1.5x growth factor.
    void _grow() { _reallocExact(_nextCapacity()); }

    /// @brief Ensures capacity is at least minCap, growing by 1.5x if needed.
    /// @param[in] minCap The minimum required capacity.
    void _ensureCapacity(SizeType minCap)
    {
        if (minCap <= m_capacity) { return; }
        SizeType newCap = _nextCapacity();
        while (newCap < minCap)
        {
            const SizeType growth = newCap >> 1;
            newCap = newCap + (growth > 0 ? growth : 1);
        }
        _reallocExact(newCap);
    }

    /// @brief Reallocates storage to exactly newCap, moving existing elements.
    /// @param[in] newCap The exact new capacity to allocate.
    void _reallocExact(SizeType newCap)
    {
        Pointer newData = Traits::allocate(m_allocator, newCap);
        if (m_size > 0) { _relocate(newData, m_data, m_size); }
        if (m_data) { Traits::deallocate(m_allocator, m_data, m_capacity); }
        m_data = newData;
        m_capacity = newCap;
    }

    /// @brief Relocates n elements from src to dst. Uses memcpy for trivial types.
    /// @param[in] dst The destination pointer to move elements to.
    /// @param[in] src The source pointer to move elements from.
    /// @param[in] n The number of elements to relocate.
    static void _relocate(Pointer dst, Pointer src, SizeType n)
    {
        if constexpr (std::is_trivially_copyable_v<T>) { std::memcpy(dst, src, n * sizeof(T)); }
        else
        {
            for (SizeType i = 0; i < n; ++i)
            {
                ::new (static_cast<void*>(dst + i)) T(static_cast<T&&>(src[i]));
                src[i].~T();
            }
        }
    }

    /// @brief Shifts elements right by 'count' positions for insertion.
    /// @param[in] pos The position to start shifting from.
    /// @param[in] n The number of elements to shift.
    /// @param[in] count The number of positions to shift (default is 1).
    void _shiftRight(Pointer pos, SizeType n, SizeType count = 1)
    {
        if (n == 0) { return; }
        if constexpr (std::is_trivially_copyable_v<T>) { std::memmove(pos + count, pos, n * sizeof(T)); }
        else
        {
            for (SizeType i = n; i > 0; --i)
            {
                ::new (static_cast<void*>(pos + i - 1 + count)) T(static_cast<T&&>(pos[i - 1]));
                pos[i - 1].~T();
            }
        }
    }

    /// @brief Shifts elements left by 'count' positions after erasure.
    /// @param[in] pos The position to start shifting from.
    /// @param[in] n The number of elements to shift.
    /// @param[in] count The number of positions to shift (default is 1).
    static void _shiftLeft(Pointer pos, SizeType n, SizeType count = 1)
    {
        if (n == 0) { return; }
        if constexpr (std::is_trivially_copyable_v<T>) { std::memmove(pos - count, pos, n * sizeof(T)); }
        else
        {
            for (SizeType i = 0; i < n; ++i)
            {
                ::new (static_cast<void*>(pos - count + i)) T(static_cast<T&&>(pos[i]));
                pos[i].~T();
            }
        }
    }

    /// @brief Destroys n elements starting at ptr.
    /// @param[in] ptr The pointer to the first element to destroy.
    /// @param[in] n The number of elements to destroy.
    void _destroyRange(Pointer ptr, SizeType n)
    {
        if constexpr (!std::is_trivially_destructible_v<T>)
        {
            for (SizeType i = 0; i < n; ++i) { Traits::destroy(m_allocator, ptr + i); }
        }
    }

    /// @brief Default-constructs n elements at ptr.
    /// @param[in] ptr The pointer to the first element to default-construct.
    /// @param[in] n The number of elements to default-construct.
    void _defaultConstructRange(Pointer ptr, SizeType n)
    {
        if constexpr (std::is_trivially_default_constructible_v<T>) { std::memset(ptr, 0, n * sizeof(T)); }
        else
        {
            for (SizeType i = 0; i < n; ++i) { Traits::construct(m_allocator, ptr + i); }
        }
    }

    /// @brief Copy-constructs n elements at dst from src.
    /// @param[in] dst The pointer to the first element to copy-construct.
    /// @param[in] src The pointer to the first element to copy from.
    /// @param[in] n The number of elements to copy-construct.
    void _copyConstructRange(Pointer dst, ConstPointer src, SizeType n)
    {
        if constexpr (std::is_trivially_copyable_v<T>) { std::memcpy(dst, src, n * sizeof(T)); }
        else
        {
            for (SizeType i = 0; i < n; ++i) { Traits::construct(m_allocator, dst + i, src[i]); }
        }
    }

    /// @brief Fill-constructs n elements at ptr with value.
    /// @param[in] ptr The pointer to the first element to fill-construct.
    /// @param[in] n The number of elements to fill-construct.
    /// @param[in] value The value to copy for fill-construction.
    void _fillConstructRange(Pointer ptr, SizeType n, const T& value)
    {
        for (SizeType i = 0; i < n; ++i) { Traits::construct(m_allocator, ptr + i, value); }
    }

    /// @brief Move-constructs n elements at dst from src.
    /// @param[in] dst The pointer to the first element to move-construct.
    /// @param[in] src The pointer to the first element to move from.
    /// @param[in] n The number of elements to move-construct.
    void _moveConstructRange(Pointer dst, Pointer src, SizeType n)
    {
        if constexpr (std::is_trivially_copyable_v<T>) { std::memcpy(dst, src, n * sizeof(T)); }
        else
        {
            for (SizeType i = 0; i < n; ++i) { Traits::construct(m_allocator, dst + i, static_cast<T&&>(src[i])); }
        }
    }

    /// @brief Destroys all elements and frees the backing store.
    void _destroyAndFree()
    {
        _destroyRange(m_data, m_size);
        if (m_data)
        {
            Traits::deallocate(m_allocator, m_data, m_capacity);
            m_data = nullptr;
        }
        m_size = 0;
        m_capacity = 0;
    }

    /// @brief Common path for copy-assignment and initializer_list assignment.
    /// @param[in] src The source pointer to copy from.
    /// @param[in] count The number of elements to copy.
    void _assignFrom(ConstPointer src, SizeType count)
    {
        _destroyRange(m_data, m_size);
        m_size = 0;
        if (count > m_capacity) { _reallocExact(count); }
        _copyConstructRange(m_data, src, count);
        m_size = count;
    }

    /// @brief Common path for move-assignment with unequal allocators.
    /// @param[in] src The source pointer to move from.
    /// @param[in] count The number of elements to move.
    void _assignFromMove(Pointer src, SizeType count)
    {
        _destroyRange(m_data, m_size);
        m_size = 0;
        if (count > m_capacity) { _reallocExact(count); }
        _moveConstructRange(m_data, src, count);
        m_size = count;
    }

    /// @brief Initializes from an iterator range, dispatching on iterator category.
    /// @tparam InputIt The input iterator type.
    /// @param[in] first Iterator to the beginning of the range.
    /// @param[in] last Iterator to the end of the range.
    template <typename InputIt>
    void _rangeInit(InputIt first, InputIt last)
    {
        if constexpr (std::is_base_of_v<
                          std::random_access_iterator_tag,
                          typename std::iterator_traits<InputIt>::iterator_category>)
        {
            const SizeType count = static_cast<SizeType>(last - first);
            if (count > 0)
            {
                m_data = Traits::allocate(m_allocator, count);
                m_capacity = count;
                if constexpr (std::is_trivially_copyable_v<T> && std::is_pointer_v<InputIt>)
                {
                    std::memcpy(m_data, first, count * sizeof(T));
                }
                else
                {
                    for (SizeType i = 0; i < count; ++i)
                    {
                        Traits::construct(m_allocator, m_data + i, *first);
                        ++first;
                    }
                }
                m_size = count;
            }
        }
        else
        {
            for (; first != last; ++first) { pushBack(*first); }
        }
    }
};

}   // namespace gp
