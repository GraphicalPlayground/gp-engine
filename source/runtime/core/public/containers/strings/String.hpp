// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Concepts.hpp"
#include "containers/ContainerForward.hpp"
#include "containers/strings/CharTraits.hpp"
#include "containers/strings/FixedString.hpp"
#include "containers/strings/StringEnums.hpp"
#include "containers/strings/Unicode.hpp"
#include "containers/views/StringView.hpp"
#include "CoreMinimal.hpp"
#include "memory/allocators/AllocatorConcepts.hpp"
#include "memory/allocators/HeapAllocator.hpp"
#include "memory/allocators/InlineAllocator.hpp"          // IWYU pragma: keep
#include "memory/allocators/PolymorphicAllocator.hpp"     // IWYU pragma: keep
#include "memory/allocators/ThreadScratchAllocator.hpp"   // IWYU pragma: keep
#include "memory/MemoryBase.hpp"
#include "memory/utils/AllocatorUtilities.hpp"
#include "platforms/base/Platform.hpp"
#include <charconv>
#include <compare>
#include <format>
#include <initializer_list>
#include <iterator>
#include <limits>   // IWYU pragma: keep
#include <ostream>
#include <string_view>
#include <type_traits>
#include <utility>

namespace gp::container
{

/// @brief Dynamic, allocator-aware string with a 24-byte Tagged-Union Small String Optimization (SSO) footprint.
/// @details Combines a 24-byte footprint (`22` `char` inline SSO capacity with zero heap allocations) with first-class
///          `gp::concepts::IsAllocator` support (`DefaultAllocator`, `LinearAllocator`, `StackAllocator`,
///          `InlineAllocator`, `ThreadScratchAllocator`, `TaggedAllocator`, `PolymorphicAllocator`).
///          Always maintains a trailing null terminator (`data()[size()] == InCharT{0}`) in both SSO and Heap modes.
/// @tparam InCharT The character type (`char`, `wchar_t`, `char8_t`, `char16_t`, or `char32_t`).
/// @tparam InAllocator Engine memory allocator satisfying `gp::concepts::IsAllocator`.
template <concepts::IsCharacter InCharT, typename InAllocator>
class BasicString
{
    static_assert(
        concepts::IsAllocator<InAllocator>, "BasicString requires InAllocator to satisfy gp::concepts::IsAllocator."
    );

public:
    using TraitsType = CharTraits<InCharT>;
    using ValueType = InCharT;
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
    using ViewType = BasicStringView<InCharT>;

    /// @brief Sentinel value indicating "not found" or "until the end of the string".
    static constexpr SizeType npos = ViewType::npos;

    /// @brief Total byte footprint of the union storage block.
    static constexpr SizeType kTotalBytes = 24u;

    /// @brief Bytes available for inline SSO characters + null terminator before the tag byte at offset 23.
    static constexpr SizeType kSsoBufferBytes = 23u;

    /// @brief Total number of `InCharT` code units (including the null terminator) stored in SSO mode.
    static constexpr SizeType kSsoMaxUnits = kSsoBufferBytes / sizeof(InCharT);

    /// @brief Maximum string length (excluding the null terminator) that fits in inline SSO storage (`22` for `char`).
    static constexpr SizeType kSsoCapacity = kSsoMaxUnits - 1u;

    /// @brief Number of alignment padding bytes between the SSO character array and the tag byte at offset 23.
    static constexpr SizeType kSsoPaddingBytes = kSsoBufferBytes - (kSsoMaxUnits * sizeof(InCharT));

    /// @brief High-bit flag mask in `HeapLayout::capacityAndFlag` that marks Long (Heap/Arena) mode.
    static constexpr SizeType kLongFlagMask = (SizeType{ 1u } << (sizeof(SizeType) * 8u - 1u));

    /// @brief High-bit flag mask in `SsoLayout::ssoSizeAndFlag` (byte 23 on little-endian 64-bit targets).
    static constexpr gp::UInt8 kSsoLongFlagByteMask = 0x80u;

    /// @brief Memory alignment used for dynamic buffer allocations.
    static constexpr gp::UInt32 kAllocAlignment = static_cast<gp::UInt32>(
        alignof(InCharT) > gp::memory::kMinimumAlignment ? alignof(InCharT) : gp::memory::kMinimumAlignment
    );

public:
    /// @brief Default constructor. Initializes an empty string in inline SSO storage with zero heap allocations.
    constexpr BasicString() noexcept = default;

    /// @brief Constructs an empty string bound to a copy of `allocator`.
    /// @param[in] allocator Allocator instance to copy.
    constexpr explicit BasicString(const InAllocator& allocator) noexcept
        requires std::is_copy_constructible_v<InAllocator>
        : m_storage{}
        , m_allocator(allocator)
    {}

    /// @brief Constructs an empty string by moving `allocator` into this container.
    /// @param[in,out] allocator Allocator instance to move from.
    constexpr explicit BasicString(InAllocator&& allocator) noexcept requires std::is_move_constructible_v<InAllocator>
        : m_storage{}
        , m_allocator(std::move(allocator))
    {}

    /// @brief Constructs a string with `count` copies of character `ch`.
    /// @param[in] count Number of characters to initialize.
    /// @param[in] ch Fill character.
    constexpr BasicString(const SizeType count, const InCharT ch) noexcept
    {
        assign(count, ch);
    }

    /// @brief Constructs a string with `count` copies of character `ch` using `allocator`.
    /// @param[in] count Number of characters to initialize.
    /// @param[in] ch Fill character.
    /// @param[in] allocator Allocator instance to copy.
    constexpr BasicString(const SizeType count, const InCharT ch, const InAllocator& allocator) noexcept
        requires std::is_copy_constructible_v<InAllocator>
        : m_storage{}
        , m_allocator(allocator)
    {
        assign(count, ch);
    }

    /// @brief Constructs a string from a null-terminated C-string.
    /// @param[in] str Null-terminated character array (`nullptr` produces an empty string).
    constexpr BasicString(const InCharT* const str) noexcept
    {
        assign(ViewType(str));
    }

    /// @brief Constructs a string from a null-terminated C-string using `allocator`.
    /// @param[in] str Null-terminated character array (`nullptr` produces an empty string).
    /// @param[in] allocator Allocator instance to copy.
    constexpr BasicString(const InCharT* const str, const InAllocator& allocator) noexcept
        requires std::is_copy_constructible_v<InAllocator>
        : m_storage{}
        , m_allocator(allocator)
    {
        assign(ViewType(str));
    }

    /// @brief Constructs a string from a character pointer and explicit length.
    /// @param[in] str Pointer to the character array.
    /// @param[in] count Number of code units to copy.
    constexpr BasicString(const InCharT* const str, const SizeType count) noexcept
    {
        assign(str, count);
    }

    /// @brief Constructs a string from a character pointer, explicit length, and `allocator`.
    /// @param[in] str Pointer to the character array.
    /// @param[in] count Number of code units to copy.
    /// @param[in] allocator Allocator instance to copy.
    constexpr BasicString(const InCharT* const str, const SizeType count, const InAllocator& allocator) noexcept
        requires std::is_copy_constructible_v<InAllocator>
        : m_storage{}
        , m_allocator(allocator)
    {
        assign(str, count);
    }

    /// @brief Constructs a string from a `BasicStringView`.
    /// @param[in] strView The string view to copy from.
    constexpr BasicString(const ViewType strView) noexcept
    {
        assign(strView);
    }

    /// @brief Constructs a string from a `BasicStringView` using `allocator`.
    /// @param[in] strView The string view to copy from.
    /// @param[in] allocator Allocator instance to copy.
    constexpr BasicString(const ViewType strView, const InAllocator& allocator) noexcept
        requires std::is_copy_constructible_v<InAllocator>
        : m_storage{}
        , m_allocator(allocator)
    {
        assign(strView);
    }

    /// @brief Constructs a string from a stack-allocated `BasicFixedString`.
    /// @tparam N Capacity of the source `BasicFixedString`.
    /// @param[in] fixedStr Source fixed string.
    template <gp::USize N>
    constexpr BasicString(const BasicFixedString<InCharT, N>& fixedStr) noexcept
    {
        assign(fixedStr.view());
    }

    /// @brief Constructs a string from an iterator range `[first, last)`.
    /// @tparam InInputIt Input iterator yielding elements convertible to `InCharT`.
    /// @param[in] first Beginning of the range.
    /// @param[in] last End of the range.
    template <typename InInputIt>
    requires(!std::is_integral_v<InInputIt>) constexpr BasicString(InInputIt first, InInputIt last) noexcept
    {
        for (; first != last; ++first)
        {
            pushBack(static_cast<InCharT>(*first));
        }
    }

    /// @brief Constructs a string from an initializer list of characters.
    /// @param[in] ilist Initializer list of characters.
    constexpr BasicString(const std::initializer_list<InCharT> ilist) noexcept
    {
        assign(ilist.begin(), ilist.size());
    }

    /// @brief Copy constructor (when `InAllocator` is copy-constructible).
    /// @param[in] other Source string to copy.
    constexpr BasicString(const BasicString& other) noexcept requires std::is_copy_constructible_v<InAllocator>
        : m_storage{}
        , m_allocator(other.m_allocator)
    {
        assign(other.view());
    }

    /// @brief Copy constructor (when `InAllocator` is not copy-constructible, e.g., `InlineAllocator`).
    /// @param[in] other Source string to copy into a default-constructed allocator.
    constexpr BasicString(const BasicString& other) noexcept
        requires(!std::is_copy_constructible_v<InAllocator> && std::is_default_constructible_v<InAllocator>)
        : m_storage{}
        , m_allocator{}
    {
        assign(other.view());
    }

    /// @brief Cross-allocator copy constructor from any `BasicString` with a different allocator type.
    /// @tparam InOtherAllocator Source string's allocator type.
    /// @param[in] other Source string to copy characters from.
    template <concepts::IsAllocator InOtherAllocator>
    requires(!std::is_same_v<InAllocator, InOtherAllocator>)
    constexpr explicit BasicString(const BasicString<InCharT, InOtherAllocator>& other) noexcept
    {
        assign(other.view());
    }

    /// @brief Move constructor (when `InAllocator` is move-constructible).
    /// @details Transfers heap ownership in O(1) if the moved allocator compares equal and does not point into an
    ///          embedded inline buffer owned by `other`. Otherwise copies the character range into `m_allocator`.
    /// @param[in,out] other Source string to move from; left in a valid empty SSO state.
    constexpr BasicString(BasicString&& other) noexcept requires std::is_move_constructible_v<InAllocator>
        : m_storage{}
        , m_allocator(std::move(other.m_allocator))
    {
        if (other.isLong())
        {
            if (m_allocator == other.m_allocator)
            {
                m_storage.heap = other.m_storage.heap;
                other.initSsoEmpty();
            }
            else
            {
                assign(other.view());
                other.reset();
            }
        }
        else
        {
            m_storage.sso = other.m_storage.sso;
            other.initSsoEmpty();
        }
    }

    /// @brief Move constructor (when `InAllocator` is not move-constructible, e.g., `InlineAllocator`).
    /// @param[in,out] other Source string to move from; left in a valid empty SSO state.
    constexpr BasicString(BasicString&& other) noexcept
        requires(!std::is_move_constructible_v<InAllocator> && std::is_default_constructible_v<InAllocator>)
        : m_storage{}
        , m_allocator{}
    {
        assign(other.view());
        other.reset();
    }

    /// @brief Destructor. Releases any heap/arena allocation back to `m_allocator`.
    constexpr ~BasicString() noexcept
    {
        if (!std::is_constant_evaluated())
        {
            deallocateLongBuffer();
        }
    }

public:
    /// @brief Copy assignment operator.
    /// @param[in] other Source string to copy.
    /// @return Reference to `*this`.
    constexpr BasicString& operator=(const BasicString& other) noexcept
    {
        if (this != &other)
        {
            assign(other.view());
        }
        return *this;
    }

    /// @brief Move assignment operator.
    /// @details Steals the heap buffer in O(1) when `m_allocator == other.m_allocator`; otherwise copies into
    ///          `this->m_allocator` and resets `other`.
    /// @param[in,out] other Source string to move from.
    /// @return Reference to `*this`.
    constexpr BasicString& operator=(BasicString&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        if (other.isLong() && (m_allocator == other.m_allocator))
        {
            if (!std::is_constant_evaluated())
            {
                deallocateLongBuffer();
            }
            m_storage.heap = other.m_storage.heap;
            other.initSsoEmpty();
            return *this;
        }

        assign(other.view());
        other.reset();
        return *this;
    }

    /// @brief Assigns the contents of a `BasicStringView` to this string.
    /// @param[in] strView The string view to copy from.
    /// @return Reference to `*this`.
    constexpr BasicString& operator=(const ViewType strView) noexcept
    {
        return assign(strView);
    }

    /// @brief Assigns a null-terminated C-string to this string.
    /// @param[in] str The null-terminated C-string to copy from.
    /// @return Reference to `*this`.
    constexpr BasicString& operator=(const InCharT* const str) noexcept
    {
        return assign(ViewType(str));
    }

    /// @brief Assigns a single character to this string.
    /// @param[in] ch The character to assign.
    /// @return Reference to `*this`.
    constexpr BasicString& operator=(const InCharT ch) noexcept
    {
        return assign(1u, ch);
    }

    /// @brief Assigns an initializer list of characters to this string.
    /// @param[in] ilist The initializer list to copy from.
    /// @return Reference to `*this`.
    constexpr BasicString& operator=(const std::initializer_list<InCharT> ilist) noexcept
    {
        return assign(ilist.begin(), ilist.size());
    }

public:
    /// @brief Checks whether the string is currently using a dynamic Long (Heap/Arena) allocation.
    /// @return True if in Long mode, false if in inline SSO mode.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool isLong() const noexcept
    {
        if (std::is_constant_evaluated())
        {
            return false;
        }
        return (m_storage.sso.ssoSizeAndFlag & kSsoLongFlagByteMask) != 0u;
    }

    /// @brief Checks whether the string is currently stored inside the 24-byte inline SSO buffer.
    /// @return True if in inline SSO mode, false if in Long mode.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool isSso() const noexcept
    {
        return !isLong();
    }

    /// @brief Returns the number of active code units in the string (excluding the null terminator).
    /// @return Number of code units in the string.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr SizeType size() const noexcept
    {
        return isLong() ? m_storage.heap.size : static_cast<SizeType>(m_storage.sso.ssoSizeAndFlag);
    }

    /// @brief Returns the number of active code units in the string (alias for `size()`).
    /// @return Number of code units in the string.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr SizeType length() const noexcept
    {
        return size();
    }

    /// @brief Returns the byte size of the active string characters (excluding the null terminator).
    /// @return `size() * sizeof(InCharT)`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr SizeType sizeInBytes() const noexcept
    {
        return size() * sizeof(InCharT);
    }

    /// @brief Returns the maximum number of code units that can be held without reallocation (excluding null).
    /// @return Current capacity in code units (`>= kSsoCapacity`).
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr SizeType capacity() const noexcept
    {
        return isLong() ? (m_storage.heap.capacityAndFlag & ~kLongFlagMask) : kSsoCapacity;
    }

    /// @brief Returns the theoretical maximum number of code units a `BasicString` can represent.
    /// @return Maximum representable length before the high-bit Long flag.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr SizeType maxSize() const noexcept
    {
        return (kLongFlagMask - 1u) / sizeof(InCharT) - 1u;
    }

    /// @brief Checks whether the string is empty (`size() == 0`).
    /// @return True if the string contains zero characters, false otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool isEmpty() const noexcept
    {
        return size() == 0u;
    }

    /// @brief Returns a mutable pointer to the underlying null-terminated character buffer.
    /// @return Mutable pointer to the first character.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr Pointer data() noexcept
    {
        return isLong() ? m_storage.heap.data : m_storage.sso.buffer;
    }

    /// @brief Returns a const pointer to the underlying null-terminated character buffer.
    /// @return Const pointer to the first character.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr ConstPointer data() const noexcept
    {
        return isLong() ? m_storage.heap.data : m_storage.sso.buffer;
    }

    /// @brief Returns a const pointer to the underlying null-terminated C-string.
    /// @return Null-terminated `const InCharT*`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr ConstPointer cString() const noexcept
    {
        return data();
    }

    /// @brief Returns a const pointer to the underlying null-terminated C-string (Unreal `*Str` idiom).
    /// @return Null-terminated `const InCharT*`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr ConstPointer operator*() const noexcept
    {
        return data();
    }

    /// @brief Returns a non-owning `BasicStringView` over the entire string.
    /// @return `BasicStringView<InCharT>(data(), size())`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr ViewType view() const noexcept
    {
        return ViewType(data(), size());
    }

    /// @brief Implicitly converts this string to a non-owning `BasicStringView`.
    /// @return `BasicStringView<InCharT>(data(), size())`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr operator ViewType() const noexcept
    {
        return ViewType(data(), size());
    }

    /// @brief Returns a mutable reference to the bound allocator instance.
    /// @return Reference to `m_allocator`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr InAllocator& getAllocator() noexcept
    {
        return m_allocator;
    }

    /// @brief Returns a const reference to the bound allocator instance.
    /// @return Const reference to `m_allocator`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr const InAllocator& getAllocator() const noexcept
    {
        return m_allocator;
    }

public:
    /// @brief Gets a mutable reference to the character at `pos`.
    /// @param[in] pos Zero-based index (`< size()`).
    /// @return Reference to the character at `pos`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr Reference operator[](const SizeType pos) noexcept
    {
        GP_ASSERT(pos < size());
        return data()[pos];
    }

    /// @brief Gets a const reference to the character at `pos`.
    /// @param[in] pos Zero-based index (`< size()`).
    /// @return Const reference to the character at `pos`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr ConstReference operator[](const SizeType pos) const noexcept
    {
        GP_ASSERT(pos < size());
        return data()[pos];
    }

    /// @brief Gets a mutable reference to the character at `pos` with bounds assertion.
    /// @param[in] pos Zero-based index (`< size()`).
    /// @return Reference to the character at `pos`.
    [[nodiscard]] constexpr Reference at(const SizeType pos) noexcept
    {
        GP_ASSERT(pos < size());
        return data()[pos];
    }

    /// @brief Gets a const reference to the character at `pos` with bounds assertion.
    /// @param[in] pos Zero-based index (`< size()`).
    /// @return Const reference to the character at `pos`.
    [[nodiscard]] constexpr ConstReference at(const SizeType pos) const noexcept
    {
        GP_ASSERT(pos < size());
        return data()[pos];
    }

    /// @brief Gets a mutable reference to the first character in the string.
    /// @return Reference to `data()[0]`.
    [[nodiscard]] constexpr Reference front() noexcept
    {
        GP_ASSERT(!isEmpty());
        return data()[0];
    }

    /// @brief Gets a const reference to the first character in the string.
    /// @return Const reference to `data()[0]`.
    [[nodiscard]] constexpr ConstReference front() const noexcept
    {
        GP_ASSERT(!isEmpty());
        return data()[0];
    }

    /// @brief Gets a mutable reference to the last character in the string.
    /// @return Reference to `data()[size() - 1]`.
    [[nodiscard]] constexpr Reference back() noexcept
    {
        GP_ASSERT(!isEmpty());
        return data()[size() - 1u];
    }

    /// @brief Gets a const reference to the last character in the string.
    /// @return Const reference to `data()[size() - 1]`.
    [[nodiscard]] constexpr ConstReference back() const noexcept
    {
        GP_ASSERT(!isEmpty());
        return data()[size() - 1u];
    }

    /// @brief Gets a mutable iterator to the beginning of the string.
    /// @return Iterator to the first character.
    [[nodiscard]] constexpr Iterator begin() noexcept
    {
        return data();
    }

    /// @brief Gets a const iterator to the beginning of the string.
    /// @return Const iterator to the first character.
    [[nodiscard]] constexpr ConstIterator begin() const noexcept
    {
        return data();
    }

    /// @brief Gets a mutable iterator one past the end of the string.
    /// @return Iterator one past the last character.
    [[nodiscard]] constexpr Iterator end() noexcept
    {
        return data() + size();
    }

    /// @brief Gets a const iterator one past the end of the string.
    /// @return Const iterator one past the last character.
    [[nodiscard]] constexpr ConstIterator end() const noexcept
    {
        return data() + size();
    }

    /// @brief Gets a const iterator to the beginning of the string.
    /// @return Const iterator to the first character.
    [[nodiscard]] constexpr ConstIterator cbegin() const noexcept
    {
        return data();
    }

    /// @brief Gets a const iterator one past the end of the string.
    /// @return Const iterator one past the last character.
    [[nodiscard]] constexpr ConstIterator cend() const noexcept
    {
        return data() + size();
    }

    /// @brief Gets a mutable reverse iterator to the beginning of the reversed string.
    /// @return Reverse iterator to the last character.
    [[nodiscard]] constexpr ReverseIterator rbegin() noexcept
    {
        return ReverseIterator(end());
    }

    /// @brief Gets a const reverse iterator to the beginning of the reversed string.
    /// @return Const reverse iterator to the last character.
    [[nodiscard]] constexpr ConstReverseIterator rbegin() const noexcept
    {
        return ConstReverseIterator(end());
    }

    /// @brief Gets a mutable reverse iterator one past the end of the reversed string.
    /// @return Reverse iterator before the first character.
    [[nodiscard]] constexpr ReverseIterator rend() noexcept
    {
        return ReverseIterator(begin());
    }

    /// @brief Gets a const reverse iterator one past the end of the reversed string.
    /// @return Const reverse iterator before the first character.
    [[nodiscard]] constexpr ConstReverseIterator rend() const noexcept
    {
        return ConstReverseIterator(begin());
    }

    /// @brief Gets a const reverse iterator to the beginning of the reversed string.
    /// @return Const reverse iterator to the last character.
    [[nodiscard]] constexpr ConstReverseIterator crbegin() const noexcept
    {
        return ConstReverseIterator(end());
    }

    /// @brief Gets a const reverse iterator one past the end of the reversed string.
    /// @return Const reverse iterator before the first character.
    [[nodiscard]] constexpr ConstReverseIterator crend() const noexcept
    {
        return ConstReverseIterator(begin());
    }

public:
    /// @brief Reserves capacity for at least `newCapacity` code units (excluding the null terminator).
    /// @details Queries `gp::memory::calculateSlackReserve` to quantize the allocation to the underlying allocator's
    ///          usable block size. No-op if `newCapacity <= capacity()`.
    /// @param[in] newCapacity Minimum desired capacity in code units.
    constexpr void reserve(const SizeType newCapacity) noexcept
    {
        if (newCapacity <= capacity())
        {
            return;
        }

        const SizeType requestedElementsWithNull = newCapacity + 1u;
        SizeType targetElementsWithNull = gp::memory::calculateSlackReserve<SizeType>(
            m_allocator, requestedElementsWithNull, sizeof(InCharT), true, kAllocAlignment
        );
        if (targetElementsWithNull < requestedElementsWithNull)
        {
            targetElementsWithNull = requestedElementsWithNull;
        }

        reallocateToCapacity(targetElementsWithNull - 1u);
    }

    /// @brief Shrinks the string's capacity to fit its current size.
    /// @details If `size() <= kSsoCapacity` while in Long mode, copies the characters back into the 24-byte inline SSO
    ///          buffer and frees the heap block completely!
    constexpr void shrinkToFit() noexcept
    {
        if (!isLong())
        {
            return;
        }

        const SizeType curSize = m_storage.heap.size;
        const SizeType curCap = m_storage.heap.capacityAndFlag & ~kLongFlagMask;
        Pointer oldData = m_storage.heap.data;
        const SizeType oldAllocBytes = (curCap + 1u) * sizeof(InCharT);

        if (curSize <= kSsoCapacity)
        {
            initSsoEmpty();
            if (curSize > 0u)
            {
                TraitsType::copy(m_storage.sso.buffer, oldData, curSize);
            }
            setSsoSize(curSize);
            m_allocator.deallocate(oldData, oldAllocBytes, kAllocAlignment);
            return;
        }

        const SizeType targetElementsWithNull = gp::memory::calculateSlackReserve<SizeType>(
            m_allocator, curSize + 1u, sizeof(InCharT), true, kAllocAlignment
        );
        const SizeType targetCap = (targetElementsWithNull > curSize + 1u) ? (targetElementsWithNull - 1u) : curSize;

        if (targetCap < curCap)
        {
            const SizeType newAllocBytes = (targetCap + 1u) * sizeof(InCharT);
            Pointer newData =
                static_cast<Pointer>(m_allocator.reallocate(oldData, oldAllocBytes, newAllocBytes, kAllocAlignment));
            GP_ASSERT(newData != nullptr && "BasicString::shrinkToFit reallocation failed");
            setLongState(newData, curSize, targetCap);
        }
    }

    /// @brief Sets the string length to 0 and writes a null terminator without releasing allocated capacity.
    constexpr void clear() noexcept
    {
        setInternalSize(0u);
    }

    /// @brief Resets the string to empty AND releases any dynamically allocated buffer back to `m_allocator`.
    constexpr void reset() noexcept
    {
        if (!std::is_constant_evaluated() && isLong())
        {
            deallocateLongBuffer();
        }
        initSsoEmpty();
    }

    /// @brief Resizes the string to `newSize` without initializing newly added characters, writing only the trailing
    ///        null terminator at `data()[newSize]`.
    /// @note Designed for high-performance I/O, formatting, and UTF transcoding where the caller immediately overwrites
    ///       the buffer.
    /// @param[in] newSize New active length in code units.
    constexpr void resizeUninitialized(const SizeType newSize) noexcept
    {
        if (newSize > capacity())
        {
            reserve(newSize);
        }
        setInternalSize(newSize);
    }

    /// @brief Resizes the string to `newSize`, filling any newly added characters with `fillChar`.
    /// @param[in] newSize New active length in code units.
    /// @param[in] fillChar Character used to initialize new elements when growing.
    constexpr void resize(const SizeType newSize, const InCharT fillChar = InCharT{ 0 }) noexcept
    {
        const SizeType oldSize = size();
        if (newSize > oldSize)
        {
            if (newSize > capacity())
            {
                reserve(newSize);
            }
            TraitsType::assign(data() + oldSize, newSize - oldSize, fillChar);
        }
        setInternalSize(newSize);
    }

public:
    /// @brief Replaces the contents of the string with `count` copies of `ch`.
    /// @param[in] count Number of characters to write.
    /// @param[in] ch Fill character.
    /// @return Reference to `*this`.
    constexpr BasicString& assign(const SizeType count, const InCharT ch) noexcept
    {
        if (count > capacity())
        {
            reserve(count);
        }
        if (count > 0u)
        {
            TraitsType::assign(data(), count, ch);
        }
        setInternalSize(count);
        return *this;
    }

    /// @brief Replaces the contents of the string with `strView` (safe for overlapping self-subviews).
    /// @param[in] strView Source string view.
    /// @return Reference to `*this`.
    constexpr BasicString& assign(const ViewType strView) noexcept
    {
        const SizeType newLen = strView.size();
        if (newLen <= capacity())
        {
            Pointer dst = data();
            if (newLen > 0u)
            {
                TraitsType::move(dst, strView.data(), newLen);
            }
            setInternalSize(newLen);
            return *this;
        }

        reserve(newLen);
        TraitsType::copy(data(), strView.data(), newLen);
        setInternalSize(newLen);
        return *this;
    }

    /// @brief Replaces the contents of the string with `[str, str + count)`.
    /// @param[in] str Pointer to the character array.
    /// @param[in] count Number of characters to copy.
    /// @return Reference to `*this`.
    constexpr BasicString& assign(const InCharT* const str, const SizeType count) noexcept
    {
        return assign(ViewType(str, count));
    }

    /// @brief Replaces the contents of the string with a null-terminated C-string.
    /// @param[in] str Null-terminated character array.
    /// @return Reference to `*this`.
    constexpr BasicString& assign(const InCharT* const str) noexcept
    {
        return assign(ViewType(str));
    }

    /// @brief Appends a `BasicStringView` to the end of the string (safe for self-aliasing `str.append(str)`).
    /// @param[in] strView The string view to append.
    /// @return Reference to `*this`.
    constexpr BasicString& append(const ViewType strView) noexcept
    {
        const SizeType addLen = strView.size();
        if (addLen == 0u)
        {
            return *this;
        }

        const SizeType oldLen = size();
        const SizeType newLen = oldLen + addLen;
        const ConstPointer srcPtr = strView.data();
        SizeType aliasOffset = 0u;
        const bool isSelfAliased = tryGetSelfAliasOffset(srcPtr, oldLen, aliasOffset);

        if (newLen > capacity())
        {
            if (isSelfAliased)
            {
                growToFit(newLen);
                Pointer dst = data();
                TraitsType::move(dst + oldLen, dst + aliasOffset, addLen);
                setInternalSize(newLen);
                return *this;
            }

            growToFit(newLen);
        }

        Pointer dst = data();
        TraitsType::move(dst + oldLen, srcPtr, addLen);
        setInternalSize(newLen);
        return *this;
    }

    /// @brief Appends `count` copies of character `ch` to the end of the string.
    /// @param[in] count Number of characters to append.
    /// @param[in] ch Fill character.
    /// @return Reference to `*this`.
    constexpr BasicString& append(const SizeType count, const InCharT ch) noexcept
    {
        if (count == 0u)
        {
            return *this;
        }

        const SizeType oldLen = size();
        const SizeType newLen = oldLen + count;
        if (newLen > capacity())
        {
            growToFit(newLen);
        }

        TraitsType::assign(data() + oldLen, count, ch);
        setInternalSize(newLen);
        return *this;
    }

    /// @brief Appends `[str, str + count)` to the end of the string.
    /// @param[in] str Pointer to the character array.
    /// @param[in] count Number of characters to append.
    /// @return Reference to `*this`.
    constexpr BasicString& append(const InCharT* const str, const SizeType count) noexcept
    {
        return append(ViewType(str, count));
    }

    /// @brief Appends a null-terminated C-string to the end of the string.
    /// @param[in] str Null-terminated character array.
    /// @return Reference to `*this`.
    constexpr BasicString& append(const InCharT* const str) noexcept
    {
        return append(ViewType(str));
    }

    /// @brief Appends a single character to the end of the string.
    /// @param[in] ch Character to append.
    constexpr void pushBack(const InCharT ch) noexcept
    {
        const SizeType oldLen = size();
        const SizeType newLen = oldLen + 1u;
        if (newLen > capacity())
        {
            growToFit(newLen);
        }
        data()[oldLen] = ch;
        setInternalSize(newLen);
    }

    /// @brief Removes the last character from the string.
    constexpr void popBack() noexcept
    {
        const SizeType oldLen = size();
        GP_ASSERT(oldLen > 0u && "Cannot popBack from an empty BasicString");
        setInternalSize(oldLen - 1u);
    }

    /// @brief Appends a `BasicStringView` to this string.
    /// @param[in] strView The string view to append.
    /// @return Reference to `*this`.
    constexpr BasicString& operator+=(const ViewType strView) noexcept
    {
        return append(strView);
    }

    /// @brief Appends a null-terminated C-string to this string.
    /// @param[in] str The null-terminated C-string to append.
    /// @return Reference to `*this`.
    constexpr BasicString& operator+=(const InCharT* const str) noexcept
    {
        return append(ViewType(str));
    }

    /// @brief Appends a single character to this string.
    /// @param[in] ch The character to append.
    /// @return Reference to `*this`.
    constexpr BasicString& operator+=(const InCharT ch) noexcept
    {
        pushBack(ch);
        return *this;
    }

    /// @brief Inserts `strView` at the beginning of the string.
    /// @param[in] strView Prefix string view to prepend.
    /// @return Reference to `*this`.
    constexpr BasicString& prepend(const ViewType strView) noexcept
    {
        return insert(0u, strView);
    }

    /// @brief Inserts a single character at the beginning of the string.
    /// @param[in] ch Character to prepend.
    /// @return Reference to `*this`.
    constexpr BasicString& prepend(const InCharT ch) noexcept
    {
        return insert(0u, 1u, ch);
    }

    /// @brief Inserts `strView` at `index` (safe for self-aliasing subviews).
    /// @param[in] index Zero-based insertion position (`<= size()`).
    /// @param[in] strView String view to insert.
    /// @return Reference to `*this`.
    constexpr BasicString& insert(const SizeType index, const ViewType strView) noexcept
    {
        const SizeType oldLen = size();
        GP_ASSERT(index <= oldLen);

        const SizeType addLen = strView.size();
        if (addLen == 0u)
        {
            return *this;
        }

        const SizeType newLen = oldLen + addLen;
        const ConstPointer srcPtr = strView.data();
        SizeType aliasOffset = 0u;
        const bool isSelfAliased = tryGetSelfAliasOffset(srcPtr, oldLen, aliasOffset);

        if (isSelfAliased)
        {
            if (newLen > capacity())
            {
                growToFit(newLen);
            }

            Pointer dst = data();
            const SizeType tailLen = oldLen - index;
            if (tailLen > 0u)
            {
                TraitsType::move(dst + index + addLen, dst + index, tailLen);
            }

            if (aliasOffset + addLen <= index)
            {
                TraitsType::move(dst + index, dst + aliasOffset, addLen);
            }
            else if (aliasOffset >= index)
            {
                TraitsType::move(dst + index, dst + aliasOffset + addLen, addLen);
            }
            else
            {
                const SizeType firstPart = index - aliasOffset;
                const SizeType secondPart = addLen - firstPart;
                TraitsType::move(dst + index, dst + aliasOffset, firstPart);
                TraitsType::move(dst + index + firstPart, dst + index + addLen, secondPart);
            }

            setInternalSize(newLen);
            return *this;
        }

        if (newLen > capacity())
        {
            growToFit(newLen);
        }

        Pointer dst = data();
        const SizeType tailLen = oldLen - index;
        if (tailLen > 0u)
        {
            TraitsType::move(dst + index + addLen, dst + index, tailLen);
        }
        TraitsType::copy(dst + index, srcPtr, addLen);
        setInternalSize(newLen);
        return *this;
    }

    /// @brief Inserts `count` copies of `ch` at `index`.
    /// @param[in] index Zero-based insertion position (`<= size()`).
    /// @param[in] count Number of copies to insert.
    /// @param[in] ch Fill character.
    /// @return Reference to `*this`.
    constexpr BasicString& insert(const SizeType index, const SizeType count, const InCharT ch) noexcept
    {
        const SizeType oldLen = size();
        GP_ASSERT(index <= oldLen);
        if (count == 0u)
        {
            return *this;
        }

        const SizeType newLen = oldLen + count;
        if (newLen > capacity())
        {
            growToFit(newLen);
        }

        Pointer dst = data();
        const SizeType tailLen = oldLen - index;
        if (tailLen > 0u)
        {
            TraitsType::move(dst + index + count, dst + index, tailLen);
        }
        TraitsType::assign(dst + index, count, ch);
        setInternalSize(newLen);
        return *this;
    }

    /// @brief Removes up to `count` characters starting at `index`.
    /// @param[in] index Starting position (`<= size()`).
    /// @param[in] count Maximum number of characters to remove (clamped to remaining length).
    /// @return Reference to `*this`.
    constexpr BasicString& erase(const SizeType index = 0u, const SizeType count = npos) noexcept
    {
        const SizeType oldLen = size();
        GP_ASSERT(index <= oldLen);

        const SizeType maxRemovable = oldLen - index;
        const SizeType removeCount = (count < maxRemovable) ? count : maxRemovable;
        if (removeCount == 0u)
        {
            return *this;
        }

        Pointer dst = data();
        const SizeType tailLen = oldLen - (index + removeCount);
        if (tailLen > 0u)
        {
            TraitsType::move(dst + index, dst + index + removeCount, tailLen);
        }
        setInternalSize(oldLen - removeCount);
        return *this;
    }

    /// @brief Removes `count` characters starting at `index` (Unreal `RemoveAt` equivalent; asserts bounds).
    /// @param[in] index Starting position.
    /// @param[in] count Exact number of characters to remove (`index + count <= size()`).
    /// @return Reference to `*this`.
    constexpr BasicString& removeAt(const SizeType index, const SizeType count = 1u) noexcept
    {
        GP_ASSERT(index + count <= size());
        return erase(index, count);
    }

    /// @brief Removes `count` characters from the beginning of the string in-place.
    /// @param[in] count Number of prefix characters to remove (`<= size()`).
    /// @return Reference to `*this`.
    constexpr BasicString& removePrefix(const SizeType count) noexcept
    {
        GP_ASSERT(count <= size());
        return erase(0u, count);
    }

    /// @brief Removes `count` characters from the end of the string in-place.
    /// @param[in] count Number of suffix characters to remove (`<= size()`).
    /// @return Reference to `*this`.
    constexpr BasicString& removeSuffix(const SizeType count) noexcept
    {
        const SizeType oldLen = size();
        GP_ASSERT(count <= oldLen);
        setInternalSize(oldLen - count);
        return *this;
    }

    /// @brief Replaces all occurrences of single character `from` with `to` in-place.
    /// @param[in] from Character to search for.
    /// @param[in] to Replacement character.
    /// @param[in] searchCase Case-sensitivity mode.
    /// @return Number of characters replaced.
    constexpr SizeType replaceCharInPlace(
        const InCharT from, const InCharT to, const SearchCase searchCase = SearchCase::CaseSensitive
    ) noexcept
    {
        Pointer dst = data();
        const SizeType len = size();
        SizeType replacedCount = 0u;

        if (searchCase == SearchCase::CaseSensitive)
        {
            for (SizeType i = 0u; i < len; ++i)
            {
                if (TraitsType::eq(dst[i], from))
                {
                    dst[i] = to;
                    ++replacedCount;
                }
            }
        }
        else
        {
            for (SizeType i = 0u; i < len; ++i)
            {
                if (TraitsType::eqIgnoreCase(dst[i], from))
                {
                    dst[i] = to;
                    ++replacedCount;
                }
            }
        }

        return replacedCount;
    }

    /// @brief Replaces all non-overlapping occurrences of `from` with `to` in-place.
    /// @details When `to.size() <= from.size()`, performs an in-place single-pass compaction with zero allocations.
    ///          When `to.size() > from.size()`, counts matches first, ensures capacity once, shifts the unread
    ///          characters to the end of the target range, and streams forward in-place with zero temporary string
    ///          allocations!
    /// @param[in] from Substring to search for (if empty, returns `0`).
    /// @param[in] to Replacement substring.
    /// @param[in] searchCase Case-sensitivity mode.
    /// @return Number of occurrences replaced.
    constexpr SizeType replaceInPlace(
        const ViewType from, const ViewType to, const SearchCase searchCase = SearchCase::CaseSensitive
    ) noexcept
    {
        if (from.isEmpty() || isEmpty())
        {
            return 0u;
        }

        const SizeType fromLen = from.size();
        const SizeType toLen = to.size();

        SizeType dummyOffset = 0u;
        const SizeType curLen = size();
        const bool isAliased = tryGetSelfAliasOffset(from.data(), curLen, dummyOffset) ||
                               tryGetSelfAliasOffset(to.data(), curLen, dummyOffset);

        if (isAliased)
        {
            const BasicString<InCharT, gp::memory::DefaultAllocator> fromCopy(from);
            const BasicString<InCharT, gp::memory::DefaultAllocator> toCopy(to);
            return replaceInPlace(fromCopy.view(), toCopy.view(), searchCase);
        }

        if (toLen <= fromLen)
        {
            Pointer buf = data();
            const SizeType oldLen = size();
            SizeType readPos = 0u;
            SizeType writePos = 0u;
            SizeType replaced = 0u;

            while (readPos < oldLen)
            {
                const ViewType remaining(buf + readPos, oldLen - readPos);
                const SizeType relMatch = remaining.find(from, 0u, searchCase);
                if (relMatch == npos)
                {
                    const SizeType tail = oldLen - readPos;
                    if (writePos != readPos && tail > 0u)
                    {
                        TraitsType::move(buf + writePos, buf + readPos, tail);
                    }
                    writePos += tail;
                    break;
                }

                if (relMatch > 0u)
                {
                    if (writePos != readPos)
                    {
                        TraitsType::move(buf + writePos, buf + readPos, relMatch);
                    }
                    writePos += relMatch;
                }

                if (toLen > 0u)
                {
                    TraitsType::copy(buf + writePos, to.data(), toLen);
                    writePos += toLen;
                }

                readPos += relMatch + fromLen;
                ++replaced;
            }

            setInternalSize(writePos);
            return replaced;
        }

        // Expansion case (`toLen > fromLen`): in-place right-shift + forward stream (zero temporary allocations).
        const SizeType matchCount = view().countOccurrences(from, searchCase);
        if (matchCount == 0u)
        {
            return 0u;
        }

        const SizeType oldLen = size();
        const SizeType diff = toLen - fromLen;
        const SizeType shift = matchCount * diff;
        const SizeType newLen = oldLen + shift;

        if (newLen > capacity())
        {
            growToFit(newLen);
        }

        Pointer buf = data();
        TraitsType::move(buf + shift, buf, oldLen);

        SizeType readPos = shift;
        SizeType writePos = 0u;
        while (readPos < newLen)
        {
            const ViewType remaining(buf + readPos, newLen - readPos);
            const SizeType relMatch = remaining.find(from, 0u, searchCase);
            if (relMatch == npos)
            {
                const SizeType tail = newLen - readPos;
                if (writePos != readPos && tail > 0u)
                {
                    TraitsType::move(buf + writePos, buf + readPos, tail);
                }
                writePos += tail;
                break;
            }

            if (relMatch > 0u)
            {
                if (writePos != readPos)
                {
                    TraitsType::move(buf + writePos, buf + readPos, relMatch);
                }
                writePos += relMatch;
                readPos += relMatch;
            }

            readPos += fromLen;
            TraitsType::copy(buf + writePos, to.data(), toLen);
            writePos += toLen;
        }

        setInternalSize(newLen);
        return matchCount;
    }

    /// @brief Returns a new string in which all occurrences of `from` are replaced by `to`.
    /// @param[in] from Substring to search for.
    /// @param[in] to Replacement substring.
    /// @param[in] searchCase Case-sensitivity mode.
    /// @return Newly constructed `BasicString` with replacements applied.
    [[nodiscard]] constexpr BasicString replace(
        const ViewType from, const ViewType to, const SearchCase searchCase = SearchCase::CaseSensitive
    ) const noexcept
    {
        if (from.isEmpty() || isEmpty())
        {
            return *this;
        }

        const SizeType matchCount = view().countOccurrences(from, searchCase);
        if (matchCount == 0u)
        {
            return *this;
        }

        const SizeType oldLen = size();
        const SizeType fromLen = from.size();
        const SizeType toLen = to.size();
        const SizeType newLen =
            (toLen >= fromLen) ? (oldLen + matchCount * (toLen - fromLen)) : (oldLen - matchCount * (fromLen - toLen));

        BasicString result = makeEmptyWithSameAllocator();
        result.reserve(newLen);

        const ViewType selfView = view();
        SizeType searchPos = 0u;
        while (searchPos < oldLen)
        {
            const SizeType matchPos = selfView.find(from, searchPos, searchCase);
            if (matchPos == npos)
            {
                result.append(selfView.subview(searchPos));
                break;
            }
            if (matchPos > searchPos)
            {
                result.append(selfView.subview(searchPos, matchPos - searchPos));
            }
            result.append(to);
            searchPos = matchPos + fromLen;
        }

        return result;
    }

    /// @brief Converts all ASCII uppercase characters in-place to lowercase.
    /// @return Reference to `*this`.
    constexpr BasicString& toLowerInPlace() noexcept
    {
        Pointer dst = data();
        const SizeType len = size();
        for (SizeType i = 0u; i < len; ++i)
        {
            dst[i] = TraitsType::toLower(dst[i]);
        }
        return *this;
    }

    /// @brief Returns a lowercase copy of this string.
    /// @return New `BasicString` with ASCII characters converted to lowercase.
    [[nodiscard]] constexpr BasicString toLower() const noexcept
    {
        BasicString copy(*this);
        copy.toLowerInPlace();
        return copy;
    }

    /// @brief Converts all ASCII lowercase characters in-place to uppercase.
    /// @return Reference to `*this`.
    constexpr BasicString& toUpperInPlace() noexcept
    {
        Pointer dst = data();
        const SizeType len = size();
        for (SizeType i = 0u; i < len; ++i)
        {
            dst[i] = TraitsType::toUpper(dst[i]);
        }
        return *this;
    }

    /// @brief Returns an uppercase copy of this string.
    /// @return New `BasicString` with ASCII characters converted to uppercase.
    [[nodiscard]] constexpr BasicString toUpper() const noexcept
    {
        BasicString copy(*this);
        copy.toUpperInPlace();
        return copy;
    }

    /// @brief Trims leading ASCII whitespace characters in-place.
    /// @return Reference to `*this`.
    constexpr BasicString& trimStartInPlace() noexcept
    {
        return assign(view().trimStart());
    }

    /// @brief Trims trailing ASCII whitespace characters in-place.
    /// @return Reference to `*this`.
    constexpr BasicString& trimEndInPlace() noexcept
    {
        const ViewType trimmed = view().trimEnd();
        setInternalSize(trimmed.size());
        return *this;
    }

    /// @brief Trims both leading and trailing ASCII whitespace characters in-place.
    /// @return Reference to `*this`.
    constexpr BasicString& trimInPlace() noexcept
    {
        return assign(view().trim());
    }

    /// @brief Returns a new string with leading ASCII whitespace removed.
    /// @return Trimmed `BasicString`.
    [[nodiscard]] constexpr BasicString trimStart() const noexcept
    {
        return makeFromViewWithSameAllocator(view().trimStart());
    }

    /// @brief Returns a new string with trailing ASCII whitespace removed.
    /// @return Trimmed `BasicString`.
    [[nodiscard]] constexpr BasicString trimEnd() const noexcept
    {
        return makeFromViewWithSameAllocator(view().trimEnd());
    }

    /// @brief Returns a new string with leading and trailing ASCII whitespace removed.
    /// @return Trimmed `BasicString`.
    [[nodiscard]] constexpr BasicString trim() const noexcept
    {
        return makeFromViewWithSameAllocator(view().trim());
    }

    /// @brief Reverses the code units of the string in-place.
    /// @return Reference to `*this`.
    constexpr BasicString& reverseInPlace() noexcept
    {
        Pointer dst = data();
        const SizeType len = size();
        for (SizeType i = 0u; i < len / 2u; ++i)
        {
            const InCharT tmp = dst[i];
            dst[i] = dst[len - 1u - i];
            dst[len - 1u - i] = tmp;
        }
        return *this;
    }

    /// @brief Returns a copy of this string with its code units reversed.
    /// @return Reversed `BasicString`.
    [[nodiscard]] constexpr BasicString reverse() const noexcept
    {
        BasicString copy(*this);
        copy.reverseInPlace();
        return copy;
    }

    /// @brief Returns a new string left-padded with `paddingChar` to at least `totalWidth` characters.
    /// @param[in] totalWidth Desired minimum width.
    /// @param[in] paddingChar Character used for padding.
    /// @return Left-padded `BasicString`.
    [[nodiscard]] constexpr BasicString
        padLeft(const SizeType totalWidth, const InCharT paddingChar = static_cast<InCharT>(' ')) const noexcept
    {
        const SizeType curLen = size();
        if (curLen >= totalWidth)
        {
            return *this;
        }
        BasicString result = makeEmptyWithSameAllocator();
        result.reserve(totalWidth);
        result.append(totalWidth - curLen, paddingChar);
        result.append(view());
        return result;
    }

    /// @brief Returns a new string right-padded with `paddingChar` to at least `totalWidth` characters.
    /// @param[in] totalWidth Desired minimum width.
    /// @param[in] paddingChar Character used for padding.
    /// @return Right-padded `BasicString`.
    [[nodiscard]] constexpr BasicString
        padRight(const SizeType totalWidth, const InCharT paddingChar = static_cast<InCharT>(' ')) const noexcept
    {
        const SizeType curLen = size();
        if (curLen >= totalWidth)
        {
            return *this;
        }
        BasicString result = makeEmptyWithSameAllocator();
        result.reserve(totalWidth);
        result.append(view());
        result.append(totalWidth - curLen, paddingChar);
        return result;
    }

    /// @brief Swaps the contents of this string with `other`.
    /// @param[in,out] other The string to swap with.
    constexpr void swap(BasicString& other) noexcept
    {
        if (this == &other)
        {
            return;
        }

        if (m_allocator == other.m_allocator)
        {
            const Storage tempStorage = m_storage;
            m_storage = other.m_storage;
            other.m_storage = tempStorage;
        }
        else
        {
            BasicString temp(view());
            assign(other.view());
            other.assign(temp.view());
        }
    }

public:
    /// @brief Compares this string with `other` for equality using `searchCase`.
    /// @param[in] other The string view to compare with.
    /// @param[in] searchCase Case-sensitivity mode.
    /// @return True if equal under `searchCase`, false otherwise.
    [[nodiscard]] constexpr bool
        equals(const ViewType other, const SearchCase searchCase = SearchCase::CaseSensitive) const noexcept
    {
        return view().equals(other, searchCase);
    }

    /// @brief Case-insensitive equality convenience check.
    /// @param[in] other The string view to compare with.
    /// @return True if equal ignoring ASCII case, false otherwise.
    [[nodiscard]] constexpr bool equalsIgnoreCase(const ViewType other) const noexcept
    {
        return view().equalsIgnoreCase(other);
    }

    /// @brief Three-way lexicographical comparison with configurable case sensitivity.
    /// @param[in] other The string view to compare with.
    /// @param[in] searchCase Case-sensitivity mode.
    /// @return Negative if `*this < other`, 0 if equal, positive if `*this > other`.
    [[nodiscard]] constexpr gp::Int32
        compare(const ViewType other, const SearchCase searchCase = SearchCase::CaseSensitive) const noexcept
    {
        return view().compare(other, searchCase);
    }

    /// @brief Case-insensitive three-way lexicographical comparison.
    /// @param[in] other The string view to compare with.
    /// @return Negative if `*this < other`, 0 if equal, positive if `*this > other`.
    [[nodiscard]] constexpr gp::Int32 compareIgnoreCase(const ViewType other) const noexcept
    {
        return view().compareIgnoreCase(other);
    }

    /// @brief Checks if the string starts with `prefix`.
    /// @param[in] prefix The prefix string view to check for.
    /// @param[in] searchCase Case-sensitivity mode.
    /// @return True if the string starts with `prefix`, false otherwise.
    [[nodiscard]] constexpr bool
        startsWith(const ViewType prefix, const SearchCase searchCase = SearchCase::CaseSensitive) const noexcept
    {
        return view().startsWith(prefix, searchCase);
    }

    /// @brief Checks if the string starts with character `ch`.
    /// @param[in] ch The character to check for.
    /// @param[in] searchCase Case-sensitivity mode.
    /// @return True if the string starts with `ch`, false otherwise.
    [[nodiscard]] constexpr bool
        startsWith(const InCharT ch, const SearchCase searchCase = SearchCase::CaseSensitive) const noexcept
    {
        return view().startsWith(ch, searchCase);
    }

    /// @brief Checks if the string ends with `suffix`.
    /// @param[in] suffix The suffix string view to check for.
    /// @param[in] searchCase Case-sensitivity mode.
    /// @return True if the string ends with `suffix`, false otherwise.
    [[nodiscard]] constexpr bool
        endsWith(const ViewType suffix, const SearchCase searchCase = SearchCase::CaseSensitive) const noexcept
    {
        return view().endsWith(suffix, searchCase);
    }

    /// @brief Checks if the string ends with character `ch`.
    /// @param[in] ch The character to check for.
    /// @param[in] searchCase Case-sensitivity mode.
    /// @return True if the string ends with `ch`, false otherwise.
    [[nodiscard]] constexpr bool
        endsWith(const InCharT ch, const SearchCase searchCase = SearchCase::CaseSensitive) const noexcept
    {
        return view().endsWith(ch, searchCase);
    }

    /// @brief Checks if the string contains `sub`.
    /// @param[in] sub The substring view to search for.
    /// @param[in] searchCase Case-sensitivity mode.
    /// @return True if `sub` occurs in the string, false otherwise.
    [[nodiscard]] constexpr bool
        contains(const ViewType sub, const SearchCase searchCase = SearchCase::CaseSensitive) const noexcept
    {
        return view().contains(sub, searchCase);
    }

    /// @brief Checks if the string contains character `ch`.
    /// @param[in] ch The character to search for.
    /// @param[in] searchCase Case-sensitivity mode.
    /// @return True if `ch` occurs in the string, false otherwise.
    [[nodiscard]] constexpr bool
        contains(const InCharT ch, const SearchCase searchCase = SearchCase::CaseSensitive) const noexcept
    {
        return view().contains(ch, searchCase);
    }

    /// @brief Finds the first occurrence of `sub` starting at `pos`.
    /// @param[in] sub Substring view to search for.
    /// @param[in] pos Starting index.
    /// @param[in] searchCase Case-sensitivity mode.
    /// @return Index of the match, or `npos` if not found.
    [[nodiscard]] constexpr SizeType find(
        const ViewType sub, const SizeType pos = 0u, const SearchCase searchCase = SearchCase::CaseSensitive
    ) const noexcept
    {
        return view().find(sub, pos, searchCase);
    }

    /// @brief Finds the first occurrence of character `ch` starting at `pos`.
    /// @param[in] ch Character to search for.
    /// @param[in] pos Starting index.
    /// @param[in] searchCase Case-sensitivity mode.
    /// @return Index of the match, or `npos` if not found.
    [[nodiscard]] constexpr SizeType find(
        const InCharT ch, const SizeType pos = 0u, const SearchCase searchCase = SearchCase::CaseSensitive
    ) const noexcept
    {
        return view().find(ch, pos, searchCase);
    }

    /// @brief Finds the last occurrence of `sub` at or before `pos`.
    /// @param[in] sub Substring view to search for.
    /// @param[in] pos Maximum starting index.
    /// @param[in] searchCase Case-sensitivity mode.
    /// @return Index of the match, or `npos` if not found.
    [[nodiscard]] constexpr SizeType rfind(
        const ViewType sub, const SizeType pos = npos, const SearchCase searchCase = SearchCase::CaseSensitive
    ) const noexcept
    {
        return view().rfind(sub, pos, searchCase);
    }

    /// @brief Finds the last occurrence of character `ch` at or before `pos`.
    /// @param[in] ch Character to search for.
    /// @param[in] pos Maximum starting index.
    /// @param[in] searchCase Case-sensitivity mode.
    /// @return Index of the match, or `npos` if not found.
    [[nodiscard]] constexpr SizeType rfind(
        const InCharT ch, const SizeType pos = npos, const SearchCase searchCase = SearchCase::CaseSensitive
    ) const noexcept
    {
        return view().rfind(ch, pos, searchCase);
    }

    /// @brief Finds an occurrence of `sub` in the specified `searchDir` (`FromStart` or `FromEnd`).
    /// @param[in] sub Substring view to search for.
    /// @param[in] searchCase Case-sensitivity mode.
    /// @param[in] searchDir Search direction (`SearchDir::FromStart` or `SearchDir::FromEnd`).
    /// @param[in] startPos Starting position (`0` for `FromStart`, `npos` for `FromEnd` when omitted).
    /// @return Index of the match, or `npos` if not found.
    [[nodiscard]] constexpr SizeType indexOf(
        const ViewType sub,
        const SearchCase searchCase = SearchCase::CaseSensitive,
        const SearchDir searchDir = SearchDir::FromStart,
        const SizeType startPos = npos
    ) const noexcept
    {
        return view().indexOf(sub, searchCase, searchDir, startPos);
    }

    /// @brief Finds an occurrence of character `ch` in the specified `searchDir` (`FromStart` or `FromEnd`).
    /// @param[in] ch Character to search for.
    /// @param[in] searchCase Case-sensitivity mode.
    /// @param[in] searchDir Search direction (`SearchDir::FromStart` or `SearchDir::FromEnd`).
    /// @param[in] startPos Starting position (`0` for `FromStart`, `npos` for `FromEnd` when omitted).
    /// @return Index of the match, or `npos` if not found.
    [[nodiscard]] constexpr SizeType indexOf(
        const InCharT ch,
        const SearchCase searchCase = SearchCase::CaseSensitive,
        const SearchDir searchDir = SearchDir::FromStart,
        const SizeType startPos = npos
    ) const noexcept
    {
        return view().indexOf(ch, searchCase, searchDir, startPos);
    }

    /// @brief Finds the first occurrence of any character in `chars` starting at `pos`.
    /// @param[in] chars Set of characters to search for.
    /// @param[in] pos Starting index.
    /// @param[in] searchCase Case-sensitivity mode.
    /// @return Index of the first matching character, or `npos` if not found.
    [[nodiscard]] constexpr SizeType findFirstOf(
        const ViewType chars, const SizeType pos = 0u, const SearchCase searchCase = SearchCase::CaseSensitive
    ) const noexcept
    {
        return view().findFirstOf(chars, pos, searchCase);
    }

    /// @brief Finds the last occurrence of any character in `chars` at or before `pos`.
    /// @param[in] chars Set of characters to search for.
    /// @param[in] pos Maximum index to inspect.
    /// @param[in] searchCase Case-sensitivity mode.
    /// @return Index of the last matching character, or `npos` if not found.
    [[nodiscard]] constexpr SizeType findLastOf(
        const ViewType chars, const SizeType pos = npos, const SearchCase searchCase = SearchCase::CaseSensitive
    ) const noexcept
    {
        return view().findLastOf(chars, pos, searchCase);
    }

    /// @brief Finds the first character NOT present in `chars` starting at `pos`.
    /// @param[in] chars Set of characters to skip.
    /// @param[in] pos Starting index.
    /// @param[in] searchCase Case-sensitivity mode.
    /// @return Index of the first non-matching character, or `npos` if all match.
    [[nodiscard]] constexpr SizeType findFirstNotOf(
        const ViewType chars, const SizeType pos = 0u, const SearchCase searchCase = SearchCase::CaseSensitive
    ) const noexcept
    {
        return view().findFirstNotOf(chars, pos, searchCase);
    }

    /// @brief Finds the last character NOT present in `chars` at or before `pos`.
    /// @param[in] chars Set of characters to skip.
    /// @param[in] pos Maximum index to inspect.
    /// @param[in] searchCase Case-sensitivity mode.
    /// @return Index of the last non-matching character, or `npos` if all match.
    [[nodiscard]] constexpr SizeType findLastNotOf(
        const ViewType chars, const SizeType pos = npos, const SearchCase searchCase = SearchCase::CaseSensitive
    ) const noexcept
    {
        return view().findLastNotOf(chars, pos, searchCase);
    }

    /// @brief Counts the number of non-overlapping occurrences of `sub`.
    /// @param[in] sub Substring to count.
    /// @param[in] searchCase Case-sensitivity mode.
    /// @return Number of occurrences.
    [[nodiscard]] constexpr SizeType
        countOccurrences(const ViewType sub, const SearchCase searchCase = SearchCase::CaseSensitive) const noexcept
    {
        return view().countOccurrences(sub, searchCase);
    }

    /// @brief Counts the number of occurrences of character `ch`.
    /// @param[in] ch Character to count.
    /// @param[in] searchCase Case-sensitivity mode.
    /// @return Number of occurrences.
    [[nodiscard]] constexpr SizeType
        countOccurrences(const InCharT ch, const SearchCase searchCase = SearchCase::CaseSensitive) const noexcept
    {
        return view().countOccurrences(ch, searchCase);
    }

    /// @brief Matches the string against a glob wildcard pattern (`*` and `?`).
    /// @param[in] pattern Wildcard pattern.
    /// @param[in] searchCase Case-sensitivity mode.
    /// @return True if the string matches `pattern`, false otherwise.
    [[nodiscard]] constexpr bool
        matchesWildcard(const ViewType pattern, const SearchCase searchCase = SearchCase::CaseSensitive) const noexcept
    {
        return view().matchesWildcard(pattern, searchCase);
    }

    /// @brief Returns a non-owning subview `[pos, pos + count)`.
    /// @param[in] pos Starting index.
    /// @param[in] count Maximum number of characters to include.
    /// @return Non-owning `BasicStringView<InCharT>`.
    [[nodiscard]] constexpr ViewType subview(const SizeType pos = 0u, const SizeType count = npos) const noexcept
    {
        return view().subview(pos, count);
    }

    /// @brief Returns a new owning `BasicString` containing `[pos, pos + count)`.
    /// @param[in] pos Starting index.
    /// @param[in] count Maximum number of characters to include.
    /// @return Owning substring.
    [[nodiscard]] constexpr BasicString substr(const SizeType pos = 0u, const SizeType count = npos) const noexcept
    {
        return makeFromViewWithSameAllocator(view().subview(pos, count));
    }

    /// @brief Returns an owning string containing the leftmost `count` characters.
    /// @param[in] count Number of characters from the start.
    /// @return Left substring.
    [[nodiscard]] constexpr BasicString left(const SizeType count) const noexcept
    {
        return makeFromViewWithSameAllocator(view().left(count));
    }

    /// @brief Returns an owning string containing the rightmost `count` characters.
    /// @param[in] count Number of characters from the end.
    /// @return Right substring.
    [[nodiscard]] constexpr BasicString right(const SizeType count) const noexcept
    {
        return makeFromViewWithSameAllocator(view().right(count));
    }

    /// @brief Returns an owning substring starting at `pos` of length `count` (clamped if `pos >= size()`).
    /// @param[in] pos Starting index.
    /// @param[in] count Maximum number of characters.
    /// @return Middle substring.
    [[nodiscard]] constexpr BasicString mid(const SizeType pos, const SizeType count = npos) const noexcept
    {
        return makeFromViewWithSameAllocator(view().mid(pos, count));
    }

    /// @brief Returns an owning string with the last `count` characters removed.
    /// @param[in] count Number of characters to chop from the right.
    /// @return Chopped `BasicString`.
    [[nodiscard]] constexpr BasicString leftChop(const SizeType count) const noexcept
    {
        return makeFromViewWithSameAllocator(view().leftChop(count));
    }

    /// @brief Returns an owning string with the first `count` characters removed.
    /// @param[in] count Number of characters to chop from the left.
    /// @return Chopped `BasicString`.
    [[nodiscard]] constexpr BasicString rightChop(const SizeType count) const noexcept
    {
        return makeFromViewWithSameAllocator(view().rightChop(count));
    }

    /// @brief Returns a non-owning view with leading ASCII whitespace removed.
    /// @return Trimmed `BasicStringView<InCharT>`.
    [[nodiscard]] constexpr ViewType trimStartView() const noexcept
    {
        return view().trimStart();
    }

    /// @brief Returns a non-owning view with trailing ASCII whitespace removed.
    /// @return Trimmed `BasicStringView<InCharT>`.
    [[nodiscard]] constexpr ViewType trimEndView() const noexcept
    {
        return view().trimEnd();
    }

    /// @brief Returns a non-owning view with leading and trailing ASCII whitespace removed.
    /// @return Trimmed `BasicStringView<InCharT>`.
    [[nodiscard]] constexpr ViewType trimView() const noexcept
    {
        return view().trim();
    }

    /// @brief Splits the string by character `delimiter` and invokes `callback(BasicStringView)` per token.
    /// @tparam InCallback Callable accepting `BasicStringView<InCharT>`.
    /// @param[in] delimiter Delimiter character.
    /// @param[in] options Bitmask of `StringSplitOptions`.
    /// @param[in] callback Visitor invoked for each token.
    /// @return Number of tokens emitted.
    template <typename InCallback>
    requires(!std::is_same_v<std::decay_t<InCallback>, StringSplitOptions>) constexpr SizeType
        splitView(const InCharT delimiter, const StringSplitOptions options, InCallback&& callback) const noexcept
    {
        return view().splitView(delimiter, options, std::forward<InCallback>(callback));
    }

    /// @brief Splits the string by character `delimiter` and invokes `callback(BasicStringView)` per token.
    /// @tparam InCallback Callable accepting `BasicStringView<InCharT>`.
    /// @param[in] delimiter Delimiter character.
    /// @param[in] callback Visitor invoked for each token.
    /// @param[in] options Bitmask of `StringSplitOptions`.
    /// @return Number of tokens emitted.
    template <typename InCallback>
    requires(!std::is_same_v<std::decay_t<InCallback>, StringSplitOptions>) constexpr SizeType splitView(
        const InCharT delimiter, InCallback&& callback, const StringSplitOptions options = StringSplitOptions::None
    ) const noexcept
    {
        return view().splitView(delimiter, options, std::forward<InCallback>(callback));
    }

    /// @brief Splits the string by substring `delimiter` and invokes `callback(BasicStringView)` per token.
    /// @tparam InCallback Callable accepting `BasicStringView<InCharT>`.
    /// @param[in] delimiter Delimiter substring view.
    /// @param[in] options Bitmask of `StringSplitOptions`.
    /// @param[in] searchCase Case-sensitivity mode for matching `delimiter`.
    /// @param[in] callback Visitor invoked for each token.
    /// @return Number of tokens emitted.
    template <typename InCallback>
    constexpr SizeType splitView(
        const ViewType delimiter, const StringSplitOptions options, const SearchCase searchCase, InCallback&& callback
    ) const noexcept
    {
        return view().splitView(delimiter, options, searchCase, std::forward<InCallback>(callback));
    }

    /// @brief Splits the string by substring `delimiter` and invokes `callback(BasicStringView)` per token.
    /// @tparam InCallback Callable accepting `BasicStringView<InCharT>`.
    /// @param[in] delimiter Delimiter substring view.
    /// @param[in] callback Visitor invoked for each token.
    /// @param[in] options Bitmask of `StringSplitOptions`.
    /// @param[in] searchCase Case-sensitivity mode for matching `delimiter`.
    /// @return Number of tokens emitted.
    template <typename InCallback>
    requires(!std::is_same_v<std::decay_t<InCallback>, StringSplitOptions>) constexpr SizeType splitView(
        const ViewType delimiter,
        InCallback&& callback,
        const StringSplitOptions options = StringSplitOptions::None,
        const SearchCase searchCase = SearchCase::CaseSensitive
    ) const noexcept
    {
        return view().splitView(delimiter, options, searchCase, std::forward<InCallback>(callback));
    }

    /// @brief Splits the string into lines (`\n`, `\r\n`, or `\r`) with zero allocations.
    /// @tparam InCallback Callable accepting `BasicStringView<InCharT>`.
    /// @param[in] options Bitmask of `StringSplitOptions`.
    /// @param[in] callback Visitor invoked for each line.
    /// @return Number of lines emitted.
    template <typename InCallback>
    requires(!std::is_same_v<std::decay_t<InCallback>, StringSplitOptions>)
    constexpr SizeType splitLines(const StringSplitOptions options, InCallback&& callback) const noexcept
    {
        return view().splitLines(options, std::forward<InCallback>(callback));
    }

    /// @brief Splits the string into lines (`\n`, `\r\n`, or `\r`) with zero allocations.
    /// @tparam InCallback Callable accepting `BasicStringView<InCharT>`.
    /// @param[in] callback Visitor invoked for each line.
    /// @param[in] options Bitmask of `StringSplitOptions`.
    /// @return Number of lines emitted.
    template <typename InCallback>
    requires(!std::is_same_v<std::decay_t<InCallback>, StringSplitOptions>) constexpr SizeType
        splitLines(InCallback&& callback, const StringSplitOptions options = StringSplitOptions::None) const noexcept
    {
        return view().splitLines(options, std::forward<InCallback>(callback));
    }

    /// @brief Computes the deterministic 64-bit FNV-1a hash of the string.
    /// @param[in] searchCase Case-sensitivity mode.
    /// @return 64-bit FNV-1a hash value.
    [[nodiscard]] constexpr gp::UInt64 hash(const SearchCase searchCase = SearchCase::CaseSensitive) const noexcept
    {
        return view().hash(searchCase);
    }

    /// @brief Computes the case-insensitive 64-bit FNV-1a hash of the string.
    /// @return 64-bit case-folded FNV-1a hash value.
    [[nodiscard]] constexpr gp::UInt64 hashIgnoreCase() const noexcept
    {
        return view().hashIgnoreCase();
    }

public:
    /// @brief Returns a non-owning range view that decodes `char32_t` Unicode codepoints on the fly.
    /// @return `unicode::CodepointView<InCharT>` over this string.
    [[nodiscard]] constexpr unicode::CodepointView<InCharT> codepoints() const noexcept
    {
        return unicode::CodepointView<InCharT>(view());
    }

    /// @brief Counts the number of decoded Unicode codepoints in this string.
    /// @return Decoded Unicode scalar count.
    [[nodiscard]] constexpr SizeType codepointCount() const noexcept
    {
        return unicode::countCodepoints(view());
    }

    /// @brief Checks whether this string contains strictly well-formed UTF-8/16/32 code units.
    /// @return True if every code unit sequence is valid Unicode, false otherwise.
    [[nodiscard]] constexpr bool isValidUtf() const noexcept
    {
        return unicode::isValidUtf(view());
    }

    /// @brief Transcodes a UTF-8 `char` string view into a `BasicString<InCharT, InAllocator>`.
    /// @param[in] src Source UTF-8 string view.
    /// @return Transcoded `BasicString`.
    [[nodiscard]] static constexpr BasicString fromUtf8(const BasicStringView<char> src) noexcept
    {
        BasicString result;
        unicode::convert(src, result);
        return result;
    }

    /// @brief Transcodes a UTF-8 `char8_t` string view into a `BasicString<InCharT, InAllocator>`.
    /// @param[in] src Source UTF-8 string view.
    /// @return Transcoded `BasicString`.
    [[nodiscard]] static constexpr BasicString fromUtf8(const BasicStringView<char8_t> src) noexcept
    {
        BasicString result;
        unicode::convert(src, result);
        return result;
    }

    /// @brief Transcodes a UTF-16 `char16_t` string view into a `BasicString<InCharT, InAllocator>`.
    /// @param[in] src Source UTF-16 string view.
    /// @return Transcoded `BasicString`.
    [[nodiscard]] static constexpr BasicString fromUtf16(const BasicStringView<char16_t> src) noexcept
    {
        BasicString result;
        unicode::convert(src, result);
        return result;
    }

    /// @brief Transcodes a UTF-32 `char32_t` string view into a `BasicString<InCharT, InAllocator>`.
    /// @param[in] src Source UTF-32 string view.
    /// @return Transcoded `BasicString`.
    [[nodiscard]] static constexpr BasicString fromUtf32(const BasicStringView<char32_t> src) noexcept
    {
        BasicString result;
        unicode::convert(src, result);
        return result;
    }

    /// @brief Transcodes a `wchar_t` string view into a `BasicString<InCharT, InAllocator>`.
    /// @param[in] src Source wide string view.
    /// @return Transcoded `BasicString`.
    [[nodiscard]] static constexpr BasicString fromWide(const BasicStringView<wchar_t> src) noexcept
    {
        BasicString result;
        unicode::convert(src, result);
        return result;
    }

    /// @brief Transcodes this string into a UTF-8 `BasicString<char>`.
    /// @return Transcoded UTF-8 `BasicString<char>`.
    [[nodiscard]] constexpr BasicString<char> toUtf8() const noexcept
    {
        BasicString<char> result;
        unicode::convert(view(), result);
        return result;
    }

    /// @brief Transcodes this string into a UTF-8 `BasicString<char8_t>`.
    /// @return Transcoded UTF-8 `BasicString<char8_t>`.
    [[nodiscard]] constexpr BasicString<char8_t> toU8String() const noexcept
    {
        BasicString<char8_t> result;
        unicode::convert(view(), result);
        return result;
    }

    /// @brief Transcodes this string into a UTF-16 `BasicString<char16_t>`.
    /// @return Transcoded UTF-16 `BasicString<char16_t>`.
    [[nodiscard]] constexpr BasicString<char16_t> toUtf16() const noexcept
    {
        BasicString<char16_t> result;
        unicode::convert(view(), result);
        return result;
    }

    /// @brief Transcodes this string into a UTF-32 `BasicString<char32_t>`.
    /// @return Transcoded UTF-32 `BasicString<char32_t>`.
    [[nodiscard]] constexpr BasicString<char32_t> toUtf32() const noexcept
    {
        BasicString<char32_t> result;
        unicode::convert(view(), result);
        return result;
    }

    /// @brief Transcodes this string into a wide `BasicString<wchar_t>`.
    /// @return Transcoded `BasicString<wchar_t>`.
    [[nodiscard]] constexpr BasicString<wchar_t> toWide() const noexcept
    {
        BasicString<wchar_t> result;
        unicode::convert(view(), result);
        return result;
    }

public:
    /// @brief Formats arguments into a new `BasicString` using compile-time checked `<format>` syntax without
    ///        allocating any `std::basic_string`.
    /// @tparam InArgs Format argument types.
    /// @param[in] fmt Compile-time format string.
    /// @param[in] args Arguments to format.
    /// @return Formatted `BasicString`.
    template <typename... InArgs>
    requires(std::is_same_v<InCharT, char> || std::is_same_v<InCharT, wchar_t>) [[nodiscard]] static BasicString
        format(const std::basic_format_string<InCharT, std::type_identity_t<InArgs>...> fmt, InArgs&&... args) noexcept
    {
        BasicString result;
        result.appendFormat(fmt, std::forward<InArgs>(args)...);
        return result;
    }

    /// @brief Appends formatted output directly into this string's buffer without allocating a `std::basic_string`.
    /// @tparam InArgs Format argument types.
    /// @param[in] fmt Compile-time format string.
    /// @param[in] args Arguments to format.
    /// @return Reference to `*this`.
    template <typename... InArgs>
    requires(std::is_same_v<InCharT, char> || std::is_same_v<InCharT, wchar_t>) BasicString& appendFormat(
        const std::basic_format_string<InCharT, std::type_identity_t<InArgs>...> fmt, InArgs&&... args
    ) noexcept
    {
        const SizeType formattedLen = static_cast<SizeType>(std::formatted_size(fmt, std::forward<InArgs>(args)...));
        if (formattedLen == 0u)
        {
            return *this;
        }

        const SizeType oldLen = size();
        resizeUninitialized(oldLen + formattedLen);
        (void)std::format_to_n(data() + oldLen, formattedLen, fmt, std::forward<InArgs>(args)...);
        data()[oldLen + formattedLen] = InCharT{ 0 };
        return *this;
    }

    /// @brief Converts a signed 64-bit integer to a `BasicString` using `<charconv>` (zero heap allocation for base
    /// 10).
    /// @param[in] value Signed integer value.
    /// @param[in] base Numeric base in `[2, 36]` (defaults to 10).
    /// @return Formatted `BasicString`.
    [[nodiscard]] static BasicString fromInt(const gp::Int64 value, const gp::Int32 base = 10) noexcept
    {
        char buf[68]{};
        const std::to_chars_result res = std::to_chars(buf, buf + sizeof(buf), value, static_cast<int>(base));
        if (res.ec != std::errc{})
        {
            return BasicString();
        }
        const SizeType len = static_cast<SizeType>(res.ptr - buf);
        BasicString result;
        result.resizeUninitialized(len);
        Pointer dst = result.data();
        for (SizeType i = 0u; i < len; ++i)
        {
            dst[i] = static_cast<InCharT>(buf[i]);
        }
        return result;
    }

    /// @brief Converts an unsigned 64-bit integer to a `BasicString` using `<charconv>`.
    /// @param[in] value Unsigned integer value.
    /// @param[in] base Numeric base in `[2, 36]` (defaults to 10).
    /// @return Formatted `BasicString`.
    [[nodiscard]] static BasicString fromUInt(const gp::UInt64 value, const gp::Int32 base = 10) noexcept
    {
        char buf[68]{};
        const std::to_chars_result res = std::to_chars(buf, buf + sizeof(buf), value, static_cast<int>(base));
        if (res.ec != std::errc{})
        {
            return BasicString();
        }
        const SizeType len = static_cast<SizeType>(res.ptr - buf);
        BasicString result;
        result.resizeUninitialized(len);
        Pointer dst = result.data();
        for (SizeType i = 0u; i < len; ++i)
        {
            dst[i] = static_cast<InCharT>(buf[i]);
        }
        return result;
    }

    /// @brief Converts a 64-bit floating-point number to a `BasicString` using `<charconv>`.
    /// @param[in] value Floating-point value.
    /// @param[in] precision Fixed decimal precision (`< 0` uses shortest round-trip representation).
    /// @return Formatted `BasicString`.
    [[nodiscard]] static BasicString fromFloat(const gp::Float64 value, const gp::Int32 precision = -1) noexcept
    {
        char buf[128]{};
        const std::to_chars_result res =
            (precision >= 0)
                ? std::to_chars(buf, buf + sizeof(buf), value, std::chars_format::fixed, static_cast<int>(precision))
                : std::to_chars(buf, buf + sizeof(buf), value);
        if (res.ec != std::errc{})
        {
            return BasicString();
        }
        const SizeType len = static_cast<SizeType>(res.ptr - buf);
        BasicString result;
        result.resizeUninitialized(len);
        Pointer dst = result.data();
        for (SizeType i = 0u; i < len; ++i)
        {
            dst[i] = static_cast<InCharT>(buf[i]);
        }
        return result;
    }

    /// @brief Converts a boolean value to `"true"` or `"false"`.
    /// @param[in] value Boolean value to convert.
    /// @return `"true"` if `value` is true, `"false"` otherwise.
    [[nodiscard]] static constexpr BasicString fromBool(const bool value) noexcept
    {
        if (value)
        {
            const InCharT trueChars[4] = { static_cast<InCharT>('t'),
                                           static_cast<InCharT>('r'),
                                           static_cast<InCharT>('u'),
                                           static_cast<InCharT>('e') };
            return BasicString(trueChars, 4u);
        }

        const InCharT falseChars[5] = { static_cast<InCharT>('f'),
                                        static_cast<InCharT>('a'),
                                        static_cast<InCharT>('l'),
                                        static_cast<InCharT>('s'),
                                        static_cast<InCharT>('e') };
        return BasicString(falseChars, 5u);
    }

    /// @brief Joins a range of string-view-convertible elements separated by `separator` with a single pre-sized
    ///        reservation pass.
    /// @tparam InRange Iterable range whose elements are convertible to `BasicStringView<InCharT>`.
    /// @param[in] elements Range of elements to join.
    /// @param[in] separator Delimiter inserted between consecutive elements.
    /// @return Joined `BasicString`.
    template <typename InRange>
    [[nodiscard]] static constexpr BasicString join(const InRange& elements, const ViewType separator) noexcept
    {
        SizeType totalLen = 0u;
        SizeType count = 0u;
        for (const auto& elem: elements)
        {
            const ViewType v(elem);
            totalLen += v.size();
            ++count;
        }

        if (count == 0u)
        {
            return BasicString();
        }

        totalLen += (count - 1u) * separator.size();
        BasicString result;
        result.reserve(totalLen);

        SizeType idx = 0u;
        for (const auto& elem: elements)
        {
            if (idx > 0u && !separator.isEmpty())
            {
                result.append(separator);
            }
            result.append(ViewType(elem));
            ++idx;
        }

        return result;
    }

    /// @brief Joins an initializer list of string views separated by `separator`.
    /// @param[in] elements Initializer list of string views.
    /// @param[in] separator Delimiter inserted between consecutive elements.
    /// @return Joined `BasicString`.
    [[nodiscard]] static constexpr BasicString
        join(const std::initializer_list<ViewType> elements, const ViewType separator) noexcept
    {
        return join<std::initializer_list<ViewType>>(elements, separator);
    }

public:
    /// @brief Parses the string as a 32-bit signed integer without throwing or allocating.
    /// @param[out] outValue Receives the parsed value on success.
    /// @param[in] base Numeric base in `[2, 36]`.
    /// @return True if the entire trimmed string was parsed as a valid `gp::Int32`.
    [[nodiscard]] bool tryParseInt32(gp::Int32& outValue, const gp::Int32 base = 10) const noexcept
        requires(sizeof(InCharT) == 1u)
    {
        return view().tryParseInt32(outValue, base);
    }

    /// @brief Parses the string as a 64-bit signed integer without throwing or allocating.
    /// @param[out] outValue Receives the parsed value on success.
    /// @param[in] base Numeric base in `[2, 36]`.
    /// @return True if the entire trimmed string was parsed as a valid `gp::Int64`.
    [[nodiscard]] bool tryParseInt64(gp::Int64& outValue, const gp::Int32 base = 10) const noexcept
        requires(sizeof(InCharT) == 1u)
    {
        return view().tryParseInt64(outValue, base);
    }

    /// @brief Parses the string as a 32-bit unsigned integer without throwing or allocating.
    /// @param[out] outValue Receives the parsed value on success.
    /// @param[in] base Numeric base in `[2, 36]`.
    /// @return True if the entire trimmed string was parsed as a valid `gp::UInt32`.
    [[nodiscard]] bool tryParseUInt32(gp::UInt32& outValue, const gp::Int32 base = 10) const noexcept
        requires(sizeof(InCharT) == 1u)
    {
        return view().tryParseUInt32(outValue, base);
    }

    /// @brief Parses the string as a 64-bit unsigned integer without throwing or allocating.
    /// @param[out] outValue Receives the parsed value on success.
    /// @param[in] base Numeric base in `[2, 36]`.
    /// @return True if the entire trimmed string was parsed as a valid `gp::UInt64`.
    [[nodiscard]] bool tryParseUInt64(gp::UInt64& outValue, const gp::Int32 base = 10) const noexcept
        requires(sizeof(InCharT) == 1u)
    {
        return view().tryParseUInt64(outValue, base);
    }

    /// @brief Parses the string as a 32-bit floating-point number without throwing or allocating.
    /// @param[out] outValue Receives the parsed value on success.
    /// @return True if the entire trimmed string was parsed as a valid `gp::Float32`.
    [[nodiscard]] bool tryParseFloat32(gp::Float32& outValue) const noexcept requires(sizeof(InCharT) == 1u)
    {
        return view().tryParseFloat32(outValue);
    }

    /// @brief Parses the string as a 64-bit floating-point number without throwing or allocating.
    /// @param[out] outValue Receives the parsed value on success.
    /// @return True if the entire trimmed string was parsed as a valid `gp::Float64`.
    [[nodiscard]] bool tryParseFloat64(gp::Float64& outValue) const noexcept requires(sizeof(InCharT) == 1u)
    {
        return view().tryParseFloat64(outValue);
    }

    /// @brief Parses the string as a boolean (`"true"`, `"false"`, `"1"`, `"0"`, `"yes"`, `"no"`, `"on"`, `"off"`).
    /// @param[out] outValue Receives the parsed boolean value on success.
    /// @return True if recognized, false otherwise.
    [[nodiscard]] constexpr bool tryParseBool(bool& outValue) const noexcept
    {
        return view().tryParseBool(outValue);
    }

    /// @brief Parses the string as a 32-bit signed integer in base 10 and returns an `Optional<gp::Int32>`.
    /// @return `Optional<gp::Int32>` containing the parsed value, or `nullopt` on failure.
    [[nodiscard]] Optional<gp::Int32> tryParseInt32() const noexcept requires(sizeof(InCharT) == 1u)
    {
        return view().tryParseInt32();
    }

    /// @brief Parses the string as a 64-bit signed integer in base 10 and returns an `Optional<gp::Int64>`.
    /// @return `Optional<gp::Int64>` containing the parsed value, or `nullopt` on failure.
    [[nodiscard]] Optional<gp::Int64> tryParseInt64() const noexcept requires(sizeof(InCharT) == 1u)
    {
        return view().tryParseInt64();
    }

    /// @brief Parses the string as a 32-bit unsigned integer in base 10 and returns an `Optional<gp::UInt32>`.
    /// @return `Optional<gp::UInt32>` containing the parsed value, or `nullopt` on failure.
    [[nodiscard]] Optional<gp::UInt32> tryParseUInt32() const noexcept requires(sizeof(InCharT) == 1u)
    {
        return view().tryParseUInt32();
    }

    /// @brief Parses the string as a 64-bit unsigned integer in base 10 and returns an `Optional<gp::UInt64>`.
    /// @return `Optional<gp::UInt64>` containing the parsed value, or `nullopt` on failure.
    [[nodiscard]] Optional<gp::UInt64> tryParseUInt64() const noexcept requires(sizeof(InCharT) == 1u)
    {
        return view().tryParseUInt64();
    }

    /// @brief Parses the string as a 32-bit floating-point number and returns an `Optional<gp::Float32>`.
    /// @return `Optional<gp::Float32>` containing the parsed value, or `nullopt` on failure.
    [[nodiscard]] Optional<gp::Float32> tryParseFloat32() const noexcept requires(sizeof(InCharT) == 1u)
    {
        return view().tryParseFloat32();
    }

    /// @brief Parses the string as a 64-bit floating-point number and returns an `Optional<gp::Float64>`.
    /// @return `Optional<gp::Float64>` containing the parsed value, or `nullopt` on failure.
    [[nodiscard]] Optional<gp::Float64> tryParseFloat64() const noexcept requires(sizeof(InCharT) == 1u)
    {
        return view().tryParseFloat64();
    }

    /// @brief Parses the string as a boolean and returns an `Optional<bool>`.
    /// @return `Optional<bool>` containing the parsed boolean value, or `nullopt` on failure.
    [[nodiscard]] constexpr Optional<bool> tryParseBool() const noexcept
    {
        return view().tryParseBool();
    }

public:
    /// @brief Compares two `BasicString` instances (possibly with different allocators) for equality.
    /// @tparam InOtherAllocator Right-hand string's allocator type.
    /// @param[in] other The string to compare with.
    /// @return True if both strings have identical code units, false otherwise.
    template <concepts::IsAllocator InOtherAllocator>
    [[nodiscard]] constexpr bool operator==(const BasicString<InCharT, InOtherAllocator>& other) const noexcept
    {
        return view() == other.view();
    }

    /// @brief Compares this string with a `BasicStringView` for equality.
    /// @param[in] other The string view to compare with.
    /// @return True if equal, false otherwise.
    [[nodiscard]] constexpr bool operator==(const ViewType other) const noexcept
    {
        return view() == other;
    }

    /// @brief Compares this string with a null-terminated C-string for equality.
    /// @param[in] rhs The null-terminated C-string to compare with.
    /// @return True if equal, false otherwise.
    [[nodiscard]] constexpr bool operator==(const InCharT* const rhs) const noexcept
    {
        return view() == ViewType(rhs);
    }

    /// @brief Compares two `BasicString` instances for three-way lexicographical ordering.
    /// @tparam InOtherAllocator Right-hand string's allocator type.
    /// @param[in] other The string to compare with.
    /// @return `std::strong_ordering` result.
    template <concepts::IsAllocator InOtherAllocator>
    [[nodiscard]] constexpr std::strong_ordering
        operator<=>(const BasicString<InCharT, InOtherAllocator>& other) const noexcept
    {
        return view() <=> other.view();
    }

    /// @brief Compares this string with a `BasicStringView` for three-way lexicographical ordering.
    /// @param[in] other The string view to compare with.
    /// @return `std::strong_ordering` result.
    [[nodiscard]] constexpr std::strong_ordering operator<=>(const ViewType other) const noexcept
    {
        return view() <=> other;
    }

    /// @brief Compares this string with a null-terminated C-string for three-way lexicographical ordering.
    /// @param[in] rhs The null-terminated C-string to compare with.
    /// @return `std::strong_ordering` result.
    [[nodiscard]] constexpr std::strong_ordering operator<=>(const InCharT* const rhs) const noexcept
    {
        return view() <=> ViewType(rhs);
    }

private:
    struct HeapLayout
    {
        Pointer data{ nullptr };
        SizeType size{ 0u };
        SizeType capacityAndFlag{ 0u };
    };

    struct SsoLayoutNoPad
    {
        ValueType buffer[kSsoMaxUnits]{};
        gp::UInt8 ssoSizeAndFlag{ 0u };
    };

    struct SsoLayoutPadded
    {
        ValueType buffer[kSsoMaxUnits]{};
        gp::UInt8 padding[kSsoPaddingBytes > 0u ? kSsoPaddingBytes : 1u]{};
        gp::UInt8 ssoSizeAndFlag{ 0u };
    };

    using SsoLayout = std::conditional_t<kSsoPaddingBytes == 0u, SsoLayoutNoPad, SsoLayoutPadded>;

    static_assert(sizeof(HeapLayout) == kTotalBytes, "BasicString::HeapLayout must be exactly 24 bytes.");
    static_assert(sizeof(SsoLayout) == kTotalBytes, "BasicString::SsoLayout must be exactly 24 bytes.");

    union Storage
    {
        SsoLayout sso{};
        HeapLayout heap;
    };

    Storage m_storage{};
    GP_NO_UNIQUE_ADDRESS InAllocator m_allocator{};

private:
    /// @brief Creates an empty `BasicString` propagating `m_allocator` when copy-constructible.
    /// @return Empty `BasicString`.
    [[nodiscard]] constexpr BasicString makeEmptyWithSameAllocator() const noexcept
    {
        if constexpr (std::is_copy_constructible_v<InAllocator>)
        {
            return BasicString(m_allocator);
        }
        else
        {
            return BasicString();
        }
    }

    /// @brief Creates a `BasicString` from `sv` propagating `m_allocator` when copy-constructible.
    /// @param[in] sv Source string view.
    /// @return Constructed `BasicString`.
    [[nodiscard]] constexpr BasicString makeFromViewWithSameAllocator(const ViewType sv) const noexcept
    {
        if constexpr (std::is_copy_constructible_v<InAllocator>)
        {
            return BasicString(sv, m_allocator);
        }
        else
        {
            return BasicString(sv);
        }
    }

    /// @brief Sets the active length when the string is in SSO mode.
    /// @param[in] newSize New SSO length (must be `<= kSsoCapacity`).
    constexpr void setSsoSize(const SizeType newSize) noexcept
    {
        GP_ASSERT(newSize <= kSsoCapacity);
        m_storage.sso.ssoSizeAndFlag = static_cast<gp::UInt8>(newSize);
        m_storage.sso.buffer[newSize] = InCharT{ 0 };
    }

    /// @brief Sets the active state when the string is in Long (Heap/Arena) mode.
    /// @param[in] ptr Allocated character buffer.
    /// @param[in] newSize Active string length in code units.
    /// @param[in] newCap Maximum capacity in code units (excluding null terminator).
    constexpr void setLongState(const Pointer ptr, const SizeType newSize, const SizeType newCap) noexcept
    {
        m_storage.heap.data = ptr;
        m_storage.heap.size = newSize;
        m_storage.heap.capacityAndFlag = newCap | kLongFlagMask;
        ptr[newSize] = InCharT{ 0 };
    }

    /// @brief Updates the active length in whichever mode (SSO or Long) is currently active, and writes null
    /// terminator.
    /// @param[in] newSize New active length in code units.
    constexpr void setInternalSize(const SizeType newSize) noexcept
    {
        if (isLong())
        {
            m_storage.heap.size = newSize;
            m_storage.heap.data[newSize] = InCharT{ 0 };
        }
        else
        {
            setSsoSize(newSize);
        }
    }

    /// @brief Resets the union to an empty SSO state without deallocating (caller must free any heap block first).
    constexpr void initSsoEmpty() noexcept
    {
        m_storage.sso = SsoLayout{};
    }

    /// @brief Deallocates the currently owned Long buffer via `m_allocator`.
    void deallocateLongBuffer() noexcept
    {
        if (isLong() && m_storage.heap.data != nullptr)
        {
            const SizeType oldCap = m_storage.heap.capacityAndFlag & ~kLongFlagMask;
            const SizeType oldAllocBytes = (oldCap + 1u) * sizeof(InCharT);
            m_allocator.deallocate(m_storage.heap.data, oldAllocBytes, kAllocAlignment);
        }
    }

    /// @brief Ensures capacity is at least `minCapacity`, using geometric growth when expanding.
    /// @param[in] minCapacity Minimum required capacity in code units (excluding null terminator).
    constexpr void growToFit(const SizeType minCapacity) noexcept
    {
        const SizeType curCap = capacity();
        if (minCapacity <= curCap)
        {
            return;
        }

        const SizeType currentElementsWithNull = isLong() ? (curCap + 1u) : 0u;
        const SizeType requestedElementsWithNull = minCapacity + 1u;
        SizeType targetElementsWithNull = gp::memory::calculateSlackGrow<SizeType>(
            m_allocator, requestedElementsWithNull, currentElementsWithNull, sizeof(InCharT), true, kAllocAlignment
        );

        if (targetElementsWithNull < requestedElementsWithNull)
        {
            targetElementsWithNull = requestedElementsWithNull;
        }

        reallocateToCapacity(targetElementsWithNull - 1u);
    }

    /// @brief Reallocates or transitions storage to hold `targetCapacity` code units (excluding null terminator).
    /// @param[in] targetCapacity Exact desired capacity (`> kSsoCapacity`).
    void reallocateToCapacity(const SizeType targetCapacity) noexcept
    {
        GP_ASSERT(targetCapacity > kSsoCapacity);
        const SizeType curSize = size();
        const SizeType newAllocBytes = (targetCapacity + 1u) * sizeof(InCharT);

        if (isLong())
        {
            const SizeType oldCap = m_storage.heap.capacityAndFlag & ~kLongFlagMask;
            const SizeType oldAllocBytes = (oldCap + 1u) * sizeof(InCharT);
            Pointer newData = static_cast<Pointer>(
                m_allocator.reallocate(m_storage.heap.data, oldAllocBytes, newAllocBytes, kAllocAlignment)
            );
            GP_ASSERT(newData != nullptr && "BasicString allocation failed");
            setLongState(newData, curSize, targetCapacity);
        }
        else
        {
            Pointer newData = static_cast<Pointer>(m_allocator.allocate(newAllocBytes, kAllocAlignment));
            GP_ASSERT(newData != nullptr && "BasicString allocation failed");
            if (curSize > 0u)
            {
                TraitsType::copy(newData, m_storage.sso.buffer, curSize);
            }
            setLongState(newData, curSize, targetCapacity);
        }
    }

    /// @brief Checks whether `ptr` points into `[data(), data() + selfLen]` and outputs its offset if so.
    /// @details Uses pointer equality during constant evaluation to avoid relational comparisons on unrelated objects.
    /// @param[in] ptr Candidate pointer to test.
    /// @param[in] selfLen Active string length to test against.
    /// @param[out] outOffset Receives the element offset `ptr - data()` when `ptr` aliases `this`.
    /// @return True if `ptr` points inside `[data(), data() + selfLen]`, false otherwise.
    [[nodiscard]] constexpr bool
        tryGetSelfAliasOffset(const ConstPointer ptr, const SizeType selfLen, SizeType& outOffset) const noexcept
    {
        if (ptr == nullptr)
        {
            return false;
        }

        const ConstPointer curBegin = data();
        if (std::is_constant_evaluated())
        {
            for (SizeType i = 0u; i <= selfLen; ++i)
            {
                if (curBegin + i == ptr)
                {
                    outOffset = i;
                    return true;
                }
            }
            return false;
        }
        else
        {
            const ConstPointer curEnd = curBegin + selfLen;
            if ((ptr >= curBegin) && (ptr <= curEnd))
            {
                outOffset = static_cast<SizeType>(ptr - curBegin);
                return true;
            }
            return false;
        }
    }
};

static_assert(sizeof(BasicString<char, gp::memory::DefaultAllocator>) == 24u, "gp::String must be exactly 24 bytes.");
static_assert(
    sizeof(BasicString<wchar_t, gp::memory::DefaultAllocator>) == 24u, "gp::WString must be exactly 24 bytes."
);
static_assert(
    sizeof(BasicString<char8_t, gp::memory::DefaultAllocator>) == 24u, "gp::U8String must be exactly 24 bytes."
);
static_assert(
    sizeof(BasicString<char16_t, gp::memory::DefaultAllocator>) == 24u, "gp::U16String must be exactly 24 bytes."
);
static_assert(
    sizeof(BasicString<char32_t, gp::memory::DefaultAllocator>) == 24u, "gp::U32String must be exactly 24 bytes."
);

/// @brief Concatenates two `BasicString` instances.
/// @tparam InCharT Character type.
/// @tparam InLhsAlloc Left-hand allocator type.
/// @tparam InRhsAlloc Right-hand allocator type.
/// @param[in] lhs Left-hand string.
/// @param[in] rhs Right-hand string.
/// @return Concatenated `BasicString<InCharT, InLhsAlloc>`.
template <concepts::IsCharacter InCharT, concepts::IsAllocator InLhsAlloc, concepts::IsAllocator InRhsAlloc>
[[nodiscard]] constexpr BasicString<InCharT, InLhsAlloc>
    operator+(const BasicString<InCharT, InLhsAlloc>& lhs, const BasicString<InCharT, InRhsAlloc>& rhs) noexcept
{
    if constexpr (std::is_copy_constructible_v<InLhsAlloc>)
    {
        BasicString<InCharT, InLhsAlloc> result(lhs.getAllocator());
        result.reserve(lhs.size() + rhs.size());
        result.append(lhs.view());
        result.append(rhs.view());
        return result;
    }
    else
    {
        BasicString<InCharT, InLhsAlloc> result;
        result.reserve(lhs.size() + rhs.size());
        result.append(lhs.view());
        result.append(rhs.view());
        return result;
    }
}

/// @brief Concatenates a `BasicString` and a `BasicStringView`.
/// @tparam InCharT Character type.
/// @tparam InAllocator Allocator type.
/// @param[in] lhs Left-hand string.
/// @param[in] rhs Right-hand string view.
/// @return Concatenated `BasicString`.
template <concepts::IsCharacter InCharT, concepts::IsAllocator InAllocator>
[[nodiscard]] constexpr BasicString<InCharT, InAllocator>
    operator+(const BasicString<InCharT, InAllocator>& lhs, const BasicStringView<InCharT> rhs) noexcept
{
    if constexpr (std::is_copy_constructible_v<InAllocator>)
    {
        BasicString<InCharT, InAllocator> result(lhs.getAllocator());
        result.reserve(lhs.size() + rhs.size());
        result.append(lhs.view());
        result.append(rhs);
        return result;
    }
    else
    {
        BasicString<InCharT, InAllocator> result;
        result.reserve(lhs.size() + rhs.size());
        result.append(lhs.view());
        result.append(rhs);
        return result;
    }
}

/// @brief Concatenates a `BasicStringView` and a `BasicString`.
/// @tparam InCharT Character type.
/// @tparam InAllocator Allocator type.
/// @param[in] lhs Left-hand string view.
/// @param[in] rhs Right-hand string.
/// @return Concatenated `BasicString`.
template <concepts::IsCharacter InCharT, concepts::IsAllocator InAllocator>
[[nodiscard]] constexpr BasicString<InCharT, InAllocator>
    operator+(const BasicStringView<InCharT> lhs, const BasicString<InCharT, InAllocator>& rhs) noexcept
{
    if constexpr (std::is_copy_constructible_v<InAllocator>)
    {
        BasicString<InCharT, InAllocator> result(rhs.getAllocator());
        result.reserve(lhs.size() + rhs.size());
        result.append(lhs);
        result.append(rhs.view());
        return result;
    }
    else
    {
        BasicString<InCharT, InAllocator> result;
        result.reserve(lhs.size() + rhs.size());
        result.append(lhs);
        result.append(rhs.view());
        return result;
    }
}

/// @brief Concatenates a `BasicString` and a null-terminated C-string.
/// @tparam InCharT Character type.
/// @tparam InAllocator Allocator type.
/// @param[in] lhs Left-hand string.
/// @param[in] rhs Right-hand C-string.
/// @return Concatenated `BasicString`.
template <concepts::IsCharacter InCharT, concepts::IsAllocator InAllocator>
[[nodiscard]] constexpr BasicString<InCharT, InAllocator>
    operator+(const BasicString<InCharT, InAllocator>& lhs, const InCharT* const rhs) noexcept
{
    return lhs + BasicStringView<InCharT>(rhs);
}

/// @brief Concatenates a null-terminated C-string and a `BasicString`.
/// @tparam InCharT Character type.
/// @tparam InAllocator Allocator type.
/// @param[in] lhs Left-hand C-string.
/// @param[in] rhs Right-hand string.
/// @return Concatenated `BasicString`.
template <concepts::IsCharacter InCharT, concepts::IsAllocator InAllocator>
[[nodiscard]] constexpr BasicString<InCharT, InAllocator>
    operator+(const InCharT* const lhs, const BasicString<InCharT, InAllocator>& rhs) noexcept
{
    return BasicStringView<InCharT>(lhs) + rhs;
}

/// @brief Concatenates a `BasicString` and a single character.
/// @tparam InCharT Character type.
/// @tparam InAllocator Allocator type.
/// @param[in] lhs Left-hand string.
/// @param[in] rhs Right-hand character.
/// @return Concatenated `BasicString`.
template <concepts::IsCharacter InCharT, concepts::IsAllocator InAllocator>
[[nodiscard]] constexpr BasicString<InCharT, InAllocator>
    operator+(const BasicString<InCharT, InAllocator>& lhs, const InCharT rhs) noexcept
{
    if constexpr (std::is_copy_constructible_v<InAllocator>)
    {
        BasicString<InCharT, InAllocator> result(lhs.getAllocator());
        result.reserve(lhs.size() + 1u);
        result.append(lhs.view());
        result.pushBack(rhs);
        return result;
    }
    else
    {
        BasicString<InCharT, InAllocator> result;
        result.reserve(lhs.size() + 1u);
        result.append(lhs.view());
        result.pushBack(rhs);
        return result;
    }
}

/// @brief Concatenates a single character and a `BasicString`.
/// @tparam InCharT Character type.
/// @tparam InAllocator Allocator type.
/// @param[in] lhs Left-hand character.
/// @param[in] rhs Right-hand string.
/// @return Concatenated `BasicString`.
template <concepts::IsCharacter InCharT, concepts::IsAllocator InAllocator>
[[nodiscard]] constexpr BasicString<InCharT, InAllocator>
    operator+(const InCharT lhs, const BasicString<InCharT, InAllocator>& rhs) noexcept
{
    if constexpr (std::is_copy_constructible_v<InAllocator>)
    {
        BasicString<InCharT, InAllocator> result(rhs.getAllocator());
        result.reserve(1u + rhs.size());
        result.pushBack(lhs);
        result.append(rhs.view());
        return result;
    }
    else
    {
        BasicString<InCharT, InAllocator> result;
        result.reserve(1u + rhs.size());
        result.pushBack(lhs);
        result.append(rhs.view());
        return result;
    }
}

/// @brief Swaps two `BasicString` instances.
/// @tparam InCharT Character type.
/// @tparam InAllocator Allocator type.
/// @param[in,out] lhs First string.
/// @param[in,out] rhs Second string.
template <concepts::IsCharacter InCharT, concepts::IsAllocator InAllocator>
constexpr void swap(BasicString<InCharT, InAllocator>& lhs, BasicString<InCharT, InAllocator>& rhs) noexcept
{
    lhs.swap(rhs);
}

/// @brief Stream insertion operator for `BasicString` (supports GoogleTest diagnostic printing).
/// @tparam InCharT Character type.
/// @tparam InAllocator Allocator type.
/// @param[in,out] os Output stream.
/// @param[in] str String to write.
/// @return Reference to `os`.
template <concepts::IsCharacter InCharT, concepts::IsAllocator InAllocator>
inline std::basic_ostream<InCharT>&
    operator<<(std::basic_ostream<InCharT>& os, const BasicString<InCharT, InAllocator>& str)
{
    return os.write(str.data(), static_cast<std::streamsize>(str.size()));
}

}   // namespace gp::container

namespace gp::concepts
{

namespace detail
{

template <typename InType>
struct IsBasicString : std::false_type
{};

template <IsCharacter InCharT, IsAllocator InAllocator>
struct IsBasicString<gp::container::BasicString<InCharT, InAllocator>> : std::true_type
{};

}   // namespace detail

/// @brief Concept satisfied by any instantiation of `gp::container::BasicString`.
template <typename InType>
concept IsString = detail::IsBasicString<std::remove_cvref_t<InType>>::value;

}   // namespace gp::concepts

namespace std
{

/// @brief `std::formatter` specialization for `gp::container::BasicString`.
template <gp::concepts::IsCharacter InCharT, gp::concepts::IsAllocator InAllocator>
struct formatter<gp::container::BasicString<InCharT, InAllocator>, InCharT>
    : formatter<basic_string_view<InCharT>, InCharT>
{
    /// @brief Formats `str` by delegating to `std::basic_string_view<InCharT>`.
    /// @tparam InFormatContext Format context type.
    /// @param[in] str The string to format.
    /// @param[in,out] ctx The format context.
    /// @return Iterator past the end of the formatted output.
    template <typename InFormatContext>
    auto format(const gp::container::BasicString<InCharT, InAllocator>& str, InFormatContext& ctx) const
    {
        return formatter<basic_string_view<InCharT>, InCharT>::format(
            basic_string_view<InCharT>(str.data(), str.size()), ctx
        );
    }
};

}   // namespace std
