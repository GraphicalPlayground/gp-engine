// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

/// @section Clang compiler version helpers.

#define GP_COMPILER_VERSION_MAJOR   __clang_major__
#define GP_COMPILER_VERSION_MINOR   __clang_minor__
#define GP_COMPILER_VERSION_PATCH   __clang_patchlevel__

/// @section Stack / frame intrinsics.

#define GP_PLATFORM_RETURN_ADDRESS()         __builtin_return_address(0)
#define GP_PLATFORM_RETURN_ADDRESS_POINTER() __builtin_frame_address(0)

/// @section Annotation attributes.

/// @brief Marks a reference/pointer parameter or return value as lifetime-bound.
///        Triggers a warning if the annotated reference outlives the object it refers to.
#define GP_LIFETIMEBOUND [[clang::lifetimebound]]

/// @brief Suppresses debug-info generation for a function in debug builds.
///        Useful for thin wrappers that would otherwise pollute stepping in the debugger.
#define GP_NODEBUG [[clang::nodebug]]

/// @section Allocation attributes.

/// @brief Marks a function as an allocator (enables alias-analysis optimisations).
///        Optional: pass the size argument index (1-based) and optionally the alignment index.
#define GP_ALLOCATION_FUNCTION_0()            [[gnu::malloc]]
#define GP_ALLOCATION_FUNCTION_1(size)        [[gnu::malloc, gnu::alloc_size(size)]]
#define GP_ALLOCATION_FUNCTION_2(size, align) [[gnu::malloc, gnu::alloc_size(size)]]
#define GP_ALLOCATION_FUNCTION_X(x, size, align, func, ...) func
#define GP_ALLOCATION_FUNCTION(...) \
    GP_ALLOCATION_FUNCTION_X(__VA_OPT__(,) __VA_ARGS__, \
        GP_ALLOCATION_FUNCTION_2(__VA_ARGS__), \
        GP_ALLOCATION_FUNCTION_1(__VA_ARGS__), \
        GP_ALLOCATION_FUNCTION_0(__VA_ARGS__))

/// @brief Marks a function as guaranteed to return a non-null pointer.
#define GP_FUNCTION_NON_NULL_RETURN_START [[gnu::returns_nonnull]]

/// @section Inlining control.

/// @brief Forces inlining of a function at every call site regardless of optimisation level.
#define GP_CALLSITE_FORCEINLINE [[clang::always_inline]]

/// @brief Forces inlining of the function body AND all of its callees.
///        Use with care: can dramatically increase code size.
#define GP_FLATTEN __attribute__((flatten))
