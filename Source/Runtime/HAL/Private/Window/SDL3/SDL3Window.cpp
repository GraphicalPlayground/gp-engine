// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Window/SDL3/SDL3Window.hpp"

namespace GP
{

///
/// @section State queries
///

GP_NODISCARD bool FSDL3Window::IsOpen() const noexcept { return false; }

GP_NODISCARD bool FSDL3Window::IsValid() const noexcept { return false; }

GP_NODISCARD bool FSDL3Window::IsActive() const noexcept { return false; }

GP_NODISCARD bool FSDL3Window::IsFocused() const noexcept { return false; }

GP_NODISCARD bool FSDL3Window::IsMinimized() const noexcept { return false; }

GP_NODISCARD bool FSDL3Window::IsMaximized() const noexcept { return false; }

GP_NODISCARD bool FSDL3Window::IsResizable() const noexcept { return false; }

GP_NODISCARD bool FSDL3Window::IsFullscreen() const noexcept { return false; }

GP_NODISCARD bool FSDL3Window::IsBorderless() const noexcept { return false; }

GP_NODISCARD bool FSDL3Window::IsTransparent() const noexcept { return false; }

GP_NODISCARD bool FSDL3Window::IsAlwaysOnTop() const noexcept { return false; }

GP_NODISCARD bool FSDL3Window::IsHovered() const noexcept { return false; }

GP_NODISCARD bool FSDL3Window::IsVisible() const noexcept { return false; }

GP_NODISCARD bool FSDL3Window::IsHidden() const noexcept { return false; }

GP_NODISCARD bool FSDL3Window::IsVSyncEnabled() const noexcept { return false; }

GP_NODISCARD bool FSDL3Window::IsHighDPI() const noexcept { return false; }

GP_NODISCARD bool FSDL3Window::IsInputGrabbed() const noexcept { return false; }

GP_NODISCARD bool FSDL3Window::IsMouseGrabbed() const noexcept { return false; }

GP_NODISCARD bool FSDL3Window::IsKeyboardGrabbed() const noexcept { return false; }

GP_NODISCARD bool FSDL3Window::IsCursorVisible() const noexcept { return false; }

GP_NODISCARD bool FSDL3Window::IsCursorHidden() const noexcept { return false; }

GP_NODISCARD bool FSDL3Window::IsCursorConfined() const noexcept { return false; }

GP_NODISCARD bool FSDL3Window::IsOccluded() const noexcept { return false; }

///
/// @section Getters
///

GP_NODISCARD FIntPoint2D FSDL3Window::GetPosition() const noexcept { return FIntPoint2D{ 0, 0 }; }

GP_NODISCARD FIntPoint2D FSDL3Window::GetFullPosition() const noexcept { return FIntPoint2D{ 0, 0 }; }

GP_NODISCARD FIntExtent2D FSDL3Window::GetSize() const noexcept { return FIntExtent2D{ 0, 0 }; }

GP_NODISCARD FIntExtent2D FSDL3Window::GetFullSize() const noexcept { return FIntExtent2D{ 0, 0 }; }

GP_NODISCARD FIntExtent2D FSDL3Window::GetFramebufferSize() const noexcept { return FIntExtent2D{ 0, 0 }; }

GP_NODISCARD FIntExtent2D FSDL3Window::GetMinimumSize() const noexcept { return FIntExtent2D{ 0, 0 }; }

GP_NODISCARD FIntExtent2D FSDL3Window::GetMaximumSize() const noexcept { return FIntExtent2D{ 0, 0 }; }

GP_NODISCARD void* FSDL3Window::GetNativeHandle() const noexcept { return nullptr; }

GP_NODISCARD const IDisplay* FSDL3Window::GetCurrentDisplay() const noexcept { return nullptr; }

///
/// @section Setters
///

void FSDL3Window::SetPosition(const FIntPoint2D& position) noexcept { GP_UNUSED(position); }

void FSDL3Window::SetSize(const FIntExtent2D& size) noexcept { GP_UNUSED(size); }

void FSDL3Window::SetFullSize(const FIntExtent2D& size) noexcept { GP_UNUSED(size); }

void FSDL3Window::SetMinimumSize(const FIntExtent2D& size) noexcept { GP_UNUSED(size); }

void FSDL3Window::SetMaximumSize(const FIntExtent2D& size) noexcept { GP_UNUSED(size); }

///
/// @section Actions
///

void FSDL3Window::Open() {}

void FSDL3Window::Close() {}

void FSDL3Window::RequestClose() {}

void FSDL3Window::Show() {}

void FSDL3Window::Hide() {}

void FSDL3Window::Minimize() {}

void FSDL3Window::Maximize() {}

void FSDL3Window::Restore() {}

void FSDL3Window::Focus() {}

void FSDL3Window::RequestAttention() {}

}   // namespace GP
