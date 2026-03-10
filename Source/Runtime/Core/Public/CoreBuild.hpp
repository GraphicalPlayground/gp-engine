// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

// Platform detection
#if defined(_WIN32) || defined(_WIN64)
    #define GP_PLATFORM_WINDOWS 1
#elif defined(__APPLE__) && defined(__MACH__)
    #include <TargetConditionals.h>
    #if defined(TARGET_IPHONE_SIMULATOR) || defined(TARGET_OS_IPHONE)
        #define GP_PLATFORM_IOS 1
    #else
        #define GP_PLATFORM_MACOS 1
    #endif
#elif defined(__linux__)
    #define GP_PLATFORM_LINUX 1
#elif defined(__ANDROID__)
    #define GP_PLATFORM_ANDROID 1
#elif defined(__EMSCRIPTEN__)
    #define GP_PLATFORM_WEB 1
#else
    #error "[GP] Unsupported platform."
#endif

#ifndef GP_PLATFORM_WINDOWS
    #define GP_PLATFORM_WINDOWS 0
#endif
#ifndef GP_PLATFORM_LINUX
    #define GP_PLATFORM_LINUX 0
#endif
#ifndef GP_PLATFORM_MACOS
    #define GP_PLATFORM_MACOS 0
#endif
#ifndef GP_PLATFORM_IOS
    #define GP_PLATFORM_IOS 0
#endif
#ifndef GP_PLATFORM_ANDROID
    #define GP_PLATFORM_ANDROID 0
#endif
#ifndef GP_PLATFORM_WEB
    #define GP_PLATFORM_WEB 0
#endif

// Windows-specific macros
#if GP_PLATFORM_WINDOWS
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
#endif

// Platform families
#define GP_PLATFORM_DESKTOP (GP_PLATFORM_WINDOWS || GP_PLATFORM_LINUX || GP_PLATFORM_MACOS)
#define GP_PLATFORM_MOBILE (GP_PLATFORM_IOS || GP_PLATFORM_ANDROID)
#define GP_PLATFORM_APPLE (GP_PLATFORM_MACOS || GP_PLATFORM_IOS)
#define GP_PLATFORM_UNIX (GP_PLATFORM_LINUX || GP_PLATFORM_MACOS || GP_PLATFORM_IOS || GP_PLATFORM_ANDROID)

// Architecture detection
#if defined(_M_X64) || defined(__x86_64__)
    #define GP_ARCHITECTURE_X64 1
#elif defined(_M_IX86) || defined(__i386__)
    #define GP_ARCHITECTURE_X86 1
#elif defined(_M_ARM64) || defined(__aarch64__) || defined(_M_ARM64EC)
    #define GP_ARCHITECTURE_ARM64 1
#elif defined(_M_ARM) || defined(__arm__) || defined(__ARM_NEON)
    #define GP_ARCHITECTURE_ARM32 1
#elif defined(__EMSCRIPTEN__)
    #define GP_ARCHITECTURE_WASM 1
#else
    #error "[GP] Unsupported architecture."
#endif

#ifndef GP_ARCHITECTURE_X86
    #define GP_ARCHITECTURE_X86 0
#endif
#ifndef GP_ARCHITECTURE_X64
    #define GP_ARCHITECTURE_X64 0
#endif
#ifndef GP_ARCHITECTURE_ARM32
    #define GP_ARCHITECTURE_ARM32 0
#endif
#ifndef GP_ARCHITECTURE_ARM64
    #define GP_ARCHITECTURE_ARM64 0
#endif
#ifndef GP_ARCHITECTURE_WASM
    #define GP_ARCHITECTURE_WASM 0
#endif

// Architecture families
#define GP_ARCHITECTURE_X86_FAMILY (GP_ARCHITECTURE_X86 || GP_ARCHITECTURE_X64)
#define GP_ARCHITECTURE_ARM_FAMILY (GP_ARCHITECTURE_ARM32 || GP_ARCHITECTURE_ARM64)
#define GP_ARCHITECTURE_64BIT (GP_ARCHITECTURE_X64 || GP_ARCHITECTURE_ARM64)
#define GP_ARCHITECTURE_32BIT (GP_ARCHITECTURE_X86 || GP_ARCHITECTURE_ARM32)

// Compiler detection
#if defined(_MSC_VER)
    #define GP_COMPILER_MSVC 1
    #define GP_COMPILER_VERSION _MSC_VER
#elif defined(__clang__)
    #define GP_COMPILER_CLANG 1
    #define GP_COMPILER_VERSION (__clang_major__ * 10000 + __clang_minor__ * 100 + __clang_patchlevel__)
#elif defined(__GNUC__)
    #define GP_COMPILER_GCC 1
    #define GP_COMPILER_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#elif defined(__EMSCRIPTEN__)
    #define GP_COMPILER_EMSCRIPTEN 1
    #define GP_COMPILER_VERSION __EMSCRIPTEN__
#elif defined(__INTEL_COMPILER) || defined(__ICC)
    #define GP_COMPILER_INTEL 1
    #define GP_COMPILER_VERSION __INTEL_COMPILER
#else
    #error "[GP] Unsupported compiler."
#endif

#ifndef GP_COMPILER_MSVC
    #define GP_COMPILER_MSVC 0
#endif
#ifndef GP_COMPILER_CLANG
    #define GP_COMPILER_CLANG 0
#endif
#ifndef GP_COMPILER_GCC
    #define GP_COMPILER_GCC 0
#endif
#ifndef GP_COMPILER_EMSCRIPTEN
    #define GP_COMPILER_EMSCRIPTEN 0
#endif
#ifndef GP_COMPILER_INTEL
    #define GP_COMPILER_INTEL 0
#endif

//  C++ standard detection
#if defined(_MSVC_LANG)
    #define GP_CXX_STANDARD _MSVC_LANG
#elif defined(__cplusplus)
    #define GP_CXX_STANDARD __cplusplus
#else
    #error "[GP] Cannot determine C++ standard version"
#endif

#define GP_INTERNAL_CXX11 (GP_CXX_STANDARD >= 201103L)
#define GP_INTERNAL_CXX14 (GP_CXX_STANDARD >= 201402L)
#define GP_INTERNAL_CXX17 (GP_CXX_STANDARD >= 201703L)
#define GP_INTERNAL_CXX20 (GP_CXX_STANDARD >= 202002L)
#define GP_INTERNAL_CXX23 (GP_CXX_STANDARD >= 202302L)
#define GP_INTERNAL_CXX26 (GP_CXX_STANDARD >= 202602L)

#ifndef GP_ALLOW_OLDER_STANDARDS
    #if !GP_INTERNAL_CXX23
        #error "[GP] C++23 is required."
    #endif
#else
    #pragma message("[GP] Warning: Building with an older C++ standard than C++23 is not officially supported.")
#endif

// Build configuration
#if defined(DEBUG) || defined(_DEBUG)
    #define GP_BUILD_DEBUG 1
    #define GP_BUILD_RELEASE 0
#else
    #define GP_BUILD_DEBUG 0
    #define GP_BUILD_RELEASE 1
#endif

// Contexpr if available
#if GP_INTERNAL_CXX17
    #define GP_CONSTEXPR constexpr
#else
    #define GP_CONSTEXPR
#endif

// Endianness detection
#if defined(__BYTE_ORDER__)
    #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        #define GP_ENDIAN_LITTLE 1
        #define GP_ENDIAN_BIG 0
    #elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        #define GP_ENDIAN_LITTLE 0
        #define GP_ENDIAN_BIG 1
    #else
        #error "Unknown byte order"
    #endif
#elif defined(_WIN32) || defined(_WIN64)
    // Windows is always little-endian
    #define GP_ENDIAN_LITTLE 1
    #define GP_ENDIAN_BIG 0
#elif defined(__LITTLE_ENDIAN__) || defined(__ARMEL__) || defined(__THUMBEL__) || defined(__AARCH64EL__) || \
    defined(_MIPSEL) || defined(__MIPSEL) || defined(__MIPSEL__)
    #define GP_ENDIAN_LITTLE 1
    #define GP_ENDIAN_BIG 0
#elif defined(__BIG_ENDIAN__) || defined(__ARMEB__) || defined(__THUMBEB__) || defined(__AARCH64EB__) || \
    defined(_MIPSEB) || defined(__MIPSEB) || defined(__MIPSEB__)
    #define GP_ENDIAN_LITTLE 0
    #define GP_ENDIAN_BIG 1
#else
    // Default to little-endian for x86/x64
    #if GP_ARCHITECTURE_X86_FAMILY
        #define GP_ENDIAN_LITTLE 1
        #define GP_ENDIAN_BIG 0
    #else
        #error "Cannot determine endianness"
    #endif
#endif

// SSE (x86/x64)
#if GP_ARCHITECTURE_X86_FAMILY
    #if defined(__SSE__) || defined(_M_IX86_FP) || defined(_M_X64)
        #define GP_SIMD_SSE 1
    #endif
    #if defined(__SSE2__) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2) || defined(_M_X64)
        #define GP_SIMD_SSE2 1
    #endif
    #if defined(__SSE3__)
        #define GP_SIMD_SSE3 1
    #endif
    #if defined(__SSSE3__)
        #define GP_SIMD_SSSE3 1
    #endif
    #if defined(__SSE4_1__)
        #define GP_SIMD_SSE4_1 1
    #endif
    #if defined(__SSE4_2__)
        #define GP_SIMD_SSE4_2 1
    #endif
    #if defined(__AVX__)
        #define GP_SIMD_AVX 1
    #endif
    #if defined(__AVX2__)
        #define GP_SIMD_AVX2 1
    #endif
    #if defined(__AVX512F__)
        #define GP_SIMD_AVX512 1
    #endif
    #if defined(__FMA__)
        #define GP_SIMD_FMA 1
    #endif
#endif

// NEON (ARM)
#if GP_ARCHITECTURE_ARM_FAMILY
    #if defined(__ARM_NEON) || defined(__ARM_NEON__)
        #define GP_SIMD_NEON 1
    #endif
    #if defined(__ARM_FEATURE_FMA)
        #define GP_SIMD_ARM_FMA 1
    #endif
#endif

// WASM SIMD
#if GP_ARCHITECTURE_WASM
    #if defined(__wasm_simd128__)
        #define GP_SIMD_WASM128 1
    #endif
#endif

// Default values for undefined SIMD macros
#ifndef GP_SIMD_SSE
    #define GP_SIMD_SSE 0
#endif
#ifndef GP_SIMD_SSE2
    #define GP_SIMD_SSE2 0
#endif
#ifndef GP_SIMD_SSE3
    #define GP_SIMD_SSE3 0
#endif
#ifndef GP_SIMD_SSSE3
    #define GP_SIMD_SSSE3 0
#endif
#ifndef GP_SIMD_SSE4_1
    #define GP_SIMD_SSE4_1 0
#endif
#ifndef GP_SIMD_SSE4_2
    #define GP_SIMD_SSE4_2 0
#endif
#ifndef GP_SIMD_AVX
    #define GP_SIMD_AVX 0
#endif
#ifndef GP_SIMD_AVX2
    #define GP_SIMD_AVX2 0
#endif
#ifndef GP_SIMD_AVX512
    #define GP_SIMD_AVX512 0
#endif
#ifndef GP_SIMD_FMA
    #define GP_SIMD_FMA 0
#endif
#ifndef GP_SIMD_NEON
    #define GP_SIMD_NEON 0
#endif
#ifndef GP_SIMD_ARM_FMA
    #define GP_SIMD_ARM_FMA 0
#endif
#ifndef GP_SIMD_WASM128
    #define GP_SIMD_WASM128 0
#endif

// Generic SIMD availability
#define GP_SIMD_AVAILABLE (GP_SIMD_SSE || GP_SIMD_NEON || GP_SIMD_WASM128)

// Combine Major, Minor, Patch into a single version number
#define GP_VERSION(major, minor, patch) (((major) << 16) | ((minor) << 8) | (patch))

/// @brief Forces the compiler to inline the decorated function, if possible.
#if GP_COMPILER_MSVC
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

// DLL Export/Import
#if GP_PLATFORM_WINDOWS
    #if defined(GP_BUILD_SHARED)
        #define GP_API __declspec(dllexport)
    #elif defined(GP_USE_SHARED)
        #define GP_API __declspec(dllimport)
    #else
        #define GP_API
    #endif
#elif GP_COMPILER_GCC || GP_COMPILER_CLANG
    #if defined(GP_BUILD_SHARED)
        #define GP_API __attribute__((visibility("default")))
    #else
        #define GP_API
    #endif
#else
    #define GP_API
#endif

// Debug break
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

// Function signature
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

// Deprecated
#if GP_INTERNAL_CXX14
    #define GP_DEPRECATED(msg) [[deprecated(msg)]]
#elif GP_COMPILER_MSVC
    #define GP_DEPRECATED(msg) __declspec(deprecated(msg))
#elif GP_COMPILER_GCC || GP_COMPILER_CLANG
    #define GP_DEPRECATED(msg) __attribute__((deprecated(msg)))
#else
    #define GP_DEPRECATED(msg)
#endif

// No discard
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

// Maybe unused
#if GP_INTERNAL_CXX17
    #define GP_MAYBE_UNUSED [[maybe_unused]]
#elif GP_COMPILER_GCC || GP_COMPILER_CLANG
    #define GP_MAYBE_UNUSED __attribute__((unused))
#else
    #define GP_MAYBE_UNUSED
#endif

// No return
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

/// @brief Prefetches a cache line into L1 for reading.
/// @brief Prefetches a cache line into L1 for writing.
/// @note  Uses __builtin_prefetch on Clang, GCC, and MSVC (VS 2019 16.8+ supports the GCC builtin).
///        Falls back to a no-op on unknown compilers.
#if GP_COMPILER_CLANG || GP_COMPILER_GCC || GP_COMPILER_MSVC
    #define GP_PREFETCH_R(ptr) __builtin_prefetch((ptr), 0, 3)
    #define GP_PREFETCH_W(ptr) __builtin_prefetch((ptr), 1, 3)
#else
    #define GP_PREFETCH_R(ptr) (void)(ptr)
    #define GP_PREFETCH_W(ptr) (void)(ptr)
#endif

// Cache line size (typical values)
#if GP_ARCHITECTURE_X86_FAMILY
    #define GP_CACHE_LINE_SIZE 64
#elif GP_ARCHITECTURE_ARM_FAMILY
    #define GP_CACHE_LINE_SIZE 64
#else
    #define GP_CACHE_LINE_SIZE 64
#endif

// Cache line alignment
#define GP_CACHE_ALIGNED GP_ALIGN(GP_CACHE_LINE_SIZE)

// String concatenation
#define GP_INTERNAL_CONCAT_IMPL(a, b) a##b
#define GP_CONCAT(a, b) GP_INTERNAL_CONCAT_IMPL(a, b)

// Stringification
#define GP_INTERNAL_STRINGIFY_IMPL(x) #x
#define GP_STRINGIFY(x) GP_INTERNAL_STRINGIFY_IMPL(x)

// Get first argument or default
#define GP_FIRST_OR_DEFAULT(first, ...) first

// Get first argument
#define GP_FIRST(...) GP_FIRST_HELPER(__VA_ARGS__, "")
#define GP_FIRST_HELPER(first, ...) first

// Expand remaining arguments (for printf-style formatting)
#define GP_EXPAND_ARGS(...) GP_EXPAND_ARGS_HELPER(__VA_ARGS__, )
#define GP_EXPAND_ARGS_HELPER(first, ...) , ##__VA_ARGS__

// Format message helper
#define GP_FORMAT_MSG(...) GP_FIRST(__VA_ARGS__)

// Bit manipulation
#define GP_BIT(x) (1 << (x))
#define GP_BIT64(x) (1ULL << (x))

// Array size
#define GP_ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

// Offset of member
#if GP_INTERNAL_CXX17
    #define GP_OFFSETOF(type, member) offsetof(type, member)
#else
    #define GP_OFFSETOF(type, member) ((size_t)&(((type*)0)->member))
#endif

// Unused parameter
#define GP_UNUSED(x) (void)(x)

#if GP_DEBUG
    #define GP_ASSERT(expr, ...)
    #define GP_ASSERT_FATAL(expr, ...)
    #define GP_ENSURE(expr, ...)
#else
    #define GP_ASSERT(expr, ...)
    #define GP_ASSERT_FATAL(expr, ...)
    #define GP_ENSURE(expr, ...)
#endif
