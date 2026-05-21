// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

/// @section GCC compiler version helpers.

#define GP_COMPILER_VERSION_MAJOR   __GNUC__
#define GP_COMPILER_VERSION_MINOR   __GNUC_MINOR__
#define GP_COMPILER_VERSION_PATCH   __GNUC_PATCHLEVEL__

/// @section Stack / frame intrinsics.

#define GP_PLATFORM_RETURN_ADDRESS()         __builtin_return_address(0)
#define GP_PLATFORM_RETURN_ADDRESS_POINTER() __builtin_frame_address(0)

/// @section Annotation attributes.

/// @brief Suppresses debug info generation for a function (GCC 7+).
#define GP_NODEBUG __attribute__((nodebug))

/// @brief Marks a reference/pointer parameter or return value as lifetime-bound.
/// @note  GCC does not have a direct [[lifetimebound]] equivalent; leave empty so
///        the caller still compiles correctly without the safety annotation.
#define GP_LIFETIMEBOUND

/// @section Allocation attributes.

/// @brief Marks a function as an allocator (enables alias analysis optimisations).
#define GP_ALLOCATION_FUNCTION_0()           __attribute__((malloc))
#define GP_ALLOCATION_FUNCTION_1(size)       __attribute__((malloc, alloc_size(size)))
#define GP_ALLOCATION_FUNCTION_2(size, align) __attribute__((malloc, alloc_size(size)))
#define GP_ALLOCATION_FUNCTION_X(x, size, align, func, ...) func
#define GP_ALLOCATION_FUNCTION(...) \
    GP_ALLOCATION_FUNCTION_X(__VA_OPT__(,) __VA_ARGS__, \
        GP_ALLOCATION_FUNCTION_2(__VA_ARGS__), \
        GP_ALLOCATION_FUNCTION_1(__VA_ARGS__), \
        GP_ALLOCATION_FUNCTION_0(__VA_ARGS__))

/// @brief Marks a function as guaranteed to return a non-null pointer.
#define GP_FUNCTION_NON_NULL_RETURN_START __attribute__((returns_nonnull))

/// @section Inlining control.

/// @brief Forces inlining of a function at every call site.
#define GP_CALLSITE_FORCEINLINE __attribute__((always_inline))

/// @brief Forces inlining of the function body AND all of its callees (GCC 4.6+).
#define GP_FLATTEN __attribute__((flatten))
