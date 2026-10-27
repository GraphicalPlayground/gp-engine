// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/views/StringView.hpp"
#include "CoreMinimal.hpp"
#include "dialog/DialogEnums.hpp"
#include "dialog/DialogTypes.hpp"
#include "HALBase.hpp"

namespace gp::hal
{

/// @brief Platform-agnostic interface for native OS modal message boxes and asynchronous file/folder dialogs.
class GP_HAL_BASE_API IDialog
{
public:
    /// @brief Default constructor.
    [[nodiscard]] constexpr IDialog() noexcept = default;

    /// @brief Virtual destructor.
    virtual ~IDialog() noexcept = default;

    IDialog(const IDialog&) = delete;
    IDialog& operator=(const IDialog&) = delete;
    IDialog(IDialog&&) = delete;
    IDialog& operator=(IDialog&&) = delete;

public:
    /// @brief Displays a synchronous modal message box using a standard button preset.
    /// @param[in] icon Severity icon (`Information`, `Warning`, `Error`).
    /// @param[in] title UTF-8 window title string view.
    /// @param[in] message UTF-8 body text string view.
    /// @param[in] preset Standard button combination (`Ok`, `OkCancel`, `YesNo`, `AbortRetryIgnore`, etc.).
    /// @param[in] parentWindow Optional parent `IWindow` (`nullptr` for system-level modal).
    /// @return The `DialogButtonId` clicked by the user, or `DialogButtonId::Invalid` on failure.
    virtual DialogButtonId showAlert(
        DialogIcon icon,
        StringView title,
        StringView message,
        DialogButtonPreset preset = DialogButtonPreset::Ok,
        const IWindow* parentWindow = nullptr
    ) noexcept = 0;

    /// @brief Displays a synchronous modal message box with custom buttons and optional color theming.
    /// @param[in] desc Full configuration descriptor for the message box.
    /// @return The user-defined `DialogButtonDesc::id` clicked by the user, or `-1` on failure/dismissal.
    virtual gp::Int32 showMessageBox(const MessageBoxDesc& desc) noexcept = 0;

    /// @brief Opens an asynchronous native OS "Open File" dialog.
    /// @param[in] desc File dialog configuration, extension filters, and completion callback.
    /// @return True if the dialog request was successfully dispatched to the OS, false otherwise.
    virtual bool showOpenFileDialog(const FileDialogDesc& desc) noexcept = 0;

    /// @brief Opens an asynchronous native OS "Save File" dialog.
    /// @param[in] desc File dialog configuration, extension filters, and completion callback.
    /// @return True if the dialog request was successfully dispatched to the OS, false otherwise.
    virtual bool showSaveFileDialog(const FileDialogDesc& desc) noexcept = 0;

    /// @brief Opens an asynchronous native OS "Select Folder" dialog.
    /// @param[in] desc Folder dialog configuration and completion callback.
    /// @return True if the dialog request was successfully dispatched to the OS, false otherwise.
    virtual bool showOpenFolderDialog(const FileDialogDesc& desc) noexcept = 0;
};

}   // namespace gp::hal
