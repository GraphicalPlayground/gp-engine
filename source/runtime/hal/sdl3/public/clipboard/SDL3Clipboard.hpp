// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "clipboard/GenericClipboard.hpp"
#include "CoreMinimal.hpp"

namespace gp::hal::sdl3
{

/// @brief Concrete SDL3 implementation of `gp::hal::IClipboard`.
/// @details Bridges `StringView` text and binary MIME operations to native OS clipboards via SDL3,
///          copying returned strings into `generic::Clipboard`'s inline cache and immediately invoking
///          `SDL_free()` so zero heap allocations ever leak to the caller.
class Clipboard final : public gp::hal::generic::Clipboard
{
public:
    /// @brief Default constructor.
    [[nodiscard]] constexpr Clipboard() noexcept = default;

    /// @brief Destructor.
    ~Clipboard() noexcept override = default;

public:
    /// @copydoc IClipboard::setText()
    ClipboardResult setText(StringView text, ClipboardTarget target = ClipboardTarget::Standard) noexcept override;

    /// @copydoc IClipboard::getText()
    [[nodiscard]] StringView getText(ClipboardTarget target = ClipboardTarget::Standard) noexcept override;

    /// @copydoc IClipboard::getTextBuffer()
    gp::USize getTextBuffer(
        char* outBuffer, gp::USize capacity, ClipboardTarget target = ClipboardTarget::Standard
    ) noexcept override;

    /// @copydoc IClipboard::hasText()
    [[nodiscard]] bool hasText(ClipboardTarget target = ClipboardTarget::Standard) const noexcept override;

    /// @copydoc IClipboard::setData()
    ClipboardResult setData(StringView mimeType, const void* data, gp::USize sizeBytes) noexcept override;

    /// @copydoc IClipboard::hasData()
    [[nodiscard]] bool hasData(StringView mimeType) const noexcept override;

    /// @copydoc IClipboard::readData()
    ClipboardResult
        readData(StringView mimeType, ClipboardDataCallback callback, void* userData = nullptr) noexcept override;

    /// @copydoc IClipboard::clear()
    bool clear() noexcept override;
};

using SDL3Clipboard = Clipboard;

}   // namespace gp::hal::sdl3
