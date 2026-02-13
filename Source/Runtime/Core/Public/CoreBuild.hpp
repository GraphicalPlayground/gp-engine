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
    #error "Unknown platform"
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
    #error "Unknown architecture"
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
    #error "Unknown compiler"
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
    #error "Cannot determine C++ standard version"
#endif

#define GP_INTERNAL_CXX11 (GP_CXX_STANDARD >= 201103L)
#define GP_INTERNAL_CXX14 (GP_CXX_STANDARD >= 201402L)
#define GP_INTERNAL_CXX17 (GP_CXX_STANDARD >= 201703L)
#define GP_INTERNAL_CXX20 (GP_CXX_STANDARD >= 202002L)
#define GP_INTERNAL_CXX23 (GP_CXX_STANDARD >= 202302L)
#define GP_INTERNAL_CXX26 (GP_CXX_STANDARD >= 202602L)

#ifndef GP_ALLOW_OLDER_STANDARDS
    #if !GP_INTERNAL_CXX20
        #error "This project requires at least C++20 standard"
    #endif
#else
    #pragma message("Warning: Building with an older C++ standard than C++20 is not officially supported.")
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

// Force inline
#if GP_COMPILER_MSVC
    #define GP_FORCEINLINE __forceinline
#elif GP_COMPILER_GCC || GP_COMPILER_CLANG
    #define GP_FORCEINLINE inline __attribute__((always_inline))
#else
    #define GP_FORCEINLINE inline
#endif

// No inline
#if GP_COMPILER_MSVC
    #define GP_NOINLINE __declspec(noinline)
#elif GP_COMPILER_GCC || GP_COMPILER_CLANG
    #define GP_NOINLINE __attribute__((noinline))
#else
    #define GP_NOINLINE
#endif

// Restrict keyword
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

// Likely/Unlikely hints
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

// Unreachable
#if GP_INTERNAL_CXX23
    #define GP_UNREACHABLE() std::unreachable()
#elif GP_COMPILER_MSVC
    #define GP_UNREACHABLE() __assume(0)
#elif GP_COMPILER_GCC || GP_COMPILER_CLANG
    #define GP_UNREACHABLE() __builtin_unreachable()
#else
    #define GP_UNREACHABLE() ((void)0)
#endif

// Assume
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
#elif GP_COMPILER_MSVC && _MSC_VER >= 1700
    #define GP_NODISCARD _Check_return_
#elif GP_COMPILER_GCC || GP_COMPILER_CLANG
    #define GP_NODISCARD __attribute__((warn_unused_result))
#else
    #define GP_NODISCARD
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

// Alignment
#if GP_INTERNAL_CXX11
    #define GP_ALIGN(x) alignas(x)
#elif GP_COMPILER_MSVC
    #define GP_ALIGN(x) __declspec(align(x))
#elif GP_COMPILER_GCC || GP_COMPILER_CLANG
    #define GP_ALIGN(x) __attribute__((aligned(x)))
#else
    #define GP_ALIGN(x)
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

// Physics options
#ifndef GP_ENABLE_PHYSICS
    #define GP_ENABLE_PHYSICS 1
#endif

// Audio options
#ifndef GP_ENABLE_AUDIO
    #define GP_ENABLE_AUDIO 1
#endif

#ifndef GP_ENABLE_3D_AUDIO
    #define GP_ENABLE_3D_AUDIO 1
#endif

// Networking options
#ifndef GP_ENABLE_NETWORKING
    #define GP_ENABLE_NETWORKING 1
#endif

// Profiling options
#ifndef GP_ENABLE_PROFILING
    #define GP_ENABLE_PROFILING GP_BUILD_DEBUG
#endif

#ifndef GP_ENABLE_GPU_PROFILING
    #define GP_ENABLE_GPU_PROFILING GP_BUILD_DEBUG
#endif

// Memory tracking
#ifndef GP_ENABLE_MEMORY_TRACKING
    #define GP_ENABLE_MEMORY_TRACKING GP_BUILD_DEBUG
#endif

#ifndef GP_ENABLE_MEMORY_VALIDATION
    #define GP_ENABLE_MEMORY_VALIDATION GP_BUILD_DEBUG
#endif

// Logging options
#ifndef GP_ENABLE_LOGGING
    #define GP_ENABLE_LOGGING 1
#endif

#ifndef GP_ENABLE_VERBOSE_LOGGING
    #define GP_ENABLE_VERBOSE_LOGGING GP_BUILD_DEBUG
#endif

// Assertion options
#ifndef GP_ENABLE_ASSERTS
    #define GP_ENABLE_ASSERTS GP_BUILD_DEBUG
#endif

// Hot reload support
#ifndef GP_ENABLE_HOT_RELOAD
    #define GP_ENABLE_HOT_RELOAD GP_BUILD_DEBUG
#endif

// Script binding options
#ifndef GP_ENABLE_SCRIPT_BINDINGS
    #define GP_ENABLE_SCRIPT_BINDINGS 1
#endif

// Editor mode
#ifndef GP_ENABLE_EDITOR
    #define GP_ENABLE_EDITOR 0
#endif

// Double precision math
#ifndef GP_ENABLE_DOUBLE_PRECISION
    #define GP_ENABLE_DOUBLE_PRECISION 0
#endif

// Assertions
#if GP_ENABLE_ASSERTS
    #define GP_ASSERT_IMPL(expr, msg, file, line, func)   // \
        //do { \
        //    if (GP_UNLIKELY(!(expr))) { \
        //        ::GP::AssertAction action = ::GP::DefaultAssertHandler( \
        //            #expr, msg, file, line, func \
        //        ); \
        //        if (action == ::GP::AssertAction::Break) { \
        //            GP_DEBUGBREAK(); \
        //        } else if (action == ::GP::AssertAction::Abort) { \
        //            std::abort(); \
        //        } \
        //    } \
        //} while(0)

    #define GP_ASSERT(expr, ...) GP_ASSERT_IMPL(expr, GP_FORMAT_MSG(__VA_ARGS__), __FILE__, __LINE__, GP_FUNCSIG)

    #define GP_ASSERT_FATAL(expr, ...)   //\
        //do { \
        //    if (GP_UNLIKELY(!(expr))) { \
        //        ::GP::Log::Write(::GP::Log::Level::Fatal, __FILE__, __LINE__, GP_FUNCSIG, \
        //            "FATAL ASSERTION FAILED: %s\n    Message: " GP_FIRST(__VA_ARGS__), \
        //            #expr GP_EXPAND_ARGS(__VA_ARGS__)); \
        //        GP_DEBUGBREAK(); \
        //        std::abort(); \
        //    } \
        //} while(0)

    #define GP_VERIFY(expr, ...) GP_ASSERT(expr, __VA_ARGS__)

    #define GP_CHECK(expr, ...) GP_ASSERT(expr, __VA_ARGS__)

    #define GP_ENSURE(expr, ...)   // \
        //do { \
        //    if (GP_UNLIKELY(!(expr))) { \
        //        ::GP::Log::Write(::GP::Log::Level::Warning, __FILE__, __LINE__, GP_FUNCSIG, \
        //            "ENSURE FAILED: %s\n    Message: " GP_FIRST(__VA_ARGS__), \
        //            #expr GP_EXPAND_ARGS(__VA_ARGS__)); \
        //    } \
        //} while(0)

    #define GP_STATIC_ASSERT(expr, msg) static_assert(expr, msg)

    #define GP_ASSERT_NOT_NULL(ptr, ...) GP_ASSERT((ptr) != nullptr, __VA_ARGS__)

    #define GP_ASSERT_ALIGNED(ptr, alignment) GP_ASSERT((reinterpret_cast<uintptr>(ptr) & ((alignment) - 1)) == 0, \
        "Pointer %p is not aligned to %zu bytes", (void*)(ptr), (size_t)(alignment))

    #define GP_ASSERT_RANGE(value, min, max)  \
        GP_ASSERT((value) >= (min) && (value) <= (max), \
            "Value %lld is out of range [%lld, %lld]", \
            (long long)(value), (long long)(min), (long long)(max))

    #define GP_ASSERT_UNREACHABLE(...)  \
        do { \
            GP_ASSERT_FATAL(false, "Unreachable code executed: " GP_FIRST_OR_DEFAULT(__VA_ARGS__, "No message")); \
            GP_UNREACHABLE(); \
        } while(0)

    #define GP_ASSERT_NOT_IMPLEMENTED(...) \
        GP_ASSERT_FATAL(false, "Not implemented: " GP_FIRST_OR_DEFAULT(__VA_ARGS__, GP_FUNCSIG))

#else
    #define GP_ASSERT(expr, ...) ((void)0)
    #define GP_ASSERT_FATAL(expr, ...) do { if (!(expr)) { std::abort(); } } while(0)
    #define GP_VERIFY(expr, ...) ((void)(expr))
    #define GP_CHECK(expr, ...) ((void)(expr))
    #define GP_ENSURE(expr, ...) ((void)0)
    #define GP_STATIC_ASSERT(expr, msg) static_assert(expr, msg)
    #define GP_ASSERT_NOT_NULL(ptr, ...) ((void)0)
    #define GP_ASSERT_ALIGNED(ptr, alignment) ((void)0)
    #define GP_ASSERT_RANGE(value, min, max) ((void)0)
    #define GP_ASSERT_UNREACHABLE(...) GP_UNREACHABLE()
    #define GP_ASSERT_NOT_IMPLEMENTED(...) GP_UNREACHABLE()
#endif

namespace GP::Build
{

namespace Platform
{

inline constexpr bool IsWindows = GP_PLATFORM_WINDOWS;
inline constexpr bool IsLinux = GP_PLATFORM_LINUX;
inline constexpr bool IsMacOS = GP_PLATFORM_MACOS;
inline constexpr bool IsIOS = GP_PLATFORM_IOS;
inline constexpr bool IsAndroid = GP_PLATFORM_ANDROID;
inline constexpr bool IsWeb = GP_PLATFORM_WEB;

inline constexpr bool IsDesktop = GP_PLATFORM_DESKTOP;
inline constexpr bool IsMobile = GP_PLATFORM_MOBILE;
inline constexpr bool IsApple = GP_PLATFORM_APPLE;
inline constexpr bool IsUnix = GP_PLATFORM_UNIX;

}   // namespace Platform

namespace Architecture
{

inline constexpr bool IsX86 = GP_ARCHITECTURE_X86;
inline constexpr bool IsX64 = GP_ARCHITECTURE_X64;
inline constexpr bool IsARM32 = GP_ARCHITECTURE_ARM32;
inline constexpr bool IsARM64 = GP_ARCHITECTURE_ARM64;
inline constexpr bool IsWASM = GP_ARCHITECTURE_WASM;

inline constexpr bool IsX86Family = GP_ARCHITECTURE_X86_FAMILY;
inline constexpr bool IsARMFamily = GP_ARCHITECTURE_ARM_FAMILY;
inline constexpr bool Is64Bit = GP_ARCHITECTURE_64BIT;
inline constexpr bool Is32Bit = GP_ARCHITECTURE_32BIT;

}   // namespace Architecture

namespace Compiler
{

inline constexpr bool IsMSVC = GP_COMPILER_MSVC;
inline constexpr bool IsClang = GP_COMPILER_CLANG;
inline constexpr bool IsGCC = GP_COMPILER_GCC;
inline constexpr bool IsEmscripten = GP_COMPILER_EMSCRIPTEN;
inline constexpr bool IsIntel = GP_COMPILER_INTEL;

inline constexpr int Version = GP_COMPILER_VERSION;

}   // namespace Compiler

namespace Language
{

inline constexpr long Standard = GP_CXX_STANDARD;
inline constexpr bool IsCXX11 = GP_INTERNAL_CXX11;
inline constexpr bool IsCXX14 = GP_INTERNAL_CXX14;
inline constexpr bool IsCXX17 = GP_INTERNAL_CXX17;
inline constexpr bool IsCXX20 = GP_INTERNAL_CXX20;
inline constexpr bool IsCXX23 = GP_INTERNAL_CXX23;
inline constexpr bool IsCXX26 = GP_INTERNAL_CXX26;

}   // namespace Language

namespace Configuration
{

inline constexpr bool IsDebug = GP_BUILD_DEBUG;
inline constexpr bool IsRelease = GP_BUILD_RELEASE;

}   // namespace Configuration

namespace Endian
{

inline constexpr bool IsLittle = GP_ENDIAN_LITTLE;
inline constexpr bool IsBig = GP_ENDIAN_BIG;

}   // namespace Endian

namespace SIMD
{

// x86/x64 SIMD
inline constexpr bool HasSSE = GP_SIMD_SSE;
inline constexpr bool HasSSE2 = GP_SIMD_SSE2;
inline constexpr bool HasSSE3 = GP_SIMD_SSE3;
inline constexpr bool HasSSSE3 = GP_SIMD_SSSE3;
inline constexpr bool HasSSE4_1 = GP_SIMD_SSE4_1;
inline constexpr bool HasSSE4_2 = GP_SIMD_SSE4_2;
inline constexpr bool HasAVX = GP_SIMD_AVX;
inline constexpr bool HasAVX2 = GP_SIMD_AVX2;
inline constexpr bool HasAVX512 = GP_SIMD_AVX512;
inline constexpr bool HasFMA = GP_SIMD_FMA;

// ARM SIMD
inline constexpr bool HasNEON = GP_SIMD_NEON;
inline constexpr bool HasARMFMA = GP_SIMD_ARM_FMA;

// WASM SIMD
inline constexpr bool HasWASM128 = GP_SIMD_WASM128;

// Generic
inline constexpr bool IsAvailable = GP_SIMD_AVAILABLE;

}   // namespace SIMD

namespace Features
{

inline constexpr bool HasPhysics = GP_ENABLE_PHYSICS;
inline constexpr bool HasAudio = GP_ENABLE_AUDIO;
inline constexpr bool Has3DAudio = GP_ENABLE_3D_AUDIO;
inline constexpr bool HasNetworking = GP_ENABLE_NETWORKING;
inline constexpr bool HasProfiling = GP_ENABLE_PROFILING;
inline constexpr bool HasGPUProfiling = GP_ENABLE_GPU_PROFILING;
inline constexpr bool HasMemoryTracking = GP_ENABLE_MEMORY_TRACKING;
inline constexpr bool HasMemoryValidation = GP_ENABLE_MEMORY_VALIDATION;
inline constexpr bool HasLogging = GP_ENABLE_LOGGING;
inline constexpr bool HasVerboseLogging = GP_ENABLE_VERBOSE_LOGGING;
inline constexpr bool HasAsserts = GP_ENABLE_ASSERTS;
inline constexpr bool HasHotReload = GP_ENABLE_HOT_RELOAD;
inline constexpr bool HasScriptBindings = GP_ENABLE_SCRIPT_BINDINGS;
inline constexpr bool IsEditor = GP_ENABLE_EDITOR;
inline constexpr bool HasDoublePrecision = GP_ENABLE_DOUBLE_PRECISION;

}   // namespace Features

}   // namespace GP::Build

#include <cstddef>
#include <cstdint>

namespace GP
{

// Scalar Types

using Int8 = std::int8_t;
using UInt8 = std::uint8_t;
using Int16 = std::int16_t;
using UInt16 = std::uint16_t;
using Int32 = std::int32_t;
using UInt32 = std::uint32_t;
using Int64 = std::int64_t;
using UInt64 = std::uint64_t;

using Float32 = float;
using Float64 = double;

// Character Types

using Char8 = char;
#if GP_INTERNAL_CXX20
using UChar8 = char8_t;   // Native C++20 char8
#else
using UChar8 = unsigned char;
#endif
using Char16 = char16_t;
using Char32 = char32_t;
using WideChar = wchar_t;

// System / Memory Types

using SizeT = std::size_t;
using SSizeT = std::ptrdiff_t;
using OffsetT = std::ptrdiff_t;
using UIntPtr = std::uintptr_t;
using IntPtr = std::intptr_t;
using NullPtr = std::nullptr_t;
#if GP_INTERNAL_CXX20
using Byte = std::byte;
#else
using Byte = std::uint8_t;
#endif

// Validations

static_assert(sizeof(Int8) == 1, "Int8 must be 1 byte");
static_assert(sizeof(Int16) == 2, "Int16 must be 2 bytes");
static_assert(sizeof(Int32) == 4, "Int32 must be 4 bytes");
static_assert(sizeof(Int64) == 8, "Int64 must be 8 bytes");
static_assert(sizeof(Float32) == 4, "Float32 must be 4 bytes");
static_assert(sizeof(Float64) == 8, "Float64 must be 8 bytes");

// Architecture-specific validation
#if GP_ARCHITECTURE_64BIT
static_assert(sizeof(SizeT) == 8, "Size must be 8 bytes on 64-bit platforms");
#else
static_assert(sizeof(SizeT) == 4, "Size must be 4 bytes on 32-bit platforms");
#endif

// Real type definition
#if GP_ENABLE_DOUBLE_PRECISION
using Real = Float64;
#else
using Real = Float32;
#endif

}   // namespace GP
