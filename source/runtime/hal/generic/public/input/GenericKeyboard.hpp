// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "input/IKeyboard.hpp"

namespace gp::hal::input::generic
{

/// @brief Platform-agnostic implementation of `IKeyboard` with O(1) Scancode/Keycode tables and sub-frame edge
/// latching.
class Keyboard : public IKeyboard
{
public:
    static constexpr gp::UInt32 kOverflowKeycodeCapacity = 32u;

protected:
    struct OverflowKeyEntry
    {
        Keycode keycode{ Keycode::Unknown };
        ButtonStateFlags flags{ ButtonStateFlags::None };
    };

    ButtonStateFlags m_scancodes[kMaxScancodes]{};
    ButtonStateFlags m_keycodes[kMaxKeycodeSlots]{};
    OverflowKeyEntry m_overflowKeycodes[kOverflowKeycodeCapacity]{};
    KeyModifier m_modifiers{ KeyModifier::None };
    InputDeviceId m_deviceId{ 1u };
    bool m_isConnected{ true };

public:
    /// @brief Default constructor initializes all key states to Released.
    [[nodiscard]] constexpr Keyboard() noexcept = default;

    /// @brief Virtual destructor.
    ~Keyboard() noexcept override = default;

    /// @copydoc IInputDevice::getDeviceId()
    [[nodiscard]] constexpr InputDeviceId getDeviceId() const noexcept override
    {
        return m_deviceId;
    }

    /// @copydoc IInputDevice::getDeviceType()
    [[nodiscard]] constexpr InputDeviceType getDeviceType() const noexcept override
    {
        return InputDeviceType::Keyboard;
    }

    /// @copydoc IInputDevice::getDeviceName()
    [[nodiscard]] constexpr StringView getDeviceName() const noexcept override
    {
        return StringView("System Keyboard");
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
        for (gp::UInt32 i = 0u; i < kMaxScancodes; ++i)
        {
            m_scancodes[i] &= ButtonStateFlags::Held;
        }
        for (gp::UInt32 i = 0u; i < kMaxKeycodeSlots; ++i)
        {
            m_keycodes[i] &= ButtonStateFlags::Held;
        }
        for (gp::UInt32 i = 0u; i < kOverflowKeycodeCapacity; ++i)
        {
            m_overflowKeycodes[i].flags &= ButtonStateFlags::Held;
        }
    }

    /// @copydoc IInputDevice::releaseAllInputs()
    constexpr void releaseAllInputs() noexcept override
    {
        for (gp::UInt32 i = 0u; i < kMaxScancodes; ++i)
        {
            if ((m_scancodes[i] & ButtonStateFlags::Held) != ButtonStateFlags::None)
            {
                m_scancodes[i] = (m_scancodes[i] & ~ButtonStateFlags::Held) | ButtonStateFlags::ReleasedThisFrame;
            }
        }
        for (gp::UInt32 i = 0u; i < kMaxKeycodeSlots; ++i)
        {
            if ((m_keycodes[i] & ButtonStateFlags::Held) != ButtonStateFlags::None)
            {
                m_keycodes[i] = (m_keycodes[i] & ~ButtonStateFlags::Held) | ButtonStateFlags::ReleasedThisFrame;
            }
        }
        for (gp::UInt32 i = 0u; i < kOverflowKeycodeCapacity; ++i)
        {
            if ((m_overflowKeycodes[i].flags & ButtonStateFlags::Held) != ButtonStateFlags::None)
            {
                m_overflowKeycodes[i].flags =
                    (m_overflowKeycodes[i].flags & ~ButtonStateFlags::Held) | ButtonStateFlags::ReleasedThisFrame;
            }
        }
        m_modifiers = KeyModifier::None;
    }

    /// @copydoc IInputDevice::reset()
    constexpr void reset() noexcept override
    {
        for (gp::UInt32 i = 0u; i < kMaxScancodes; ++i)
        {
            m_scancodes[i] = ButtonStateFlags::None;
        }
        for (gp::UInt32 i = 0u; i < kMaxKeycodeSlots; ++i)
        {
            m_keycodes[i] = ButtonStateFlags::None;
        }
        for (gp::UInt32 i = 0u; i < kOverflowKeycodeCapacity; ++i)
        {
            m_overflowKeycodes[i] = OverflowKeyEntry{};
        }
        m_modifiers = KeyModifier::None;
    }

    /// @copydoc IKeyboard::isScancodeDown()
    [[nodiscard]] constexpr bool isScancodeDown(Scancode scancode) const noexcept override
    {
        return (getScancodeState(scancode) & ButtonStateFlags::Held) != ButtonStateFlags::None;
    }

    /// @copydoc IKeyboard::wasScancodeJustPressed()
    [[nodiscard]] constexpr bool wasScancodeJustPressed(Scancode scancode) const noexcept override
    {
        return (getScancodeState(scancode) & ButtonStateFlags::PressedThisFrame) != ButtonStateFlags::None;
    }

    /// @copydoc IKeyboard::wasScancodeJustReleased()
    [[nodiscard]] constexpr bool wasScancodeJustReleased(Scancode scancode) const noexcept override
    {
        return (getScancodeState(scancode) & ButtonStateFlags::ReleasedThisFrame) != ButtonStateFlags::None;
    }

    /// @copydoc IKeyboard::wasScancodeRepeated()
    [[nodiscard]] constexpr bool wasScancodeRepeated(Scancode scancode) const noexcept override
    {
        return (getScancodeState(scancode) & ButtonStateFlags::RepeatedThisFrame) != ButtonStateFlags::None;
    }

    /// @copydoc IKeyboard::getScancodeState()
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr ButtonStateFlags
        getScancodeState(Scancode scancode) const noexcept override
    {
        const auto idx = static_cast<gp::UInt16>(scancode);
        return (idx < kMaxScancodes) ? m_scancodes[idx] : ButtonStateFlags::None;
    }

    /// @copydoc IKeyboard::isKeyDown()
    [[nodiscard]] constexpr bool isKeyDown(Keycode keycode) const noexcept override
    {
        return (getKeyState(keycode) & ButtonStateFlags::Held) != ButtonStateFlags::None;
    }

    /// @copydoc IKeyboard::wasKeyJustPressed()
    [[nodiscard]] constexpr bool wasKeyJustPressed(Keycode keycode) const noexcept override
    {
        return (getKeyState(keycode) & ButtonStateFlags::PressedThisFrame) != ButtonStateFlags::None;
    }

    /// @copydoc IKeyboard::wasKeyJustReleased()
    [[nodiscard]] constexpr bool wasKeyJustReleased(Keycode keycode) const noexcept override
    {
        return (getKeyState(keycode) & ButtonStateFlags::ReleasedThisFrame) != ButtonStateFlags::None;
    }

    /// @copydoc IKeyboard::getKeyState()
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr ButtonStateFlags getKeyState(Keycode keycode) const noexcept override
    {
        const gp::UInt32 slot = mapKeycodeToSlot(keycode);
        if (slot < kMaxKeycodeSlots) [[likely]]
        {
            return m_keycodes[slot];
        }

        for (gp::UInt32 i = 0u; i < kOverflowKeycodeCapacity; ++i)
        {
            if (m_overflowKeycodes[i].keycode == keycode)
            {
                return m_overflowKeycodes[i].flags;
            }
        }
        return ButtonStateFlags::None;
    }

    /// @copydoc IKeyboard::getModifiers()
    [[nodiscard]] constexpr KeyModifier getModifiers() const noexcept override
    {
        return m_modifiers;
    }

    /// @copydoc IKeyboard::hasModifiers()
    [[nodiscard]] constexpr bool hasModifiers(KeyModifier modifiers) const noexcept override
    {
        if (modifiers == KeyModifier::None)
        {
            return true;
        }

        KeyModifier remaining = modifiers;

        // Evaluate paired convenience masks (Shift, Ctrl, Alt, Gui): satisfied if EITHER Left or Right is held
        if ((remaining & KeyModifier::Shift) == KeyModifier::Shift)
        {
            if ((m_modifiers & KeyModifier::Shift) == KeyModifier::None)
            {
                return false;
            }
            remaining &= ~KeyModifier::Shift;
        }

        if ((remaining & KeyModifier::Ctrl) == KeyModifier::Ctrl)
        {
            if ((m_modifiers & KeyModifier::Ctrl) == KeyModifier::None)
            {
                return false;
            }
            remaining &= ~KeyModifier::Ctrl;
        }

        if ((remaining & KeyModifier::Alt) == KeyModifier::Alt)
        {
            if ((m_modifiers & KeyModifier::Alt) == KeyModifier::None)
            {
                return false;
            }
            remaining &= ~KeyModifier::Alt;
        }

        if ((remaining & KeyModifier::Gui) == KeyModifier::Gui)
        {
            if ((m_modifiers & KeyModifier::Gui) == KeyModifier::None)
            {
                return false;
            }
            remaining &= ~KeyModifier::Gui;
        }

        // Any remaining single-key bits (e.g. LeftCtrl, RightAlt, CapsLock, NumLock) must match exactly
        return (m_modifiers & remaining) == remaining;
    }

    /// @copydoc IKeyboard::onKeyboardEvent()
    constexpr void onKeyboardEvent(const event::KeyboardEvent& ev) noexcept override
    {
        const bool isRepeat = (ev.isRepeat != 0u);
        const auto scancodeIdx = static_cast<gp::UInt16>(ev.scancode);

        if (scancodeIdx < kMaxScancodes) [[likely]]
        {
            applyTransition(m_scancodes[scancodeIdx], ev.state, isRepeat);
        }

        const gp::UInt32 keySlot = mapKeycodeToSlot(ev.keycode);
        if (keySlot < kMaxKeycodeSlots) [[likely]]
        {
            applyTransition(m_keycodes[keySlot], ev.state, isRepeat);
        }
        else
        {
            updateOverflowKeycode(ev.keycode, ev.state, isRepeat);
        }

        // Combine OS-reported modifiers with physical modifier scancode states and strip any just-released modifier
        m_modifiers = (ev.modifiers | deriveHeldModifiersFromScancodes());
        if (ev.state == KeyState::Released)
        {
            m_modifiers &= ~scancodeToModifier(ev.scancode);
        }
    }

private:
    /// @brief Maps a physical modifier `Scancode` to its corresponding `KeyModifier` bit.
    /// @param[in] scancode Physical key scancode.
    /// @return Corresponding `KeyModifier` bit, or `KeyModifier::None` if not a modifier key.
    [[nodiscard]] GP_FORCEINLINE_HINT static constexpr KeyModifier scancodeToModifier(Scancode scancode) noexcept
    {
        switch (scancode)
        {
        case Scancode::LeftShift:
            return KeyModifier::LeftShift;
        case Scancode::RightShift:
            return KeyModifier::RightShift;
        case Scancode::LeftCtrl:
            return KeyModifier::LeftCtrl;
        case Scancode::RightCtrl:
            return KeyModifier::RightCtrl;
        case Scancode::LeftAlt:
            return KeyModifier::LeftAlt;
        case Scancode::RightAlt:
            return KeyModifier::RightAlt;
        case Scancode::LeftGui:
            return KeyModifier::LeftGui;
        case Scancode::RightGui:
            return KeyModifier::RightGui;
        default:
            return KeyModifier::None;
        }
    }

    /// @brief Builds a `KeyModifier` bitmask from the currently held physical modifier scancodes.
    /// @return Active `KeyModifier` flags derived from `m_scancodes`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr KeyModifier deriveHeldModifiersFromScancodes() const noexcept
    {
        KeyModifier held = KeyModifier::None;
        if (isScancodeDown(Scancode::LeftShift))
        {
            held |= KeyModifier::LeftShift;
        }
        if (isScancodeDown(Scancode::RightShift))
        {
            held |= KeyModifier::RightShift;
        }
        if (isScancodeDown(Scancode::LeftCtrl))
        {
            held |= KeyModifier::LeftCtrl;
        }
        if (isScancodeDown(Scancode::RightCtrl))
        {
            held |= KeyModifier::RightCtrl;
        }
        if (isScancodeDown(Scancode::LeftAlt))
        {
            held |= KeyModifier::LeftAlt;
        }
        if (isScancodeDown(Scancode::RightAlt))
        {
            held |= KeyModifier::RightAlt;
        }
        if (isScancodeDown(Scancode::LeftGui))
        {
            held |= KeyModifier::LeftGui;
        }
        if (isScancodeDown(Scancode::RightGui))
        {
            held |= KeyModifier::RightGui;
        }
        return held;
    }

    /// @brief Maps a 32-bit `Keycode` into `[0, kMaxKeycodeSlots)` in O(1) time.
    /// @param[in] keycode Virtual keycode to map.
    /// @return Compact table index, or `kMaxKeycodeSlots` if unmapped Unicode.
    [[nodiscard]] GP_FORCEINLINE_HINT static constexpr gp::UInt32 mapKeycodeToSlot(Keycode keycode) noexcept
    {
        const gp::UInt32 raw = static_cast<gp::UInt32>(keycode);
        if (raw < 256u) [[likely]]
        {
            return raw;
        }
        if ((raw & kScancodeMask) != 0u) [[likely]]
        {
            const gp::UInt32 sc = raw & ~kScancodeMask;
            if (sc < kMaxScancodes) [[likely]]
            {
                return 256u + sc;
            }
        }
        return kMaxKeycodeSlots;
    }

    /// @brief Applies a press or release transition while preserving intra-frame edge latches.
    /// @param[in,out] flags Target button state flags.
    /// @param[in] state Incoming key press or release state.
    /// @param[in] isRepeat True if generated by OS key auto-repeat.
    GP_FORCEINLINE_HINT static constexpr void
        applyTransition(ButtonStateFlags& flags, KeyState state, bool isRepeat) noexcept
    {
        if (state == KeyState::Pressed)
        {
            if (isRepeat)
            {
                flags |= ButtonStateFlags::Held | ButtonStateFlags::RepeatedThisFrame;
            }
            else if ((flags & ButtonStateFlags::Held) == ButtonStateFlags::None)
            {
                flags |= ButtonStateFlags::Held | ButtonStateFlags::PressedThisFrame;
            }
        }
        else
        {
            if ((flags & ButtonStateFlags::Held) != ButtonStateFlags::None)
            {
                flags = (flags & ~ButtonStateFlags::Held) | ButtonStateFlags::ReleasedThisFrame;
            }
        }
    }

    /// @brief Updates the fallback table for rare international layout keycodes outside `[0..255]` and `kScancodeMask`.
    /// @param[in] keycode Target virtual keycode.
    /// @param[in] state Incoming press or release state.
    /// @param[in] isRepeat True if OS key repeat.
    constexpr void updateOverflowKeycode(Keycode keycode, KeyState state, bool isRepeat) noexcept
    {
        gp::UInt32 freeIdx = kOverflowKeycodeCapacity;
        for (gp::UInt32 i = 0u; i < kOverflowKeycodeCapacity; ++i)
        {
            if (m_overflowKeycodes[i].keycode == keycode)
            {
                applyTransition(m_overflowKeycodes[i].flags, state, isRepeat);
                return;
            }
            if (freeIdx == kOverflowKeycodeCapacity && m_overflowKeycodes[i].keycode == Keycode::Unknown)
            {
                freeIdx = i;
            }
        }
        if (freeIdx < kOverflowKeycodeCapacity)
        {
            m_overflowKeycodes[freeIdx].keycode = keycode;
            applyTransition(m_overflowKeycodes[freeIdx].flags, state, isRepeat);
        }
    }
};

}   // namespace gp::hal::input::generic
