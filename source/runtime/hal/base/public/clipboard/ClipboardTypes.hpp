// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "clipboard/ClipboardEnums.hpp"   // IWYU pragma: keep
#include "containers/views/StringView.hpp"
#include "CoreMinimal.hpp"

namespace gp::hal
{

/// @brief Maximum UTF-8 byte capacity cached inline for zero-allocation `StringView` text queries (16 KB).
inline constexpr gp::USize kMaxInlineClipboardTextBytes = 16384u;

/// @brief Maximum byte capacity for inline headless binary MIME loopback storage (16 KB).
inline constexpr gp::USize kMaxInlineClipboardDataBytes = 16384u;

/// @brief Maximum UTF-8 byte length for a single MIME type identifier string (e.g., "application/x-gp-node").
inline constexpr gp::USize kMaxClipboardMimeLength = 128u;

/// @brief Read-only view over a binary or structured MIME payload retrieved from the OS clipboard.
struct ClipboardDataView
{
    /// @brief Pointer to the raw payload bytes (valid strictly for the duration of `ClipboardDataCallback`).
    const void* data{ nullptr };

    /// @brief Size of the payload buffer in bytes.
    gp::USize sizeBytes{ 0u };

    /// @brief UTF-8 MIME type string describing the payload format.
    StringView mimeType{};

    /// @brief Default constructor.
    [[nodiscard]] constexpr ClipboardDataView() noexcept = default;

    /// @brief Constructs a clipboard data view from raw memory and a MIME type view.
    /// @param[in] inData Pointer to payload bytes.
    /// @param[in] inSizeBytes Number of bytes in the payload.
    /// @param[in] inMimeType MIME type identifier string view.
    [[nodiscard]] constexpr ClipboardDataView(const void* inData, gp::USize inSizeBytes, StringView inMimeType) noexcept
        : data(inData)
        , sizeBytes(inSizeBytes)
        , mimeType(inMimeType)
    {}
};

/// @brief Scoped callback invoked by `IClipboard::readData()` while the OS clipboard buffer is locked in memory.
/// @param[in] userData Opaque user context pointer passed to `readData()`.
/// @param[in] view Read-only view of the retrieved clipboard bytes and MIME type.
using ClipboardDataCallback = void (*)(void* userData, const ClipboardDataView& view) noexcept;

}   // namespace gp::hal
