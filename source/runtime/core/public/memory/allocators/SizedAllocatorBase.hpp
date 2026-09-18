// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "compilers/clang/ClangCompiler.hpp"
#include "concepts/Concepts.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"         // IWYU pragma: keep
#include "memory/allocators/AllocatorUtilities.hpp"
#include "memory/MemoryBase.hpp"
#include "platforms/base/Platform.hpp"
#include <limits>
#include <utility>

namespace gp::memory
{

namespace detail
{

/// @brief A base class for sized allocators that provides common functionality and type definitions.
template <gp::Int32 Bits>
struct BitsToSizeType
{
    /// @details Compile-time failure for unsupported allocator index sizes.
    static_assert(Bits == Bits + 1, "Unsupported allocator index size.");
};

// clang-format off

/// @details We use signed integer types for size types to allow for negative values to be used safely.
template<> struct BitsToSizeType<8>  { using Type = gp::Int8; };
template<> struct BitsToSizeType<16> { using Type = gp::Int16; };
template<> struct BitsToSizeType<32> { using Type = gp::Int32; };
template<> struct BitsToSizeType<64> { using Type = gp::Int64; };

// clang-format on

}   // namespace detail

/// @brief A base class for sized allocators that provides common functionality and type definitions.
/// @tparam IndexSize The size of the index type used by the allocator (in bits).
/// @tparam SubClass The derived class that inherits from this base class.
template <gp::Int32 IndexSize, typename SubClass>
class SizedAllocatorBase
{
public:
    using SizeType = detail::BitsToSizeType<IndexSize>::Type;
    using USizeType = std::make_unsigned_t<SizeType>;

public:
    static constexpr bool kNeedsElementType = false;
    static constexpr bool kRequireRangeCheck = true;

public:
    /// @brief Untyped allocation interface for containers that manage their own element types.
    class ForAnyElementType
    {
    private:
        template <gp::Int32, typename>
        friend class SizedAllocatorBase;

    private:
        gp::UntypedContainerElement* m_data{ nullptr };

    public:
        /// @brief Default constructor, initializes with no allocation.
        GP_NODEBUG constexpr ForAnyElementType() noexcept
            : m_data(nullptr)
        {}

        /// @brief Copying allocators is explicitly disabled.
        ForAnyElementType(const ForAnyElementType&) = delete;
        ForAnyElementType& operator=(const ForAnyElementType&) = delete;

        /// @brief Destructor, frees any held memory via the derived allocator class.
        GP_NODEBUG GP_FORCEINLINE_HINT constexpr ~ForAnyElementType() noexcept
        {
            if (m_data)
            {
                // C++23 deducing `this` cannot be applied to destructors, so the CRTP cast remains necessary here.
                static_cast<SubClass::ForAnyElementType*>(this)->deallocate(m_data);
            }
        }

    public:
        /// @brief Transfers memory ownership from a different allocator type.
        /// @tparam OtherAllocator Source allocator type.
        /// @tparam Self Deducing this type.
        /// @param[in] self The target allocator instance.
        /// @param[in,out] other The source allocator instance to steal memory from.
        template <typename OtherAllocator, typename Self>
        GP_NODEBUG GP_FORCEINLINE_HINT constexpr void
            takeOwnershipFromOther(this Self&& self, typename OtherAllocator::ForAnyElementType& other)
        {
            // TODO: Add a real check for allocator compatibility here.
            [[assume(static_cast<void*>(&self) != static_cast<void*>(&other))]];

            if (self.m_data)
            {
                self.deallocate(self.m_data);
            }

            // Optimized move, generates zero-cost register swaps.
            self.m_data = std::exchange(other.m_data, nullptr);
        }

        /// @brief Transfers memory ownership from another allocator of the same derived type.
        /// @tparam Self Deducing this type.
        /// @param[in] self The target allocator instance.
        /// @param[in,out] other The source allocator instance to steal memory from.
        template <typename Self>
        GP_NODEBUG GP_FORCEINLINE_HINT constexpr void
            takeOwnership(this Self&& self, SubClass::ForAnyElementType& other)
        {
            self.template takeOwnershipFromOther<SubClass>(other);
        }

        /// @brief Retrieves the raw pointer to the managed memory block.
        /// @return Pointer to the untyped allocation.
        [[nodiscard]] GP_NODEBUG GP_FORCEINLINE_HINT constexpr gp::UntypedContainerElement*
            getAllocation() const noexcept
        {
            return m_data;
        }

        /// @brief Checks if the allocator currently holds an active memory allocation.
        /// @return True if memory is allocated, false otherwise.
        [[nodiscard]] GP_NODEBUG constexpr bool hasAllocation() const noexcept
        {
            return m_data != nullptr;
        }

        /// @brief Modifies the size of the current memory allocation.
        /// @tparam Self Deducing this type.
        /// @param[in] self The allocator instance.
        /// @param[in] currentSize Current capacity in elements.
        /// @param[in] newSize Requested capacity in elements.
        /// @param[in] bytesPerElement Size of a single element in bytes.
        template <typename Self>
        GP_NODEBUG void resizeAllocation(
            this Self&& self, [[maybe_unused]] SizeType currentSize, SizeType newSize, gp::USize bytesPerElement
        )
        {
            if (!self.m_data && newSize == 0)
            {
                return;
            }

            static_assert(sizeof(SizeType) <= sizeof(gp::USize), "gp::USize is expected to handle all possible sizes.");

            bool isInvalidResize = newSize < 0 || bytesPerElement < 1 ||
                                   bytesPerElement > static_cast<gp::USize>(std::numeric_limits<gp::Int32>::max());
            if constexpr (sizeof(SizeType) == sizeof(gp::USize))
            {
                isInvalidResize =
                    isInvalidResize || (static_cast<gp::USize>(static_cast<USizeType>(newSize)) >
                                        static_cast<gp::USize>(std::numeric_limits<SizeType>::max()) / bytesPerElement);
            }

            if (isInvalidResize) [[unlikely]]
            {
                // TODO: Implement a proper error handling mechanism for invalid resize operations.
                //       OnInvalidSizedHeapAllocatorNum(indexSize, newSize, bytesPerElement);
            }

            self.m_data =
                static_cast<gp::UntypedContainerElement*>(self.reallocate(self.m_data, newSize, bytesPerElement));
        }

        /// @brief Modifies the size of the current memory allocation, respecting strict alignment requirements.
        /// @tparam Self Deducing this type.
        /// @param[in] self The allocator instance.
        /// @param[in] currentSize Current capacity in elements.
        /// @param[in] newSize Requested capacity in elements.
        /// @param[in] bytesPerElement Size of a single element in bytes.
        /// @param[in] alignment Required memory alignment.
        template <typename Self>
        GP_NODEBUG void resizeAllocation(
            this Self&& self,
            [[maybe_unused]] SizeType currentSize,
            SizeType newSize,
            gp::USize bytesPerElement,
            gp::UInt32 alignment
        )
        {
            if (!self.m_data && newSize == 0)
            {
                return;
            }

            static_assert(sizeof(SizeType) <= sizeof(gp::USize), "gp::USize is expected to handle all possible sizes.");

            bool isInvalidResize = newSize < 0 || bytesPerElement < 1 ||
                                   bytesPerElement > static_cast<gp::USize>(std::numeric_limits<gp::Int32>::max());
            if constexpr (sizeof(SizeType) == sizeof(gp::USize))
            {
                isInvalidResize =
                    isInvalidResize || (static_cast<gp::USize>(static_cast<USizeType>(newSize)) >
                                        static_cast<gp::USize>(std::numeric_limits<SizeType>::max()) / bytesPerElement);
            }

            if (isInvalidResize) [[unlikely]]
            {
                // TODO: Implement a proper error handling mechanism for invalid resize operations.
                //       OnInvalidSizedHeapAllocatorNum(indexSize, newSize, bytesPerElement);
            }

            self.m_data = static_cast<gp::UntypedContainerElement*>(
                self.reallocate(self.m_data, newSize, bytesPerElement, alignment)
            );
        }

        [[nodiscard]] GP_NODEBUG constexpr gp::USize
            getAllocatedSize(SizeType currentSize, gp::USize bytesPerElement) const
        {
            return currentSize * bytesPerElement;
        }

        [[nodiscard]] GP_NODEBUG constexpr SizeType getInitialCapacity() const noexcept
        {
            return 0;
        }

        /// @brief Platform-specific hook to reallocate a memory block. Must be implemented by the derived class.
        /// @param[in] data Existing memory block to resize, or null to allocate new.
        /// @param[in] newSize Requested new size in elements.
        /// @param[in] bytesPerElement Size of a single element in bytes.
        /// @param[in] alignment Required memory alignment.
        /// @return Pointer to the newly allocated or resized memory block.
        [[nodiscard]] void* reallocate(
            void* data,
            SizeType newSize,
            gp::USize bytesPerElement,
            gp::UInt32 alignment = gp::memory::kDefaultAlignment
        ) = delete;

        /// @brief Platform-specific hook to free a memory block. Must be implemented by the derived class.
        /// @param[in] data Memory block to deallocate.
        void deallocate(void* data) = delete;

        /// @brief Computes the optimal capacity for a reservation request.
        /// @param[in] newSize Minimum required capacity in elements.
        /// @param[in] bytesPerElement Size of a single element in bytes.
        /// @return The computed capacity in elements.
        [[nodiscard]] GP_NODEBUG GP_FORCEINLINE_HINT constexpr SizeType
            calculateSlackReserve(SizeType newSize, gp::USize bytesPerElement) const noexcept
        {
            return detail::defaultCalculateSlackReserve(newSize, bytesPerElement, true);
        }

        /// @brief Computes the optimal capacity for a reservation request, respecting alignment.
        /// @param[in] newSize Minimum required capacity in elements.
        /// @param[in] bytesPerElement Size of a single element in bytes.
        /// @param[in] alignment Required memory alignment.
        /// @return The computed capacity in elements.
        [[nodiscard]] GP_NODEBUG GP_FORCEINLINE_HINT constexpr SizeType
            calculateSlackReserve(SizeType newSize, gp::USize bytesPerElement, gp::UInt32 alignment) const noexcept
        {
            return detail::defaultCalculateSlackReserve(newSize, bytesPerElement, true, alignment);
        }

        /// @brief Computes the new capacity when shrinking a container, minimizing excessive reallocation.
        /// @param[in] newSize Requested new size in elements.
        /// @param[in] currentSize Current capacity in elements.
        /// @param[in] bytesPerElement Size of a single element in bytes.
        /// @return The computed capacity in elements.
        [[nodiscard]] GP_NODEBUG GP_FORCEINLINE_HINT constexpr SizeType
            calculateSlackShrink(SizeType newSize, SizeType currentSize, gp::USize bytesPerElement) const noexcept
        {
            return detail::defaultCalculateSlackShrink(newSize, currentSize, bytesPerElement, true);
        }

        /// @brief Computes the new capacity when shrinking a container, minimizing excessive reallocation, respecting
        /// alignment.
        /// @param[in] newSize Requested new size in elements.
        /// @param[in] currentSize Current capacity in elements.
        /// @param[in] bytesPerElement Size of a single element in bytes.
        /// @param[in] alignment Required memory alignment.
        /// @return The computed capacity in elements.
        [[nodiscard]] GP_NODEBUG GP_FORCEINLINE_HINT constexpr SizeType calculateSlackShrink(
            SizeType newSize, SizeType currentSize, gp::USize bytesPerElement, gp::UInt32 alignment
        ) const noexcept
        {
            return detail::defaultCalculateSlackShrink(newSize, currentSize, bytesPerElement, true, alignment);
        }

        /// @brief Computes the next capacity when a container exhausts its current allocation.
        /// @param[in] newSize Minimum capacity required to fulfill the current operation.
        /// @param[in] currentSize Current capacity in elements.
        /// @param[in] bytesPerElement Size of a single element in bytes.
        /// @return The computed capacity in elements.
        [[nodiscard]] GP_NODEBUG GP_FORCEINLINE_HINT constexpr SizeType
            calculateSlackGrow(SizeType newSize, SizeType currentSize, gp::USize bytesPerElement) const noexcept
        {
            return detail::defaultCalculateSlackGrow(newSize, currentSize, bytesPerElement, true);
        }

        /// @brief Computes the next capacity when a container exhausts its current allocation, respecting alignment.
        /// @param[in] newSize Minimum capacity required to fulfill the current operation.
        /// @param[in] currentSize Current capacity in elements.
        /// @param[in] bytesPerElement Size of a single element in bytes.
        /// @param[in] alignment Required memory alignment.
        /// @return The computed capacity in elements.
        [[nodiscard]] GP_NODEBUG GP_FORCEINLINE_HINT constexpr SizeType calculateSlackGrow(
            SizeType newSize, SizeType currentSize, gp::USize bytesPerElement, gp::UInt32 alignment
        ) const noexcept
        {
            return detail::defaultCalculateSlackGrow(newSize, currentSize, bytesPerElement, true, alignment);
        }
    };

    /// @brief Typed allocation interface for containers with known element types.
    /// @tparam T The element type being allocated.
    template <typename T>
    class ForElementType : public SubClass::ForAnyElementType
    {
    public:
        /// @brief Default constructor.
        ForElementType() = default;

    public:
        /// @brief Retrieves the strongly-typed pointer to the managed memory block.
        /// @return Typed pointer to the allocation.
        [[nodiscard]] GP_NODEBUG GP_FORCEINLINE_HINT constexpr T* getAllocation() const noexcept
        {
            return static_cast<T*>(ForAnyElementType::getAllocation());
        }
    };
};

}   // namespace gp::memory
