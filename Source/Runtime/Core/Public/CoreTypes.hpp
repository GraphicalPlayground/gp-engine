// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"

namespace GP
{

// clang-format off

typedef signed char        Int8;
typedef short              Int16;
typedef int                Int32;
typedef long long          Int64;
typedef unsigned char      UInt8;
typedef unsigned short     UInt16;
typedef unsigned int       UInt32;
typedef unsigned long long UInt64;

typedef float              Float32;
typedef double             Float64;

typedef char               Char8;
typedef char16_t           Char16;
typedef char32_t           Char32;
typedef wchar_t            WideChar;
#if GP_INTERNAL_CXX20
typedef char8_t            UChar8;      // Native C++20 uchar8
#else
typedef unsigned char      UChar8;
#endif

typedef unsigned char      Byte;
typedef decltype(nullptr)  NullPtrT;
typedef double             MaxAlignT;   // Most aligned type

#if GP_ARCHITECTURE_64BIT
typedef unsigned long long SizeT;
#if GP_COMPILER_MSVC
typedef long long          SSizeT;
#else
typedef signed long long   SSizeT;
#endif
#elif GP_ARCHITECTURE_32BIT
typedef unsigned int       SizeT;
typedef signed int         SSizeT;
#else
    // Fallback for compilers that provide built-in types
    #if defined(__SIZE_TYPE__)
typedef __SIZE_TYPE__      SizeT;
    #elif GP_PLATFORM_WINDOWS
typedef unsigned __int64   SizeT;
    #else
typedef unsigned long      SizeT;
    #endif

    #if defined(__PTRDIFF_TYPE__)
typedef __PTRDIFF_TYPE__   SSizeT;
    #elif GP_PLATFORM_WINDOWS
typedef __int64            SSizeT;
    #else
typedef signed long        SSizeT;
    #endif
#endif

typedef SSizeT             OffsetT;
typedef SSizeT             PtrDiffT;

#if GP_ARCHITECTURE_64BIT
typedef unsigned long long UIntPtrT;
typedef signed long long   IntPtrT;
#elif GP_ARCHITECTURE_32BIT
typedef unsigned int       UIntPtrT;
typedef signed int         IntPtrT;
#else
    // Fallback for compilers that provide built-in types
    #if defined(__UINTPTR_TYPE__)
typedef __UINTPTR_TYPE__   UIntPtrT;
    #else
typedef unsigned long      UIntPtrT;
    #endif

    #if defined(__INTPTR_TYPE__)
typedef __INTPTR_TYPE__    IntPtrT;
    #else
typedef signed long        IntPtrT;
    #endif
#endif

static_assert(sizeof(Int8) == 1, "Int8 must be 1 byte");
static_assert(sizeof(Int16) == 2, "Int16 must be 2 bytes");
static_assert(sizeof(Int32) == 4, "Int32 must be 4 bytes");
static_assert(sizeof(Int64) == 8, "Int64 must be 8 bytes");
static_assert(sizeof(Float32) == 4, "Float32 must be 4 bytes");
static_assert(sizeof(Float64) == 8, "Float64 must be 8 bytes");

#if GP_ARCHITECTURE_64BIT
static_assert(sizeof(SizeT) == 8, "Size must be 8 bytes on 64-bit platforms");
#else
static_assert(sizeof(SizeT) == 4, "Size must be 4 bytes on 32-bit platforms");
#endif

#if GP_ENABLE_DOUBLE_PRECISION
using Real = Float64;
#else
using Real = Float32;
#endif

// clang-format on

}   // namespace GP
