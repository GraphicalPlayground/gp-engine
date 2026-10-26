// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#include "clipboard/SDL3Clipboard.hpp"
#include <SDL3/SDL.h>

namespace gp::hal::sdl3
{

namespace
{

/// @brief Computes bounded string length without `<cstring>`.
[[nodiscard]] constexpr gp::USize boundedStrLen(const char* str, gp::USize maxLen) noexcept
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

/// @brief Copies a `StringView` into a stack buffer and null-terminates it.
template <gp::USize Capacity>
constexpr const char* toNullTerminated(StringView src, char (&dst)[Capacity]) noexcept
{
    const gp::USize count = (src.size() < (Capacity - 1u)) ? src.size() : (Capacity - 1u);
    for (gp::USize i = 0u; i < count; ++i)
    {
        dst[i] = src[i];
    }
    dst[count] = '\0';
    return dst;
}

/// @brief Callback invoked by SDL3 when another application pastes our custom binary MIME payload.
const void* SDLCALL onSdlClipboardDataRequest(void* userdata, const char* mime_type, size_t* size) noexcept
{
    if (userdata == nullptr || mime_type == nullptr || size == nullptr) [[unlikely]]
    {
        return nullptr;
    }

    auto* self = static_cast<Clipboard*>(userdata);
    const StringView requestedMime(mime_type, boundedStrLen(mime_type, kMaxClipboardMimeLength));

    struct ExportQuery
    {
        const void* ptr{ nullptr };
        size_t length{ 0u };
    } query{};

    // Read directly from the inline loopback cache inside `generic::Clipboard`
    const ClipboardResult res = self->gp::hal::generic::Clipboard::readData(
        requestedMime,
        [](void* ctx, const ClipboardDataView& view) noexcept
    {
        auto* q = static_cast<ExportQuery*>(ctx);
        q->ptr = view.data;
        q->length = static_cast<size_t>(view.sizeBytes);
    },
        &query
    );

    if (res == ClipboardResult::Success && query.ptr != nullptr)
    {
        *size = query.length;
        return query.ptr;
    }

    *size = 0u;
    return nullptr;
}

}   // namespace

ClipboardResult Clipboard::setText(StringView text, ClipboardTarget target) noexcept
{
    // Update local inline cache first so `m_standardTextCache` / `m_primaryTextCache` is null-terminated
    (void)gp::hal::generic::Clipboard::setText(text, target);

    const bool ok = (target == ClipboardTarget::PrimarySelection) ? SDL_SetPrimarySelectionText(m_primaryTextCache)
                                                                  : SDL_SetClipboardText(m_standardTextCache);

    return ok ? ClipboardResult::Success : ClipboardResult::BackendError;
}

StringView Clipboard::getText(ClipboardTarget target) noexcept
{
    char* sdlText =
        (target == ClipboardTarget::PrimarySelection) ? SDL_GetPrimarySelectionText() : SDL_GetClipboardText();

    if (sdlText == nullptr) [[unlikely]]
    {
        if (target == ClipboardTarget::PrimarySelection)
        {
            m_primaryTextCache[0] = '\0';
            m_primaryTextLength = 0u;
        }
        else
        {
            m_standardTextCache[0] = '\0';
            m_standardTextLength = 0u;
        }
        return StringView{};
    }

    const gp::USize len = boundedStrLen(sdlText, kMaxInlineClipboardTextBytes - 1u);
    const StringView rawView(sdlText, len);

    if (target == ClipboardTarget::PrimarySelection)
    {
        cacheTextToBuffer(rawView, m_primaryTextCache, m_primaryTextLength);
        SDL_free(sdlText);
        return StringView(m_primaryTextCache, m_primaryTextLength);
    }

    cacheTextToBuffer(rawView, m_standardTextCache, m_standardTextLength);
    SDL_free(sdlText);
    return StringView(m_standardTextCache, m_standardTextLength);
}

gp::USize Clipboard::getTextBuffer(char* outBuffer, gp::USize capacity, ClipboardTarget target) noexcept
{
    if (outBuffer == nullptr || capacity == 0u) [[unlikely]]
    {
        return 0u;
    }

    char* sdlText =
        (target == ClipboardTarget::PrimarySelection) ? SDL_GetPrimarySelectionText() : SDL_GetClipboardText();

    if (sdlText == nullptr) [[unlikely]]
    {
        outBuffer[0] = '\0';
        return 0u;
    }

    const gp::USize maxCopy = capacity - 1u;
    gp::USize written = 0u;
    while (written < maxCopy && sdlText[written] != '\0')
    {
        outBuffer[written] = sdlText[written];
        ++written;
    }
    outBuffer[written] = '\0';

    SDL_free(sdlText);
    return written;
}

bool Clipboard::hasText(ClipboardTarget target) const noexcept
{
    return (target == ClipboardTarget::PrimarySelection) ? SDL_HasPrimarySelectionText() : SDL_HasClipboardText();
}

ClipboardResult Clipboard::setData(StringView mimeType, const void* data, gp::USize sizeBytes) noexcept
{
    // Store payload and null-terminated MIME string in our inline cache so `onSdlClipboardDataRequest` can serve it
    const ClipboardResult cacheRes = gp::hal::generic::Clipboard::setData(mimeType, data, sizeBytes);
    if (cacheRes != ClipboardResult::Success) [[unlikely]]
    {
        return cacheRes;
    }

    const char* mimeArray[1] = { m_binaryMimeCache };
    const bool ok = SDL_SetClipboardData(onSdlClipboardDataRequest, nullptr, this, mimeArray, 1u);

    return ok ? ClipboardResult::Success : ClipboardResult::BackendError;
}

bool Clipboard::hasData(StringView mimeType) const noexcept
{
    if (mimeType.isEmpty()) [[unlikely]]
    {
        return false;
    }

    char mimeBuf[kMaxClipboardMimeLength]{};
    return SDL_HasClipboardData(toNullTerminated(mimeType, mimeBuf));
}

ClipboardResult Clipboard::readData(StringView mimeType, ClipboardDataCallback callback, void* userData) noexcept
{
    if (mimeType.isEmpty()) [[unlikely]]
    {
        return ClipboardResult::UnsupportedMimeType;
    }

    char mimeBuf[kMaxClipboardMimeLength]{};
    const char* mimeCStr = toNullTerminated(mimeType, mimeBuf);

    if (!SDL_HasClipboardData(mimeCStr))
    {
        return ClipboardResult::UnsupportedMimeType;
    }

    size_t dataSize = 0u;
    void* sdlData = SDL_GetClipboardData(mimeCStr, &dataSize);
    if (sdlData == nullptr) [[unlikely]]
    {
        return ClipboardResult::Empty;
    }

    if (callback != nullptr)
    {
        const ClipboardDataView view(sdlData, static_cast<gp::USize>(dataSize), mimeType);
        callback(userData, view);
    }

    SDL_free(sdlData);
    return ClipboardResult::Success;
}

bool Clipboard::clear() noexcept
{
    (void)gp::hal::generic::Clipboard::clear();
    return SDL_ClearClipboardData();
}

}   // namespace gp::hal::sdl3
