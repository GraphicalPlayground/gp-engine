// Copyright (c) - Graphical Playground. All rights reserved.

#include "Windowing/BaseWindow.hpp"

namespace GP::HAL
{

FBaseWindow::FBaseWindow(bool headless) noexcept
    : m_isHeadless(headless)
    , m_isOpen(headless)
{}

GP_NODISCARD bool FBaseWindow::IsOpen() const noexcept { return m_isOpen; }

GP_NODISCARD bool FBaseWindow::ShouldClose() const noexcept { return m_shouldClose; }

GP_NODISCARD bool FBaseWindow::IsFocused() const noexcept { return m_isFocused; }

GP_NODISCARD bool FBaseWindow::IsActive() const noexcept { return m_isOpen && m_isFocused; }

GP_NODISCARD bool FBaseWindow::IsFullscreen() const noexcept
{
    return m_windowMode == EWindowMode::Fullscreen || m_windowMode == EWindowMode::FullscreenWindowed;
}

GP_NODISCARD bool FBaseWindow::IsResizable() const noexcept
{
    return Enums::HasAny(m_windowStyle, EWindowStyle::Resizable);
}

GP_NODISCARD bool FBaseWindow::IsVisible() const noexcept
{
    return Enums::HasAny(m_windowStyle, EWindowStyle::Visible);
}

GP_NODISCARD bool FBaseWindow::IsMinimized() const noexcept { return m_windowState == EWindowState::Minimized; }

GP_NODISCARD bool FBaseWindow::IsMaximized() const noexcept { return m_windowState == EWindowState::Maximized; }

GP_NODISCARD bool FBaseWindow::IsBorderless() const noexcept
{
    return m_windowMode == EWindowMode::Borderless || m_windowMode == EWindowMode::FullscreenWindowed;
}

GP_NODISCARD bool FBaseWindow::IsDecorated() const noexcept
{
    return Enums::HasAny(m_windowStyle, EWindowStyle::Decorated);
}

GP_NODISCARD bool FBaseWindow::IsTransparent() const noexcept
{
    return Enums::HasAny(m_windowStyle, EWindowStyle::TransparentFramebuffer) && m_opacity < 1.0f;
}

GP_NODISCARD bool FBaseWindow::IsAlwaysOnTop() const noexcept
{
    return Enums::HasAny(m_windowStyle, EWindowStyle::Floating);
}

GP_NODISCARD bool FBaseWindow::IsHovered() const noexcept { return m_isHovered; }

GP_NODISCARD bool FBaseWindow::IsInputGrabbed() const noexcept { return m_isMouseGrabbed || m_isKeyboardGrabbed; }

GP_NODISCARD bool FBaseWindow::IsMouseGrabbed() const noexcept { return m_isMouseGrabbed; }

GP_NODISCARD bool FBaseWindow::IsKeyboardGrabbed() const noexcept { return m_isKeyboardGrabbed; }

GP_NODISCARD bool FBaseWindow::IsHighDPI() const noexcept { return m_isHighDPI; }

GP_NODISCARD bool FBaseWindow::IsOccluded() const noexcept { return m_isOccluded; }

GP_NODISCARD bool FBaseWindow::IsVSyncEnabled() const noexcept { return m_isVSync; }

GP_NODISCARD bool FBaseWindow::HasMouseCapture() const noexcept { return m_isMouseGrabbed; }

GP_NODISCARD EWindowMode FBaseWindow::GetWindowMode() const noexcept { return m_windowMode; }

GP_NODISCARD EWindowState FBaseWindow::GetWindowState() const noexcept { return m_windowState; }

GP_NODISCARD EWindowStyle FBaseWindow::GetWindowStyle() const noexcept { return m_windowStyle; }

GP_NODISCARD ECursorMode FBaseWindow::GetCursorMode() const noexcept { return m_cursorMode; }

GP_NODISCARD ECursorShape FBaseWindow::GetCursorShape() const noexcept { return m_cursorShape; }

GP_NODISCARD Float32 FBaseWindow::GetOpacity() const noexcept { return m_opacity; }

GP_NODISCARD FIntExtent2D FBaseWindow::GetFullWindowSize() const noexcept { return m_fullWindowSize; }

GP_NODISCARD UInt32 FBaseWindow::GetFullWindowWidth() const noexcept { return m_fullWindowSize.width; }

GP_NODISCARD UInt32 FBaseWindow::GetFullWindowHeight() const noexcept { return m_fullWindowSize.height; }

GP_NODISCARD FIntExtent2D FBaseWindow::GetSize() const noexcept { return m_contentSize; }

GP_NODISCARD UInt32 FBaseWindow::GetWidth() const noexcept { return m_contentSize.width; }

GP_NODISCARD UInt32 FBaseWindow::GetHeight() const noexcept { return m_contentSize.height; }

GP_NODISCARD FIntExtent2D FBaseWindow::GetFramebufferSize() const noexcept { return m_framebufferSize; }

GP_NODISCARD UInt32 FBaseWindow::GetFramebufferWidth() const noexcept { return m_framebufferSize.width; }

GP_NODISCARD UInt32 FBaseWindow::GetFramebufferHeight() const noexcept { return m_framebufferSize.height; }

GP_NODISCARD FIntPoint2D FBaseWindow::GetPosition() const noexcept { return m_position; }

GP_NODISCARD UInt32 FBaseWindow::GetX() const noexcept { return static_cast<UInt32>(m_position.x); }

GP_NODISCARD UInt32 FBaseWindow::GetY() const noexcept { return static_cast<UInt32>(m_position.y); }

GP_NODISCARD FIntPoint2D FBaseWindow::GetCenter() const noexcept
{
    return { m_position.x + static_cast<Int32>(m_contentSize.width / 2u),
             m_position.y + static_cast<Int32>(m_contentSize.height / 2u) };
}

GP_NODISCARD FIntPoint2D FBaseWindow::GetTopLeft() const noexcept { return m_position; }

GP_NODISCARD FIntPoint2D FBaseWindow::GetTopRight() const noexcept
{
    return { m_position.x + static_cast<Int32>(m_contentSize.width), m_position.y };
}

GP_NODISCARD FIntPoint2D FBaseWindow::GetBottomLeft() const noexcept
{
    return { m_position.x, m_position.y + static_cast<Int32>(m_contentSize.height) };
}

GP_NODISCARD FIntPoint2D FBaseWindow::GetBottomRight() const noexcept
{
    return { m_position.x + static_cast<Int32>(m_contentSize.width),
             m_position.y + static_cast<Int32>(m_contentSize.height) };
}

GP_NODISCARD Float32 FBaseWindow::GetAspectRatio() const noexcept { return m_aspectRatio; }

GP_NODISCARD Float32 FBaseWindow::GetContentScale() const noexcept { return m_contentScale; }

GP_NODISCARD FStringView FBaseWindow::GetTitle() const noexcept
{
    return FStringView(m_title.Data(), m_title.Length());
}

GP_NODISCARD FIntExtent2D FBaseWindow::GetMinSize() const noexcept { return m_minSize; }

GP_NODISCARD FIntExtent2D FBaseWindow::GetMaxSize() const noexcept { return m_maxSize; }

GP_NODISCARD const IMonitor* FBaseWindow::GetCurrentMonitor() const noexcept { return nullptr; }

GP_NODISCARD void* FBaseWindow::GetNativeHandle() const noexcept { return nullptr; }

void FBaseWindow::Close()
{
    m_isOpen = false;
    m_shouldClose = true;
    m_isFocused = false;
    m_windowStyle = Enums::ClearFlags(m_windowStyle, EWindowStyle::Visible);
    m_windowState = EWindowState::Hidden;
}

void FBaseWindow::Show()
{
    m_isOpen = true;
    m_shouldClose = false;
    m_windowStyle = Enums::SetFlags(m_windowStyle, EWindowStyle::Visible);
    if (m_windowState == EWindowState::Hidden) { m_windowState = EWindowState::Normal; }
}

void FBaseWindow::Hide()
{
    m_windowStyle = Enums::ClearFlags(m_windowStyle, EWindowStyle::Visible);
    m_windowState = EWindowState::Hidden;
    m_isFocused = false;
}

void FBaseWindow::Focus() { m_isFocused = true; }

void FBaseWindow::Maximize()
{
    m_windowState = EWindowState::Maximized;
    m_windowStyle = Enums::SetFlags(Enums::ClearFlags(m_windowStyle, EWindowStyle::Minimized), EWindowStyle::Maximized);
}

void FBaseWindow::Minimize()
{
    m_windowState = EWindowState::Minimized;
    m_windowStyle = Enums::SetFlags(Enums::ClearFlags(m_windowStyle, EWindowStyle::Maximized), EWindowStyle::Minimized);
    m_isFocused = false;
}

void FBaseWindow::Restore()
{
    m_windowState = EWindowState::Normal;
    m_windowStyle = Enums::ClearFlags(m_windowStyle, EWindowStyle::Minimized | EWindowStyle::Maximized);
}

void FBaseWindow::RequestAttention()
{
    // No platform handle available; platform-specific subclasses should override this method.
}

void FBaseWindow::SetWindowMode(EWindowMode mode) { m_windowMode = mode; }

void FBaseWindow::SetWindowStyle(EWindowStyle style) { m_windowStyle = style; }

void FBaseWindow::SetCursorMode(ECursorMode mode) { m_cursorMode = mode; }

void FBaseWindow::SetCursorShape(ECursorShape shape) { m_cursorShape = shape; }

void FBaseWindow::SetOpacity(Float32 opacity) { m_opacity = opacity < 0.0f ? 0.0f : (opacity > 1.0f ? 1.0f : opacity); }

void FBaseWindow::SetSize(UInt32 width, UInt32 height)
{
    // Clamp width to [minSize, maxSize] — 0 means "no limit" for max.
    const UInt32 clampedWidth = (m_minSize.width > 0u && width < m_minSize.width) ? m_minSize.width : width;
    const UInt32 clampedHeight = (m_minSize.height > 0u && height < m_minSize.height) ? m_minSize.height : height;
    const UInt32 finalWidth = (m_maxSize.width > 0u && clampedWidth > m_maxSize.width) ? m_maxSize.width : clampedWidth;
    const UInt32 finalHeight =
        (m_maxSize.height > 0u && clampedHeight > m_maxSize.height) ? m_maxSize.height : clampedHeight;

    m_contentSize = { finalWidth, finalHeight };
    m_framebufferSize = m_isHighDPI ? FIntExtent2D{ finalWidth * 2u, finalHeight * 2u } : m_contentSize;

    if (finalHeight > 0u) { m_aspectRatio = static_cast<Float32>(finalWidth) / static_cast<Float32>(finalHeight); }
}

void FBaseWindow::SetSize(const FIntExtent2D& size) { SetSize(size.width, size.height); }

void FBaseWindow::SetWidth(UInt32 width) { SetSize(width, m_contentSize.height); }

void FBaseWindow::SetHeight(UInt32 height) { SetSize(m_contentSize.width, height); }

void FBaseWindow::SetFullWindowSize(UInt32 width, UInt32 height) { m_fullWindowSize = { width, height }; }

void FBaseWindow::SetFullWindowSize(const FIntExtent2D& size) { m_fullWindowSize = size; }

void FBaseWindow::SetFullWindowWidth(UInt32 width) { m_fullWindowSize.width = width; }

void FBaseWindow::SetFullWindowHeight(UInt32 height) { m_fullWindowSize.height = height; }

void FBaseWindow::SetPosition(UInt32 x, UInt32 y)
{
    m_position.x = static_cast<Int32>(x);
    m_position.y = static_cast<Int32>(y);
}

void FBaseWindow::SetPosition(const FIntPoint2D& position) { m_position = position; }

void FBaseWindow::SetX(UInt32 x) { m_position.x = static_cast<Int32>(x); }

void FBaseWindow::SetY(UInt32 y) { m_position.y = static_cast<Int32>(y); }

void FBaseWindow::SetAspectRatio(Float32 aspectRatio) { m_aspectRatio = aspectRatio; }

void FBaseWindow::SetAspectRatio(UInt32 numerator, UInt32 denominator)
{
    GP_ASSERT(denominator > 0u, "Aspect ratio denominator must be greater than zero.");
    if (denominator > 0u) { m_aspectRatio = static_cast<Float32>(numerator) / static_cast<Float32>(denominator); }
}

void FBaseWindow::SetSizeLimits(UInt32 minWidth, UInt32 minHeight, UInt32 maxWidth, UInt32 maxHeight)
{
    m_minSize = { minWidth, minHeight };
    m_maxSize = { maxWidth, maxHeight };
}

void FBaseWindow::SetSizeLimits(const FIntExtent2D& minSize, const FIntExtent2D& maxSize)
{
    m_minSize = minSize;
    m_maxSize = maxSize;
}

void FBaseWindow::SetMinSize(const FIntExtent2D& minSize) { m_minSize = minSize; }

void FBaseWindow::SetMinSize(UInt32 minWidth, UInt32 minHeight) { m_minSize = { minWidth, minHeight }; }

void FBaseWindow::SetMinWidth(UInt32 minWidth) { m_minSize.width = minWidth; }

void FBaseWindow::SetMinHeight(UInt32 minHeight) { m_minSize.height = minHeight; }

void FBaseWindow::SetMaxSize(const FIntExtent2D& maxSize) { m_maxSize = maxSize; }

void FBaseWindow::SetMaxSize(UInt32 maxWidth, UInt32 maxHeight) { m_maxSize = { maxWidth, maxHeight }; }

void FBaseWindow::SetMaxWidth(UInt32 maxWidth) { m_maxSize.width = maxWidth; }

void FBaseWindow::SetMaxHeight(UInt32 maxHeight) { m_maxSize.height = maxHeight; }

void FBaseWindow::Center(const IMonitor* monitor)
{
    GP_UNUSED(monitor);
    // No real display context in the base implementation; position is reset to the screen origin.
    // Platform-specific subclasses should override this to query the monitor's work-area bounds.
    m_position = DEFAULT_POSITION;
}

void FBaseWindow::SetTitle(FStringView title) { m_title = title; }

void FBaseWindow::SetFullscreen(bool enabled)
{
    m_windowMode = enabled ? EWindowMode::Fullscreen : EWindowMode::Windowed;
}

void FBaseWindow::SetBorderless(bool enabled)
{
    if (enabled)
    {
        m_windowMode = EWindowMode::Borderless;
        m_windowStyle = Enums::ClearFlags(m_windowStyle, EWindowStyle::Decorated);
    }
    else
    {
        if (m_windowMode == EWindowMode::Borderless) { m_windowMode = EWindowMode::Windowed; }
        m_windowStyle = Enums::SetFlags(m_windowStyle, EWindowStyle::Decorated);
    }
}

void FBaseWindow::SetResizable(bool enabled)
{
    m_windowStyle = enabled ? Enums::SetFlags(m_windowStyle, EWindowStyle::Resizable)
                            : Enums::ClearFlags(m_windowStyle, EWindowStyle::Resizable);
}

void FBaseWindow::SetDecorated(bool enabled)
{
    m_windowStyle = enabled ? Enums::SetFlags(m_windowStyle, EWindowStyle::Decorated)
                            : Enums::ClearFlags(m_windowStyle, EWindowStyle::Decorated);
}

void FBaseWindow::SetTransparent(bool enabled)
{
    m_windowStyle = enabled ? Enums::SetFlags(m_windowStyle, EWindowStyle::TransparentFramebuffer)
                            : Enums::ClearFlags(m_windowStyle, EWindowStyle::TransparentFramebuffer);
}

void FBaseWindow::SetAlwaysOnTop(bool enabled)
{
    m_windowStyle = enabled ? Enums::SetFlags(m_windowStyle, EWindowStyle::Floating)
                            : Enums::ClearFlags(m_windowStyle, EWindowStyle::Floating);
}

void FBaseWindow::SetCursorVisibility(bool visible)
{
    if (visible)
    {
        if (m_cursorMode == ECursorMode::Hidden) { m_cursorMode = ECursorMode::Normal; }
    }
    else
    {
        if (m_cursorMode == ECursorMode::Normal) { m_cursorMode = ECursorMode::Hidden; }
    }
}

void FBaseWindow::SetCursorGrab(bool grabbed)
{
    m_isMouseGrabbed = grabbed;
    m_cursorMode = grabbed ? ECursorMode::Captured : ECursorMode::Normal;
}

void FBaseWindow::SetKeyboardGrab(bool grabbed) { m_isKeyboardGrabbed = grabbed; }

void FBaseWindow::SetHighDPI(bool enabled)
{
    if (m_isHighDPI == enabled) { return; }

    m_isHighDPI = enabled;
    m_contentScale = enabled ? 2.0f : DEFAULT_CONTENT_SCALE;
    m_framebufferSize = enabled ? FIntExtent2D{ m_contentSize.width * 2u, m_contentSize.height * 2u } : m_contentSize;
}

void FBaseWindow::SetVSync(bool enabled) { m_isVSync = enabled; }

void FBaseWindow::SetIcon(const UInt8* pixels, UInt32 width, UInt32 height)
{
    GP_UNUSED(pixels);
    GP_UNUSED(width);
    GP_UNUSED(height);
    // No platform handle available; platform-specific subclasses should override this method.
}

}   // namespace GP::HAL
