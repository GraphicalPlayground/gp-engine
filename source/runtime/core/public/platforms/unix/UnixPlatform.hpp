// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "miscellaneous/PreProcessorUtilities.hpp"
#include <linux/version.h>
#include <signal.h>

#define _GP_IS_PLATFORM_64BIT                           GP_TRUE

#define GP_PLATFORM_SUPPORTS_MIMALLOC                   _GP_IS_PLATFORM_64BIT
#define GP_PLATFORM_IS_ANSI_MALLOC_THREADSAFE           GP_TRUE
#define GP_PLATFORM_USE_ANSI_POSIX_MALLOC               GP_TRUE
#define GP_PLATFORM_SUPPORTS_BINDLESS_RENDERING         GP_TRUE
#define GP_PLATFORM_SUPPORTS_MESH_SHADERS               GP_TRUE

#if GP_ARCHITECTURE_X86_FAMILY
    #define GP_PLATFORM_BREAK() { __asm__ volatile("int $0x03"); }
#else
    #define GP_PLATFORM_BREAK() raise(SIGTRAP)
#endif

#define GP_VARARGS
#define GP_CDECL
#define GP_STDCALL
#if GP_BUILD_DEBUG
    #define GP_FORCEINLINE inline
#else
    #define GP_FORCEINLINE inline __attribute__ ((always_inline))
#endif
#define GP_FORCENOINLINE __attribute__((noinline))
#define GP_FUNCTION_CHECK_RETURN_END __attribute__ ((warn_unused_result))
#define GP_FUNCTION_NO_RETURN_END __attribute__ ((noreturn))

#define GP_ABSTRACT abstract

#if __has_feature(cxx_noexcept)
    #define GP_OPERATOR_NEW_THROW_SPEC
#else
    #define GP_OPERATOR_NEW_THROW_SPEC throw (std::bad_alloc)
#endif
#define GP_OPERATOR_DELETE_THROW_SPEC noexcept
#define GP_OPERATOR_NEW_NOTHROW_SPEC noexcept
#define GP_OPERATOR_DELETE_NOTHROW_SPEC noexcept

#define GP_DLLEXPORT __attribute__((visibility("default")))
#define GP_DLLIMPORT __attribute__((visibility("default")))
