// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Concepts.hpp"
#include "CoreMinimal.hpp"
#include "memory/Memory.hpp"
#include <new>   // IWYU pragma: keep
#include <utility>

namespace gp::memory
{

/// @brief Default-constructs a contiguous range of elements in uninitialized memory.
/// @details For trivially constructible types, this is a zero-cost no-op (leaving memory uninitialized for immediate
///          overwrite). For non-trivial types, invokes the default constructor via placement new on each element.
/// @tparam InElementType Element type to construct.
/// @tparam InSizeType Integral count type.
/// @param[in,out] address Pointer to the uninitialized destination buffer.
/// @param[in] count Number of elements to construct.
template <typename InElementType, concepts::IsIntegral InSizeType>
GP_FORCEINLINE_HINT constexpr void defaultConstructItems(void* address, const InSizeType count) noexcept
{
    if constexpr (!concepts::IsTriviallyConstructible<InElementType>)
    {
        InElementType* element = static_cast<InElementType*>(address);
        for (InSizeType i = 0; i < count; ++i)
        {
            ::new (static_cast<void*>(element + i)) InElementType;
        }
    }
    else
    {
        (void)address;
        (void)count;
    }
}

/// @brief Value-constructs (zero-initializes trivial types) a contiguous range of elements in uninitialized memory.
/// @details Trivially constructible types are bulk-zeroed via `zeroMemory`. Non-trivial types invoke `InElementType()`
///          via placement new.
/// @tparam InElementType Element type to value-construct.
/// @tparam InSizeType Integral count type.
/// @param[in,out] address Pointer to the uninitialized destination buffer.
/// @param[in] count Number of elements to construct.
template <typename InElementType, concepts::IsIntegral InSizeType>
GP_FORCEINLINE_HINT void valueConstructItems(void* address, const InSizeType count) noexcept
{
    if (count <= 0 || address == nullptr) [[unlikely]]
    {
        return;
    }

    if constexpr (concepts::IsTriviallyConstructible<InElementType>)
    {
        gp::memory::zeroMemory(address, static_cast<gp::USize>(count) * sizeof(InElementType));
    }
    else
    {
        InElementType* element = static_cast<InElementType*>(address);
        for (InSizeType i = 0; i < count; ++i)
        {
            ::new (static_cast<void*>(element + i)) InElementType();
        }
    }
}

/// @brief Destructs a single element in memory without freeing the underlying storage.
/// @details Compiles to a no-op if `InElementType` is trivially destructible.
/// @tparam InElementType Element type to destruct.
/// @param[in,out] element Pointer to the live element to destruct.
template <typename InElementType>
GP_FORCEINLINE_HINT constexpr void destructItem(InElementType* element) noexcept
{
    if constexpr (!concepts::IsTriviallyDestructible<InElementType>)
    {
        if (element != nullptr) [[likely]]
        {
            using ElementTypeAlias = InElementType;
            element->ElementTypeAlias::~ElementTypeAlias();
        }
    }
    else
    {
        (void)element;
    }
}

/// @brief Destructs a contiguous range of elements in memory without freeing the underlying storage.
/// @details Compiles to a no-op if `InElementType` is trivially destructible.
/// @tparam InElementType Element type to destruct.
/// @tparam InSizeType Integral count type.
/// @param[in,out] elements Pointer to the first element in the range.
/// @param[in] count Number of elements to destruct.
template <typename InElementType, concepts::IsIntegral InSizeType>
GP_FORCEINLINE_HINT constexpr void destructItems(InElementType* elements, const InSizeType count) noexcept
{
    if constexpr (!concepts::IsTriviallyDestructible<InElementType>)
    {
        if (elements == nullptr || count <= 0) [[unlikely]]
        {
            return;
        }

        using ElementTypeAlias = InElementType;
        for (InSizeType i = 0; i < count; ++i)
        {
            elements[i].ElementTypeAlias::~ElementTypeAlias();
        }
    }
    else
    {
        (void)elements;
        (void)count;
    }
}

/// @brief Copy-constructs a range of elements into uninitialized memory from a source range.
/// @details Optimizes to a single `copyMemory` (`memcpy`) call when the source and destination types are identical and
///          bitwise copyable.
/// @tparam InDestinationType Destination element type.
/// @tparam InSourceType Source element type.
/// @tparam InSizeType Integral count type.
/// @param[out] destination Uninitialized destination buffer.
/// @param[in] source Source array of elements to copy from.
/// @param[in] count Number of elements to construct.
template <typename InDestinationType, typename InSourceType, concepts::IsIntegral InSizeType>
GP_FORCEINLINE_HINT void constructItems(void* destination, const InSourceType* source, const InSizeType count) noexcept
{
    if (count <= 0 || destination == nullptr || source == nullptr) [[unlikely]]
    {
        return;
    }

    if constexpr (concepts::IsSameAs<InDestinationType, InSourceType> && concepts::IsBitwiseCopyable<InDestinationType>)
    {
        gp::memory::copyMemory(destination, source, static_cast<gp::USize>(count) * sizeof(InDestinationType));
    }
    else
    {
        InDestinationType* destElement = static_cast<InDestinationType*>(destination);
        for (InSizeType i = 0; i < count; ++i)
        {
            ::new (static_cast<void*>(destElement + i)) InDestinationType(source[i]);
        }
    }
}

/// @brief Move-constructs a range of elements into uninitialized memory from a mutable source range.
/// @details Optimizes to `moveMemory` (`memmove`) for bitwise copyable types; otherwise invokes the move constructor
///          per element while leaving source elements in a valid moved-from state.
/// @tparam InElementType Element type to move-construct.
/// @tparam InSizeType Integral count type.
/// @param[out] destination Uninitialized destination buffer.
/// @param[in,out] source Source array of elements to move from.
/// @param[in] count Number of elements to move-construct.
template <typename InElementType, concepts::IsIntegral InSizeType>
GP_FORCEINLINE_HINT void moveConstructItems(void* destination, InElementType* source, const InSizeType count) noexcept
{
    if (count <= 0 || destination == nullptr || source == nullptr) [[unlikely]]
    {
        return;
    }

    if constexpr (concepts::IsBitwiseCopyable<InElementType>)
    {
        gp::memory::moveMemory(destination, source, static_cast<gp::USize>(count) * sizeof(InElementType));
    }
    else
    {
        InElementType* destElement = static_cast<InElementType*>(destination);
        for (InSizeType i = 0; i < count; ++i)
        {
            ::new (static_cast<void*>(destElement + i)) InElementType(std::move(source[i]));
        }
    }
}

/// @brief Destructively relocates a contiguous range of elements to a new uninitialized memory buffer.
/// @details Implements the AAA engine "destructive move" pattern: for `IsRelocatable` types, performs a single bulk
///          `moveMemory` without invoking per-element move constructors or destructors. For non-relocatable types,
///          move-constructs each element at the destination and immediately invokes the destructor on the source.
/// @tparam InDestinationType Destination element type.
/// @tparam InSourceType Source element type.
/// @tparam InSizeType Integral count type.
/// @param[out] destination Uninitialized destination buffer.
/// @param[in,out] source Source array of elements to relocate and end lifetime of.
/// @param[in] count Number of elements to relocate.
template <typename InDestinationType, typename InSourceType, concepts::IsIntegral InSizeType>
GP_FORCEINLINE_HINT void
    relocateConstructItems(void* destination, InSourceType* source, const InSizeType count) noexcept
{
    static_assert(!concepts::IsConst<InSourceType>, "relocateConstructItems: source elements cannot be const.");

    if (count <= 0 || destination == nullptr || source == nullptr) [[unlikely]]
    {
        return;
    }

    if constexpr (concepts::IsSameAs<InDestinationType, InSourceType> && concepts::IsRelocatable<InDestinationType>)
    {
        gp::memory::moveMemory(destination, source, static_cast<gp::USize>(count) * sizeof(InDestinationType));
    }
    else
    {
        using SourceTypeAlias = InSourceType;
        InDestinationType* destElement = static_cast<InDestinationType*>(destination);
        for (InSizeType i = 0; i < count; ++i)
        {
            ::new (static_cast<void*>(destElement + i)) InDestinationType(std::move(source[i]));
            source[i].SourceTypeAlias::~SourceTypeAlias();
        }
    }
}

}   // namespace gp::memory
