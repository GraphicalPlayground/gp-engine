// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"

namespace GP
{

/// @brief Default deleter, calls scalar delete on the managed pointer.
/// @tparam T The type to delete.
template <typename T>
struct TDefaultDeleter
{
public:
    constexpr TDefaultDeleter() noexcept = default;

    /// @brief Converting constructor, allows TDefaultDeleter<Derived> → TDefaultDeleter<Base>.
    template <typename U>
    constexpr TDefaultDeleter(const TDefaultDeleter<U>&) noexcept
    {}

public:
    void operator()(T* ptr) const noexcept
    {
        static_assert(sizeof(T) > 0, "TDefaultDeleter: Cannot delete an incomplete type.");
        delete ptr;
    }
};

/// @brief Default deleter specialisation for arrays — calls array delete.
/// @tparam T The array element type.
template <typename T>
struct TDefaultDeleter<T[]>
{
public:
    constexpr TDefaultDeleter() noexcept = default;

public:
    void operator()(T* ptr) const noexcept
    {
        static_assert(sizeof(T) > 0, "TDefaultDeleter: Cannot delete an incomplete type.");
        delete[] ptr;
    }
};

/// @brief No-op deleter — for non-owning smart pointer wrappers over external memory.
template <typename T>
struct TNullDeleter
{
public:
    void operator()(T*) const noexcept {}
};

}   // namespace GP
