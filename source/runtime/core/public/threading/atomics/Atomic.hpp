// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Concepts.hpp"
#include "concepts/Fundamental.hpp"
#include "concepts/Memory.hpp"
#include "concepts/Relationship.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "memory/MemoryOrder.hpp"
#include "platforms/base/PlatformAtomics.hpp"
#include "threading/details/AtomicStorage.hpp"

namespace gp::thread
{

/// @brief Concept constraining types eligible for lock-free atomic operations.
/// @details Requires trivial copyability, absence of const or volatile qualifiers, and a size of 1, 2, 4, or 8 bytes.
template <typename T>
concept AtomicValue = gp::concepts::IsTriviallyCopyable<T> && gp::concepts::IsSameAs<T, std::remove_cv_t<T>> &&
                      (sizeof(T) == 1u || sizeof(T) == 2u || sizeof(T) == 4u || sizeof(T) == 8u);

/// @brief Hardware-accelerated, lock-free atomic primitive.
/// @details Evaluates at compile time during constant evaluation and maps to native CPU instructions at runtime.
/// @tparam T Value type satisfying the AtomicValue concept.
template <AtomicValue T>
class Atomic final
{
public:
    /// @brief Type of the value stored in the atomic variable.
    using ValueType = T;

    /// @brief Integer storage type with the same size as T.
    using StorageType = typename detail::AtomicStorageSelector<sizeof(T)>::UnsignedType;

    /// @brief Compile-time constant indicating that operations on this type never use software locks.
    static constexpr bool kIsAlwaysLockFree = true;

    /// @brief Natural alignment required to prevent split cache-line accesses.
    static constexpr gp::USize kRequiredAlignment = sizeof(T);

private:
    alignas(kRequiredAlignment) T m_value{};

public:
    /// @brief Value-initializes the atomic storage to zero.
    [[nodiscard]] constexpr Atomic() noexcept = default;

    /// @brief Constructs the atomic variable with an initial value.
    /// @param[in] initialValue Initial value to store.
    /// @note This initialization is non-atomic.
    [[nodiscard]] constexpr explicit(false) Atomic(T initialValue) noexcept
        : m_value(initialValue)
    {}

    /// @brief Deleted copy constructor preventing cross-thread state copies.
    Atomic(const Atomic&) = delete;

    /// @brief Deleted copy assignment operator preventing cross-thread state copies.
    Atomic& operator=(const Atomic&) = delete;

    /// @brief Deleted move constructor preventing cross-thread state moves.
    Atomic(Atomic&&) = delete;

    /// @brief Deleted move assignment operator preventing cross-thread state moves.
    Atomic& operator=(Atomic&&) = delete;

    /// @brief Destructor.
    constexpr ~Atomic() noexcept = default;

public:
    /// @brief Indicates whether operations on this atomic variable are lock-free.
    /// @return Always true for supported AtomicValue types.
    [[nodiscard]] GP_FORCEINLINE_HINT static constexpr bool isLockFree() noexcept
    {
        return true;
    }

    /// @brief Atomically reads the current value.
    /// @param[in] order Memory ordering constraint (cannot be Release or AcqRel).
    /// @return Current value held in the atomic variable.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr T
        load(gp::memory::MemoryOrder order = gp::memory::MemoryOrder::SeqCst) const noexcept
    {
        if consteval
        {
            return m_value;
        }
        else
        {
            return gp::platform::Atomics::load(&m_value, order);
        }
    }

    /// @brief Atomically replaces the current value with a new one.
    /// @param[in] desired Value to write.
    /// @param[in] order Memory ordering constraint (cannot be Acquire, Consume, or AcqRel).
    GP_FORCEINLINE_HINT constexpr void
        store(T desired, gp::memory::MemoryOrder order = gp::memory::MemoryOrder::SeqCst) noexcept
    {
        if consteval
        {
            m_value = desired;
        }
        else
        {
            gp::platform::Atomics::store(&m_value, desired, order);
        }
    }

    /// @brief Atomically stores a new value and returns the previous one.
    /// @param[in] desired Value to store.
    /// @param[in] order Memory ordering constraint.
    /// @return Value held immediately before this operation.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr T
        exchange(T desired, gp::memory::MemoryOrder order = gp::memory::MemoryOrder::SeqCst) noexcept
    {
        if consteval
        {
            const T old = m_value;
            m_value = desired;
            return old;
        }
        else
        {
            return gp::platform::Atomics::exchange(&m_value, desired, order);
        }
    }

    /// @brief Atomically compares memory with an expected value and updates it on match.
    /// @param[in,out] expected Reference containing the expected value; updated on failure.
    /// @param[in] desired Value to store if values match.
    /// @param[in] success Memory ordering constraint if comparison succeeds.
    /// @param[in] failure Memory ordering constraint if comparison fails.
    /// @return True if swap succeeded, false otherwise.
    /// @note May fail spuriously on load-linked/store-conditional architectures such as ARM.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool compareExchangeWeak(
        T& expected, T desired, gp::memory::MemoryOrder success, gp::memory::MemoryOrder failure
    ) noexcept
    {
        if consteval
        {
            return compareExchangeConsteval(expected, desired);
        }
        else
        {
            return gp::platform::Atomics::compareExchangeWeak(&m_value, expected, desired, success, failure);
        }
    }

    /// @brief Atomically compares memory with an expected value using the same order for success and failure.
    /// @param[in,out] expected Reference containing the expected value; updated on failure.
    /// @param[in] desired Value to store if values match.
    /// @param[in] order Memory ordering constraint.
    /// @return True if swap succeeded, false otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool compareExchangeWeak(
        T& expected, T desired, gp::memory::MemoryOrder order = gp::memory::MemoryOrder::SeqCst
    ) noexcept
    {
        return compareExchangeWeak(expected, desired, order, order);
    }

    /// @brief Atomically compares memory with an expected value and updates it on match without spurious failure.
    /// @param[in,out] expected Reference containing the expected value; updated on failure.
    /// @param[in] desired Value to store if values match.
    /// @param[in] success Memory ordering constraint if comparison succeeds.
    /// @param[in] failure Memory ordering constraint if comparison fails.
    /// @return True if swap succeeded, false otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool compareExchangeStrong(
        T& expected, T desired, gp::memory::MemoryOrder success, gp::memory::MemoryOrder failure
    ) noexcept
    {
        if consteval
        {
            return compareExchangeConsteval(expected, desired);
        }
        else
        {
            return gp::platform::Atomics::compareExchangeStrong(&m_value, expected, desired, success, failure);
        }
    }

    /// @brief Atomically compares memory with an expected value using the same order for success and failure.
    /// @param[in,out] expected Reference containing the expected value; updated on failure.
    /// @param[in] desired Value to store if values match.
    /// @param[in] order Memory ordering constraint.
    /// @return True if swap succeeded, false otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool compareExchangeStrong(
        T& expected, T desired, gp::memory::MemoryOrder order = gp::memory::MemoryOrder::SeqCst
    ) noexcept
    {
        return compareExchangeStrong(expected, desired, order, order);
    }

public:
    /// @brief Atomically adds an argument to an integer atomic and returns the previous value.
    /// @param[in] arg Value to add.
    /// @param[in] order Memory ordering constraint.
    /// @return Value held immediately before addition.
    GP_FORCEINLINE_HINT constexpr T
        fetchAdd(T arg, gp::memory::MemoryOrder order = gp::memory::MemoryOrder::SeqCst) noexcept
        requires(concepts::IsIntegral<T> && !concepts::IsBool<T>)
    {
        if consteval
        {
            const T old = m_value;
            m_value = static_cast<T>(static_cast<StorageType>(m_value) + static_cast<StorageType>(arg));
            return old;
        }
        else
        {
            return gp::platform::Atomics::fetchAdd(&m_value, arg, order);
        }
    }

    /// @brief Atomically subtracts an argument from an integer atomic and returns the previous value.
    /// @param[in] arg Value to subtract.
    /// @param[in] order Memory ordering constraint.
    /// @return Value held immediately before subtraction.
    GP_FORCEINLINE_HINT constexpr T
        fetchSub(T arg, gp::memory::MemoryOrder order = gp::memory::MemoryOrder::SeqCst) noexcept
        requires(concepts::IsIntegral<T> && !concepts::IsBool<T>)
    {
        if consteval
        {
            const T old = m_value;
            m_value = static_cast<T>(static_cast<StorageType>(m_value) - static_cast<StorageType>(arg));
            return old;
        }
        else
        {
            return gp::platform::Atomics::fetchSub(&m_value, arg, order);
        }
    }

    /// @brief Atomically performs bitwise AND on the atomic variable and returns the previous value.
    /// @param[in] arg Bitmask to apply.
    /// @param[in] order Memory ordering constraint.
    /// @return Value held immediately before bitwise AND.
    GP_FORCEINLINE_HINT constexpr T
        fetchAnd(T arg, gp::memory::MemoryOrder order = gp::memory::MemoryOrder::SeqCst) noexcept
        requires((concepts::IsIntegral<T> && !concepts::IsBool<T>) || concepts::IsEnum<T>)
    {
        if consteval
        {
            const T old = m_value;
            m_value = static_cast<T>(static_cast<StorageType>(m_value) & static_cast<StorageType>(arg));
            return old;
        }
        else
        {
            auto* rawPtr = reinterpret_cast<StorageType*>(&m_value);
            const StorageType prev = gp::platform::Atomics::fetchAnd(rawPtr, static_cast<StorageType>(arg), order);
            return static_cast<T>(prev);
        }
    }

    /// @brief Atomically performs bitwise OR on the atomic variable and returns the previous value.
    /// @param[in] arg Bitmask to apply.
    /// @param[in] order Memory ordering constraint.
    /// @return Value held immediately before bitwise OR.
    GP_FORCEINLINE_HINT constexpr T
        fetchOr(T arg, gp::memory::MemoryOrder order = gp::memory::MemoryOrder::SeqCst) noexcept
        requires((concepts::IsIntegral<T> && !concepts::IsBool<T>) || concepts::IsEnum<T>)
    {
        if consteval
        {
            const T old = m_value;
            m_value = static_cast<T>(static_cast<StorageType>(m_value) | static_cast<StorageType>(arg));
            return old;
        }
        else
        {
            auto* rawPtr = reinterpret_cast<StorageType*>(&m_value);
            const StorageType prev = gp::platform::Atomics::fetchOr(rawPtr, static_cast<StorageType>(arg), order);
            return static_cast<T>(prev);
        }
    }

    /// @brief Atomically performs bitwise XOR on the atomic variable and returns the previous value.
    /// @param[in] arg Bitmask to apply.
    /// @param[in] order Memory ordering constraint.
    /// @return Value held immediately before bitwise XOR.
    GP_FORCEINLINE_HINT constexpr T
        fetchXor(T arg, gp::memory::MemoryOrder order = gp::memory::MemoryOrder::SeqCst) noexcept
        requires((concepts::IsIntegral<T> && !concepts::IsBool<T>) || concepts::IsEnum<T>)
    {
        if consteval
        {
            const T old = m_value;
            m_value = static_cast<T>(static_cast<StorageType>(m_value) ^ static_cast<StorageType>(arg));
            return old;
        }
        else
        {
            auto* rawPtr = reinterpret_cast<StorageType*>(&m_value);
            const StorageType prev = gp::platform::Atomics::fetchXor(rawPtr, static_cast<StorageType>(arg), order);
            return static_cast<T>(prev);
        }
    }

    /// @brief Atomically sets a bit at the given index and returns its previous state.
    /// @param[in] bitIndex Zero-based bit position to set.
    /// @param[in] order Memory ordering constraint.
    /// @return True if the bit was previously set, false if it was zero.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool
        fetchBitSet(gp::UInt32 bitIndex, gp::memory::MemoryOrder order = gp::memory::MemoryOrder::SeqCst) noexcept
        requires(concepts::IsIntegral<T> && !concepts::IsBool<T>)
    {
        GP_ASSERT(bitIndex < sizeof(T) * 8u, "bitIndex out of range");
        const StorageType mask = static_cast<StorageType>(1u) << bitIndex;
        const StorageType prev = static_cast<StorageType>(fetchOr(static_cast<T>(mask), order));
        return (prev & mask) != 0u;
    }

    /// @brief Atomically clears a bit at the given index and returns its previous state.
    /// @param[in] bitIndex Zero-based bit position to clear.
    /// @param[in] order Memory ordering constraint.
    /// @return True if the bit was previously set, false if it was zero.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool
        fetchBitClear(gp::UInt32 bitIndex, gp::memory::MemoryOrder order = gp::memory::MemoryOrder::SeqCst) noexcept
        requires(concepts::IsIntegral<T> && !concepts::IsBool<T>)
    {
        GP_ASSERT(bitIndex < sizeof(T) * 8u, "bitIndex out of range");
        const StorageType mask = static_cast<StorageType>(1u) << bitIndex;
        const StorageType prev = static_cast<StorageType>(fetchAnd(static_cast<T>(~mask), order));
        return (prev & mask) != 0u;
    }

public:
    /// @brief Atomically advances the pointer by an element count and returns the previous address.
    /// @param[in] deltaElements Number of elements to step forward.
    /// @param[in] order Memory ordering constraint.
    /// @return Pointer address held immediately before addition.
    GP_FORCEINLINE_HINT constexpr T
        fetchAdd(gp::IntPtr deltaElements, gp::memory::MemoryOrder order = gp::memory::MemoryOrder::SeqCst) noexcept
        requires(concepts::IsPointer<T> && requires { sizeof(std::remove_pointer_t<T>); })
    {
        using PointeeT = std::remove_pointer_t<T>;
        if consteval
        {
            const T old = m_value;
            m_value += deltaElements;
            return old;
        }
        else
        {
            const StorageType byteDelta =
                static_cast<StorageType>(deltaElements) * static_cast<StorageType>(sizeof(PointeeT));
            return gp::platform::Atomics::fetchAdd(&m_value, byteDelta, order);
        }
    }

    /// @brief Atomically moves the pointer backward by an element count and returns the previous address.
    /// @param[in] deltaElements Number of elements to step backward.
    /// @param[in] order Memory ordering constraint.
    /// @return Pointer address held immediately before subtraction.
    GP_FORCEINLINE_HINT constexpr T
        fetchSub(gp::IntPtr deltaElements, gp::memory::MemoryOrder order = gp::memory::MemoryOrder::SeqCst) noexcept
        requires(concepts::IsPointer<T> && requires { sizeof(std::remove_pointer_t<T>); })
    {
        using PointeeT = std::remove_pointer_t<T>;
        if consteval
        {
            const T old = m_value;
            m_value -= deltaElements;
            return old;
        }
        else
        {
            const StorageType byteDelta =
                static_cast<StorageType>(deltaElements) * static_cast<StorageType>(sizeof(PointeeT));
            return gp::platform::Atomics::fetchSub(&m_value, byteDelta, order);
        }
    }

public:
    /// @brief Atomically adds an argument to a floating-point atomic and returns the previous value.
    /// @param[in] arg Value to add.
    /// @param[in] order Memory ordering constraint.
    /// @return Value held immediately before addition.
    GP_FORCEINLINE_HINT constexpr T
        fetchAdd(T arg, gp::memory::MemoryOrder order = gp::memory::MemoryOrder::SeqCst) noexcept
        requires concepts::IsFloatingPoint<T>
    {
        T expected = load(gp::memory::MemoryOrder::Relaxed);
        while (!compareExchangeWeak(expected, expected + arg, order, gp::memory::MemoryOrder::Relaxed))
        {
        }
        return expected;
    }

    /// @brief Atomically subtracts an argument from a floating-point atomic and returns the previous value.
    /// @param[in] arg Value to subtract.
    /// @param[in] order Memory ordering constraint.
    /// @return Value held immediately before subtraction.
    GP_FORCEINLINE_HINT constexpr T
        fetchSub(T arg, gp::memory::MemoryOrder order = gp::memory::MemoryOrder::SeqCst) noexcept
        requires concepts::IsFloatingPoint<T>
    {
        T expected = load(gp::memory::MemoryOrder::Relaxed);
        while (!compareExchangeWeak(expected, expected - arg, order, gp::memory::MemoryOrder::Relaxed))
        {
        }
        return expected;
    }

public:
    /// @brief Assigns a new value using sequentially consistent ordering.
    /// @param[in] desired Value to store.
    /// @return Value that was assigned.
    GP_FORCEINLINE_HINT constexpr T operator=(T desired) noexcept
    {
        store(desired, gp::memory::MemoryOrder::SeqCst);
        return desired;
    }

    /// @brief Implicitly loads the current value using sequentially consistent ordering.
    /// @return Current value held in the atomic variable.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr operator T() const noexcept
    {
        return load(gp::memory::MemoryOrder::SeqCst);
    }

    /// @brief Pre-increments the value and returns the result.
    /// @return Value after increment.
    GP_FORCEINLINE_HINT constexpr T operator++() noexcept
        requires((concepts::IsIntegral<T> && !concepts::IsBool<T>) || concepts::IsPointer<T>)
    {
        return static_cast<T>(fetchAdd(1) + 1);
    }

    /// @brief Post-increments the value and returns the previous value.
    /// @return Value before increment.
    GP_FORCEINLINE_HINT constexpr T operator++(int) noexcept
        requires((concepts::IsIntegral<T> && !concepts::IsBool<T>) || concepts::IsPointer<T>)
    {
        return fetchAdd(1);
    }

    /// @brief Pre-decrements the value and returns the result.
    /// @return Value after decrement.
    GP_FORCEINLINE_HINT constexpr T operator--() noexcept
        requires((concepts::IsIntegral<T> && !concepts::IsBool<T>) || concepts::IsPointer<T>)
    {
        return static_cast<T>(fetchSub(1) - 1);
    }

    /// @brief Post-decrements the value and returns the previous value.
    /// @return Value before decrement.
    GP_FORCEINLINE_HINT constexpr T operator--(int) noexcept
        requires((concepts::IsIntegral<T> && !concepts::IsBool<T>) || concepts::IsPointer<T>)
    {
        return fetchSub(1);
    }

    /// @brief Adds an argument to the atomic value.
    /// @param[in] arg Value to add.
    /// @return Value after addition.
    GP_FORCEINLINE_HINT constexpr T operator+=(T arg) noexcept
        requires((concepts::IsIntegral<T> && !concepts::IsBool<T>) || concepts::IsFloatingPoint<T>)
    {
        return static_cast<T>(fetchAdd(arg) + arg);
    }

    /// @brief Adds an element count to a pointer atomic value.
    /// @param[in] deltaElements Number of elements to step forward.
    /// @return Pointer address after addition.
    GP_FORCEINLINE_HINT constexpr T operator+=(gp::IntPtr deltaElements) noexcept requires concepts::IsPointer<T>
    {
        return fetchAdd(deltaElements) + deltaElements;
    }

    /// @brief Subtracts an argument from the atomic value.
    /// @param[in] arg Value to subtract.
    /// @return Value after subtraction.
    GP_FORCEINLINE_HINT constexpr T operator-=(T arg) noexcept
        requires((concepts::IsIntegral<T> && !concepts::IsBool<T>) || concepts::IsFloatingPoint<T>)
    {
        return static_cast<T>(fetchSub(arg) - arg);
    }

    /// @brief Subtracts an element count from a pointer atomic value.
    /// @param[in] deltaElements Number of elements to step backward.
    /// @return Pointer address after subtraction.
    GP_FORCEINLINE_HINT constexpr T operator-=(gp::IntPtr deltaElements) noexcept requires concepts::IsPointer<T>
    {
        return fetchSub(deltaElements) - deltaElements;
    }

    /// @brief Performs bitwise AND with an argument.
    /// @param[in] arg Bitmask to apply.
    /// @return Value after bitwise AND.
    GP_FORCEINLINE_HINT constexpr T operator&=(T arg) noexcept
        requires((concepts::IsIntegral<T> && !concepts::IsBool<T>) || concepts::IsEnum<T>)
    {
        return static_cast<T>(static_cast<StorageType>(fetchAnd(arg)) & static_cast<StorageType>(arg));
    }

    /// @brief Performs bitwise OR with an argument.
    /// @param[in] arg Bitmask to apply.
    /// @return Value after bitwise OR.
    GP_FORCEINLINE_HINT constexpr T operator|=(T arg) noexcept
        requires((concepts::IsIntegral<T> && !concepts::IsBool<T>) || concepts::IsEnum<T>)
    {
        return static_cast<T>(static_cast<StorageType>(fetchOr(arg)) | static_cast<StorageType>(arg));
    }

    /// @brief Performs bitwise XOR with an argument.
    /// @param[in] arg Bitmask to apply.
    /// @return Value after bitwise XOR.
    GP_FORCEINLINE_HINT constexpr T operator^=(T arg) noexcept
        requires((concepts::IsIntegral<T> && !concepts::IsBool<T>) || concepts::IsEnum<T>)
    {
        return static_cast<T>(static_cast<StorageType>(fetchXor(arg)) ^ static_cast<StorageType>(arg));
    }

private:
    /// @brief Helper for constant evaluation compare-and-swap comparing exact bit representation.
    /// @param[in,out] expected Expected value; receives current value on failure.
    /// @param[in] desired Value to store on match.
    /// @return True if swap succeeded, false otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool compareExchangeConsteval(T& expected, T desired) noexcept
    {
        if constexpr (concepts::IsPointer<T>)
        {
            if (m_value == expected)
            {
                m_value = desired;
                return true;
            }
            expected = m_value;
            return false;
        }
        else
        {
            const StorageType curBits = __builtin_bit_cast(StorageType, m_value);
            const StorageType expBits = __builtin_bit_cast(StorageType, expected);
            if (curBits == expBits)
            {
                m_value = desired;
                return true;
            }
            expected = m_value;
            return false;
        }
    }
};

}   // namespace gp::thread
