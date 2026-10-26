// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"

namespace gp::hal
{

/// @brief Identifies the target OS clipboard buffer to read from or write to.
enum class ClipboardTarget : gp::UInt8
{
    /// @brief Standard system clipboard (Ctrl+C / Ctrl+V on Windows/Linux, Cmd+C / Cmd+V on macOS).
    Standard = 0,

    /// @brief Primary selection buffer (highlighted text pasted via middle-mouse click on X11/Wayland).
    PrimarySelection = 1
};

/// @brief Explicit status codes for clipboard data operations.
enum class [[nodiscard]] ClipboardResult : gp::UInt8
{
    Success = 0,
    Empty,
    UnsupportedMimeType,
    PayloadTooLarge,
    BackendError
};

}   // namespace gp::hal
