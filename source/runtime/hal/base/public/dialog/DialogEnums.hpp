// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "templates/Enums.hpp"

namespace gp::hal
{

/// @brief Visual severity icon displayed inside a native modal message box.
enum class DialogIcon : gp::UInt8
{
    None = 0,
    Information,
    Warning,
    Error
};

/// @brief Standard button layout presets for quick modal alerts.
enum class DialogButtonPreset : gp::UInt8
{
    Ok = 0,
    OkCancel,
    YesNo,
    YesNoCancel,
    RetryCancel,
    AbortRetryIgnore
};

/// @brief Standardized button identifiers returned by `IDialog::showAlert()`.
enum class DialogButtonId : gp::Int32
{
    Invalid = -1,
    Ok = 1,
    Cancel = 2,
    Yes = 3,
    No = 4,
    Retry = 5,
    Abort = 6,
    Ignore = 7
};

/// @brief Bitmask flags assigning keyboard shortcut semantics to a custom message box button.
enum class DialogButtonFlags : gp::UInt8
{
    None = 0,
    ReturnKeyDefault = 1 << 0,   // Activated when the user presses Enter / Return
    EscapeKeyDefault = 1 << 1    // Activated when the user presses Escape
};

/// @brief Completion status reported to an asynchronous native file or folder dialog callback.
enum class DialogFileStatus : gp::UInt8
{
    /// @brief The user selected one or more valid paths and confirmed the dialog.
    Accepted = 0,

    /// @brief The user closed or cancelled the dialog without selecting any path.
    Cancelled = 1,

    /// @brief The OS failed to spawn the file dialog or encountered an internal error.
    Error = 2
};

}   // namespace gp::hal

GP_ENABLE_ENUM_BITWISE_OPERATIONS(gp::hal::DialogButtonFlags);
