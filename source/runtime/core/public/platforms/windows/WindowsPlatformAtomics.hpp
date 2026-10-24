// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"                       // IWYU pragma: keep
#include "memory/MemoryOrder.hpp"                // IWYU pragma: keep
#include "threading/details/AtomicStorage.hpp"   // IWYU pragma: keep

#if !GP_COMPILER_MSVC
    #include "platforms/generic/GenericPlatformAtomics.hpp"
#else
    #include <intrin.h>
#endif

namespace gp::platform::windows
{

#if !GP_COMPILER_MSVC

/// @brief Windows platform atomic operations implementation for Clang and GCC toolchains.
/// @note Delegates to generic built-in atomic implementations.
class Atomics final : public gp::platform::generic::Atomics
{
public:
    Atomics() = delete;
    ~Atomics() = delete;
};

#else

/// @brief Windows platform atomic operations implementation using MSVC intrinsics.
class Atomics final
{
public:
    Atomics() = delete;
    ~Atomics() = delete;

private:
    template <gp::USize ByteSize>
    using StorageSelector = gp::thread::detail::AtomicStorageSelector<ByteSize>;

public:
    /// @brief Reads a value from memory with specified ordering constraints using MSVC intrinsics.
    /// @tparam T Trivially copyable value type.
    /// @param[in] address Memory location to read from.
    /// @param[in] order Memory ordering constraint.
    /// @return Current value stored at the address.
    template <typename T>
    [[nodiscard]] GP_FORCEINLINE_HINT static T load(const T* address, gp::memory::MemoryOrder order) noexcept
    {
        constexpr gp::USize kByteSize = sizeof(T);
        using UnsignedT = typename StorageSelector<kByteSize>::UnsignedType;
        using MsvcT = typename StorageSelector<kByteSize>::MsvcInterlockedType;

        const volatile MsvcT* ptr = static_cast<const volatile MsvcT*>(static_cast<const void*>(address));
        MsvcT rawValue{};

        if constexpr (kByteSize == 1u)
        {
            rawValue = __iso_volatile_load8(ptr);
        }
        else if constexpr (kByteSize == 2u)
        {
            rawValue = __iso_volatile_load16(ptr);
        }
        else if constexpr (kByteSize == 4u)
        {
            rawValue = __iso_volatile_load32(reinterpret_cast<const volatile int*>(ptr));
        }
        else if constexpr (kByteSize == 8u)
        {
            rawValue = __iso_volatile_load64(ptr);
        }

        if (order != gp::memory::MemoryOrder::Relaxed)
        {
    #if GP_ARCHITECTURE_ARM64
            __dmb(0x9);   // _ARM64_BARRIER_ISHLD
    #endif
            _ReadWriteBarrier();
        }

        const UnsignedT unsignedVal = static_cast<UnsignedT>(rawValue);
        return __builtin_bit_cast(T, unsignedVal);
    }

    /// @brief Writes a value to memory with specified ordering constraints using MSVC intrinsics.
    /// @tparam T Trivially copyable value type.
    /// @param[out] address Memory location to update.
    /// @param[in] desired Value to store.
    /// @param[in] order Memory ordering constraint.
    template <typename T>
    GP_FORCEINLINE_HINT static void store(T* address, T desired, gp::memory::MemoryOrder order) noexcept
    {
        constexpr gp::USize kByteSize = sizeof(T);
        using UnsignedT = typename StorageSelector<kByteSize>::UnsignedType;
        using MsvcT = typename StorageSelector<kByteSize>::MsvcInterlockedType;

        volatile MsvcT* ptr = static_cast<volatile MsvcT*>(static_cast<void*>(address));
        const UnsignedT rawDesiredUnsigned = __builtin_bit_cast(UnsignedT, desired);
        const MsvcT rawDesired = static_cast<MsvcT>(rawDesiredUnsigned);

    #if GP_ARCHITECTURE_X86_FAMILY
        if (order == gp::memory::MemoryOrder::SeqCst)
        {
            // On x86 and x64, XCHG drains the store buffer faster than MOV followed by MFENCE.
            if constexpr (kByteSize == 1u)
            {
                (void)_InterlockedExchange8(ptr, rawDesired);
            }
            else if constexpr (kByteSize == 2u)
            {
                (void)_InterlockedExchange16(ptr, rawDesired);
            }
            else if constexpr (kByteSize == 4u)
            {
                (void)_InterlockedExchange(ptr, rawDesired);
            }
            else if constexpr (kByteSize == 8u)
            {
                (void)_InterlockedExchange64(ptr, rawDesired);
            }
            return;
        }

        if (order != gp::memory::MemoryOrder::Relaxed)
        {
            _ReadWriteBarrier();
        }

        if constexpr (kByteSize == 1u)
        {
            __iso_volatile_store8(ptr, rawDesired);
        }
        else if constexpr (kByteSize == 2u)
        {
            __iso_volatile_store16(ptr, rawDesired);
        }
        else if constexpr (kByteSize == 4u)
        {
            __iso_volatile_store32(reinterpret_cast<volatile int*>(ptr), static_cast<int>(rawDesired));
        }
        else if constexpr (kByteSize == 8u)
        {
            __iso_volatile_store64(ptr, rawDesired);
        }
    #else
        if (order != gp::memory::MemoryOrder::Relaxed)
        {
            _ReadWriteBarrier();
            __dmb(0xB);   // _ARM64_BARRIER_ISH
        }

        if constexpr (kByteSize == 1u)
        {
            __iso_volatile_store8(ptr, rawDesired);
        }
        else if constexpr (kByteSize == 2u)
        {
            __iso_volatile_store16(ptr, rawDesired);
        }
        else if constexpr (kByteSize == 4u)
        {
            __iso_volatile_store32(reinterpret_cast<volatile int*>(ptr), static_cast<int>(rawDesired));
        }
        else if constexpr (kByteSize == 8u)
        {
            __iso_volatile_store64(ptr, rawDesired);
        }

        if (order == gp::memory::MemoryOrder::SeqCst)
        {
            __dmb(0xB);
            _ReadWriteBarrier();
        }
    #endif
    }

    /// @brief Replaces the value at a memory location and returns the previous value.
    /// @tparam T Trivially copyable value type.
    /// @param[in,out] address Memory location to modify.
    /// @param[in] desired New value to store.
    /// @param[in] order Memory ordering constraint.
    /// @return Value held at the address prior to the exchange.
    template <typename T>
    [[nodiscard]] GP_FORCEINLINE_HINT static T
        exchange(T* address, T desired, [[maybe_unused]] gp::memory::MemoryOrder order) noexcept
    {
        constexpr gp::USize kByteSize = sizeof(T);
        using UnsignedT = typename StorageSelector<kByteSize>::UnsignedType;
        using MsvcT = typename StorageSelector<kByteSize>::MsvcInterlockedType;

        volatile MsvcT* ptr = static_cast<volatile MsvcT*>(static_cast<void*>(address));
        const UnsignedT rawDesiredUnsigned = __builtin_bit_cast(UnsignedT, desired);
        const MsvcT rawDesired = static_cast<MsvcT>(rawDesiredUnsigned);
        MsvcT prev{};

        if constexpr (kByteSize == 1u)
        {
            prev = _InterlockedExchange8(ptr, rawDesired);
        }
        else if constexpr (kByteSize == 2u)
        {
            prev = _InterlockedExchange16(ptr, rawDesired);
        }
        else if constexpr (kByteSize == 4u)
        {
            prev = _InterlockedExchange(ptr, rawDesired);
        }
        else if constexpr (kByteSize == 8u)
        {
            prev = _InterlockedExchange64(ptr, rawDesired);
        }

        const UnsignedT prevUnsigned = static_cast<UnsignedT>(prev);
        return __builtin_bit_cast(T, prevUnsigned);
    }

    /// @brief Compares memory content with expected value and replaces it if matching.
    /// @tparam T Trivially copyable value type.
    /// @param[in,out] address Memory location to compare and update.
    /// @param[in,out] expected Expected value; receives current value on failure.
    /// @param[in] desired New value to store on match.
    /// @param[in] success Memory order applied on success.
    /// @param[in] failure Memory order applied on failure.
    /// @return True if swap succeeded, false otherwise.
    template <typename T>
    [[nodiscard]] GP_FORCEINLINE_HINT static bool compareExchangeWeak(
        T* address,
        T& expected,
        T desired,
        [[maybe_unused]] gp::memory::MemoryOrder success,
        [[maybe_unused]] gp::memory::MemoryOrder failure
    ) noexcept
    {
        return compareExchangeStrong(address, expected, desired, success, failure);
    }

    /// @brief Compares memory content with expected value and replaces it if matching.
    /// @tparam T Trivially copyable value type.
    /// @param[in,out] address Memory location to compare and update.
    /// @param[in,out] expected Expected value; receives current value on failure.
    /// @param[in] desired New value to store on match.
    /// @param[in] success Memory order applied on success.
    /// @param[in] failure Memory order applied on failure.
    /// @return True if swap succeeded, false otherwise.
    template <typename T>
    [[nodiscard]] GP_FORCEINLINE_HINT static bool compareExchangeStrong(
        T* address,
        T& expected,
        T desired,
        [[maybe_unused]] gp::memory::MemoryOrder success,
        [[maybe_unused]] gp::memory::MemoryOrder failure
    ) noexcept
    {
        constexpr gp::USize kByteSize = sizeof(T);
        using UnsignedT = typename StorageSelector<kByteSize>::UnsignedType;
        using MsvcT = typename StorageSelector<kByteSize>::MsvcInterlockedType;

        volatile MsvcT* ptr = static_cast<volatile MsvcT*>(static_cast<void*>(address));
        const UnsignedT rawExpectedUnsigned = __builtin_bit_cast(UnsignedT, expected);
        const UnsignedT rawDesiredUnsigned = __builtin_bit_cast(UnsignedT, desired);

        const MsvcT comparand = static_cast<MsvcT>(rawExpectedUnsigned);
        const MsvcT exchangeVal = static_cast<MsvcT>(rawDesiredUnsigned);
        MsvcT prev{};

        if constexpr (kByteSize == 1u)
        {
            prev = _InterlockedCompareExchange8(ptr, exchangeVal, comparand);
        }
        else if constexpr (kByteSize == 2u)
        {
            prev = _InterlockedCompareExchange16(ptr, exchangeVal, comparand);
        }
        else if constexpr (kByteSize == 4u)
        {
            prev = _InterlockedCompareExchange(ptr, exchangeVal, comparand);
        }
        else if constexpr (kByteSize == 8u)
        {
            prev = _InterlockedCompareExchange64(ptr, exchangeVal, comparand);
        }

        if (prev == comparand)
        {
            return true;
        }

        const UnsignedT prevUnsigned = static_cast<UnsignedT>(prev);
        expected = __builtin_bit_cast(T, prevUnsigned);
        return false;
    }

    /// @brief Atomically adds an argument to a memory location and returns the previous value.
    /// @tparam T Integer or pointer storage type.
    /// @tparam DeltaT Unsigned delta type.
    /// @param[in,out] address Memory location to update.
    /// @param[in] delta Value to add.
    /// @param[in] order Memory ordering constraint.
    /// @return Previous value held at the address.
    template <typename T, typename DeltaT>
    [[nodiscard]] GP_FORCEINLINE_HINT static T
        fetchAdd(T* address, DeltaT delta, [[maybe_unused]] gp::memory::MemoryOrder order) noexcept
    {
        constexpr gp::USize kByteSize = sizeof(T);
        using UnsignedT = typename StorageSelector<kByteSize>::UnsignedType;
        using MsvcT = typename StorageSelector<kByteSize>::MsvcInterlockedType;

        volatile MsvcT* ptr = static_cast<volatile MsvcT*>(static_cast<void*>(address));
        const MsvcT rawArg = static_cast<MsvcT>(static_cast<UnsignedT>(delta));
        MsvcT prev{};

        if constexpr (kByteSize == 1u)
        {
            prev = _InterlockedExchangeAdd8(ptr, rawArg);
        }
        else if constexpr (kByteSize == 2u)
        {
            prev = _InterlockedExchangeAdd16(ptr, rawArg);
        }
        else if constexpr (kByteSize == 4u)
        {
            prev = _InterlockedExchangeAdd(ptr, rawArg);
        }
        else if constexpr (kByteSize == 8u)
        {
            prev = _InterlockedExchangeAdd64(ptr, rawArg);
        }

        const UnsignedT prevUnsigned = static_cast<UnsignedT>(prev);
        return __builtin_bit_cast(T, prevUnsigned);
    }

    /// @brief Atomically subtracts an argument from a memory location and returns the previous value.
    /// @tparam T Integer or pointer storage type.
    /// @tparam DeltaT Unsigned delta type.
    /// @param[in,out] address Memory location to update.
    /// @param[in] delta Value to subtract.
    /// @param[in] order Memory ordering constraint.
    /// @return Previous value held at the address.
    template <typename T, typename DeltaT>
    [[nodiscard]] GP_FORCEINLINE_HINT static T
        fetchSub(T* address, DeltaT delta, [[maybe_unused]] gp::memory::MemoryOrder order) noexcept
    {
        constexpr gp::USize kByteSize = sizeof(T);
        using UnsignedT = typename StorageSelector<kByteSize>::UnsignedType;
        const UnsignedT negatedArg = static_cast<UnsignedT>(0u) - static_cast<UnsignedT>(delta);
        return fetchAdd(address, negatedArg, order);
    }

    /// @brief Atomically performs bitwise AND on a memory location and returns the previous value.
    /// @tparam T Integer storage type.
    /// @param[in,out] address Memory location to update.
    /// @param[in] arg Mask operand.
    /// @param[in] order Memory ordering constraint.
    /// @return Previous value held at the address.
    template <typename T>
    [[nodiscard]] GP_FORCEINLINE_HINT static T
        fetchAnd(T* address, T arg, [[maybe_unused]] gp::memory::MemoryOrder order) noexcept
    {
        constexpr gp::USize kByteSize = sizeof(T);
        using UnsignedT = typename StorageSelector<kByteSize>::UnsignedType;
        using MsvcT = typename StorageSelector<kByteSize>::MsvcInterlockedType;

        volatile MsvcT* ptr = static_cast<volatile MsvcT*>(static_cast<void*>(address));
        const MsvcT rawArg = static_cast<MsvcT>(static_cast<UnsignedT>(arg));
        MsvcT prev{};

        if constexpr (kByteSize == 1u)
        {
            prev = _InterlockedAnd8(ptr, rawArg);
        }
        else if constexpr (kByteSize == 2u)
        {
            prev = _InterlockedAnd16(ptr, rawArg);
        }
        else if constexpr (kByteSize == 4u)
        {
            prev = _InterlockedAnd(ptr, rawArg);
        }
        else if constexpr (kByteSize == 8u)
        {
            prev = _InterlockedAnd64(ptr, rawArg);
        }

        const UnsignedT prevUnsigned = static_cast<UnsignedT>(prev);
        return static_cast<T>(prevUnsigned);
    }

    /// @brief Atomically performs bitwise OR on a memory location and returns the previous value.
    /// @tparam T Integer storage type.
    /// @param[in,out] address Memory location to update.
    /// @param[in] arg Mask operand.
    /// @param[in] order Memory ordering constraint.
    /// @return Previous value held at the address.
    template <typename T>
    [[nodiscard]] GP_FORCEINLINE_HINT static T
        fetchOr(T* address, T arg, [[maybe_unused]] gp::memory::MemoryOrder order) noexcept
    {
        constexpr gp::USize kByteSize = sizeof(T);
        using UnsignedT = typename StorageSelector<kByteSize>::UnsignedType;
        using MsvcT = typename StorageSelector<kByteSize>::MsvcInterlockedType;

        volatile MsvcT* ptr = static_cast<volatile MsvcT*>(static_cast<void*>(address));
        const MsvcT rawArg = static_cast<MsvcT>(static_cast<UnsignedT>(arg));
        MsvcT prev{};

        if constexpr (kByteSize == 1u)
        {
            prev = _InterlockedOr8(ptr, rawArg);
        }
        else if constexpr (kByteSize == 2u)
        {
            prev = _InterlockedOr16(ptr, rawArg);
        }
        else if constexpr (kByteSize == 4u)
        {
            prev = _InterlockedOr(ptr, rawArg);
        }
        else if constexpr (kByteSize == 8u)
        {
            prev = _InterlockedOr64(ptr, rawArg);
        }

        const UnsignedT prevUnsigned = static_cast<UnsignedT>(prev);
        return static_cast<T>(prevUnsigned);
    }

    /// @brief Atomically performs bitwise XOR on a memory location and returns the previous value.
    /// @tparam T Integer storage type.
    /// @param[in,out] address Memory location to update.
    /// @param[in] arg Mask operand.
    /// @param[in] order Memory ordering constraint.
    /// @return Previous value held at the address.
    template <typename T>
    [[nodiscard]] GP_FORCEINLINE_HINT static T
        fetchXor(T* address, T arg, [[maybe_unused]] gp::memory::MemoryOrder order) noexcept
    {
        constexpr gp::USize kByteSize = sizeof(T);
        using UnsignedT = typename StorageSelector<kByteSize>::UnsignedType;
        using MsvcT = typename StorageSelector<kByteSize>::MsvcInterlockedType;

        volatile MsvcT* ptr = static_cast<volatile MsvcT*>(static_cast<void*>(address));
        const MsvcT rawArg = static_cast<MsvcT>(static_cast<UnsignedT>(arg));
        MsvcT prev{};

        if constexpr (kByteSize == 1u)
        {
            prev = _InterlockedXor8(ptr, rawArg);
        }
        else if constexpr (kByteSize == 2u)
        {
            prev = _InterlockedXor16(ptr, rawArg);
        }
        else if constexpr (kByteSize == 4u)
        {
            prev = _InterlockedXor(ptr, rawArg);
        }
        else if constexpr (kByteSize == 8u)
        {
            prev = _InterlockedXor64(ptr, rawArg);
        }

        const UnsignedT prevUnsigned = static_cast<UnsignedT>(prev);
        return static_cast<T>(prevUnsigned);
    }

    /// @brief Prevents compiler instruction reordering across this barrier.
    GP_FORCEINLINE_HINT static void compilerBarrier() noexcept
    {
        _ReadWriteBarrier();
    }

    /// @brief Establishes ordering between a thread and a signal handler executed on the same thread.
    /// @param[in] order Memory ordering constraint to enforce.
    GP_FORCEINLINE_HINT static void signalFence(gp::memory::MemoryOrder order) noexcept
    {
        if (order != gp::memory::MemoryOrder::Relaxed)
        {
            _ReadWriteBarrier();
        }
    }

    /// @brief Issues a hardware memory barrier across CPU cores.
    /// @param[in] order Memory ordering constraint to enforce.
    GP_FORCEINLINE_HINT static void threadFence(gp::memory::MemoryOrder order) noexcept
    {
        if (order == gp::memory::MemoryOrder::Relaxed) [[unlikely]]
        {
            return;
        }

    #if GP_ARCHITECTURE_ARM64
        _ReadWriteBarrier();
        if (order == gp::memory::MemoryOrder::Acquire || order == gp::memory::MemoryOrder::Consume)
        {
            __dmb(0x9);   // _ARM64_BARRIER_ISHLD
        }
        else
        {
            __dmb(0xB);   // _ARM64_BARRIER_ISH
        }
        _ReadWriteBarrier();
    #elif GP_ARCHITECTURE_X86_FAMILY
        if (order == gp::memory::MemoryOrder::SeqCst)
        {
            _ReadWriteBarrier();
            _mm_mfence();
            _ReadWriteBarrier();
        }
        else
        {
            _ReadWriteBarrier();
        }
    #else
        #error "Unsupported architecture for MSVC thread fence"
    #endif
    }
};

#endif

}   // namespace gp::platform::windows
