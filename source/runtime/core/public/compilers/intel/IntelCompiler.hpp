// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "miscellaneous/PreProcessorUtilities.hpp"

// Intel has two distinct compiler families:
//   • ICC  (classic): defines __INTEL_COMPILER and __ICC.  GCC-ABI, supports most gnu:: attributes.
//   • ICX  (modern):  defines __INTEL_LLVM_COMPILER.  Built on LLVM/Clang; clang:: attributes work.
//
// Both are captured here so that the correct attribute set is selected at compile time.

#if defined(__INTEL_LLVM_COMPILER)

/// @section ICX (Intel oneAPI DPC++/C++, LLVM front-end).
///          Largely identical to Clang; reuse its attribute vocabulary.

    #define GP_COMPILER_VERSION_MAJOR   (__INTEL_LLVM_COMPILER / 10000)
    #define GP_COMPILER_VERSION_MINOR   (__INTEL_LLVM_COMPILER / 100 % 100)
    #define GP_COMPILER_VERSION_PATCH   (__INTEL_LLVM_COMPILER % 100)

    #define GP_PLATFORM_RETURN_ADDRESS()         __builtin_return_address(0)
    #define GP_PLATFORM_RETURN_ADDRESS_POINTER() __builtin_frame_address(0)

    #define GP_LIFETIMEBOUND                     [[clang::lifetimebound]]
    #define GP_NODEBUG                           [[clang::nodebug]]

    #define GP_ALLOCATION_FUNCTION_0()            [[gnu::malloc]]
    #define GP_ALLOCATION_FUNCTION_1(size)        [[gnu::malloc, gnu::alloc_size(size)]]
    #define GP_ALLOCATION_FUNCTION_2(size, align) [[gnu::malloc, gnu::alloc_size(size)]]
    #define GP_ALLOCATION_FUNCTION_X(x, size, align, func, ...) func
    #define GP_ALLOCATION_FUNCTION(...) \
        GP_ALLOCATION_FUNCTION_X(__VA_OPT__(,) __VA_ARGS__, \
            GP_ALLOCATION_FUNCTION_2(__VA_ARGS__), \
            GP_ALLOCATION_FUNCTION_1(__VA_ARGS__), \
            GP_ALLOCATION_FUNCTION_0(__VA_ARGS__))

    #define GP_FUNCTION_NON_NULL_RETURN_START    [[gnu::returns_nonnull]]
    #define GP_CALLSITE_FORCEINLINE              [[clang::always_inline]]
    #define GP_FLATTEN                           __attribute__((flatten))

    #define GP_SELECT_ANY                        __attribute__((weak))
    #define GP_PACKED                            __attribute__((packed))
    #define GP_BUILTIN_UNREACHABLE()             __builtin_unreachable()
    #define GP_COLD                              __attribute__((cold))

    #define GP_GCC_WARNING_PUSH()                _Pragma("GCC diagnostic push")
    #define GP_GCC_WARNING_POP()                 _Pragma("GCC diagnostic pop")
    #define GP_GCC_WARNING_DISABLE(w)            _Pragma(GP_STRINGIFY(GCC diagnostic ignored w))

    #define GP_DISABLE_DEPRECATION_WARNINGS \
        GP_GCC_WARNING_PUSH()               \
        GP_GCC_WARNING_DISABLE("-Wdeprecated-declarations")
    #define GP_ENABLE_DEPRECATION_WARNINGS GP_GCC_WARNING_POP()

    #define GP_THIRD_PARTY_INCLUDES_START                     \
        GP_GCC_WARNING_PUSH()                                 \
        GP_GCC_WARNING_DISABLE("-Wdeprecated-declarations")   \
        GP_GCC_WARNING_DISABLE("-Wunused-parameter")          \
        GP_GCC_WARNING_DISABLE("-Wshadow")                    \
        GP_GCC_WARNING_DISABLE("-Wmissing-field-initializers") \
        GP_GCC_WARNING_DISABLE("-Wgnu-anonymous-struct")      \
        GP_GCC_WARNING_DISABLE("-Wnested-anon-types")
    #define GP_THIRD_PARTY_INCLUDES_END GP_GCC_WARNING_POP()

#else   // Classic ICC

/// @section ICC (classic Intel C++ compiler, GCC ABI).

    #define GP_COMPILER_VERSION_MAJOR   (__INTEL_COMPILER / 100)
    #define GP_COMPILER_VERSION_MINOR   (__INTEL_COMPILER % 100 / 10)
    #define GP_COMPILER_VERSION_PATCH   (__INTEL_COMPILER % 10)

    #define GP_PLATFORM_RETURN_ADDRESS()         __builtin_return_address(0)
    #define GP_PLATFORM_RETURN_ADDRESS_POINTER() __builtin_frame_address(0)

    // ICC does not implement [[clang::lifetimebound]]; map to empty.
    #define GP_LIFETIMEBOUND
    // ICC does not implement [[gnu::nodebug]]; map to empty.
    #define GP_NODEBUG

    #define GP_ALLOCATION_FUNCTION_0()            __attribute__((malloc))
    #define GP_ALLOCATION_FUNCTION_1(size)        __attribute__((malloc, alloc_size(size)))
    #define GP_ALLOCATION_FUNCTION_2(size, align) __attribute__((malloc, alloc_size(size)))
    #define GP_ALLOCATION_FUNCTION_X(x, size, align, func, ...) func
    #define GP_ALLOCATION_FUNCTION(...) \
        GP_ALLOCATION_FUNCTION_X(__VA_OPT__(,) __VA_ARGS__, \
            GP_ALLOCATION_FUNCTION_2(__VA_ARGS__), \
            GP_ALLOCATION_FUNCTION_1(__VA_ARGS__), \
            GP_ALLOCATION_FUNCTION_0(__VA_ARGS__))

    #define GP_FUNCTION_NON_NULL_RETURN_START    __attribute__((returns_nonnull))
    #define GP_CALLSITE_FORCEINLINE              __attribute__((always_inline))
    // ICC supports __attribute__((flatten)) since version 19.1.
    #define GP_FLATTEN                           __attribute__((flatten))

    #define GP_SELECT_ANY                        __attribute__((weak))
    #define GP_PACKED                            __attribute__((packed))
    #define GP_BUILTIN_UNREACHABLE()             __builtin_unreachable()
    #define GP_COLD                              __attribute__((cold))

    #define GP_GCC_WARNING_PUSH()                _Pragma("GCC diagnostic push")
    #define GP_GCC_WARNING_POP()                 _Pragma("GCC diagnostic pop")
    #define GP_GCC_WARNING_DISABLE(w)            _Pragma(GP_STRINGIFY(GCC diagnostic ignored w))

    #define GP_DISABLE_DEPRECATION_WARNINGS \
        GP_GCC_WARNING_PUSH()               \
        GP_GCC_WARNING_DISABLE("-Wdeprecated-declarations")
    #define GP_ENABLE_DEPRECATION_WARNINGS GP_GCC_WARNING_POP()

    #define GP_THIRD_PARTY_INCLUDES_START                     \
        GP_GCC_WARNING_PUSH()                                 \
        GP_GCC_WARNING_DISABLE("-Wdeprecated-declarations")   \
        GP_GCC_WARNING_DISABLE("-Wunused-parameter")          \
        GP_GCC_WARNING_DISABLE("-Wshadow")                    \
        GP_GCC_WARNING_DISABLE("-Wmissing-field-initializers")
    #define GP_THIRD_PARTY_INCLUDES_END GP_GCC_WARNING_POP()

#endif   // __INTEL_LLVM_COMPILER
