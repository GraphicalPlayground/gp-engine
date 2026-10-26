// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "input/IMouse.hpp"

namespace gp::hal::input::generic
{

/// @brief Platform-agnostic implementation of `IMouse` tracking buttons, sub-pixel coords, relative deltas, and scroll.
class Mouse : public IMouse
{
protected:
    ButtonStateFlags m_buttons[kMaxMouseButtons]{};
    gp::UInt8 m_clicks[kMaxMouseButtons]{};
    Stick2D m_position{};
    Stick2D m_delta{};
    Stick2D m_wheelDelta{};
    MouseButtonMask m_heldMask{ MouseButtonMask::None };
    gp::UInt32 m_windowId{ 0u };
    InputDeviceId m_deviceId{ 1u };
    bool m_isConnected{ true };

public:
    /// @brief Default constructor.
    [[nodiscard]] constexpr Mouse() noexcept = default;

    /// @brief Virtual destructor.
    ~Mouse() noexcept override = default;

    /// @copydoc IInputDevice::getDeviceId()
    [[nodiscard]] constexpr InputDeviceId getDeviceId() const noexcept override
    {
        return m_deviceId;
    }

    /// @copydoc IInputDevice::getDeviceType()
    [[nodiscard]] constexpr InputDeviceType getDeviceType() const noexcept override
    {
        return InputDeviceType::Mouse;
    }

    /// @copydoc IInputDevice::getDeviceName()
    [[nodiscard]] constexpr StringView getDeviceName() const noexcept override
    {
        return StringView("System Mouse");
    }

    /// @copydoc IInputDevice::isConnected()
    [[nodiscard]] constexpr bool isConnected() const noexcept override
    {
        return m_isConnected;
    }

    /// @copydoc IInputDevice::getBatteryPercent()
    [[nodiscard]] constexpr gp::UInt8 getBatteryPercent() const noexcept override
    {
        return 100u;
    }

    /// @copydoc IInputDevice::beginFrame()
    constexpr void beginFrame() noexcept override
    {
        m_delta = Stick2D(0.0f, 0.0f);
        m_wheelDelta = Stick2D(0.0f, 0.0f);
        for (gp::UInt32 i = 0u; i < kMaxMouseButtons; ++i)
        {
            m_buttons[i] &= ButtonStateFlags::Held;
        }
    }

    /// @copydoc IInputDevice::releaseAllInputs()
    constexpr void releaseAllInputs() noexcept override
    {
        m_delta = Stick2D(0.0f, 0.0f);
        m_wheelDelta = Stick2D(0.0f, 0.0f);
        m_heldMask = MouseButtonMask::None;
        for (gp::UInt32 i = 0u; i < kMaxMouseButtons; ++i)
        {
            if ((m_buttons[i] & ButtonStateFlags::Held) != ButtonStateFlags::None)
            {
                m_buttons[i] = (m_buttons[i] & ~ButtonStateFlags::Held) | ButtonStateFlags::ReleasedThisFrame;
            }
            m_clicks[i] = 0u;
        }
    }

    /// @copydoc IInputDevice::reset()
    constexpr void reset() noexcept override
    {
        for (gp::UInt32 i = 0u; i < kMaxMouseButtons; ++i)
        {
            m_buttons[i] = ButtonStateFlags::None;
            m_clicks[i] = 0u;
        }
        m_position = Stick2D(0.0f, 0.0f);
        m_delta = Stick2D(0.0f, 0.0f);
        m_wheelDelta = Stick2D(0.0f, 0.0f);
        m_heldMask = MouseButtonMask::None;
        m_windowId = 0u;
    }

    /// @copydoc IMouse::isButtonDown()
    [[nodiscard]] constexpr bool isButtonDown(MouseButton button) const noexcept override
    {
        return (getButtonState(button) & ButtonStateFlags::Held) != ButtonStateFlags::None;
    }

    /// @copydoc IMouse::wasButtonJustPressed()
    [[nodiscard]] constexpr bool wasButtonJustPressed(MouseButton button) const noexcept override
    {
        return (getButtonState(button) & ButtonStateFlags::PressedThisFrame) != ButtonStateFlags::None;
    }

    /// @copydoc IMouse::wasButtonJustReleased()
    [[nodiscard]] constexpr bool wasButtonJustReleased(MouseButton button) const noexcept override
    {
        return (getButtonState(button) & ButtonStateFlags::ReleasedThisFrame) != ButtonStateFlags::None;
    }

    /// @copydoc IMouse::wasButtonDoubleClicked()
    [[nodiscard]] constexpr bool wasButtonDoubleClicked(MouseButton button) const noexcept override
    {
        const auto idx = static_cast<gp::UInt8>(button);
        return (idx < kMaxMouseButtons) && wasButtonJustPressed(button) && (m_clicks[idx] >= 2u);
    }

    /// @copydoc IMouse::getButtonState()
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr ButtonStateFlags
        getButtonState(MouseButton button) const noexcept override
    {
        const auto idx = static_cast<gp::UInt8>(button);
        return (idx < kMaxMouseButtons) ? m_buttons[idx] : ButtonStateFlags::None;
    }

    /// @copydoc IMouse::getHeldButtonMask()
    [[nodiscard]] constexpr MouseButtonMask getHeldButtonMask() const noexcept override
    {
        return m_heldMask;
    }

    /// @copydoc IMouse::getPosition()
    [[nodiscard]] constexpr Stick2D getPosition() const noexcept override
    {
        return m_position;
    }

    /// @copydoc IMouse::getDelta()
    [[nodiscard]] constexpr Stick2D getDelta() const noexcept override
    {
        return m_delta;
    }

    /// @copydoc IMouse::getWheelDelta()
    [[nodiscard]] constexpr Stick2D getWheelDelta() const noexcept override
    {
        return m_wheelDelta;
    }

    /// @copydoc IMouse::getFocusedWindowId()
    [[nodiscard]] constexpr gp::UInt32 getFocusedWindowId() const noexcept override
    {
        return m_windowId;
    }

    /// @copydoc IMouse::onMouseEvent()
    constexpr void onMouseEvent(const event::MouseEvent& ev) noexcept override
    {
        m_windowId = ev.windowId;
        if (ev.deviceId != 0u)
        {
            m_deviceId = ev.deviceId;
        }

        switch (ev.action)
        {
        case event::MouseAction::Moved:
            m_position = Stick2D(ev.x, ev.y);
            m_delta.x += ev.deltaX;
            m_delta.y += ev.deltaY;
            m_heldMask = ev.buttonMask;
            break;

        case event::MouseAction::ButtonChanged:
        {
            m_position = Stick2D(ev.x, ev.y);
            const auto idx = static_cast<gp::UInt8>(ev.button);
            if (idx < kMaxMouseButtons) [[likely]]
            {
                m_clicks[idx] = ev.clicks;
                if (ev.state == KeyState::Pressed)
                {
                    if ((m_buttons[idx] & ButtonStateFlags::Held) == ButtonStateFlags::None)
                    {
                        m_buttons[idx] |= ButtonStateFlags::Held | ButtonStateFlags::PressedThisFrame;
                    }
                    if (idx >= 1u && idx <= 5u)
                    {
                        m_heldMask |= static_cast<MouseButtonMask>(1u << (idx - 1u));
                    }
                }
                else
                {
                    if ((m_buttons[idx] & ButtonStateFlags::Held) != ButtonStateFlags::None)
                    {
                        m_buttons[idx] =
                            (m_buttons[idx] & ~ButtonStateFlags::Held) | ButtonStateFlags::ReleasedThisFrame;
                    }
                    if (idx >= 1u && idx <= 5u)
                    {
                        m_heldMask &= ~static_cast<MouseButtonMask>(1u << (idx - 1u));
                    }
                }
            }
            break;
        }

        case event::MouseAction::WheelScrolled:
        {
            const gp::Float32 sign = (ev.isWheelFlipped != 0u) ? -1.0f : 1.0f;
            m_wheelDelta.x += ev.deltaX * sign;
            m_wheelDelta.y += ev.deltaY * sign;
            break;
        }

        case event::MouseAction::None:
        default:
            break;
        }
    }
};

}   // namespace gp::hal::input::generic
