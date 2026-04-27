// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "memory/DefaultAllocator.hpp"
#include "memory/Forward.hpp"

namespace gp
{

/// @brief Default stateless deleter for UniquePtr.
/// @details
/// Destroys the object via its destructor and releases the underlying memory through gp::memory::DefaultAllocator.
/// Being empty and trivially constructible, this deleter participates in empty-base optimization via
/// [[no_unique_address]], guaranteeing sizeof(UniquePtr<T>) == sizeof(T*).
/// @tparam T The type of object being deleted.
template <typename T>
struct DefaultDelete
{
public:
    /// @brief Default constructor.
    constexpr DefaultDelete() noexcept = default;

    /// @brief Enables converting construction from a compatible deleter (e.g. Derived -> Base).
    template <typename U, typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
    constexpr DefaultDelete(const DefaultDelete<U>&) noexcept
    {}

public:
    /// @brief Destroys the object and frees its memory through the engine default allocator.
    /// @param[in] ptr Pointer to the object to destroy. nullptr is a safe no-op.
    GP_FORCEINLINE void operator()(T* ptr) const noexcept
    {
        static_assert(!std::is_void_v<T>, "gp::DefaultDelete cannot delete an incomplete type.");
        if (ptr)
        {
            ptr->~T();
            gp::memory::DefaultAllocator::get().deallocate(ptr, sizeof(T));
        }
    }
};

/// @brief Exclusive-ownership smart pointer with zero runtime overhead.
/// @details
/// Semantically mirrors std::unique_ptr but routes all allocations/deallocations through the engine's allocator
/// subsystem. The deleter is stored via [[no_unique_address]] so a stateless deleter collapses to zero bytes,
/// guaranteeing sizeof(UniquePtr<T>) == sizeof(T*) on MSVC, Clang and GCC.
///
/// Ownership is moved-only: the copy constructor and copy assignment operator are deleted. Move operations are
/// guaranteed noexcept so UniquePtr is safe to store inside noexcept-strong containers (e.g. gp::Vector with a
/// trivial-relocate optimization).
/// @tparam T The pointee type.
/// @tparam Deleter The deleter callable; defaults to gp::DefaultDelete<T>.
template <typename T, typename Deleter = DefaultDelete<T>>
class UniquePtr
{
public:
    using ElementType = T;
    using Pointer = T*;
    using DeleterType = Deleter;

private:
    Pointer m_pointer{ nullptr };
    GP_NO_UNIQUE_ADDRESS Deleter m_deleter{};

public:
    /// @brief Constructs an empty UniquePtr.
    constexpr UniquePtr() noexcept = default;

    /// @brief Constructs an empty UniquePtr from nullptr_t.
    constexpr UniquePtr(gp::NullPtr) noexcept {}

    /// @brief Takes exclusive ownership of the given raw pointer.
    /// @param[in] ptr Pointer to adopt; may be nullptr.
    explicit UniquePtr(Pointer ptr) noexcept
        : m_pointer(ptr)
    {}

    /// @brief Takes ownership of a raw pointer with a user-supplied deleter.
    /// @param[in] ptr Pointer to adopt.
    /// @param[in] deleter Deleter instance to store.
    UniquePtr(Pointer ptr, const Deleter& deleter) noexcept
        : m_pointer(ptr)
        , m_deleter(deleter)
    {}

    /// @brief Takes ownership of a raw pointer with a user-supplied deleter (rvalue overload).
    /// @param[in] ptr Pointer to adopt.
    /// @param[in] deleter Deleter instance to store (moved).
    UniquePtr(Pointer ptr, Deleter&& deleter) noexcept
        : m_pointer(ptr)
        , m_deleter(std::move(deleter))
    {}

    /// @brief Deleted copy constructor to enforce unique ownership semantics.
    UniquePtr(const UniquePtr&) = delete;

    /// @brief Deleted copy assignment operator to enforce unique ownership semantics.
    UniquePtr& operator=(const UniquePtr&) = delete;

    /// @brief Transfers ownership from another UniquePtr.
    UniquePtr(UniquePtr&& other) noexcept
        : m_pointer(other.m_pointer)
        , m_deleter(std::move(other.m_deleter))
    {
        other.m_pointer = nullptr;
    }

    /// @brief Converting move constructor for compatible pointee types (e.g. Derived -> Base).
    /// @tparam U The source UniquePtr's pointee type.
    /// @tparam E The source UniquePtr's deleter type.
    /// @param[in] other The UniquePtr to move from; must be convertible to this UniquePtr's types.
    template <
        typename U,
        typename E,
        typename = std::enable_if_t<std::is_convertible_v<U*, T*> && std::is_convertible_v<E, Deleter>>>
    UniquePtr(UniquePtr<U, E>&& other) noexcept
        : m_pointer(other.release())
        , m_deleter(std::forward<E>(other.getDeleter()))
    {}

    /// @brief Destructor that destroys the owned object (if any) via the deleter.
    ~UniquePtr() noexcept { reset(); }

    /// @brief Move assignment operator. Destroys the currently owned object.
    /// @param[in] other The UniquePtr to move from.
    /// @return A reference to this UniquePtr.
    UniquePtr& operator=(UniquePtr&& other) noexcept
    {
        if (this != &other)
        {
            reset(other.m_pointer);
            other.m_pointer = nullptr;
            m_deleter = std::move(other.m_deleter);
        }
        return *this;
    }

    /// @brief Converting move assignment for compatible pointee types.
    /// @tparam U The source UniquePtr's pointee type.
    /// @tparam E The source UniquePtr's deleter type.
    /// @param[in] other The UniquePtr to move from; must be convertible to this UniquePtr's types.
    /// @return A reference to this UniquePtr.
    template <
        typename U,
        typename E,
        typename = std::enable_if_t<std::is_convertible_v<U*, T*> && std::is_convertible_v<E, Deleter>>>
    UniquePtr& operator=(UniquePtr<U, E>&& other) noexcept
    {
        reset(other.release());
        m_deleter = std::forward<E>(other.getDeleter());
        return *this;
    }

    /// @brief Resets the pointer to empty on nullptr assignment.
    UniquePtr& operator=(gp::NullPtr) noexcept
    {
        reset();
        return *this;
    }

public:
    /// @brief Dereferences the owned object.
    /// @return A reference to the owned object.
    GP_FORCEINLINE T& operator*() const noexcept
    {
        GP_ASSERT(m_pointer != nullptr, "Dereferencing a null UniquePtr");
        return *m_pointer;
    }

    /// @brief Member access on the owned object.
    /// @return The raw pointer to the owned object.
    GP_FORCEINLINE Pointer operator->() const noexcept
    {
        GP_ASSERT(m_pointer != nullptr, "Accessing a null UniquePtr");
        return m_pointer;
    }

    /// @brief Tests whether the pointer is non-null.
    /// @return true if the UniquePtr owns an object; false if it is empty.
    GP_FORCEINLINE explicit operator bool() const noexcept { return m_pointer != nullptr; }

public:
    /// @brief Returns the underlying raw pointer without releasing ownership.
    /// @return The raw pointer to the owned object, or nullptr if empty.
    GP_NODISCARD GP_FORCEINLINE Pointer get() const noexcept { return m_pointer; }

    /// @brief Returns a mutable reference to the stored deleter.
    /// @return A reference to the deleter.
    GP_NODISCARD GP_FORCEINLINE Deleter& getDeleter() noexcept { return m_deleter; }

    /// @brief Returns a const reference to the stored deleter.
    /// @return A const reference to the deleter.
    GP_NODISCARD GP_FORCEINLINE const Deleter& getDeleter() const noexcept { return m_deleter; }

    /// @brief Relinquishes ownership, returning the raw pointer without destroying the object.
    /// @return The previously owned pointer; the UniquePtr becomes empty.
    GP_NODISCARD GP_FORCEINLINE Pointer release() noexcept
    {
        Pointer result = m_pointer;
        m_pointer = nullptr;
        return result;
    }

    /// @brief Destroys the currently owned object (if any) and adopts a new pointer.
    /// @param[in] ptr New pointer to adopt; defaults to nullptr.
    GP_FORCEINLINE void reset(Pointer ptr = nullptr) noexcept
    {
        Pointer old = m_pointer;
        m_pointer = ptr;
        if (old) { m_deleter(old); }
    }

    /// @brief Swaps the contents of this UniquePtr with another.
    /// @param[in] other The UniquePtr to swap with.
    GP_FORCEINLINE void swap(UniquePtr& other) noexcept
    {
        Pointer tmpPtr = m_pointer;
        m_pointer = other.m_pointer;
        other.m_pointer = tmpPtr;

        using std::swap;
        swap(m_deleter, other.m_deleter);
    }
};

/// @brief Constructs a T in allocator-backed memory and wraps it in a UniquePtr.
/// @details
/// Allocates through gp::memory::DefaultAllocator and uses placement-new to construct the object. If T's
/// constructor throws (only possible in builds compiled with exceptions enabled), the freshly allocated memory is
/// released before the exception propagates. In GP's default -fno-exceptions builds this function is
/// unconditionally noexcept and simply returns a default-constructed UniquePtr on allocation failure.
/// @tparam T The object type to construct.
/// @tparam Args Parameter pack forwarded to T's constructor.
/// @param[in] args Arguments forwarded to T's constructor.
/// @return A UniquePtr owning the newly constructed object, or an empty UniquePtr if allocation fails.
template <typename T, typename... Args>
GP_NODISCARD UniquePtr<T> makeUnique(Args&&... args) noexcept
{
    auto& allocator = gp::memory::DefaultAllocator::get();
    void* memory = allocator.allocate(sizeof(T), alignof(T));
    if (!memory) { return UniquePtr<T>{}; }
    T* object = ::new (memory) T(std::forward<Args>(args)...);
    return UniquePtr<T>(object);
}

/// @brief Non-member swap matching the standard convention.
/// @param[in] lhs The first UniquePtr to swap.
/// @param[in] rhs The second UniquePtr to swap.
template <typename T, typename D>
GP_FORCEINLINE void swap(UniquePtr<T, D>& lhs, UniquePtr<T, D>& rhs) noexcept
{
    lhs.swap(rhs);
}

}   // namespace gp

/// @brief Equality and ordering comparisons.
/// @param[in] lhs The left-hand side UniquePtr in the comparison.
/// @param[in] rhs The right-hand side UniquePtr in the comparison.
/// @return true if the raw pointers owned by lhs and rhs are equal; false otherwise.
template <typename T, typename D1, typename U, typename D2>
GP_FORCEINLINE bool operator==(const gp::UniquePtr<T, D1>& lhs, const gp::UniquePtr<U, D2>& rhs) noexcept
{
    return lhs.get() == rhs.get();
}

/// @brief Inequality comparison.
/// @param[in] lhs The left-hand side UniquePtr in the comparison.
/// @param[in] rhs The right-hand side UniquePtr in the comparison.
/// @return true if the raw pointers owned by lhs and rhs are not equal; false otherwise.
template <typename T, typename D1, typename U, typename D2>
GP_FORCEINLINE bool operator!=(const gp::UniquePtr<T, D1>& lhs, const gp::UniquePtr<U, D2>& rhs) noexcept
{
    return lhs.get() != rhs.get();
}

/// @brief Equality comparison with nullptr.
/// @param[in] lhs The UniquePtr to compare.
/// @param[in] rhs The nullptr to compare against.
/// @return true if lhs is empty (owns no object); false otherwise.
template <typename T, typename D>
GP_FORCEINLINE bool operator==(const gp::UniquePtr<T, D>& lhs, gp::NullPtr) noexcept
{
    return lhs.get() == nullptr;
}

/// @brief Equality comparison with nullptr (reversed).
/// @param[in] lhs The nullptr to compare against.
/// @param[in] rhs The UniquePtr to compare.
/// @return true if rhs is empty (owns no object); false otherwise.
template <typename T, typename D>
GP_FORCEINLINE bool operator==(gp::NullPtr, const gp::UniquePtr<T, D>& rhs) noexcept
{
    return rhs.get() == nullptr;
}

/// @brief Inequality comparison with nullptr.
/// @param[in] lhs The UniquePtr to compare.
/// @param[in] rhs The nullptr to compare against.
/// @return true if lhs is non-empty (owns an object); false otherwise.
template <typename T, typename D>
GP_FORCEINLINE bool operator!=(const gp::UniquePtr<T, D>& lhs, gp::NullPtr) noexcept
{
    return lhs.get() != nullptr;
}

/// @brief Inequality comparison with nullptr (reversed).
/// @param[in] lhs The nullptr to compare against.
/// @param[in] rhs The UniquePtr to compare.
/// @return true if rhs is non-empty (owns an object); false otherwise.
template <typename T, typename D>
GP_FORCEINLINE bool operator!=(gp::NullPtr, const gp::UniquePtr<T, D>& rhs) noexcept
{
    return rhs.get() != nullptr;
}
