// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "platforms/windows/system/WindowsFutex.hpp"
#include "memory/MemoryOrder.hpp"
#include "platforms/windows/WindowsHeaderWrapper.hpp"   // IWYU pragma: keep

#pragma comment(lib, "Synchronization.lib")

namespace gp::platform::windows
{

FutexResult Futex::waitOnAddress(
    const gp::thread::Atomic<gp::UInt32>& address, const gp::UInt32 expectedValue, const gp::UInt64 timeoutNanoseconds
) noexcept
{
    if (address.load(gp::MemoryOrder::Acquire) != expectedValue)
    {
        return FutexResult::ValueMismatch;
    }

    if (timeoutNanoseconds == 0ULL)
    {
        return FutexResult::TimedOut;
    }

    ::DWORD timeoutMilliseconds = INFINITE;
    if (timeoutNanoseconds != kInfiniteFutexTimeoutNs)
    {
        const gp::UInt64 roundedMs = (timeoutNanoseconds + 999'999ULL) / 1'000'000ULL;
        if (roundedMs >= static_cast<gp::UInt64>(INFINITE - 1u))
        {
            timeoutMilliseconds = INFINITE - 1u;
        }
        else
        {
            timeoutMilliseconds = static_cast<::DWORD>(roundedMs);
        }
    }

    gp::UInt32 compareCopy = expectedValue;
    const ::BOOL waitSucceeded = ::WaitOnAddress(
        const_cast<void*>(static_cast<const volatile void*>(&address)),
        &compareCopy,
        sizeof(gp::UInt32),
        timeoutMilliseconds
    );

    if (waitSucceeded != FALSE)
    {
        return FutexResult::Success;
    }

    const ::DWORD error = ::GetLastError();
    if (error == ERROR_TIMEOUT)
    {
        return FutexResult::TimedOut;
    }

    return (address.load(gp::MemoryOrder::Acquire) != expectedValue) ? FutexResult::ValueMismatch
                                                                     : FutexResult::WaitFailed;
}

void Futex::wakeOne(const gp::thread::Atomic<gp::UInt32>& address) noexcept
{
    ::WakeByAddressSingle(const_cast<void*>(static_cast<const volatile void*>(&address)));
}

void Futex::wakeAll(const gp::thread::Atomic<gp::UInt32>& address) noexcept
{
    ::WakeByAddressAll(const_cast<void*>(static_cast<const volatile void*>(&address)));
}

}   // namespace gp::platform::windows
