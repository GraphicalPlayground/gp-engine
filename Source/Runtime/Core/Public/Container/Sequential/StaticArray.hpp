// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Container/Utility/Iterator.hpp"
#include "Container/Utility/ReverseIterator.hpp"
#include "CoreTypes.hpp"
#include "Templates/Concepts/CoreConcepts.hpp"
#include "Templates/Concepts/IteratorConcepts.hpp"
#include "Templates/Concepts/ObjectConcepts.hpp"
#include "Templates/Core/InitializerList.hpp"
#include "Templates/Core/Utility.hpp"
#include <compare>
#include <cstring>

namespace GP
{

template <typename T, SizeT N>
class TStaticArray
{
private:
    static_assert(!CReference<T>, "TArray cannot store reference types");
    static_assert(CDestructible<T>, "TArray element type must be destructible");
    static_assert(
        CTriviallyCopyable<T> || TIsNothrowConstructible_V<T, T&&>,
        "TArray<T>: T must be trivially copyable or nothrow move-constructible; " "a throwing move during Reallocate corrupts the container."
    );

public:
    using ValueType = T;
    using Pointer = T*;
    using ConstPointer = const T*;
    using Reference = T&;
    using ConstReference = const T&;
    using SizeType = SizeT;
    using DiffType = SSizeT;
    using Iterator = T*;
    using ConstIterator = TConstIterator<T>;
    using ReverseIterator = TReverseIteratorT<T>;
    using ConstReverseIterator = TConstReverseIterator<T>;

public:
    /// @brief Compile-time element count.
    static constexpr SizeType ArraySize = N;

    /// @brief Compile-time byte size of the storage.
    static constexpr SizeType ByteSize = N * sizeof(T);

    /// @brief Sentinel value indicating not-found for index-based searches.
    static constexpr SizeType NPOS = static_cast<SizeType>(-1);

private:
    alignas(T) T m_data[N]{};   //<! Inline storage for N elements. Stack-allocated, zero heap overhead.

public:
    /// @brief default constructor. Value-initializes all N elements.
    constexpr TStaticArray() = default;

    /// @brief Fill constructor. Initializes all N elements to the given value.
    /// @param value The value to copy into every element.
    explicit constexpr TStaticArray(const T& value) noexcept
    {
        for (SizeType i = 0; i < N; ++i) { m_data[i] = value; }
    }

    /// @brief Fill constructor. Initializes all N elements to the given value.
    /// @param value The value to copy into every element.
    explicit constexpr TStaticArray(const T& value) noexcept requires CTriviallyCopyable<T>
    {
        for (SizeType i = 0; i < N; ++i) { m_data[i] = value; }
    }

    /// @brief Variadic element constructor. Accepts exactly N arguments.
    /// @param args The N values to initialize the array with.
    template <typename... Args>
    requires(sizeof...(Args) == N) && (CConstructibleFrom<T, Args> && ...)
    constexpr TStaticArray(Args&&... args) noexcept
        : m_data{ static_cast<T>(Forward<Args>(args))... }
    {}

    /// @brief Iterator range constructor. The range must contain exactly N elements.
    /// @tparam InputIt An input iterator type whose dereferenced value is convertible to T.
    /// @param first Iterator to the beginning of the range.
    /// @param last Iterator to the end of the range.
    template <CInputIterator InputIt>
    constexpr TStaticArray(InputIt first, InputIt last)
    {
        SizeType i = 0;
        for (; first != last && i < N; ++first, ++i) { m_data[i] = *first; }
        GP_ASSERT(i == N && first == last, "TStaticArray iterator range size must match N");
    }

    /// @brief Copy constructor.
    /// @param other The static array to copy from.
    constexpr TStaticArray(const TStaticArray& other) noexcept
    {
        if constexpr (CTriviallyCopyable<T>) { ::memcpy(m_data, other.m_data, ByteSize); }
        else
        {
            for (SizeType i = 0; i < N; ++i) { m_data[i] = other.m_data[i]; }
        }
    }

    /// @brief Move constructor.
    /// @param other The static array to move from. Elements are left in a valid but unspecified state.
    constexpr TStaticArray(TStaticArray&& other) noexcept
    {
        if constexpr (CTriviallyCopyable<T>) { ::memcpy(m_data, other.m_data, ByteSize); }
        else
        {
            for (SizeType i = 0; i < N; ++i) { m_data[i] = Move(other.m_data[i]); }
        }
    }

    /// @brief Destructor. Destroys all elements. No-op for trivially destructible types.
    ~TStaticArray() noexcept
    {
        if constexpr (!CTriviallyDestructible<T>)
        {
            for (SizeType i = 0; i < N; ++i) { m_data[i].~T(); }
        }
    }

public:
    /// @brief Copy assignment operator.
    /// @param other The static array to copy from.
    /// @return A reference to this array after the assignment.
    constexpr TStaticArray& operator=(const TStaticArray& other) noexcept
    {
        if (this != &other)
        {
            if constexpr (CTriviallyCopyable<T>) { ::memcpy(m_data, other.m_data, ByteSize); }
            else
            {
                for (SizeType i = 0; i < N; ++i) { m_data[i] = other.m_data[i]; }
            }
        }
        return *this;
    }

    /// @brief Move assignment operator.
    /// @param other The static array to move from. Elements are left in a valid but unspecified state.
    /// @return A reference to this array after the assignment.
    constexpr TStaticArray& operator=(TStaticArray&& other) noexcept
    {
        if (this != &other)
        {
            if constexpr (CTriviallyCopyable<T>) { ::memcpy(m_data, other.m_data, ByteSize); }
            else
            {
                for (SizeType i = 0; i < N; ++i) { m_data[i] = Move(other.m_data[i]); }
            }
        }
        return *this;
    }

    /// @brief Subscript operator for non-const access. Bounds-checked in debug builds.
    /// @param index The index of the element to access. Must be less than N.
    /// @return A reference to the element at the specified index.
    GP_NODISCARD constexpr Reference operator[](SizeType index) noexcept
    {
        GP_ASSERT(index < N, "TStaticArray index out of bounds");
        return m_data[index];
    }

    /// @brief Subscript operator for const access. Bounds-checked in debug builds.
    /// @param index The index of the element to access. Must be less than N.
    /// @return A const reference to the element at the specified index.
    GP_NODISCARD constexpr ConstReference operator[](SizeType index) const noexcept
    {
        GP_ASSERT(index < N, "TStaticArray index out of bounds");
        return m_data[index];
    }

    /// @brief Equality comparison. Compares all N elements pairwise.
    /// @param other The array to compare with.
    /// @return true if all elements are equal, false otherwise.
    GP_NODISCARD bool operator==(const TStaticArray& other) const noexcept requires CEqualityComparable<T>
    {
        for (SizeType i = 0; i < N; ++i)
        {
            if (m_data[i] != other.m_data[i]) { return false; }
        }
        return true;
    }

    /// @brief Inequality comparison with another array.
    /// @param other Array to compare against.
    /// @return true if arrays differ.
    GP_NODISCARD bool operator!=(const TStaticArray& other) const noexcept requires CEqualityComparable<T>
    {
        return !(*this == other);
    }

    /// @brief Three-way lexicographical comparison.
    /// @param other The array to compare with.
    /// @return Ordering result of the lexicographical comparison.
    GP_NODISCARD constexpr auto operator<=>(const TStaticArray& other) const noexcept requires CThreeWayComparable<T>
    {
        for (SizeType i = 0; i < N; ++i)
        {
            if (auto cmp = m_data[i] <=> other.m_data[i]; cmp != 0) { return cmp; }
        }
        return decltype(m_data[0] <=> other.m_data[0])::equivalent;
    }

public:
    /// @brief Bounds-checked element access.
    /// @param index The index of the element to access. Must be less than N.
    /// @return A reference to the element at the specified index.
    GP_NODISCARD constexpr Reference At(SizeType index)
    {
        GP_ASSERT(index < N, "TStaticArray::At index out of bounds");
        return m_data[index];
    }

    /// @brief Bounds-checked element access (const).
    /// @param index The index of the element to access. Must be less than N.
    /// @return A const reference to the element at the specified index.
    GP_NODISCARD constexpr ConstReference At(SizeType index) const
    {
        GP_ASSERT(index < N, "TStaticArray::At index out of bounds");
        return m_data[index];
    }

    /// @brief Access the first element.
    /// @return A reference to the first element.
    GP_NODISCARD constexpr Reference Front() noexcept { return m_data[0]; }

    /// @brief Access the first element (const).
    /// @return A const reference to the first element.
    GP_NODISCARD constexpr ConstReference Front() const noexcept { return m_data[0]; }

    /// @brief Access the last element.
    /// @return A reference to the last element.
    GP_NODISCARD constexpr Reference Back() noexcept { return m_data[N - 1]; }

    /// @brief Access the last element (const).
    /// @return A reference to the last element.
    GP_NODISCARD constexpr ConstReference Back() const noexcept { return m_data[N - 1]; }

    /// @brief Direct pointer to the underlying contiguous storage.
    /// @return A pointer to the first element.
    GP_NODISCARD constexpr Pointer Data() noexcept { return N != 0 ? m_data : nullptr; }

    /// @brief Direct pointer to the underlying contiguous storage (const).
    /// @return A pointer to the first element.
    GP_NODISCARD constexpr ConstPointer Data() const noexcept { return N != 0 ? m_data : nullptr; }

    /// @brief Returns the number of elements. Always N.
    /// @return The compile-time size of the array.
    GP_NODISCARD constexpr SizeType Size() const noexcept { return N; }

    /// @brief Returns the number of elements. Always N.
    /// @return The compile-time size of the array.
    GP_NODISCARD constexpr SizeType MaxSize() const noexcept { return N; }

    /// @brief Returns the number of bytes used by the array. Always N * sizeof(T).
    /// @return The compile-time byte size of the array.
    GP_NODISCARD constexpr SizeType Sizebytes() const noexcept { return ByteSize; }

    /// @brief Checks if the array is empty. Always false for N > 0.
    /// @return true if N == 0, false otherwise.
    GP_NODISCARD constexpr bool IsEmpty() const noexcept { return N == 0; }

    /// @brief Sets all elements to the given value.
    /// @param value The value to fill every element with.
    constexpr void Fill(const T& value) noexcept
    {
        for (SizeType i = 0; i < N; ++i) { m_data[i] = value; }
    }

    /// @brief swaps the contents of given array with another
    /// @param other other TStaticArray whos conent is the subject of swapping
    constexpr void Swap(TStaticArray& other) noexcept
    {
        // Bulk swap via temporary buffer for trivial objects. as no special constraints, swapping is done much faster.
        if constexpr (CTriviallyCopyable<T>)
        {
            GP_ALIGN(T) Byte temp[ByteSize];
            ::memcpy(temp, m_data, ByteSize);
            ::memcpy(m_data, other.m_data, ByteSize);
            ::memcpy(other.m_data, temp, ByteSize);
        }
        // swaps individual elements for non-trivial types, as they may have specific move semantics that must be
        // respected.
        else
        {
            for (SizeType i = 0; i < N; ++i)
            {
                T temp = Move(m_data[i]);
                m_data[i] = Move(other.m_data[i]);
                other.m_data[i] = Move(temp);
            }
        }
    }

    /// @brief Zero-fills the raw storage. Only valid for trivially copyable types (scalars, POD structs).
    ///        Useful for resetting vertex buffers, color arrays, or numeric accumulators to zero.
    constexpr void ZeroMemory() noexcept requires CTriviallyCopyable<T> { ::memset(m_data, 0, ByteSize); }

public:
    /// @brief Finds the first index pointer of the given value, or NPOS if not found.
    /// @param value The value to search for.
    /// @return The index of the first occurrence of value, or NPOS if not found.
    GP_NODISCARD constexpr Iterator Find(const T& value) noexcept
    {
        for (SizeType i = 0; i < N; ++i)
        {
            if (m_data[i] == value) { return m_data + i; }
        }
        return End();
    }

    /// @brief Finds the first index pointer of the given value, or NPOS if not found (const).
    /// @param value The value to search for.
    /// @return The index of the first occurrence of value, or NPOS if not found.
    GP_NODISCARD constexpr ConstIterator Find(const T& value) const noexcept
    {
        for (SizeType i = 0; i < N; ++i)
        {
            if (m_data[i] == value) { return ConstIterator(m_data + i); }
        }
        return CEnd();
    }

    /// @brief Checks whether the given value exists in the array.
    /// @param value The value to search for.
    /// @return true if the value is found, false otherwise.
    GP_NODISCARD constexpr bool Contains(const T& value) const noexcept { return Find(value) != CEnd(); }

    /// @brief Returns the index of the first occurrence of the specified value.
    /// @param value The value to search for.
    /// @return The index of the element, or NPOS if not found.
    GP_NODISCARD constexpr SizeType IndexOf(const T& value) const noexcept
    {
        for (SizeType i = 0; i < N; ++i)
        {
            if (m_data[i] == value) { return i; };
        }
        return NPOS;
    }

    /// @brief Returns the index of the first occurrence of the specified value (const).
    /// @param value The value to search for.
    /// @return The index of the element, or NPOS if not found.
    GP_NODISCARD constexpr SizeType IndexLastOf(const T& value) const noexcept
    {
        for (SizeType i = N; N > 0; --i)
        {
            if (m_data[i] == value) { return i - 1; };
        }
        return NPOS;
    }

    /// \brief Returns an iterator to the first element.
    /// \return An iterator pointing to the first element.
    GP_NODISCARD constexpr Iterator Begin() noexcept { return m_data; }

    /// \brief Returns an iterator past the last element.
    /// \return An iterator pointing past the last element.
    GP_NODISCARD constexpr Iterator End() noexcept { return m_data + N; }

    /// \brief Returns a const iterator to the first element.
    /// \return A const iterator pointing to the first element.
    GP_NODISCARD constexpr ConstIterator Begin() const noexcept { return ConstIterator(m_data); }

    /// \brief Returns a const iterator past the last element.
    /// \return A const iterator pointing past the last element.
    GP_NODISCARD constexpr ConstIterator End() const noexcept { return ConstIterator(m_data + N); }

    /// \brief Returns a const iterator to the first element.
    /// \return A const iterator pointing to the first element.
    GP_NODISCARD constexpr ConstIterator CBegin() const noexcept { return ConstIterator(m_data); }

    /// \brief Returns a const iterator past the last element.
    /// \return A const iterator pointing past the last element.
    GP_NODISCARD constexpr ConstIterator CEnd() const noexcept { return ConstIterator(m_data + N); }

    /// \brief Returns a reverse iterator to the last element.
    /// \return A reverse iterator pointing to the last element.
    GP_NODISCARD constexpr ReverseIterator RBegin() noexcept { return ReverseIterator(End()); }

    /// \brief Returns a reverse iterator before the first element.
    /// \return A reverse iterator pointing before the first element.
    GP_NODISCARD constexpr ReverseIterator REnd() noexcept { return ReverseIterator(Begin()); }

    /// \brief Returns a const reverse iterator to the last element.
    /// \return A const reverse iterator pointing to the last element.
    GP_NODISCARD constexpr ConstReverseIterator CRBegin() const noexcept { return ConstReverseIterator(CEnd()); }

    /// \brief Returns a const reverse iterator before the first element.
    /// \return A const reverse iterator pointing before the first element.
    GP_NODISCARD constexpr ConstReverseIterator CREnd() const noexcept { return ConstReverseIterator(CBegin()); }

    /// \brief Returns an iterator to the first element (STL compatibility).
    /// \return An iterator pointing to the first element.
    GP_NODISCARD constexpr Iterator begin() noexcept { return m_data; }

    /// \brief Returns an iterator past the last element (STL compatibility).
    /// \return An iterator pointing past the last element.
    GP_NODISCARD constexpr Iterator end() noexcept { return m_data + N; }

    /// \brief Returns a const iterator to the first element (STL compatibility).
    /// \return A const iterator pointing to the first element.
    GP_NODISCARD constexpr ConstIterator begin() const noexcept { return ConstIterator(m_data); }

    /// \brief Returns a const iterator past the last element (STL compatibility).
    /// \return A const iterator pointing past the last element.
    GP_NODISCARD constexpr ConstIterator end() const noexcept { return ConstIterator(m_data + N); }

    /// \brief Returns a const iterator to the first element (STL compatibility).
    /// \return A const iterator pointing to the first element.
    GP_NODISCARD constexpr ConstIterator cbegin() const noexcept { return ConstIterator(m_data); }

    /// \brief Returns a const iterator past the last element (STL compatibility).
    /// \return A const iterator pointing past the last element.
    GP_NODISCARD constexpr ConstIterator cend() const noexcept { return ConstIterator(m_data + N); }

    /// \brief Returns a reverse iterator to the last element (STL compatibility).
    /// \return A reverse iterator pointing to the last element.
    GP_NODISCARD constexpr ReverseIterator rbegin() noexcept { return ReverseIterator(end()); }

    /// \brief Returns a reverse iterator before the first element (STL compatibility).
    /// \return A reverse iterator pointing before the first element.
    GP_NODISCARD constexpr ReverseIterator rend() noexcept { return ReverseIterator(begin()); }

    /// \brief Returns a const reverse iterator to the last element (STL compatibility).
    /// \return A const reverse iterator pointing to the last element.
    GP_NODISCARD constexpr ConstReverseIterator crbegin() const noexcept { return ConstReverseIterator(cend()); }

    /// \brief Returns a const reverse iterator before the first element (STL compatibility).
    /// \return A const reverse iterator pointing before the first element.
    GP_NODISCARD constexpr ConstReverseIterator crend() const noexcept { return ConstReverseIterator(cbegin()); }
};

}   // namespace GP
