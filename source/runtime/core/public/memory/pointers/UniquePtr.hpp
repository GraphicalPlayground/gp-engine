// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Concepts.hpp"
#include "concepts/Fundamental.hpp"
#include "concepts/Relationship.hpp"
#include "CoreMinimal.hpp"
#include <compare>

namespace gp
{

/// @brief Default deleter for use with UniquePtr.
/// @tparam T The type of the object to be deleted.
template <typename T>
class DefaultDelete
{
public:
    /// @brief Default constructor, copy constructor, copy assignment operator, and destructor.
    constexpr DefaultDelete() noexcept = default;
    constexpr DefaultDelete(const DefaultDelete&) noexcept = default;
    constexpr DefaultDelete& operator=(const DefaultDelete&) noexcept = default;
    constexpr ~DefaultDelete() noexcept = default;

    /// @brief Templated copy constructor and assignment operator to allow for polymorphic deletion.
    /// @tparam U Convertible type to T, allowing for deletion of derived types through base pointers.
    template <concepts::IsPointerConvertibleTo<T> U>
    constexpr DefaultDelete(const DefaultDelete<U>&) noexcept
    {}

public:
    /// @brief Templated assignment operator to allow for polymorphic deletion.
    /// @tparam U Convertible type to T, allowing for deletion of derived types through base pointers.
    template <concepts::IsPointerConvertibleTo<T> U>
    constexpr DefaultDelete& operator=(const DefaultDelete<U>&) noexcept
    {
        return *this;
    }

    /// @brief Function call operator to delete the managed object.
    /// @param[in] ptr Pointer to the object to be deleted.
    constexpr void operator()(T* ptr) const noexcept
    {
        delete ptr;
    }
};

/// @brief Specialization of DefaultDelete for array types.
/// @tparam T The type of the array elements to be deleted.
template <typename T>
class DefaultDelete<T[]>
{
public:
    /// @brief Default constructor, copy constructor, copy assignment operator, and destructor.
    constexpr DefaultDelete() noexcept = default;
    constexpr DefaultDelete(const DefaultDelete&) noexcept = default;
    constexpr DefaultDelete& operator=(const DefaultDelete&) noexcept = default;
    constexpr ~DefaultDelete() noexcept = default;

    /// @brief Templated copy constructor to allow for polymorphic deletion of arrays.
    /// @tparam U Convertible type to T, allowing for deletion of derived types through base pointers.
    template <typename U>
    requires concepts::IsPointerConvertibleTo<U (*)[], T (*)[]>
    constexpr DefaultDelete(const DefaultDelete<U[]>&) noexcept
    {}

public:
    /// @brief Templated assignment operator to allow for polymorphic deletion of arrays.
    /// @tparam U Convertible type to T, allowing for deletion of derived types through base pointers.
    template <typename U>
    requires concepts::IsPointerConvertibleTo<U (*)[], T (*)[]>
    constexpr DefaultDelete& operator=(const DefaultDelete<U[]>&) noexcept
    {
        return *this;
    }

    /// @brief Function call operator to delete the managed array.
    /// @tparam U Convertible type to T, allowing for deletion of derived types through base pointers.
    /// @param[in] ptr Pointer to the array to be deleted.
    template <typename U>
    requires concepts::IsPointerConvertibleTo<U (*)[], T (*)[]> constexpr void operator()(U* ptr) const noexcept
    {
        delete[] ptr;
    }
};

/// @brief A smart pointer that manages the lifetime of a dynamically allocated object, ensuring unique ownership and
/// automatic deletion when the UniquePtr goes out of scope.
/// @tparam T The type of the object to be managed by the UniquePtr.
/// @tparam Deleter The type of the deleter to be used for managing the lifetime of the object.
/// @see SharedPtr, WeakPtr, RefCountedPtr
template <typename T, typename Deleter = DefaultDelete<T>>
class UniquePtr : private Deleter
{
private:
    template <typename OtherT, typename OtherDeleter>
    friend class UniquePtr;

public:
    using ElementType = T;
    using PointerType = T*;

private:
    PointerType m_ptr{ nullptr };

public:
    /// @brief Deleted copy constructor and copy assignment operator to enforce unique ownership semantics.
    UniquePtr(const UniquePtr&) noexcept = delete;
    UniquePtr& operator=(const UniquePtr&) noexcept = delete;

    /// @brief Default constructor, constructs a UniquePtr that owns no object.
    constexpr UniquePtr() noexcept
        : Deleter()
    {}

    /// @brief Constructor that creates a UniquePtr that owns no object, using a null pointer.
    constexpr UniquePtr(gp::NullPtrT) noexcept
        : Deleter()
    {}

    /// @brief Move constructor, transfers ownership from another UniquePtr.
    /// @param[in] other The UniquePtr to transfer ownership from.
    constexpr UniquePtr(UniquePtr&& other) noexcept
        : Deleter(std::move(other.getDeleter()))
        , m_ptr(other.m_ptr)
    {
        other.m_ptr = nullptr;
    }

    /// @brief Constructor that takes ownership of a raw pointer, using the default deleter.
    /// @tparam U A type that is convertible to T, allowing for polymorphic ownership.
    /// @param[in] ptr The raw pointer to take ownership of.
    template <concepts::IsPointerConvertibleTo<T> U>
    explicit constexpr UniquePtr(U* ptr) noexcept
        : Deleter()
        , m_ptr(ptr)
    {}

    /// @brief Constructor that takes ownership of a raw pointer, using a custom deleter.
    /// @tparam U A type that is convertible to T, allowing for polymorphic ownership.
    /// @param[in] ptr The raw pointer to take ownership of.
    /// @param[in] deleter The deleter to use for managing the lifetime of the object.
    template <concepts::IsPointerConvertibleTo<T> U>
    explicit constexpr UniquePtr(U* ptr, Deleter&& deleter) noexcept
        : Deleter(std::move(deleter))
        , m_ptr(ptr)
    {}

    /// @brief Constructor that takes ownership of a raw pointer, using a custom deleter.
    /// @tparam U A type that is convertible to T, allowing for polymorphic ownership.
    /// @param[in] ptr The raw pointer to take ownership of.
    /// @param[in] deleter The deleter to use for managing the lifetime of the object.
    template <concepts::IsPointerConvertibleTo<T> U>
    explicit constexpr UniquePtr(U* ptr, const Deleter& deleter) noexcept
        : Deleter(deleter)
        , m_ptr(ptr)
    {}

    /// @brief Templated move constructor that allows for transferring ownership from a UniquePtr of a different type.
    /// @tparam U A type that is convertible to T, allowing for polymorphic ownership.
    /// @tparam D A deleter type that is convertible to Deleter, allowing for polymorphic deletion.
    /// @param[in] other The UniquePtr to transfer ownership from.
    template <concepts::IsPointerConvertibleTo<T> U, typename D>
    requires(!concepts::IsArray<U>) constexpr UniquePtr(UniquePtr<U, D>&& other) noexcept
        : Deleter(std::move(other.getDeleter()))
        , m_ptr(other.m_ptr)
    {
        other.m_ptr = nullptr;
    }

    /// @brief Destructor, deletes the managed object if it exists.
    GP_FORCEINLINE_HINT constexpr ~UniquePtr() noexcept
    {
        getDeleter()(m_ptr);
    }

public:
    /// @brief Move assignment operator, transfers ownership from another UniquePtr.
    /// @param[in] other The UniquePtr to transfer ownership from.
    constexpr UniquePtr& operator=(UniquePtr&& other) noexcept
    {
        if (this != &other)
        {
            PointerType ptr = m_ptr;
            m_ptr = other.m_ptr;
            other.m_ptr = nullptr;
            getDeleter()(ptr);
        }

        getDeleter() = std::move(other.getDeleter());
        return *this;
    }

    /// @brief Assignment operator that resets the UniquePtr to manage no object.
    constexpr UniquePtr& operator=(gp::NullPtrT) noexcept
    {
        PointerType ptr = m_ptr;
        m_ptr = nullptr;
        getDeleter()(ptr);
        return *this;
    }

    /// @brief Templated move assignment operator that allows for transferring ownership from a UniquePtr of a different
    /// type.
    /// @tparam U A type that is convertible to T, allowing for polymorphic ownership.
    /// @tparam D A deleter type that is convertible to Deleter, allowing for polymorphic deletion.
    /// @param[in] other The UniquePtr to transfer ownership from.
    template <concepts::IsPointerConvertibleTo<T> U, typename D>
    requires(!concepts::IsArray<U>) constexpr UniquePtr& operator=(UniquePtr<U, D>&& other) noexcept
    {
        PointerType ptr = m_ptr;
        m_ptr = other.m_ptr;
        other.m_ptr = nullptr;
        getDeleter()(ptr);
        getDeleter() = std::move(other.getDeleter());
        return *this;
    }

    /// @brief Conversion operator to check if the UniquePtr currently owns an object.
    /// @return True if the UniquePtr owns an object, false otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT explicit constexpr operator bool() const noexcept
    {
        return isValid();
    }

    /// @brief Indirection operator to access the managed object.
    /// @return Pointer to the managed object.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr PointerType operator->() const noexcept
    {
        return m_ptr;
    }

    /// @brief Dereference operator to access the managed object.
    /// @return Reference to the managed object.
    /// @note This operator is only valid if the UniquePtr currently owns an object.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr std::add_lvalue_reference_t<T> operator*() const noexcept
        requires(!gp::concepts::IsVoid<T>)
    {
        // TODO: Replace std::add_lvalue_reference_t with a custom type trait.
        return *m_ptr;
    }

    /// @brief Equality operator to compare two UniquePtr instances for equality.
    /// @tparam U The type of the other UniquePtr to compare with.
    /// @param[in] rhs The other UniquePtr to compare with.
    /// @return True if both UniquePtr instances manage the same object, false otherwise.
    template <typename U>
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool operator==(const UniquePtr<U>& rhs) const noexcept
    {
        return get() == rhs.get();
    }

    /// @brief Equality operator to compare a UniquePtr with a null pointer.
    /// @param[in] rhs The null pointer to compare with.
    /// @return True if the UniquePtr does not own an object, false otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool operator==(gp::NullPtrT) const noexcept
    {
        return !isValid();
    }

    /// @brief Three-way comparison operator to compare two UniquePtr instances.
    /// @tparam U The type of the other UniquePtr to compare with.
    /// @param[in] rhs The other UniquePtr to compare with.
    /// @return A std::strong_ordering value indicating the relative ordering of the two UniquePtr instances.
    /// @details This operator allows for comparisons between UniquePtr instances, enabling sorting and ordering based
    /// on the addresses of the managed objects.
    template <typename U>
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr std::strong_ordering operator<=>(const UniquePtr<U>& rhs) const noexcept
    {
        return std::compare_three_way{}(get(), rhs.get());
    }

    /// @brief Three-way comparison operator to compare a UniquePtr with a null pointer.
    /// @param[in] rhs The null pointer to compare with.
    /// @return A std::strong_ordering value indicating the relative ordering of the UniquePtr and the null pointer.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr std::strong_ordering operator<=>(gp::NullPtrT) const noexcept
    {
        return std::compare_three_way{}(get(), static_cast<PointerType>(nullptr));
    }

public:
    /// @brief Checks if the UniquePtr currently owns an object.
    /// @return True if the UniquePtr owns an object, false otherwise.
    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        return m_ptr != nullptr;
    }

    /// @brief Gets the raw pointer managed by the UniquePtr.
    /// @return The raw pointer to the managed object.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr PointerType get() const noexcept
    {
        return m_ptr;
    }

    /// @brief Releases ownership of the managed object and returns the raw pointer.
    /// @return The raw pointer to the managed object, or nullptr if no object is owned.
    [[nodiscard]] constexpr PointerType release() noexcept
    {
        PointerType ptr = m_ptr;
        m_ptr = nullptr;
        return ptr;
    }

    /// @brief Resets the UniquePtr to manage a new object, deleting the currently managed object if it exists.
    /// @param[in] ptr The new raw pointer to manage. If nullptr, the UniquePtr will not manage any object.
    constexpr void reset(PointerType ptr = nullptr) noexcept
    {
        if (m_ptr != ptr)
        {
            PointerType oldPtr = m_ptr;
            m_ptr = ptr;
            getDeleter()(oldPtr);
        }
    }

    /// @brief Gets the deleter associated with this UniquePtr.
    /// @return Reference to the deleter object.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr Deleter& getDeleter() noexcept
    {
        return static_cast<Deleter&>(*this);
    }

    /// @brief Gets the deleter associated with this UniquePtr (const version).
    /// @return Const reference to the deleter object.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr const Deleter& getDeleter() const noexcept
    {
        return static_cast<const Deleter&>(*this);
    }
};

/// @brief
/// @tparam T
/// @tparam Deleter
template <typename T, typename Deleter>
class UniquePtr<T[], Deleter> : private Deleter
{
private:
    template <typename OtherT, typename OtherDeleter>
    friend class UniquePtr;
};

/// @brief Creates a UniquePtr that manages a dynamically allocated object of type T.
/// The object is constructed by forwarding the provided arguments. If no arguments are
/// provided, the object is value-initialized (which zeroes out trivial types).
/// @tparam T The type of the object to be managed by the UniquePtr.
/// @tparam Ts The types of the arguments to be forwarded to the constructor of T.
/// @param[in] args The arguments to be forwarded to the constructor of T.
/// @return A UniquePtr that manages the newly created object of type T.
template <typename T, typename... Ts>
requires(!concepts::IsArray<T>)
[[nodiscard]] GP_FORCEINLINE_HINT constexpr UniquePtr<T> makeUnique(Ts&&... args) noexcept
{
    return UniquePtr<T>(new T(std::forward<Ts>(args)...));
}

/// @brief Creates a UniquePtr that manages a dynamically allocated object of type T.
/// The object is default-initialized, meaning trivial types (like ints, floats, or POD structs)
/// are left uninitialized (garbage memory) to save CPU cycles.
/// @note Use this ONLY when you intend to immediately overwrite the allocated memory.
/// @tparam T The type of the object to be managed by the UniquePtr.
/// @return A UniquePtr that manages the newly created uninitialized object of type T.
template <typename T>
requires(!concepts::IsArray<T>)
[[nodiscard]] GP_FORCEINLINE_HINT constexpr UniquePtr<T> makeUniqueForOverwrite() noexcept
{
    return UniquePtr<T>(new T);
}

}   // namespace gp

