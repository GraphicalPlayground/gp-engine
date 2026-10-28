// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "dialog/IDialog.hpp"
#include "GenericHAL.hpp"

namespace gp::hal::generic
{

/// @brief Platform-agnostic base and headless fallback implementation of `IDialog`.
/// @details Automatically resolves modal message boxes by selecting the Return-key default button
///          and immediately cancels file dialogs so headless servers and CI tests never hang.
class GP_HAL_GENERIC_API Dialog : public gp::hal::IDialog
{
public:
    /// @brief Default constructor.
    [[nodiscard]] constexpr Dialog() noexcept = default;

    /// @brief Virtual destructor.
    ~Dialog() noexcept override = default;

public:
    /// @copydoc IDialog::showAlert()
    constexpr DialogButtonId showAlert(
        [[maybe_unused]] DialogIcon icon,
        [[maybe_unused]] StringView title,
        [[maybe_unused]] StringView message,
        DialogButtonPreset preset = DialogButtonPreset::Ok,
        [[maybe_unused]] const IWindow* parentWindow = nullptr
    ) noexcept override
    {
        switch (preset)
        {
        case DialogButtonPreset::Ok:
        case DialogButtonPreset::OkCancel:
            return DialogButtonId::Ok;
        case DialogButtonPreset::YesNo:
        case DialogButtonPreset::YesNoCancel:
            return DialogButtonId::Yes;
        case DialogButtonPreset::RetryCancel:
            return DialogButtonId::Cancel;
        case DialogButtonPreset::AbortRetryIgnore:
            return DialogButtonId::Abort;
        default:
            return DialogButtonId::Ok;
        }
    }

    /// @copydoc IDialog::showMessageBox()
    constexpr gp::Int32 showMessageBox(const MessageBoxDesc& desc) noexcept override
    {
        if (desc.buttons == nullptr || desc.buttonCount == 0u) [[unlikely]]
        {
            return -1;
        }

        for (gp::UInt32 i = 0u; i < desc.buttonCount; ++i)
        {
            if ((desc.buttons[i].flags & DialogButtonFlags::ReturnKeyDefault) != DialogButtonFlags::None)
            {
                return desc.buttons[i].id;
            }
        }

        return desc.buttons[0].id;
    }

    /// @copydoc IDialog::showOpenFileDialog()
    constexpr bool showOpenFileDialog(const FileDialogDesc& desc) noexcept override
    {
        if (desc.callback != nullptr)
        {
            FileDialogResult result{};
            result.status = DialogFileStatus::Cancelled;
            desc.callback(desc.userData, result);
        }
        return false;
    }

    /// @copydoc IDialog::showSaveFileDialog()
    constexpr bool showSaveFileDialog(const FileDialogDesc& desc) noexcept override
    {
        if (desc.callback != nullptr)
        {
            FileDialogResult result{};
            result.status = DialogFileStatus::Cancelled;
            desc.callback(desc.userData, result);
        }
        return false;
    }

    /// @copydoc IDialog::showOpenFolderDialog()
    constexpr bool showOpenFolderDialog(const FileDialogDesc& desc) noexcept override
    {
        if (desc.callback != nullptr)
        {
            FileDialogResult result{};
            result.status = DialogFileStatus::Cancelled;
            desc.callback(desc.userData, result);
        }
        return false;
    }
};

}   // namespace gp::hal::generic
