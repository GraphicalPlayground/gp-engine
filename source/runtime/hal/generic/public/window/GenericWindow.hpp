// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "templates/Enums.hpp"
#include "window/IWindow.hpp"

namespace gp::hal::generic
{

/// @brief Platform-agnostic base window implementation maintaining the Game Thread shadow state (ADR-0027).
/// @details Provides complete, zero-cost `constexpr` implementations for all `IWindow` state queries and
///          processes incoming `event::WindowEvent` packets to keep cached geometry and flags synchronized.
class Window : public gp::hal::IWindow
{
protected:
    /// @brief Fixed capacity for the internal null-terminated UTF-8 window title buffer.
    static constexpr gp::USize kMaxTitleBytes = 256u;

    WindowDesc m_desc{};
    NativeWindowHandle m_nativeHandle{ NativeWindowBackend::Unknown };

    // Shadow geometry & display metrics
    Extent2D m_framebufferSize{ 0u, 0u };
    Int32 m_borderTop{ 0 };
    Int32 m_borderLeft{ 0 };
    Int32 m_borderBottom{ 0 };
    Int32 m_borderRight{ 0 };
    Float32 m_contentScale{ 1.0f };
    WindowId m_windowId{ kInvalidWindowId };
    gp::UInt32 m_currentDisplayId{ 0u };

    // Shadow state flags
    bool m_isOpen{ false };
    bool m_isCloseRequested{ false };
    bool m_isFocused{ false };
    bool m_isHovered{ false };
    bool m_isHDREnabled{ false };

    // Owned null-terminated UTF-8 title buffer so `m_desc.title` never dangles
    char m_titleBuffer[kMaxTitleBytes]{ "Graphical Playground" };

public:
    /// @brief Default constructor initializes the window with default shadow state values.
    [[nodiscard]] explicit constexpr Window() noexcept
    {
        cacheTitleString(m_desc.title);
        m_framebufferSize = m_desc.size;
    }

    /// @brief Initializes the window with the specified description.
    /// @param[in] desc The window description to initialize the window with.
    [[nodiscard]] explicit constexpr Window(const WindowDesc& desc) noexcept
        : m_desc(desc)
        , m_framebufferSize(desc.size)
        , m_isFocused(desc.startFocused && desc.startVisible && !desc.useHeadless)
    {
        cacheTitleString(desc.title);
    }

    /// @brief Virtual destructor.
    ~Window() noexcept override = default;

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) = delete;
    Window& operator=(Window&&) = delete;

public:
    /// @copydoc IWindow::getId()
    [[nodiscard]] constexpr WindowId getId() const noexcept override
    {
        return m_windowId;
    }

    /// @copydoc IWindow::isOpen()
    [[nodiscard]] constexpr bool isOpen() const noexcept override
    {
        return m_isOpen;
    }

    /// @copydoc IWindow::isValid()
    [[nodiscard]] constexpr bool isValid() const noexcept override
    {
        return m_isOpen && (m_windowId != kInvalidWindowId);
    }

    /// @copydoc IWindow::isCloseRequested()
    [[nodiscard]] constexpr bool isCloseRequested() const noexcept override
    {
        return m_isCloseRequested;
    }

    /// @copydoc IWindow::isActive()
    [[nodiscard]] constexpr bool isActive() const noexcept override
    {
        return m_isOpen && isVisible() && m_isFocused;
    }

    /// @copydoc IWindow::isFocused()
    [[nodiscard]] constexpr bool isFocused() const noexcept override
    {
        return m_isFocused;
    }

    /// @copydoc IWindow::isMinimized()
    [[nodiscard]] constexpr bool isMinimized() const noexcept override
    {
        return m_desc.initialState == WindowState::Minimized;
    }

    /// @copydoc IWindow::isMaximized()
    [[nodiscard]] constexpr bool isMaximized() const noexcept override
    {
        return m_desc.initialState == WindowState::Maximized;
    }

    /// @copydoc IWindow::isResizable()
    [[nodiscard]] constexpr bool isResizable() const noexcept override
    {
        return (m_desc.style & WindowStyle::Resizable) != WindowStyle::None;
    }

    /// @copydoc IWindow::isFullscreen()
    [[nodiscard]] constexpr bool isFullscreen() const noexcept override
    {
        return m_desc.mode == WindowMode::Fullscreen || m_desc.mode == WindowMode::FullscreenDesktop;
    }

    /// @copydoc IWindow::isBorderless()
    [[nodiscard]] constexpr bool isBorderless() const noexcept override
    {
        return m_desc.mode == WindowMode::Borderless || (m_desc.style & WindowStyle::TitleBar) == WindowStyle::None;
    }

    /// @copydoc IWindow::isTransparent()
    [[nodiscard]] constexpr bool isTransparent() const noexcept override
    {
        return (m_desc.style & WindowStyle::Transparent) != WindowStyle::None;
    }

    /// @copydoc IWindow::isAlwaysOnTop()
    [[nodiscard]] constexpr bool isAlwaysOnTop() const noexcept override
    {
        return (m_desc.style & WindowStyle::AlwaysOnTop) != WindowStyle::None;
    }

    /// @copydoc IWindow::isHovered()
    [[nodiscard]] constexpr bool isHovered() const noexcept override
    {
        return m_isHovered;
    }

    /// @copydoc IWindow::isVisible()
    [[nodiscard]] constexpr bool isVisible() const noexcept override
    {
        return m_isOpen && !m_desc.useHeadless && m_desc.startVisible && m_desc.initialState != WindowState::Hidden &&
               m_desc.initialState != WindowState::Minimized;
    }

    /// @copydoc IWindow::isHidden()
    [[nodiscard]] constexpr bool isHidden() const noexcept override
    {
        return !isVisible();
    }

    /// @copydoc IWindow::isVSyncEnabled()
    [[nodiscard]] constexpr bool isVSyncEnabled() const noexcept override
    {
        return m_desc.useVSync;
    }

    /// @copydoc IWindow::isHighDPI()
    [[nodiscard]] constexpr bool isHighDPI() const noexcept override
    {
        return m_desc.useHighDPI;
    }

    /// @copydoc IWindow::isHDREnabled()
    [[nodiscard]] constexpr bool isHDREnabled() const noexcept override
    {
        return m_isHDREnabled;
    }

    /// @copydoc IWindow::isInputGrabbed()
    [[nodiscard]] constexpr bool isInputGrabbed() const noexcept override
    {
        return isMouseGrabbed() || isKeyboardGrabbed();
    }

    /// @copydoc IWindow::isMouseGrabbed()
    [[nodiscard]] constexpr bool isMouseGrabbed() const noexcept override
    {
        return m_desc.grabMouse || m_desc.cursorMode == CursorMode::Confined || m_desc.cursorMode == CursorMode::Locked;
    }

    /// @copydoc IWindow::isKeyboardGrabbed()
    [[nodiscard]] constexpr bool isKeyboardGrabbed() const noexcept override
    {
        return m_desc.grabKeyboard;
    }

    /// @copydoc IWindow::isCursorVisible()
    [[nodiscard]] constexpr bool isCursorVisible() const noexcept override
    {
        return m_desc.cursorMode == CursorMode::Normal || m_desc.cursorMode == CursorMode::Confined;
    }

    /// @copydoc IWindow::isCursorHidden()
    [[nodiscard]] constexpr bool isCursorHidden() const noexcept override
    {
        return !isCursorVisible();
    }

    /// @copydoc IWindow::isCursorConfined()
    [[nodiscard]] constexpr bool isCursorConfined() const noexcept override
    {
        return m_desc.cursorMode == CursorMode::Confined || m_desc.cursorMode == CursorMode::Locked;
    }

    /// @copydoc IWindow::isCursorLocked()
    [[nodiscard]] constexpr bool isCursorLocked() const noexcept override
    {
        return m_desc.cursorMode == CursorMode::Locked;
    }

    /// @copydoc IWindow::isOccluded()
    [[nodiscard]] constexpr bool isOccluded() const noexcept override
    {
        return m_desc.initialState == WindowState::Occluded;
    }

    /// @copydoc IWindow::getWindowState()
    [[nodiscard]] constexpr WindowState getWindowState() const noexcept override
    {
        return m_desc.initialState;
    }

    /// @copydoc IWindow::getWindowMode()
    [[nodiscard]] constexpr WindowMode getWindowMode() const noexcept override
    {
        return m_desc.mode;
    }

    /// @copydoc IWindow::getCursorMode()
    [[nodiscard]] constexpr CursorMode getCursorMode() const noexcept override
    {
        return m_desc.cursorMode;
    }

    /// @copydoc IWindow::getCursorShape()
    [[nodiscard]] constexpr CursorShape getCursorShape() const noexcept override
    {
        return m_desc.cursorShape;
    }

    /// @copydoc IWindow::getPosition()
    [[nodiscard]] constexpr Point2D getPosition() const noexcept override
    {
        return m_desc.position;
    }

    /// @copydoc IWindow::getFullPosition()
    [[nodiscard]] constexpr Point2D getFullPosition() const noexcept override
    {
        return Point2D(m_desc.position.x - m_borderLeft, m_desc.position.y - m_borderTop);
    }

    /// @copydoc IWindow::getSize()
    [[nodiscard]] constexpr Extent2D getSize() const noexcept override
    {
        return m_desc.size;
    }

    /// @copydoc IWindow::getFullSize()
    [[nodiscard]] constexpr Extent2D getFullSize() const noexcept override
    {
        const UInt32 extraW =
            static_cast<UInt32>((m_borderLeft + m_borderRight) > 0 ? (m_borderLeft + m_borderRight) : 0);
        const UInt32 extraH =
            static_cast<UInt32>((m_borderTop + m_borderBottom) > 0 ? (m_borderTop + m_borderBottom) : 0);
        return Extent2D(m_desc.size.width + extraW, m_desc.size.height + extraH);
    }

    /// @copydoc IWindow::getFramebufferSize()
    [[nodiscard]] constexpr Extent2D getFramebufferSize() const noexcept override
    {
        return m_framebufferSize;
    }

    /// @copydoc IWindow::getMinimumSize()
    [[nodiscard]] constexpr Extent2D getMinimumSize() const noexcept override
    {
        return m_desc.minimumSize;
    }

    /// @copydoc IWindow::getMaximumSize()
    [[nodiscard]] constexpr Extent2D getMaximumSize() const noexcept override
    {
        return m_desc.maximumSize;
    }

    /// @copydoc IWindow::getNativeHandle()
    [[nodiscard]] constexpr NativeWindowHandle getNativeHandle() const noexcept override
    {
        return m_nativeHandle;
    }

    /// @copydoc IWindow::getCurrentDisplay()
    [[nodiscard]] constexpr const IDisplay* getCurrentDisplay() const noexcept override
    {
        return m_desc.targetDisplay;
    }

    /// @copydoc IWindow::getCurrentDisplayId()
    [[nodiscard]] constexpr gp::UInt32 getCurrentDisplayId() const noexcept override
    {
        return m_currentDisplayId;
    }

    /// @copydoc IWindow::getContentScale()
    [[nodiscard]] constexpr Float32 getContentScale() const noexcept override
    {
        return m_contentScale;
    }

    /// @copydoc IWindow::getOpacity()
    [[nodiscard]] constexpr Float32 getOpacity() const noexcept override
    {
        return m_desc.opacity;
    }

    /// @copydoc IWindow::getTitle()
    [[nodiscard]] constexpr StringView getTitle() const noexcept override
    {
        return m_desc.title;
    }

    /// @copydoc IWindow::setPosition(const Point2D&)
    constexpr void setPosition(const Point2D& position) noexcept override
    {
        m_desc.position = position;
    }

    /// @copydoc IWindow::setPosition(Int32, Int32)
    constexpr void setPosition(Int32 x, Int32 y) noexcept override
    {
        setPosition(Point2D(x, y));
    }

    /// @copydoc IWindow::setSize(const Extent2D&)
    constexpr void setSize(const Extent2D& size) noexcept override
    {
        m_desc.size = size;
    }

    /// @copydoc IWindow::setSize(UInt32, UInt32)
    constexpr void setSize(UInt32 width, UInt32 height) noexcept override
    {
        setSize(Extent2D(width, height));
    }

    /// @copydoc IWindow::setFullSize(const Extent2D&)
    constexpr void setFullSize(const Extent2D& size) noexcept override
    {
        const UInt32 borderW =
            static_cast<UInt32>((m_borderLeft + m_borderRight) > 0 ? (m_borderLeft + m_borderRight) : 0);
        const UInt32 borderH =
            static_cast<UInt32>((m_borderTop + m_borderBottom) > 0 ? (m_borderTop + m_borderBottom) : 0);
        const UInt32 clientW = (size.width > borderW) ? (size.width - borderW) : 1u;
        const UInt32 clientH = (size.height > borderH) ? (size.height - borderH) : 1u;
        setSize(Extent2D(clientW, clientH));
    }

    /// @copydoc IWindow::setFullSize(UInt32, UInt32)
    constexpr void setFullSize(UInt32 width, UInt32 height) noexcept override
    {
        setFullSize(Extent2D(width, height));
    }

    /// @copydoc IWindow::setSizeLimits()
    constexpr void setSizeLimits(const Extent2D& minimum, const Extent2D& maximum) noexcept override
    {
        setMinimumSize(minimum);
        setMaximumSize(maximum);
    }

    /// @copydoc IWindow::setMinimumSize(const Extent2D&)
    constexpr void setMinimumSize(const Extent2D& size) noexcept override
    {
        m_desc.minimumSize = size;
    }

    /// @copydoc IWindow::setMinimumSize(UInt32, UInt32)
    constexpr void setMinimumSize(UInt32 width, UInt32 height) noexcept override
    {
        setMinimumSize(Extent2D(width, height));
    }

    /// @copydoc IWindow::setMaximumSize(const Extent2D&)
    constexpr void setMaximumSize(const Extent2D& size) noexcept override
    {
        m_desc.maximumSize = size;
    }

    /// @copydoc IWindow::setMaximumSize(UInt32, UInt32)
    constexpr void setMaximumSize(UInt32 width, UInt32 height) noexcept override
    {
        setMaximumSize(Extent2D(width, height));
    }

    /// @copydoc IWindow::setTitle()
    constexpr void setTitle(StringView title) noexcept override
    {
        cacheTitleString(title);
    }

    /// @copydoc IWindow::setOpacity()
    constexpr void setOpacity(Float32 opacity) noexcept override
    {
        m_desc.opacity = (opacity < 0.0f) ? 0.0f : ((opacity > 1.0f) ? 1.0f : opacity);
    }

    /// @copydoc IWindow::setResizable()
    constexpr void setResizable(bool resizable) noexcept override
    {
        if (resizable)
        {
            m_desc.style |= WindowStyle::Resizable;
        }
        else
        {
            m_desc.style &= ~WindowStyle::Resizable;
        }
    }

    /// @copydoc IWindow::setAlwaysOnTop()
    constexpr void setAlwaysOnTop(bool alwaysOnTop) noexcept override
    {
        if (alwaysOnTop)
        {
            m_desc.style |= WindowStyle::AlwaysOnTop;
        }
        else
        {
            m_desc.style &= ~WindowStyle::AlwaysOnTop;
        }
    }

    /// @copydoc IWindow::setVSyncEnabled()
    constexpr void setVSyncEnabled(bool enabled) noexcept override
    {
        m_desc.useVSync = enabled;
    }

    /// @copydoc IWindow::setCursorMode()
    constexpr void setCursorMode(CursorMode mode) noexcept override
    {
        m_desc.cursorMode = mode;
    }

    /// @copydoc IWindow::setCursorVisible()
    constexpr void setCursorVisible(bool visible) noexcept override
    {
        if (visible)
        {
            if (m_desc.cursorMode == CursorMode::Hidden || m_desc.cursorMode == CursorMode::Locked)
            {
                setCursorMode(CursorMode::Normal);
            }
        }
        else
        {
            if (m_desc.cursorMode == CursorMode::Normal)
            {
                setCursorMode(CursorMode::Hidden);
            }
        }
    }

    /// @copydoc IWindow::setCursorConfined()
    constexpr void setCursorConfined(bool confined) noexcept override
    {
        setCursorMode(confined ? CursorMode::Confined : CursorMode::Normal);
    }

    /// @copydoc IWindow::setMouseGrabbed()
    constexpr void setMouseGrabbed(bool grabbed) noexcept override
    {
        m_desc.grabMouse = grabbed;
    }

    /// @copydoc IWindow::setKeyboardGrabbed()
    constexpr void setKeyboardGrabbed(bool grabbed) noexcept override
    {
        m_desc.grabKeyboard = grabbed;
    }

    /// @copydoc IWindow::setCursorShape()
    constexpr void setCursorShape(CursorShape shape) noexcept override
    {
        m_desc.cursorShape = shape;
    }

    /// @copydoc IWindow::setCursorPosition()
    constexpr void setCursorPosition([[maybe_unused]] const Point2D& position) noexcept override
    {}

    /// @copydoc IWindow::setWindowMode()
    constexpr void setWindowMode(WindowMode mode, const IDisplay* targetDisplay = nullptr) noexcept override
    {
        m_desc.mode = mode;
        if (targetDisplay != nullptr)
        {
            m_desc.targetDisplay = targetDisplay;
        }
    }

    /// @copydoc IWindow::open()
    constexpr void open() noexcept override
    {}

    /// @copydoc IWindow::close()
    constexpr void close() noexcept override
    {
        m_isOpen = false;
        m_windowId = kInvalidWindowId;
        m_nativeHandle = NativeWindowHandle{ NativeWindowBackend::Unknown };
    }

    /// @copydoc IWindow::requestClose()
    constexpr void requestClose() noexcept override
    {
        m_isCloseRequested = true;
    }

    /// @copydoc IWindow::clearCloseRequest()
    constexpr void clearCloseRequest() noexcept override
    {
        m_isCloseRequested = false;
    }

    /// @copydoc IWindow::show()
    constexpr void show() noexcept override
    {
        if (!m_desc.useHeadless)
        {
            m_desc.startVisible = true;
            if (m_desc.initialState == WindowState::Hidden)
            {
                m_desc.initialState = WindowState::Normal;
            }
        }
    }

    /// @copydoc IWindow::hide()
    constexpr void hide() noexcept override
    {
        m_desc.startVisible = false;
        m_desc.initialState = WindowState::Hidden;
    }

    /// @copydoc IWindow::minimize()
    constexpr void minimize() noexcept override
    {
        m_desc.initialState = WindowState::Minimized;
    }

    /// @copydoc IWindow::maximize()
    constexpr void maximize() noexcept override
    {
        m_desc.initialState = WindowState::Maximized;
    }

    /// @copydoc IWindow::restore()
    constexpr void restore() noexcept override
    {
        m_desc.initialState = WindowState::Normal;
    }

    /// @copydoc IWindow::focus()
    constexpr void focus() noexcept override
    {
        if (isVisible())
        {
            m_isFocused = true;
        }
    }

    /// @copydoc IWindow::requestAttention()
    constexpr void
        requestAttention([[maybe_unused]] WindowFlashMode mode = WindowFlashMode::UntilFocused) noexcept override
    {}

    /// @copydoc IWindow::centerOnDisplay()
    constexpr void centerOnDisplay([[maybe_unused]] const IDisplay* display = nullptr) noexcept override
    {}

    /// @copydoc IWindow::onWindowEvent()
    constexpr void onWindowEvent(const event::WindowEvent& ev) noexcept override
    {
        if (ev.windowId != m_windowId && m_windowId != kInvalidWindowId) [[unlikely]]
        {
            return;
        }

        switch (ev.action)
        {
        case event::WindowAction::Shown:
            m_desc.startVisible = true;
            if (m_desc.initialState == WindowState::Hidden)
            {
                m_desc.initialState = WindowState::Normal;
            }
            break;

        case event::WindowAction::Hidden:
            m_desc.startVisible = false;
            m_desc.initialState = WindowState::Hidden;
            m_isFocused = false;
            m_isHovered = false;
            break;

        case event::WindowAction::Moved:
            m_desc.position = ev.position;
            break;

        case event::WindowAction::Resized:
            m_desc.size = ev.extent;
            if (!m_desc.useHighDPI)
            {
                m_framebufferSize = ev.extent;
            }
            break;

        case event::WindowAction::PixelSizeChanged:
            m_framebufferSize = ev.extent;
            break;

        case event::WindowAction::StateChanged:
            m_desc.initialState = ev.state;
            if (ev.state == WindowState::Minimized || ev.state == WindowState::Hidden)
            {
                m_isFocused = false;
                m_isHovered = false;
            }
            break;

        case event::WindowAction::ModeChanged:
            m_desc.mode = ev.mode;
            break;

        case event::WindowAction::MouseEntered:
            m_isHovered = true;
            break;

        case event::WindowAction::MouseLeft:
            m_isHovered = false;
            break;

        case event::WindowAction::FocusGained:
            m_isFocused = true;
            break;

        case event::WindowAction::FocusLost:
            m_isFocused = false;
            break;

        case event::WindowAction::CloseRequested:
            m_isCloseRequested = true;
            break;

        case event::WindowAction::DisplayChanged:
            m_currentDisplayId = ev.display.displayId;
            break;

        case event::WindowAction::DPIChanged:
            m_contentScale = ev.dpi.scaleX;
            break;

        case event::WindowAction::HDRStateChanged:
            m_isHDREnabled = (ev.isHDREnabled != 0u);
            break;

        case event::WindowAction::Exposed:
        case event::WindowAction::None:
        default:
            break;
        }
    }

protected:
    /// @brief Safely copies a `StringView` into the internal null-terminated `m_titleBuffer` and rebinds
    /// `m_desc.title`.
    /// @param[in] title Source UTF-8 string view.
    constexpr void cacheTitleString(StringView title) noexcept
    {
        const gp::USize maxCopy = (title.size() < (kMaxTitleBytes - 1u)) ? title.size() : (kMaxTitleBytes - 1u);
        for (gp::USize i = 0u; i < maxCopy; ++i)
        {
            m_titleBuffer[i] = title[i];
        }
        m_titleBuffer[maxCopy] = '\0';
        m_desc.title = StringView(m_titleBuffer, maxCopy);
    }
};

}   // namespace gp::hal::generic
