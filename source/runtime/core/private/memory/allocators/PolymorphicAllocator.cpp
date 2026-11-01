// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "memory/allocators/PolymorphicAllocator.hpp"
#include "memory/allocators/HeapAllocator.hpp"

namespace gp::memory
{

namespace detail
{

/// @brief Process-wide `IAllocator` adapter routing virtual calls to `HeapAllocator`.
class DefaultPolymorphicHeapResource final : public IAllocator
{
public:
    constexpr DefaultPolymorphicHeapResource() noexcept = default;
    virtual ~DefaultPolymorphicHeapResource() noexcept override = default;

    [[nodiscard]] virtual void* allocate(const gp::USize sizeInBytes, const gp::UInt32 alignment) noexcept override
    {
        HeapAllocator heap{};
        return heap.allocate(sizeInBytes, alignment);
    }

    [[nodiscard]] virtual void* reallocate(
        void* ptr, const gp::USize oldSizeInBytes, const gp::USize newSizeInBytes, const gp::UInt32 alignment
    ) noexcept override
    {
        HeapAllocator heap{};
        return heap.reallocate(ptr, oldSizeInBytes, newSizeInBytes, alignment);
    }

    virtual void deallocate(void* ptr, const gp::USize oldSizeInBytes, const gp::UInt32 alignment) noexcept override
    {
        HeapAllocator heap{};
        heap.deallocate(ptr, oldSizeInBytes, alignment);
    }

    [[nodiscard]] virtual gp::USize
        getActualAllocationSize(const gp::USize requestedSize, const gp::UInt32 alignment) const noexcept override
    {
        const HeapAllocator heap{};
        return heap.getActualAllocationSize(requestedSize, alignment);
    }

    [[nodiscard]] virtual bool owns(const void* ptr) const noexcept override
    {
        const HeapAllocator heap{};
        return heap.owns(ptr);
    }

    [[nodiscard]] virtual const char* getName() const noexcept override
    {
        return "PolymorphicAllocator.DefaultHeap";
    }
};

}   // namespace detail

IAllocator& PolymorphicAllocator::getDefaultResource() noexcept
{
    static detail::DefaultPolymorphicHeapResource s_defaultResource{};
    return s_defaultResource;
}

}   // namespace gp::memory
