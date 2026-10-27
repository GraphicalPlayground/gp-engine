// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "clipboard/ClipboardEnums.hpp"
#include "clipboard/ClipboardTypes.hpp"
#include "containers/views/StringView.hpp"
#include "CoreMinimal.hpp"
#include "event/data/ClipboardEvent.hpp"
#include "HALBase.hpp"

namespace gp::hal
{

/// @brief Platform-agnostic interface for interacting with the OS system clipboard and primary selection buffer.
/// @details Supports UTF-8 text, X11/Wayland primary selection buffers, and arbitrary binary MIME payloads
///          (for editor asset copy-paste) while guaranteeing zero memory leaks or dangling `StringView` pointers.
class GP_HAL_BASE_API IClipboard
{
public:
    /// @brief Default constructor.
    [[nodiscard]] constexpr IClipboard() noexcept = default;

    /// @brief Virtual destructor.
    virtual ~IClipboard() noexcept = default;

    IClipboard(const IClipboard&) = delete;
    IClipboard& operator=(const IClipboard&) = delete;
    IClipboard(IClipboard&&) = delete;
    IClipboard& operator=(IClipboard&&) = delete;

public:
    /// @brief Copies a UTF-8 string into the specified OS clipboard target.
    /// @param[in] text UTF-8 string view to copy into the clipboard.
    /// @param[in] target Target buffer (`Standard` clipboard or `PrimarySelection`).
    /// @return `ClipboardResult::Success` if the text was committed, or an error code.
    virtual ClipboardResult setText(StringView text, ClipboardTarget target = ClipboardTarget::Standard) noexcept = 0;

    /// @brief Queries the current UTF-8 text from the specified OS clipboard target.
    /// @note The returned `StringView` points to an internal inline cache that remains valid until the next
    ///       `getText()`, `setText()`, or `clear()` call on this instance.
    /// @param[in] target Target buffer (`Standard` clipboard or `PrimarySelection`).
    /// @return `StringView` referencing the retrieved UTF-8 text (empty view if the clipboard has no text).
    [[nodiscard]] virtual StringView getText(ClipboardTarget target = ClipboardTarget::Standard) noexcept = 0;

    /// @brief Copies the current UTF-8 clipboard text into a caller-provided buffer (for payloads > 16 KB).
    /// @param[out] outBuffer Destination character buffer (always null-terminated if `capacity > 0`).
    /// @param[in] capacity Total byte capacity of `outBuffer`.
    /// @param[in] target Target buffer (`Standard` clipboard or `PrimarySelection`).
    /// @return Number of UTF-8 bytes written (excluding the null terminator).
    virtual gp::USize getTextBuffer(
        char* outBuffer, gp::USize capacity, ClipboardTarget target = ClipboardTarget::Standard
    ) noexcept = 0;

    /// @brief Checks whether the specified OS clipboard target currently contains non-empty UTF-8 text.
    /// @param[in] target Target buffer (`Standard` clipboard or `PrimarySelection`).
    /// @return True if text is available, false otherwise.
    [[nodiscard]] virtual bool hasText(ClipboardTarget target = ClipboardTarget::Standard) const noexcept = 0;

    /// @brief Publishes a binary payload tagged with a specific MIME type to the system clipboard.
    /// @param[in] mimeType UTF-8 MIME identifier (e.g., "application/x-gp-entity", "text/html").
    /// @param[in] data Pointer to the raw payload bytes to copy.
    /// @param[in] sizeBytes Length of `data` in bytes.
    /// @return `ClipboardResult::Success` on success, or an error code.
    virtual ClipboardResult setData(StringView mimeType, const void* data, gp::USize sizeBytes) noexcept = 0;

    /// @brief Checks whether the system clipboard currently offers data matching the specified MIME type.
    /// @param[in] mimeType UTF-8 MIME identifier to query.
    /// @return True if matching MIME data is available, false otherwise.
    [[nodiscard]] virtual bool hasData(StringView mimeType) const noexcept = 0;

    /// @brief Reads binary data of the requested MIME type via a scoped callback, automatically freeing OS buffers.
    /// @param[in] mimeType UTF-8 MIME identifier to request.
    /// @param[in] callback Function invoked synchronously with a `ClipboardDataView` of the retrieved payload.
    /// @param[in] userData Optional opaque context pointer forwarded to `callback`.
    /// @return `ClipboardResult::Success` if the payload was read and passed to `callback`, or an error code.
    virtual ClipboardResult
        readData(StringView mimeType, ClipboardDataCallback callback, void* userData = nullptr) noexcept = 0;

    /// @brief Clears all text and binary data currently held in the clipboard.
    /// @return True if cleared successfully, false otherwise.
    virtual bool clear() noexcept = 0;

    /// @brief Returns the monotonic generation counter incremented whenever the clipboard contents change.
    /// @return Current 64-bit clipboard sequence number.
    [[nodiscard]] virtual gp::UInt64 getSequenceNumber() const noexcept = 0;

    /// @brief Synchronizes internal cached state when an OS `ClipboardEvent` is drained from the SPSC ring buffer.
    /// @param[in] ev Decoded clipboard event packet.
    virtual void onClipboardEvent(const event::ClipboardEvent& ev) noexcept = 0;
};

}   // namespace gp::hal
