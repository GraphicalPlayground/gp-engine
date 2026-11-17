// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Concepts.hpp"
#include "containers/arrays/Array.hpp"
#include "containers/ContainerForward.hpp"
#include "containers/views/StridedView.hpp"
#include "containers/views/VectorView.hpp"
#include "CoreMinimal.hpp"
#include "maths/base/Scalar.hpp"
#include "memory/allocators/AlignedHeapAllocator.hpp"     // IWYU pragma: keep
#include "memory/allocators/AllocatorConcepts.hpp"
#include "memory/allocators/FixedBufferAllocator.hpp"     // IWYU pragma: keep
#include "memory/allocators/HeapAllocator.hpp"
#include "memory/allocators/InlineAllocator.hpp"          // IWYU pragma: keep
#include "memory/allocators/PolymorphicAllocator.hpp"     // IWYU pragma: keep
#include "memory/allocators/ThreadScratchAllocator.hpp"   // IWYU pragma: keep
#include "memory/Memory.hpp"
#include "memory/MemoryBase.hpp"
#include "memory/utils/AllocatorUtilities.hpp"
#include "memory/utils/MemoryOps.hpp"
#include "platforms/base/Platform.hpp"
#include <algorithm>
#include <compare>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <type_traits>
#include <utility>

namespace gp::container
{

namespace detail
{

/// @brief Computes the embedded inline element capacity for SBO allocators (`InlineAllocator`, `FixedBufferAllocator`).
/// @tparam InAllocator Engine allocator type.
/// @tparam InT Element type.
/// @return Number of `InT` elements that fit in the allocator's inline buffer, or `0` for non-inline allocators.
template <typename InAllocator, typename InT>
[[nodiscard]] consteval gp::USize resolveInlineElementCapacity() noexcept
{
    if constexpr (requires { InAllocator::kInlineCapacityBytes; })
    {
        return sizeof(InT) > 0u ? (InAllocator::kInlineCapacityBytes / sizeof(InT)) : 0u;
    }
    else if constexpr (requires { InAllocator::kCapacityBytes; })
    {
        return sizeof(InT) > 0u ? (InAllocator::kCapacityBytes / sizeof(InT)) : 0u;
    }
    else
    {
        return 0u;
    }
}

}   // namespace detail

/// @brief Allocator-aware contiguous dynamic array with bitwise relocatability and swap-erase ergonomics (ADR-0055).
/// @details Stores a 24-byte `{ InT* m_data, USize m_size, USize m_capacity }` header alongside a
///          `GP_NO_UNIQUE_ADDRESS` allocator satisfying `gp::concepts::IsAllocator` (ADR-0042).
///          Supports zero-overhead stateless heap/scratch allocators, stack-embedded `InlineVector` / `FixedVector`
///          small-buffer optimization, runtime-injected `PolymorphicVector` arenas, uninitialized/defaulted resize
///          modes, self-aliasing-safe mutations, and O(1) unordered swap-erase operations.
/// @tparam InT The type of elements stored in the vector. Cannot be `const`.
/// @tparam InAllocator Engine allocator type satisfying `gp::concepts::IsAllocator`.
template <typename InT, typename InAllocator>
class Vector
{
    static_assert(!concepts::IsConst<InT>, "gp::Vector element type InT cannot be const.");
    static_assert(
        concepts::IsAllocator<InAllocator>, "gp::Vector requires InAllocator to satisfy gp::concepts::IsAllocator."
    );

    template <typename InOtherT, typename InOtherAllocator>
    friend class Vector;

public:
    using ValueType = InT;
    using AllocatorType = InAllocator;
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
    using ViewType = VectorView<ValueType>;
    using ConstViewType = VectorView<const ValueType>;
    using StridedViewType = StridedView<ValueType>;
    using ConstStridedViewType = StridedView<const ValueType>;

    /// @brief Sentinel index returned when a search operation fails to find a matching element.
    static constexpr SizeType npos = static_cast<SizeType>(-1);

    /// @brief Byte alignment enforced on all backing buffer allocations.
    static constexpr gp::UInt32 kAllocAlignment = static_cast<gp::UInt32>(
        alignof(InT) > gp::memory::kMinimumAlignment ? alignof(InT) : gp::memory::kMinimumAlignment
    );

    /// @brief Number of elements that fit inside the allocator's embedded inline buffer (`0` for heap/arena allocators).
    static constexpr SizeType kInlineElementCapacity = detail::resolveInlineElementCapacity<InAllocator, InT>();

public:
    /// @brief Default constructor. Initializes an empty vector (`nullptr`, size `0`, capacity `0`) with zero
    /// allocations.
    [[nodiscard]] constexpr Vector() noexcept = default;

    /// @brief Constructs an empty vector bound to a copy of `allocator`.
    /// @param[in] allocator Allocator instance to copy.
    [[nodiscard]] constexpr explicit Vector(const InAllocator& allocator) noexcept
        requires std::is_copy_constructible_v<InAllocator>
        : m_data(nullptr)
        , m_size(0u)
        , m_capacity(0u)
        , m_allocator(allocator)
    {}

    /// @brief Constructs an empty vector by moving `allocator` into this container.
    /// @param[in,out] allocator Allocator instance to move from.
    [[nodiscard]] constexpr explicit Vector(InAllocator&& allocator) noexcept
        requires std::is_move_constructible_v<InAllocator>
        : m_data(nullptr)
        , m_size(0u)
        , m_capacity(0u)
        , m_allocator(std::move(allocator))
    {}

    /// @brief Constructs a vector with `count` value-initialized elements.
    /// @param[in] count Number of elements to construct.
    [[nodiscard]] constexpr explicit Vector(const SizeType count) noexcept
    {
        resize(count);
    }

    /// @brief Constructs a vector with `count` value-initialized elements using `allocator`.
    /// @param[in] count Number of elements to construct.
    /// @param[in] allocator Allocator instance to copy.
    [[nodiscard]] constexpr Vector(const SizeType count, const InAllocator& allocator) noexcept
        requires std::is_copy_constructible_v<InAllocator>
        : m_data(nullptr)
        , m_size(0u)
        , m_capacity(0u)
        , m_allocator(allocator)
    {
        resize(count);
    }

    /// @brief Constructs a vector with `count` copies of `value`.
    /// @param[in] count Number of elements to construct.
    /// @param[in] value Prototype value to copy into each slot.
    [[nodiscard]] constexpr Vector(const SizeType count, const InT& value) noexcept
    {
        assign(count, value);
    }

    /// @brief Constructs a vector with `count` copies of `value` using `allocator`.
    /// @param[in] count Number of elements to construct.
    /// @param[in] value Prototype value to copy into each slot.
    /// @param[in] allocator Allocator instance to copy.
    [[nodiscard]] constexpr Vector(const SizeType count, const InT& value, const InAllocator& allocator) noexcept
        requires std::is_copy_constructible_v<InAllocator>
        : m_data(nullptr)
        , m_size(0u)
        , m_capacity(0u)
        , m_allocator(allocator)
    {
        assign(count, value);
    }

    /// @brief Constructs a vector by copying `count` elements from `source`.
    /// @param[in] source Pointer to the source element array (may be `nullptr` if `count == 0`).
    /// @param[in] count Number of elements to copy.
    [[nodiscard]] constexpr Vector(const InT* const source, const SizeType count) noexcept
    {
        assign(source, count);
    }

    /// @brief Constructs a vector by copying `count` elements from `source` using `allocator`.
    /// @param[in] source Pointer to the source element array (may be `nullptr` if `count == 0`).
    /// @param[in] count Number of elements to copy.
    /// @param[in] allocator Allocator instance to copy.
    [[nodiscard]] constexpr Vector(const InT* const source, const SizeType count, const InAllocator& allocator) noexcept
        requires std::is_copy_constructible_v<InAllocator>
        : m_data(nullptr)
        , m_size(0u)
        , m_capacity(0u)
        , m_allocator(allocator)
    {
        assign(source, count);
    }

    /// @brief Constructs a vector from an iterator range `[first, last)`.
    /// @tparam InInputIt Input iterator yielding elements convertible to `InT`.
    /// @param[in] first Beginning of the range.
    /// @param[in] last End of the range.
    template <typename InInputIt>
    requires(!std::is_integral_v<InInputIt>)
    [[nodiscard]] constexpr Vector(InInputIt first, InInputIt last) noexcept
    {
        assign(first, last);
    }

    /// @brief Constructs a vector from an iterator range `[first, last)` using `allocator`.
    /// @tparam InInputIt Input iterator yielding elements convertible to `InT`.
    /// @param[in] first Beginning of the range.
    /// @param[in] last End of the range.
    /// @param[in] allocator Allocator instance to copy.
    template <typename InInputIt>
    requires(!std::is_integral_v<InInputIt> && std::is_copy_constructible_v<InAllocator>)
    [[nodiscard]] constexpr Vector(InInputIt first, InInputIt last, const InAllocator& allocator) noexcept
        : m_data(nullptr)
        , m_size(0u)
        , m_capacity(0u)
        , m_allocator(allocator)
    {
        assign(first, last);
    }

    /// @brief Constructs a vector from an initializer list.
    /// @param[in] init Initializer list of elements.
    [[nodiscard]] constexpr Vector(const std::initializer_list<InT> init) noexcept
    {
        assign(init.begin(), init.size());
    }

    /// @brief Constructs a vector from an initializer list using `allocator`.
    /// @param[in] init Initializer list of elements.
    /// @param[in] allocator Allocator instance to copy.
    [[nodiscard]] constexpr Vector(const std::initializer_list<InT> init, const InAllocator& allocator) noexcept
        requires std::is_copy_constructible_v<InAllocator>
        : m_data(nullptr)
        , m_size(0u)
        , m_capacity(0u)
        , m_allocator(allocator)
    {
        assign(init.begin(), init.size());
    }

    /// @brief Copy constructor when `InAllocator` is copy-constructible.
    /// @param[in] other Source vector to copy.
    [[nodiscard]] constexpr Vector(const Vector& other) noexcept requires std::is_copy_constructible_v<InAllocator>
        : m_data(nullptr)
        , m_size(0u)
        , m_capacity(0u)
        , m_allocator(other.m_allocator)
    {
        assign(other.m_data, other.m_size);
    }

    /// @brief Copy constructor when `InAllocator` is not copy-constructible (e.g. `InlineAllocator`,
    /// `FixedBufferAllocator`).
    /// @param[in] other Source vector to copy into a default-constructed allocator.
    [[nodiscard]] constexpr Vector(const Vector& other) noexcept
        requires(!std::is_copy_constructible_v<InAllocator> && std::is_default_constructible_v<InAllocator>)
        : m_data(nullptr)
        , m_size(0u)
        , m_capacity(0u)
        , m_allocator{}
    {
        assign(other.m_data, other.m_size);
    }

    /// @brief Copy constructor with an explicit target `allocator`.
    /// @param[in] other Source vector to copy.
    /// @param[in] allocator Allocator instance to copy.
    [[nodiscard]] constexpr Vector(const Vector& other, const InAllocator& allocator) noexcept
        requires std::is_copy_constructible_v<InAllocator>
        : m_data(nullptr)
        , m_size(0u)
        , m_capacity(0u)
        , m_allocator(allocator)
    {
        assign(other.m_data, other.m_size);
    }

    /// @brief Cross-allocator copy constructor from a `Vector` using a different allocator type.
    /// @tparam InOtherAllocator Source vector's allocator type.
    /// @param[in] other Source vector to copy elements from.
    template <concepts::IsAllocator InOtherAllocator>
    requires(!std::is_same_v<InAllocator, InOtherAllocator>)
    [[nodiscard]] constexpr explicit Vector(const Vector<InT, InOtherAllocator>& other) noexcept
        : m_data(nullptr)
        , m_size(0u)
        , m_capacity(0u)
    {
        assign(other.m_data, other.m_size);
    }

    /// @brief Cross-allocator copy constructor from a `Vector` with an explicit target `allocator`.
    /// @tparam InOtherAllocator Source vector's allocator type.
    /// @param[in] other Source vector to copy elements from.
    /// @param[in] allocator Allocator instance to copy.
    template <concepts::IsAllocator InOtherAllocator>
    requires(!std::is_same_v<InAllocator, InOtherAllocator> && std::is_copy_constructible_v<InAllocator>)
    [[nodiscard]] constexpr Vector(const Vector<InT, InOtherAllocator>& other, const InAllocator& allocator) noexcept
        : m_data(nullptr)
        , m_size(0u)
        , m_capacity(0u)
        , m_allocator(allocator)
    {
        assign(other.m_data, other.m_size);
    }

    /// @brief Constructs a vector from a fixed-size `gp::Array`.
    /// @tparam InN Number of elements in the source `Array`.
    /// @tparam InAlign Alignment of the source `Array`.
    /// @param[in] arr Source fixed array to copy elements from.
    template <gp::USize InN, gp::USize InAlign>
    [[nodiscard]] constexpr explicit Vector(const Array<InT, InN, InAlign>& arr) noexcept
    {
        assign(arr.data(), arr.size());
    }

    /// @brief Constructs a vector by copying elements from a `VectorView`.
    /// @tparam InOtherT View element type (`InT` or `const InT`).
    /// @param[in] view Source contiguous view.
    template <typename InOtherT>
    requires(concepts::IsSameAs<std::remove_cv_t<InOtherT>, InT>)
    [[nodiscard]] constexpr explicit Vector(const VectorView<InOtherT>& view) noexcept
    {
        assign(view.data(), view.size());
    }

    /// @brief Constructs a vector by copying elements from a `VectorView` using `allocator`.
    /// @tparam InOtherT View element type (`InT` or `const InT`).
    /// @param[in] view Source contiguous view.
    /// @param[in] allocator Allocator instance to copy.
    template <typename InOtherT>
    requires(concepts::IsSameAs<std::remove_cv_t<InOtherT>, InT> && std::is_copy_constructible_v<InAllocator>)
    [[nodiscard]] constexpr Vector(const VectorView<InOtherT>& view, const InAllocator& allocator) noexcept
        : m_data(nullptr)
        , m_size(0u)
        , m_capacity(0u)
        , m_allocator(allocator)
    {
        assign(view.data(), view.size());
    }

    /// @brief Constructs a vector by copying elements from a `StridedView`.
    /// @tparam InOtherT Strided view element type (`InT` or `const InT`).
    /// @param[in] strided Source strided view.
    template <typename InOtherT>
    requires(concepts::IsSameAs<std::remove_cv_t<InOtherT>, InT>)
    [[nodiscard]] constexpr explicit Vector(const StridedView<InOtherT>& strided) noexcept
    {
        assign(strided);
    }

    /// @brief Constructs a vector by copying elements from a `StridedView` using `allocator`.
    /// @tparam InOtherT Strided view element type (`InT` or `const InT`).
    /// @param[in] strided Source strided view.
    /// @param[in] allocator Allocator instance to copy.
    template <typename InOtherT>
    requires(concepts::IsSameAs<std::remove_cv_t<InOtherT>, InT> && std::is_copy_constructible_v<InAllocator>)
    [[nodiscard]] constexpr Vector(const StridedView<InOtherT>& strided, const InAllocator& allocator) noexcept
        : m_data(nullptr)
        , m_size(0u)
        , m_capacity(0u)
        , m_allocator(allocator)
    {
        assign(strided);
    }

    /// @brief Move constructor when `InAllocator` is move-constructible.
    /// @details Transfers pointer ownership in O(1) when the moved allocator compares equal to `other.m_allocator`.
    ///          Otherwise relocates elements into `this->m_allocator` and frees `other`'s buffer.
    /// @param[in,out] other Source vector to move from; left in a valid empty state.
    [[nodiscard]] constexpr Vector(Vector&& other) noexcept requires std::is_move_constructible_v<InAllocator>
        : m_data(nullptr)
        , m_size(0u)
        , m_capacity(0u)
        , m_allocator(std::move(other.m_allocator))
    {
        if (m_allocator == other.m_allocator)
        {
            m_data = other.m_data;
            m_size = other.m_size;
            m_capacity = other.m_capacity;
            other.m_data = nullptr;
            other.m_size = 0u;
            other.m_capacity = 0u;
        }
        else
        {
            relocateFromOther(other);
        }
    }

    /// @brief Move constructor when `InAllocator` is not move-constructible (e.g. `InlineAllocator`,
    /// `FixedBufferAllocator`).
    /// @details If `other`'s buffer lives on a movable fallback allocator that compares equal, steals the heap pointer
    ///          in O(1). If `other`'s buffer points inside `other`'s embedded inline storage, relocates elements into
    ///          this instance's inline buffer with zero element copies.
    /// @param[in,out] other Source vector to move from; left in a valid empty state.
    [[nodiscard]] constexpr Vector(Vector&& other) noexcept
        requires(!std::is_move_constructible_v<InAllocator> && std::is_default_constructible_v<InAllocator>)
        : m_data(nullptr)
        , m_size(0u)
        , m_capacity(0u)
        , m_allocator{}
    {
        if (canStealFromNonMovableAllocator(other))
        {
            m_data = other.m_data;
            m_size = other.m_size;
            m_capacity = other.m_capacity;
            other.m_data = nullptr;
            other.m_size = 0u;
            other.m_capacity = 0u;
        }
        else
        {
            relocateFromOther(other);
        }
    }

    /// @brief Move constructor with an explicit target `allocator`.
    /// @param[in,out] other Source vector to move from; left in a valid empty state.
    /// @param[in] allocator Target allocator instance.
    [[nodiscard]] constexpr Vector(Vector&& other, const InAllocator& allocator) noexcept
        requires std::is_copy_constructible_v<InAllocator>
        : m_data(nullptr)
        , m_size(0u)
        , m_capacity(0u)
        , m_allocator(allocator)
    {
        if (m_allocator == other.m_allocator)
        {
            m_data = other.m_data;
            m_size = other.m_size;
            m_capacity = other.m_capacity;
            other.m_data = nullptr;
            other.m_size = 0u;
            other.m_capacity = 0u;
        }
        else
        {
            relocateFromOther(other);
        }
    }

    /// @brief Destructor. Destructs all live elements and releases any backing allocation to `m_allocator`.
    constexpr ~Vector() noexcept
    {
        reset();
    }

public:
    /// @brief Copy assignment operator.
    /// @param[in] other Source vector to copy from.
    /// @return Reference to `*this`.
    constexpr Vector& operator=(const Vector& other) noexcept
    {
        if (this != &other)
        {
            assign(other.m_data, other.m_size);
        }
        return *this;
    }

    /// @brief Cross-allocator copy assignment operator.
    /// @tparam InOtherAllocator Source vector's allocator type.
    /// @param[in] other Source vector to copy elements from.
    /// @return Reference to `*this`.
    template <concepts::IsAllocator InOtherAllocator>
    requires(!std::is_same_v<InAllocator, InOtherAllocator>)
    constexpr Vector& operator=(const Vector<InT, InOtherAllocator>& other) noexcept
    {
        assign(other.m_data, other.m_size);
        return *this;
    }

    /// @brief Move assignment operator.
    /// @details Steals the backing pointer in O(1) when allocators share ownership; otherwise relocates elements into
    ///          `this->m_allocator` and resets `other`.
    /// @param[in,out] other Source vector to move from; left in a valid empty state.
    /// @return Reference to `*this`.
    constexpr Vector& operator=(Vector&& other) noexcept
    {
        if (this == &other) [[unlikely]]
        {
            return *this;
        }

        if ((m_allocator == other.m_allocator) || canStealFromNonMovableAllocator(other))
        {
            reset();
            m_data = other.m_data;
            m_size = other.m_size;
            m_capacity = other.m_capacity;
            other.m_data = nullptr;
            other.m_size = 0u;
            other.m_capacity = 0u;
            return *this;
        }

        relocateFromOther(other);
        return *this;
    }

    /// @brief Initializer-list assignment operator.
    /// @param[in] init Initializer list of elements to copy.
    /// @return Reference to `*this`.
    constexpr Vector& operator=(const std::initializer_list<InT> init) noexcept
    {
        return assign(init.begin(), init.size());
    }

public:
    /// @brief Replaces the contents of the vector with `count` copies of `value`.
    /// @param[in] count Number of elements to assign.
    /// @param[in] value Prototype value to copy into each slot.
    /// @return Reference to `*this`.
    constexpr Vector& assign(const SizeType count, const InT& value) noexcept
    {
        if (count == 0u)
        {
            clear();
            return *this;
        }

        if (isPointerInCurrentBuffer(&value)) [[unlikely]]
        {
            const InT valueCopy(value);
            return assignNonAliasingValue(count, valueCopy);
        }

        return assignNonAliasingValue(count, value);
    }

    /// @brief Replaces the contents of the vector by copying `count` elements from `source` (self-aliasing safe).
    /// @param[in] source Pointer to the source element array.
    /// @param[in] count Number of elements to copy.
    /// @return Reference to `*this`.
    constexpr Vector& assign(const InT* const source, const SizeType count) noexcept
    {
        if (count == 0u || source == nullptr)
        {
            clear();
            return *this;
        }

        if (isRangeOverlappingCurrentBuffer(source, count)) [[unlikely]]
        {
            const SizeType offset = static_cast<SizeType>(source - m_data);
            if constexpr (concepts::IsBitwiseCopyable<InT>)
            {
                if (!std::is_constant_evaluated())
                {
                    if (offset > 0u)
                    {
                        gp::memory::moveMemory(m_data, m_data + offset, count * sizeof(InT));
                    }
                    m_size = count;
                    return *this;
                }
            }

            for (SizeType i = 0u; i < count; ++i)
            {
                if (offset > 0u)
                {
                    m_data[i] = std::move(m_data[offset + i]);
                }
            }
            destructRange(m_data + count, m_size - count);
            m_size = count;
            return *this;
        }

        clear();
        if (count > m_capacity)
        {
            const SizeType targetCap = computeReserveCapacity(count);
            reallocateBuffer(targetCap);
        }

        copyConstructRange(m_data, source, count);
        m_size = count;
        return *this;
    }

    /// @brief Replaces the contents of the vector with elements from `[first, last)`.
    /// @tparam InInputIt Input iterator yielding elements convertible to `InT`.
    /// @param[in] first Beginning of the source range.
    /// @param[in] last End of the source range.
    /// @return Reference to `*this`.
    template <typename InInputIt>
    requires(!std::is_integral_v<InInputIt>)
    constexpr Vector& assign(InInputIt first, InInputIt last) noexcept
    {
        if constexpr (std::contiguous_iterator<InInputIt> &&
                      concepts::IsSameAs<std::remove_cvref_t<std::iter_value_t<InInputIt>>, InT>)
        {
            const SizeType count = static_cast<SizeType>(std::distance(first, last));
            return assign( count > 0u ? std::to_address(first) : nullptr, count );
        }
        else
        {
            clear();
            if constexpr (std::forward_iterator<InInputIt>)
            {
                const SizeType dist = static_cast<SizeType>(std::distance(first, last));
                if (dist > m_capacity)
                {
                    reserve(dist);
                }
            }

            for (; first != last; ++first)
            {
                emplaceBack(*first);
            }
            return *this;
        }
    }

    /// @brief Replaces the contents of the vector with an initializer list.
    /// @param[in] init Initializer list of elements.
    /// @return Reference to `*this`.
    constexpr Vector& assign(const std::initializer_list<InT> init) noexcept
    {
        return assign(init.begin(), init.size());
    }

    /// @brief Replaces the contents of the vector with elements from a `VectorView`.
    /// @tparam InOtherT View element type (`InT` or `const InT`).
    /// @param[in] view Source contiguous view.
    /// @return Reference to `*this`.
    template <typename InOtherT>
    requires(concepts::IsSameAs<std::remove_cv_t<InOtherT>, InT>)
    constexpr Vector& assign(const VectorView<InOtherT>& view) noexcept
    {
        return assign(view.data(), view.size());
    }

    /// @brief Replaces the contents of the vector with elements from a `StridedView`.
    /// @tparam InOtherT Strided view element type (`InT` or `const InT`).
    /// @param[in] strided Source strided view.
    /// @return Reference to `*this`.
    template <typename InOtherT>
    requires(concepts::IsSameAs<std::remove_cv_t<InOtherT>, InT>)
    constexpr Vector& assign(const StridedView<InOtherT>& strided) noexcept
    {
        if (strided.isContiguous())
        {
            return assign(strided.data(), strided.size());
        }

        clear();
        if (strided.size() > m_capacity)
        {
            reallocateBuffer(computeReserveCapacity(strided.size()));
        }

        for (SizeType i = 0u; i < strided.size(); ++i)
        {
            constructAtSlot(m_data + i, strided[i]);
        }
        m_size = strided.size();
        return *this;
    }

public:
    /// @brief Unchecked element access by index.
    /// @param[in] index Zero-based index of the element (`index < size()`).
    /// @return Reference to the element at `index`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr Reference operator[](const SizeType index) noexcept
    {
        GP_ASSERT(index < m_size, "Vector::operator[] index out of bounds.");
        return m_data[index];
    }

    /// @brief Unchecked const element access by index.
    /// @param[in] index Zero-based index of the element (`index < size()`).
    /// @return Const reference to the element at `index`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr ConstReference operator[](const SizeType index) const noexcept
    {
        GP_ASSERT(index < m_size, "Vector::operator[] const index out of bounds.");
        return m_data[index];
    }

    /// @brief Bounds-checked element access by index.
    /// @param[in] index Zero-based index of the element (`index < size()`).
    /// @return Reference to the element at `index`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr Reference at(const SizeType index) noexcept
    {
        GP_ASSERT(index < m_size, "Vector::at index out of bounds.");
        return m_data[index];
    }

    /// @brief Bounds-checked const element access by index.
    /// @param[in] index Zero-based index of the element (`index < size()`).
    /// @return Const reference to the element at `index`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr ConstReference at(const SizeType index) const noexcept
    {
        GP_ASSERT(index < m_size, "Vector::at const index out of bounds.");
        return m_data[index];
    }

    /// @brief Returns a reference to the first element in the vector.
    /// @return Reference to `m_data[0]`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr Reference front() noexcept
    {
        GP_ASSERT(m_size > 0u, "Vector::front called on an empty vector.");
        return m_data[0u];
    }

    /// @brief Returns a const reference to the first element in the vector.
    /// @return Const reference to `m_data[0]`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr ConstReference front() const noexcept
    {
        GP_ASSERT(m_size > 0u, "Vector::front const called on an empty vector.");
        return m_data[0u];
    }

    /// @brief Returns a reference to the last element in the vector.
    /// @return Reference to `m_data[size() - 1]`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr Reference back() noexcept
    {
        GP_ASSERT(m_size > 0u, "Vector::back called on an empty vector.");
        return m_data[m_size - 1u];
    }

    /// @brief Returns a const reference to the last element in the vector.
    /// @return Const reference to `m_data[size() - 1]`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr ConstReference back() const noexcept
    {
        GP_ASSERT(m_size > 0u, "Vector::back const called on an empty vector.");
        return m_data[m_size - 1u];
    }

    /// @brief Accesses an element indexed backward from the end of the vector (`0` = last element, `1` = second-to-last).
    /// @param[in] indexFromEnd Offset backward from `size() - 1`.
    /// @return Reference to `m_data[size() - 1 - indexFromEnd]`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr Reference last(const SizeType indexFromEnd = 0u) noexcept
    {
        GP_ASSERT(indexFromEnd < m_size, "Vector::last indexFromEnd out of bounds.");
        return m_data[m_size - 1u - indexFromEnd];
    }

    /// @brief Accesses a const element indexed backward from the end of the vector (`0` = last element).
    /// @param[in] indexFromEnd Offset backward from `size() - 1`.
    /// @return Const reference to `m_data[size() - 1 - indexFromEnd]`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr ConstReference last(const SizeType indexFromEnd = 0u) const noexcept
    {
        GP_ASSERT(indexFromEnd < m_size, "Vector::last const indexFromEnd out of bounds.");
        return m_data[m_size - 1u - indexFromEnd];
    }

    /// @brief Returns a direct pointer to the underlying contiguous array.
    /// @return Pointer to the first element, or `nullptr` if no buffer is allocated.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr Pointer data() noexcept
    {
        return m_data;
    }

    /// @brief Returns a direct const pointer to the underlying contiguous array.
    /// @return Const pointer to the first element, or `nullptr` if no buffer is allocated.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr ConstPointer data() const noexcept
    {
        return m_data;
    }

    /// @brief Checks whether `index` is within `[0, size())`.
    /// @param[in] index Index to test.
    /// @return True if `index < size()`, false otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool isValidIndex(const SizeType index) const noexcept
    {
        return index < m_size;
    }

public:
    /// @brief Returns an iterator to the beginning of the vector.
    /// @return Pointer to the first element.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr Iterator begin() noexcept
    {
        return m_data;
    }

    /// @brief Returns a const iterator to the beginning of the vector.
    /// @return Const pointer to the first element.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr ConstIterator begin() const noexcept
    {
        return m_data;
    }

    /// @brief Returns an iterator one past the last element of the vector.
    /// @return Pointer to `m_data + m_size`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr Iterator end() noexcept
    {
        return m_data + m_size;
    }

    /// @brief Returns a const iterator one past the last element of the vector.
    /// @return Const pointer to `m_data + m_size`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr ConstIterator end() const noexcept
    {
        return m_data + m_size;
    }

    /// @brief Returns a const iterator to the beginning of the vector.
    /// @return Const pointer to the first element.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr ConstIterator cbegin() const noexcept
    {
        return m_data;
    }

    /// @brief Returns a const iterator one past the last element of the vector.
    /// @return Const pointer to `m_data + m_size`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr ConstIterator cend() const noexcept
    {
        return m_data + m_size;
    }

    /// @brief Returns a reverse iterator to the last element of the vector.
    /// @return `ReverseIterator(end())`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr ReverseIterator rbegin() noexcept
    {
        return ReverseIterator(end());
    }

    /// @brief Returns a const reverse iterator to the last element of the vector.
    /// @return `ConstReverseIterator(end())`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr ConstReverseIterator rbegin() const noexcept
    {
        return ConstReverseIterator(end());
    }

    /// @brief Returns a reverse iterator one before the first element of the vector.
    /// @return `ReverseIterator(begin())`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr ReverseIterator rend() noexcept
    {
        return ReverseIterator(begin());
    }

    /// @brief Returns a const reverse iterator one before the first element of the vector.
    /// @return `ConstReverseIterator(begin())`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr ConstReverseIterator rend() const noexcept
    {
        return ConstReverseIterator(begin());
    }

    /// @brief Returns a const reverse iterator to the last element of the vector.
    /// @return `ConstReverseIterator(end())`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr ConstReverseIterator crbegin() const noexcept
    {
        return ConstReverseIterator(end());
    }

    /// @brief Returns a const reverse iterator one before the first element of the vector.
    /// @return `ConstReverseIterator(begin())`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr ConstReverseIterator crend() const noexcept
    {
        return ConstReverseIterator(begin());
    }

public:
    /// @brief Returns the number of live elements in the vector.
    /// @return Active element count.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr SizeType size() const noexcept
    {
        return m_size;
    }

    /// @brief Returns the total byte size of all live elements (`size() * sizeof(InT)`).
    /// @return Active payload size in bytes.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr SizeType sizeInBytes() const noexcept
    {
        return m_size * sizeof(InT);
    }

    /// @brief Returns the number of elements that can be held without reallocating.
    /// @return Allocated capacity in elements.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr SizeType capacity() const noexcept
    {
        return m_capacity;
    }

    /// @brief Returns the total byte size of the currently allocated buffer (`capacity() * sizeof(InT)`).
    /// @return Allocated buffer size in bytes.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr SizeType capacityInBytes() const noexcept
    {
        return m_capacity * sizeof(InT);
    }

    /// @brief Returns the number of unused element slots currently reserved (`capacity() - size()`).
    /// @return Available element slack.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr SizeType slack() const noexcept
    {
        return m_capacity - m_size;
    }

    /// @brief Returns the theoretical maximum number of `InT` elements representable by `SizeType`.
    /// @return Maximum element count.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr SizeType maxSize() const noexcept
    {
        return (npos - 1u) / sizeof(InT);
    }

    /// @brief Checks whether the vector contains zero elements (`size() == 0`).
    /// @return True if empty, false otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool isEmpty() const noexcept
    {
        return m_size == 0u;
    }

    /// @brief Returns a mutable reference to the underlying allocator instance.
    /// @return Reference to `m_allocator`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr InAllocator& getAllocator() noexcept
    {
        return m_allocator;
    }

    /// @brief Returns a const reference to the underlying allocator instance.
    /// @return Const reference to `m_allocator`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr const InAllocator& getAllocator() const noexcept
    {
        return m_allocator;
    }

    /// @brief Reserves storage for at least `minCapacity` elements without changing `size()`.
    /// @param[in] minCapacity Minimum desired capacity in elements.
    constexpr void reserve(const SizeType minCapacity) noexcept
    {
        if (minCapacity <= m_capacity)
        {
            return;
        }

        const SizeType targetCap = computeReserveCapacity(minCapacity);
        reallocateBuffer(targetCap);
    }

    /// @brief Attempts to reserve storage for at least `minCapacity` elements without asserting on fixed-arena
    ///        exhaustion.
    /// @param[in] minCapacity Minimum desired capacity in elements.
    /// @return True if `capacity() >= minCapacity`, false if the underlying arena could not satisfy the allocation.
    [[nodiscard]] constexpr bool tryReserve(const SizeType minCapacity) noexcept
    {
        if (minCapacity <= m_capacity)
        {
            return true;
        }

        const SizeType targetCap = computeReserveCapacity(minCapacity);
        return tryReallocateBuffer(targetCap);
    }

    /// @brief Shrinks the allocated buffer to match `size()` (or frees the buffer if empty).
    /// @details For `InlineVector<T, N>`, if `size() <= N`, demotes a spilled heap buffer back into the inline SBO
    ///          buffer and frees the heap block.
    constexpr void shrinkToFit() noexcept
    {
        if (m_size == 0u)
        {
            if (m_data != nullptr)
            {
                deallocateRaw(m_data, m_capacity);
                m_data = nullptr;
                m_capacity = 0u;
            }
            return;
        }

        const SizeType targetCap = clampToInlineCapacity(m_size);
        if (targetCap < m_capacity)
        {
            reallocateBuffer(targetCap);
        }
    }

    /// @brief Conditionally shrinks capacity using `gp::memory::calculateSlackShrink` heuristics.
    /// @details Reallocates only when unused slack exceeds 16 KiB or one-third of capacity (and > 64 elements),
    ///          preventing reallocation thrashing during alternating push/pop frame workloads.
    constexpr void compact() noexcept
    {
        if (m_size == 0u)
        {
            shrinkToFit();
            return;
        }

        if (std::is_constant_evaluated())
        {
            shrinkToFit();
            return;
        }

        const SizeType shrunkCap = clampToInlineCapacity(
            gp::memory::calculateSlackShrink<SizeType>(m_allocator, m_size, m_capacity, sizeof(InT), true, kAllocAlignment)
        );
        if (shrunkCap < m_capacity)
        {
            reallocateBuffer(shrunkCap);
        }
    }

    /// @brief Destructs all live elements and sets `size()` to `0` while retaining the current `capacity()`.
    constexpr void clear() noexcept
    {
        destructRange(m_data, m_size);
        m_size = 0u;
    }

    /// @brief Destructs all live elements, frees the backing buffer (if `newCapacity == 0`), and optionally reserves
    ///        `newCapacity` elements.
    /// @param[in] newCapacity Capacity to reserve after resetting (defaults to `0`, which frees all storage).
    constexpr void reset(const SizeType newCapacity = 0u) noexcept
    {
        destructRange(m_data, m_size);
        m_size = 0u;

        if (newCapacity == 0u)
        {
            if (m_data != nullptr)
            {
                deallocateRaw(m_data, m_capacity);
                m_data = nullptr;
                m_capacity = 0u;
            }
        }
        else if (newCapacity > m_capacity)
        {
            reallocateBuffer(computeReserveCapacity(newCapacity));
        }
    }

public:
    /// @brief Resizes the vector to `newSize`, value-initializing (`zeroMemory` for trivial types) any newly added
    ///        elements.
    /// @param[in] newSize Target number of elements.
    constexpr void resize(const SizeType newSize) noexcept
    {
        if (newSize < m_size)
        {
            destructRange(m_data + newSize, m_size - newSize);
            m_size = newSize;
        }
        else if (newSize > m_size)
        {
            if (newSize > m_capacity)
            {
                reallocateBuffer(computeReserveCapacity(newSize));
            }
            valueConstructRange(m_data + m_size, newSize - m_size);
            m_size = newSize;
        }
    }

    /// @brief Resizes the vector to `newSize`, copy-constructing any newly added elements from `value`.
    /// @param[in] newSize Target number of elements.
    /// @param[in] value Prototype value for newly added elements (self-aliasing safe).
    constexpr void resize(const SizeType newSize, const InT& value) noexcept
    {
        if (newSize < m_size)
        {
            destructRange(m_data + newSize, m_size - newSize);
            m_size = newSize;
        }
        else if (newSize > m_size)
        {
            if (isPointerInCurrentBuffer(&value)) [[unlikely]]
            {
                const InT valueCopy(value);
                if (newSize > m_capacity)
                {
                    reallocateBuffer(computeReserveCapacity(newSize));
                }
                fillConstructRange(m_data + m_size, newSize - m_size, valueCopy);
                m_size = newSize;
                return;
            }

            if (newSize > m_capacity)
            {
                reallocateBuffer(computeReserveCapacity(newSize));
            }
            fillConstructRange(m_data + m_size, newSize - m_size, value);
            m_size = newSize;
        }
    }

    /// @brief Resizes the vector to `newSize` using default construction (`defaultConstructItems`).
    /// @details For trivially constructible types, leaves new elements uninitialized (avoiding redundant `memset`
    ///          zero-fill). For non-trivial types, invokes the default constructor on each new element.
    /// @param[in] newSize Target number of elements.
    constexpr void resizeDefaulted(const SizeType newSize) noexcept
    {
        if (newSize < m_size)
        {
            destructRange(m_data + newSize, m_size - newSize);
            m_size = newSize;
        }
        else if (newSize > m_size)
        {
            if (newSize > m_capacity)
            {
                reallocateBuffer(computeReserveCapacity(newSize));
            }
            defaultConstructRange(m_data + m_size, newSize - m_size);
            m_size = newSize;
        }
    }

    /// @brief Resizes the vector to `newSize` without invoking constructors on newly added slots.
    /// @warning Newly added slots contain uninitialized memory. The caller must construct or overwrite them before
    ///          reading or destroying non-trivial elements.
    /// @param[in] newSize Target number of elements.
    constexpr void resizeUninitialized(const SizeType newSize) noexcept
    {
        if (newSize < m_size)
        {
            destructRange(m_data + newSize, m_size - newSize);
            m_size = newSize;
        }
        else if (newSize > m_size)
        {
            if (newSize > m_capacity)
            {
                reallocateBuffer(computeReserveCapacity(newSize));
            }
            if (std::is_constant_evaluated())
            {
                valueConstructRange(m_data + m_size, newSize - m_size);
            }
            m_size = newSize;
        }
    }

    /// @brief Resizes the vector to `newSize`, zeroing raw bytes and default-constructing any non-trivial new elements.
    /// @param[in] newSize Target number of elements.
    constexpr void resizeZeroed(const SizeType newSize) noexcept
    {
        if (newSize < m_size)
        {
            destructRange(m_data + newSize, m_size - newSize);
            m_size = newSize;
        }
        else if (newSize > m_size)
        {
            if (newSize > m_capacity)
            {
                reallocateBuffer(computeReserveCapacity(newSize));
            }
            zeroAndDefaultConstructRange(m_data + m_size, newSize - m_size);
            m_size = newSize;
        }
    }

    /// @brief Attempts to resize the vector to `newSize` without asserting if a fixed-capacity arena is exhausted.
    /// @param[in] newSize Target number of elements.
    /// @return True if the resize succeeded, false if allocation failed.
    [[nodiscard]] constexpr bool tryResize(const SizeType newSize) noexcept
    {
        if (newSize < m_size)
        {
            destructRange(m_data + newSize, m_size - newSize);
            m_size = newSize;
            return true;
        }

        if (newSize > m_size)
        {
            if (newSize > m_capacity)
            {
                if (!tryReallocateBuffer(computeReserveCapacity(newSize)))
                {
                    return false;
                }
            }
            valueConstructRange(m_data + m_size, newSize - m_size);
            m_size = newSize;
        }
        return true;
    }

public:
    /// @brief Appends a copy of `value` to the end of the vector (self-aliasing safe).
    /// @param[in] value Element to copy-append.
    /// @return Reference to the newly appended element.
    constexpr Reference pushBack(const InT& value) noexcept
    {
        if (m_size == m_capacity) [[unlikely]]
        {
            if (isPointerInCurrentBuffer(&value)) [[unlikely]]
            {
                const SizeType aliasIndex = static_cast<SizeType>(&value - m_data);
                growForAdditional(1u);
                constructAtSlot(m_data + m_size, m_data[aliasIndex]);
                ++m_size;
                return m_data[m_size - 1u];
            }
            growForAdditional(1u);
        }

        constructAtSlot(m_data + m_size, value);
        ++m_size;
        return m_data[m_size - 1u];
    }

    /// @brief Appends `value` to the end of the vector by moving (self-aliasing safe).
    /// @param[in,out] value Element to move-append.
    /// @return Reference to the newly appended element.
    constexpr Reference pushBack(InT&& value) noexcept
    {
        if (m_size == m_capacity) [[unlikely]]
        {
            if (isPointerInCurrentBuffer(&value)) [[unlikely]]
            {
                const SizeType aliasIndex = static_cast<SizeType>(&value - m_data);
                growForAdditional(1u);
                constructAtSlot(m_data + m_size, std::move(m_data[aliasIndex]));
                ++m_size;
                return m_data[m_size - 1u];
            }
            growForAdditional(1u);
        }

        constructAtSlot(m_data + m_size, std::move(value));
        ++m_size;
        return m_data[m_size - 1u];
    }

    /// @brief Constructs an element in-place at the end of the vector.
    /// @tparam InArgs Constructor argument types.
    /// @param[in] args Arguments forwarded to `InT`'s constructor.
    /// @return Reference to the newly constructed element.
    template <typename... InArgs>
    constexpr Reference emplaceBack(InArgs&&... args) noexcept
    {
        if (m_size == m_capacity) [[unlikely]]
        {
            if constexpr (sizeof...(InArgs) > 0u)
            {
                if (m_data != nullptr && (isAddressInCurrentBuffer(args) || ...)) [[unlikely]]
                {
                    InT stagedValue(std::forward<InArgs>(args)...);
                    growForAdditional(1u);
                    constructAtSlot(m_data + m_size, std::move(stagedValue));
                    ++m_size;
                    return m_data[m_size - 1u];
                }
            }
            growForAdditional(1u);
        }

        constructAtSlot(m_data + m_size, std::forward<InArgs>(args)...);
        ++m_size;
        return m_data[m_size - 1u];
    }

    /// @brief Attempts to append a copy of `value` without asserting if a bounded arena is exhausted.
    /// @param[in] value Element to copy-append.
    /// @return True if the element was appended, false if capacity was exhausted and allocation failed.
    [[nodiscard]] constexpr bool tryPushBack(const InT& value) noexcept
    {
        return tryEmplaceBack(value) != nullptr;
    }

    /// @brief Attempts to move-append `value` without asserting if a bounded arena is exhausted.
    /// @param[in,out] value Element to move-append.
    /// @return True if the element was appended, false if capacity was exhausted and allocation failed.
    [[nodiscard]] constexpr bool tryPushBack(InT&& value) noexcept
    {
        return tryEmplaceBack(std::move(value)) != nullptr;
    }

    /// @brief Attempts to construct an element in-place at the end of the vector without asserting on arena exhaustion.
    /// @tparam InArgs Constructor argument types.
    /// @param[in] args Arguments forwarded to `InT`'s constructor.
    /// @return Pointer to the newly constructed element, or `nullptr` if allocation failed.
    template <typename... InArgs>
    [[nodiscard]] constexpr Pointer tryEmplaceBack(InArgs&&... args) noexcept
    {
        if (m_size == m_capacity) [[unlikely]]
        {
            if constexpr (sizeof...(InArgs) > 0u)
            {
                if (m_data != nullptr && (isAddressInCurrentBuffer(args) || ...)) [[unlikely]]
                {
                    InT stagedValue(std::forward<InArgs>(args)...);
                    if (!tryReallocateBuffer(computeGrowCapacity(m_size + 1u)))
                    {
                        return nullptr;
                    }
                    constructAtSlot(m_data + m_size, std::move(stagedValue));
                    ++m_size;
                    return m_data + (m_size - 1u);
                }
            }

            if (!tryReallocateBuffer(computeGrowCapacity(m_size + 1u)))
            {
                return nullptr;
            }
        }

        constructAtSlot(m_data + m_size, std::forward<InArgs>(args)...);
        ++m_size;
        return m_data + (m_size - 1u);
    }

    /// @brief Appends `count` default-constructed elements (`no-op` for trivial types) and returns the starting index.
    /// @param[in] count Number of elements to append.
    /// @return Index of the first newly added element.
    constexpr SizeType addDefaulted(const SizeType count = 1u) noexcept
    {
        const SizeType firstIndex = m_size;
        if (count > 0u)
        {
            if ((m_size + count) > m_capacity)
            {
                growForAdditional(count);
            }
            defaultConstructRange(m_data + m_size, count);
            m_size += count;
        }
        return firstIndex;
    }

    /// @brief Appends `count` uninitialized element slots and returns the starting index.
    /// @param[in] count Number of uninitialized slots to append.
    /// @return Index of the first newly added slot.
    constexpr SizeType addUninitialized(const SizeType count = 1u) noexcept
    {
        const SizeType firstIndex = m_size;
        if (count > 0u)
        {
            if ((m_size + count) > m_capacity)
            {
                growForAdditional(count);
            }
            if (std::is_constant_evaluated())
            {
                valueConstructRange(m_data + m_size, count);
            }
            m_size += count;
        }
        return firstIndex;
    }

    /// @brief Appends `count` zeroed and default-constructed elements and returns the starting index.
    /// @param[in] count Number of zeroed elements to append.
    /// @return Index of the first newly added element.
    constexpr SizeType addZeroed(const SizeType count = 1u) noexcept
    {
        const SizeType firstIndex = m_size;
        if (count > 0u)
        {
            if ((m_size + count) > m_capacity)
            {
                growForAdditional(count);
            }
            zeroAndDefaultConstructRange(m_data + m_size, count);
            m_size += count;
        }
        return firstIndex;
    }

    /// @brief Appends a single default-constructed element (`no-op` for trivial types) and returns a reference to it.
    /// @return Reference to the newly added element.
    constexpr Reference pushBackDefaulted() noexcept
    {
        const SizeType idx = addDefaulted(1u);
        return m_data[idx];
    }

    /// @brief Appends a single uninitialized element slot and returns a reference to it.
    /// @return Reference to the newly added uninitialized slot.
    constexpr Reference pushBackUninitialized() noexcept
    {
        const SizeType idx = addUninitialized(1u);
        return m_data[idx];
    }

    /// @brief Appends a single zero-initialized element and returns a reference to it.
    /// @return Reference to the newly added zeroed element.
    constexpr Reference pushBackZeroed() noexcept
    {
        const SizeType idx = addZeroed(1u);
        return m_data[idx];
    }

    /// @brief Appends `value` only if no equal element already exists in the vector.
    /// @param[in] value Element to search for and conditionally append.
    /// @return Index of the existing matching element, or index of the newly appended element.
    constexpr SizeType pushBackUnique(const InT& value) noexcept
    {
        const SizeType existingIdx = indexOf(value);
        if (existingIdx != npos)
        {
            return existingIdx;
        }

        pushBack(value);
        return m_size - 1u;
    }

    /// @brief Removes and destructs the last element in the vector.
    constexpr void popBack() noexcept
    {
        GP_ASSERT(m_size > 0u, "Vector::popBack called on an empty vector.");
        --m_size;
        destructRange(m_data + m_size, 1u);
    }

    /// @brief Moves the last element out of the vector, destructs its slot, and returns the moved value.
    /// @return The removed tail element.
    [[nodiscard]] constexpr InT popBackValue() noexcept
    {
        GP_ASSERT(m_size > 0u, "Vector::popBackValue called on an empty vector.");
        --m_size;
        InT result(std::move(m_data[m_size]));
        destructRange(m_data + m_size, 1u);
        return result;
    }

public:
    /// @brief Appends `count` elements copied from `source` (self-aliasing safe).
    /// @param[in] source Pointer to the source element array.
    /// @param[in] count Number of elements to append.
    /// @return Reference to `*this`.
    constexpr Vector& append(const InT* const source, const SizeType count) noexcept
    {
        if (count == 0u || source == nullptr)
        {
            return *this;
        }

        if (isRangeOverlappingCurrentBuffer(source, count)) [[unlikely]]
        {
            const SizeType offset = static_cast<SizeType>(source - m_data);
            if ((m_size + count) > m_capacity)
            {
                growForAdditional(count);
            }
            copyConstructRange(m_data + m_size, m_data + offset, count);
            m_size += count;
            return *this;
        }

        if ((m_size + count) > m_capacity)
        {
            growForAdditional(count);
        }
        copyConstructRange(m_data + m_size, source, count);
        m_size += count;
        return *this;
    }

    /// @brief Appends all elements from another `Vector` (with any allocator type).
    /// @tparam InOtherAllocator Source vector's allocator type.
    /// @param[in] other Source vector whose elements are copied to the end of `*this`.
    /// @return Reference to `*this`.
    template <concepts::IsAllocator InOtherAllocator>
    constexpr Vector& append(const Vector<InT, InOtherAllocator>& other) noexcept
    {
        return append(other.data(), other.size());
    }

    /// @brief Appends all elements from a fixed-size `gp::Array`.
    /// @tparam InN Number of elements in `arr`.
    /// @tparam InAlign Alignment of `arr`.
    /// @param[in] arr Source fixed array.
    /// @return Reference to `*this`.
    template <gp::USize InN, gp::USize InAlign>
    constexpr Vector& append(const Array<InT, InN, InAlign>& arr) noexcept
    {
        return append(arr.data(), arr.size());
    }

    /// @brief Appends all elements from an initializer list.
    /// @param[in] init Initializer list of elements to append.
    /// @return Reference to `*this`.
    constexpr Vector& append(const std::initializer_list<InT> init) noexcept
    {
        return append(init.begin(), init.size());
    }

    /// @brief Appends all elements from a `VectorView`.
    /// @tparam InOtherT View element type (`InT` or `const InT`).
    /// @param[in] view Source contiguous view.
    /// @return Reference to `*this`.
    template <typename InOtherT>
    requires(concepts::IsSameAs<std::remove_cv_t<InOtherT>, InT>)
    constexpr Vector& append(const VectorView<InOtherT>& view) noexcept
    {
        return append(view.data(), view.size());
    }

    /// @brief Appends all elements from a `StridedView`.
    /// @tparam InOtherT Strided view element type (`InT` or `const InT`).
    /// @param[in] strided Source strided view.
    /// @return Reference to `*this`.
    template <typename InOtherT>
    requires(concepts::IsSameAs<std::remove_cv_t<InOtherT>, InT>)
    constexpr Vector& append(const StridedView<InOtherT>& strided) noexcept
    {
        if (strided.isEmpty())
        {
            return *this;
        }

        if (strided.isContiguous())
        {
            return append(strided.data(), strided.size());
        }

        if ((m_size + strided.size()) > m_capacity)
        {
            growForAdditional(strided.size());
        }

        for (SizeType i = 0u; i < strided.size(); ++i)
        {
            constructAtSlot(m_data + m_size + i, strided[i]);
        }
        m_size += strided.size();
        return *this;
    }

    /// @brief Inserts a copy of `value` before `pos` (self-aliasing safe).
    /// @param[in] pos Iterator at which to insert (`begin() <= pos <= end()`).
    /// @param[in] value Element to copy-insert.
    /// @return Iterator pointing to the inserted element.
    constexpr Iterator insert(const ConstIterator pos, const InT& value) noexcept
    {
        GP_ASSERT(pos >= cbegin() && pos <= cend(), "Vector::insert iterator out of bounds.");
        const SizeType index = static_cast<SizeType>(pos - cbegin());
        if (isPointerInCurrentBuffer(&value)) [[unlikely]]
        {
            InT valueCopy(value);
            return insertAtImpl(index, std::move(valueCopy));
        }
        return insertCountValueImpl(index, 1u, value);
    }

    /// @brief Inserts `value` before `pos` by moving (self-aliasing safe).
    /// @param[in] pos Iterator at which to insert (`begin() <= pos <= end()`).
    /// @param[in,out] value Element to move-insert.
    /// @return Iterator pointing to the inserted element.
    constexpr Iterator insert(const ConstIterator pos, InT&& value) noexcept
    {
        GP_ASSERT(pos >= cbegin() && pos <= cend(), "Vector::insert rvalue iterator out of bounds.");
        const SizeType index = static_cast<SizeType>(pos - cbegin());
        if (isPointerInCurrentBuffer(&value)) [[unlikely]]
        {
            InT valueCopy(std::move(value));
            return insertAtImpl(index, std::move(valueCopy));
        }
        return insertAtImpl(index, std::move(value));
    }

    /// @brief Inserts `count` copies of `value` before `pos` (self-aliasing safe).
    /// @param[in] pos Iterator at which to insert.
    /// @param[in] count Number of copies to insert.
    /// @param[in] value Prototype value to insert.
    /// @return Iterator pointing to the first inserted element (or `pos` if `count == 0`).
    constexpr Iterator insert(const ConstIterator pos, const SizeType count, const InT& value) noexcept
    {
        GP_ASSERT(pos >= cbegin() && pos <= cend(), "Vector::insert count iterator out of bounds.");
        const SizeType index = static_cast<SizeType>(pos - cbegin());
        if (count == 0u)
        {
            return m_data + index;
        }

        if (isPointerInCurrentBuffer(&value)) [[unlikely]]
        {
            const InT valueCopy(value);
            return insertCountValueImpl(index, count, valueCopy);
        }
        return insertCountValueImpl(index, count, value);
    }

    /// @brief Inserts `count` elements from `source` before `pos` with zero temporary heap allocations (self-aliasing
    ///        safe).
    /// @param[in] pos Iterator at which to insert.
    /// @param[in] source Pointer to the source element array.
    /// @param[in] count Number of elements to insert.
    /// @return Iterator pointing to the first inserted element.
    constexpr Iterator insert(const ConstIterator pos, const InT* const source, const SizeType count) noexcept
    {
        GP_ASSERT(pos >= cbegin() && pos <= cend(), "Vector::insert pointer range iterator out of bounds.");
        const SizeType index = static_cast<SizeType>(pos - cbegin());
        if (count == 0u || source == nullptr)
        {
            return m_data + index;
        }

        if (isRangeOverlappingCurrentBuffer(source, count)) [[unlikely]]
        {
            const SizeType srcOffset = static_cast<SizeType>(source - m_data);
            return insertAliasingRangeImpl(index, srcOffset, count);
        }

        return insertRangeImpl(index, source, count);
    }

    /// @brief Inserts elements from `[first, last)` before `pos` without temporary heap allocations for forward
    ///        iterators.
    /// @tparam InInputIt Input iterator yielding elements convertible to `InT`.
    /// @param[in] pos Iterator at which to insert.
    /// @param[in] first Beginning of the source range.
    /// @param[in] last End of the source range.
    /// @return Iterator pointing to the first inserted element.
    template <typename InInputIt>
    requires(!std::is_integral_v<InInputIt>)
    constexpr Iterator insert(const ConstIterator pos, InInputIt first, InInputIt last) noexcept
    {
        GP_ASSERT(pos >= cbegin() && pos <= cend(), "Vector::insert iterator range out of bounds.");
        const SizeType index = static_cast<SizeType>(pos - cbegin());
        if (first == last)
        {
            return m_data + index;
        }

        if constexpr (std::contiguous_iterator<InInputIt> &&
                      concepts::IsSameAs<std::remove_cvref_t<std::iter_value_t<InInputIt>>, InT>)
        {
            const SizeType count = static_cast<SizeType>(std::distance(first, last));
            return insert(pos, std::to_address(first), count);
        }
        else if constexpr (std::forward_iterator<InInputIt>)
        {
            const SizeType count = static_cast<SizeType>(std::distance(first, last));
            openGapForInsert(index, count);
            for (SizeType i = 0u; i < count; ++i, ++first)
            {
                constructAtSlot(m_data + index + i, *first);
            }
            m_size += count;
            return m_data + index;
        }
        else
        {
            SizeType currentIdx = index;
            for (; first != last; ++first, ++currentIdx)
            {
                emplaceAt(currentIdx, *first);
            }
            return m_data + index;
        }
    }

    /// @brief Inserts elements from an initializer list before `pos`.
    /// @param[in] pos Iterator at which to insert.
    /// @param[in] init Initializer list of elements.
    /// @return Iterator pointing to the first inserted element.
    constexpr Iterator insert(const ConstIterator pos, const std::initializer_list<InT> init) noexcept
    {
        return insert(pos, init.begin(), init.size());
    }

    /// @brief Inserts elements from a `VectorView` before `pos`.
    /// @tparam InOtherT View element type (`InT` or `const InT`).
    /// @param[in] pos Iterator at which to insert.
    /// @param[in] view Source contiguous view.
    /// @return Iterator pointing to the first inserted element.
    template <typename InOtherT>
    requires(concepts::IsSameAs<std::remove_cv_t<InOtherT>, InT>)
    constexpr Iterator insert(const ConstIterator pos, const VectorView<InOtherT>& view) noexcept
    {
        return insert(pos, view.data(), view.size());
    }

    /// @brief Inserts `count` default-constructed elements (`no-op` for trivial types) at `index`.
    /// @param[in] index Target insertion index (`index <= size()`).
    /// @param[in] count Number of default-constructed elements to insert.
    /// @return Iterator pointing to `m_data + index`.
    constexpr Iterator insertDefaulted(const SizeType index, const SizeType count = 1u) noexcept
    {
        GP_ASSERT(index <= m_size, "Vector::insertDefaulted index out of bounds.");
        if (count > 0u)
        {
            openGapForInsert(index, count);
            defaultConstructRange(m_data + index, count);
            m_size += count;
        }
        return m_data + index;
    }

    /// @brief Inserts `count` uninitialized element slots at `index`.
    /// @param[in] index Target insertion index (`index <= size()`).
    /// @param[in] count Number of uninitialized slots to insert.
    /// @return Iterator pointing to `m_data + index`.
    constexpr Iterator insertUninitialized(const SizeType index, const SizeType count = 1u) noexcept
    {
        GP_ASSERT(index <= m_size, "Vector::insertUninitialized index out of bounds.");
        if (count > 0u)
        {
            openGapForInsert(index, count);
            if (std::is_constant_evaluated())
            {
                valueConstructRange(m_data + index, count);
            }
            m_size += count;
        }
        return m_data + index;
    }

    /// @brief Inserts `count` zeroed and default-constructed elements at `index`.
    /// @param[in] index Target insertion index (`index <= size()`).
    /// @param[in] count Number of zeroed elements to insert.
    /// @return Iterator pointing to `m_data + index`.
    constexpr Iterator insertZeroed(const SizeType index, const SizeType count = 1u) noexcept
    {
        GP_ASSERT(index <= m_size, "Vector::insertZeroed index out of bounds.");
        if (count > 0u)
        {
            openGapForInsert(index, count);
            zeroAndDefaultConstructRange(m_data + index, count);
            m_size += count;
        }
        return m_data + index;
    }

    /// @brief Constructs an element in-place before `pos`.
    /// @tparam InArgs Constructor argument types.
    /// @param[in] pos Iterator at which to emplace.
    /// @param[in] args Arguments forwarded to `InT`'s constructor.
    /// @return Iterator pointing to the newly emplaced element.
    template <typename... InArgs>
    constexpr Iterator emplace(const ConstIterator pos, InArgs&&... args) noexcept
    {
        GP_ASSERT(pos >= cbegin() && pos <= cend(), "Vector::emplace iterator out of bounds.");
        const SizeType index = static_cast<SizeType>(pos - cbegin());
        if (index == m_size)
        {
            emplaceBack(std::forward<InArgs>(args)...);
            return m_data + (m_size - 1u);
        }

        InT stagedValue(std::forward<InArgs>(args)...);
        return insertAtImpl(index, std::move(stagedValue));
    }

    /// @brief Inserts a copy of `value` at zero-based `index`.
    /// @param[in] index Target insertion index (`index <= size()`).
    /// @param[in] value Element to copy-insert.
    /// @return Reference to the inserted element.
    constexpr Reference insertAt(const SizeType index, const InT& value) noexcept
    {
        GP_ASSERT(index <= m_size, "Vector::insertAt index out of bounds.");
        return *insert(cbegin() + index, value);
    }

    /// @brief Inserts `value` at zero-based `index` by moving.
    /// @param[in] index Target insertion index (`index <= size()`).
    /// @param[in,out] value Element to move-insert.
    /// @return Reference to the inserted element.
    constexpr Reference insertAt(const SizeType index, InT&& value) noexcept
    {
        GP_ASSERT(index <= m_size, "Vector::insertAt rvalue index out of bounds.");
        return *insert(cbegin() + index, std::move(value));
    }

    /// @brief Constructs an element in-place at zero-based `index`.
    /// @tparam InArgs Constructor argument types.
    /// @param[in] index Target insertion index (`index <= size()`).
    /// @param[in] args Arguments forwarded to `InT`'s constructor.
    /// @return Reference to the newly emplaced element.
    template <typename... InArgs>
    constexpr Reference emplaceAt(const SizeType index, InArgs&&... args) noexcept
    {
        GP_ASSERT(index <= m_size, "Vector::emplaceAt index out of bounds.");
        return *emplace(cbegin() + index, std::forward<InArgs>(args)...);
    }

public:
    /// @brief Ordered removal of the element at `pos` (shifts subsequent elements left in O(N) time).
    /// @param[in] pos Iterator to the element to remove (`begin() <= pos < end()`).
    /// @return Iterator following the removed element.
    constexpr Iterator erase(const ConstIterator pos) noexcept
    {
        GP_ASSERT(pos >= cbegin() && pos < cend(), "Vector::erase iterator out of bounds.");
        const SizeType index = static_cast<SizeType>(pos - cbegin());
        removeAt(index, 1u);
        return m_data + index;
    }

    /// @brief Ordered removal of the range `[first, last)` (shifts subsequent elements left in O(N) time).
    /// @param[in] first Beginning of the range to remove.
    /// @param[in] last End of the range to remove.
    /// @return Iterator following the last removed element.
    constexpr Iterator erase(const ConstIterator first, const ConstIterator last) noexcept
    {
        GP_ASSERT(first >= cbegin() && first <= last && last <= cend(), "Vector::erase range out of bounds.");
        const SizeType index = static_cast<SizeType>(first - cbegin());
        const SizeType count = static_cast<SizeType>(last - first);
        if (count > 0u)
        {
            removeAt(index, count);
        }
        return m_data + index;
    }

    /// @brief Ordered removal of `count` elements starting at `index` (shifts trailing elements left in O(N) time).
    /// @param[in] index Starting index (`index + count <= size()`).
    /// @param[in] count Number of elements to remove.
    constexpr void removeAt(const SizeType index, const SizeType count = 1u) noexcept
    {
        if (count == 0u)
        {
            return;
        }

        GP_ASSERT(index <= m_size && count <= (m_size - index), "Vector::removeAt range out of bounds.");
        const SizeType tailCount = m_size - (index + count);

        destructRange(m_data + index, count);
        if (tailCount > 0u)
        {
            relocateRange(m_data + index, m_data + index + count, tailCount);
        }
        m_size -= count;
    }

    /// @brief Unordered O(1) swap-erase of the element at `pos` (relocates the last element into `pos`).
    /// @note Does not preserve relative element order.
    /// @param[in] pos Iterator to the element to remove (`begin() <= pos < end()`).
    /// @return Iterator to the slot that was erased (now holding the former tail element, or `end()`).
    constexpr Iterator eraseSwap(const ConstIterator pos) noexcept
    {
        GP_ASSERT(pos >= cbegin() && pos < cend(), "Vector::eraseSwap iterator out of bounds.");
        const SizeType index = static_cast<SizeType>(pos - cbegin());
        removeAtSwap(index, 1u);
        return m_data + index;
    }

    /// @brief Unordered O(count) swap-erase of `count` elements starting at `index`.
    /// @details Destructs the target range `[index, index + count)` and relocates up to `count` elements from the tail
    ///          of the vector into the vacated slots without shifting intermediate elements.
    /// @param[in] index Starting index (`index + count <= size()`).
    /// @param[in] count Number of elements to remove.
    constexpr void removeAtSwap(const SizeType index, const SizeType count = 1u) noexcept
    {
        if (count == 0u)
        {
            return;
        }

        GP_ASSERT(index <= m_size && count <= (m_size - index), "Vector::removeAtSwap range out of bounds.");
        destructRange(m_data + index, count);

        const SizeType tailAvailable = m_size - (index + count);
        const SizeType numToMoveFromTail = tailAvailable < count ? tailAvailable : count;
        if (numToMoveFromTail > 0u)
        {
            relocateRange(m_data + index, m_data + (m_size - numToMoveFromTail), numToMoveFromTail);
        }

        m_size -= count;
    }

    /// @brief Removes the first occurrence of `value` while preserving element order.
    /// @param[in] value Value to search for and remove.
    /// @return True if an element was removed, false if `value` was not found.
    constexpr bool removeFirst(const InT& value) noexcept
    {
        const SizeType idx = indexOf(value);
        if (idx == npos)
        {
            return false;
        }
        removeAt(idx, 1u);
        return true;
    }

    /// @brief Removes the first occurrence of `value` in O(1) after lookup using swap-erase.
    /// @param[in] value Value to search for and swap-remove.
    /// @return True if an element was removed, false if `value` was not found.
    constexpr bool removeFirstSwap(const InT& value) noexcept
    {
        const SizeType idx = indexOf(value);
        if (idx == npos)
        {
            return false;
        }
        removeAtSwap(idx, 1u);
        return true;
    }

    /// @brief Removes all occurrences of `value` while preserving relative element order.
    /// @param[in] value Value to remove.
    /// @return Number of elements removed.
    constexpr SizeType removeAll(const InT& value) noexcept
    {
        const InT valueCopy(value);
        return removeAllIf(
            [&valueCopy](const InT& elem) -> bool
            {
                return elem == valueCopy;
            }
        );
    }

    /// @brief Removes all occurrences of `value` using O(1)-per-match unordered swap-erase.
    /// @param[in] value Value to remove.
    /// @return Number of elements removed.
    constexpr SizeType removeAllSwap(const InT& value) noexcept
    {
        const InT valueCopy(value);
        return removeAllIfSwap(
            [&valueCopy](const InT& elem) -> bool
            {
                return elem == valueCopy;
            }
        );
    }

    /// @brief Removes the first element satisfying `predicate` while preserving order.
    /// @tparam InPredicate Unary predicate `(const InT&) -> bool`.
    /// @param[in] predicate Condition to match.
    /// @return True if an element was removed, false otherwise.
    template <typename InPredicate>
    constexpr bool removeFirstIf(InPredicate predicate) noexcept
    {
        const SizeType idx = indexOfIf(predicate);
        if (idx == npos)
        {
            return false;
        }
        removeAt(idx, 1u);
        return true;
    }

    /// @brief Removes the first element satisfying `predicate` using O(1) swap-erase.
    /// @tparam InPredicate Unary predicate `(const InT&) -> bool`.
    /// @param[in] predicate Condition to match.
    /// @return True if an element was removed, false otherwise.
    template <typename InPredicate>
    constexpr bool removeFirstIfSwap(InPredicate predicate) noexcept
    {
        const SizeType idx = indexOfIf(predicate);
        if (idx == npos)
        {
            return false;
        }
        removeAtSwap(idx, 1u);
        return true;
    }

    /// @brief Removes all elements satisfying `predicate` in a single O(N) compaction pass while preserving order.
    /// @tparam InPredicate Unary predicate `(const InT&) -> bool`.
    /// @param[in] predicate Condition to match for removal.
    /// @return Number of elements removed.
    template <typename InPredicate>
    constexpr SizeType removeAllIf(InPredicate predicate) noexcept
    {
        SizeType writeIdx = 0u;
        for (SizeType readIdx = 0u; readIdx < m_size; ++readIdx)
        {
            if (!predicate(m_data[readIdx]))
            {
                if (writeIdx != readIdx)
                {
                    m_data[writeIdx] = std::move(m_data[readIdx]);
                }
                ++writeIdx;
            }
        }

        const SizeType removedCount = m_size - writeIdx;
        if (removedCount > 0u)
        {
            destructRange(m_data + writeIdx, removedCount);
            m_size = writeIdx;
        }
        return removedCount;
    }

    /// @brief Removes all elements satisfying `predicate` using unordered tail swap-erase.
    /// @tparam InPredicate Unary predicate `(const InT&) -> bool`.
    /// @param[in] predicate Condition to match for removal.
    /// @return Number of elements removed.
    template <typename InPredicate>
    constexpr SizeType removeAllIfSwap(InPredicate predicate) noexcept
    {
        SizeType removedCount = 0u;
        SizeType i = 0u;
        while (i < m_size)
        {
            if (predicate(m_data[i]))
            {
                removeAtSwap(i, 1u);
                ++removedCount;
            }
            else
            {
                ++i;
            }
        }
        return removedCount;
    }

public:
    /// @brief Finds the first occurrence of `value` in the vector.
    /// @param[in] value Element value to search for.
    /// @return Iterator to the first matching element, or `end()` if not found.
    [[nodiscard]] constexpr Iterator find(const InT& value) noexcept
    {
        for (SizeType i = 0u; i < m_size; ++i)
        {
            if (m_data[i] == value)
            {
                return m_data + i;
            }
        }
        return end();
    }

    /// @brief Finds the first occurrence of `value` in the vector (const overload).
    /// @param[in] value Element value to search for.
    /// @return Const iterator to the first matching element, or `end()` if not found.
    [[nodiscard]] constexpr ConstIterator find(const InT& value) const noexcept
    {
        for (SizeType i = 0u; i < m_size; ++i)
        {
            if (m_data[i] == value)
            {
                return m_data + i;
            }
        }
        return end();
    }

    /// @brief Finds the first element satisfying `predicate`.
    /// @tparam InPredicate Unary predicate `(const InT&) -> bool`.
    /// @param[in] predicate Search predicate.
    /// @return Iterator to the first matching element, or `end()` if not found.
    template <typename InPredicate>
    [[nodiscard]] constexpr Iterator findIf(InPredicate predicate) noexcept
    {
        for (SizeType i = 0u; i < m_size; ++i)
        {
            if (predicate(m_data[i]))
            {
                return m_data + i;
            }
        }
        return end();
    }

    /// @brief Finds the first element satisfying `predicate` (const overload).
    /// @tparam InPredicate Unary predicate `(const InT&) -> bool`.
    /// @param[in] predicate Search predicate.
    /// @return Const iterator to the first matching element, or `end()` if not found.
    template <typename InPredicate>
    [[nodiscard]] constexpr ConstIterator findIf(InPredicate predicate) const noexcept
    {
        for (SizeType i = 0u; i < m_size; ++i)
        {
            if (predicate(m_data[i]))
            {
                return m_data + i;
            }
        }
        return end();
    }

    /// @brief Finds the last occurrence of `value` in the vector.
    /// @param[in] value Element value to search for.
    /// @return Iterator to the last matching element, or `end()` if not found.
    [[nodiscard]] constexpr Iterator findLast(const InT& value) noexcept
    {
        for (SizeType i = m_size; i > 0u; --i)
        {
            if (m_data[i - 1u] == value)
            {
                return m_data + (i - 1u);
            }
        }
        return end();
    }

    /// @brief Finds the last occurrence of `value` in the vector (const overload).
    /// @param[in] value Element value to search for.
    /// @return Const iterator to the last matching element, or `end()` if not found.
    [[nodiscard]] constexpr ConstIterator findLast(const InT& value) const noexcept
    {
        for (SizeType i = m_size; i > 0u; --i)
        {
            if (m_data[i - 1u] == value)
            {
                return m_data + (i - 1u);
            }
        }
        return end();
    }

    /// @brief Finds the first index of `value` in the vector.
    /// @param[in] value Element value to search for.
    /// @return Zero-based index of the first match, or `npos` if not found.
    [[nodiscard]] constexpr SizeType indexOf(const InT& value) const noexcept
    {
        for (SizeType i = 0u; i < m_size; ++i)
        {
            if (m_data[i] == value)
            {
                return i;
            }
        }
        return npos;
    }

    /// @brief Finds the first index of an element satisfying `predicate`.
    /// @tparam InPredicate Unary predicate `(const InT&) -> bool`.
    /// @param[in] predicate Search predicate.
    /// @return Zero-based index of the first match, or `npos` if not found.
    template <typename InPredicate>
    [[nodiscard]] constexpr SizeType indexOfIf(InPredicate predicate) const noexcept
    {
        for (SizeType i = 0u; i < m_size; ++i)
        {
            if (predicate(m_data[i]))
            {
                return i;
            }
        }
        return npos;
    }

    /// @brief Finds the last index of `value` in the vector.
    /// @param[in] value Element value to search for.
    /// @return Zero-based index of the last match, or `npos` if not found.
    [[nodiscard]] constexpr SizeType lastIndexOf(const InT& value) const noexcept
    {
        for (SizeType i = m_size; i > 0u; --i)
        {
            if (m_data[i - 1u] == value)
            {
                return i - 1u;
            }
        }
        return npos;
    }

    /// @brief Finds the last index of an element satisfying `predicate`.
    /// @tparam InPredicate Unary predicate `(const InT&) -> bool`.
    /// @param[in] predicate Search predicate.
    /// @return Zero-based index of the last match, or `npos` if not found.
    template <typename InPredicate>
    [[nodiscard]] constexpr SizeType lastIndexOfIf(InPredicate predicate) const noexcept
    {
        for (SizeType i = m_size; i > 0u; --i)
        {
            if (predicate(m_data[i - 1u]))
            {
                return i - 1u;
            }
        }
        return npos;
    }

    /// @brief Checks whether the vector contains at least one element equal to `value`.
    /// @param[in] value Element value to search for.
    /// @return True if found, false otherwise.
    [[nodiscard]] constexpr bool contains(const InT& value) const noexcept
    {
        return indexOf(value) != npos;
    }

    /// @brief Checks whether the vector contains at least one element satisfying `predicate`.
    /// @tparam InPredicate Unary predicate `(const InT&) -> bool`.
    /// @param[in] predicate Search predicate.
    /// @return True if a matching element exists, false otherwise.
    template <typename InPredicate>
    [[nodiscard]] constexpr bool containsIf(InPredicate predicate) const noexcept
    {
        return indexOfIf(predicate) != npos;
    }

    /// @brief Counts the number of elements equal to `value`.
    /// @param[in] value Element value to count.
    /// @return Number of matching elements.
    [[nodiscard]] constexpr SizeType count(const InT& value) const noexcept
    {
        SizeType matches = 0u;
        for (SizeType i = 0u; i < m_size; ++i)
        {
            if (m_data[i] == value)
            {
                ++matches;
            }
        }
        return matches;
    }

    /// @brief Counts the number of elements satisfying `predicate`.
    /// @tparam InPredicate Unary predicate `(const InT&) -> bool`.
    /// @param[in] predicate Search predicate.
    /// @return Number of matching elements.
    template <typename InPredicate>
    [[nodiscard]] constexpr SizeType countIf(InPredicate predicate) const noexcept
    {
        SizeType matches = 0u;
        for (SizeType i = 0u; i < m_size; ++i)
        {
            if (predicate(m_data[i]))
            {
                ++matches;
            }
        }
        return matches;
    }

public:
    /// @brief Assigns `value` to every live element in `[0, size())`.
    /// @param[in] value Value to fill with.
    constexpr void fill(const InT& value) noexcept
    {
        for (SizeType i = 0u; i < m_size; ++i)
        {
            m_data[i] = value;
        }
    }

    /// @brief Reverses the order of elements in the vector in place.
    constexpr void reverse() noexcept
    {
        if (m_size > 1u)
        {
            std::ranges::reverse(m_data, m_data + m_size);
        }
    }

    /// @brief Sorts the elements in ascending order using `operator<`.
    constexpr void sort() noexcept
    {
        if (m_size > 1u)
        {
            std::ranges::sort(m_data, m_data + m_size);
        }
    }

    /// @brief Sorts the elements according to comparator `comp`.
    /// @tparam InCompare Strict weak ordering comparator `(const InT&, const InT&) -> bool`.
    /// @param[in] comp Comparator functor.
    template <typename InCompare>
    constexpr void sort(InCompare comp) noexcept
    {
        if (m_size > 1u)
        {
            std::ranges::sort(m_data, m_data + m_size, comp);
        }
    }

    /// @brief Returns the index of the first element `>= value` in a sorted vector, or `size()` if all are smaller.
    /// @param[in] value Value to search for.
    /// @return Lower-bound insertion index in `[0, size()]`.
    [[nodiscard]] constexpr SizeType lowerBound(const InT& value) const noexcept
    {
        const ConstIterator it = std::ranges::lower_bound(cbegin(), cend(), value);
        return static_cast<SizeType>(it - cbegin());
    }

    /// @brief Returns the index of the first element `> value` in a sorted vector, or `size()` if none are greater.
    /// @param[in] value Value to search for.
    /// @return Upper-bound insertion index in `[0, size()]`.
    [[nodiscard]] constexpr SizeType upperBound(const InT& value) const noexcept
    {
        const ConstIterator it = std::ranges::upper_bound(cbegin(), cend(), value);
        return static_cast<SizeType>(it - cbegin());
    }

    /// @brief Performs O(log N) binary search for `value` in a sorted vector.
    /// @param[in] value Value to search for.
    /// @return Index of a matching element, or `npos` if not found.
    [[nodiscard]] constexpr SizeType binarySearch(const InT& value) const noexcept
    {
        const SizeType idx = lowerBound(value);
        if (idx < m_size && (m_data[idx] == value))
        {
            return idx;
        }
        return npos;
    }

    /// @brief Removes consecutive duplicate elements in O(N) time (similar to `std::unique` + `erase`).
    /// @return Number of duplicate elements removed.
    constexpr SizeType deduplicate() noexcept
    {
        if (m_size <= 1u)
        {
            return 0u;
        }

        SizeType writeIdx = 1u;
        for (SizeType readIdx = 1u; readIdx < m_size; ++readIdx)
        {
            if (!(m_data[readIdx] == m_data[writeIdx - 1u]))
            {
                if (writeIdx != readIdx)
                {
                    m_data[writeIdx] = std::move(m_data[readIdx]);
                }
                ++writeIdx;
            }
        }

        const SizeType removed = m_size - writeIdx;
        if (removed > 0u)
        {
            destructRange(m_data + writeIdx, removed);
            m_size = writeIdx;
        }
        return removed;
    }

    /// @brief Sorts the vector in ascending order and removes all duplicate elements.
    /// @return Number of duplicate elements removed.
    constexpr SizeType sortAndDeduplicate() noexcept
    {
        sort();
        return deduplicate();
    }

public:
    /// @brief Returns a mutable `VectorView<InT>` spanning all elements in this vector.
    /// @return `VectorView<InT>(m_data, m_size)`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr ViewType view() noexcept
    {
        return ViewType(m_data, m_size);
    }

    /// @brief Returns a read-only `ConstVectorView<InT>` spanning all elements in this vector.
    /// @return `ConstVectorView<InT>(m_data, m_size)`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr ConstViewType view() const noexcept
    {
        return ConstViewType(m_data, m_size);
    }

    /// @brief Returns a read-only `ConstVectorView<InT>` spanning all elements in this vector.
    /// @return `ConstVectorView<InT>(m_data, m_size)`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr ConstViewType constView() const noexcept
    {
        return ConstViewType(m_data, m_size);
    }

    /// @brief Returns a mutable `VectorView<InT>` over `[offset, offset + count)`.
    /// @param[in] offset Starting index (`offset <= size()`).
    /// @param[in] count Number of elements (defaults to `npos`, meaning until the end).
    /// @return Mutable sub-view.
    [[nodiscard]] constexpr ViewType slice(const SizeType offset, const SizeType count = npos) noexcept
    {
        return view().slice(offset, count);
    }

    /// @brief Returns a read-only `ConstVectorView<InT>` over `[offset, offset + count)`.
    /// @param[in] offset Starting index (`offset <= size()`).
    /// @param[in] count Number of elements (defaults to `npos`, meaning until the end).
    /// @return Read-only sub-view.
    [[nodiscard]] constexpr ConstViewType slice(const SizeType offset, const SizeType count = npos) const noexcept
    {
        return constView().slice(offset, count);
    }

    /// @brief Returns a mutable `VectorView<InT>` of the first `count` elements.
    /// @param[in] count Number of leading elements (`count <= size()`).
    /// @return Mutable sub-view `[0, count)`.
    [[nodiscard]] constexpr ViewType first(const SizeType count) noexcept
    {
        return view().first(count);
    }

    /// @brief Returns a read-only `ConstVectorView<InT>` of the first `count` elements.
    /// @param[in] count Number of leading elements (`count <= size()`).
    /// @return Read-only sub-view `[0, count)`.
    [[nodiscard]] constexpr ConstViewType first(const SizeType count) const noexcept
    {
        return constView().first(count);
    }

    /// @brief Returns a mutable `VectorView<InT>` of the last `count` elements.
    /// @param[in] count Number of trailing elements (`count <= size()`).
    /// @return Mutable sub-view `[size() - count, size())`.
    [[nodiscard]] constexpr ViewType lastSlice(const SizeType count) noexcept
    {
        return view().last(count);
    }

    /// @brief Returns a read-only `ConstVectorView<InT>` of the last `count` elements.
    /// @param[in] count Number of trailing elements (`count <= size()`).
    /// @return Read-only sub-view `[size() - count, size())`.
    [[nodiscard]] constexpr ConstViewType lastSlice(const SizeType count) const noexcept
    {
        return constView().last(count);
    }

    /// @brief Returns a read-only byte view (`VectorView<const gp::UInt8>`) over the live elements.
    /// @return Read-only byte view of length `sizeInBytes()`.
    [[nodiscard]] VectorView<const gp::UInt8> asBytes() const noexcept
    {
        return constView().asBytes();
    }

    /// @brief Returns a writable byte view (`VectorView<gp::UInt8>`) over the live elements.
    /// @return Writable byte view of length `sizeInBytes()`.
    [[nodiscard]] VectorView<gp::UInt8> asWritableBytes() noexcept
    {
        return view().asWritableBytes();
    }

    /// @brief Returns a mutable `StridedView<InT>` over this vector (`stride() == sizeof(InT)`).
    /// @return Mutable `StridedView<InT>`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr StridedViewType stridedView() noexcept
    {
        return StridedViewType(m_data, m_size);
    }

    /// @brief Returns a read-only `ConstStridedView<InT>` over this vector (`stride() == sizeof(InT)`).
    /// @return Read-only `ConstStridedView<InT>`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr ConstStridedViewType stridedView() const noexcept
    {
        return ConstStridedViewType(m_data, m_size);
    }

    /// @brief Returns a read-only `ConstStridedView<InT>` over this vector (`stride() == sizeof(InT)`).
    /// @return Read-only `ConstStridedView<InT>`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr ConstStridedViewType constStridedView() const noexcept
    {
        return ConstStridedViewType(m_data, m_size);
    }

    /// @brief Projects this vector onto a data member `memberPtr` of `InT` (mutable AoS field view).
    /// @tparam InMember Member field type.
    /// @tparam InStructBase Struct or base class owning `memberPtr`.
    /// @param[in] memberPtr Pointer-to-data-member `&InStructBase::field`.
    /// @return Mutable `StridedView<InMember>` over the projected field.
    template <typename InMember, typename InStructBase>
    requires(std::is_base_of_v<InStructBase, InT>)
    [[nodiscard]] constexpr StridedView<InMember> project(InMember InStructBase::* const memberPtr) noexcept
    {
        return stridedView().project(memberPtr);
    }

    /// @brief Projects this vector onto a data member `memberPtr` of `InT` (read-only AoS field view).
    /// @tparam InMember Member field type.
    /// @tparam InStructBase Struct or base class owning `memberPtr`.
    /// @param[in] memberPtr Pointer-to-data-member `&InStructBase::field`.
    /// @return Read-only `StridedView<const InMember>` over the projected field.
    template <typename InMember, typename InStructBase>
    requires(std::is_base_of_v<InStructBase, InT>)
    [[nodiscard]] constexpr StridedView<const InMember>
        project(InMember InStructBase::* const memberPtr) const noexcept
    {
        return constStridedView().project(memberPtr);
    }

    /// @brief Swaps the contents of this vector with `other`.
    /// @details Exchanges pointers in O(1) when allocators share ownership (including heap-spilled `InlineVector`s);
    ///          otherwise performs a safe element relocation across the two allocators.
    /// @param[in,out] other Vector to swap with.
    constexpr void swap(Vector& other) noexcept
    {
        if (this == &other)
        {
            return;
        }

        const bool canSwapPointersDirectly =
            ((m_allocator == other.m_allocator) && (kInlineElementCapacity == 0u)) ||
            (canStealFromNonMovableAllocator(*this) && canStealFromNonMovableAllocator(other));

        if (canSwapPointersDirectly)
        {
            Pointer tempData = m_data;
            m_data = other.m_data;
            other.m_data = tempData;

            SizeType tempSize = m_size;
            m_size = other.m_size;
            other.m_size = tempSize;

            SizeType tempCap = m_capacity;
            m_capacity = other.m_capacity;
            other.m_capacity = tempCap;
            return;
        }

        Vector temp(std::move(*this));
        *this = std::move(other);
        other = std::move(temp);
    }

public:
    /// @brief Equality comparison across vectors with any allocator type.
    /// @tparam InOtherAllocator Right-hand vector's allocator type.
    /// @param[in] rhs Vector to compare against.
    /// @return True if both vectors have the same size and equal elements, false otherwise.
    template <concepts::IsAllocator InOtherAllocator>
    [[nodiscard]] constexpr bool operator==(const Vector<InT, InOtherAllocator>& rhs) const noexcept
    {
        if (m_size != rhs.m_size)
        {
            return false;
        }

        for (SizeType i = 0u; i < m_size; ++i)
        {
            if (!(m_data[i] == rhs.m_data[i]))
            {
                return false;
            }
        }
        return true;
    }

    /// @brief Lexicographical three-way comparison across vectors with any allocator type.
    /// @tparam InOtherAllocator Right-hand vector's allocator type.
    /// @param[in] rhs Vector to compare against.
    /// @return Three-way comparison ordering result.
    template <concepts::IsAllocator InOtherAllocator>
    [[nodiscard]] constexpr auto operator<=>(const Vector<InT, InOtherAllocator>& rhs) const noexcept
        requires std::three_way_comparable<InT>
    {
        return std::lexicographical_compare_three_way(begin(), end(), rhs.begin(), rhs.end());
    }

private:
    /// @brief Checks whether the address of `arg` lies inside the raw byte range `[m_data, m_data + m_capacity)`.
    /// @tparam InArg Argument type.
    /// @param[in] arg Reference to the argument to inspect.
    /// @return True if `std::addressof(arg)` aliases the current buffer.
    template <typename InArg>
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool isAddressInCurrentBuffer(const InArg& arg) const noexcept
    {
        if (std::is_constant_evaluated())
        {
            if constexpr (concepts::IsSameAs<std::remove_cvref_t<InArg>, InT>)
            {
                return isPointerInCurrentBuffer(std::addressof(arg));
            }
            else
            {
                return false;
            }
        }

        if (m_data == nullptr)
        {
            return false;
        }

        const gp::UIntPtr addr = reinterpret_cast<gp::UIntPtr>(std::addressof(arg));
        const gp::UIntPtr beginAddr = reinterpret_cast<gp::UIntPtr>(m_data);
        const gp::UIntPtr endAddr = reinterpret_cast<gp::UIntPtr>(m_data + m_capacity);
        return addr >= beginAddr && addr < endAddr;
    }

    /// @brief Checks whether `ptr` points inside the currently allocated `[m_data, m_data + m_capacity)` buffer.
    /// @param[in] ptr Pointer to test.
    /// @return True if `ptr` aliases the current buffer.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool isPointerInCurrentBuffer(const InT* const ptr) const noexcept
    {
        if (std::is_constant_evaluated())
        {
            for (SizeType i = 0u; i < m_size; ++i)
            {
                if (ptr == (m_data + i))
                {
                    return true;
                }
            }
            return false;
        }

        if (m_data == nullptr || ptr == nullptr)
        {
            return false;
        }

        const gp::UIntPtr addr = reinterpret_cast<gp::UIntPtr>(ptr);
        const gp::UIntPtr beginAddr = reinterpret_cast<gp::UIntPtr>(m_data);
        const gp::UIntPtr endAddr = reinterpret_cast<gp::UIntPtr>(m_data + m_capacity);
        return addr >= beginAddr && addr < endAddr;
    }

    /// @brief Checks whether `[source, source + count)` overlaps `[m_data, m_data + m_capacity)`.
    /// @param[in] source Start of source range.
    /// @param[in] count Number of elements in source range.
    /// @return True if the ranges overlap.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool
        isRangeOverlappingCurrentBuffer(const InT* const source, const SizeType count) const noexcept
    {
        if (m_data == nullptr || source == nullptr || count == 0u)
        {
            return false;
        }

        if (std::is_constant_evaluated())
        {
            for (SizeType i = 0u; i < m_size; ++i)
            {
                for (SizeType j = 0u; j < count; ++j)
                {
                    if ((m_data + i) == (source + j))
                    {
                        return true;
                    }
                }
            }
            return false;
        }

        const gp::UIntPtr srcBegin = reinterpret_cast<gp::UIntPtr>(source);
        const gp::UIntPtr srcEnd = reinterpret_cast<gp::UIntPtr>(source + count);
        const gp::UIntPtr bufBegin = reinterpret_cast<gp::UIntPtr>(m_data);
        const gp::UIntPtr bufEnd = reinterpret_cast<gp::UIntPtr>(m_data + m_capacity);
        return srcBegin < bufEnd && bufBegin < srcEnd;
    }

    /// @brief Clamps a positive capacity up to `kInlineElementCapacity` for SBO allocators.
    /// @param[in] requestedCap Requested element capacity.
    /// @return Effective element capacity.
    [[nodiscard]] static GP_FORCEINLINE_HINT constexpr SizeType
        clampToInlineCapacity(const SizeType requestedCap) noexcept
    {
        if constexpr (kInlineElementCapacity > 0u)
        {
            if (requestedCap > 0u && requestedCap < kInlineElementCapacity)
            {
                return kInlineElementCapacity;
            }
        }
        return requestedCap;
    }

    /// @brief Computes quantized reservation capacity via `gp::memory::calculateSlackReserve`.
    /// @param[in] minCapacity Minimum required capacity in elements.
    /// @return Target capacity in elements.
    [[nodiscard]] constexpr SizeType computeReserveCapacity(const SizeType minCapacity) const noexcept
    {
        const SizeType clampedMin = clampToInlineCapacity(minCapacity);
        if (std::is_constant_evaluated())
        {
            return clampedMin;
        }

        if constexpr (kInlineElementCapacity > 0u)
        {
            if (clampedMin <= kInlineElementCapacity)
            {
                return kInlineElementCapacity;
            }
        }

        return gp::memory::calculateSlackReserve<SizeType>(
            m_allocator, clampedMin, sizeof(InT), true, kAllocAlignment
        );
    }

    /// @brief Computes geometric growth capacity via `gp::memory::calculateSlackGrow`.
    /// @param[in] minCapacity Minimum required capacity in elements.
    /// @return Grown capacity in elements.
    [[nodiscard]] constexpr SizeType computeGrowCapacity(const SizeType minCapacity) const noexcept
    {
        const SizeType clampedMin = clampToInlineCapacity(minCapacity);
        if (std::is_constant_evaluated())
        {
            const SizeType doubled = m_capacity > 0u ? (m_capacity * 2u) : 4u;
            return doubled >= clampedMin ? doubled : clampedMin;
        }

        if constexpr (kInlineElementCapacity > 0u)
        {
            if (clampedMin <= kInlineElementCapacity)
            {
                return kInlineElementCapacity;
            }
        }

        return gp::memory::calculateSlackGrow<SizeType>(
            m_allocator, clampedMin, m_capacity, sizeof(InT), true, kAllocAlignment
        );
    }

    /// @brief Grows the vector buffer to accommodate at least `m_size + additionalCount` elements.
    /// @param[in] additionalCount Number of new elements to make room for.
    constexpr void growForAdditional(const SizeType additionalCount) noexcept
    {
        const SizeType newCapacity = computeGrowCapacity(m_size + additionalCount);
        reallocateBuffer(newCapacity);
    }

    /// @brief Attempts to allocate or resize the backing buffer to `newCapacity` elements without asserting on failure.
    /// @param[in] newCapacity Target capacity in elements (`newCapacity >= m_size`).
    /// @return True if the buffer was resized (or was already at `newCapacity`), false if allocation returned
    ///         `nullptr`.
    [[nodiscard]] constexpr bool tryReallocateBuffer(const SizeType newCapacity) noexcept
    {
        if (newCapacity == m_capacity)
        {
            return true;
        }

        if (newCapacity == 0u)
        {
            if (m_data != nullptr)
            {
                deallocateRaw(m_data, m_capacity);
                m_data = nullptr;
                m_capacity = 0u;
            }
            return true;
        }

        if (std::is_constant_evaluated())
        {
            std::allocator<InT> stdAlloc;
            Pointer newData = stdAlloc.allocate(newCapacity);
            for (SizeType i = 0u; i < m_size; ++i)
            {
                std::construct_at(newData + i, std::move(m_data[i]));
                std::destroy_at(m_data + i);
            }
            if (m_data != nullptr)
            {
                stdAlloc.deallocate(m_data, m_capacity);
            }
            m_data = newData;
            m_capacity = newCapacity;
            return true;
        }

        const gp::USize oldBytes = m_capacity * sizeof(InT);
        const gp::USize newBytes = newCapacity * sizeof(InT);

        if constexpr (concepts::IsRelocatable<InT>)
        {
            Pointer resizedData = static_cast<Pointer>(
                m_allocator.reallocate(m_data, oldBytes, newBytes, kAllocAlignment)
            );
            if (resizedData == nullptr) [[unlikely]]
            {
                return false;
            }
            m_data = resizedData;
            m_capacity = newCapacity;
            return true;
        }
        else
        {
            Pointer newData = static_cast<Pointer>(m_allocator.allocate(newBytes, kAllocAlignment));
            if (newData == nullptr) [[unlikely]]
            {
                return false;
            }
            if (m_size > 0u && m_data != nullptr)
            {
                gp::memory::relocateConstructItems<InT>(newData, m_data, m_size);
            }
            if (m_data != nullptr)
            {
                m_allocator.deallocate(m_data, oldBytes, kAllocAlignment);
            }
            m_data = newData;
            m_capacity = newCapacity;
            return true;
        }
    }

    /// @brief Allocates or resizes the backing buffer to `newCapacity` elements (ADR-0055 two-path reallocation).
    /// @param[in] newCapacity Target capacity in elements (`newCapacity >= m_size`).
    constexpr void reallocateBuffer(const SizeType newCapacity) noexcept
    {
        const bool ok = tryReallocateBuffer(newCapacity);
        GP_ASSERT(ok, "Vector::reallocateBuffer failed to allocate memory.");
        (void)ok;
    }

    /// @brief Deallocates `ptr` holding `cap` elements without invoking element destructors.
    /// @param[in] ptr Buffer pointer to free.
    /// @param[in] cap Capacity in elements of `ptr`.
    constexpr void deallocateRaw(Pointer ptr, const SizeType cap) noexcept
    {
        if (ptr == nullptr || cap == 0u)
        {
            return;
        }

        if (std::is_constant_evaluated())
        {
            std::allocator<InT> stdAlloc;
            stdAlloc.deallocate(ptr, cap);
            return;
        }

        m_allocator.deallocate(ptr, cap * sizeof(InT), kAllocAlignment);
    }

    /// @brief Checks whether `other` uses an `InlineAllocator` whose current buffer lives on a movable fallback heap.
    /// @param[in] other Source vector being moved from.
    /// @return True if `other.m_data` can be stolen in O(1) without copying.
    [[nodiscard]] constexpr bool canStealFromNonMovableAllocator(const Vector& other) const noexcept
    {
        if (std::is_constant_evaluated())
        {
            return false;
        }

        if constexpr (requires {
                          other.m_allocator.isInlinePointer(other.m_data);
                          m_allocator.getFallbackAllocator() == other.m_allocator.getFallbackAllocator();
                      })
        {
            return (other.m_data != nullptr) && !other.m_allocator.isInlinePointer(other.m_data) &&
                   (m_allocator.getFallbackAllocator() == other.m_allocator.getFallbackAllocator());
        }
        else
        {
            return false;
        }
    }

    /// @brief Relocates elements from `other` into `this->m_allocator` and frees `other`'s buffer.
    /// @param[in,out] other Source vector to drain.
    constexpr void relocateFromOther(Vector& other) noexcept
    {
        clear();
        if (other.m_size > 0u)
        {
            if (other.m_size > m_capacity)
            {
                reallocateBuffer(computeReserveCapacity(other.m_size));
            }
            relocateRange(m_data, other.m_data, other.m_size);
            m_size = other.m_size;
        }

        if (other.m_data != nullptr)
        {
            other.deallocateRaw(other.m_data, other.m_capacity);
            other.m_data = nullptr;
            other.m_size = 0u;
            other.m_capacity = 0u;
        }
    }

    /// @brief Helper for `assign(count, value)` once `value` is guaranteed not to alias `m_data`.
    /// @param[in] count Number of copies to assign.
    /// @param[in] value Non-aliasing prototype value.
    /// @return Reference to `*this`.
    constexpr Vector& assignNonAliasingValue(const SizeType count, const InT& value) noexcept
    {
        clear();
        if (count > m_capacity)
        {
            reallocateBuffer(computeReserveCapacity(count));
        }
        fillConstructRange(m_data, count, value);
        m_size = count;
        return *this;
    }

    /// @brief Inserts a single rvalue at `index`.
    /// @param[in] index Target index (`index <= m_size`).
    /// @param[in,out] value Value to move-insert.
    /// @return Iterator to `m_data + index`.
    constexpr Iterator insertAtImpl(const SizeType index, InT&& value) noexcept
    {
        if (index == m_size)
        {
            pushBack(std::move(value));
            return m_data + (m_size - 1u);
        }

        if (m_size == m_capacity)
        {
            growForAdditional(1u);
        }

        const SizeType tailCount = m_size - index;
        relocateRange(m_data + index + 1u, m_data + index, tailCount);
        constructAtSlot(m_data + index, std::move(value));
        ++m_size;
        return m_data + index;
    }

    /// @brief Opens an uninitialized gap of `count` elements starting at `index` by growing if needed and relocating
    ///        `[index, m_size)` to `index + count`.
    /// @param[in] index Gap start index (`index <= m_size`).
    /// @param[in] count Number of uninitialized slots to open.
    constexpr void openGapForInsert(const SizeType index, const SizeType count) noexcept
    {
        if ((m_size + count) > m_capacity)
        {
            growForAdditional(count);
        }

        const SizeType tailCount = m_size - index;
        if (tailCount > 0u)
        {
            relocateRange(m_data + index + count, m_data + index, tailCount);
        }
    }

    /// @brief Inserts `count` copies of non-aliasing `value` at `index`.
    /// @param[in] index Target index (`index <= m_size`).
    /// @param[in] count Number of copies to insert.
    /// @param[in] value Prototype value.
    /// @return Iterator to `m_data + index`.
    constexpr Iterator insertCountValueImpl(const SizeType index, const SizeType count, const InT& value) noexcept
    {
        openGapForInsert(index, count);
        fillConstructRange(m_data + index, count, value);
        m_size += count;
        return m_data + index;
    }

    /// @brief Inserts `count` elements from non-aliasing `source` at `index`.
    /// @param[in] index Target index (`index <= m_size`).
    /// @param[in] source Non-aliasing source array.
    /// @param[in] count Number of elements to copy-insert.
    /// @return Iterator to `m_data + index`.
    constexpr Iterator insertRangeImpl(const SizeType index, const InT* const source, const SizeType count) noexcept
    {
        openGapForInsert(index, count);
        copyConstructRange(m_data + index, source, count);
        m_size += count;
        return m_data + index;
    }

    /// @brief Inserts `count` elements from an overlapping sub-range `[srcOffset, srcOffset + count)` at `index`
    ///        with zero temporary heap allocations.
    /// @param[in] index Target insertion index (`index <= m_size`).
    /// @param[in] srcOffset Starting index of the source sub-range inside `m_data`.
    /// @param[in] count Number of elements to copy-insert.
    /// @return Iterator to `m_data + index`.
    constexpr Iterator
        insertAliasingRangeImpl(const SizeType index, const SizeType srcOffset, const SizeType count) noexcept
    {
        openGapForInsert(index, count);

        if ((srcOffset + count) <= index)
        {
            copyConstructRange(m_data + index, m_data + srcOffset, count);
        }
        else if (srcOffset >= index)
        {
            copyConstructRange(m_data + index, m_data + srcOffset + count, count);
        }
        else
        {
            const SizeType beforeGapCount = index - srcOffset;
            const SizeType afterGapCount = count - beforeGapCount;
            copyConstructRange(m_data + index, m_data + srcOffset, beforeGapCount);
            copyConstructRange(m_data + index + beforeGapCount, m_data + index + count, afterGapCount);
        }

        m_size += count;
        return m_data + index;
    }

    /// @brief Constructs a single element at `slot` with `args...`.
    /// @tparam InArgs Constructor argument types.
    /// @param[out] slot Uninitialized element slot.
    /// @param[in] args Constructor arguments.
    template <typename... InArgs>
    static GP_FORCEINLINE_HINT constexpr void constructAtSlot(Pointer slot, InArgs&&... args) noexcept
    {
        std::construct_at(slot, std::forward<InArgs>(args)...);
    }

    /// @brief Default-constructs `count` elements at `dest` (no-op at runtime for trivial types).
    /// @param[out] dest Uninitialized destination pointer.
    /// @param[in] count Number of elements.
    static GP_FORCEINLINE_HINT constexpr void defaultConstructRange(Pointer dest, const SizeType count) noexcept
    {
        if (count == 0u || dest == nullptr)
        {
            return;
        }

        if (std::is_constant_evaluated())
        {
            for (SizeType i = 0u; i < count; ++i)
            {
                std::construct_at(dest + i);
            }
            return;
        }

        gp::memory::defaultConstructItems<InT>(dest, count);
    }

    /// @brief Value-constructs `count` elements at `dest` (`zeroMemory` at runtime for trivial types).
    /// @param[out] dest Uninitialized destination pointer.
    /// @param[in] count Number of elements.
    static GP_FORCEINLINE_HINT constexpr void valueConstructRange(Pointer dest, const SizeType count) noexcept
    {
        if (count == 0u || dest == nullptr)
        {
            return;
        }

        if (std::is_constant_evaluated())
        {
            for (SizeType i = 0u; i < count; ++i)
            {
                std::construct_at(dest + i);
            }
            return;
        }

        gp::memory::valueConstructItems<InT>(dest, count);
    }

    /// @brief Zeroes raw storage and default-constructs non-trivial elements at `dest`.
    /// @param[out] dest Uninitialized destination pointer.
    /// @param[in] count Number of elements.
    static GP_FORCEINLINE_HINT constexpr void zeroAndDefaultConstructRange(Pointer dest, const SizeType count) noexcept
    {
        if (count == 0u || dest == nullptr)
        {
            return;
        }

        if (std::is_constant_evaluated())
        {
            for (SizeType i = 0u; i < count; ++i)
            {
                std::construct_at(dest + i);
            }
            return;
        }

        gp::memory::zeroMemory(dest, count * sizeof(InT));
        if constexpr (!concepts::IsTriviallyConstructible<InT>)
        {
            gp::memory::defaultConstructItems<InT>(dest, count);
        }
    }

    /// @brief Copy-constructs `count` elements at `dest` from `value`.
    /// @param[out] dest Uninitialized destination pointer.
    /// @param[in] count Number of elements.
    /// @param[in] value Prototype value.
    static GP_FORCEINLINE_HINT constexpr void
        fillConstructRange(Pointer dest, const SizeType count, const InT& value) noexcept
    {
        for (SizeType i = 0u; i < count; ++i)
        {
            std::construct_at(dest + i, value);
        }
    }

    /// @brief Copy-constructs `count` elements at `dest` from `source`.
    /// @param[out] dest Uninitialized destination pointer.
    /// @param[in] source Source array pointer.
    /// @param[in] count Number of elements.
    static GP_FORCEINLINE_HINT constexpr void
        copyConstructRange(Pointer dest, ConstPointer source, const SizeType count) noexcept
    {
        if (count == 0u || dest == nullptr || source == nullptr)
        {
            return;
        }

        if (std::is_constant_evaluated())
        {
            for (SizeType i = 0u; i < count; ++i)
            {
                std::construct_at(dest + i, source[i]);
            }
            return;
        }

        gp::memory::constructItems<InT>(dest, source, count);
    }

    /// @brief Destructively relocates `count` elements from `source` to `dest` (handles overlapping ranges).
    /// @param[out] dest Destination pointer.
    /// @param[in,out] source Source pointer.
    /// @param[in] count Number of elements to relocate.
    static GP_FORCEINLINE_HINT constexpr void
        relocateRange(Pointer dest, Pointer source, const SizeType count) noexcept
    {
        if (count == 0u || dest == nullptr || source == nullptr || dest == source)
        {
            return;
        }

        if (std::is_constant_evaluated())
        {
            if (dest > source && dest < (source + count))
            {
                for (SizeType i = count; i > 0u; --i)
                {
                    const SizeType idx = i - 1u;
                    std::construct_at(dest + idx, std::move(source[idx]));
                    std::destroy_at(source + idx);
                }
            }
            else
            {
                for (SizeType i = 0u; i < count; ++i)
                {
                    std::construct_at(dest + i, std::move(source[i]));
                    std::destroy_at(source + i);
                }
            }
            return;
        }

        gp::memory::relocateConstructItems<InT>(dest, source, count);
    }

    /// @brief Destructs `count` elements starting at `elements`.
    /// @param[in,out] elements Pointer to first element to destruct.
    /// @param[in] count Number of elements to destruct.
    static GP_FORCEINLINE_HINT constexpr void destructRange(Pointer elements, const SizeType count) noexcept
    {
        if (count == 0u || elements == nullptr)
        {
            return;
        }

        if (std::is_constant_evaluated())
        {
            for (SizeType i = 0u; i < count; ++i)
            {
                std::destroy_at(elements + i);
            }
            return;
        }

        gp::memory::destructItems(elements, count);
    }

private:
    Pointer m_data{ nullptr };
    SizeType m_size{ 0u };
    SizeType m_capacity{ 0u };
    GP_NO_UNIQUE_ADDRESS InAllocator m_allocator{};
};

/// @brief Swaps the contents of two `Vector` instances.
/// @tparam InT Element type.
/// @tparam InAllocator Allocator type.
/// @param[in,out] lhs First vector.
/// @param[in,out] rhs Second vector.
template <typename InT, concepts::IsAllocator InAllocator>
constexpr void swap(Vector<InT, InAllocator>& lhs, Vector<InT, InAllocator>& rhs) noexcept
{
    lhs.swap(rhs);
}

}   // namespace gp::container

namespace gp
{

using container::swap;

}   // namespace gp

namespace gp::detail
{

/// @brief Helper variable template to determine if a type is a `gp::Vector`.
template <typename InT>
constexpr bool IsVectorV = false;

/// @brief Specialization for `gp::container::Vector` (`gp::Vector`) types.
template <typename InT, typename InAllocator>
constexpr bool IsVectorV<gp::container::Vector<InT, InAllocator>> = true;

}   // namespace gp::detail

namespace gp::concepts
{

/// @brief `gp::Vector` instantiated with a stateless allocator is bitwise relocatable (ADR-0055).
template <typename InT, IsAllocator InAllocator>
requires IsStatelessAllocator<InAllocator>
constexpr bool IsRelocatableV<gp::container::Vector<InT, InAllocator>> = true;

/// @brief Concept to check if a type is any instantiation of `gp::Vector`.
/// @tparam InT The type to check.
template <typename InT>
concept IsVector = gp::detail::IsVectorV<std::remove_cvref_t<InT>>;

}   // namespace gp::concepts
