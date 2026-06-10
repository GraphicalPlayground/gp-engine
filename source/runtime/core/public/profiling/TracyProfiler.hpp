// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep

#ifndef TRACY_ENABLE
    #define TRACY_ENABLE
#endif

#include <tracy/Tracy.hpp>
#include <tracy/TracyC.h>
#ifndef GP_TRACY_HAS_GPU
    #define GP_TRACY_HAS_GPU GP_FALSE
#endif

/// @section CPU Zones

#define GP_PROFILE_SCOPE() ZoneScoped
#define GP_PROFILE_SCOPE_N(name) ZoneScopedN(name)
#define GP_PROFILE_SCOPE_C(color) ZoneScopedC(color)
#define GP_PROFILE_SCOPE_NC(name, color) ZoneScopedNC(name, color)
#define GP_PROFILE_FUNCTION() ZoneScoped

#define GP_PROFILE_ZONE_TEXT(text, size) ZoneText(text, size)
#define GP_PROFILE_ZONE_NAME(text, size) ZoneName(text, size)
#define GP_PROFILE_ZONE_VALUE(val) ZoneValue(val)
#define GP_PROFILE_ZONE_COLOR(color) ZoneColor(color)

/// @section Frame Markers

#define GP_FRAME_MARK() FrameMark
#define GP_FRAME_MARK_N(name) FrameMarkNamed(name)
#define GP_FRAME_MARK_START(name) FrameMarkStart(name)
#define GP_FRAME_MARK_END(name) FrameMarkEnd(name)

/// @section Memory Tracking

#define GP_MEM_ALLOC(ptr, size) TracyAlloc(ptr, size)
#define GP_MEM_FREE(ptr) TracyFree(ptr)
#define GP_MEM_ALLOC_N(ptr, size, pool) TracyAllocN(ptr, size, pool)
#define GP_MEM_FREE_N(ptr, pool) TracyFreeN(ptr, pool)

/// @section Messages / Markers

#define GP_MESSAGE(text, size) TracyMessage(text, size)
#define GP_MESSAGE_L(literal) TracyMessageL(literal)
#define GP_MESSAGE_C(text, size, color) TracyMessageC(text, size, color)
#define GP_MESSAGE_LC(literal, color) TracyMessageLC(literal, color)

/// @section Plots / Graphs

#define GP_PLOT(name, val) TracyPlot(name, val)
#define GP_PLOT_CONFIG(name, type, step, fill, color) TracyPlotConfig(name, type, step, fill, color)

/// @section Threading

#define GP_SET_THREAD_NAME(name) tracy::SetThreadName(name)

/// @section Application Info

#define GP_APP_INFO(text, size) TracyAppInfo(text, size)

/// @section GPU Zones

#if GP_TRACY_HAS_GPU
#define GP_GPU_SCOPE(name) TracyGpuZone(name)
#define GP_GPU_SCOPE_C(name, color) TracyGpuZoneC(name, color)
#define GP_GPU_SCOPE_CTX(ctx, name) TracyGpuZoneTransient(ctx, ___gpuZone, name, true)
#define GP_GPU_SCOPE_CTX_C(ctx, name, color) TracyGpuZoneTransient(ctx, ___gpuZone, name, true)
#define GP_GPU_MARKER(ctx, color, name) (void)(ctx)
#define GP_GPU_COLLECT() TracyGpuCollect
#else
#define GP_GPU_SCOPE(name)
#define GP_GPU_SCOPE_C(name, color)
#define GP_GPU_SCOPE_CTX(ctx, name)
#define GP_GPU_SCOPE_CTX_C(ctx, name, color)
#define GP_GPU_MARKER(ctx, color, name)
#define GP_GPU_COLLECT()
#endif

/// @section Locks / Mutexes

#define GP_LOCKABLE(type, var) TracyLockable(type, var)
#define GP_LOCKABLE_N(type, var, desc) TracyLockableN(type, var, desc)
