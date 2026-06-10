// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

/// @section CPU Zones

#define GP_PROFILE_SCOPE()
#define GP_PROFILE_SCOPE_N(name)
#define GP_PROFILE_SCOPE_C(color)
#define GP_PROFILE_SCOPE_NC(name, color)
#define GP_PROFILE_FUNCTION()

#define GP_PROFILE_ZONE_TEXT(text, size)
#define GP_PROFILE_ZONE_NAME(text, size)
#define GP_PROFILE_ZONE_VALUE(val)
#define GP_PROFILE_ZONE_COLOR(color)

/// @section Frame Markers

#define GP_FRAME_MARK()
#define GP_FRAME_MARK_N(name)
#define GP_FRAME_MARK_START(name)
#define GP_FRAME_MARK_END(name)

/// @section Memory Tracking

#define GP_MEM_ALLOC(ptr, size)
#define GP_MEM_FREE(ptr)
#define GP_MEM_ALLOC_N(ptr, size, pool)
#define GP_MEM_FREE_N(ptr, pool)

/// @section Messages / Markers

#define GP_MESSAGE(text, size)
#define GP_MESSAGE_L(literal)
#define GP_MESSAGE_C(text, size, color)
#define GP_MESSAGE_LC(literal, color)

/// @section Plots / Graphs

#define GP_PLOT(name, val)
#define GP_PLOT_CONFIG(name, type, step, fill, color)

/// @section Threading

#define GP_SET_THREAD_NAME(name)

/// @section Application Info

#define GP_APP_INFO(text, size)

/// @section GPU Zones

#define GP_GPU_SCOPE(name)
#define GP_GPU_SCOPE_C(name, color)
#define GP_GPU_SCOPE_CTX(ctx, name)
#define GP_GPU_SCOPE_CTX_C(ctx, name, color)
#define GP_GPU_MARKER(ctx, color, name)
#define GP_GPU_COLLECT()

/// @section Locks / Mutexes

#define GP_LOCKABLE(type, var)
#define GP_LOCKABLE_N(type, var, desc)
