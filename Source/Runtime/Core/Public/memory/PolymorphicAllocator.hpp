// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreMinimal.hpp"
#include "memory/Allocator.hpp"
#include "memory/DefaultAllocator.hpp"

namespace gp::memory
{

/// @brief Type-erased allocator wrapper for use as a container template parameter.
/// @details
/// Acts as the GP equivalent of `std::pmr::PolymorphicAllocator`. Wraps a pointer to `gp::memory::Allocator`,
/// providing a strongly-typed interface that containers interact with through `AllocatorTraits`.
///
/// Propagation semantics follow C++17 `std::pmr`:
///   - Never propagate on copy (copy yields default_allocator)
///   - Always propagate on move (move transfers the resource)
///   - Swap: O(1) if allocators are equal, otherwise undefined
///
/// sizeof(PolymorphicAllocator<T>) == sizeof(void*). Zero overhead when stored inline in a container.
/// @tparam T The value type this allocator produces.
template <typename T>
class PolymorphicAllocator
{
public:
    using ValueType = T;
    using SizeType = USize;
    using DifferenceType = ISize;
    using PropagateOnContainerCopyAssignment = std::false_type;
    using PropagateOnContainerMoveAssignment = std::true_type;
    using PropagateOnContainerSwap = std::false_type;
    using IsAlwaysEqual = std::false_type;

private:
    memory::Allocator* m_resource;

public:
    /// @brief Constructs a PolymorphicAllocator using the global default allocator.
    PolymorphicAllocator() noexcept
        : m_resource(&memory::DefaultAllocator::get())
    {}

    /// @brief Constructs a PolymorphicAllocator wrapping the given resource.
    /// @param[in] resource Pointer to the backing allocator. Must outlive this wrapper and all containers using it.
    PolymorphicAllocator(memory::Allocator* resource) noexcept
        : m_resource(resource)
    {}

    /// @brief Converting copy constructor for rebinding to a different value type.
    /// @tparam U The source value type.
    template <typename U>
    PolymorphicAllocator(const PolymorphicAllocator<U>& other) noexcept
        : m_resource(other.getResource())
    {}

    PolymorphicAllocator(const PolymorphicAllocator&) noexcept = default;
    PolymorphicAllocator& operator=(const PolymorphicAllocator&) noexcept = default;
    ~PolymorphicAllocator() = default;

public:
    /// @brief Two PolymorphicAllocators are equal if they wrap the same resource.
    template <typename U>
    GP_NODISCARD bool operator==(const PolymorphicAllocator > &other) const noexcept
    {
        return m_resource == other.getResource();
    }

    /// @brief Inequality comparison.
    template <typename U>
    GP_NODISCARD bool operator!=(const PolymorphicAllocator > &other) const noexcept
    {
        return m_resource != other.getResource();
    }

public:
    /// @brief Allocates memory for n objects of type T.
    /// @param[in] n Number of objects to allocate space for.
    /// @return Pointer to the allocated memory, or nullptr on failure.
    GP_NODISCARD T* allocate(SizeType n) { return static_cast<T*>(m_resource->allocate(n * sizeof(T), alignof(T))); }

    /// @brief Deallocates memory for n objects of type T.
    /// @param[in] ptr Pointer previously returned by allocate().
    /// @param[in] n Number of objects the allocation was sized for.
    void deallocate(T* ptr, SizeType n) { m_resource->deallocate(ptr, n * sizeof(T)); }

    /// @brief Constructs an object at the given pointer using placement new.
    /// @tparam U The type to construct.
    /// @tparam Args Constructor argument types.
    /// @param[in] ptr Pointer to uninitialized storage.
    /// @param[in] args Arguments forwarded to U's constructor.
    template <typename U, typename... Args>
    void construct(U* ptr, Args&&... args)
    {
        ::new (static_cast<void*>(ptr)) U(static_cast<Args&&>(args)...);
    }

    /// @brief Destroys the object at the given pointer without deallocating.
    /// @tparam U The type to destroy.
    /// @param[in] ptr Pointer to the object.
    template <typename U>
    void destroy(U* ptr)
    {
        ptr->~U();
    }

    /// @brief Returns a default-constructed PolymorphicAllocator for copy-constructed containers.
    /// @details Per std::pmr semantics, copied containers do not inherit the source's allocator.
    PolymorphicAllocator selectOnContainerCopyConstruction() const { return PolymorphicAllocator(); }

    /// @brief Returns the underlying memory resource.
    /// @return Pointer to the backing gp::memory::Allocator.
    GP_NODISCARD GP_FORCEINLINE memory::Allocator* getResource() const noexcept { return m_resource; }
};

}   // namespace gp::memory
