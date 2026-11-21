// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "threading/sync/JobWaitNode.hpp"

namespace gp::thread
{

namespace detail
{

static thread_local JobFiberParker tl_activeParker{};

static GP_FORCEINLINE void compilerMemoryBarrier() noexcept
{
#if defined(__clang__) || defined(__GNUC__)
    __asm__ volatile("" ::: "memory");
#else
    _ReadWriteBarrier();
#endif
}

/// @brief Reads the calling OS hardware thread's active `JobFiberParker` slot.
/// @note Marked `GP_FORCENOINLINE` with a compiler memory barrier so optimizing compilers never hoist or cache the
///       thread's TLS base pointer across cooperative fiber context switches and core migrations ([ADR-0049],
///       [ADR-0050]).
[[nodiscard]] static GP_FORCENOINLINE JobFiberParker getThreadParkerTLS() noexcept
{
    compilerMemoryBarrier();
    return tl_activeParker;
}

/// @brief Updates the calling OS hardware thread's active `JobFiberParker` slot and returns the previous value.
/// @param[in] parker New parker configuration to bind to the calling OS thread.
/// @return Previous `JobFiberParker` bound on the calling OS thread.
[[nodiscard]] static GP_FORCENOINLINE JobFiberParker setThreadParkerTLS(const JobFiberParker& parker) noexcept
{
    compilerMemoryBarrier();
    const JobFiberParker previous = tl_activeParker;
    tl_activeParker = parker;
    compilerMemoryBarrier();
    return previous;
}

}   // namespace detail

JobFiberParker JobFiberParker::bindThreadParker(const JobFiberParker& parker) noexcept
{
    return detail::setThreadParkerTLS(parker);
}

JobFiberParker JobFiberParker::getThreadParker() noexcept
{
    return detail::getThreadParkerTLS();
}

bool JobFiberParker::hasThreadParker() noexcept
{
    return detail::getThreadParkerTLS().isValid();
}

}   // namespace gp::thread
