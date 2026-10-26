// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "dialog/GenericDialog.hpp"

namespace gp::hal::sdl3
{

/// @brief Concrete SDL3 implementation of `gp::hal::IDialog`.
/// @details Translates `StringView` descriptors into native OS modal message boxes (`SDL_ShowMessageBox`)
///          and asynchronous portal/OS file pickers (`SDL_ShowOpenFileDialog`, `SDL_ShowSaveFileDialog`,
///          `SDL_ShowOpenFolderDialog`) with fixed-capacity context pooling for filter lifetime safety.
class Dialog final : public gp::hal::generic::Dialog
{
public:
    /// @brief Default constructor.
    [[nodiscard]] constexpr Dialog() noexcept = default;

    /// @brief Destructor.
    ~Dialog() noexcept override = default;

public:
    /// @copydoc IDialog::showAlert()
    DialogButtonId showAlert(
        DialogIcon icon,
        StringView title,
        StringView message,
        DialogButtonPreset preset = DialogButtonPreset::Ok,
        const IWindow* parentWindow = nullptr
    ) noexcept override;

    /// @copydoc IDialog::showMessageBox()
    gp::Int32 showMessageBox(const MessageBoxDesc& desc) noexcept override;

    /// @copydoc IDialog::showOpenFileDialog()
    bool showOpenFileDialog(const FileDialogDesc& desc) noexcept override;

    /// @copydoc IDialog::showSaveFileDialog()
    bool showSaveFileDialog(const FileDialogDesc& desc) noexcept override;

    /// @copydoc IDialog::showOpenFolderDialog()
    bool showOpenFolderDialog(const FileDialogDesc& desc) noexcept override;
};

}   // namespace gp::hal::sdl3
