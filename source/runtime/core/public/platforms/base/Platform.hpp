// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "miscellaneous/BuildDefines.hpp"   // IWYU pragma: export
#include "miscellaneous/PreProcessorUtilities.hpp"
#include <type_traits>

// C++23 language version guard, must appear before any feature-test checks.
#if __cplusplus < 202302L
    #error "gp-engine requires C++23 or later. Pass -std=c++23 (or /std:c++latest on MSVC)."
#endif

// Automatically detect the platform, architecture and compiler if not already defined by GPBT
#if !defined(GP_SKIP_BUILD_DETECTION) || !GP_SKIP_BUILD_DETECTION
    #include "platforms/base/PlatformDetection.hpp"
#endif

/// @section Platform Detection.

#ifndef GP_PLATFORM_WINDOWS
    #define GP_PLATFORM_WINDOWS             GP_FALSE
#endif
#ifndef GP_PLATFORM_LINUX
    #define GP_PLATFORM_LINUX               GP_FALSE
#endif
#ifndef GP_PLATFORM_MACOS
    #define GP_PLATFORM_MACOS               GP_FALSE
#endif

#if (GP_PLATFORM_WINDOWS + GP_PLATFORM_LINUX + GP_PLATFORM_MACOS != 1)
    #error "Exactly one platform must be defined as true!"
#endif

#define GP_PLATFORM_DESKTOP                 (GP_PLATFORM_WINDOWS || GP_PLATFORM_LINUX || GP_PLATFORM_MACOS)
#define GP_PLATFORM_APPLE                   (GP_PLATFORM_MACOS)
#define GP_PLATFORM_UNIX                    (GP_PLATFORM_LINUX || GP_PLATFORM_APPLE)
#define GP_PLATFORM_POSIX                   (GP_PLATFORM_UNIX)
#define GP_PLATFORM_MICROSOFT               (GP_PLATFORM_WINDOWS)

/// @section Architecture Detection.

#ifndef GP_ARCHITECTURE_X86
    #define GP_ARCHITECTURE_X86             GP_FALSE
#endif
#ifndef GP_ARCHITECTURE_X64
    #define GP_ARCHITECTURE_X64             GP_FALSE
#endif
#ifndef GP_ARCHITECTURE_ARM32
    #define GP_ARCHITECTURE_ARM32           GP_FALSE
#endif
#ifndef GP_ARCHITECTURE_ARM64
    #define GP_ARCHITECTURE_ARM64           GP_FALSE
#endif

#if (GP_ARCHITECTURE_X86 + GP_ARCHITECTURE_X64 + GP_ARCHITECTURE_ARM32 + GP_ARCHITECTURE_ARM64 != 1)
    #error "Exactly one architecture must be defined as true!"
#endif

#define GP_ARCHITECTURE_X86_FAMILY          (GP_ARCHITECTURE_X86 || GP_ARCHITECTURE_X64)
#define GP_ARCHITECTURE_ARM_FAMILY          (GP_ARCHITECTURE_ARM32 || GP_ARCHITECTURE_ARM64)
#define GP_ARCHITECTURE_64BIT               (GP_ARCHITECTURE_X64 || GP_ARCHITECTURE_ARM64)
#define GP_ARCHITECTURE_32BIT               (GP_ARCHITECTURE_X86 || GP_ARCHITECTURE_ARM32)

/// @section Compiler Detection.

#ifndef GP_COMPILER_MSVC
    #define GP_COMPILER_MSVC               GP_FALSE
#endif
#ifndef GP_COMPILER_CLANG
    #define GP_COMPILER_CLANG              GP_FALSE
#endif
#ifndef GP_COMPILER_GCC
    #define GP_COMPILER_GCC                GP_FALSE
#endif
#ifndef GP_COMPILER_INTEL
    #define GP_COMPILER_INTEL              GP_FALSE
#endif

#if (GP_COMPILER_MSVC + GP_COMPILER_CLANG + GP_COMPILER_GCC + GP_COMPILER_INTEL == 0)
    #error "At least one compiler must be defined as true!"
#endif

/// @section Compiler minimum version checks.
/// @note All feature checks below require C++23 which is already guarded above.

#if GP_COMPILER_MSVC
    // VS 2022 17.2+ (_MSC_VER 1932) ships full C++23 support behind /std:c++latest.
    #if _MSC_VER < 1930
        #error "MSVC 2022 17.0 or later is required for C++23 support."
    #endif
#endif

#if !defined(__cpp_if_constexpr)
    #error "Compiler is expected to support `if constexpr` statements."
#endif

#if !defined(__cpp_fold_expressions)
    #error "Compiler is expected to support fold expressions."
#endif

// __has_feature is a Clang extension; use the standard feature-test macro instead.
#if !defined(__cpp_decltype_auto)
    #error "Compiler is expected to support `decltype(auto)`."
#endif

/// @section Platform-Specific / Compiler Specific Headers.

// Include Generic Platform-Specific Headers
#include "platforms/generic/GenericPlatform.hpp"   // IWYU pragma: export

#if GP_PLATFORM_WINDOWS
    #include "platforms/windows/WindowsPlatform.hpp"   // IWYU pragma: export
#elif GP_PLATFORM_LINUX
    #include "platforms/linux/LinuxPlatform.hpp"       // IWYU pragma: export
#elif GP_PLATFORM_MACOS
    #include "platforms/macos/MacOSPlatform.hpp"       // IWYU pragma: export
#endif

#if GP_COMPILER_CLANG
    #include "compilers/clang/ClangCompiler.hpp"   // IWYU pragma: export
#elif GP_COMPILER_MSVC
    #include "compilers/msvc/MSVCCompiler.hpp"     // IWYU pragma: export
#elif GP_COMPILER_GCC
    #include "compilers/gcc/GCCCompiler.hpp"       // IWYU pragma: export
#elif GP_COMPILER_INTEL
    #include "compilers/intel/IntelCompiler.hpp"   // IWYU pragma: export
#endif

/// @section Platform supports features flags.

#ifndef GP_PLATFORM_SUPPORTS_TEXTURE_STREAMING
    #define GP_PLATFORM_SUPPORTS_TEXTURE_STREAMING GP_TRUE
#endif

#ifndef GP_PLATFORM_SUPPORTS_VIRTUAL_TEXTURES
    #define GP_PLATFORM_SUPPORTS_VIRTUAL_TEXTURES GP_FALSE
#endif

#ifndef GP_PLATFORM_SUPPORTS_VARIABLE_RATE_SHADING
    #define GP_PLATFORM_SUPPORTS_VARIABLE_RATE_SHADING GP_FALSE
#endif

#ifndef GP_PLATFORM_SUPPORTS_GEOMETRY_SHADERS
    #define GP_PLATFORM_SUPPORTS_GEOMETRY_SHADERS GP_TRUE
#endif

#ifndef GP_PLATFORM_SUPPORTS_MESH_SHADERS
    #define GP_PLATFORM_SUPPORTS_MESH_SHADERS GP_FALSE
#endif

#ifndef GP_PLATFORM_SUPPORTS_WORKGRAPH_SHADERS
    #define GP_PLATFORM_SUPPORTS_WORKGRAPH_SHADERS GP_FALSE
#endif

#ifndef GP_PLATFORM_SUPPORTS_BINDLESS_RENDERING
    #define GP_PLATFORM_SUPPORTS_BINDLESS_RENDERING GP_FALSE
#endif

#ifndef GP_PLATFORM_BUILTIN_VERTEX_HALF_FLOAT
    #define GP_PLATFORM_BUILTIN_VERTEX_HALF_FLOAT GP_TRUE
#endif

#ifndef GP_PLATFORM_SUPPORTS_TBB
    #define GP_PLATFORM_SUPPORTS_TBB GP_FALSE
#endif

#ifndef GP_PLATFORM_SUPPORTS_JEMALLOC
    #define GP_PLATFORM_SUPPORTS_JEMALLOC GP_FALSE
#endif

#ifndef GP_PLATFORM_SUPPORTS_MIMALLOC
    #define GP_PLATFORM_SUPPORTS_MIMALLOC GP_FALSE
#endif

#ifndef GP_PLATFORM_USE_ANSI_POSIX_MALLOC
    #define GP_PLATFORM_USE_ANSI_POSIX_MALLOC GP_FALSE
#endif

#ifndef GP_PLATFORM_USE_ALIGNED_MALLOC
    #define GP_PLATFORM_USE_ALIGNED_MALLOC GP_FALSE
#endif

#ifndef GP_PLATFORM_USE_ANSI_MEMALIGN
    #define GP_PLATFORM_USE_ANSI_MEMALIGN GP_FALSE
#endif

#ifndef GP_PLATFORM_IS_ANSI_MALLOC_THREADSAFE
    #define GP_PLATFORM_IS_ANSI_MALLOC_THREADSAFE GP_FALSE
#endif

#ifndef GP_PLATFORM_SUPPORTS_NAMED_PIPES
    #define GP_PLATFORM_SUPPORTS_NAMED_PIPES GP_FALSE
#endif

#ifndef GP_PLATFORM_SUPPORTS_BORDERLESS_WINDOW
    #define GP_PLATFORM_SUPPORTS_BORDERLESS_WINDOW GP_FALSE
#endif

#ifndef GP_PLATFORM_SUPPORTS_MULTIPLE_NATIVE_WINDOWS
    #define GP_PLATFORM_SUPPORTS_MULTIPLE_NATIVE_WINDOWS GP_TRUE
#endif

#ifndef GP_PLATFORM_HAS_128BIT_ATOMICS
    #define GP_PLATFORM_HAS_128BIT_ATOMICS GP_FALSE
#endif

#ifndef GP_PLATFORM_SUPPORTS_ASYMMETRIC_FENCES
    #define GP_PLATFORM_SUPPORTS_ASYMMETRIC_FENCES GP_FALSE
#endif

#ifndef GP_PLATFORM_SUPPORTS_STACK_SYMBOLS
    #define GP_PLATFORM_SUPPORTS_STACK_SYMBOLS GP_FALSE
#endif

/// @section Platform-Independent Function Specifiers.

#ifndef GP_VARARGS
    #define GP_VARARGS
#endif
#ifndef GP_CDECL
    #define GP_CDECL
#endif
#ifndef GP_STDCALL
    #define GP_STDCALL
#endif
#ifndef GP_INLINE
    #define GP_INLINE inline
#endif
#ifndef GP_FORCEINLINE
    #define GP_FORCEINLINE inline
#endif
#ifndef GP_FORCENOINLINE
    #define GP_FORCENOINLINE
#endif
#ifndef GP_RESTRICT
    #define GP_RESTRICT __restrict
#endif
#ifndef GP_CALLSITE_FORCEINLINE
    #define GP_CALLSITE_FORCEINLINE
#endif
#ifndef GP_FLATTEN
    #define GP_FLATTEN
#endif

#ifndef GP_DEFINE_FORCEINLINE_HINT_TO_INLINE
    #define GP_DEFINE_FORCEINLINE_HINT_TO_INLINE GP_FALSE
#endif

#if GP_DEFINE_FORCEINLINE_HINT_TO_INLINE
    #define GP_FORCEINLINE_HINT inline
#else
    #define GP_FORCEINLINE_HINT GP_FORCEINLINE
#endif

/// @section Abstract keyword.
/// @note  On MSVC/Apple, 'abstract' is a compiler extension for abstract classes.
///        On other platforms it is left empty; use pure virtual '= 0' directly.
#ifndef GP_ABSTRACT
    #define GP_ABSTRACT
#endif

/// @section Debug break.
/// @note Falls back to a no-op trap if the platform header did not provide one.

#ifndef GP_PLATFORM_BREAK
    #if defined(__has_builtin) && __has_builtin(__builtin_debugtrap)
        #define GP_PLATFORM_BREAK() __builtin_debugtrap()
    #elif GP_COMPILER_CLANG || GP_COMPILER_GCC
        #define GP_PLATFORM_BREAK() __builtin_trap()
    #else
        #define GP_PLATFORM_BREAK() (void(0))
    #endif
#endif

/// @section Standard C++ attributes (C++11–C++23).

/// @brief [[nodiscard]], emit a warning when the return value is discarded.
#if !defined(GP_NODISCARD) && defined(__has_cpp_attribute)
    #if __has_cpp_attribute(nodiscard)
        #define GP_NODISCARD [[nodiscard]]
    #endif
#endif
#ifndef GP_NODISCARD
    #define GP_NODISCARD
#endif

/// @brief [[nodiscard(msg)]], nodiscard with an explanatory message (C++20).
#if !defined(GP_NODISCARD_MSG) && defined(__has_cpp_attribute)
    #if __has_cpp_attribute(nodiscard) >= 201907L
        #define GP_NODISCARD_MSG(msg) [[nodiscard(msg)]]
    #endif
#endif
#ifndef GP_NODISCARD_MSG
    #define GP_NODISCARD_MSG(msg) GP_NODISCARD
#endif

/// @brief [[maybe_unused]], suppress unused-variable/parameter warnings.
#if !defined(GP_MAYBE_UNUSED) && defined(__has_cpp_attribute)
    #if __has_cpp_attribute(maybe_unused)
        #define GP_MAYBE_UNUSED [[maybe_unused]]
    #endif
#endif
#ifndef GP_MAYBE_UNUSED
    #define GP_MAYBE_UNUSED
#endif

/// @brief [[noreturn]], function never returns (abort, throw, infinite loop).
#if !defined(GP_NORETURN) && defined(__has_cpp_attribute)
    #if __has_cpp_attribute(noreturn)
        #define GP_NORETURN [[noreturn]]
    #endif
#endif
#ifndef GP_NORETURN
    #define GP_NORETURN
#endif

/// @brief [[deprecated]] / [[deprecated(msg)]], mark a declaration as deprecated.
#if !defined(GP_DEPRECATED) && defined(__has_cpp_attribute)
    #if __has_cpp_attribute(deprecated)
        #define GP_DEPRECATED            [[deprecated]]
        #define GP_DEPRECATED_MSG(msg)   [[deprecated(msg)]]
    #endif
#endif
#ifndef GP_DEPRECATED
    #define GP_DEPRECATED
    #define GP_DEPRECATED_MSG(msg)
#endif

/// @brief [[no_unique_address]], allow empty members to share storage (C++20).
///        MSVC requires [[msvc::no_unique_address]] for correct ABI behaviour.
#if !defined(GP_NO_UNIQUE_ADDRESS) && defined(__has_cpp_attribute)
    #if __has_cpp_attribute(msvc::no_unique_address)
        #define GP_NO_UNIQUE_ADDRESS [[msvc::no_unique_address]]
    #elif __has_cpp_attribute(no_unique_address)
        #define GP_NO_UNIQUE_ADDRESS [[no_unique_address]]
    #endif
#endif
#ifndef GP_NO_UNIQUE_ADDRESS
    #define GP_NO_UNIQUE_ADDRESS
#endif

/// @section Compiler-specific annotation macros.

/// @brief Marks a pointer/reference parameter as lifetime-bound to the call site.
///        Enables Clang's (and MSVC's) dangling-reference diagnostics.
#ifndef GP_LIFETIMEBOUND
    #define GP_LIFETIMEBOUND
#endif

/// @brief Suppresses debug-info generation for a function.
#ifndef GP_NODEBUG
    #define GP_NODEBUG
#endif

/// @brief Marks an allocator function, enabling alias analysis and leak detection.
///        Usage: GP_ALLOCATION_FUNCTION() / GP_ALLOCATION_FUNCTION(sizeArgIdx)
#ifndef GP_ALLOCATION_FUNCTION
    #define GP_ALLOCATION_FUNCTION(...)
#endif

/// @brief Marks a function as guaranteed to return a non-null pointer.
#ifndef GP_FUNCTION_NON_NULL_RETURN_START
    #define GP_FUNCTION_NON_NULL_RETURN_START
#endif
#ifndef GP_FUNCTION_NON_NULL_RETURN_END
    #define GP_FUNCTION_NON_NULL_RETURN_END
#endif

/// @brief Marks a function return value or parameter as mandatory to check
///        (warn_unused_result / [[nodiscard]]-style, but at the end of the declaration).
#ifndef GP_FUNCTION_CHECK_RETURN_END
    #define GP_FUNCTION_CHECK_RETURN_END
#endif

/// @brief Marks a function as never returning (noreturn), placed at end of declaration.
#ifndef GP_FUNCTION_NO_RETURN_END
    #define GP_FUNCTION_NO_RETURN_END
#endif

/// @section selectany / weak linkage.

#ifndef GP_SELECT_ANY
    #if GP_COMPILER_MSVC
        #define GP_SELECT_ANY __declspec(selectany)
    #elif GP_COMPILER_CLANG || GP_COMPILER_GCC || GP_COMPILER_INTEL
        #define GP_SELECT_ANY __attribute__((weak))
    #else
        #define GP_SELECT_ANY
    #endif
#endif

/// @section DLL export / import.

#ifndef GP_DLLEXPORT
    #define GP_DLLEXPORT
#endif
#ifndef GP_DLLIMPORT
    #define GP_DLLIMPORT
#endif

/// @section Struct layout.

/// @brief Forces a struct/class to be packed with no padding bytes.
///        Use with caution: misaligned access can be costly on some architectures.
#ifndef GP_PACKED
    #if GP_COMPILER_MSVC
        #define GP_PACKED __pragma(pack(push, 1)) /* struct body */ __pragma(pack(pop))
    // Note: GP_PACKED wraps the struct on MSVC. For per-struct use, prefer:
    //   #pragma pack(push, 1) / struct Foo { ... }; / #pragma pack(pop)
    #elif GP_COMPILER_CLANG || GP_COMPILER_GCC || GP_COMPILER_INTEL
        #define GP_PACKED __attribute__((packed))
    #else
        #define GP_PACKED
    #endif
#endif

/// @brief Specifies the alignment of a type or variable. Wraps alignas(n).
///        alignas is standard since C++11 and available on all supported compilers.
#ifndef GP_ALIGN
    #define GP_ALIGN(n) alignas(n)
#endif

/// @section Branch-prediction hints.

/// @brief Suggests to the compiler that expression x is likely to be true.
///        In C++23 prefer [[likely]] on the if/else branch body directly.
#ifndef GP_LIKELY
    #if GP_COMPILER_CLANG || GP_COMPILER_GCC || GP_COMPILER_INTEL
        #define GP_LIKELY(x)   __builtin_expect(!!(x), 1)
    #else
        #define GP_LIKELY(x)   (!!(x))
    #endif
#endif

/// @brief Suggests to the compiler that expression x is unlikely to be true.
#ifndef GP_UNLIKELY
    #if GP_COMPILER_CLANG || GP_COMPILER_GCC || GP_COMPILER_INTEL
        #define GP_UNLIKELY(x) __builtin_expect(!!(x), 0)
    #else
        #define GP_UNLIKELY(x) (!!(x))
    #endif
#endif

/// @section Optimisation hints.

/// @brief Informs the optimiser that expression x is assumed to be true.
///        Uses C++23 [[assume(x)]] when supported; falls back to compiler intrinsics.
///        WARNING: undefined behaviour if the assumption is false at runtime.
#ifndef GP_ASSUME
    #if defined(__cpp_assume)   // C++23 P1774R8
        #define GP_ASSUME(x) [[assume(x)]]
    #elif GP_COMPILER_CLANG
        #define GP_ASSUME(x) __builtin_assume(x)
    #elif GP_COMPILER_MSVC
        #define GP_ASSUME(x) __assume(x)
    #elif GP_COMPILER_GCC
        // GCC 13+ supports __attribute__((assume(x))).
        #if GP_COMPILER_VERSION_MAJOR >= 13
            #define GP_ASSUME(x) __attribute__((assume(x)))
        #else
            // Pre-GCC 13: manufacture an unreachable path as a weaker hint.
            #define GP_ASSUME(x) do { if (!(x)) __builtin_unreachable(); } while (0)
        #endif
    #else
        #define GP_ASSUME(x) (void(0))
    #endif
#endif

/// @brief Marks a code path as unreachable.
///        Triggers undefined behaviour if reached; allows the optimiser to eliminate
///        the dead path entirely.
#ifndef GP_BUILTIN_UNREACHABLE
    #if GP_COMPILER_CLANG || GP_COMPILER_GCC || GP_COMPILER_INTEL
        #define GP_BUILTIN_UNREACHABLE() __builtin_unreachable()
    #elif GP_COMPILER_MSVC
        #define GP_BUILTIN_UNREACHABLE() __assume(false)
    #else
        #define GP_BUILTIN_UNREACHABLE() (void(0))
    #endif
#endif

/// @brief Safe wrapper around __has_builtin.
///        Evaluates to 1 if the named built-in is available, 0 otherwise.
#if defined(__has_builtin)
    #define GP_HAS_BUILTIN(x) __has_builtin(x)
#else
    #define GP_HAS_BUILTIN(x) 0
#endif

/// @brief Marks a function as cold (rarely called).
///        The optimiser de-prioritises cold functions during inlining and code placement.
#if !defined(GP_COLD)
    #if GP_COMPILER_MSVC
        #define GP_COLD __declspec(noinline)
    #elif GP_COMPILER_CLANG || GP_COMPILER_GCC || GP_COMPILER_INTEL
        #define GP_COLD __attribute__((cold))
    #else
        #define GP_COLD
    #endif
#endif

/// @section Cache-line size.

#ifndef GP_PLATFORM_CACHE_LINE_SIZE
    #define GP_PLATFORM_CACHE_LINE_SIZE 64
#endif

/// @section Assertion macros.

/// @brief GP_ASSERT(expr), runtime assertion, breaks into the debugger if expr is false.
#ifndef GP_ASSERT
    #define GP_ASSERT(expr, ...)
#endif

/// @section Warning suppression helpers.
///
/// Use these to bracket third-party headers or intentional warning-generating code.
/// Always prefer fixing the root cause over suppressing warnings.
///
/// MSVC example:
///   GP_MSVC_WARNING_PUSH()
///   GP_MSVC_WARNING_DISABLE(4100)   // unreferenced formal parameter
///   #include <some_library.h>
///   GP_MSVC_WARNING_POP()
///
/// GCC/Clang example:
///   GP_GCC_WARNING_PUSH()
///   GP_GCC_WARNING_DISABLE("-Wunused-parameter")
///   #include <some_library.h>
///   GP_GCC_WARNING_POP()

#if GP_COMPILER_MSVC
    #define GP_MSVC_WARNING_PUSH()          __pragma(warning(push))
    #define GP_MSVC_WARNING_POP()           __pragma(warning(pop))
    #define GP_MSVC_WARNING_DISABLE(code)   __pragma(warning(disable : code))
#else
    #define GP_MSVC_WARNING_PUSH()
    #define GP_MSVC_WARNING_POP()
    #define GP_MSVC_WARNING_DISABLE(code)
#endif

#if GP_COMPILER_CLANG || GP_COMPILER_GCC || GP_COMPILER_INTEL
    #define GP_GCC_WARNING_PUSH()           _Pragma("GCC diagnostic push")
    #define GP_GCC_WARNING_POP()            _Pragma("GCC diagnostic pop")
    #define GP_GCC_WARNING_DISABLE(w)       _Pragma(GP_STRINGIFY(GCC diagnostic ignored w))
#else
    #define GP_GCC_WARNING_PUSH()
    #define GP_GCC_WARNING_POP()
    #define GP_GCC_WARNING_DISABLE(w)
#endif

/// @section Intrinsic cast.

#ifndef GP_INTRINSIC_CAST
    #define GP_INTRINSIC_CAST
#endif

#include <cstddef>
#include <cstdint>

namespace gp
{

/// @section Integer types with fixed widths.
/// @see https://en.cppreference.com/w/cpp/types/integer

/// @brief 8-bit unsigned integer type.
///        limits: [0, 255]
using UInt8 = std::uint8_t;

/// @brief 16-bit unsigned integer type.
///        limits: [0, 65535]
using UInt16 = std::uint16_t;

/// @brief 32-bit unsigned integer type.
///        limits: [0, 4294967295]
using UInt32 = std::uint32_t;

/// @brief 64-bit unsigned integer type.
///        limits: [0, 18446744073709551615]
using UInt64 = std::uint64_t;

/// @brief 8-bit signed integer type.
///        limits: [-128, 127]
using Int8 = std::int8_t;

/// @brief 16-bit signed integer type.
///        limits: [-32768, 32767]
using Int16 = std::int16_t;

/// @brief 32-bit signed integer type.
///        limits: [-2147483648, 2147483647]
using Int32 = std::int32_t;

/// @brief 64-bit signed integer type.
///        limits: [-9223372036854775808, 9223372036854775807]
using Int64 = std::int64_t;

/// @section Floating-point types with fixed widths.

/// @brief 32-bit floating-point type (single precision).
///        Approximate limits: [1.18e-38, 3.4e+38]
using Float32 = float;

/// @brief 64-bit floating-point type (double precision).
///        Approximate limits: [2.22e-308, 1.8e+308]
using Float64 = double;

/// @section Size and pointer difference types.

using USize = std::size_t;
using ISize = std::ptrdiff_t;
using UIntPtr = std::uintptr_t;
using IntPtr = std::intptr_t;
using PtrDiff = std::ptrdiff_t;

/// @section Character types.

using Char8 = char8_t;
using Char16 = char16_t;
using Char32 = char32_t;
using Char = char;
using AnsiChar = char;
using WChar = wchar_t;

/// @section Special values.

using Byte = std::byte;
using NullPtrT = std::nullptr_t;

}   // namespace gp

namespace
{

static_assert(char(-1) < char(0), "Unsigned char type test failed.");

static_assert(!std::is_same_v<gp::AnsiChar, gp::WChar>, "AnsiChar and WChar types should be different.");

static_assert(sizeof(gp::UInt8) == 1, "UInt8 type size test failed.");
static_assert(gp::Int32(gp::UInt8(-1)) == 0xFF, "UInt8 type sign test failed.");

static_assert(sizeof(gp::UInt16) == 2, "UInt16 type size test failed.");
static_assert(gp::Int32(gp::UInt16(-1)) == 0xFFFF, "UInt16 type sign test failed.");

static_assert(sizeof(gp::UInt32) == 4, "UInt32 type size test failed.");
static_assert(gp::Int64(gp::UInt32(-1)) == gp::Int64(0xFFFFFFFF), "UInt32 type sign test failed.");

static_assert(sizeof(gp::UInt64) == 8, "UInt64 type size test failed.");
static_assert(gp::UInt64(-1) > gp::UInt64(0), "UInt64 type sign test failed.");

static_assert(sizeof(gp::Int8) == 1, "Int8 type size test failed.");
static_assert(gp::Int32(gp::Int8(-1)) == -1, "Int8 type sign test failed.");

static_assert(sizeof(gp::Int16) == 2, "Int16 type size test failed.");
static_assert(gp::Int32(gp::Int16(-1)) == -1, "Int16 type sign test failed.");

static_assert(sizeof(gp::Int32) == 4, "Int32 type size test failed.");
static_assert(gp::Int64(gp::Int32(-1)) == gp::Int64(-1), "Int32 type sign test failed.");

static_assert(sizeof(gp::Int64) == 8, "Int64 type size test failed.");
static_assert(gp::Int64(-1) < gp::Int64(0), "Int64 type sign test failed.");

}   // namespace
