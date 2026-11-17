// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "diagnostics/Assert.hpp"   // IWYU pragma: keep
#include "miscellaneous/BuildDefines.hpp"
#include "platforms/base/Platform.hpp"

#ifdef GP_ASSERT
    #undef GP_ASSERT
#endif

/// @section Fatal Check & Legacy Assert Macros (Gated by GP_DO_CHECK)

#if GP_DO_CHECK

    /// @brief Verifies a runtime invariant and terminates if @p expr evaluates to false.
    /// @details Compiles out completely with zero argument evaluation when `GP_DO_CHECK` is disabled.
    /// @param[in] expr Boolean expression expected to hold true.
    #define GP_CHECK(expr)                                                                         \
        do                                                                                         \
        {                                                                                          \
            if (!(expr)) [[unlikely]]                                                              \
            {                                                                                      \
                ::gp::diagnostics::reportCheckFailure(                                             \
                    ::gp::diagnostics::AssertKind::Check, #expr, nullptr, __FILE__, __LINE__, __func__ \
                );                                                                                 \
            }                                                                                      \
        } while (false)

    /// @brief Verifies a runtime invariant with a printf-style message if @p expr evaluates to false.
    /// @param[in] expr Boolean expression expected to hold true.
    /// @param[in] format Printf-style message format string.
    #define GP_CHECKF(expr, format, ...)                                                                        \
        do                                                                                                      \
        {                                                                                                       \
            if (!(expr)) [[unlikely]]                                                                           \
            {                                                                                                   \
                ::gp::diagnostics::reportCheckFailureFormatted(                                                 \
                    ::gp::diagnostics::AssertKind::Check, #expr, __FILE__, __LINE__, __func__,                  \
                    format __VA_OPT__(, ) __VA_ARGS__                                                           \
                );                                                                                              \
            }                                                                                                   \
        } while (false)

    /// @brief Legacy core assertion macro accepting an expression alone or an expression with a message/format.
    /// @param[in] expr Boolean expression expected to hold true.
    #define GP_ASSERT(expr, ...)                                                                         \
        do                                                                                               \
        {                                                                                                \
            if (!(expr)) [[unlikely]]                                                                    \
            {                                                                                            \
                ::gp::diagnostics::detail::dispatchAssertFailure(                                        \
                    ::gp::diagnostics::AssertKind::Check, #expr, __FILE__, __LINE__,                     \
                    __func__ __VA_OPT__(, __VA_ARGS__)                                                   \
                );                                                                                       \
            }                                                                                            \
        } while (false)

    /// @brief Marks a branch as logically unreachable; triggers an assertion failure if execution enters it.
    #define GP_CHECK_NO_ENTRY()                                                                            \
        do                                                                                                 \
        {                                                                                                  \
            ::gp::diagnostics::reportCheckFailure(                                                         \
                ::gp::diagnostics::AssertKind::NoEntry,                                                    \
                "Unreachable code path entered",                                                           \
                nullptr,                                                                                   \
                __FILE__,                                                                                  \
                __LINE__,                                                                                  \
                __func__                                                                                   \
            );                                                                                             \
        } while (false)

    /// @brief Marks a stub function or missing code path as unimplemented; triggers an assertion failure on entry.
    #define GP_UNIMPLEMENTED()                                                                             \
        do                                                                                                 \
        {                                                                                                  \
            ::gp::diagnostics::reportCheckFailure(                                                         \
                ::gp::diagnostics::AssertKind::Unimplemented,                                              \
                "Unimplemented code path called",                                                          \
                nullptr,                                                                                   \
                __FILE__,                                                                                  \
                __LINE__,                                                                                  \
                __func__                                                                                   \
            );                                                                                             \
        } while (false)

    /// @brief Verifies @p expr, preserving side-effect evaluation even when `GP_DO_CHECK` is disabled.
    /// @param[in] expr Expression whose side effects must always run.
    #define GP_VERIFY(expr)                                                                                 \
        do                                                                                                  \
        {                                                                                                   \
            if (!(expr)) [[unlikely]]                                                                       \
            {                                                                                               \
                ::gp::diagnostics::reportCheckFailure(                                                      \
                    ::gp::diagnostics::AssertKind::Verify, #expr, nullptr, __FILE__, __LINE__, __func__     \
                );                                                                                          \
            }                                                                                               \
        } while (false)

    /// @brief Formatted variant of `GP_VERIFY`, preserving side-effect evaluation in all build configurations.
    /// @param[in] expr Expression whose side effects must always run.
    /// @param[in] format Printf-style message format string.
    #define GP_VERIFYF(expr, format, ...)                                                                       \
        do                                                                                                      \
        {                                                                                                       \
            if (!(expr)) [[unlikely]]                                                                           \
            {                                                                                                   \
                ::gp::diagnostics::reportCheckFailureFormatted(                                                 \
                    ::gp::diagnostics::AssertKind::Verify, #expr, __FILE__, __LINE__, __func__,                 \
                    format __VA_OPT__(, ) __VA_ARGS__                                                           \
                );                                                                                              \
            }                                                                                                   \
        } while (false)

#else

    #define GP_CHECK(expr) \
        do                 \
        {                  \
        } while (false)

    #define GP_CHECKF(expr, format, ...) \
        do                               \
        {                                \
        } while (false)

    #define GP_ASSERT(expr, ...) \
        do                       \
        {                        \
        } while (false)

    #define GP_CHECK_NO_ENTRY() GP_BUILTIN_UNREACHABLE()

    #define GP_UNIMPLEMENTED() \
        do                     \
        {                      \
        } while (false)

    #define GP_VERIFY(expr) \
        do                  \
        {                   \
            if (expr)       \
            {               \
            }               \
        } while (false)

    #define GP_VERIFYF(expr, format, ...) \
        do                                \
        {                                 \
            if (expr)                     \
            {                             \
            }                             \
        } while (false)

#endif   // GP_DO_CHECK

/// @section Slow Debug-Only Checks (Gated by GP_DO_GUARD_SLOW)

#if GP_DO_GUARD_SLOW

    /// @brief Expensive invariant check compiled out in Development, Profile, and Shipping builds.
    /// @param[in] expr Boolean expression expected to hold true.
    #define GP_CHECK_SLOW(expr)                                                                                 \
        do                                                                                                      \
        {                                                                                                       \
            if (!(expr)) [[unlikely]]                                                                           \
            {                                                                                                   \
                ::gp::diagnostics::reportCheckFailure(                                                          \
                    ::gp::diagnostics::AssertKind::CheckSlow, #expr, nullptr, __FILE__, __LINE__, __func__      \
                );                                                                                              \
            }                                                                                                   \
        } while (false)

    /// @brief Formatted expensive invariant check active only when `GP_DO_GUARD_SLOW` is enabled.
    /// @param[in] expr Boolean expression expected to hold true.
    /// @param[in] format Printf-style message format string.
    #define GP_CHECKF_SLOW(expr, format, ...)                                                                   \
        do                                                                                                      \
        {                                                                                                       \
            if (!(expr)) [[unlikely]]                                                                           \
            {                                                                                                   \
                ::gp::diagnostics::reportCheckFailureFormatted(                                                 \
                    ::gp::diagnostics::AssertKind::CheckSlow, #expr, __FILE__, __LINE__, __func__,              \
                    format __VA_OPT__(, ) __VA_ARGS__                                                           \
                );                                                                                              \
            }                                                                                                   \
        } while (false)

#else

    #define GP_CHECK_SLOW(expr) \
        do                      \
        {                       \
        } while (false)

    #define GP_CHECKF_SLOW(expr, format, ...) \
        do                                    \
        {                                     \
        } while (false)

#endif   // GP_DO_GUARD_SLOW

/// @section Non-Fatal First-Hit Ensure Macros (Gated by GP_DO_ENSURE)

#if GP_DO_ENSURE

    /// @brief Non-fatal verification usable as a `bool` condition; reports only on the first failure at each site.
    /// @details Evaluates to `true` when @p expr holds. On failure, claims a call-site atomic byte and invokes
    ///          `reportEnsureFailure` once without aborting execution.
    /// @param[in] expr Boolean expression to test.
    /// @return `true` if @p expr succeeded, `false` if it failed.
    #define GP_ENSURE(expr)                                                               \
        ((!!(expr)) ||                                                                    \
         ([&](const char* const _gpFuncName) noexcept -> bool                             \
          {                                                                               \
              static ::gp::UInt8 _gpEnsureTriggered = 0u;                                 \
              ::gp::diagnostics::reportEnsureFailure(                                     \
                  _gpEnsureTriggered, #expr, nullptr, __FILE__, __LINE__, _gpFuncName     \
              );                                                                          \
              return false;                                                               \
          }(__func__)))

    /// @brief Formatted variant of `GP_ENSURE` returning `bool` and reporting only on the first failure at each site.
    /// @param[in] expr Boolean expression to test.
    /// @param[in] format Printf-style message format string.
    /// @return `true` if @p expr succeeded, `false` if it failed.
    #define GP_ENSURE_MSG(expr, format, ...)                                              \
        ((!!(expr)) ||                                                                    \
         ([&](const char* const _gpFuncName) noexcept -> bool                             \
          {                                                                               \
              static ::gp::UInt8 _gpEnsureTriggered = 0u;                                 \
              ::gp::diagnostics::reportEnsureFailureFormatted(                            \
                  _gpEnsureTriggered, #expr, __FILE__, __LINE__, _gpFuncName,             \
                  format __VA_OPT__(, ) __VA_ARGS__                                       \
              );                                                                          \
              return false;                                                               \
          }(__func__)))

#else

    #define GP_ENSURE(expr) (!!(expr))

    #define GP_ENSURE_MSG(expr, format, ...) (!!(expr))

#endif   // GP_DO_ENSURE
