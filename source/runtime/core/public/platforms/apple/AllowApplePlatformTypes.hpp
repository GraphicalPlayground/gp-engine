// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

// NOTE: Intentionally omits `#pragma once` so paired Allow/Hide brackets can nest or repeat within a translation unit.

/// @file AllowApplePlatformTypes.hpp
/// @brief Opens a macro-isolation scope before including Apple Cocoa/Carbon/AssertMacros SDK headers.
/// @details Saves macros such as `check`, `verify`, `require`, `BOOL`, `YES`, `NO`, `Nil`, and `nil` until closed by
///          `HideApplePlatformTypes.hpp`.

#pragma push_macro("check")
#pragma push_macro("verify")
#pragma push_macro("require")
#pragma push_macro("require_noerr")
#pragma push_macro("BOOL")
#pragma push_macro("YES")
#pragma push_macro("NO")
#pragma push_macro("Nil")
#pragma push_macro("nil")
