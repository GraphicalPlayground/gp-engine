// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

// NOTE: Intentionally omits `#pragma once` so paired Allow/Hide brackets can nest or repeat within a translation unit.

/// @file AllowWindowsPlatformTypes.hpp
/// @brief Temporarily opens a scope allowing native Windows/DXGI/D3D SDK headers to define Win32 macros and types.
/// @details Must always be paired with `HideWindowsPlatformTypes.hpp` immediately after the Windows SDK `#include`
/// block.
///          Combine with `GP_THIRD_PARTY_INCLUDES_START` / `GP_THIRD_PARTY_INCLUDES_END` when SDK headers emit
///          warnings.

#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
    #define NOMINMAX
#endif

#ifndef STRICT
    #define STRICT
#endif

#pragma push_macro("TRUE")
#pragma push_macro("FALSE")
#pragma push_macro("CONST")
#pragma push_macro("TEXT")
#pragma push_macro("INT")
#pragma push_macro("UINT")
#pragma push_macro("DWORD")
#pragma push_macro("FLOAT")
#pragma push_macro("min")
#pragma push_macro("max")
#pragma push_macro("CDECL")
#pragma push_macro("PF_MAX")
#pragma push_macro("CaptureStackBackTrace")
#pragma push_macro("CopyFile")
#pragma push_macro("CreateDesktop")
#pragma push_macro("CreateDirectory")
#pragma push_macro("CreateFont")
#pragma push_macro("DeleteFile")
#pragma push_macro("DrawText")
#pragma push_macro("FindWindow")
#pragma push_macro("GetClassInfo")
#pragma push_macro("GetClassName")
#pragma push_macro("GetCommandLine")
#pragma push_macro("GetCurrentTime")
#pragma push_macro("GetEnvironmentVariable")
#pragma push_macro("GetFileAttributes")
#pragma push_macro("GetFreeSpace")
#pragma push_macro("GetMessage")
#pragma push_macro("GetNextSibling")
#pragma push_macro("GetObject")
#pragma push_macro("GetProp")
#pragma push_macro("GetTempFileName")
#pragma push_macro("IsMaximized")
#pragma push_macro("IsMinimized")
#pragma push_macro("LoadString")
#pragma push_macro("MemoryBarrier")
#pragma push_macro("MoveFile")
#pragma push_macro("PlaySound")
#pragma push_macro("PostMessage")
#pragma push_macro("ReportEvent")
#pragma push_macro("SendMessage")
#pragma push_macro("SetPort")
#pragma push_macro("SetProp")
#pragma push_macro("UpdateResource")
#pragma push_macro("Yield")

#undef INT
#undef UINT
#undef DWORD
#undef FLOAT
