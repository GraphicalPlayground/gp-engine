// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "miscellaneous/PreProcessorUtilities.hpp"

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
#if defined(_MSC_VER)
    #define GP_NODEBUG __declspec(noinline)
#else
    #define GP_NODEBUG [[clang::nodebug]]
#endif

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

/// @section Linkage, packing, and optimisation hints.

/// @brief Marks a global variable or static member with weak/selectany linkage across translation units.
#define GP_SELECT_ANY __attribute__((weak))

/// @brief Forces a struct/class to be packed with no padding bytes.
#define GP_PACKED __attribute__((packed))

/// @brief Marks a code path as unreachable to allow dead-branch elimination.
#define GP_BUILTIN_UNREACHABLE() __builtin_unreachable()

/// @brief Marks a function as cold (de-prioritised during inlining and code placement).
#define GP_COLD __attribute__((cold))

/// @section Thread safety static analysis attributes (Clang -Wthread-safety).

/// @brief Declares that a class or struct represents a synchronization capability (e.g. `"mutex"`).
#define GP_THREAD_CAPABILITY(name) __attribute__((capability(name)))

/// @brief Declares that a class acts as an RAII scoped guard acquiring and releasing a capability.
#define GP_SCOPED_THREAD_CAPABILITY __attribute__((scoped_lockable))

/// @brief Annotates a member variable as protected by the specified capability (`m_mutex`).
#define GP_GUARDED_BY(cap) __attribute__((guarded_by(cap)))

/// @brief Annotates a pointer member whose pointee data is protected by the specified capability.
#define GP_PT_GUARDED_BY(cap) __attribute__((pt_guarded_by(cap)))

/// @brief Declares that a function acquires exclusive ownership of the specified capability.
#define GP_ACQUIRE_CAPABILITY(...) __attribute__((acquire_capability(__VA_ARGS__)))

/// @brief Declares that a function acquires shared (reader) ownership of the specified capability.
#define GP_ACQUIRE_SHARED_CAPABILITY(...) __attribute__((acquire_shared_capability(__VA_ARGS__)))

/// @brief Declares that a function attempts to acquire exclusive ownership and returns a boolean success indicator.
#define GP_TRY_ACQUIRE_CAPABILITY(...) __attribute__((try_acquire_capability(__VA_ARGS__)))

/// @brief Declares that a function attempts to acquire shared ownership and returns a boolean success indicator.
#define GP_TRY_ACQUIRE_SHARED_CAPABILITY(...) __attribute__((try_acquire_shared_capability(__VA_ARGS__)))

/// @brief Declares that a function releases exclusive ownership of the specified capability.
#define GP_RELEASE_CAPABILITY(...) __attribute__((release_capability(__VA_ARGS__)))

/// @brief Declares that a function releases shared ownership of the specified capability.
#define GP_RELEASE_SHARED_CAPABILITY(...) __attribute__((release_shared_capability(__VA_ARGS__)))

/// @brief Declares that a function releases a generic capability (exclusive or shared).
#define GP_RELEASE_GENERIC_CAPABILITY(...) __attribute__((release_generic_capability(__VA_ARGS__)))

/// @brief Declares that the caller must already hold exclusive ownership of the specified capability.
#define GP_REQUIRES_CAPABILITY(...) __attribute__((requires_capability(__VA_ARGS__)))

/// @brief Declares that the caller must already hold shared ownership of the specified capability.
#define GP_REQUIRES_SHARED_CAPABILITY(...) __attribute__((requires_shared_capability(__VA_ARGS__)))

/// @brief Declares that the caller must not hold the specified capability on entry (prevents self-deadlock).
#define GP_EXCLUDES_CAPABILITY(...) __attribute__((locks_excluded(__VA_ARGS__)))

/// @brief Disables Clang thread-safety analysis for a specific function body (e.g. custom lock internals).
#define GP_NO_THREAD_SAFETY_ANALYSIS __attribute__((no_thread_safety_analysis))

/// @section Warning suppression & third-party include brackets.

/// @brief Pushes the current Clang/GCC diagnostic state onto the compiler stack.
#define GP_GCC_WARNING_PUSH()     _Pragma("GCC diagnostic push")

/// @brief Restores the previously pushed Clang/GCC diagnostic state.
#define GP_GCC_WARNING_POP()      _Pragma("GCC diagnostic pop")

/// @brief Disables a specific `-W...` diagnostic flag until `GP_GCC_WARNING_POP()`.
/// @param[in] w Quoted warning flag literal, e.g. `"-Wunused-parameter"`.
#define GP_GCC_WARNING_DISABLE(w) _Pragma(GP_STRINGIFY(GCC diagnostic ignored w))

/// @brief Suppresses deprecation warnings (`-Wdeprecated-declarations`) until `GP_ENABLE_DEPRECATION_WARNINGS`.
#define GP_DISABLE_DEPRECATION_WARNINGS \
    GP_GCC_WARNING_PUSH()               \
    GP_GCC_WARNING_DISABLE("-Wdeprecated-declarations")

/// @brief Restores deprecation warning diagnostics pushed by `GP_DISABLE_DEPRECATION_WARNINGS`.
#define GP_ENABLE_DEPRECATION_WARNINGS GP_GCC_WARNING_POP()

/// @brief Brackets external or OS SDK headers to suppress noisy warnings without relaxing engine warning levels.
#define GP_THIRD_PARTY_INCLUDES_START                     \
    GP_GCC_WARNING_PUSH()                                 \
    GP_GCC_WARNING_DISABLE("-Wdeprecated-declarations")   \
    GP_GCC_WARNING_DISABLE("-Wunused-parameter")          \
    GP_GCC_WARNING_DISABLE("-Wshadow")                    \
    GP_GCC_WARNING_DISABLE("-Wmissing-field-initializers") \
    GP_GCC_WARNING_DISABLE("-Wgnu-anonymous-struct")      \
    GP_GCC_WARNING_DISABLE("-Wnested-anon-types")

/// @brief Restores compiler warning state pushed by `GP_THIRD_PARTY_INCLUDES_START`.
#define GP_THIRD_PARTY_INCLUDES_END GP_GCC_WARNING_POP()
