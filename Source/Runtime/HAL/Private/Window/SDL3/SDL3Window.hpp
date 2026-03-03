// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Window/BaseWindow.hpp"
#include "Window/WindowDesc.hpp"
#include <SDL3/SDL.h>

namespace GP
{

/// @brief
class FSDL3Window final : public FBaseWindow
{
private:
    static constexpr FIntExtent2D DefaultWindowSize{ 1280u, 720u };

private:
    FWindowDesc m_desc{};

    EWindowMode m_windowMode{ EWindowMode::Windowed };
    EWindowState m_windowState{ EWindowState::Normal };
    EWindowStyle m_windowStyle{ EWindowStyle::Default };
    ECursorMode m_cursorMode{ ECursorMode::Normal };
    ECursorShape m_cursorShape{ ECursorShape::Arrow };
    Float32 m_opacity{ 1.0f };
    // FString m_title{ "Graphical Playground" };
    FIntExtent2D m_minSize{ 0u, 0u };
    FIntExtent2D m_maxSize{ 0u, 0u };
    FIntExtent2D m_fullWindowSize{ DefaultWindowSize };
    FIntExtent2D m_contentSize{ DefaultWindowSize };
    FIntExtent2D m_framebufferSize{ DefaultWindowSize };
    FIntPoint2D m_position{ 0, 0 };
    Float32 m_aspectRatio{ static_cast<Float32>(DefaultWindowSize.width) /
                           static_cast<Float32>(DefaultWindowSize.height) };
    Float32 m_contentScale{ 1.0f };

    bool m_shouldClose{ false };
    bool m_isFocused{ false };
    bool m_isHovered{ false };
    bool m_isOccluded{ false };
    bool m_isHighDPI{ false };
    bool m_isVSync{ false };
    bool m_isKeyboardGrabbed{ false };
    bool m_isMouseGrabbed{ false };
    bool m_isHeadless{ false };

    SDL_Window* m_window{ nullptr };
    SDL_Cursor* m_cursor{ nullptr };

public:
    FSDL3Window(const FWindowDesc& desc) noexcept;
    ~FSDL3Window() noexcept;

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
    virtual void SetSize(const FIntExtent2D& size) noexcept override;
    virtual void SetFullSize(const FIntExtent2D& size) noexcept override;
    virtual void SetMinimumSize(const FIntExtent2D& size) noexcept override;
    virtual void SetMaximumSize(const FIntExtent2D& size) noexcept override;

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

private:
    void ResetToDefaultState() noexcept;
    void SyncStateWithSDL() noexcept;
    void ApplySDLCursorShape(ECursorShape shape) noexcept;
    void ApplySDLCursorMode(ECursorMode mode) noexcept;

private:
    static void BuildSDLFlags(const FWindowDesc& desc) noexcept;
};

}   // namespace GP
