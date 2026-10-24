// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "memory/HardwareConstants.hpp"
#include "threading/atomics/Atomic.hpp"

namespace gp::thread
{

/// @brief Atomic variable padded and aligned to the target CPU cache line size.
/// @details Prevents false sharing between CPU cores by ensuring no other variable shares the same cache line.
/// @tparam T Value type satisfying the AtomicValue concept.
template <AtomicValue T>
struct alignas(gp::memory::HardwareConstants::cacheLineSize) CacheAlignedAtomic final
{
    /// @brief Underlying atomic storage.
    Atomic<T> value{};

    /// @brief Default-constructs the atomic variable.
    [[nodiscard]] constexpr CacheAlignedAtomic() noexcept = default;

    /// @brief Constructs the atomic variable with an initial value.
    /// @param[in] initialValue Initial value to store.
    [[nodiscard]] constexpr explicit(false) CacheAlignedAtomic(T initialValue) noexcept
        : value(initialValue)
    {}

    /// @brief Deleted copy constructor.
    CacheAlignedAtomic(const CacheAlignedAtomic&) = delete;

    /// @brief Deleted copy assignment operator.
    CacheAlignedAtomic& operator=(const CacheAlignedAtomic&) = delete;

    /// @brief Deleted move constructor.
    CacheAlignedAtomic(CacheAlignedAtomic&&) = delete;

    /// @brief Deleted move assignment operator.
    CacheAlignedAtomic& operator=(CacheAlignedAtomic&&) = delete;

    /// @brief Destructor.
    constexpr ~CacheAlignedAtomic() noexcept = default;
};

static_assert(
    sizeof(CacheAlignedAtomic<gp::UInt32>) == gp::memory::HardwareConstants::cacheLineSize,
    "CacheAlignedAtomic size must match the target CPU cache line size."
);

}   // namespace gp::thread
