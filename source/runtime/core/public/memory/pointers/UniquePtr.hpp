// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Concepts.hpp"
#include "CoreMinimal.hpp"

namespace gp
{

/// @brief Default deleter for use with UniquePtr.
/// @tparam T The type of the object to be deleted.
template <typename T>
class DefaultDelete
{
public:
    /// @brief Default constructor, copy constructor, copy assignment operator, and destructor.
    DefaultDelete() noexcept = default;
    DefaultDelete(const DefaultDelete&) noexcept = default;
    DefaultDelete& operator=(const DefaultDelete&) noexcept = default;
    ~DefaultDelete() noexcept = default;

    /// @brief Templated copy constructor and assignment operator to allow for polymorphic deletion.
    /// @tparam U Convertible type to T, allowing for deletion of derived types through base pointers.
    template <concepts::IsPointerConvertibleTo<T> U>
    DefaultDelete(const DefaultDelete<U>&) noexcept
    {}

public:
    /// @brief Templated assignment operator to allow for polymorphic deletion.
    /// @tparam U Convertible type to T, allowing for deletion of derived types through base pointers.
    template <concepts::IsPointerConvertibleTo<T> U>
    DefaultDelete& operator=(const DefaultDelete<U>&) noexcept
    {
        return *this;
    }

    /// @brief Function call operator to delete the managed object.
    /// @param[in] ptr Pointer to the object to be deleted.
    void operator()(T* ptr) const noexcept
    {
        delete ptr;
    }
};

/// @brief
/// @tparam T
template <typename T>
class UniquePtr
{};

}   // namespace gp
