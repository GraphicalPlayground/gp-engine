// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "CoreMinimal.hpp"

#if defined(WINDOWS_H_WRAPPER_GUARD)
    #error "[GP] Windows.h has already been included."
#endif
#define WINDOWS_H_WRAPPER_GUARD

#if GP_COMPILER_CLANG
    #pragma warning(disable : 4103)
#endif

#if GP_COMPILER_CLANG
    #if GP_ARCHITECTURE_32BIT
_Pragma("pack(push)") _Pragma("pack(8)")
    #elif GP_ARCHITECTURE_64BIT
_Pragma("pack(push)") _Pragma("pack(16)")
    #endif
#else
    #if GP_ARCHITECTURE_32BIT
__pragma(pack(push, 8))
    #elif GP_ARCHITECTURE_64BIT
__pragma(pack(push, 16))
    #endif
#endif

// Save these macros for later, Windows redefines them
#pragma push_macro("MAX_uint8")
#pragma push_macro("MAX_uint16")
#pragma push_macro("MAX_uint32")
#pragma push_macro("MAX_int32")
#pragma push_macro("TEXT")
#pragma push_macro("TRUE")
#pragma push_macro("FALSE")

#ifndef STRICT
    #define STRICT
#endif

#if defined(_WINDOWS_) && !defined(GP_MINIMAL_WINDOWS_INCLUDE)
    #pragma message(" ")
    #pragma message("You have included windows.h before MinWindows.h")
    #pragma message("All useless stuff from the windows headers won't be excluded !!!")
    #pragma message(" ")
#endif
#define GP_MINIMAL_WINDOWS_INCLUDE

// WIN32_LEAN_AND_MEAN excludes rarely-used services from windows headers.
#define WIN32_LEAN_AND_MEAN

// The below excludes some other unused services from the windows headers, see windows.h for details.
// clang-format off
#define NOGDICAPMASKS           // CC_*, LC_*, PC_*, CP_*, TC_*, RC_
// #define NOVIRTUALKEYCODES    // VK_*
// #define NOWINMESSAGES        // WM_*, EM_*, LB_*, CB_*
// #define NOWINSTYLES          // WS_*, CS_*, ES_*, LBS_*, SBS_*, CBS_*
// #define NOSYSMETRICS         // SM_*
//  #define NOMENUS             // MF_*
// #define NOICONS              // IDI_*
// #define NOKEYSTATES          // MK_*
// #define NOSYSCOMMANDS        // SC_*
// #define NORASTEROPS          // Binary and Tertiary raster ops
// #define NOSHOWWINDOW         // SW_*
#define OEMRESOURCE             // OEM Resource values
#define NOATOM                  // Atom Manager routines
// #define NOCLIPBOARD          // Clipboard routines
// #define NOCOLOR              // Screen colors
// #define NOCTLMGR             // Control and Dialog routines
// #define NODRAWTEXT           // DrawText() and DT_*
// #define NOGDI                // All GDI #defines and routines
#define NOKERNEL                // All KERNEL #defines and routines
// #define NOUSER               // All USER #defines and routines
// #define NONLS                // All NLS #defines and routines
// #define NOMB                 // MB_* and MessageBox()
#define NOMEMMGR                // GMEM_*, LMEM_*, GHND, LHND, associated routines
#define NOMETAFILE              // typedef METAFILEPICT
#define NOMINMAX                // Macros min(a,b) and max(a,b)
// #define NOMSG                // typedef MSG and associated routines
#define NOOPENFILE              // OpenFile(), OemToAnsi, AnsiToOem, and OF_*
#define NOSCROLL                // SB_* and scrolling routines
#define NOSERVICE               // All Service Controller routines, SERVICE_ equates, etc.
#define NOSOUND                 // Sound driver routines
// #define NOTEXTMETRIC         // typedef TEXTMETRIC and associated routines
// #define NOWH                 // SetWindowsHook and WH_*
// #define NOWINOFFSETS         // GWL_*, GCL_*, associated routines
#define NOCOMM                  // COMM driver routines
#define NOKANJI                 // Kanji support stuff.
#define NOHELP                  // Help engine interface.
#define NOPROFILER              // Profiler interface.
#define NODEFERWINDOWPOS        // DeferWindowPos routines
#define NOMCX                   // Modem Configuration Extensions
#define NOCRYPT
#define NOTAPE
#define NOIMAGE
#define NOPROXYSTUB
#define NORPC
// clang-format on

// Finally include the Windows headers
#include <windows.h>

// Hide Windows-only types (same as HideWindowsPlatformTypes.h)
#undef INT
#undef UINT
#undef DWORD
#undef FLOAT

// Undo any Windows defines.
#undef uint8
#undef uint16
#undef uint32
#undef int32
#undef float
#undef CDECL
#undef PF_MAX
#undef CaptureStackBackTrace
#undef CopyFile
#undef CreateDesktop
#undef CreateDirectory
#undef CreateFont
#undef DeleteFile
#undef DrawText
#undef FindWindow
#undef GetClassInfo
#undef GetClassName
#undef GetCommandLine
#undef GetCurrentTime
#undef GetEnvironmentVariable
#undef GetFileAttributes
#undef GetFreeSpace
#undef GetMessage
#undef GetNextSibling
#undef GetObject
#undef GetProp
#undef GetTempFileName
#undef IMediaEventSink
#undef IsMaximized
#undef IsMinimized
#undef LoadString
#undef MemoryBarrier
#undef MoveFile
#undef PlaySound
#undef PostMessage
#undef ReportEvent
#undef SendMessage
#undef SetPort
#undef SetProp
#undef UpdateResource
#undef Yield

// Restore any previously defined macros
#pragma pop_macro("MAX_uint8")
#pragma pop_macro("MAX_uint16")
#pragma pop_macro("MAX_uint32")
#pragma pop_macro("MAX_int32")
#pragma pop_macro("TEXT")
#pragma pop_macro("TRUE")
#pragma pop_macro("FALSE")

#if GP_COMPILER_CLANG
    _Pragma("pack(pop)")
#else
    __pragma(pack(pop))
#endif

#undef WINDOWS_H_WRAPPER_GUARD
