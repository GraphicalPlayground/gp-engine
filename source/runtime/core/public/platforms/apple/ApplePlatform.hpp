// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "miscellaneous/PreProcessorUtilities.hpp"

#define GP_PLATFORM_SUPPORTS_TEXTURE_STREAMING          GP_TRUE
#define GP_PLATFORM_SUPPORTS_STACK_SYMBOLS              GP_TRUE
#define GP_PLATFORM_IS_ANSI_MALLOC_THREADSAFE           GP_TRUE

#define GP_PLATFORM_BREAK() __builtin_debugtrap()

#define GP_VARARGS
#define GP_CDECL
#define GP_STDCALL
#define GP_FORCENOINLINE __attribute__((noinline))
#define GP_FUNCTION_CHECK_RETURN_END __attribute__ ((warn_unused_result))
#define GP_FUNCTION_NO_RETURN_END __attribute__ ((noreturn))

#define GP_ABSTRACT abstract

#define GP_DLLEXPORT __attribute__((visibility("default")))
#define GP_DLLIMPORT __attribute__((visibility("default")))
