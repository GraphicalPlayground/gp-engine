// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Memory.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include <new>
#include <type_traits>
#include <utility>

namespace gp
{

/// @brief Aligned, uninitialized inline byte storage matching the exact size and alignment of @p T.
/// @details Decouples static or arena storage allocation from C++ object lifetime, replacing C++11
///          `std::aligned_storage` (deprecated in C++23 by P1413R3) with direct `alignas(T) Byte[sizeof(T)]` storage.
///          Because default construction and destruction are trivial no-ops, fixed chunk arrays such as GPU profiler
///          event pages or inline slot pools pay zero initialization overhead until placement `new` creates a live @p
///          T.
/// @note Typed accessors use `std::launder` so reusing the buffer across distinct placement lifetimes remains strictly
///       compliant with C++ pointer-provenance and strict-aliasing rules, even when @p T contains `const` or reference
///       members.
/// @warning Callers are responsible for tracking whether a live @p T currently occupies the storage; invoking
///          `destruct()`, `getTypedPtr()`, or `asRef()` on uninitialized storage does not track occupancy at runtime.
/// @tparam T Element type whose size and alignment this buffer reproduces.
template <typename T>
struct TypeCompatibleBytes final
{
    static_assert(!std::is_reference_v<T>, "TypeCompatibleBytes cannot store reference types.");
    static_assert(!std::is_void_v<T>, "TypeCompatibleBytes cannot store void.");

public:
    using ElementType = T;

private:
    alignas(T) Byte m_bytes[sizeof(T)];

public:
    /// @brief Leaves backing bytes intentionally uninitialized so bulk storage arrays incur zero setup cost.
    TypeCompatibleBytes() noexcept = default;

    /// @brief Trivially destructible; callers must call `destruct()` before the storage ends if a live `T` was built.
    ~TypeCompatibleBytes() noexcept = default;

    /// @brief Constructs an object of type @p T in-place using placement `new`.
    /// @tparam Args Constructor argument types.
    /// @param[in] args Arguments forwarded to `T`'s constructor.
    /// @return Pointer to the newly constructed live `T` instance.
    template <typename... Args>
    requires std::is_constructible_v<T, Args...>
    T* construct(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>)
    {
        return ::new (static_cast<void*>(m_bytes)) T(std::forward<Args>(args)...);
    }

    /// @brief Invokes `T`'s destructor in-place (a no-op if `T` is trivially destructible).
    void destruct() noexcept
    {
        if constexpr (!concepts::IsTriviallyDestructible<T>)
        {
            using ElementTypeAlias = T;
            getTypedPtr()->ElementTypeAlias::~ElementTypeAlias();
        }
    }

    /// @brief Returns a pointer to the live @p T stored inside this buffer, laundered for strict aliasing safety.
    /// @return Typed pointer to the contained object.
    [[nodiscard]] T* getTypedPtr() noexcept
    {
        return std::launder(reinterpret_cast<T*>(m_bytes));
    }

    /// @brief Returns a read-only pointer to the live @p T stored inside this buffer.
    /// @return Const-qualified typed pointer to the contained object.
    [[nodiscard]] const T* getTypedPtr() const noexcept
    {
        return std::launder(reinterpret_cast<const T*>(m_bytes));
    }

    /// @brief Returns a typed reference to the live @p T stored inside this buffer.
    /// @return Mutable reference to the contained object.
    [[nodiscard]] T& asRef() noexcept
    {
        return *getTypedPtr();
    }

    /// @brief Returns a read-only typed reference to the live @p T stored inside this buffer.
    /// @return Const reference to the contained object.
    [[nodiscard]] const T& asRef() const noexcept
    {
        return *getTypedPtr();
    }

    /// @brief Returns raw byte access to the underlying storage buffer.
    /// @return Pointer to the first storage byte.
    [[nodiscard]] Byte* data() noexcept
    {
        return m_bytes;
    }

    /// @brief Returns read-only raw byte access to the underlying storage buffer.
    /// @return Const pointer to the first storage byte.
    [[nodiscard]] const Byte* data() const noexcept
    {
        return m_bytes;
    }
};

}   // namespace gp
