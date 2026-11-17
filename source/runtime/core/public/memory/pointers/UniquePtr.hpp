// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Comparison.hpp"
#include "concepts/Construction.hpp"
#include "concepts/Fundamental.hpp"
#include "concepts/Memory.hpp"
#include "concepts/Relationship.hpp"
#include "CoreMinimal.hpp"
#include <compare>
#include <type_traits>
#include <utility>

namespace gp
{

namespace detail
{

/// @brief Checks whether @p T is an array of unknown bound (`U[]`).
/// @tparam T Candidate type to test.
template <typename T>
concept IsUnboundedArray = std::is_unbounded_array_v<T>;

/// @brief Checks whether @p T is a fixed-size array (`U[N]`).
/// @tparam T Candidate type to test.
template <typename T>
concept IsBoundedArray = std::is_bounded_array_v<T>;

/// @brief Checks qualification convertibility for dynamic array element pointers (`InSourceType(*)[]` to
///        `InTargetType(*)[]`).
/// @details Allows adding cv-qualifiers (such as `int*` to `const int*`) while rejecting derived-to-base array
///          conversions where element strides differ and `delete[]` would invoke undefined behavior. Guarding against
///          `void` prevents forming an invalid `void(*)[]` type during constraint evaluation.
/// @tparam InSourceType Pointee element type of the source array pointer.
/// @tparam InTargetType Pointee element type of the target `UniquePtr<InTargetType[]>`.
template <typename InSourceType, typename InTargetType>
concept IsArrayPointerConvertibleTo = !concepts::IsVoid<InSourceType> && !concepts::IsVoid<InTargetType> &&
                                      concepts::IsConvertibleTo<InSourceType (*)[], InTargetType (*)[]>;

/// @brief Checks whether @p InDeleter can be safely default-constructed by `UniquePtr`.
/// @details Disallows raw function pointers and reference deleters because default-initializing them would leave a
///          null function pointer or fail to bind a reference.
/// @tparam InDeleter Deleter type stored by `UniquePtr`.
template <typename InDeleter>
concept IsDefaultConstructibleDeleter =
    !std::is_pointer_v<InDeleter> && !std::is_reference_v<InDeleter> && concepts::IsDefaultConstructible<InDeleter>;

/// @brief Checks whether a converting move constructor can construct @p InTargetDeleter from @p InSourceDeleter.
/// @details Reference target deleters require an exact type match so converting from a different deleter type cannot
///          bind a reference to a temporary.
/// @tparam InTargetDeleter Destination deleter type.
/// @tparam InSourceDeleter Source deleter type.
template <typename InTargetDeleter, typename InSourceDeleter>
concept IsCompatibleDeleterMove =
    (std::is_reference_v<InTargetDeleter> && concepts::IsSameAs<InSourceDeleter, InTargetDeleter>) ||
    (!std::is_reference_v<InTargetDeleter> && concepts::IsConvertibleTo<InSourceDeleter, InTargetDeleter>);

/// @brief Parameter type used when initializing a `UniquePtr` from an lvalue deleter.
/// @tparam InDeleter Deleter type (value, pointer, or reference).
template <typename InDeleter>
using DeleterLValueParam = std::conditional_t<std::is_reference_v<InDeleter>, InDeleter, const InDeleter&>;

/// @brief Parameter type used when initializing a `UniquePtr` from an rvalue deleter (deleted for reference deleters).
/// @tparam InDeleter Deleter type (value, pointer, or reference).
template <typename InDeleter>
using DeleterRValueParam = std::remove_reference_t<InDeleter>&&;

/// @brief Packs a raw pointer alongside its deleter using `GP_NO_UNIQUE_ADDRESS`.
/// @details Empty deleters such as `DefaultDelete<T>` occupy zero extra bytes, keeping `sizeof(UniquePtr<T>)` equal to
///          `sizeof(T*)` for single-register passing. Unlike private base inheritance, member storage also accepts
///          `final` deleters, stateless lambdas, and function pointers.
/// @tparam InPointerType Raw pointer type managed by `UniquePtr`.
/// @tparam InDeleter Value or function-pointer deleter type.
template <typename InPointerType, typename InDeleter>
struct UniquePtrStorage
{
public:
    InPointerType ptr{ nullptr };
    GP_NO_UNIQUE_ADDRESS InDeleter deleter;

public:
    /// @brief Default-initializes a null pointer and value-initializes the deleter.
    constexpr UniquePtrStorage() noexcept requires concepts::IsDefaultConstructible<InDeleter>
        : ptr(nullptr)
        , deleter()
    {}

    /// @brief Stores @p inPtr and value-initializes the deleter.
    /// @param[in] inPtr Raw pointer to own.
    constexpr explicit UniquePtrStorage(const InPointerType inPtr) noexcept
        requires concepts::IsDefaultConstructible<InDeleter>
        : ptr(inPtr)
        , deleter()
    {}

    /// @brief Stores @p inPtr and constructs the deleter from @p inDeleter.
    /// @tparam InDeleterArg Forwarded deleter argument type.
    /// @param[in] inPtr Raw pointer to own.
    /// @param[in] inDeleter Deleter value or convertible deleter forwarded to construct `deleter`.
    template <typename InDeleterArg>
    constexpr UniquePtrStorage(const InPointerType inPtr, InDeleterArg&& inDeleter) noexcept
        : ptr(inPtr)
        , deleter(std::forward<InDeleterArg>(inDeleter))
    {}
};

/// @brief Storage specialization for lvalue-reference deleters (`InDeleter&` and `const InDeleter&`).
/// @tparam InPointerType Raw pointer type managed by `UniquePtr`.
/// @tparam InDeleter Referenced deleter type.
template <typename InPointerType, typename InDeleter>
struct UniquePtrStorage<InPointerType, InDeleter&>
{
public:
    InPointerType ptr{ nullptr };
    InDeleter& deleter;

public:
    /// @brief Stores @p inPtr and binds to the caller-owned @p inDeleter reference.
    /// @param[in] inPtr Raw pointer to own.
    /// @param[in] inDeleter External deleter reference whose lifetime must outlive the `UniquePtr`.
    constexpr UniquePtrStorage(const InPointerType inPtr, InDeleter& inDeleter) noexcept
        : ptr(inPtr)
        , deleter(inDeleter)
    {}
};

}   // namespace detail

/// @brief Default stateless deleter invoking `delete` for single objects managed by `UniquePtr<T>`.
/// @tparam T Type of the managed object.
template <typename T>
class DefaultDelete
{
public:
    /// @brief Defaulted special member functions.
    constexpr DefaultDelete() noexcept = default;
    constexpr DefaultDelete(const DefaultDelete&) noexcept = default;
    constexpr DefaultDelete& operator=(const DefaultDelete&) noexcept = default;
    constexpr ~DefaultDelete() noexcept = default;

    /// @brief Converts from a `DefaultDelete<U>` when `U*` converts implicitly to `T*` for polymorphic cleanup.
    /// @tparam U Source pointee type convertible to `T`.
    /// @param[in] other Source deleter instance (stateless).
    template <concepts::IsPointerConvertibleTo<T> U>
    constexpr DefaultDelete([[maybe_unused]] const DefaultDelete<U>& other) noexcept
    {}

public:
    /// @brief Assigns from a `DefaultDelete<U>` when `U*` converts implicitly to `T*`.
    /// @tparam U Source pointee type convertible to `T`.
    /// @param[in] other Source deleter instance (stateless).
    /// @return Reference to `*this`.
    template <concepts::IsPointerConvertibleTo<T> U>
    constexpr DefaultDelete& operator=([[maybe_unused]] const DefaultDelete<U>& other) noexcept
    {
        return *this;
    }

    /// @brief Deletes the managed object via scalar `delete`.
    /// @param[in] ptr Pointer to the object to delete (may be `nullptr`).
    constexpr void operator()(T* ptr) const noexcept
    {
        static_assert(!concepts::IsVoid<T>, "DefaultDelete cannot delete a void pointer.");
        if constexpr (!concepts::IsVoid<T>)
        {
            static_assert(sizeof(T) > 0, "DefaultDelete cannot delete an incomplete type.");
            delete ptr;
        }
    }
};

/// @brief Specialization of `DefaultDelete` invoking `delete[]` for dynamic arrays managed by `UniquePtr<T[]>`.
/// @tparam T Element type of the managed array.
template <typename T>
class DefaultDelete<T[]>
{
public:
    /// @brief Defaulted special member functions.
    constexpr DefaultDelete() noexcept = default;
    constexpr DefaultDelete(const DefaultDelete&) noexcept = default;
    constexpr DefaultDelete& operator=(const DefaultDelete&) noexcept = default;
    constexpr ~DefaultDelete() noexcept = default;

    /// @brief Converts from `DefaultDelete<U[]>` when `U(*)[]` converts to `T(*)[]` (qualification conversions only).
    /// @tparam U Source array element type.
    /// @param[in] other Source array deleter instance (stateless).
    template <typename U>
    requires detail::IsArrayPointerConvertibleTo<U, T>
    constexpr DefaultDelete([[maybe_unused]] const DefaultDelete<U[]>& other) noexcept
    {}

public:
    /// @brief Assigns from `DefaultDelete<U[]>` when `U(*)[]` converts to `T(*)[]`.
    /// @tparam U Source array element type.
    /// @param[in] other Source array deleter instance (stateless).
    /// @return Reference to `*this`.
    template <typename U>
    requires detail::IsArrayPointerConvertibleTo<U, T>
    constexpr DefaultDelete& operator=([[maybe_unused]] const DefaultDelete<U[]>& other) noexcept
    {
        return *this;
    }

    /// @brief Deletes the managed array via array `delete[]`.
    /// @tparam U Array element type qualification-convertible to `T`.
    /// @param[in] ptr Pointer to the first element of the array to delete (may be `nullptr`).
    template <typename U>
    requires detail::IsArrayPointerConvertibleTo<U, T> constexpr void operator()(U* ptr) const noexcept
    {
        static_assert(sizeof(U) > 0, "DefaultDelete<T[]> cannot delete an incomplete type.");
        delete[] ptr;
    }
};

/// @brief Exclusive-ownership smart pointer for a single dynamically allocated object.
/// @details Occupies `sizeof(T*)` bytes when `Deleter` is stateless (`DefaultDelete<T>`, empty lambdas, or `final`
///          empty structs) and transfers ownership strictly by move. Incomplete types are permitted in member
///          declarations (such as the PImpl idiom) as long as destruction, `reset()`, and move assignment instantiate
///          where `T` is complete.
/// @tparam T Type of the managed object.
/// @tparam Deleter Callable invoked to destroy a non-null managed pointer.
/// @see DefaultDelete, RefCountedPtr, SharedPtr, WeakPtr
template <typename T, typename Deleter = DefaultDelete<T>>
class UniquePtr
{
public:
    /// @brief Managed object type.
    using ElementType = T;

    /// @brief Raw pointer type stored internally.
    using PointerType = T*;

    /// @brief Deleter type invoked on cleanup.
    using DeleterType = Deleter;

public:
    UniquePtr(const UniquePtr&) = delete;
    UniquePtr& operator=(const UniquePtr&) = delete;

    /// @brief Constructs an empty `UniquePtr` holding `nullptr` and a value-initialized deleter.
    constexpr UniquePtr() noexcept requires detail::IsDefaultConstructibleDeleter<Deleter>
        : m_storage()
    {}

    /// @brief Constructs an empty `UniquePtr` from `nullptr`.
    /// @param[in] nullValue Null pointer literal.
    constexpr explicit(false) UniquePtr([[maybe_unused]] gp::NullPtrT nullValue) noexcept
        requires detail::IsDefaultConstructibleDeleter<Deleter>
        : m_storage()
    {}

    /// @brief Takes exclusive ownership of @p ptr using a value-initialized deleter.
    /// @tparam U Pointee type convertible to `T*`.
    /// @param[in] ptr Raw pointer whose ownership is transferred to this `UniquePtr`.
    template <concepts::IsPointerConvertibleTo<T> U>
    explicit constexpr UniquePtr(U* ptr) noexcept requires detail::IsDefaultConstructibleDeleter<Deleter>
        : m_storage(ptr)
    {}

    /// @brief Takes exclusive ownership of @p ptr with a copied or referenced deleter.
    /// @param[in] ptr Raw pointer to own (may be `nullptr`).
    /// @param[in] deleter Deleter copied into storage, or bound by reference when `Deleter` is an lvalue reference.
    constexpr UniquePtr(const PointerType ptr, detail::DeleterLValueParam<Deleter> deleter) noexcept requires(
        (!std::is_reference_v<Deleter> && concepts::IsCopyConstructible<Deleter>) || std::is_reference_v<Deleter>
    )
        : m_storage(ptr, deleter)
    {}

    /// @brief Takes exclusive ownership of @p ptr with a moved deleter.
    /// @param[in] ptr Raw pointer to own (may be `nullptr`).
    /// @param[in] deleter Rvalue deleter moved into storage.
    constexpr UniquePtr(const PointerType ptr, detail::DeleterRValueParam<Deleter> deleter) noexcept
        requires(!std::is_reference_v<Deleter> && concepts::IsMoveConstructible<Deleter>)
        : m_storage(ptr, std::move(deleter))
    {}

    /// @brief Prevents binding an rvalue temporary deleter when `Deleter` is a reference type.
    constexpr UniquePtr(PointerType, detail::DeleterRValueParam<Deleter>) noexcept
        requires(std::is_reference_v<Deleter>) = delete;

    /// @brief Transfers ownership from @p other, leaving @p other null.
    /// @param[in,out] other Source `UniquePtr` to move from.
    constexpr UniquePtr(UniquePtr&& other) noexcept requires concepts::IsMoveConstructible<Deleter>
        : m_storage(other.m_storage.ptr, std::forward<Deleter>(other.m_storage.deleter))
    {
        other.m_storage.ptr = nullptr;
    }

    /// @brief Transfers ownership from a compatible `UniquePtr<U, D>`, supporting derived-to-base upcasts.
    /// @tparam U Non-array source pointee type convertible to `T`.
    /// @tparam D Source deleter type compatible with `Deleter`.
    /// @param[in,out] other Source `UniquePtr` to move from; reset to `nullptr`.
    template <typename U, typename D>
    requires(
        !concepts::IsArray<U> && concepts::IsPointerConvertibleTo<U, T> && detail::IsCompatibleDeleterMove<Deleter, D>
    ) constexpr explicit(false) UniquePtr(UniquePtr<U, D>&& other) noexcept
        : m_storage(other.m_storage.ptr, std::forward<D>(other.m_storage.deleter))
    {
        other.m_storage.ptr = nullptr;
    }

    /// @brief Destroys the managed object if non-null.
    /// @details Clears `m_storage.ptr` before invoking the deleter so any reentrant observer inspecting `*this` during
    ///          destruction sees a null state rather than a dangling address.
    GP_FORCEINLINE_HINT constexpr ~UniquePtr() noexcept
    {
        const PointerType oldPtr = m_storage.ptr;
        m_storage.ptr = nullptr;
        if (oldPtr != nullptr)
        {
            getDeleter()(oldPtr);
        }
    }

public:
    /// @brief Moves ownership from @p other, deleting any previously owned object with the current deleter first.
    /// @param[in,out] other Source `UniquePtr` to move from; reset to `nullptr`.
    /// @return Reference to `*this`.
    constexpr UniquePtr& operator=(UniquePtr&& other) noexcept requires std::is_assignable_v<Deleter&, Deleter&&>
    {
        if (this != &other)
        {
            const PointerType oldPtr = m_storage.ptr;
            m_storage.ptr = other.m_storage.ptr;
            other.m_storage.ptr = nullptr;
            if (oldPtr != nullptr)
            {
                getDeleter()(oldPtr);
            }
            getDeleter() = std::forward<Deleter>(other.m_storage.deleter);
        }
        return *this;
    }

    /// @brief Resets the `UniquePtr` to null, deleting any previously owned object.
    /// @param[in] nullValue Null pointer literal.
    /// @return Reference to `*this`.
    constexpr UniquePtr& operator=([[maybe_unused]] gp::NullPtrT nullValue) noexcept
    {
        reset();
        return *this;
    }

    /// @brief Moves ownership from a compatible `UniquePtr<U, D>`, deleting any previously owned object first.
    /// @tparam U Non-array source pointee type convertible to `T`.
    /// @tparam D Source deleter type assignable to `Deleter`.
    /// @param[in,out] other Source `UniquePtr` to move from; reset to `nullptr`.
    /// @return Reference to `*this`.
    template <typename U, typename D>
    requires(!concepts::IsArray<U> && concepts::IsPointerConvertibleTo<U, T> && std::is_assignable_v<Deleter&, D &&>)
    constexpr UniquePtr& operator=(UniquePtr<U, D>&& other) noexcept
    {
        const PointerType oldPtr = m_storage.ptr;
        m_storage.ptr = other.m_storage.ptr;
        other.m_storage.ptr = nullptr;
        if (oldPtr != nullptr)
        {
            getDeleter()(oldPtr);
        }
        getDeleter() = std::forward<D>(other.m_storage.deleter);
        return *this;
    }

    /// @brief Explicit boolean conversion indicating whether an object is owned.
    /// @return `true` if `get() != nullptr`, `false` otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT explicit constexpr operator bool() const noexcept
    {
        return isValid();
    }

    /// @brief Member access operator for the managed object.
    /// @note Triggers `GP_ASSERT` in checked builds if the `UniquePtr` is null.
    /// @return Raw pointer to the managed object.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr PointerType operator->() const noexcept
    {
        GP_ASSERT(m_storage.ptr != nullptr, "Attempting to dereference a null UniquePtr via operator->().");
        return m_storage.ptr;
    }

    /// @brief Dereferences the managed object.
    /// @note Triggers `GP_ASSERT` in checked builds if the `UniquePtr` is null.
    /// @return Reference to the managed object.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr std::add_lvalue_reference_t<T> operator*() const noexcept
        requires(!concepts::IsVoid<T>)
    {
        GP_ASSERT(m_storage.ptr != nullptr, "Attempting to dereference a null UniquePtr via operator*().");
        return *m_storage.ptr;
    }

public:
    /// @brief Reports whether this `UniquePtr` owns a non-null object.
    /// @return `true` if `m_storage.ptr != nullptr`, `false` otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool isValid() const noexcept
    {
        return m_storage.ptr != nullptr;
    }

    /// @brief Returns the managed raw pointer without releasing ownership.
    /// @return Managed raw pointer, or `nullptr` if empty.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr PointerType get() const noexcept
    {
        return m_storage.ptr;
    }

    /// @brief Releases ownership of the managed object without invoking the deleter.
    /// @return Previously owned raw pointer, or `nullptr` if empty.
    [[nodiscard]] constexpr PointerType release() noexcept
    {
        const PointerType ptr = m_storage.ptr;
        m_storage.ptr = nullptr;
        return ptr;
    }

    /// @brief Replaces the managed pointer with @p ptr and deletes any previously owned object.
    /// @details Updates `m_storage.ptr` before deleting `oldPtr` and skips the deleter if `oldPtr` is null or equal to
    ///          @p ptr.
    /// @param[in] ptr New raw pointer to own, or `nullptr` to clear ownership.
    constexpr void reset(const PointerType ptr = nullptr) noexcept
    {
        if (m_storage.ptr != ptr)
        {
            const PointerType oldPtr = m_storage.ptr;
            m_storage.ptr = ptr;
            if (oldPtr != nullptr)
            {
                getDeleter()(oldPtr);
            }
        }
    }

    /// @brief Exchanges the managed pointers and deleters of `*this` and @p other.
    /// @param[in,out] other Other `UniquePtr` to swap with.
    constexpr void swap(UniquePtr& other) noexcept requires std::is_swappable_v<Deleter>
    {
        using std::swap;
        swap(m_storage.ptr, other.m_storage.ptr);
        swap(m_storage.deleter, other.m_storage.deleter);
    }

    /// @brief Returns a mutable reference to the stored deleter.
    /// @return Reference to the deleter.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr Deleter& getDeleter() noexcept
    {
        return m_storage.deleter;
    }

    /// @brief Returns a read-only reference to the stored deleter.
    /// @return Const reference to the deleter.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr const Deleter& getDeleter() const noexcept
    {
        return m_storage.deleter;
    }

private:
    template <typename OtherT, typename OtherDeleter>
    friend class UniquePtr;

    detail::UniquePtrStorage<PointerType, Deleter> m_storage;
};

/// @brief Exclusive-ownership smart pointer specialization for dynamically allocated arrays (`T[]`).
/// @details Differs from primary `UniquePtr<T, Deleter>` in three language contracts:
///          1. Uses `DefaultDelete<T[]>` (`delete[]`) by default.
///          2. Exposes element indexing (`operator[]`) in place of `operator*` and `operator->`.
///          3. Restricts raw-pointer construction, `reset()`, and converting moves to qualification conversions
///             (`U(*)[]` convertible to `T(*)[]`), rejecting `Derived*` to `Base*` array conversions whose mismatched
///             element strides would corrupt memory during indexing or `delete[]`.
/// @tparam T Element type of the managed array.
/// @tparam Deleter Callable invoked to destroy a non-null array pointer.
/// @see DefaultDelete, makeUnique, makeUniqueForOverwrite
template <typename T, typename Deleter>
class UniquePtr<T[], Deleter>
{
public:
    /// @brief Element type stored in the managed array.
    using ElementType = T;

    /// @brief Raw pointer type to the first array element.
    using PointerType = T*;

    /// @brief Deleter type invoked on cleanup.
    using DeleterType = Deleter;

public:
    UniquePtr(const UniquePtr&) = delete;
    UniquePtr& operator=(const UniquePtr&) = delete;

    /// @brief Constructs an empty `UniquePtr<T[]>` holding `nullptr` and a value-initialized deleter.
    constexpr UniquePtr() noexcept requires detail::IsDefaultConstructibleDeleter<Deleter>
        : m_storage()
    {}

    /// @brief Constructs an empty `UniquePtr<T[]>` from `nullptr`.
    /// @param[in] nullValue Null pointer literal.
    constexpr explicit(false) UniquePtr([[maybe_unused]] gp::NullPtrT nullValue) noexcept
        requires detail::IsDefaultConstructibleDeleter<Deleter>
        : m_storage()
    {}

    /// @brief Takes exclusive ownership of an array pointer @p ptr using a value-initialized deleter.
    /// @tparam U Element type whose array pointer `U(*)[]` converts to `T(*)[]`.
    /// @param[in] ptr Raw pointer to the first element of a `new U[]` allocation.
    template <typename U>
    requires(detail::IsArrayPointerConvertibleTo<U, T> && detail::IsDefaultConstructibleDeleter<Deleter>)
    explicit constexpr UniquePtr(U* ptr) noexcept
        : m_storage(ptr)
    {}

    /// @brief Takes exclusive ownership of an array pointer @p ptr with a copied or referenced deleter.
    /// @tparam U Element type whose array pointer `U(*)[]` converts to `T(*)[]`.
    /// @param[in] ptr Raw array pointer to own.
    /// @param[in] deleter Deleter copied into storage, or bound by reference when `Deleter` is an lvalue reference.
    template <typename U>
    requires(
        detail::IsArrayPointerConvertibleTo<U, T> &&
        ((!std::is_reference_v<Deleter> && concepts::IsCopyConstructible<Deleter>) || std::is_reference_v<Deleter>)
    ) constexpr UniquePtr(U* ptr, detail::DeleterLValueParam<Deleter> deleter) noexcept
        : m_storage(ptr, deleter)
    {}

    /// @brief Takes exclusive ownership of an array pointer @p ptr with a moved deleter.
    /// @tparam U Element type whose array pointer `U(*)[]` converts to `T(*)[]`.
    /// @param[in] ptr Raw array pointer to own.
    /// @param[in] deleter Rvalue deleter moved into storage.
    template <typename U>
    requires(
        detail::IsArrayPointerConvertibleTo<U, T> && !std::is_reference_v<Deleter> &&
        concepts::IsMoveConstructible<Deleter>
    ) constexpr UniquePtr(U* ptr, detail::DeleterRValueParam<Deleter> deleter) noexcept
        : m_storage(ptr, std::move(deleter))
    {}

    /// @brief Prevents binding an rvalue temporary deleter when `Deleter` is a reference type.
    template <typename U>
    requires(detail::IsArrayPointerConvertibleTo<U, T> && std::is_reference_v<Deleter>)
    constexpr UniquePtr(U*, detail::DeleterRValueParam<Deleter>) noexcept = delete;

    /// @brief Constructs an empty `UniquePtr<T[]>` from `nullptr` with a copied or referenced deleter.
    /// @param[in] nullValue Null pointer literal.
    /// @param[in] deleter Deleter copied into storage, or bound by reference when `Deleter` is an lvalue reference.
    constexpr UniquePtr([[maybe_unused]] gp::NullPtrT nullValue, detail::DeleterLValueParam<Deleter> deleter) noexcept
        requires(
            (!std::is_reference_v<Deleter> && concepts::IsCopyConstructible<Deleter>) || std::is_reference_v<Deleter>
        )
        : m_storage(nullptr, deleter)
    {}

    /// @brief Constructs an empty `UniquePtr<T[]>` from `nullptr` with a moved deleter.
    /// @param[in] nullValue Null pointer literal.
    /// @param[in] deleter Rvalue deleter moved into storage.
    constexpr UniquePtr([[maybe_unused]] gp::NullPtrT nullValue, detail::DeleterRValueParam<Deleter> deleter) noexcept
        requires(!std::is_reference_v<Deleter> && concepts::IsMoveConstructible<Deleter>)
        : m_storage(nullptr, std::move(deleter))
    {}

    /// @brief Prevents binding an rvalue temporary deleter when constructing from `nullptr`.
    constexpr UniquePtr(gp::NullPtrT, detail::DeleterRValueParam<Deleter>) noexcept
        requires(std::is_reference_v<Deleter>) = delete;

    /// @brief Transfers array ownership from @p other, leaving @p other null.
    /// @param[in,out] other Source `UniquePtr<T[]>` to move from.
    constexpr UniquePtr(UniquePtr&& other) noexcept requires concepts::IsMoveConstructible<Deleter>
        : m_storage(other.m_storage.ptr, std::forward<Deleter>(other.m_storage.deleter))
    {
        other.m_storage.ptr = nullptr;
    }

    /// @brief Transfers ownership from a qualification-compatible `UniquePtr<U[], D>` (for instance `int[]` to
    ///        `const int[]`).
    /// @tparam U Source array element type whose array pointer converts to `T(*)[]`.
    /// @tparam D Source deleter type compatible with `Deleter`.
    /// @param[in,out] other Source `UniquePtr` to move from; reset to `nullptr`.
    template <typename U, typename D>
    requires(detail::IsArrayPointerConvertibleTo<U, T> && detail::IsCompatibleDeleterMove<Deleter, D>)
    constexpr explicit(false) UniquePtr(UniquePtr<U[], D>&& other) noexcept
        : m_storage(other.m_storage.ptr, std::forward<D>(other.m_storage.deleter))
    {
        other.m_storage.ptr = nullptr;
    }

    /// @brief Destroys the managed array if non-null.
    /// @details Clears `m_storage.ptr` before invoking the deleter so any reentrant observer inspecting `*this` during
    ///          element destruction sees a null state rather than a dangling address.
    GP_FORCEINLINE_HINT constexpr ~UniquePtr() noexcept
    {
        const PointerType oldPtr = m_storage.ptr;
        m_storage.ptr = nullptr;
        if (oldPtr != nullptr)
        {
            getDeleter()(oldPtr);
        }
    }

public:
    /// @brief Moves array ownership from @p other, deleting any previously owned array with the current deleter first.
    /// @param[in,out] other Source `UniquePtr<T[]>` to move from; reset to `nullptr`.
    /// @return Reference to `*this`.
    constexpr UniquePtr& operator=(UniquePtr&& other) noexcept requires std::is_assignable_v<Deleter&, Deleter&&>
    {
        if (this != &other)
        {
            const PointerType oldPtr = m_storage.ptr;
            m_storage.ptr = other.m_storage.ptr;
            other.m_storage.ptr = nullptr;
            if (oldPtr != nullptr)
            {
                getDeleter()(oldPtr);
            }
            getDeleter() = std::forward<Deleter>(other.m_storage.deleter);
        }
        return *this;
    }

    /// @brief Resets the `UniquePtr<T[]>` to null, deleting any previously owned array.
    /// @param[in] nullValue Null pointer literal.
    /// @return Reference to `*this`.
    constexpr UniquePtr& operator=([[maybe_unused]] gp::NullPtrT nullValue) noexcept
    {
        reset();
        return *this;
    }

    /// @brief Moves ownership from a qualification-compatible `UniquePtr<U[], D>`, deleting any previous array first.
    /// @tparam U Source array element type whose array pointer converts to `T(*)[]`.
    /// @tparam D Source deleter type assignable to `Deleter`.
    /// @param[in,out] other Source `UniquePtr` to move from; reset to `nullptr`.
    /// @return Reference to `*this`.
    template <typename U, typename D>
    requires(detail::IsArrayPointerConvertibleTo<U, T> && std::is_assignable_v<Deleter&, D &&>)
    constexpr UniquePtr& operator=(UniquePtr<U[], D>&& other) noexcept
    {
        const PointerType oldPtr = m_storage.ptr;
        m_storage.ptr = other.m_storage.ptr;
        other.m_storage.ptr = nullptr;
        if (oldPtr != nullptr)
        {
            getDeleter()(oldPtr);
        }
        getDeleter() = std::forward<D>(other.m_storage.deleter);
        return *this;
    }

    /// @brief Explicit boolean conversion indicating whether an array is owned.
    /// @return `true` if `get() != nullptr`, `false` otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT explicit constexpr operator bool() const noexcept
    {
        return isValid();
    }

    /// @brief Indexes into the managed array.
    /// @note Triggers `GP_ASSERT` in checked builds if the `UniquePtr` is null. Bounds checking is the caller's
    ///       responsibility because `UniquePtr<T[]>` stores only the base pointer with no element count.
    /// @param[in] index Zero-based element index.
    /// @return Reference to the element at @p index.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr T& operator[](const gp::USize index) const noexcept
    {
        GP_ASSERT(m_storage.ptr != nullptr, "Attempting to index a null UniquePtr<T[]> via operator[]().");
        return m_storage.ptr[index];
    }

public:
    /// @brief Reports whether this `UniquePtr<T[]>` owns a non-null array.
    /// @return `true` if `m_storage.ptr != nullptr`, `false` otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool isValid() const noexcept
    {
        return m_storage.ptr != nullptr;
    }

    /// @brief Returns the raw pointer to the first element without releasing ownership.
    /// @return Raw array pointer, or `nullptr` if empty.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr PointerType get() const noexcept
    {
        return m_storage.ptr;
    }

    /// @brief Releases ownership of the managed array without invoking the deleter.
    /// @return Previously owned raw array pointer, or `nullptr` if empty.
    [[nodiscard]] constexpr PointerType release() noexcept
    {
        const PointerType ptr = m_storage.ptr;
        m_storage.ptr = nullptr;
        return ptr;
    }

    /// @brief Replaces the managed array with @p ptr and deletes any previously owned array.
    /// @tparam U Element type whose array pointer `U(*)[]` converts to `T(*)[]`.
    /// @param[in] ptr New array pointer to own.
    template <typename U>
    requires detail::IsArrayPointerConvertibleTo<U, T> constexpr void reset(U* ptr) noexcept
    {
        if (m_storage.ptr != ptr)
        {
            const PointerType oldPtr = m_storage.ptr;
            m_storage.ptr = ptr;
            if (oldPtr != nullptr)
            {
                getDeleter()(oldPtr);
            }
        }
    }

    /// @brief Resets the `UniquePtr<T[]>` to null and deletes any previously owned array.
    /// @param[in] nullValue Null pointer literal.
    constexpr void reset([[maybe_unused]] gp::NullPtrT nullValue = nullptr) noexcept
    {
        reset(static_cast<PointerType>(nullptr));
    }

    /// @brief Exchanges the managed array pointers and deleters of `*this` and @p other.
    /// @param[in,out] other Other `UniquePtr<T[]>` to swap with.
    constexpr void swap(UniquePtr& other) noexcept requires std::is_swappable_v<Deleter>
    {
        using std::swap;
        swap(m_storage.ptr, other.m_storage.ptr);
        swap(m_storage.deleter, other.m_storage.deleter);
    }

    /// @brief Returns a mutable reference to the stored deleter.
    /// @return Reference to the deleter.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr Deleter& getDeleter() noexcept
    {
        return m_storage.deleter;
    }

    /// @brief Returns a read-only reference to the stored deleter.
    /// @return Const reference to the deleter.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr const Deleter& getDeleter() const noexcept
    {
        return m_storage.deleter;
    }

private:
    template <typename OtherT, typename OtherDeleter>
    friend class UniquePtr;

    detail::UniquePtrStorage<PointerType, Deleter> m_storage;
};

/// @brief ADL-friendly swap exchanging two `UniquePtr` instances (single object or array).
/// @tparam T Managed object or array type.
/// @tparam Deleter Deleter type.
/// @param[in,out] lhs First `UniquePtr` to swap.
/// @param[in,out] rhs Second `UniquePtr` to swap.
template <typename T, typename Deleter>
requires std::is_swappable_v<Deleter>
GP_FORCEINLINE_HINT constexpr void swap(UniquePtr<T, Deleter>& lhs, UniquePtr<T, Deleter>& rhs) noexcept
{
    lhs.swap(rhs);
}

/// @brief Compares the managed addresses of two `UniquePtr` instances across arbitrary compatible types and deleters.
/// @tparam T1 Managed type of @p lhs.
/// @tparam D1 Deleter type of @p lhs.
/// @tparam T2 Managed type of @p rhs.
/// @tparam D2 Deleter type of @p rhs.
/// @param[in] lhs Left-hand `UniquePtr`.
/// @param[in] rhs Right-hand `UniquePtr`.
/// @return `true` if `lhs.get() == rhs.get()`, `false` otherwise.
template <typename T1, typename D1, typename T2, typename D2>
requires concepts::
    IsEqualityComparableWith<typename UniquePtr<T1, D1>::PointerType, typename UniquePtr<T2, D2>::PointerType>
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool
    operator==(const UniquePtr<T1, D1>& lhs, const UniquePtr<T2, D2>& rhs) noexcept
{
    return lhs.get() == rhs.get();
}

/// @brief Compares a `UniquePtr` against `nullptr` (C++20 synthesizes `!=` and reversed `nullptr == ptr`).
/// @tparam T Managed type of @p ptr.
/// @tparam Deleter Deleter type of @p ptr.
/// @param[in] ptr `UniquePtr` to test.
/// @param[in] nullValue Null pointer literal.
/// @return `true` if @p ptr does not own an object, `false` otherwise.
template <typename T, typename Deleter>
[[nodiscard]] GP_FORCEINLINE_HINT constexpr bool
    operator==(const UniquePtr<T, Deleter>& ptr, [[maybe_unused]] gp::NullPtrT nullValue) noexcept
{
    return !ptr.isValid();
}

/// @brief Three-way compares two `UniquePtr` instances by address across arbitrary compatible types and deleters.
/// @tparam T1 Managed type of @p lhs.
/// @tparam D1 Deleter type of @p lhs.
/// @tparam T2 Managed type of @p rhs.
/// @tparam D2 Deleter type of @p rhs.
/// @param[in] lhs Left-hand `UniquePtr`.
/// @param[in] rhs Right-hand `UniquePtr`.
/// @return Ordering relationship of `lhs.get()` and `rhs.get()`.
template <typename T1, typename D1, typename T2, typename D2>
requires std::
    three_way_comparable_with<typename UniquePtr<T1, D1>::PointerType, typename UniquePtr<T2, D2>::PointerType>
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr std::
        compare_three_way_result_t<typename UniquePtr<T1, D1>::PointerType, typename UniquePtr<T2, D2>::PointerType>
    operator<=>(const UniquePtr<T1, D1>& lhs, const UniquePtr<T2, D2>& rhs) noexcept
{
    return std::compare_three_way{}(lhs.get(), rhs.get());
}

/// @brief Three-way compares a `UniquePtr` against `nullptr` (synthesizing `<`, `<=`, `>`, and `>=`).
/// @tparam T Managed type of @p ptr.
/// @tparam Deleter Deleter type of @p ptr.
/// @param[in] ptr `UniquePtr` to compare.
/// @param[in] nullValue Null pointer literal.
/// @return `std::strong_ordering` comparing `ptr.get()` with a null pointer.
template <typename T, typename Deleter>
[[nodiscard]] GP_FORCEINLINE_HINT constexpr std::strong_ordering
    operator<=>(const UniquePtr<T, Deleter>& ptr, [[maybe_unused]] gp::NullPtrT nullValue) noexcept
{
    return std::compare_three_way{}(ptr.get(), static_cast<typename UniquePtr<T, Deleter>::PointerType>(nullptr));
}

/// @brief Allocates and constructs a single object of type @p T managed by `UniquePtr<T>`.
/// @details Forwards @p args to `T`'s constructor. With no arguments, `T` is value-initialized (zeroing trivial types).
/// @tparam T Non-array object type to construct.
/// @tparam Ts Constructor argument types forwarded to `T`.
/// @param[in] args Constructor arguments for `T`.
/// @return `UniquePtr<T>` owning the newly allocated object.
template <typename T, typename... Ts>
requires(!concepts::IsArray<T> && concepts::IsConstructibleWith<T, Ts && ...>)
[[nodiscard]] GP_FORCEINLINE_HINT constexpr UniquePtr<T> makeUnique(Ts&&... args) noexcept
{
    return UniquePtr<T>(new T(std::forward<Ts>(args)...));
}

/// @brief Allocates a dynamic array of @p count value-initialized elements managed by `UniquePtr<T>`.
/// @details Uses `new ElementType[count]()`, zeroing trivial element types and default-constructing class elements.
/// @tparam T Unbounded array type (`ElementType[]`).
/// @param[in] count Number of elements to allocate (may be `0`).
/// @return `UniquePtr<T>` owning the allocated array.
template <typename T>
requires(detail::IsUnboundedArray<T> && concepts::IsConstructibleWith<std::remove_extent_t<T>>)
[[nodiscard]] GP_FORCEINLINE_HINT constexpr UniquePtr<T> makeUnique(const gp::USize count) noexcept
{
    using ElementType = std::remove_extent_t<T>;
    return UniquePtr<T>(new ElementType[count]());
}

/// @brief Explicitly rejects bounded array types (`T[N]`) in `makeUnique`.
/// @details Bounded array extents belong in `gp::Array<T, N>` or `makeUnique<T[]>(N)`; `UniquePtr<T[N]>` would select
///          the single-object primary template and invoke scalar `delete` instead of `delete[]`.
/// @tparam T Bounded array type (`ElementType[N]`).
/// @tparam Ts Unused argument types.
template <typename T, typename... Ts>
requires detail::IsBoundedArray<T> void makeUnique(Ts&&...) = delete;

/// @brief Allocates a single default-initialized object of type @p T managed by `UniquePtr<T>`.
/// @details Trivial types (primitives and POD structs) remain uninitialized to avoid zero-fill overhead when the caller
///          immediately populates the memory.
/// @note Only use when the allocated object will be written before it is read.
/// @tparam T Non-array, default-initializable type to allocate.
/// @return `UniquePtr<T>` owning the newly allocated object.
template <typename T>
requires(!concepts::IsArray<T> && concepts::IsDefaultConstructible<T>)
[[nodiscard]] GP_FORCEINLINE_HINT constexpr UniquePtr<T> makeUniqueForOverwrite() noexcept
{
    return UniquePtr<T>(new T);
}

/// @brief Allocates a dynamic array of @p count default-initialized elements managed by `UniquePtr<T>`.
/// @details Uses `new ElementType[count]` without `()`, skipping zero-initialization for trivial types (such as byte
///          staging buffers, scratch arrays, or POD structs) while still running default constructors on non-trivial
///          elements.
/// @note Only use for trivial types when the caller overwrites every element before reading it.
/// @tparam T Unbounded array type (`ElementType[]`) with a default-initializable element type.
/// @param[in] count Number of elements to allocate (may be `0`).
/// @return `UniquePtr<T>` owning the allocated array.
template <typename T>
requires(detail::IsUnboundedArray<T> && concepts::IsDefaultConstructible<std::remove_extent_t<T>>)
[[nodiscard]] GP_FORCEINLINE_HINT constexpr UniquePtr<T> makeUniqueForOverwrite(const gp::USize count) noexcept
{
    using ElementType = std::remove_extent_t<T>;
    return UniquePtr<T>(new ElementType[count]);
}

/// @brief Explicitly rejects bounded array types (`T[N]`) in `makeUniqueForOverwrite`.
/// @tparam T Bounded array type (`ElementType[N]`).
/// @tparam Ts Unused argument types.
template <typename T, typename... Ts>
requires detail::IsBoundedArray<T> void makeUniqueForOverwrite(Ts&&...) = delete;

namespace concepts
{

/// @brief Marks `UniquePtr<T, Deleter>` as bitwise relocatable whenever its stored deleter is non-reference and
///        relocatable.
/// @details Enables container growth (`gp::memory::relocateConstructItems`) to relocate `UniquePtr` arrays via a
///          single `memmove` instead of per-element move construction and destruction.
/// @tparam T Managed object or array type.
/// @tparam Deleter Stored deleter type.
template <typename T, typename Deleter>
struct IsRelocatableTrait<gp::UniquePtr<T, Deleter>>
    : std::bool_constant<!std::is_reference_v<Deleter> && IsRelocatable<Deleter>>
{};

}   // namespace concepts

}   // namespace gp
