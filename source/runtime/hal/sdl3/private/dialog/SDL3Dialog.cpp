// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#include "dialog/SDL3Dialog.hpp"
#include "threading/Atomic.hpp"
#include "window/SDL3Window.hpp"   // IWYU pragma: keep
#include <SDL3/SDL.h>

namespace gp::hal::sdl3
{

namespace detail
{

constexpr gp::USize kMaxTitleBytes = 256u;
constexpr gp::USize kMaxMessageBytes = 2048u;
constexpr gp::USize kMaxButtonLabelBytes = 64u;
constexpr gp::USize kMaxPathBytes = 1024u;
constexpr gp::USize kMaxFilterStringBytes = 128u;
constexpr gp::UInt32 kMaxConcurrentFileDialogs = 4u;

/// @brief Copies a `StringView` into a fixed char buffer and appends a null terminator.
template <gp::USize Capacity>
constexpr const char* copyStringViewToNullTerm(StringView src, char (&dst)[Capacity]) noexcept
{
    const gp::USize count = (src.size() < (Capacity - 1u)) ? src.size() : (Capacity - 1u);
    for (gp::USize i = 0u; i < count; ++i)
    {
        dst[i] = src[i];
    }
    dst[count] = '\0';
    return dst;
}

/// @brief Computes bounded C-string length without `<cstring>`.
[[nodiscard]] constexpr gp::USize safeStrLen(const char* str, gp::USize maxLen = 4096u) noexcept
{
    if (str == nullptr)
    {
        return 0u;
    }
    gp::USize len = 0u;
    while (len < maxLen && str[len] != '\0')
    {
        ++len;
    }
    return len;
}

/// @brief Extracts the underlying `SDL_Window*` from an `IWindow` pointer if it is an open SDL3 window.
[[nodiscard]] SDL_Window* extractSdlParentWindow(const IWindow* parentWindow) noexcept
{
    if (parentWindow == nullptr || !parentWindow->isOpen())
    {
        return nullptr;
    }
    const WindowId winId = parentWindow->getId();
    return (winId != kInvalidWindowId) ? SDL_GetWindowFromID(static_cast<SDL_WindowID>(winId)) : nullptr;
}

/// @brief Maps `DialogIcon` to `SDL_MessageBoxFlags`.
[[nodiscard]] constexpr SDL_MessageBoxFlags toSdlIconFlags(DialogIcon icon) noexcept
{
    switch (icon)
    {
    case DialogIcon::Error:
        return SDL_MESSAGEBOX_ERROR;
    case DialogIcon::Warning:
        return SDL_MESSAGEBOX_WARNING;
    case DialogIcon::Information:
        return SDL_MESSAGEBOX_INFORMATION;
    case DialogIcon::None:
    default:
        return 0u;
    }
}

/// @brief Fixed-capacity slot keeping filter strings and callback pointers alive while an OS file dialog is open.
struct AsyncFileDialogContext
{
    gp::Atomic<bool> inUse{ false };
    FileDialogCallback userCallback{ nullptr };
    void* userData{ nullptr };
    SDL_DialogFileFilter sdlFilters[kMaxDialogFileFilters]{};
    char filterNames[kMaxDialogFileFilters][kMaxFilterStringBytes]{};
    char filterPatterns[kMaxDialogFileFilters][kMaxFilterStringBytes]{};
    gp::UInt32 filterCount{ 0u };
};

AsyncFileDialogContext s_fileDialogContexts[kMaxConcurrentFileDialogs]{};

/// @brief Claims an available `AsyncFileDialogContext` slot using a lock-free CAS.
[[nodiscard]] AsyncFileDialogContext* acquireFileDialogContext() noexcept
{
    for (gp::UInt32 i = 0u; i < kMaxConcurrentFileDialogs; ++i)
    {
        bool expected = false;
        if (s_fileDialogContexts[i].inUse.compareExchangeStrong(
                expected, true, gp::MemoryOrder::Acquire, gp::MemoryOrder::Relaxed
            ))
        {
            return &s_fileDialogContexts[i];
        }
    }
    return nullptr;
}

/// @brief SDL3 C callback adapter that converts `const char* const* filelist` into `StringView` slices.
void SDLCALL onSdlFileDialogComplete(void* userdata, const char* const* filelist, int filter) noexcept
{
    auto* ctx = static_cast<AsyncFileDialogContext*>(userdata);
    if (ctx == nullptr) [[unlikely]]
    {
        return;
    }

    if (ctx->userCallback != nullptr)
    {
        FileDialogResult result{};
        result.selectedFilterIndex = static_cast<gp::Int32>(filter);

        StringView pathViews[kMaxDialogSelectedFiles]{};

        if (filelist == nullptr)
        {
            // SDL3 passes nullptr for `filelist` when an internal OS error occurred
            result.status = DialogFileStatus::Error;
            const char* err = SDL_GetError();
            result.errorMessage = StringView(err, safeStrLen(err, 512u));
        }
        else if (filelist[0] == nullptr)
        {
            // SDL3 passes an isEmpty list (`filelist[0] == nullptr`) when the user cancels
            result.status = DialogFileStatus::Cancelled;
        }
        else
        {
            result.status = DialogFileStatus::Accepted;
            gp::UInt32 count = 0u;
            while (filelist[count] != nullptr && count < kMaxDialogSelectedFiles)
            {
                pathViews[count] = StringView(filelist[count], safeStrLen(filelist[count], kMaxPathBytes));
                ++count;
            }
            result.paths = pathViews;
            result.pathCount = count;
        }

        ctx->userCallback(ctx->userData, result);
    }

    ctx->userCallback = nullptr;
    ctx->userData = nullptr;
    ctx->inUse.store(false, gp::MemoryOrder::Release);
}

/// @brief Shared internal dispatcher for SDL3 Open File, Save File, and Open Folder dialogs via
/// `SDL_ShowFileDialogWithProperties`.
[[nodiscard]] bool dispatchSdlFileDialog(SDL_FileDialogType dialogType, const FileDialogDesc& desc) noexcept
{
    AsyncFileDialogContext* ctx = acquireFileDialogContext();
    if (ctx == nullptr) [[unlikely]]
    {
        if (desc.callback != nullptr)
        {
            FileDialogResult errResult{};
            errResult.status = DialogFileStatus::Error;
            errResult.errorMessage = StringView("Maximum concurrent file dialogs exceeded");
            desc.callback(desc.userData, errResult);
        }
        return false;
    }

    ctx->userCallback = desc.callback;
    ctx->userData = desc.userData;
    ctx->filterCount = 0u;

    if (dialogType != SDL_FILEDIALOG_OPENFOLDER && desc.filters != nullptr && desc.filterCount > 0u)
    {
        const gp::UInt32 limit = (desc.filterCount < kMaxDialogFileFilters) ? desc.filterCount : kMaxDialogFileFilters;

        for (gp::UInt32 i = 0u; i < limit; ++i)
        {
            ctx->sdlFilters[i].name = copyStringViewToNullTerm(desc.filters[i].name, ctx->filterNames[i]);
            ctx->sdlFilters[i].pattern = copyStringViewToNullTerm(desc.filters[i].pattern, ctx->filterPatterns[i]);
        }
        ctx->filterCount = limit;
    }

    const SDL_PropertiesID props = SDL_CreateProperties();

    if (SDL_Window* sdlParent = extractSdlParentWindow(desc.parentWindow))
    {
        (void)SDL_SetPointerProperty(props, SDL_PROP_FILE_DIALOG_WINDOW_POINTER, sdlParent);
    }

    if (ctx->filterCount > 0u)
    {
        (void)SDL_SetPointerProperty(props, SDL_PROP_FILE_DIALOG_FILTERS_POINTER, ctx->sdlFilters);
        (void)SDL_SetNumberProperty(props, SDL_PROP_FILE_DIALOG_NFILTERS_NUMBER, static_cast<Sint64>(ctx->filterCount));
    }

    char locationBuf[kMaxPathBytes]{};
    if (!desc.defaultLocation.isEmpty())
    {
        (void)SDL_SetStringProperty(
            props, SDL_PROP_FILE_DIALOG_LOCATION_STRING, copyStringViewToNullTerm(desc.defaultLocation, locationBuf)
        );
    }

    char titleBuf[kMaxTitleBytes]{};
    if (!desc.title.isEmpty())
    {
        (void)SDL_SetStringProperty(
            props, SDL_PROP_FILE_DIALOG_TITLE_STRING, copyStringViewToNullTerm(desc.title, titleBuf)
        );
    }

    char acceptBuf[kMaxButtonLabelBytes]{};
    if (!desc.acceptLabel.isEmpty())
    {
        (void)SDL_SetStringProperty(
            props, SDL_PROP_FILE_DIALOG_ACCEPT_STRING, copyStringViewToNullTerm(desc.acceptLabel, acceptBuf)
        );
    }

    char cancelBuf[kMaxButtonLabelBytes]{};
    if (!desc.cancelLabel.isEmpty())
    {
        (void)SDL_SetStringProperty(
            props, SDL_PROP_FILE_DIALOG_CANCEL_STRING, copyStringViewToNullTerm(desc.cancelLabel, cancelBuf)
        );
    }

    if (dialogType != SDL_FILEDIALOG_SAVEFILE && desc.allowMultiple)
    {
        (void)SDL_SetBooleanProperty(props, SDL_PROP_FILE_DIALOG_MANY_BOOLEAN, true);
    }

    SDL_ShowFileDialogWithProperties(dialogType, onSdlFileDialogComplete, ctx, props);
    SDL_DestroyProperties(props);
    return true;
}

}   // namespace detail

DialogButtonId Dialog::showAlert(
    DialogIcon icon, StringView title, StringView message, DialogButtonPreset preset, const IWindow* parentWindow
) noexcept
{
    DialogButtonDesc buttons[3]{};
    gp::UInt32 buttonCount = 0u;

    switch (preset)
    {
    case DialogButtonPreset::Ok:
        buttons[0] =
            DialogButtonDesc(static_cast<gp::Int32>(DialogButtonId::Ok), "OK", DialogButtonFlags::ReturnKeyDefault);
        buttonCount = 1u;
        break;

    case DialogButtonPreset::OkCancel:
        buttons[0] =
            DialogButtonDesc(static_cast<gp::Int32>(DialogButtonId::Ok), "OK", DialogButtonFlags::ReturnKeyDefault);
        buttons[1] = DialogButtonDesc(
            static_cast<gp::Int32>(DialogButtonId::Cancel), "Cancel", DialogButtonFlags::EscapeKeyDefault
        );
        buttonCount = 2u;
        break;

    case DialogButtonPreset::YesNo:
        buttons[0] =
            DialogButtonDesc(static_cast<gp::Int32>(DialogButtonId::Yes), "Yes", DialogButtonFlags::ReturnKeyDefault);
        buttons[1] =
            DialogButtonDesc(static_cast<gp::Int32>(DialogButtonId::No), "No", DialogButtonFlags::EscapeKeyDefault);
        buttonCount = 2u;
        break;

    case DialogButtonPreset::YesNoCancel:
        buttons[0] =
            DialogButtonDesc(static_cast<gp::Int32>(DialogButtonId::Yes), "Yes", DialogButtonFlags::ReturnKeyDefault);
        buttons[1] = DialogButtonDesc(static_cast<gp::Int32>(DialogButtonId::No), "No");
        buttons[2] = DialogButtonDesc(
            static_cast<gp::Int32>(DialogButtonId::Cancel), "Cancel", DialogButtonFlags::EscapeKeyDefault
        );
        buttonCount = 3u;
        break;

    case DialogButtonPreset::RetryCancel:
        buttons[0] = DialogButtonDesc(
            static_cast<gp::Int32>(DialogButtonId::Retry), "Retry", DialogButtonFlags::ReturnKeyDefault
        );
        buttons[1] = DialogButtonDesc(
            static_cast<gp::Int32>(DialogButtonId::Cancel), "Cancel", DialogButtonFlags::EscapeKeyDefault
        );
        buttonCount = 2u;
        break;

    case DialogButtonPreset::AbortRetryIgnore:
        buttons[0] = DialogButtonDesc(
            static_cast<gp::Int32>(DialogButtonId::Abort), "Abort", DialogButtonFlags::EscapeKeyDefault
        );
        buttons[1] = DialogButtonDesc(
            static_cast<gp::Int32>(DialogButtonId::Retry), "Retry", DialogButtonFlags::ReturnKeyDefault
        );
        buttons[2] = DialogButtonDesc(static_cast<gp::Int32>(DialogButtonId::Ignore), "Ignore");
        buttonCount = 3u;
        break;

    default:
        buttons[0] =
            DialogButtonDesc(static_cast<gp::Int32>(DialogButtonId::Ok), "OK", DialogButtonFlags::ReturnKeyDefault);
        buttonCount = 1u;
        break;
    }

    MessageBoxDesc desc{};
    desc.title = title;
    desc.message = message;
    desc.parentWindow = parentWindow;
    desc.buttons = buttons;
    desc.buttonCount = buttonCount;
    desc.icon = icon;

    return static_cast<DialogButtonId>(showMessageBox(desc));
}

gp::Int32 Dialog::showMessageBox(const MessageBoxDesc& desc) noexcept
{
    char titleBuf[detail::kMaxTitleBytes]{};
    char messageBuf[detail::kMaxMessageBytes]{};
    const char* titleCStr = detail::copyStringViewToNullTerm(desc.title, titleBuf);
    const char* messageCStr = detail::copyStringViewToNullTerm(desc.message, messageBuf);

    SDL_Window* sdlParent = detail::extractSdlParentWindow(desc.parentWindow);
    SDL_MessageBoxFlags flags = detail::toSdlIconFlags(desc.icon);
    flags |= desc.buttonsRightToLeft ? SDL_MESSAGEBOX_BUTTONS_RIGHT_TO_LEFT : SDL_MESSAGEBOX_BUTTONS_LEFT_TO_RIGHT;

    // Fast path for single-button default message box
    if (desc.buttons == nullptr || desc.buttonCount == 0u)
    {
        if (SDL_ShowSimpleMessageBox(flags, titleCStr, messageCStr, sdlParent))
        {
            return static_cast<gp::Int32>(DialogButtonId::Ok);
        }
        return -1;
    }

    const gp::UInt32 count = (desc.buttonCount < kMaxDialogButtons) ? desc.buttonCount : kMaxDialogButtons;
    SDL_MessageBoxButtonData sdlButtons[kMaxDialogButtons]{};
    char buttonLabels[kMaxDialogButtons][detail::kMaxButtonLabelBytes]{};

    for (gp::UInt32 i = 0u; i < count; ++i)
    {
        SDL_MessageBoxButtonFlags btnFlags = 0u;
        if ((desc.buttons[i].flags & DialogButtonFlags::ReturnKeyDefault) != DialogButtonFlags::None)
        {
            btnFlags |= SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT;
        }
        if ((desc.buttons[i].flags & DialogButtonFlags::EscapeKeyDefault) != DialogButtonFlags::None)
        {
            btnFlags |= SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
        }

        sdlButtons[i].flags = btnFlags;
        sdlButtons[i].buttonID = desc.buttons[i].id;
        sdlButtons[i].text = detail::copyStringViewToNullTerm(desc.buttons[i].text, buttonLabels[i]);
    }

    SDL_MessageBoxColorScheme sdlScheme{};
    const SDL_MessageBoxColorScheme* schemePtr = nullptr;

    if (desc.colorScheme != nullptr)
    {
        const DialogColorScheme& cs = *desc.colorScheme;
        sdlScheme.colors[SDL_MESSAGEBOX_COLOR_BACKGROUND] = { cs.background.r, cs.background.g, cs.background.b };
        sdlScheme.colors[SDL_MESSAGEBOX_COLOR_TEXT] = { cs.text.r, cs.text.g, cs.text.b };
        sdlScheme.colors[SDL_MESSAGEBOX_COLOR_BUTTON_BORDER] = { cs.buttonBorder.r,
                                                                 cs.buttonBorder.g,
                                                                 cs.buttonBorder.b };
        sdlScheme.colors[SDL_MESSAGEBOX_COLOR_BUTTON_BACKGROUND] = { cs.buttonBackground.r,
                                                                     cs.buttonBackground.g,
                                                                     cs.buttonBackground.b };
        sdlScheme.colors[SDL_MESSAGEBOX_COLOR_BUTTON_SELECTED] = { cs.buttonSelected.r,
                                                                   cs.buttonSelected.g,
                                                                   cs.buttonSelected.b };
        schemePtr = &sdlScheme;
    }

    SDL_MessageBoxData boxData{};
    boxData.flags = flags;
    boxData.window = sdlParent;
    boxData.title = titleCStr;
    boxData.message = messageCStr;
    boxData.numbuttons = static_cast<int>(count);
    boxData.buttons = sdlButtons;
    boxData.colorScheme = schemePtr;

    int clickedButtonId = -1;
    if (!SDL_ShowMessageBox(&boxData, &clickedButtonId)) [[unlikely]]
    {
        return -1;
    }

    return static_cast<gp::Int32>(clickedButtonId);
}

bool Dialog::showOpenFileDialog(const FileDialogDesc& desc) noexcept
{
    return detail::dispatchSdlFileDialog(SDL_FILEDIALOG_OPENFILE, desc);
}

bool Dialog::showSaveFileDialog(const FileDialogDesc& desc) noexcept
{
    return detail::dispatchSdlFileDialog(SDL_FILEDIALOG_SAVEFILE, desc);
}

bool Dialog::showOpenFolderDialog(const FileDialogDesc& desc) noexcept
{
    return detail::dispatchSdlFileDialog(SDL_FILEDIALOG_OPENFOLDER, desc);
}

}   // namespace gp::hal::sdl3
