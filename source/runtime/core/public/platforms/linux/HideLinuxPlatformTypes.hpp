// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

// NOTE: Intentionally omits `#pragma once` so paired Allow/Hide brackets can nest or repeat within a translation unit.

/// @file HideLinuxPlatformTypes.hpp
/// @brief Closes a scope opened by `AllowLinuxPlatformTypes.hpp`, restoring pre-include macro state and removing X11
/// macros.

#pragma pop_macro("unix")
#pragma pop_macro("linux")
#pragma pop_macro("KeyCode")
#pragma pop_macro("Visual")
#pragma pop_macro("Cursor")
#pragma pop_macro("Font")
#pragma pop_macro("Display")
#pragma pop_macro("Window")
#pragma pop_macro("CurrentTime")
#pragma pop_macro("Always")
#pragma pop_macro("Success")
#pragma pop_macro("False")
#pragma pop_macro("True")
#pragma pop_macro("Status")
#pragma pop_macro("Bool")
#pragma pop_macro("None")
