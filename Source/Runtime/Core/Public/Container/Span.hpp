// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include <array>
#include <cstddef>
#include <iterator>
#include <span>
#include <type_traits>

namespace GP::Container
{

/// \brief A constant representing a dynamic extent for spans. When used as the `Extent` template parameter in `TSpan`,
/// it indicates that the span can have any size and is not fixed at compile time.
static inline constexpr SizeT DynamicExtent = static_cast<SizeT>(-1);

/// \brief A view over a contiguous sequence of objects. It does not own the underlying data and is typically used for
/// read-only access to a range of elements.
/// \tparam T The type of elements in the span.
/// \tparam Extent The number of elements in the span. If set to `static_cast<SizeT>(-1)`, the span is dynamic and can
/// have any size.
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

    /// \brief Constructor from a `std::array`. Creates a span that views all elements of the array. Only allowed if the
    /// extent is dynamic or matches the size of the array.
    /// \param arr A reference to a `std::array` of elements.
    template <SizeT N>
    constexpr TSpan(std::array<T, N>& arr) noexcept requires(Extent == DynamicExtent || Extent == N)
        : m_data(arr.data())
        , m_size(N)
    {}

    /// \brief Constructor from a std::span. Creates a span that views the same elements as the provided std::span. Only
    /// allowed if the extent is dynamic or matches the size of the std::span.
    /// \param other A reference to a `std::span` of elements.
    template <SizeT N>
    constexpr TSpan(std::span<T, N> other) noexcept requires(Extent == DynamicExtent || Extent == N)
        : m_data(other.data())
        , m_size(other.size())
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

    /// \brief Returns a subspan that represents the last `count` elements of the span. Performs bounds checking in
    /// debug builds.
    /// \param count The number of elements to include in the subspan.
    GP_NODISCARD constexpr TSpan<T, DynamicExtent> Last(SizeType count) const noexcept
    {
        GP_ASSERT(count <= m_size, "Last count exceeds span size");
        return { m_data + (m_size - count), count };
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
