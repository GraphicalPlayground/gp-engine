// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include <concepts>
#include <type_traits>

namespace gp::memory
{

namespace detail
{

template <typename Alloc, typename = void>
struct HasConstruct : std::false_type
{};

template <typename Alloc>
struct HasConstruct<
    Alloc,
    std::void_t<decltype(std::declval<Alloc&>().construct(std::declval<typename Alloc::ValueType*>()))>>
    : std::true_type
{};

template <typename Alloc, typename = void>
struct HasDestroy : std::false_type
{};

template <typename Alloc>
struct HasDestroy<
    Alloc,
    std::void_t<decltype(std::declval<Alloc&>().destroy(std::declval<typename Alloc::ValueType*>()))>> : std::true_type
{};

template <typename Alloc, typename = void>
struct HasMaxSize : std::false_type
{};

template <typename Alloc>
struct HasMaxSize<Alloc, std::void_t<decltype(std::declval<const Alloc&>().maxSize())>> : std::true_type
{};

template <typename Alloc, typename = void>
struct HasSelectOnContainerCopyConstruction : std::false_type
{};

template <typename Alloc>
struct HasSelectOnContainerCopyConstruction<
    Alloc,
    std::void_t<decltype(std::declval<const Alloc&>().selectOnContainerCopyConstruction())>> : std::true_type
{};

template <typename, typename = void>
struct HasPropagateOnCopy : std::false_type
{};

template <typename Alloc>
struct HasPropagateOnCopy<Alloc, std::void_t<typename Alloc::PropagateOnContainerCopyAssignment>> : std::true_type
{};

template <typename, typename = void>
struct HasPropagateOnMove : std::false_type
{};

template <typename Alloc>
struct HasPropagateOnMove<Alloc, std::void_t<typename Alloc::PropagateOnContainerMoveAssignment>> : std::true_type
{};

template <typename, typename = void>
struct HasPropagateOnSwap : std::false_type
{};

template <typename Alloc>
struct HasPropagateOnSwap<Alloc, std::void_t<typename Alloc::PropagateOnContainerSwap>> : std::true_type
{};

template <typename, typename = void>
struct HasIsAlwaysEqual : std::false_type
{};

template <typename Alloc>
struct HasIsAlwaysEqual<Alloc, std::void_t<typename Alloc::IsAlwaysEqual>> : std::true_type
{};

}   // namespace detail

/// @brief Allocator traits class template. Provides a uniform interface to access allocator properties and operations.
/// @tparam Alloc Allocator type.
template <typename Alloc>
struct AllocatorTraits
{
    using AllocatorType = Alloc;
    using ValueType = typename Alloc::value_type;
    using Pointer = ValueType*;
    using ConstPointer = const ValueType*;
    using VoidPointer = void*;
    using ConstVoidPointer = const void*;
    using SizeType = USize;
    using DifferenceType = ISize;

    using PropagateOnContainerCopyAssignment = typename std::conditional_t<
        detail::HasPropagateOnCopy<Alloc>::value,
        typename Alloc::PropagateOnContainerCopyAssignment,
        std::false_type>;

    using PropagateOnContainerMoveAssignment = typename std::conditional_t<
        detail::HasPropagateOnMove<Alloc>::value,
        typename Alloc::PropagateOnContainerMoveAssignment,
        std::true_type>;

    using PropagateOnContainerSwap = typename std::conditional_t<
        detail::HasPropagateOnSwap<Alloc>::value,
        typename Alloc::PropagateOnContainerSwap,
        std::false_type>;

    using IsAlwaysEqual = typename std::conditional_t<
        detail::HasIsAlwaysEqual<Alloc>::value,
        typename Alloc::IsAlwaysEqual,
        typename std::is_empty<Alloc>::type>;

    /// @brief Allocates memory for n objects of type value_type using the allocator alloc.
    /// @param[in] alloc The allocator to use for memory allocation.
    /// @param[in] n The number of objects to allocate memory for.
    /// @return A pointer to the allocated memory.
    GP_NODISCARD static Pointer allocate(Alloc& alloc, SizeType n) { return alloc.allocate(n); }

    static void deallocate(Alloc& alloc, Pointer ptr, SizeType n) { alloc.deallocate(ptr, n); }

    /// @brief Constructs an object of type T at the location pointed to by ptr using the allocator alloc and the
    /// provided arguments.
    /// @tparam T The type of the object to construct.
    /// @tparam Args The types of the arguments to forward to the constructor of T.
    /// @param[in] alloc The allocator to use for construction.
    /// @param[in] ptr The pointer to the memory location where the object should be constructed.
    /// @param[in] args The arguments to forward to the constructor of T.
    template <typename T, typename... Args>
    static void construct(Alloc& alloc, T* ptr, Args&&... args)
    {
        if constexpr (detail::HasConstruct<Alloc>::value) { alloc.construct(ptr, static_cast<Args&&>(args)...); }
        else { ::new (static_cast<void*>(ptr)) T(static_cast<Args&&>(args)...); }
    }

    /// @brief Destroys the object of type T at the location pointed to by ptr using the allocator alloc.
    /// @tparam T The type of the object to destroy.
    /// @param[in] alloc The allocator to use for destruction.
    /// @param[in] ptr The pointer to the memory location where the object should be destroyed.
    template <typename T>
    static void destroy(Alloc& alloc, T* ptr)
    {
        if constexpr (detail::HasDestroy<Alloc>::value) { alloc.destroy(ptr); }
        else { ptr->~T(); }
    }

    /// @brief Returns the maximum number of elements that can be allocated by the allocator alloc.
    /// @param[in] alloc The allocator to query for the maximum size.
    /// @return The maximum number of elements that can be allocated by the allocator alloc.
    static constexpr SizeType maxSize(const Alloc& alloc) noexcept
    {
        if constexpr (detail::HasMaxSize<Alloc>::value) { return alloc.maxSize(); }
        else { return static_cast<SizeType>(-1) / sizeof(value_type); }
    }

    /// @brief Returns a copy of the allocator alloc to be used for copy construction of a container. If the allocator
    /// type has a selectOnContainerCopyConstruction member function, it is called to obtain the copy. Otherwise, a copy
    /// of the allocator is returned.
    /// @param[in] alloc The allocator to copy for container copy construction.
    /// @return A copy of the allocator alloc to be used for copy construction of a container.
    static Alloc selectOnContainerCopyConstruction(const Alloc& alloc)
    {
        if constexpr (detail::HasSelectOnContainerCopyConstruction<Alloc>::value)
        {
            return alloc.selectOnContainerCopyConstruction();
        }
        else { return alloc; }
    }
};

}   // namespace gp::memory
