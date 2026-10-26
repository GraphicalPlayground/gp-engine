// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "input/GenericGamepad.hpp"
#include "input/GenericKeyboard.hpp"
#include "input/GenericMouse.hpp"
#include "input/GenericSpatialDevice.hpp"
#include "input/IInputSystem.hpp"

namespace gp::hal::input::generic
{

/// @brief Platform-agnostic coordinator implementation of `IInputSystem`.
/// @details Owns the primary `Keyboard`, `Mouse`, and `SpatialDevice`, and routes controller events
///          across 8 logical player `Gamepad` slots with hysteresis-gated active device detection.
class InputSystem : public IInputSystem
{
public:
    /// @brief Minimum analog stick deflection required to switch `m_activeDeviceType` to `Gamepad`.
    static constexpr gp::Float32 kDeviceSwitchStickThreshold = 0.25f;

    /// @brief Minimum squared mouse movement distance (in pixels) required to switch `m_activeDeviceType` to `Mouse`.
    static constexpr gp::Float32 kDeviceSwitchMouseThresholdSq = 4.0f;

protected:
    Keyboard m_keyboard{};
    Mouse m_mouse{};
    SpatialDevice m_spatialDevice{};
    Gamepad m_defaultGamepads[kMaxGamepads]{};
    Gamepad* m_gamepadTable[kMaxGamepads]{};

    GamepadSlot m_mostRecentGamepadSlot{ GamepadSlot::Player1 };
    InputDeviceType m_activeDeviceType{ InputDeviceType::Keyboard };

public:
    /// @brief Constructs the generic input system and binds the default generic gamepad slots.
    [[nodiscard]] constexpr InputSystem() noexcept
    {
        for (gp::UInt32 i = 0u; i < kMaxGamepads; ++i)
        {
            m_gamepadTable[i] = &m_defaultGamepads[i];
        }
    }

    /// @brief Virtual destructor.
    ~InputSystem() noexcept override = default;

    /// @copydoc IInputSystem::beginFrame()
    constexpr void beginFrame() noexcept override
    {
        m_keyboard.beginFrame();
        m_mouse.beginFrame();
        m_spatialDevice.beginFrame();

        for (gp::UInt32 i = 0u; i < kMaxGamepads; ++i)
        {
            if (m_gamepadTable[i] != nullptr && m_gamepadTable[i]->isConnected())
            {
                m_gamepadTable[i]->beginFrame();
            }
        }
    }

    /// @copydoc IInputSystem::releaseAllInputs()
    constexpr void releaseAllInputs() noexcept override
    {
        m_keyboard.releaseAllInputs();
        m_mouse.releaseAllInputs();
        m_spatialDevice.releaseAllInputs();

        for (gp::UInt32 i = 0u; i < kMaxGamepads; ++i)
        {
            if (m_gamepadTable[i] != nullptr)
            {
                m_gamepadTable[i]->releaseAllInputs();
            }
        }
    }

    /// @copydoc IInputSystem::reset()
    constexpr void reset() noexcept override
    {
        m_keyboard.reset();
        m_mouse.reset();
        m_spatialDevice.reset();

        for (gp::UInt32 i = 0u; i < kMaxGamepads; ++i)
        {
            if (m_gamepadTable[i] != nullptr)
            {
                m_gamepadTable[i]->reset();
            }
        }
        m_mostRecentGamepadSlot = GamepadSlot::Player1;
        m_activeDeviceType = InputDeviceType::Keyboard;
    }

    /// @copydoc IInputSystem::getActiveDeviceType()
    [[nodiscard]] constexpr InputDeviceType getActiveDeviceType() const noexcept override
    {
        return m_activeDeviceType;
    }

    /// @copydoc IInputSystem::getKeyboard()
    [[nodiscard]] constexpr IKeyboard& getKeyboard() noexcept override
    {
        return m_keyboard;
    }

    /// @copydoc IInputSystem::getKeyboard() const
    [[nodiscard]] constexpr const IKeyboard& getKeyboard() const noexcept override
    {
        return m_keyboard;
    }

    /// @copydoc IInputSystem::getMouse()
    [[nodiscard]] constexpr IMouse& getMouse() noexcept override
    {
        return m_mouse;
    }

    /// @copydoc IInputSystem::getMouse() const
    [[nodiscard]] constexpr const IMouse& getMouse() const noexcept override
    {
        return m_mouse;
    }

    /// @copydoc IInputSystem::getGamepad()
    [[nodiscard]] constexpr IGamepad* getGamepad(GamepadSlot slot = GamepadSlot::Player1) noexcept override
    {
        const gp::UInt32 idx = resolveSlotIndex(slot);
        return (idx < kMaxGamepads) ? m_gamepadTable[idx] : nullptr;
    }

    /// @copydoc IInputSystem::getGamepad() const
    [[nodiscard]] constexpr const IGamepad* getGamepad(GamepadSlot slot = GamepadSlot::Player1) const noexcept override
    {
        const gp::UInt32 idx = resolveSlotIndex(slot);
        return (idx < kMaxGamepads) ? m_gamepadTable[idx] : nullptr;
    }

    /// @copydoc IInputSystem::getConnectedGamepadCount()
    [[nodiscard]] constexpr gp::UInt32 getConnectedGamepadCount() const noexcept override
    {
        gp::UInt32 count = 0u;
        for (gp::UInt32 i = 0u; i < kMaxGamepads; ++i)
        {
            if (m_gamepadTable[i] != nullptr && m_gamepadTable[i]->isConnected())
            {
                ++count;
            }
        }
        return count;
    }

    /// @copydoc IInputSystem::getSpatialDevice()
    [[nodiscard]] constexpr ISpatialDevice& getSpatialDevice() noexcept override
    {
        return m_spatialDevice;
    }

    /// @copydoc IInputSystem::getSpatialDevice() const
    [[nodiscard]] constexpr const ISpatialDevice& getSpatialDevice() const noexcept override
    {
        return m_spatialDevice;
    }

    /// @copydoc IInputSystem::isScancodeDown()
    [[nodiscard]] constexpr bool isScancodeDown(Scancode scancode) const noexcept override
    {
        return m_keyboard.isScancodeDown(scancode);
    }

    /// @copydoc IInputSystem::wasScancodeJustPressed()
    [[nodiscard]] constexpr bool wasScancodeJustPressed(Scancode scancode) const noexcept override
    {
        return m_keyboard.wasScancodeJustPressed(scancode);
    }

    /// @copydoc IInputSystem::isKeyDown()
    [[nodiscard]] constexpr bool isKeyDown(Keycode keycode) const noexcept override
    {
        return m_keyboard.isKeyDown(keycode);
    }

    /// @copydoc IInputSystem::wasKeyJustPressed()
    [[nodiscard]] constexpr bool wasKeyJustPressed(Keycode keycode) const noexcept override
    {
        return m_keyboard.wasKeyJustPressed(keycode);
    }

    /// @copydoc IInputSystem::isMouseButtonDown()
    [[nodiscard]] constexpr bool isMouseButtonDown(MouseButton button) const noexcept override
    {
        return m_mouse.isButtonDown(button);
    }

    /// @copydoc IInputSystem::isGamepadButtonDown()
    [[nodiscard]] constexpr bool isGamepadButtonDown(GamepadButton button, GamepadSlot slot) const noexcept override
    {
        const IGamepad* pad = getGamepad(slot);
        return (pad != nullptr) && pad->isButtonDown(button);
    }

    /// @copydoc IInputSystem::onKeyboardEvent()
    constexpr void onKeyboardEvent(const event::KeyboardEvent& ev) noexcept override
    {
        m_keyboard.onKeyboardEvent(ev);
        if (ev.state == KeyState::Pressed)
        {
            m_activeDeviceType = InputDeviceType::Keyboard;
        }
    }

    /// @copydoc IInputSystem::onMouseEvent()
    constexpr void onMouseEvent(const event::MouseEvent& ev) noexcept override
    {
        m_mouse.onMouseEvent(ev);

        if (ev.action == event::MouseAction::ButtonChanged && ev.state == KeyState::Pressed)
        {
            m_activeDeviceType = InputDeviceType::Mouse;
        }
        else if (ev.action == event::MouseAction::WheelScrolled)
        {
            m_activeDeviceType = InputDeviceType::Mouse;
        }
        else if (ev.action == event::MouseAction::Moved)
        {
            const gp::Float32 distSq = (ev.deltaX * ev.deltaX) + (ev.deltaY * ev.deltaY);
            if (distSq >= kDeviceSwitchMouseThresholdSq)
            {
                m_activeDeviceType = InputDeviceType::Mouse;
            }
        }
    }

    /// @copydoc IInputSystem::onGamepadEvent()
    constexpr void onGamepadEvent(const event::GamepadEvent& ev) noexcept override
    {
        const gp::UInt32 slotIdx = resolveOrAllocateSlotForDevice(ev.deviceId);
        if (slotIdx >= kMaxGamepads || m_gamepadTable[slotIdx] == nullptr) [[unlikely]]
        {
            return;
        }

        Gamepad& pad = *m_gamepadTable[slotIdx];
        pad.onGamepadEvent(ev);

        if (ev.action == event::GamepadAction::ButtonChanged && ev.state == KeyState::Pressed)
        {
            m_mostRecentGamepadSlot = static_cast<GamepadSlot>(slotIdx);
            m_activeDeviceType = InputDeviceType::Gamepad;
        }
        else if (ev.action == event::GamepadAction::AxisMoved)
        {
            const gp::Float32 threshSq = kDeviceSwitchStickThreshold * kDeviceSwitchStickThreshold;
            if (pad.getLeftStick().lengthSquared() >= threshSq || pad.getRightStick().lengthSquared() >= threshSq ||
                pad.getAxis(ev.control.axis) >= kDeviceSwitchStickThreshold)
            {
                m_mostRecentGamepadSlot = static_cast<GamepadSlot>(slotIdx);
                m_activeDeviceType = InputDeviceType::Gamepad;
            }
        }
        else if (ev.action == event::GamepadAction::HatChanged && ev.control.hatPosition != HatPosition::Centered)
        {
            m_mostRecentGamepadSlot = static_cast<GamepadSlot>(slotIdx);
            m_activeDeviceType = InputDeviceType::Gamepad;
        }
    }

    /// @copydoc IInputSystem::onTouchEvent()
    constexpr void onTouchEvent(const event::TouchEvent& ev) noexcept override
    {
        m_spatialDevice.onTouchEvent(ev);
        m_activeDeviceType = InputDeviceType::Touch;
    }

    /// @copydoc IInputSystem::onPenEvent()
    constexpr void onPenEvent(const event::PenEvent& ev) noexcept override
    {
        m_spatialDevice.onPenEvent(ev);
        m_activeDeviceType = InputDeviceType::Pen;
    }

    /// @copydoc IInputSystem::onSensorEvent()
    constexpr void onSensorEvent(const event::SensorEvent& ev) noexcept override
    {
        // Route to the owning controller if `ev.deviceId` matches a connected gamepad; otherwise route to standalone
        // spatial device
        for (gp::UInt32 i = 0u; i < kMaxGamepads; ++i)
        {
            if (m_gamepadTable[i] != nullptr && m_gamepadTable[i]->getDeviceId() == ev.deviceId)
            {
                m_gamepadTable[i]->onSensorEvent(ev);
                return;
            }
        }
        m_spatialDevice.onSensorEvent(ev);
    }

    /// @copydoc IInputSystem::onDeviceChangeEvent()
    void onDeviceChangeEvent(const event::DeviceChangeEvent& ev) noexcept override
    {
        if (ev.domain != event::DeviceDomain::Input || ev.inputType != InputDeviceType::Gamepad)
        {
            return;
        }

        if (ev.action == event::DeviceAction::Connected)
        {
            const gp::UInt32 slotIdx = resolveOrAllocateSlotForDevice(ev.deviceId);
            if (slotIdx < kMaxGamepads)
            {
                onBackendGamepadConnected(slotIdx, ev.deviceId);
            }
        }
        else if (ev.action == event::DeviceAction::Disconnected)
        {
            for (gp::UInt32 i = 0u; i < kMaxGamepads; ++i)
            {
                if (m_gamepadTable[i] != nullptr && m_gamepadTable[i]->getDeviceId() == ev.deviceId)
                {
                    onBackendGamepadDisconnected(i, ev.deviceId);
                    m_gamepadTable[i]->detach();
                    break;
                }
            }
        }
    }

protected:
    /// @brief Hook invoked when a gamepad connects to `slotIndex` so derived backends (SDL3) can open native handles.
    /// @param[in] slotIndex Logical player slot index `[0, kMaxGamepads)`.
    /// @param[in] deviceId Native OS instance identifier.
    virtual void onBackendGamepadConnected(
        [[maybe_unused]] gp::UInt32 slotIndex, [[maybe_unused]] InputDeviceId deviceId
    ) noexcept
    {}

    /// @brief Hook invoked when a gamepad disconnects from `slotIndex` so derived backends can close native handles.
    /// @param[in] slotIndex Logical player slot index `[0, kMaxGamepads)`.
    /// @param[in] deviceId Native OS instance identifier.
    virtual void onBackendGamepadDisconnected(
        [[maybe_unused]] gp::UInt32 slotIndex, [[maybe_unused]] InputDeviceId deviceId
    ) noexcept
    {}

    /// @brief Resolves a `GamepadSlot` enumerator (including `GamepadSlot::Any`) to an array index.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::UInt32 resolveSlotIndex(GamepadSlot slot) const noexcept
    {
        return (slot == GamepadSlot::Any) ? static_cast<gp::UInt32>(m_mostRecentGamepadSlot)
                                          : static_cast<gp::UInt32>(slot);
    }

    /// @brief Finds the slot bound to `deviceId` or allocates the lowest-numbered disconnected player slot (ADR-0036).
    [[nodiscard]] constexpr gp::UInt32 resolveOrAllocateSlotForDevice(InputDeviceId deviceId) noexcept
    {
        if (deviceId == kInvalidInputDeviceId) [[unlikely]]
        {
            return kMaxGamepads;
        }

        for (gp::UInt32 i = 0u; i < kMaxGamepads; ++i)
        {
            if (m_gamepadTable[i] != nullptr && m_gamepadTable[i]->getDeviceId() == deviceId)
            {
                return i;
            }
        }

        for (gp::UInt32 i = 0u; i < kMaxGamepads; ++i)
        {
            if (m_gamepadTable[i] != nullptr && !m_gamepadTable[i]->isConnected())
            {
                m_gamepadTable[i]->attach(deviceId, static_cast<GamepadSlot>(i));
                return i;
            }
        }

        return kMaxGamepads;
    }
};

}   // namespace gp::hal::input::generic
