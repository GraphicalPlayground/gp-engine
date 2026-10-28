// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "clipboard/IClipboard.hpp"
#include "CoreMinimal.hpp"
#include "GenericHAL.hpp"

namespace gp::hal::generic
{

/// @brief Platform-agnostic base and headless loopback implementation of `IClipboard`.
/// @details Maintains inline buffers for standard text, primary selection text, and binary MIME payloads
///          so `StringView` returns never dangle and headless unit tests execute deterministically.
class GP_HAL_GENERIC_API Clipboard : public gp::hal::IClipboard
{
protected:
    char m_standardTextCache[kMaxInlineClipboardTextBytes]{};
    gp::USize m_standardTextLength{ 0u };

    char m_primaryTextCache[kMaxInlineClipboardTextBytes]{};
    gp::USize m_primaryTextLength{ 0u };

    gp::UInt8 m_binaryDataCache[kMaxInlineClipboardDataBytes]{};
    gp::USize m_binaryDataLength{ 0u };
    char m_binaryMimeCache[kMaxClipboardMimeLength]{};
    gp::USize m_binaryMimeLength{ 0u };

    gp::UInt64 m_sequenceNumber{ 0ULL };

public:
    /// @brief Default constructor initializes an empty loopback clipboard.
    [[nodiscard]] constexpr Clipboard() noexcept = default;

    /// @brief Virtual destructor.
    ~Clipboard() noexcept override = default;

public:
    /// @copydoc IClipboard::setText()
    constexpr ClipboardResult
        setText(StringView text, ClipboardTarget target = ClipboardTarget::Standard) noexcept override
    {
        if (target == ClipboardTarget::PrimarySelection)
        {
            cacheTextToBuffer(text, m_primaryTextCache, m_primaryTextLength);
        }
        else
        {
            cacheTextToBuffer(text, m_standardTextCache, m_standardTextLength);
        }
        ++m_sequenceNumber;
        return ClipboardResult::Success;
    }

    /// @copydoc IClipboard::getText()
    [[nodiscard]] constexpr StringView getText(ClipboardTarget target = ClipboardTarget::Standard) noexcept override
    {
        if (target == ClipboardTarget::PrimarySelection)
        {
            return StringView(m_primaryTextCache, m_primaryTextLength);
        }
        return StringView(m_standardTextCache, m_standardTextLength);
    }

    /// @copydoc IClipboard::getTextBuffer()
    constexpr gp::USize getTextBuffer(
        char* outBuffer, gp::USize capacity, ClipboardTarget target = ClipboardTarget::Standard
    ) noexcept override
    {
        if (outBuffer == nullptr || capacity == 0u) [[unlikely]]
        {
            return 0u;
        }

        const StringView src = getText(target);
        const gp::USize copyLen = (src.size() < (capacity - 1u)) ? src.size() : (capacity - 1u);
        for (gp::USize i = 0u; i < copyLen; ++i)
        {
            outBuffer[i] = src[i];
        }
        outBuffer[copyLen] = '\0';
        return copyLen;
    }

    /// @copydoc IClipboard::hasText()
    [[nodiscard]] constexpr bool hasText(ClipboardTarget target = ClipboardTarget::Standard) const noexcept override
    {
        return (target == ClipboardTarget::PrimarySelection) ? (m_primaryTextLength > 0u) : (m_standardTextLength > 0u);
    }

    /// @copydoc IClipboard::setData()
    constexpr ClipboardResult setData(StringView mimeType, const void* data, gp::USize sizeBytes) noexcept override
    {
        if (mimeType.isEmpty() || (data == nullptr && sizeBytes > 0u)) [[unlikely]]
        {
            return ClipboardResult::BackendError;
        }

        if (sizeBytes > kMaxInlineClipboardDataBytes) [[unlikely]]
        {
            return ClipboardResult::PayloadTooLarge;
        }

        const gp::USize mimeCopy =
            (mimeType.size() < (kMaxClipboardMimeLength - 1u)) ? mimeType.size() : (kMaxClipboardMimeLength - 1u);
        for (gp::USize i = 0u; i < mimeCopy; ++i)
        {
            m_binaryMimeCache[i] = mimeType[i];
        }
        m_binaryMimeCache[mimeCopy] = '\0';
        m_binaryMimeLength = mimeCopy;

        const auto* srcBytes = static_cast<const gp::UInt8*>(data);
        for (gp::USize i = 0u; i < sizeBytes; ++i)
        {
            m_binaryDataCache[i] = srcBytes[i];
        }
        m_binaryDataLength = sizeBytes;
        ++m_sequenceNumber;

        return ClipboardResult::Success;
    }

    /// @copydoc IClipboard::hasData()
    [[nodiscard]] constexpr bool hasData(StringView mimeType) const noexcept override
    {
        if (mimeType.isEmpty() || m_binaryMimeLength == 0u)
        {
            return false;
        }

        const StringView cachedMime(m_binaryMimeCache, m_binaryMimeLength);
        return cachedMime == mimeType;
    }

    /// @copydoc IClipboard::readData()
    constexpr ClipboardResult
        readData(StringView mimeType, ClipboardDataCallback callback, void* userData = nullptr) noexcept override
    {
        if (!hasData(mimeType))
        {
            return ClipboardResult::UnsupportedMimeType;
        }

        if (callback != nullptr)
        {
            const ClipboardDataView view(
                m_binaryDataCache, m_binaryDataLength, StringView(m_binaryMimeCache, m_binaryMimeLength)
            );
            callback(userData, view);
        }
        return ClipboardResult::Success;
    }

    /// @copydoc IClipboard::clear()
    constexpr bool clear() noexcept override
    {
        m_standardTextCache[0] = '\0';
        m_standardTextLength = 0u;
        m_primaryTextCache[0] = '\0';
        m_primaryTextLength = 0u;
        m_binaryDataLength = 0u;
        m_binaryMimeCache[0] = '\0';
        m_binaryMimeLength = 0u;
        ++m_sequenceNumber;
        return true;
    }

    /// @copydoc IClipboard::getSequenceNumber()
    [[nodiscard]] constexpr gp::UInt64 getSequenceNumber() const noexcept override
    {
        return m_sequenceNumber;
    }

    /// @copydoc IClipboard::onClipboardEvent()
    constexpr void onClipboardEvent([[maybe_unused]] const event::ClipboardEvent& ev) noexcept override
    {
        ++m_sequenceNumber;
    }

protected:
    /// @brief Copies a `StringView` into a fixed capacity char buffer and updates its cached length.
    /// @param[in] src Source UTF-8 string view.
    /// @param[out] dstBuffer Target fixed-size character array.
    /// @param[out] outLength Receives the number of bytes written (excluding null terminator).
    static constexpr void cacheTextToBuffer(
        StringView src, char (&dstBuffer)[kMaxInlineClipboardTextBytes], gp::USize& outLength
    ) noexcept
    {
        const gp::USize copyLen =
            (src.size() < (kMaxInlineClipboardTextBytes - 1u)) ? src.size() : (kMaxInlineClipboardTextBytes - 1u);

        for (gp::USize i = 0u; i < copyLen; ++i)
        {
            dstBuffer[i] = src[i];
        }
        dstBuffer[copyLen] = '\0';
        outLength = copyLen;
    }
};

}   // namespace gp::hal::generic
