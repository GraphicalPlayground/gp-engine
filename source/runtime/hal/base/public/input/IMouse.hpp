// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "event/input/MouseEvent.hpp"
#include "input/IInputDevice.hpp"

namespace gp::hal::input
{

/// @brief Interface for mouse pointer, button, and scroll wheel input devices.
class IMouse : public IInputDevice
{
public:
    /// @brief Default constructor.
    [[nodiscard]] constexpr IMouse() noexcept = default;

    /// @brief Virtual destructor.
    ~IMouse() noexcept override = default;

public:
    /// @brief Checks if the specified mouse button is currently held down.
    /// @param[in] button Target `MouseButton` (`Left`, `Middle`, `Right`, `X1`, `X2`).
    /// @return True if held down, false otherwise.
    [[nodiscard]] virtual bool isButtonDown(MouseButton button) const noexcept = 0;

    /// @brief Checks if the specified mouse button transitioned to Pressed during the current frame.
    /// @param[in] button Target `MouseButton`.
    /// @return True if pressed this frame (preserves sub-frame micro-clicks).
    [[nodiscard]] virtual bool wasButtonJustPressed(MouseButton button) const noexcept = 0;

    /// @brief Checks if the specified mouse button transitioned to Released during the current frame.
    /// @param[in] button Target `MouseButton`.
    /// @return True if released this frame, false otherwise.
    [[nodiscard]] virtual bool wasButtonJustReleased(MouseButton button) const noexcept = 0;

    /// @brief Checks if the specified mouse button was double-clicked during the current frame.
    /// @param[in] button Target `MouseButton`.
    /// @return True if pressed this frame with a click count >= 2.
    [[nodiscard]] virtual bool wasButtonDoubleClicked(MouseButton button) const noexcept = 0;

    /// @brief Retrieves the raw `ButtonStateFlags` bitmask for a mouse button.
    /// @param[in] button Target `MouseButton`.
    /// @return `ButtonStateFlags` bitmask.
    [[nodiscard]] virtual ButtonStateFlags getButtonState(MouseButton button) const noexcept = 0;

    /// @brief Retrieves the bitmask of all simultaneously held mouse buttons.
    /// @return `MouseButtonMask` bitmask.
    [[nodiscard]] virtual MouseButtonMask getHeldButtonMask() const noexcept = 0;

    /// @brief Retrieves the current cursor position in window client coordinates.
    /// @return `Stick2D` containing sub-pixel `(x, y)` coordinates.
    [[nodiscard]] virtual Stick2D getPosition() const noexcept = 0;

    /// @brief Retrieves the accumulated raw relative mouse movement delta for the current frame.
    /// @return `Stick2D` containing `(deltaX, deltaY)`.
    [[nodiscard]] virtual Stick2D getDelta() const noexcept = 0;

    /// @brief Retrieves the accumulated mouse scroll wheel delta for the current frame.
    /// @return `Stick2D` containing `(horizontalScroll, verticalScroll)`.
    [[nodiscard]] virtual Stick2D getWheelDelta() const noexcept = 0;

    /// @brief Retrieves the ID of the window currently receiving pointer events from this mouse.
    /// @return Window identifier.
    [[nodiscard]] virtual gp::UInt32 getFocusedWindowId() const noexcept = 0;

    /// @brief Ingests a decoded HAL `MouseEvent` packet into the mouse state.
    /// @param[in] ev The decoded mouse event packet.
    virtual void onMouseEvent(const event::MouseEvent& ev) noexcept = 0;
};

}   // namespace gp::hal::input
