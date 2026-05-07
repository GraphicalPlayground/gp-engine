// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "miscellaneous/BuildDefines.hpp"   // IWYU pragma: export
#include "miscellaneous/PreProcessorUtilities.hpp"
#include <type_traits>

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

#if (GP_COMPILER_MSVC && _MSC_VER < 1920) || (!GP_COMPILER_MSVC && !defined(__cpp_if_constexpr))
    #error "Compiler is expected to support `if constexpr` statements."
#endif

#if !defined(__cpp_fold_expressions)
    #error "Compiler is expected to support fold expressions."
#endif

#if !__has_feature(cxx_decltype_auto)
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
#ifndef GP_FORCEINLINE
    #define GP_FORCEINLINE
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

#ifndef GP_DEFINE_FORCEINLINE_HINT_TO_INLINE
    #define GP_DEFINE_FORCEINLINE_HINT_TO_INLINE GP_FALSE
#endif

#if GP_DEFINE_FORCEINLINE_HINT_TO_INLINE
    #define GP_FORCEINLINE_HINT inline
#else
    #define GP_FORCEINLINE_HINT GP_FORCEINLINE
#endif

#if !defined(GP_NODISCARD) && defined(__has_cpp_attribute)
    #if __has_cpp_attribute(nodiscard)
        #if (defined(_MSC_VER) && _MSC_VER >= 1924) || (defined(__clang__) && __clang_major__ >= 10)
            #define GP_NODISCARD [[nodiscard]]
        #endif
    #endif
#endif
#ifndef GP_NODISCARD
    #define GP_NODISCARD
#endif

#if !defined(GP_MAYBE_UNUSED) && defined(__has_cpp_attribute)
    #if __has_cpp_attribute(maybe_unused)
        #define GP_MAYBE_UNUSED [[maybe_unused]]
    #endif
#endif
#ifndef GP_MAYBE_UNUSED
    #define GP_MAYBE_UNUSED
#endif

#ifndef GP_SELECT_ANY
    #if GP_COMPILER_MSVC
        #define GP_SELECT_ANY __declspec(selectany)
    #else
        #define GP_SELECT_ANY __attribute__((selectany))
    #endif
#endif

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

#ifndef GP_FUNCTION_NON_NULL_RETURN_START
    #define GP_FUNCTION_NON_NULL_RETURN_START
#endif
#ifndef GP_FUNCTION_NON_NULL_RETURN_END
    #define GP_FUNCTION_NON_NULL_RETURN_END
#endif

#ifndef GP_LIFETIMEBOUND
    #define GP_LIFETIMEBOUND
#endif

#ifndef GP_NODEBUG
    #define GP_NODEBUG
#endif

#ifndef GP_ALLOCATION_FUNCTION
    #define GP_ALLOCATION_FUNCTION(...)
#endif

#ifndef GP_ASSUME
    #if GP_COMPILER_CLANG
        #define GP_ASSUME(x) __builtin_assume(x)
    #elif GP_COMPILER_MSVC
        #define GP_ASSUME(x) __assume(x)
    #else
        #define GP_ASSUME(x)
    #endif
#endif

#ifndef GP_INTRINSIC_CAST
    #define GP_INTRINSIC_CAST
#endif

#ifndef GP_LIKELY
    #if GP_COMPILER_CLANG || GP_COMPILER_GCC
        #define GP_LIKELY(x) __builtin_expect(!!(x), 1)
    #else
        #define GP_LIKELY(x) (!!(x))
    #endif
#endif

#ifndef GP_UNLIKELY
    #if GP_COMPILER_CLANG || GP_COMPILER_GCC
        #define GP_UNLIKELY(x) __builtin_expect(!!(x), 0)
    #else
        #define GP_UNLIKELY(x) (!!(x))
    #endif
#endif

#ifndef GP_PLATFORM_CACHE_LINE_SIZE
    #define GP_PLATFORM_CACHE_LINE_SIZE 64
#endif

#if !defined(GP_COLD)
    #if GP_COMPILER_MSVC
        #define GP_COLD __declspec(noinline)
    #elif GP_COMPILER_CLANG || GP_COMPILER_GCC
        #define GP_COLD __attribute__((cold))
    #else
        #define GP_COLD
    #endif
#endif

#ifndef GP_DLLEXPORT
    #define GP_DLLEXPORT
#endif
#ifndef GP_DLLIMPORT
    #define GP_DLLIMPORT
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
