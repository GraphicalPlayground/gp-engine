// Copyright (c) - Graphical Playground. All rights reserved.

#include "Window/BaseWindow.hpp"

namespace GP
{

///
/// @section State queries
/// @note These default implementations return false for all queries. Derived classes should override these to provide
///

GP_NODISCARD bool FBaseWindow::IsOpen() const noexcept { return false; }

GP_NODISCARD bool FBaseWindow::IsValid() const noexcept { return false; }

GP_NODISCARD bool FBaseWindow::IsActive() const noexcept { return false; }

GP_NODISCARD bool FBaseWindow::IsFocused() const noexcept { return false; }

GP_NODISCARD bool FBaseWindow::IsMinimized() const noexcept { return false; }

GP_NODISCARD bool FBaseWindow::IsMaximized() const noexcept { return false; }

GP_NODISCARD bool FBaseWindow::IsResizable() const noexcept { return false; }

GP_NODISCARD bool FBaseWindow::IsFullscreen() const noexcept { return false; }

GP_NODISCARD bool FBaseWindow::IsBorderless() const noexcept { return false; }

GP_NODISCARD bool FBaseWindow::IsTransparent() const noexcept { return false; }

GP_NODISCARD bool FBaseWindow::IsAlwaysOnTop() const noexcept { return false; }

GP_NODISCARD bool FBaseWindow::IsHovered() const noexcept { return false; }

GP_NODISCARD bool FBaseWindow::IsVisible() const noexcept { return false; }

GP_NODISCARD bool FBaseWindow::IsHidden() const noexcept { return false; }

GP_NODISCARD bool FBaseWindow::IsVSyncEnabled() const noexcept { return false; }

GP_NODISCARD bool FBaseWindow::IsHighDPI() const noexcept { return false; }

GP_NODISCARD bool FBaseWindow::IsInputGrabbed() const noexcept { return false; }

GP_NODISCARD bool FBaseWindow::IsMouseGrabbed() const noexcept { return false; }

GP_NODISCARD bool FBaseWindow::IsKeyboardGrabbed() const noexcept { return false; }

GP_NODISCARD bool FBaseWindow::IsCursorVisible() const noexcept { return false; }

GP_NODISCARD bool FBaseWindow::IsCursorHidden() const noexcept { return false; }

GP_NODISCARD bool FBaseWindow::IsCursorConfined() const noexcept { return false; }

GP_NODISCARD bool FBaseWindow::IsOccluded() const noexcept { return false; }

///
/// @section Getters
/// @note These default implementations return {0, 0} for all position/size queries. Derived classes should override
///       these to provide actual values.
///

GP_NODISCARD FIntPoint2D FBaseWindow::GetPosition() const noexcept { return FIntPoint2D{ 0, 0 }; }

GP_NODISCARD FIntPoint2D FBaseWindow::GetFullPosition() const noexcept { return FIntPoint2D{ 0, 0 }; }

GP_NODISCARD FIntExtent2D FBaseWindow::GetSize() const noexcept { return FIntExtent2D{ 0, 0 }; }

GP_NODISCARD FIntExtent2D FBaseWindow::GetFullSize() const noexcept { return FIntExtent2D{ 0, 0 }; }

GP_NODISCARD FIntExtent2D FBaseWindow::GetFramebufferSize() const noexcept { return FIntExtent2D{ 0, 0 }; }

GP_NODISCARD FIntExtent2D FBaseWindow::GetMinimumSize() const noexcept { return FIntExtent2D{ 0, 0 }; }

GP_NODISCARD FIntExtent2D FBaseWindow::GetMaximumSize() const noexcept { return FIntExtent2D{ 0, 0 }; }

GP_NODISCARD void* FBaseWindow::GetNativeHandle() const noexcept { return nullptr; }

GP_NODISCARD const IDisplay* FBaseWindow::GetCurrentDisplay() const noexcept { return nullptr; }

///
/// @section Setters
/// @note These default implementations do nothing. Derived classes should override these to actually change the
///

void FBaseWindow::SetPosition(const FIntPoint2D& position) noexcept { GP_UNUSED(position); }

void FBaseWindow::SetPosition(const Int32 x, const Int32 y) noexcept { SetPosition(FIntPoint2D{ x, y }); }

void FBaseWindow::SetSize(const FIntExtent2D& size) noexcept { GP_UNUSED(size); }

void FBaseWindow::SetSize(const UInt32 width, const UInt32 height) noexcept { SetSize(FIntExtent2D{ width, height }); }

void FBaseWindow::SetFullSize(const FIntExtent2D& size) noexcept { GP_UNUSED(size); }

void FBaseWindow::SetFullSize(const UInt32 width, const UInt32 height) noexcept
{
    SetFullSize(FIntExtent2D{ width, height });
}

void FBaseWindow::SetSizeLimits(const FIntExtent2D& minimum, const FIntExtent2D& maximum) noexcept
{
    SetMinimumSize(minimum);
    SetMaximumSize(maximum);
}

void FBaseWindow::SetMinimumSize(const FIntExtent2D& size) noexcept { GP_UNUSED(size); }

void FBaseWindow::SetMinimumSize(const UInt32 width, const UInt32 height) noexcept
{
    SetMinimumSize(FIntExtent2D{ width, height });
}

void FBaseWindow::SetMaximumSize(const FIntExtent2D& size) noexcept { GP_UNUSED(size); }

void FBaseWindow::SetMaximumSize(const UInt32 width, const UInt32 height) noexcept
{
    SetMaximumSize(FIntExtent2D{ width, height });
}

///
/// @section Actions
/// @note These default implementations do nothing. Derived classes should override these to actually perform the
///

void FBaseWindow::Open() {}

void FBaseWindow::Close() {}

void FBaseWindow::RequestClose() {}

void FBaseWindow::Show() {}

void FBaseWindow::Hide() {}

void FBaseWindow::Minimize() {}

void FBaseWindow::Maximize() {}

void FBaseWindow::Restore() {}

void FBaseWindow::Focus() {}

void FBaseWindow::RequestAttention(EWindowFlashMode mode) { GP_UNUSED(mode); }

void FBaseWindow::CenterOnDisplay(const IDisplay* display) { GP_UNUSED(display); }

void FBaseWindow::PollEvents() noexcept {}

}   // namespace GP
