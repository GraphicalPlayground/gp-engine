// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "memory/MemoryOrder.hpp"
#include "threading/details/AtomicStorage.hpp"

namespace gp::platform::generic
{

/// @brief Generic hardware and compiler atomic operations based on compiler built-ins.
/// @details Uses GCC and Clang __atomic_* built-ins to emit native lock-free instructions.
class Atomics
{
public:
    Atomics() = delete;
    ~Atomics() = delete;

private:
    template <gp::USize ByteSize>
    using StorageSelector = gp::thread::detail::AtomicStorageSelector<ByteSize>;

public:
    /// @brief Converts a memory order enum to the corresponding compiler built-in integer constant.
    /// @param[in] order Memory ordering constraint to map.
    /// @return Integer value understood by __atomic built-in functions.
    [[nodiscard]] GP_FORCEINLINE_HINT static constexpr int toBuiltInOrder(gp::memory::MemoryOrder order) noexcept
    {
        switch (order)
        {
        case gp::memory::MemoryOrder::Relaxed:
            return __ATOMIC_RELAXED;
        case gp::memory::MemoryOrder::Consume:
            return __ATOMIC_CONSUME;
        case gp::memory::MemoryOrder::Acquire:
            return __ATOMIC_ACQUIRE;
        case gp::memory::MemoryOrder::Release:
            return __ATOMIC_RELEASE;
        case gp::memory::MemoryOrder::AcqRel:
            return __ATOMIC_ACQ_REL;
        case gp::memory::MemoryOrder::SeqCst:
        default:
            return __ATOMIC_SEQ_CST;
        }
    }

    /// @brief Adjusts a memory order for compare-and-swap failure paths.
    /// @param[in] order Desired memory order for failure.
    /// @return Sanitized memory order without Release or AcqRel semantics.
    /// @note The C++ memory model prohibits Release and AcqRel semantics when compare-and-swap fails.
    [[nodiscard]] GP_FORCEINLINE_HINT static constexpr int toBuiltInFailureOrder(gp::memory::MemoryOrder order) noexcept
    {
        switch (order)
        {
        case gp::memory::MemoryOrder::Relaxed:
            return __ATOMIC_RELAXED;
        case gp::memory::MemoryOrder::Consume:
            return __ATOMIC_CONSUME;
        case gp::memory::MemoryOrder::Acquire:
        case gp::memory::MemoryOrder::AcqRel:
            return __ATOMIC_ACQUIRE;
        case gp::memory::MemoryOrder::Release:
            return __ATOMIC_RELAXED;
        case gp::memory::MemoryOrder::SeqCst:
        default:
            return __ATOMIC_SEQ_CST;
        }
    }

    /// @brief Reads a value from memory with specified ordering constraints.
    /// @tparam T Trivially copyable value type.
    /// @param[in] address Memory location to read from.
    /// @param[in] order Memory ordering constraint.
    /// @return Current value stored at the address.
    template <typename T>
    [[nodiscard]] GP_FORCEINLINE_HINT static T load(const T* address, gp::memory::MemoryOrder order) noexcept
    {
        constexpr gp::USize kByteSize = sizeof(T);
        using StorageT = typename StorageSelector<kByteSize>::UnsignedType;

        const StorageT* ptr = reinterpret_cast<const StorageT*>(address);
        StorageT result{};
        __atomic_load(ptr, &result, toBuiltInOrder(order));
        return __builtin_bit_cast(T, result);
    }

    /// @brief Writes a value to memory with specified ordering constraints.
    /// @tparam T Trivially copyable value type.
    /// @param[out] address Memory location to update.
    /// @param[in] desired Value to store.
    /// @param[in] order Memory ordering constraint.
    template <typename T>
    GP_FORCEINLINE_HINT static void store(T* address, T desired, gp::memory::MemoryOrder order) noexcept
    {
        constexpr gp::USize kByteSize = sizeof(T);
        using StorageT = typename StorageSelector<kByteSize>::UnsignedType;

        StorageT* ptr = reinterpret_cast<StorageT*>(address);
        StorageT rawDesired = __builtin_bit_cast(StorageT, desired);
        __atomic_store(ptr, &rawDesired, toBuiltInOrder(order));
    }

    /// @brief Replaces the value at a memory location and returns the previous value.
    /// @tparam T Trivially copyable value type.
    /// @param[in,out] address Memory location to modify.
    /// @param[in] desired New value to store.
    /// @param[in] order Memory ordering constraint.
    /// @return Value held at the address prior to the exchange.
    template <typename T>
    [[nodiscard]] GP_FORCEINLINE_HINT static T exchange(T* address, T desired, gp::memory::MemoryOrder order) noexcept
    {
        constexpr gp::USize kByteSize = sizeof(T);
        using StorageT = typename StorageSelector<kByteSize>::UnsignedType;

        StorageT* ptr = reinterpret_cast<StorageT*>(address);
        StorageT rawDesired = __builtin_bit_cast(StorageT, desired);
        StorageT result{};
        __atomic_exchange(ptr, &rawDesired, &result, toBuiltInOrder(order));
        return __builtin_bit_cast(T, result);
    }

    /// @brief Weak compare-and-swap operation that can fail spuriously on load-linked/store-conditional targets.
    /// @tparam T Trivially copyable value type.
    /// @param[in,out] address Memory location to compare and potentially update.
    /// @param[in,out] expected Reference containing the expected value; updated with current value on failure.
    /// @param[in] desired Value to store if comparison succeeds.
    /// @param[in] success Memory order applied when the swap succeeds.
    /// @param[in] failure Memory order applied when the comparison fails.
    /// @return True if the value was updated, false otherwise.
    template <typename T>
    [[nodiscard]] GP_FORCEINLINE_HINT static bool compareExchangeWeak(
        T* address, T& expected, T desired, gp::memory::MemoryOrder success, gp::memory::MemoryOrder failure
    ) noexcept
    {
        constexpr gp::USize kByteSize = sizeof(T);
        using StorageT = typename StorageSelector<kByteSize>::UnsignedType;

        StorageT* ptr = reinterpret_cast<StorageT*>(address);
        StorageT rawExpected = __builtin_bit_cast(StorageT, expected);
        StorageT rawDesired = __builtin_bit_cast(StorageT, desired);

        const bool matched = __atomic_compare_exchange(
            ptr, &rawExpected, &rawDesired, true, toBuiltInOrder(success), toBuiltInFailureOrder(failure)
        );

        if (!matched)
        {
            expected = __builtin_bit_cast(T, rawExpected);
        }
        return matched;
    }

    /// @brief Strong compare-and-swap operation that never fails spuriously.
    /// @tparam T Trivially copyable value type.
    /// @param[in,out] address Memory location to compare and potentially update.
    /// @param[in,out] expected Reference containing the expected value; updated with current value on failure.
    /// @param[in] desired Value to store if comparison succeeds.
    /// @param[in] success Memory order applied when the swap succeeds.
    /// @param[in] failure Memory order applied when the comparison fails.
    /// @return True if the value was updated, false otherwise.
    template <typename T>
    [[nodiscard]] GP_FORCEINLINE_HINT static bool compareExchangeStrong(
        T* address, T& expected, T desired, gp::memory::MemoryOrder success, gp::memory::MemoryOrder failure
    ) noexcept
    {
        constexpr gp::USize kByteSize = sizeof(T);
        using StorageT = typename StorageSelector<kByteSize>::UnsignedType;

        StorageT* ptr = reinterpret_cast<StorageT*>(address);
        StorageT rawExpected = __builtin_bit_cast(StorageT, expected);
        StorageT rawDesired = __builtin_bit_cast(StorageT, desired);

        const bool matched = __atomic_compare_exchange(
            ptr, &rawExpected, &rawDesired, false, toBuiltInOrder(success), toBuiltInFailureOrder(failure)
        );

        if (!matched)
        {
            expected = __builtin_bit_cast(T, rawExpected);
        }
        return matched;
    }

    /// @brief Atomically adds an argument to a memory location and returns the previous value.
    /// @tparam T Type of the target value.
    /// @tparam DeltaT Type of the value to add.
    /// @param[in,out] address Memory location to update.
    /// @param[in] delta Value to add.
    /// @param[in] order Memory ordering constraint.
    /// @return Previous value held at the address.
    template <typename T, typename DeltaT>
    [[nodiscard]] GP_FORCEINLINE_HINT static T
        fetchAdd(T* address, DeltaT delta, gp::memory::MemoryOrder order) noexcept
    {
        constexpr gp::USize kByteSize = sizeof(T);
        using StorageT = typename StorageSelector<kByteSize>::UnsignedType;

        StorageT* ptr = reinterpret_cast<StorageT*>(address);
        const StorageT rawDelta = static_cast<StorageT>(delta);
        const StorageT prev = __atomic_fetch_add(ptr, rawDelta, toBuiltInOrder(order));
        return __builtin_bit_cast(T, prev);
    }

    /// @brief Atomically subtracts an argument from a memory location and returns the previous value.
    /// @tparam T Type of the target value.
    /// @tparam DeltaT Type of the value to subtract.
    /// @param[in,out] address Memory location to update.
    /// @param[in] delta Value to subtract.
    /// @param[in] order Memory ordering constraint.
    /// @return Previous value held at the address.
    template <typename T, typename DeltaT>
    [[nodiscard]] GP_FORCEINLINE_HINT static T
        fetchSub(T* address, DeltaT delta, gp::memory::MemoryOrder order) noexcept
    {
        constexpr gp::USize kByteSize = sizeof(T);
        using StorageT = typename StorageSelector<kByteSize>::UnsignedType;

        StorageT* ptr = reinterpret_cast<StorageT*>(address);
        const StorageT rawDelta = static_cast<StorageT>(delta);
        const StorageT prev = __atomic_fetch_sub(ptr, rawDelta, toBuiltInOrder(order));
        return __builtin_bit_cast(T, prev);
    }

    /// @brief Atomically performs bitwise AND on a memory location and returns the previous value.
    /// @tparam T Integer storage type.
    /// @param[in,out] address Memory location to update.
    /// @param[in] arg Mask operand.
    /// @param[in] order Memory ordering constraint.
    /// @return Previous value held at the address.
    template <typename T>
    [[nodiscard]] GP_FORCEINLINE_HINT static T fetchAnd(T* address, T arg, gp::memory::MemoryOrder order) noexcept
    {
        constexpr gp::USize kByteSize = sizeof(T);
        using StorageT = typename StorageSelector<kByteSize>::UnsignedType;

        StorageT* ptr = reinterpret_cast<StorageT*>(address);
        const StorageT rawArg = static_cast<StorageT>(arg);
        const StorageT prev = __atomic_fetch_and(ptr, rawArg, toBuiltInOrder(order));
        return static_cast<T>(prev);
    }

    /// @brief Atomically performs bitwise OR on a memory location and returns the previous value.
    /// @tparam T Integer storage type.
    /// @param[in,out] address Memory location to update.
    /// @param[in] arg Mask operand.
    /// @param[in] order Memory ordering constraint.
    /// @return Previous value held at the address.
    template <typename T>
    [[nodiscard]] GP_FORCEINLINE_HINT static T fetchOr(T* address, T arg, gp::memory::MemoryOrder order) noexcept
    {
        constexpr gp::USize kByteSize = sizeof(T);
        using StorageT = typename StorageSelector<kByteSize>::UnsignedType;

        StorageT* ptr = reinterpret_cast<StorageT*>(address);
        const StorageT rawArg = static_cast<StorageT>(arg);
        const StorageT prev = __atomic_fetch_or(ptr, rawArg, toBuiltInOrder(order));
        return static_cast<T>(prev);
    }

    /// @brief Atomically performs bitwise XOR on a memory location and returns the previous value.
    /// @tparam T Integer storage type.
    /// @param[in,out] address Memory location to update.
    /// @param[in] arg Mask operand.
    /// @param[in] order Memory ordering constraint.
    /// @return Previous value held at the address.
    template <typename T>
    [[nodiscard]] GP_FORCEINLINE_HINT static T fetchXor(T* address, T arg, gp::memory::MemoryOrder order) noexcept
    {
        constexpr gp::USize kByteSize = sizeof(T);
        using StorageT = typename StorageSelector<kByteSize>::UnsignedType;

        StorageT* ptr = reinterpret_cast<StorageT*>(address);
        const StorageT rawArg = static_cast<StorageT>(arg);
        const StorageT prev = __atomic_fetch_xor(ptr, rawArg, toBuiltInOrder(order));
        return static_cast<T>(prev);
    }

    /// @brief Prevents the compiler from moving memory operations across this barrier.
    GP_FORCEINLINE_HINT static void compilerBarrier() noexcept
    {
        __atomic_signal_fence(__ATOMIC_SEQ_CST);
    }

    /// @brief Establishes ordering between a thread and a signal handler on the same thread.
    /// @param[in] order Memory ordering constraint to enforce.
    GP_FORCEINLINE_HINT static void signalFence(gp::memory::MemoryOrder order) noexcept
    {
        __atomic_signal_fence(toBuiltInOrder(order));
    }

    /// @brief Issues a hardware memory barrier across CPU cores.
    /// @param[in] order Memory ordering constraint to enforce.
    GP_FORCEINLINE_HINT static void threadFence(gp::memory::MemoryOrder order) noexcept
    {
        if (order == gp::memory::MemoryOrder::Relaxed) [[unlikely]]
        {
            return;
        }
        __atomic_thread_fence(toBuiltInOrder(order));
    }
};

}   // namespace gp::platform::generic
