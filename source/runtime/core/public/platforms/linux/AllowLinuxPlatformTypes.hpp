// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

// NOTE: Intentionally omits `#pragma once` so paired Allow/Hide brackets can nest or repeat within a translation unit.

/// @file AllowLinuxPlatformTypes.hpp
/// @brief Opens a macro-isolation scope before including Linux/X11 SDK headers (e.g., `<X11/Xlib.h>`, Vulkan Xlib WSI).
/// @details Saves and isolates polluting X11/Unix macros (`None`, `Success`, `Always`, `Bool`, `Status`, `Window`,
///          `Display`, etc.) until the matching `HideLinuxPlatformTypes.hpp` include restores the prior macro state.

#pragma push_macro("None")
#pragma push_macro("Bool")
#pragma push_macro("Status")
#pragma push_macro("True")
#pragma push_macro("False")
#pragma push_macro("Success")
#pragma push_macro("Always")
#pragma push_macro("CurrentTime")
#pragma push_macro("Window")
#pragma push_macro("Display")
#pragma push_macro("Font")
#pragma push_macro("Cursor")
#pragma push_macro("Visual")
#pragma push_macro("KeyCode")
#pragma push_macro("linux")
#pragma push_macro("unix")
