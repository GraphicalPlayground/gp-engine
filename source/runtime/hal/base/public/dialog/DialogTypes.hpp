// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/views/StringView.hpp"
#include "CoreMinimal.hpp"
#include "dialog/DialogEnums.hpp"

namespace gp::hal
{

// Forward declaration.
class IWindow;

/// @brief Maximum number of custom buttons supported in a single modal message box.
inline constexpr gp::UInt32 kMaxDialogButtons = 8u;

/// @brief Maximum number of file extension filters supported in a single file dialog invocation.
inline constexpr gp::UInt32 kMaxDialogFileFilters = 16u;

/// @brief Maximum number of selected file paths surfaced in a single multi-select file dialog callback.
inline constexpr gp::UInt32 kMaxDialogSelectedFiles = 64u;

/// @brief Defines a single button in a custom modal `MessageBoxDesc`.
struct DialogButtonDesc
{
    /// @brief User-defined identifier returned when this button is clicked.
    gp::Int32 id{ 0 };

    /// @brief UTF-8 label displayed on the button face.
    StringView text{ "OK" };

    /// @brief Keyboard shortcut association (`ReturnKeyDefault` or `EscapeKeyDefault`).
    DialogButtonFlags flags{ DialogButtonFlags::None };

    /// @brief Default constructor.
    [[nodiscard]] constexpr DialogButtonDesc() noexcept = default;

    /// @brief Constructs a button descriptor with explicit ID, label, and shortcut flags.
    /// @param[in] inId Identifier returned if this button is selected.
    /// @param[in] inText UTF-8 label string view.
    /// @param[in] inFlags Default key binding flags.
    [[nodiscard]] constexpr DialogButtonDesc(
        gp::Int32 inId, StringView inText, DialogButtonFlags inFlags = DialogButtonFlags::None
    ) noexcept
        : id(inId)
        , text(inText)
        , flags(inFlags)
    {}
};

/// @brief 24-bit RGB color entry used to theme native message boxes on supported window managers.
struct DialogColor
{
    gp::UInt8 r{ 0u };
    gp::UInt8 g{ 0u };
    gp::UInt8 b{ 0u };

    [[nodiscard]] constexpr DialogColor() noexcept = default;

    [[nodiscard]] constexpr DialogColor(gp::UInt8 inR, gp::UInt8 inG, gp::UInt8 inB) noexcept
        : r(inR)
        , g(inG)
        , b(inB)
    {}
};

/// @brief Custom color palette for theming native modal message boxes.
struct DialogColorScheme
{
    DialogColor background{ 32u, 34u, 37u };
    DialogColor text{ 240u, 240u, 240u };
    DialogColor buttonBorder{ 75u, 80u, 87u };
    DialogColor buttonBackground{ 47u, 49u, 54u };
    DialogColor buttonSelected{ 88u, 101u, 242u };
};

/// @brief Configuration descriptor for a customizable synchronous modal message box.
struct MessageBoxDesc
{
    /// @brief UTF-8 title displayed in the dialog's title bar.
    StringView title{ "Graphical Playground" };

    /// @brief UTF-8 body text displayed inside the dialog.
    StringView message{};

    /// @brief Optional parent window that this modal dialog should block and center over (`nullptr` for system-modal).
    const IWindow* parentWindow{ nullptr };

    /// @brief Pointer to an array of `DialogButtonDesc` definitions.
    const DialogButtonDesc* buttons{ nullptr };

    /// @brief Number of elements in the `buttons` array (clamped to `kMaxDialogButtons`).
    gp::UInt32 buttonCount{ 0u };

    /// @brief Optional pointer to a custom color scheme (`nullptr` uses the OS native theme).
    const DialogColorScheme* colorScheme{ nullptr };

    /// @brief Severity icon displayed beside the message text.
    DialogIcon icon{ DialogIcon::Information };

    /// @brief If true, lays out buttons from right-to-left; if false, left-to-right.
    bool buttonsRightToLeft{ false };
};

/// @brief Defines a file type filter entry for Open/Save file dialogs.
struct DialogFileFilter
{
    /// @brief Human-readable description of the filter (e.g., "Texture Assets", "All Files").
    StringView name{ "All Files" };

    /// @brief Semicolon-separated list of file extensions without leading dots (e.g., "png;jpg;dds" or "*").
    StringView pattern{ "*" };

    /// @brief Default constructor.
    [[nodiscard]] constexpr DialogFileFilter() noexcept = default;

    /// @brief Constructs a file filter from a display name and extension pattern.
    /// @param[in] inName Filter display name.
    /// @param[in] inPattern Semicolon-delimited extension list (e.g., "gltf;glb").
    [[nodiscard]] constexpr DialogFileFilter(StringView inName, StringView inPattern) noexcept
        : name(inName)
        , pattern(inPattern)
    {}
};

/// @brief Result payload delivered to `FileDialogCallback` when an asynchronous file/folder dialog completes.
struct FileDialogResult
{
    /// @brief Array of `StringView` slices referencing the selected UTF-8 file/folder paths.
    const StringView* paths{ nullptr };

    /// @brief Number of valid entries in `paths`.
    gp::UInt32 pathCount{ 0u };

    /// @brief Zero-based index of the `DialogFileFilter` active when the user confirmed (`-1` if unsupported/none).
    gp::Int32 selectedFilterIndex{ -1 };

    /// @brief Diagnostic UTF-8 error message if `status == DialogFileStatus::Error`.
    StringView errorMessage{};

    /// @brief Completion outcome (`Accepted`, `Cancelled`, or `Error`).
    DialogFileStatus status{ DialogFileStatus::Cancelled };
};

/// @brief C-style callback signature invoked when an asynchronous file or folder dialog finishes.
/// @param[in] userData Opaque user context pointer supplied in `FileDialogDesc::userData`.
/// @param[in] result Selection results and status code (valid for the duration of the callback).
using FileDialogCallback = void (*)(void* userData, const FileDialogResult& result) noexcept;

/// @brief Configuration descriptor for asynchronous Open File, Save File, and Open Folder dialogs.
struct FileDialogDesc
{
    /// @brief Custom UTF-8 title for the file dialog window (empty uses the OS default title).
    StringView title{};

    /// @brief Initial directory path or default filename to populate when opening the dialog.
    StringView defaultLocation{};

    /// @brief Custom UTF-8 label for the confirm/accept button (empty uses OS default "Open" / "Save").
    StringView acceptLabel{};

    /// @brief Custom UTF-8 label for the cancel button (empty uses OS default "Cancel").
    StringView cancelLabel{};

    /// @brief Optional parent window to modal-attach the file picker to (`nullptr` for unparented).
    const IWindow* parentWindow{ nullptr };

    /// @brief Pointer to an array of file extension filters (ignored for `showOpenFolderDialog`).
    const DialogFileFilter* filters{ nullptr };

    /// @brief Number of filters in the `filters` array (clamped to `kMaxDialogFileFilters`).
    gp::UInt32 filterCount{ 0u };

    /// @brief Completion callback invoked when the user confirms, cancels, or encounters an error.
    FileDialogCallback callback{ nullptr };

    /// @brief Opaque user context pointer forwarded verbatim to `callback`.
    void* userData{ nullptr };

    /// @brief If true, permits selecting multiple files/folders simultaneously (ignored for `showSaveFileDialog`).
    bool allowMultiple{ false };
};

}   // namespace gp::hal
