// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "GenericHAL.hpp"
#include "input/IInputDevice.hpp"
#include "input/ISpatialDevice.hpp"

namespace gp::hal::input::generic
{

/// @brief Platform-agnostic implementation of `ISpatialDevice` managing multi-touch contacts, 6-DoF styluses, and
/// standalone IMUs.
class GP_HAL_GENERIC_API SpatialDevice
    : public IInputDevice
    , public ISpatialDevice
{
protected:
    TouchContactPoint m_touchContacts[kMaxTouchContacts]{};
    PenStateSnapshot m_penState{};
    SensorReading m_accelerometer{};
    SensorReading m_gyroscope{};
    InputDeviceId m_deviceId{ 1u };

public:
    /// @brief Default constructor.
    [[nodiscard]] constexpr SpatialDevice() noexcept = default;

    /// @brief Virtual destructor.
    ~SpatialDevice() noexcept override = default;

    /// @copydoc IInputDevice::getDeviceId()
    [[nodiscard]] constexpr InputDeviceId getDeviceId() const noexcept override
    {
        return m_deviceId;
    }

    /// @copydoc IInputDevice::getDeviceType()
    [[nodiscard]] constexpr InputDeviceType getDeviceType() const noexcept override
    {
        return m_penState.isInProximity ? InputDeviceType::Pen : InputDeviceType::Touch;
    }

    /// @copydoc IInputDevice::getDeviceName()
    [[nodiscard]] constexpr StringView getDeviceName() const noexcept override
    {
        return StringView("System Spatial Touch/Pen Device");
    }

    /// @copydoc IInputDevice::isConnected()
    [[nodiscard]] constexpr bool isConnected() const noexcept override
    {
        return true;
    }

    /// @copydoc IInputDevice::getBatteryPercent()
    [[nodiscard]] constexpr gp::UInt8 getBatteryPercent() const noexcept override
    {
        return 100u;
    }

    /// @copydoc IInputDevice::beginFrame()
    constexpr void beginFrame() noexcept override
    {
        for (gp::UInt32 i = 0u; i < kMaxTouchContacts; ++i)
        {
            TouchContactPoint& contact = m_touchContacts[i];
            if (contact.isActive)
            {
                contact.deltaX = 0.0f;
                contact.deltaY = 0.0f;
                contact.flags &= ButtonStateFlags::Held;
                if ((contact.flags & ButtonStateFlags::Held) == ButtonStateFlags::None)
                {
                    contact.isActive = false;
                }
            }
        }

        m_penState.deltaX = 0.0f;
        m_penState.deltaY = 0.0f;
        m_penState.tipState &= ButtonStateFlags::Held;
    }

    /// @copydoc IInputDevice::releaseAllInputs()
    constexpr void releaseAllInputs() noexcept override
    {
        for (gp::UInt32 i = 0u; i < kMaxTouchContacts; ++i)
        {
            if (m_touchContacts[i].isActive)
            {
                if ((m_touchContacts[i].flags & ButtonStateFlags::Held) != ButtonStateFlags::None)
                {
                    m_touchContacts[i].flags =
                        (m_touchContacts[i].flags & ~ButtonStateFlags::Held) | ButtonStateFlags::ReleasedThisFrame;
                }
                m_touchContacts[i].pressure = 0.0f;
            }
        }

        if ((m_penState.tipState & ButtonStateFlags::Held) != ButtonStateFlags::None)
        {
            m_penState.tipState = (m_penState.tipState & ~ButtonStateFlags::Held) | ButtonStateFlags::ReleasedThisFrame;
        }
        m_penState.heldButtons = PenButton::None;
    }

    /// @copydoc IInputDevice::reset()
    constexpr void reset() noexcept override
    {
        for (gp::UInt32 i = 0u; i < kMaxTouchContacts; ++i)
        {
            m_touchContacts[i] = TouchContactPoint{};
        }
        m_penState = PenStateSnapshot{};
        m_accelerometer = SensorReading{};
        m_gyroscope = SensorReading{};
    }

    /// @copydoc ISpatialDevice::hasSensor()
    [[nodiscard]] constexpr bool hasSensor(GamepadSensorType type) const noexcept override
    {
        return (type == GamepadSensorType::Accelerometer) ? m_accelerometer.isValid : m_gyroscope.isValid;
    }

    /// @copydoc ISpatialDevice::isSensorEnabled()
    [[nodiscard]] constexpr bool isSensorEnabled(GamepadSensorType type) const noexcept override
    {
        return hasSensor(type);
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
        gp::UInt32 count = 0u;
        for (gp::UInt32 i = 0u; i < kMaxTouchContacts; ++i)
        {
            if (m_touchContacts[i].isActive &&
                (m_touchContacts[i].flags & ButtonStateFlags::Held) != ButtonStateFlags::None)
            {
                ++count;
            }
        }
        return count;
    }

    /// @copydoc ISpatialDevice::getTouchContact()
    [[nodiscard]] constexpr const TouchContactPoint* getTouchContact(gp::UInt32 index) const noexcept override
    {
        return (index < kMaxTouchContacts && m_touchContacts[index].isActive) ? &m_touchContacts[index] : nullptr;
    }

    /// @copydoc ISpatialDevice::getPenState()
    [[nodiscard]] constexpr const PenStateSnapshot& getPenState() const noexcept override
    {
        return m_penState;
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
    constexpr void onTouchEvent(const event::TouchEvent& ev) noexcept override
    {
        gp::UInt32 targetSlot = kMaxTouchContacts;
        for (gp::UInt32 i = 0u; i < kMaxTouchContacts; ++i)
        {
            if (m_touchContacts[i].isActive && m_touchContacts[i].fingerId == ev.fingerId &&
                m_touchContacts[i].touchDeviceId == ev.touchDeviceId)
            {
                targetSlot = i;
                break;
            }
            if (!m_touchContacts[i].isActive && targetSlot == kMaxTouchContacts)
            {
                targetSlot = i;
            }
        }

        if (targetSlot >= kMaxTouchContacts) [[unlikely]]
        {
            return;
        }

        TouchContactPoint& contact = m_touchContacts[targetSlot];
        contact.touchDeviceId = ev.touchDeviceId;
        contact.fingerId = ev.fingerId;
        contact.windowId = ev.windowId;
        contact.x = ev.x;
        contact.y = ev.y;
        contact.deltaX += ev.deltaX;
        contact.deltaY += ev.deltaY;
        contact.pressure = ev.pressure;
        contact.isActive = true;

        if (ev.state == TouchFingerState::Down)
        {
            if ((contact.flags & ButtonStateFlags::Held) == ButtonStateFlags::None)
            {
                contact.flags |= ButtonStateFlags::Held | ButtonStateFlags::PressedThisFrame;
            }
        }
        else if (ev.state == TouchFingerState::Up || ev.isCancelled != 0u)
        {
            if ((contact.flags & ButtonStateFlags::Held) != ButtonStateFlags::None)
            {
                contact.flags = (contact.flags & ~ButtonStateFlags::Held) | ButtonStateFlags::ReleasedThisFrame;
            }
        }
    }

    /// @copydoc ISpatialDevice::onPenEvent()
    constexpr void onPenEvent(const event::PenEvent& ev) noexcept override
    {
        m_penState.windowId = ev.windowId;
        m_penState.penId = ev.penId;

        switch (ev.action)
        {
        case event::PenAction::ProximityIn:
            m_penState.isInProximity = true;
            break;

        case event::PenAction::ProximityOut:
            m_penState.isInProximity = false;
            if ((m_penState.tipState & ButtonStateFlags::Held) != ButtonStateFlags::None)
            {
                m_penState.tipState =
                    (m_penState.tipState & ~ButtonStateFlags::Held) | ButtonStateFlags::ReleasedThisFrame;
            }
            m_penState.heldButtons = PenButton::None;
            break;

        case event::PenAction::TipChanged:
            m_penState.x = ev.x;
            m_penState.y = ev.y;
            m_penState.heldButtons = ev.buttons;
            m_penState.isEraser = (ev.isEraser != 0u);
            if (ev.state == KeyState::Pressed)
            {
                if ((m_penState.tipState & ButtonStateFlags::Held) == ButtonStateFlags::None)
                {
                    m_penState.tipState |= ButtonStateFlags::Held | ButtonStateFlags::PressedThisFrame;
                }
            }
            else
            {
                if ((m_penState.tipState & ButtonStateFlags::Held) != ButtonStateFlags::None)
                {
                    m_penState.tipState =
                        (m_penState.tipState & ~ButtonStateFlags::Held) | ButtonStateFlags::ReleasedThisFrame;
                }
            }
            break;

        case event::PenAction::Moved:
            m_penState.deltaX += (ev.x - m_penState.x);
            m_penState.deltaY += (ev.y - m_penState.y);
            m_penState.x = ev.x;
            m_penState.y = ev.y;
            m_penState.heldButtons = ev.buttons;
            m_penState.isEraser = (ev.isEraser != 0u);
            for (gp::UInt8 i = 0u; i < static_cast<gp::UInt8>(PenAxis::Count); ++i)
            {
                m_penState.axes[i] = ev.axes[i];
            }
            break;

        case event::PenAction::ButtonChanged:
            m_penState.x = ev.x;
            m_penState.y = ev.y;
            m_penState.heldButtons = ev.buttons;
            break;

        case event::PenAction::None:
        default:
            break;
        }
    }
};

}   // namespace gp::hal::input::generic
