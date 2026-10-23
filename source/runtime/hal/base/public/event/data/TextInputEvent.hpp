// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep

namespace gp::hal::event
{

/// @brief Distinguishes committed text from active IME (Input Method Editor) composition.
enum class TextInputAction : gp::UInt8
{
    None = 0,
    Committed,   // Final UTF-8 character(s) ready to be inserted into text fields
    Editing      // Active IME composition (e.g., typing Korean Hangul or Japanese Kanji)
};

/// @brief Variable-length TLV descriptor payload for EventCategory::TextInput (16 bytes).
/// @details Immediately followed by `byteLength` bytes of raw UTF-8 characters in the SPSC byte ring buffer.
struct TextInputEvent
{
    gp::UInt32 windowId = 0u;        // Window with active text input focus
    gp::Int32 cursorStart = 0;       // IME editing cursor start code-point offset
    gp::Int32 selectionLength = 0;   // IME editing selection length
    gp::UInt16 byteLength = 0u;      // Number of UTF-8 bytes immediately following this struct
    TextInputAction action = TextInputAction::None;
    gp::UInt8 reserved = 0u;
};

static_assert(sizeof(TextInputEvent) == 16, "TextInputEvent size mismatch!");

}   // namespace gp::hal::event
