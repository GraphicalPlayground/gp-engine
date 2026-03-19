// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Window/IWindow.hpp"

namespace GP
{

/// @brief
class FBaseWindow : public IWindow
{
public:
    virtual ~FBaseWindow() = default;

public:
    ///
    /// @section State queries
    ///

    GP_NODISCARD virtual bool IsOpen() const noexcept override;
    GP_NODISCARD virtual bool IsValid() const noexcept override;
    GP_NODISCARD virtual bool IsActive() const noexcept override;
    GP_NODISCARD virtual bool IsFocused() const noexcept override;
    GP_NODISCARD virtual bool IsMinimized() const noexcept override;
    GP_NODISCARD virtual bool IsMaximized() const noexcept override;
    GP_NODISCARD virtual bool IsResizable() const noexcept override;
    GP_NODISCARD virtual bool IsFullscreen() const noexcept override;
    GP_NODISCARD virtual bool IsBorderless() const noexcept override;
    GP_NODISCARD virtual bool IsTransparent() const noexcept override;
    GP_NODISCARD virtual bool IsAlwaysOnTop() const noexcept override;
    GP_NODISCARD virtual bool IsHovered() const noexcept override;
    GP_NODISCARD virtual bool IsVisible() const noexcept override;
    GP_NODISCARD virtual bool IsHidden() const noexcept override;
    GP_NODISCARD virtual bool IsVSyncEnabled() const noexcept override;
    GP_NODISCARD virtual bool IsHighDPI() const noexcept override;
    GP_NODISCARD virtual bool IsInputGrabbed() const noexcept override;
    GP_NODISCARD virtual bool IsMouseGrabbed() const noexcept override;
    GP_NODISCARD virtual bool IsKeyboardGrabbed() const noexcept override;
    GP_NODISCARD virtual bool IsCursorVisible() const noexcept override;
    GP_NODISCARD virtual bool IsCursorHidden() const noexcept override;
    GP_NODISCARD virtual bool IsCursorConfined() const noexcept override;
    GP_NODISCARD virtual bool IsOccluded() const noexcept override;

    ///
    /// @section Getters
    ///

    GP_NODISCARD virtual FIntPoint2D GetPosition() const noexcept override;
    GP_NODISCARD virtual FIntPoint2D GetFullPosition() const noexcept override;
    GP_NODISCARD virtual FIntExtent2D GetSize() const noexcept override;
    GP_NODISCARD virtual FIntExtent2D GetFullSize() const noexcept override;
    GP_NODISCARD virtual FIntExtent2D GetFramebufferSize() const noexcept override;
    GP_NODISCARD virtual FIntExtent2D GetMinimumSize() const noexcept override;
    GP_NODISCARD virtual FIntExtent2D GetMaximumSize() const noexcept override;
    GP_NODISCARD virtual void* GetNativeHandle() const noexcept override;
    GP_NODISCARD virtual const IDisplay* GetCurrentDisplay() const noexcept override;

    ///
    /// @section Setters
    ///

    virtual void SetPosition(const FIntPoint2D& position) noexcept override;
    virtual void SetPosition(const Int32 x, const Int32 y) noexcept override;
    virtual void SetSize(const FIntExtent2D& size) noexcept override;
    virtual void SetSize(const UInt32 width, const UInt32 height) noexcept override;
    virtual void SetFullSize(const FIntExtent2D& size) noexcept override;
    virtual void SetFullSize(const UInt32 width, const UInt32 height) noexcept override;
    virtual void SetSizeLimits(const FIntExtent2D& minimum, const FIntExtent2D& maximum) noexcept override;
    virtual void SetMinimumSize(const FIntExtent2D& size) noexcept override;
    virtual void SetMinimumSize(const UInt32 width, const UInt32 height) noexcept override;
    virtual void SetMaximumSize(const FIntExtent2D& size) noexcept override;
    virtual void SetMaximumSize(const UInt32 width, const UInt32 height) noexcept override;

    ///
    /// @section Actions
    ///

    virtual void Open() override;
    virtual void Close() override;
    virtual void RequestClose() override;
    virtual void Show() override;
    virtual void Hide() override;
    virtual void Minimize() override;
    virtual void Maximize() override;
    virtual void Restore() override;
    virtual void Focus() override;
    virtual void RequestAttention(EWindowFlashMode mode = EWindowFlashMode::UntilFocused) override;
    virtual void CenterOnDisplay(const IDisplay* display = nullptr) override;
    virtual void DispatchEvents() noexcept override;
};

}   // namespace GP
