// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep

namespace gp::hal::event
{

/// @brief OS clipboard notification type.
enum class ClipboardAction : gp::UInt8
{
    None = 0,
    Updated,    // OS clipboard content changed externally
    Cancelled   // Clipboard ownership lost
};

/// @brief POD payload for EventCategory::Clipboard (8 bytes, zero implicit padding).
struct ClipboardEvent
{
    gp::UInt32 byteLength = 0u;     // Optional trailing payload length in TLV stream
    ClipboardAction action = ClipboardAction::None;
    gp::UInt8 isOwner = 0u;         // Non-zero if this engine instance owns the clipboard data
    gp::UInt8 mimeTypeCount = 0u;   // Number of available OS MIME formats
    gp::UInt8 reserved = 0u;
};

static_assert(sizeof(ClipboardEvent) == 8, "ClipboardEvent size mismatch!");

}   // namespace gp::hal::event
