// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "input/IGamepad.hpp"

namespace gp::hal::input::generic
{

/// @brief Platform-agnostic base implementation of `IGamepad` with 2D radial deadzones and IMU/touchpad shadow state.
class Gamepad : public IGamepad
{
public:
    static constexpr gp::USize kMaxGamepadNameBytes = 128u;

protected:
    ButtonStateFlags m_buttons[kMaxGamepadButtons]{};
    gp::Float32 m_rawAxes[kMaxGamepadAxes]{};
    gp::Float32 m_filteredAxes[kMaxGamepadAxes]{};
    Stick2D m_leftStick{};
    Stick2D m_rightStick{};
    Stick2D m_touchpadPos{};

    AnalogDeadzoneConfig m_leftStickDeadzone{ 0.12f, 0.95f, DeadzoneMode::ScaledRadial };
    AnalogDeadzoneConfig m_rightStickDeadzone{ 0.12f, 0.95f, DeadzoneMode::ScaledRadial };
    AnalogDeadzoneConfig m_triggerDeadzone{ 0.05f, 0.98f, DeadzoneMode::Axial };

    SensorReading m_accelerometer{};
    SensorReading m_gyroscope{};
    TouchContactPoint m_touchContact{};
    PenStateSnapshot m_emptyPenState{};

    InputDeviceId m_deviceId{ kInvalidInputDeviceId };
    GamepadSlot m_slot{ GamepadSlot::Player1 };
    HatPosition m_hat{ HatPosition::Centered };
    TouchFingerState m_touchpadState{ TouchFingerState::Up };
    gp::UInt8 m_batteryPercent{ 100u };
    bool m_isConnected{ false };
    bool m_accelEnabled{ false };
    bool m_gyroEnabled{ false };

    char m_nameBuffer[kMaxGamepadNameBytes]{ "Generic Gamepad" };
    StringView m_nameView{ m_nameBuffer, 15u };

public:
    /// @brief Default constructor initializes an unconnected gamepad slot.
    [[nodiscard]] constexpr Gamepad() noexcept = default;

    /// @brief Virtual destructor.
    ~Gamepad() noexcept override = default;

    /// @brief Binds a hardware device ID and logical player slot to this controller instance.
    /// @param[in] deviceId Native hardware instance identifier.
    /// @param[in] slot Assigned logical player slot (`Player1`..`Player8`).
    /// @param[in] name UTF-8 product name of the controller.
    constexpr void attach(InputDeviceId deviceId, GamepadSlot slot, StringView name = "Generic Gamepad") noexcept
    {
        reset();
        m_deviceId = deviceId;
        m_slot = slot;
        m_isConnected = (deviceId != kInvalidInputDeviceId);
        cacheNameString(name);
    }

    /// @brief Detaches the gamepad upon hardware disconnection, synthesizing releases for any held buttons.
    constexpr void detach() noexcept
    {
        releaseAllInputs();
        m_isConnected = false;
    }

    /// @copydoc IInputDevice::getDeviceId()
    [[nodiscard]] constexpr InputDeviceId getDeviceId() const noexcept override
    {
        return m_deviceId;
    }

    /// @copydoc IInputDevice::getDeviceType()
    [[nodiscard]] constexpr InputDeviceType getDeviceType() const noexcept override
    {
        return InputDeviceType::Gamepad;
    }

    /// @copydoc IInputDevice::getDeviceName()
    [[nodiscard]] constexpr StringView getDeviceName() const noexcept override
    {
        return m_nameView;
    }

    /// @copydoc IInputDevice::isConnected()
    [[nodiscard]] constexpr bool isConnected() const noexcept override
    {
        return m_isConnected;
    }

    /// @copydoc IInputDevice::getBatteryPercent()
    [[nodiscard]] constexpr gp::UInt8 getBatteryPercent() const noexcept override
    {
        return m_batteryPercent;
    }

    /// @copydoc IInputDevice::beginFrame()
    constexpr void beginFrame() noexcept override
    {
        for (gp::UInt32 i = 0u; i < kMaxGamepadButtons; ++i)
        {
            m_buttons[i] &= ButtonStateFlags::Held;
        }
    }

    /// @copydoc IInputDevice::releaseAllInputs()
    constexpr void releaseAllInputs() noexcept override
    {
        for (gp::UInt32 i = 0u; i < kMaxGamepadButtons; ++i)
        {
            if ((m_buttons[i] & ButtonStateFlags::Held) != ButtonStateFlags::None)
            {
                m_buttons[i] = (m_buttons[i] & ~ButtonStateFlags::Held) | ButtonStateFlags::ReleasedThisFrame;
            }
        }
        for (gp::UInt32 i = 0u; i < kMaxGamepadAxes; ++i)
        {
            m_rawAxes[i] = 0.0f;
            m_filteredAxes[i] = 0.0f;
        }
        m_leftStick = Stick2D(0.0f, 0.0f);
        m_rightStick = Stick2D(0.0f, 0.0f);
        m_hat = HatPosition::Centered;
        m_touchpadState = TouchFingerState::Up;
    }

    /// @copydoc IInputDevice::reset()
    constexpr void reset() noexcept override
    {
        releaseAllInputs();
        for (gp::UInt32 i = 0u; i < kMaxGamepadButtons; ++i)
        {
            m_buttons[i] = ButtonStateFlags::None;
        }
        m_accelerometer = SensorReading{};
        m_gyroscope = SensorReading{};
        m_deviceId = kInvalidInputDeviceId;
        m_isConnected = false;
        m_batteryPercent = 100u;
    }

    /// @copydoc IGamepad::getSlot()
    [[nodiscard]] constexpr GamepadSlot getSlot() const noexcept override
    {
        return m_slot;
    }

    /// @copydoc IGamepad::isButtonDown()
    [[nodiscard]] constexpr bool isButtonDown(GamepadButton button) const noexcept override
    {
        return (getButtonState(button) & ButtonStateFlags::Held) != ButtonStateFlags::None;
    }

    /// @copydoc IGamepad::wasButtonJustPressed()
    [[nodiscard]] constexpr bool wasButtonJustPressed(GamepadButton button) const noexcept override
    {
        return (getButtonState(button) & ButtonStateFlags::PressedThisFrame) != ButtonStateFlags::None;
    }

    /// @copydoc IGamepad::wasButtonJustReleased()
    [[nodiscard]] constexpr bool wasButtonJustReleased(GamepadButton button) const noexcept override
    {
        return (getButtonState(button) & ButtonStateFlags::ReleasedThisFrame) != ButtonStateFlags::None;
    }

    /// @copydoc IGamepad::getButtonState()
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr ButtonStateFlags
        getButtonState(GamepadButton button) const noexcept override
    {
        const auto idx = static_cast<gp::UInt8>(button);
        return (m_isConnected && idx < kMaxGamepadButtons) ? m_buttons[idx] : ButtonStateFlags::None;
    }

    /// @copydoc IGamepad::getHatPosition()
    [[nodiscard]] constexpr HatPosition getHatPosition() const noexcept override
    {
        return m_isConnected ? m_hat : HatPosition::Centered;
    }

    /// @copydoc IGamepad::getAxis()
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::Float32 getAxis(GamepadAxis axis) const noexcept override
    {
        const auto idx = static_cast<gp::UInt8>(axis);
        return (m_isConnected && idx < kMaxGamepadAxes) ? m_filteredAxes[idx] : 0.0f;
    }

    /// @copydoc IGamepad::getRawAxis()
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::Float32 getRawAxis(GamepadAxis axis) const noexcept override
    {
        const auto idx = static_cast<gp::UInt8>(axis);
        return (m_isConnected && idx < kMaxGamepadAxes) ? m_rawAxes[idx] : 0.0f;
    }

    /// @copydoc IGamepad::getLeftStick()
    [[nodiscard]] constexpr Stick2D getLeftStick() const noexcept override
    {
        return m_isConnected ? m_leftStick : Stick2D(0.0f, 0.0f);
    }

    /// @copydoc IGamepad::getRightStick()
    [[nodiscard]] constexpr Stick2D getRightStick() const noexcept override
    {
        return m_isConnected ? m_rightStick : Stick2D(0.0f, 0.0f);
    }

    /// @copydoc IGamepad::setDeadzones()
    constexpr void setDeadzones(
        const AnalogDeadzoneConfig& leftStick,
        const AnalogDeadzoneConfig& rightStick,
        const AnalogDeadzoneConfig& triggers
    ) noexcept override
    {
        m_leftStickDeadzone = leftStick;
        m_rightStickDeadzone = rightStick;
        m_triggerDeadzone = triggers;
        recomputeDeadzones();
    }

    /// @copydoc IGamepad::getTouchpadState()
    [[nodiscard]] constexpr TouchFingerState getTouchpadState() const noexcept override
    {
        return m_touchpadState;
    }

    /// @copydoc IGamepad::getTouchpadPosition()
    [[nodiscard]] constexpr Stick2D getTouchpadPosition() const noexcept override
    {
        return m_touchpadPos;
    }

    /// @copydoc IGamepad::onGamepadEvent()
    constexpr void onGamepadEvent(const event::GamepadEvent& ev) noexcept override
    {
        switch (ev.action)
        {
        case event::GamepadAction::ButtonChanged:
        {
            const auto idx = static_cast<gp::UInt8>(ev.control.button);
            if (idx < kMaxGamepadButtons) [[likely]]
            {
                if (ev.state == KeyState::Pressed)
                {
                    if ((m_buttons[idx] & ButtonStateFlags::Held) == ButtonStateFlags::None)
                    {
                        m_buttons[idx] |= ButtonStateFlags::Held | ButtonStateFlags::PressedThisFrame;
                    }
                }
                else
                {
                    if ((m_buttons[idx] & ButtonStateFlags::Held) != ButtonStateFlags::None)
                    {
                        m_buttons[idx] =
                            (m_buttons[idx] & ~ButtonStateFlags::Held) | ButtonStateFlags::ReleasedThisFrame;
                    }
                }
            }
            break;
        }

        case event::GamepadAction::AxisMoved:
        {
            const auto idx = static_cast<gp::UInt8>(ev.control.axis);
            if (idx < kMaxGamepadAxes) [[likely]]
            {
                m_rawAxes[idx] = ev.value;
                recomputeDeadzones();
            }
            break;
        }

        case event::GamepadAction::HatChanged:
            m_hat = ev.control.hatPosition;
            break;

        case event::GamepadAction::TouchpadChanged:
            m_touchpadState = ev.control.touchState;
            m_touchpadPos = Stick2D(ev.value, ev.secondaryValue);
            m_touchContact.x = ev.value;
            m_touchContact.y = ev.secondaryValue;
            m_touchContact.isActive = (ev.control.touchState != TouchFingerState::Up);
            break;

        case event::GamepadAction::BatteryUpdated:
            m_batteryPercent = ev.batteryPercent;
            break;

        case event::GamepadAction::None:
        default:
            break;
        }
    }

    /// @copydoc IHapticDevice::supportsRumble()
    [[nodiscard]] constexpr bool supportsRumble() const noexcept override
    {
        return false;
    }

    /// @copydoc IHapticDevice::supportsTriggerRumble()
    [[nodiscard]] constexpr bool supportsTriggerRumble() const noexcept override
    {
        return false;
    }

    /// @copydoc IHapticDevice::supportsLEDColor()
    [[nodiscard]] constexpr bool supportsLEDColor() const noexcept override
    {
        return false;
    }

    /// @copydoc IHapticDevice::setRumble()
    constexpr bool setRumble([[maybe_unused]] const HapticRumbleDesc& desc) noexcept override
    {
        return false;
    }

    /// @copydoc IHapticDevice::setTriggerRumble()
    constexpr bool setTriggerRumble([[maybe_unused]] const HapticTriggerRumbleDesc& desc) noexcept override
    {
        return false;
    }

    /// @copydoc IHapticDevice::stopHaptics()
    constexpr void stopHaptics() noexcept override
    {}

    /// @copydoc IHapticDevice::setLEDColor()
    constexpr bool setLEDColor([[maybe_unused]] const DeviceLEDColor& color) noexcept override
    {
        return false;
    }

    /// @copydoc IHapticDevice::setPlayerLED()
    constexpr bool setPlayerLED(GamepadSlot slot) noexcept override
    {
        m_slot = slot;
        return false;
    }

    /// @copydoc ISpatialDevice::hasSensor()
    [[nodiscard]] constexpr bool hasSensor([[maybe_unused]] GamepadSensorType type) const noexcept override
    {
        return false;
    }

    /// @copydoc ISpatialDevice::isSensorEnabled()
    [[nodiscard]] constexpr bool isSensorEnabled(GamepadSensorType type) const noexcept override
    {
        if (type == GamepadSensorType::Accelerometer)
        {
            return m_accelEnabled;
        }
        if (type == GamepadSensorType::Gyroscope)
        {
            return m_gyroEnabled;
        }
        return false;
    }

    /// @copydoc ISpatialDevice::setSensorEnabled()
    constexpr bool
        setSensorEnabled([[maybe_unused]] GamepadSensorType type, [[maybe_unused]] bool enabled) noexcept override
    {
        return false;
    }

    /// @copydoc ISpatialDevice::getAccelerometerReading()
    [[nodiscard]] constexpr const SensorReading& getAccelerometerReading() const noexcept override
    {
        return m_accelerometer;
    }

    /// @copydoc ISpatialDevice::getGyroscopeReading()
    [[nodiscard]] constexpr const SensorReading& getGyroscopeReading() const noexcept override
    {
        return m_gyroscope;
    }

    /// @copydoc ISpatialDevice::getActiveTouchCount()
    [[nodiscard]] constexpr gp::UInt32 getActiveTouchCount() const noexcept override
    {
        return m_touchContact.isActive ? 1u : 0u;
    }

    /// @copydoc ISpatialDevice::getTouchContact()
    [[nodiscard]] constexpr const TouchContactPoint* getTouchContact(gp::UInt32 index) const noexcept override
    {
        return (index == 0u && m_touchContact.isActive) ? &m_touchContact : nullptr;
    }

    /// @copydoc ISpatialDevice::getPenState()
    [[nodiscard]] constexpr const PenStateSnapshot& getPenState() const noexcept override
    {
        return m_emptyPenState;
    }

    /// @copydoc ISpatialDevice::onSensorEvent()
    constexpr void onSensorEvent(const event::SensorEvent& ev) noexcept override
    {
        SensorReading reading{};
        reading.timestampNs = ev.timestampNs;
        reading.value = Vector3fInput(ev.values[0], ev.values[1], ev.values[2]);
        reading.type = ev.type;
        reading.side = ev.side;
        reading.isValid = true;

        if (ev.type == GamepadSensorType::Accelerometer)
        {
            m_accelerometer = reading;
        }
        else if (ev.type == GamepadSensorType::Gyroscope)
        {
            m_gyroscope = reading;
        }
    }

    /// @copydoc ISpatialDevice::onTouchEvent()
    constexpr void onTouchEvent([[maybe_unused]] const event::TouchEvent& ev) noexcept override
    {}

    /// @copydoc ISpatialDevice::onPenEvent()
    constexpr void onPenEvent([[maybe_unused]] const event::PenEvent& ev) noexcept override
    {}

protected:
    /// @brief Copies a `StringView` into the internal null-terminated buffer and updates `m_nameView`.
    /// @param[in] name Source UTF-8 gamepad name.
    constexpr void cacheNameString(StringView name) noexcept
    {
        const gp::USize maxCopy =
            (name.size() < (kMaxGamepadNameBytes - 1u)) ? name.size() : (kMaxGamepadNameBytes - 1u);
        for (gp::USize i = 0u; i < maxCopy; ++i)
        {
            m_nameBuffer[i] = name[i];
        }
        m_nameBuffer[maxCopy] = '\0';
        m_nameView = StringView(m_nameBuffer, maxCopy);
    }

    /// @brief Fast constexpr IEEE-754 Newton-Raphson square root for 2D radial deadzones.
    /// @param[in] val Non-negative input value.
    /// @return Square root of `val`.
    [[nodiscard]] GP_FORCEINLINE_HINT static constexpr gp::Float32 fastSqrt(gp::Float32 val) noexcept
    {
        if (val <= 0.0f) [[unlikely]]
        {
            return 0.0f;
        }
        gp::UInt32 bits = __builtin_bit_cast(gp::UInt32, val);
        bits = (bits >> 1u) + 0x1FC00000u;
        gp::Float32 guess = __builtin_bit_cast(gp::Float32, bits);
        guess = 0.5f * (guess + (val / guess));
        guess = 0.5f * (guess + (val / guess));
        guess = 0.5f * (guess + (val / guess));
        return guess;
    }

    /// @brief Applies a 1D axial deadzone and remaps `[inner, outer]` linearly onto `[0.0f, 1.0f]`.
    /// @param[in] raw Raw axis value.
    /// @param[in] cfg Deadzone configuration.
    /// @return Filtered axis reading.
    [[nodiscard]] GP_FORCEINLINE_HINT static constexpr gp::Float32
        filterAxial(gp::Float32 raw, const AnalogDeadzoneConfig& cfg) noexcept
    {
        if (cfg.mode == DeadzoneMode::None)
        {
            return raw;
        }
        const gp::Float32 sign = (raw < 0.0f) ? -1.0f : 1.0f;
        const gp::Float32 absVal = raw * sign;
        if (absVal <= cfg.innerDeadzone)
        {
            return 0.0f;
        }
        if (absVal >= cfg.outerDeadzone || cfg.outerDeadzone <= cfg.innerDeadzone)
        {
            return sign;
        }
        return sign * ((absVal - cfg.innerDeadzone) / (cfg.outerDeadzone - cfg.innerDeadzone));
    }

    /// @brief Applies a 2D Scaled Radial Deadzone to an `(x, y)` thumbstick vector (ADR-0035).
    /// @param[in] rawX Raw horizontal axis value.
    /// @param[in] rawY Raw vertical axis value.
    /// @param[in] cfg Deadzone configuration.
    /// @return Radially filtered 2D vector.
    [[nodiscard]] static constexpr Stick2D
        filterRadial(gp::Float32 rawX, gp::Float32 rawY, const AnalogDeadzoneConfig& cfg) noexcept
    {
        if (cfg.mode == DeadzoneMode::None)
        {
            return Stick2D(rawX, rawY);
        }
        if (cfg.mode == DeadzoneMode::Axial)
        {
            return Stick2D(filterAxial(rawX, cfg), filterAxial(rawY, cfg));
        }

        const gp::Float32 magSq = (rawX * rawX) + (rawY * rawY);
        if (magSq <= (cfg.innerDeadzone * cfg.innerDeadzone))
        {
            return Stick2D(0.0f, 0.0f);
        }

        const gp::Float32 mag = fastSqrt(magSq);
        if (mag <= 1e-6f) [[unlikely]]
        {
            return Stick2D(0.0f, 0.0f);
        }

        const gp::Float32 range = cfg.outerDeadzone - cfg.innerDeadzone;
        gp::Float32 norm = (range > 1e-5f) ? ((mag - cfg.innerDeadzone) / range) : 1.0f;
        if (norm > 1.0f)
        {
            norm = 1.0f;
        }
        const gp::Float32 scale = norm / mag;
        return Stick2D(rawX * scale, rawY * scale);
    }

    /// @brief Recomputes filtered stick and trigger values from `m_rawAxes`.
    constexpr void recomputeDeadzones() noexcept
    {
        m_leftStick = filterRadial(
            m_rawAxes[static_cast<gp::UInt8>(GamepadAxis::LeftX)],
            m_rawAxes[static_cast<gp::UInt8>(GamepadAxis::LeftY)],
            m_leftStickDeadzone
        );
        m_rightStick = filterRadial(
            m_rawAxes[static_cast<gp::UInt8>(GamepadAxis::RightX)],
            m_rawAxes[static_cast<gp::UInt8>(GamepadAxis::RightY)],
            m_rightStickDeadzone
        );

        m_filteredAxes[static_cast<gp::UInt8>(GamepadAxis::LeftX)] = m_leftStick.x;
        m_filteredAxes[static_cast<gp::UInt8>(GamepadAxis::LeftY)] = m_leftStick.y;
        m_filteredAxes[static_cast<gp::UInt8>(GamepadAxis::RightX)] = m_rightStick.x;
        m_filteredAxes[static_cast<gp::UInt8>(GamepadAxis::RightY)] = m_rightStick.y;
        m_filteredAxes[static_cast<gp::UInt8>(GamepadAxis::LeftTrigger)] =
            filterAxial(m_rawAxes[static_cast<gp::UInt8>(GamepadAxis::LeftTrigger)], m_triggerDeadzone);
        m_filteredAxes[static_cast<gp::UInt8>(GamepadAxis::RightTrigger)] =
            filterAxial(m_rawAxes[static_cast<gp::UInt8>(GamepadAxis::RightTrigger)], m_triggerDeadzone);
    }
};

}   // namespace gp::hal::input::generic
