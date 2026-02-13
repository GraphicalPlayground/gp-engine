// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include "Crypto/Hash/FNV1a.hpp"
#include "Crypto/Hash/HashCombine.hpp"
#include "Crypto/Hash/xxHash.hpp"
#include "Templates/Concepts.hpp"
#include "Templates/TemplateUtilities.hpp"
#include <algorithm>
#include <compare>
#include <cstddef>
#include <cstring>
#include <iterator>
#include <utility>

namespace GP::Container
{

/// \brief A constant representing a dynamic extent for spans. When used as the `Extent` template parameter in `TSpan`,
/// it indicates that the span can have any size and is not fixed at compile time.
static inline constexpr SizeT DynamicExtent = static_cast<SizeT>(-1);

/// \brief A non-owning view over a contiguous sequence of objects. TSpan is the array equivalent of TBasicStringView.
/// It provides safe, bounds-checked access to a contiguous range of elements without ownership.
///
/// **When to use**:
///   - Function parameters: use TSpan<const T> instead of const T* + count
///   - Returning sub-ranges without copying
///   - Interfacing between different container types
///
/// \note Spans are the modern C++ replacement for "pointer + size" function parameters. They prevent buffer overflows
/// and make intent clear. std::span (C++20) is the standard version; TSpan mirrors it with engine conventions.
/// \tparam T Element type.
/// \tparam Extent Compile-time extent, or DynamicExtent for runtime-sized spans.
template <typename T, SizeT Extent = DynamicExtent>
class TSpan
{
public:
    using ElementType = T;
    using ValueType = std::remove_cv_t<T>;
    using Pointer = T*;
    using ConstPointer = const T*;
    using Reference = T&;
    using ConstReference = const T&;
    using SizeType = SizeT;
    using DifferenceType = OffsetT;
    using Iterator = Pointer;
    using ConstIterator = ConstPointer;
    using ReverseIterator = std::reverse_iterator<Iterator>;
    using ConstReverseIterator = std::reverse_iterator<ConstIterator>;

public:
    static constexpr SizeType ExtentValue = Extent;

private:
    Pointer m_data;    //<! Pointer to the first element in the span.
    SizeType m_size;   //<! Number of elements in the span.

public:
    /// \brief Default constructor. Creates an empty span with a null data pointer and zero size. Only allowed if the
    /// extent is
    constexpr TSpan() noexcept requires(Extent == DynamicExtent || Extent == 0)
        : m_data(nullptr)
        , m_size(0)
    {}

    /// \brief Constructor from pointer and size. Creates a span that views `count` elements starting from `data`.
    /// \param data Pointer to the first element in the span.
    /// \param count Number of elements in the span.
    constexpr TSpan(Pointer data, SizeType count) noexcept
        : m_data(data)
        , m_size(count)
    {
        if constexpr (Extent != DynamicExtent) { GP_ASSERT(count == Extent, "Static span size mismatch"); }
    }

    /// \brief Constructor from two pointers. Creates a span that views the elements in the range [first, last).
    /// \param first Pointer to the first element in the span.
    /// \param last Pointer to one past the last element in the span.
    constexpr TSpan(Pointer first, Pointer last) noexcept
        : m_data(first)
        , m_size(static_cast<SizeType>(last - first))
    {}

    /// \brief Constructor from a C-style array. Creates a span that views all elements of the array. Only allowed if
    /// the extent is dynamic or matches the size of the array.
    /// \param arr A reference to a C-style array of elements.
    template <SizeT N>
    constexpr TSpan(T (&arr)[N]) noexcept requires(Extent == DynamicExtent || Extent == N)
        : m_data(arr)
        , m_size(N)
    {}

    /// \brief Constructor from an STL container. Creates a span that views all elements of the container. Only allowed
    /// if the extent is dynamic or matches the size of the container.
    /// \param container A reference to an STL container (e.g., std::vector, std::array).
    template <Concepts::IsSTLContainer U>
    constexpr TSpan(U& container) noexcept requires(Extent == DynamicExtent || Extent == container.size())
        : m_data(container.data())
        , m_size(static_cast<SizeType>(container.size()))
    {}

    /// \brief Constructor from a const STL container. Creates a span that views all elements of the container. Only
    /// allowed if the extent is dynamic or matches the size of the container.
    /// \param container A reference to a const STL container (e.g., const std::vector, const std::array).
    template <Concepts::IsSTLContainer U>
    constexpr TSpan(const U& container) noexcept
        requires((Extent == DynamicExtent || Extent == container.size()) && Concepts::IsConst<T>)
        : m_data(container.data())
        , m_size(static_cast<SizeType>(container.size()))
    {}

    /// \brief Constructor from an GP container. Creates a span that views all elements of the container. Only allowed
    /// if the extent is dynamic or matches the size of the container.
    /// \param container A reference to an GP container (e.g., TArray, TStaticArray).
    template <Concepts::IsContainer U>
    constexpr TSpan(U& container) noexcept requires(Extent == DynamicExtent || Extent == container.Size())
        : m_data(container.Data())
        , m_size(static_cast<SizeType>(container.Size()))
    {}

    /// \brief Constructor from a const GP container. Creates a span that views all elements of the container. Only
    /// allowed if the extent is dynamic or matches the size of the container.
    /// \param container A reference to a const GP container (e.g., const TArray, const TStaticArray).
    template <Concepts::IsContainer U>
    constexpr TSpan(const U& container) noexcept
        requires((Extent == DynamicExtent || Extent == container.Size()) && Concepts::IsConst<T>)
        : m_data(container.Data())
        , m_size(static_cast<SizeType>(container.Size()))
    {}

    /// \brief Conversion from TSpan<T> to TSpan<const T>.
    /// \param other The span to convert from. Only allowed if T is const or the extent matches.
    template <SizeT N>
    constexpr TSpan(const TSpan<Template::RemoveConst<T>, N>& other) noexcept
        requires Concepts::IsConst<T> && (Extent == DynamicExtent || Extent == N)
        : m_data(other.Data())
        , m_size(other.Size())
    {}

    /// \brief Copy constructor. Creates a span that views the same data as another span.
    constexpr TSpan(const TSpan&) noexcept = default;

public:
    /// \brief Copy assignment operator. Assigns the span to view the same data as another span.
    constexpr TSpan& operator=(const TSpan&) noexcept = default;

    /// \brief Accesses the element at the specified index. Performs bounds checking in debug builds.
    /// \param index The index of the element to access.
    /// \return A reference to the element at the specified index.
    GP_NODISCARD constexpr Reference operator[](SizeType index) const noexcept
    {
        GP_ASSERT(index < m_size, "Span index out of bounds");
        return m_data[index];
    }

    /// \brief Equality comparison operator. Compares two spans for equality by checking if they have the same size and
    /// if all corresponding elements are equal. Only allowed if the extents match.
    /// \param other The span to compare with. Only allowed if the extent matches.
    /// \return `true` if the spans are equal (same size and all elements are equal), `false` otherwise.
    template <SizeT N>
    GP_NODISCARD constexpr bool operator==(TSpan<T, N> other) const noexcept
    {
        return m_size == other.Size() && std::equal(begin(), end(), other.begin());
    }

    /// \brief Lexicographical comparison, compares two spans element by element. Only allowed if the extents match and
    /// the element type supports three-way comparison.
    /// \param other The span to compare with. Only allowed if the extent matches and the element type supports
    /// three-way comparison.
    /// \return A value less than, equal to, or greater than zero depending on whether this span is less than, equal to,
    /// or greater than the other span.
    template <SizeT N>
    GP_NODISCARD constexpr auto operator<=>(TSpan<T, N> other) const noexcept requires Concepts::IsThreeWayComparable<T>
    {
        return std::lexicographical_compare_three_way(begin(), end(), other.begin(), other.end());
    }

public:
    /// \brief Accesses the element at the specified index with bounds checking.
    /// \param index The index of the element to access.
    /// \return A reference to the element at the specified index.
    GP_NODISCARD constexpr Reference At(SizeType index) const
    {
        GP_ASSERT(index < m_size, "Span index out of bounds");
        return m_data[index];
    }

    /// \brief Accesses the first element in the span. Performs bounds checking in debug builds.
    /// \return A reference to the first element in the span.
    GP_NODISCARD constexpr Reference Front() const noexcept
    {
        GP_ASSERT(m_size > 0, "Span is empty");
        return m_data[0];
    }

    /// \brief Accesses the last element in the span. Performs bounds checking in debug builds.
    /// \return A reference to the last element in the span.
    GP_NODISCARD constexpr Reference Back() const noexcept
    {
        GP_ASSERT(m_size > 0, "Span is empty");
        return m_data[m_size - 1];
    }

    /// \brief Returns a pointer to the underlying data of the span.
    /// \return A pointer to the first element in the span.
    GP_NODISCARD constexpr Pointer Data() const noexcept { return m_data; }

    /// \brief Returns the number of elements in the span.
    /// \return The size of the span in terms of the number of elements.
    GP_NODISCARD constexpr SizeType Size() const noexcept { return m_size; }

    /// \brief Returns the size of the span in bytes. Calculated as the number of elements multiplied by the size of
    /// each element.
    /// \return The size of the span in bytes.
    GP_NODISCARD constexpr SizeType SizeBytes() const noexcept { return m_size * sizeof(T); }

    /// \brief Checks if the span is empty (i.e., has zero size).
    /// \return `true` if the span is empty, `false` otherwise.
    GP_NODISCARD constexpr bool IsEmpty() const noexcept { return m_size == 0; }

    /// \brief Returns a subspan that represents the first `count` elements of the span. Performs bounds checking in
    /// debug builds.
    /// \param count The number of elements to include in the subspan.
    /// \return A new span that views the first `count` elements of the original span
    GP_NODISCARD constexpr TSpan<T, DynamicExtent> First(SizeType count) const noexcept
    {
        GP_ASSERT(count <= m_size, "First count exceeds span size");
        return { m_data, count };
    }

    /// \brief Returns a subspan of the first Count elements (compile-time sized)
    /// \tparam Count The number of elements to include in the subspan. Must be less than or equal to the size of the
    /// original span. Only allowed if the extent is dynamic or the specified count matches the static
    /// extent.
    /// \return A new span that views the first `Count` elements of the original span.
    template <SizeType Count>
    GP_NODISCARD constexpr TSpan<T, Count> First() const noexcept
    {
        static_assert(Count <= Extent || Extent == DynamicExtent, "Count exceeds static extent");
        GP_ASSERT(Count <= m_size, "First count exceeds span size");
        return { m_data, Count };
    }

    /// \brief Returns a subspan that represents the last `count` elements of the span. Performs bounds checking in
    /// debug builds.
    /// \param count The number of elements to include in the subspan.
    GP_NODISCARD constexpr TSpan<T, DynamicExtent> Last(SizeType count) const noexcept
    {
        GP_ASSERT(count <= m_size, "Last count exceeds span size");
        return { m_data + (m_size - count), count };
    }

    /// \brief Returns a subspan of the last Count elements (compile-time sized)
    /// \tparam Count The number of elements to include in the subspan. Must be less than or equal to the size of the
    /// original span. Only allowed if the extent is dynamic or the specified count matches the static extent.
    /// \return A new span that views the last `Count` elements of the original span.
    template <SizeType Count>
    GP_NODISCARD constexpr TSpan<T, Count> Last() const noexcept
    {
        static_assert(Count <= Extent || Extent == DynamicExtent, "Count exceeds static extent");
        GP_ASSERT(Count <= m_size, "Last count exceeds span size");
        return { m_data + (m_size - Count), Count };
    }

    /// \brief Returns a subspan that represents a contiguous sequence of elements starting from the specified offset.
    /// The subspan includes `count` elements, or all remaining elements if `count` is set to `DynamicExtent`. Performs
    /// bounds checking in debug builds.
    /// \param offset The index of the first element in the subspan relative to the original span.
    /// \param count The number of elements to include in the subspan, or `DynamicExtent` to include all remaining
    /// elements.
    /// \return A new span that views the specified subrange of the original span.
    GP_NODISCARD constexpr TSpan<T, DynamicExtent>
        Subspan(SizeType offset, SizeType count = DynamicExtent) const noexcept
    {
        GP_ASSERT(offset <= m_size, "Subspan offset exceeds span size");
        SizeType actualCount = (count == DynamicExtent) ? (m_size - offset) : count;
        GP_ASSERT(offset + actualCount <= m_size, "Subspan exceeds span bounds");
        return { m_data + offset, actualCount };
    }

    /// \brief Returns a subspan of Count elements starting from Offset (compile-time sized)
    /// \tparam Offset The index of the first element in the subspan relative to the original span. Must be less than or
    /// equal to the size of the original span.
    /// \tparam Count The number of elements to include in the subspan, or `DynamicExtent` to include all remaining
    /// elements. Only allowed if the extent is dynamic or the specified count matches the static extent.
    /// \return A new span that views the specified subrange of the original span.
    template <SizeType Offset, SizeType Count = DynamicExtent>
    GP_NODISCARD constexpr auto Subspan() const noexcept
    {
        static_assert(Offset <= Extent || Extent == DynamicExtent, "Offset exceeds static extent");
        constexpr SizeType SubExtent = (Count != DynamicExtent)    ? Count
                                       : (Extent != DynamicExtent) ? (Extent - Offset)
                                                                   : DynamicExtent;
        GP_ASSERT(Offset <= m_size, "Subspan offset exceeds span size");
        SizeType actualCount = (Count == DynamicExtent) ? (m_size - Offset) : Count;
        GP_ASSERT(Offset + actualCount <= m_size, "Subspan exceeds span bounds");
        return TSpan<T, SubExtent>{ m_data + Offset, actualCount };
    }

    /// \brief Returns a read-only byte view of the span's data
    /// \return A new span that views the same data as bytes.
    GP_NODISCARD constexpr TSpan<const Byte, (Extent == DynamicExtent) ? DynamicExtent : (Extent * sizeof(T))>
        AsBytes() const noexcept
    {
        return { reinterpret_cast<const Byte*>(m_data), SizeBytes() };
    }

    /// \brief Returns a writable byte view of the span's data (only for non-const T)
    /// \return A new span that views the same data as bytes.
    GP_NODISCARD constexpr TSpan<Byte, (Extent == DynamicExtent) ? DynamicExtent : (Extent * sizeof(T))>
        AsWritableBytes() const noexcept requires(!Concepts::IsConst<T>)
    {
        return { reinterpret_cast<Byte*>(m_data), SizeBytes() };
    }

    /// \brief Reinterprets the span as a different type. Essential for GPU buffer manipulation.
    /// \tparam U The target type. Must have compatible size and alignment.
    /// \return A new span that views the same data as type U.
    template <typename U>
    GP_NODISCARD constexpr TSpan<U, DynamicExtent> ReinterpretAs() const noexcept
    {
        static_assert(Concepts::IsTriviallyCopyable<U>, "Target type must be trivially copyable");
        GP_ASSERT(SizeBytes() % sizeof(U) == 0, "Span size not divisible by target type size");
        GP_ASSERT(reinterpret_cast<UIntPtr>(m_data) % alignof(U) == 0, "Misaligned data for target type");
        return { reinterpret_cast<U*>(m_data), SizeBytes() / sizeof(U) };
    }

    /// \brief Fills the span with the specified value. Only allowed for non-const element types.
    /// \param value The value to fill the span with.
    constexpr void Fill(const ValueType& value) const noexcept requires(!Concepts::IsConst<T>)
    {
        std::fill(begin(), end(), value);
    }

    /// \brief Copies data from another span. Performs bounds checking.
    /// \param source The span to copy from. Must not exceed the size of this span. Only allowed for non-const element
    /// types.
    constexpr void CopyFrom(TSpan<const T, DynamicExtent> source) const noexcept requires(!Concepts::IsConst<T>)
    {
        GP_ASSERT(source.Size() <= m_size, "Source span larger than destination");
        std::copy(source.begin(), source.end(), begin());
    }

    /// \brief Zero-initializes the memory. Critical for security and determinism.
    constexpr void ZeroMemory() const noexcept requires(!Concepts::IsConst<T> && Concepts::IsTriviallyCopyable<T>)
    {
        std::memset(m_data, 0, SizeBytes());
    }

    /// \brief Checks if the span's data is aligned to the specified boundary
    /// \tparam Alignment The alignment boundary in bytes. Must be a power of two.
    /// \return `true` if the data pointer is aligned to the specified boundary, `false` otherwise.
    template <SizeType Alignment>
    GP_NODISCARD constexpr bool IsAligned() const noexcept
    {
        return (reinterpret_cast<UIntPtr>(m_data) % Alignment) == 0;
    }

    /// \brief Returns the alignment of the span's data pointer
    /// \return The alignment of the data pointer in bytes.
    GP_NODISCARD constexpr SizeType GetAlignment() const noexcept
    {
        auto addr = reinterpret_cast<UIntPtr>(m_data);
        return addr & ~(addr - 1);
    }

    /// \brief Splits the span into chunks of the specified size.
    /// \param chunkIndex The index of the chunk to retrieve. Must be less than the total number of chunks.
    /// \param chunkSize The size of each chunk in terms of the number of elements. Must be greater than zero.
    /// \return A new span that views the same data as an array of chunks.
    GP_NODISCARD constexpr TSpan<T, DynamicExtent> GetChunk(SizeType chunkIndex, SizeType chunkSize) const noexcept
    {
        SizeType offset = chunkIndex * chunkSize;
        GP_ASSERT(offset < m_size, "Chunk index out of bounds");
        SizeType actualSize = std::min(chunkSize, m_size - offset);
        return { m_data + offset, actualSize };
    }

    /// \brief Returns an iterator to the first element in the span.
    /// \return An iterator pointing to the first element in the span.
    GP_NODISCARD constexpr Iterator begin() const noexcept { return m_data; }

    /// \brief Returns an iterator to one past the last element in the span.
    /// \return An iterator pointing to one past the last element in the span.
    GP_NODISCARD constexpr Iterator end() const noexcept { return m_data + m_size; }

    /// \brief Returns a reverse iterator to the last element in the span.
    /// \return A reverse iterator pointing to the last element in the span.
    GP_NODISCARD constexpr ConstIterator cbegin() const noexcept { return m_data; }

    /// \brief Returns a reverse iterator to one before the first element in the span.
    /// \return A reverse iterator pointing to one before the first element in the span.
    GP_NODISCARD constexpr ConstIterator cend() const noexcept { return m_data + m_size; }

    /// \brief Returns a reverse iterator to the last element in the span.
    /// \return A reverse iterator pointing to the last element in the span.
    GP_NODISCARD constexpr ReverseIterator rbegin() const noexcept { return ReverseIterator(end()); }

    /// \brief Returns a reverse iterator to one before the first element in the span.
    /// \return A reverse iterator pointing to one before the first element in the span.
    GP_NODISCARD constexpr ReverseIterator rend() const noexcept { return ReverseIterator(begin()); }

    /// \brief Returns a reverse iterator to the last element in the span.
    /// \return A reverse iterator pointing to the last element in the span.
    GP_NODISCARD constexpr ConstReverseIterator crbegin() const noexcept { return ConstReverseIterator(cend()); }

    /// \brief Returns a reverse iterator to one before the first element in the span.
    /// \return A reverse iterator pointing to one before the first element in the span.
    GP_NODISCARD constexpr ConstReverseIterator crend() const noexcept { return ConstReverseIterator(cbegin()); }
};

}   // namespace GP::Container
