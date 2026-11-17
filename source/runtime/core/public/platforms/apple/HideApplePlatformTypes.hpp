// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

// NOTE: Intentionally omits `#pragma once` so paired Allow/Hide brackets can nest or repeat within a translation unit.

/// @file HideApplePlatformTypes.hpp
/// @brief Closes a scope opened by `AllowApplePlatformTypes.hpp`, restoring pre-include macro state.

#pragma pop_macro("nil")
#pragma pop_macro("Nil")
#pragma pop_macro("NO")
#pragma pop_macro("YES")
#pragma pop_macro("BOOL")
#pragma pop_macro("require_noerr")
#pragma pop_macro("require")
#pragma pop_macro("verify")
#pragma pop_macro("check")
