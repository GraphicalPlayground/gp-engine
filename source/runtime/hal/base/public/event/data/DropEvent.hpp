// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep

namespace gp::hal::event
{

/// @brief Drag-and-drop sequence phase.
enum class DropAction : gp::UInt8
{
    None = 0,
    Begin,         // Cursor entered window carrying OS drag payload
    Position,      // Cursor moving across window with payload (updates x, y)
    FileDropped,   // User released mouse to drop a file (followed by UTF-8 file path)
    TextDropped,   // User released mouse to drop text (followed by UTF-8 string)
    Complete       // Drag-and-drop sequence finished
};

/// @brief Variable-length TLV descriptor payload for EventCategory::Drop (16 bytes).
/// @details For FileDropped and TextDropped, `byteLength` bytes of UTF-8 string data immediately
///          follow this struct in the SPSC byte ring buffer.
struct DropEvent
{
    gp::UInt32 windowId = 0u;
    gp::Float32 x = 0.0f;         // Drop cursor X relative to window client area
    gp::Float32 y = 0.0f;         // Drop cursor Y relative to window client area
    gp::UInt16 byteLength = 0u;   // Number of trailing UTF-8 bytes (0 for Begin/Position/Complete)
    DropAction action = DropAction::None;
    gp::UInt8 reserved = 0u;
};

static_assert(sizeof(DropEvent) == 16, "DropEvent size mismatch!");

}   // namespace gp::hal::event
