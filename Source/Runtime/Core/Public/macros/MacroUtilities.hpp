// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "macros/DetectArchitecture.hpp"
#include "macros/DetectBuild.hpp"
#include "macros/DetectCompiler.hpp"
#include "macros/DetectPlatform.hpp"

/// @brief Macro to concatenate two tokens.
#define GP_INTERNAL_CONCAT_IMPL(a, b) a##b
#define GP_CONCAT(a, b) GP_INTERNAL_CONCAT_IMPL(a, b)

/// @brief Macro to stringify a token.
#define GP_INTERNAL_STRINGIFY_IMPL(x) #x
#define GP_STRINGIFY(x) GP_INTERNAL_STRINGIFY_IMPL(x)

/// @brief Macro to get the first argument or a default value if no arguments are provided.
#define GP_FIRST_OR_DEFAULT(first, ...) first

/// @brief Macro to get the first argument or an empty string if no arguments are provided.
#define GP_FIRST(...) GP_FIRST_HELPER(__VA_ARGS__, "")
#define GP_FIRST_HELPER(first, ...) first

/// @brief Expand remaining arguments (for printf-style formatting)
#define GP_EXPAND_ARGS(...) GP_EXPAND_ARGS_HELPER(__VA_ARGS__, )
#define GP_EXPAND_ARGS_HELPER(first, ...) , ##__VA_ARGS__

/// @brief Macro to get the first argument for a format string, or an empty string if no arguments are provided.
#define GP_FORMAT_MSG(...) GP_FIRST(__VA_ARGS__)

/// @brief Macro to create a version number from major, minor, and patch components.
/// @param[in] major The major version number.
/// @param[in] minor The minor version number.
/// @param[in] patch The patch version number.
#define GP_VERSION(major, minor, patch) (((major) << 16) | ((minor) << 8) | (patch))

/// @brief Macro to convert a version number into a string format "major.minor.patch".
/// @param[in] major The major version number.
/// @param[in] minor The minor version number.
/// @param[in] patch The patch version number.
#define GP_VERSION_STRINGIFY(major, minor, patch) #major "." #minor "." #patch

/// @brief Macro to force inline a function, depending on the compiler being used.
#if GP_COMPILER_MSCV
    #define GP_FORCEINLINE __forceinline
#elif GP_COMPILER_GCC || GP_COMPILER_CLANG
    #define GP_FORCEINLINE inline __attribute__((always_inline))
#else
    #define GP_FORCEINLINE inline
#endif

/// @brief Suggests the compiler to inline the decorated function, but allows it to ignore the hint.
#if GP_COMPILER_MSVC
    #define GP_INLINE __inline
#elif GP_COMPILER_GCC || GP_COMPILER_CLANG
    #define GP_INLINE inline
#else
    #define GP_INLINE inline
#endif

/// @brief Prevents the compiler from inlining the decorated function.
#if GP_COMPILER_MSVC
    #define GP_NOINLINE __declspec(noinline)
#elif GP_COMPILER_GCC || GP_COMPILER_CLANG
    #define GP_NOINLINE __attribute__((noinline))
#else
    #define GP_NOINLINE
#endif

/// @brief Asserts a pointer does not alias any other pointer in scope.
#if GP_COMPILER_MSVC
    #define GP_RESTRICT __restrict
#elif GP_COMPILER_GCC || GP_COMPILER_CLANG
    #define GP_RESTRICT __restrict__
#else
    #define GP_RESTRICT
#endif

/// @brief Macro to trigger a breakpoint in the debugger, depending on the compiler and platform being used.
#if GP_COMPILER_MSVC
    #define GP_DEBUGBREAK() __debugbreak()
#elif GP_COMPILER_GCC || GP_COMPILER_CLANG
    #if GP_PLATFORM_WINDOWS
        #define GP_DEBUGBREAK() __builtin_trap()
    #else
        #include <signal.h>
        #define GP_DEBUGBREAK() raise(SIGTRAP)
    #endif
#else
    #define GP_DEBUGBREAK() ((void)0)
#endif

/// @brief Macro to get the current function signature as a string, depending on the compiler being used.
#if GP_COMPILER_MSVC
    #define GP_FUNCSIG __FUNCSIG__
#elif GP_COMPILER_GCC || GP_COMPILER_CLANG
    #define GP_FUNCSIG __PRETTY_FUNCTION__
#else
    #define GP_FUNCSIG __func__
#endif

/// @brief Hints that the expression is likely to be true (hot path).
/// @brief Hints that the expression is likely to be false (cold path).
#if GP_INTERNAL_CXX20
    #define GP_LIKELY(x) (x) [[likely]]
    #define GP_UNLIKELY(x) (x) [[unlikely]]
#elif GP_COMPILER_GCC || GP_COMPILER_CLANG
    #define GP_LIKELY(x) __builtin_expect(!!(x), 1)
    #define GP_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
    #define GP_LIKELY(x) (x)
    #define GP_UNLIKELY(x) (x)
#endif

/// @brief Tells the compiler a code path is unreachable. Undefined behaviour if executed.
#if GP_INTERNAL_CXX23
    #include <utility>
    #define GP_UNREACHABLE() std::unreachable()
#elif GP_COMPILER_MSVC
    #define GP_UNREACHABLE() __assume(0)
#elif GP_COMPILER_GCC || GP_COMPILER_CLANG
    #define GP_UNREACHABLE() __builtin_unreachable()
#else
    #define GP_UNREACHABLE() ((void)0)
#endif

/// @brief Provides a boolean hint to the optimiser without a branch.
#if GP_COMPILER_MSVC
    #define GP_ASSUME(x) __assume(x)
#elif GP_COMPILER_CLANG
    #define GP_ASSUME(x) __builtin_assume(x)
#elif GP_COMPILER_GCC
    #define GP_ASSUME(x) do { if (!(x)) __builtin_unreachable(); } while(0)
#else
    #define GP_ASSUME(x) ((void)0)
#endif

/// @brief Macro to mark a function or variable as deprecated, with a version and message.
/// @param[in] version The framework/engine version where this was deprecated (e.g., 5.1).
/// @param[in] msg The deprecation message, typically suggesting an alternative.
#if GP_INTERNAL_CXX14
    #define GP_DEPRECATED(version, msg) [[deprecated("Deprecated in version " GP_STRINGIFY(version) ": " msg)]]
#elif GP_COMPILER_MSVC
    #define GP_DEPRECATED(version, msg) __declspec(deprecated("Deprecated in version " GP_STRINGIFY(version) ": " msg))
#elif GP_COMPILER_GCC || GP_COMPILER_CLANG
    #define GP_DEPRECATED(version, msg) __attribute__((deprecated("Deprecated in version " GP_STRINGIFY(version) ": " msg)))
#else
    #define GP_DEPRECATED(version, msg)
#endif

/// @brief Macro to mark a function's return value as "must use", with an optional message.
/// @param[in] msg An optional message to provide more context about why the return value must be used.
#if GP_INTERNAL_CXX17
    #define GP_NODISCARD [[nodiscard]]
    #define GP_NODISCARD_MSG(msg) [[nodiscard(msg)]]
#elif GP_COMPILER_MSVC && _MSC_VER >= 1700
    #define GP_NODISCARD _Check_return_
    #define GP_NODISCARD_MSG(msg) _Check_return_
#elif GP_COMPILER_GCC || GP_COMPILER_CLANG
    #define GP_NODISCARD __attribute__((warn_unused_result))
    #define GP_NODISCARD_MSG(msg) __attribute__((warn_unused_result(msg)))
#else
    #define GP_NODISCARD
    #define GP_NODISCARD_MSG(msg)
#endif

/// @brief Macro to mark a variable or function as "maybe unused", suppressing compiler warnings about unused entities.
#if GP_INTERNAL_CXX17
    #define GP_MAYBE_UNUSED [[maybe_unused]]
#elif GP_COMPILER_GCC || GP_COMPILER_CLANG
    #define GP_MAYBE_UNUSED __attribute__((unused))
#else
    #define GP_MAYBE_UNUSED
#endif

/// @brief Macro to mark a function as "noreturn", indicating that it does not return to the caller.
#if GP_INTERNAL_CXX11
    #define GP_NORETURN [[noreturn]]
#elif GP_COMPILER_MSVC
    #define GP_NORETURN __declspec(noreturn)
#elif GP_COMPILER_GCC || GP_COMPILER_CLANG
    #define GP_NORETURN __attribute__((noreturn))
#else
    #define GP_NORETURN
#endif

/// @brief Marks a struct as no unique address, allowing it to have zero size and be used for empty base optimization.
#if GP_INTERNAL_CXX20
    #define GP_NO_UNIQUE_ADDRESS [[no_unique_address]]
#elif GP_COMPILER_MSVC
    #define GP_NO_UNIQUE_ADDRESS __declspec(empty_bases)
#elif GP_COMPILER_GCC || GP_COMPILER_CLANG
    #define GP_NO_UNIQUE_ADDRESS __attribute__((no_unique_address))
#else
    #define GP_NO_UNIQUE_ADDRESS
#endif

/// @brief Specifies the minimum alignment of a variable or type in bytes.
#if GP_INTERNAL_CXX11
    #define GP_ALIGN(x) alignas(x)
#elif GP_COMPILER_MSVC
    #define GP_ALIGN(x) __declspec(align(x))
#elif GP_COMPILER_GCC || GP_COMPILER_CLANG
    #define GP_ALIGN(x) __attribute__((aligned(x)))
#else
    #define GP_ALIGN(x)
#endif

/// @brief Allocates memory with the specified alignment.  Size must be a multiple of alignment for some platforms
///        (e.g. WASM).
/// @param[in] size      The size of the memory block to allocate, in bytes.
/// @param[in] alignment The alignment requirement for the allocated memory, in bytes.
///
/// @brief Frees memory allocated with GP_ALIGNED_ALLOC.
/// @param[in] ptr A pointer to the memory block to free. This pointer must have been returned by a previous call to
///                GP_ALIGNED_ALLOC.
#if GP_COMPILER_MSVC
    #define GP_ALIGNED_ALLOC(size, alignment) _aligned_malloc(size, alignment)
    #define GP_ALIGNED_FREE(ptr) _aligned_free(ptr)
#elif GP_COMPILER_GCC || GP_COMPILER_CLANG
    #include <stdlib.h>
    #define GP_ALIGNED_ALLOC(size, alignment) aligned_alloc(alignment, size)
    #define GP_ALIGNED_FREE(ptr) free(ptr)
#else
    #define GP_ALIGNED_ALLOC(size, alignment) malloc(size)
    #define GP_ALIGNED_FREE(ptr) free(ptr)
#endif

/// @brief Macro to mark a variable or parameter as intentionally unused.
#define GP_UNUSED(x) (void)(x)

/// @brief Macro for assertions.
/// @todo Implement a custom assertion mechanism that integrates with the engine's logging system and provides more
/// context on failure. For now, this is a no-op to avoid dependencies on the logging system in core headers.
#define GP_ASSERT(expr, ...) (void)(expr)
