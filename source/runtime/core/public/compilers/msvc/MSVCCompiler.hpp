// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

/// @section MSVC compiler version helpers.
/// @see https://learn.microsoft.com/en-us/cpp/preprocessor/predefined-macros

#define GP_COMPILER_VERSION_MAJOR   (_MSC_FULL_VER / 10000000)
#define GP_COMPILER_VERSION_MINOR   (_MSC_FULL_VER / 100000 % 100)
#define GP_COMPILER_VERSION_PATCH   (_MSC_FULL_VER % 100000)

/// @section Stack / frame intrinsics.

#define GP_PLATFORM_RETURN_ADDRESS()         _ReturnAddress()
#define GP_PLATFORM_RETURN_ADDRESS_POINTER() _AddressOfReturnAddress()

/// @section Annotation attributes.

/// @brief Marks a reference/pointer parameter or return value as lifetime-bound.
///        Available since VS 2019 16.9 (_MSC_VER >= 1929).
#ifdef __has_cpp_attribute
    #if __has_cpp_attribute(msvc::lifetimebound)
        #define GP_LIFETIMEBOUND [[msvc::lifetimebound]]
    #endif
#endif
#ifndef GP_LIFETIMEBOUND
    #define GP_LIFETIMEBOUND
#endif

/// @brief MSVC has no equivalent of [[gnu::nodebug]]; map to empty.
#define GP_NODEBUG

/// @section Allocation attributes.

/// @brief MSVC exposes allocation size hints via SAL annotations.
///        Map to SAL _Ret_notnull_ for the no-argument form; richer SAL forms
///        (_Post_writable_byte_size_) are available but require including <sal.h>.
#define GP_ALLOCATION_FUNCTION_0()            __declspec(restrict)
#define GP_ALLOCATION_FUNCTION_1(size)        __declspec(restrict)
#define GP_ALLOCATION_FUNCTION_2(size, align) __declspec(restrict)
#define GP_ALLOCATION_FUNCTION_X(x, size, align, func, ...) func
#define GP_ALLOCATION_FUNCTION(...) \
    GP_ALLOCATION_FUNCTION_X(__VA_OPT__(,) __VA_ARGS__, \
        GP_ALLOCATION_FUNCTION_2(__VA_ARGS__), \
        GP_ALLOCATION_FUNCTION_1(__VA_ARGS__), \
        GP_ALLOCATION_FUNCTION_0(__VA_ARGS__))

/// @brief MSVC has no direct returns_nonnull; map to empty.
///        _Ret_notnull_ is the SAL equivalent but requires including <sal.h>.
#define GP_FUNCTION_NON_NULL_RETURN_START

/// @section Inlining control.

/// @brief Forces inlining of a function at every call site (__forceinline is already
///        defined for the function itself via GP_FORCEINLINE; this variant targets the
///        call-site hint used in wrappers where the callee is not under our control).
#define GP_CALLSITE_FORCEINLINE __forceinline

/// @brief MSVC has no flatten equivalent; fall back to the per-function forceinline.
#define GP_FLATTEN __forceinline
