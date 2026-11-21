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

/// @section Linkage, packing, and optimisation hints.

/// @brief Marks a global variable or static member with weak/selectany linkage across translation units.
#define GP_SELECT_ANY __declspec(selectany)

/// @brief Forces a struct/class to be packed with no padding bytes.
/// @note  Wraps the struct on MSVC; for per-struct packing prefer `#pragma pack(push, 1)` / `#pragma pack(pop)`.
#define GP_PACKED __pragma(pack(push, 1)) /* struct body */ __pragma(pack(pop))

/// @brief Marks a code path as unreachable to allow dead-branch elimination.
#define GP_BUILTIN_UNREACHABLE() __assume(false)

/// @brief Marks a function as cold (de-prioritised during inlining and code placement).
#define GP_COLD __declspec(noinline)

/// @section Warning suppression & third-party include brackets.

/// @brief Pushes the current MSVC warning state onto the compiler stack.
#define GP_MSVC_WARNING_PUSH()        __pragma(warning(push))

/// @brief Restores the previously pushed MSVC warning state.
#define GP_MSVC_WARNING_POP()         __pragma(warning(pop))

/// @brief Disables a specific MSVC warning number until `GP_MSVC_WARNING_POP()`.
/// @param[in] code Four-digit MSVC warning number.
#define GP_MSVC_WARNING_DISABLE(code) __pragma(warning(disable : code))

/// @brief Suppresses deprecation warnings (`C4996`) until `GP_ENABLE_DEPRECATION_WARNINGS`.
#define GP_DISABLE_DEPRECATION_WARNINGS \
    GP_MSVC_WARNING_PUSH()              \
    GP_MSVC_WARNING_DISABLE(4996)

/// @brief Restores deprecation warning diagnostics pushed by `GP_DISABLE_DEPRECATION_WARNINGS`.
#define GP_ENABLE_DEPRECATION_WARNINGS GP_MSVC_WARNING_POP()

/// @brief Brackets external or OS SDK headers to suppress noisy warnings without relaxing engine warning levels.
#define GP_THIRD_PARTY_INCLUDES_START \
    GP_MSVC_WARNING_PUSH()            \
    GP_MSVC_WARNING_DISABLE(4100)     \
    GP_MSVC_WARNING_DISABLE(4127)     \
    GP_MSVC_WARNING_DISABLE(4201)     \
    GP_MSVC_WARNING_DISABLE(4324)     \
    GP_MSVC_WARNING_DISABLE(4458)     \
    GP_MSVC_WARNING_DISABLE(4996)

/// @brief Restores compiler warning state pushed by `GP_THIRD_PARTY_INCLUDES_START`.
#define GP_THIRD_PARTY_INCLUDES_END GP_MSVC_WARNING_POP()
