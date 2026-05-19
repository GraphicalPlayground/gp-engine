// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#define GP_PLATFORM_RETURN_ADDRESS() __builtin_return_address(0)
#define GP_PLATFORM_RETURN_ADDRESS_POINTER() __builtin_frame_address(0)

#define GP_LIFETIMEBOUND [[clang::lifetimebound]]

#define GP_NODEBUG [[gnu::nodebug]]

#define GP_ALLOCATION_FUNCTION_0() [[gnu::malloc]]
#define GP_ALLOCATION_FUNCTION_1(size) [[gnu::malloc, gnu::alloc_size(size)]]

#define GP_ALLOCATION_FUNCTION_2(size, align) [[gnu::malloc, gnu::alloc_size(size)]]
#define GP_ALLOCATION_FUNCTION_X(x, size, align, func, ...) func
#define GP_ALLOCATION_FUNCTION(...) GP_ALLOCATION_FUNCTION_X(,##__VA_ARGS__, GP_ALLOCATION_FUNCTION_2(__VA_ARGS__), GP_ALLOCATION_FUNCTION_1(__VA_ARGS__), GP_ALLOCATION_FUNCTION_0(__VA_ARGS__))

#define GP_FUNCTION_NON_NULL_RETURN_START [[gnu::returns_nonnull]]

#define GP_CALLSITE_FORCEINLINE [[clang::always_inline]]
