// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "input/InputEnums.hpp"
#include "templates/Enums.hpp"

namespace gp::hal::input
{

/// @brief Unique 32-bit OS/backend hardware instance identifier for an input peripheral.
using InputDeviceId = gp::UInt32;

/// @brief Sentinel constant representing an unattached or virtual input device.
inline constexpr InputDeviceId kInvalidInputDeviceId = 0u;

/// @brief Compile-time capacity constants for flat O(1) state lookup tables.
inline constexpr gp::UInt32 kMaxScancodes = static_cast<gp::UInt32>(Scancode::Count);
inline constexpr gp::UInt32 kMaxKeycodeSlots = 256u + kMaxScancodes;
inline constexpr gp::UInt32 kMaxMouseButtons = static_cast<gp::UInt32>(MouseButton::Count);
inline constexpr gp::UInt32 kMaxGamepadButtons = static_cast<gp::UInt32>(GamepadButton::Count);
inline constexpr gp::UInt32 kMaxGamepadAxes = static_cast<gp::UInt32>(GamepadAxis::Count);
inline constexpr gp::UInt32 kMaxTouchContacts = 16u;

/// @brief Bitmask tracking both continuous hold state and sub-frame edge transitions within a single frame (ADR-0034).
enum class ButtonStateFlags : gp::UInt8
{
    None = 0,
    Held = 1 << 0,
    PressedThisFrame = 1 << 1,
    ReleasedThisFrame = 1 << 2,
    RepeatedThisFrame = 1 << 3
};

/// @brief Logical controller slot index insulating gameplay code from volatile OS hardware instance IDs (ADR-0036).
enum class GamepadSlot : gp::UInt8
{
    Player1 = 0,
    Player2 = 1,
    Player3 = 2,
    Player4 = 3,
    Player5 = 4,
    Player6 = 5,
    Player7 = 6,
    Player8 = 7,
    Count = 8,
    Any = 0xFF
};

inline constexpr gp::UInt32 kMaxGamepads = static_cast<gp::UInt32>(GamepadSlot::Count);

/// @brief Mathematical filtering mode applied to analog thumbsticks and triggers (ADR-0035).
enum class DeadzoneMode : gp::UInt8
{
    None = 0,
    Axial = 1,
    ScaledRadial = 2
};

}   // namespace gp::hal::input

GP_ENABLE_ENUM_BITWISE_OPERATIONS(gp::hal::input::ButtonStateFlags);

namespace gp::hal::input
{

/// @brief Represents a 2D analog vector used for thumbstick coordinates, mouse position, and scroll deltas.
struct Stick2D
{
    gp::Float32 x{ 0.0f };
    gp::Float32 y{ 0.0f };

    /// @brief Default constructor initializing vector to (0, 0).
    [[nodiscard]] constexpr Stick2D() noexcept = default;

    /// @brief Constructs a 2D vector with explicit X and Y components.
    /// @param[in] inX Horizontal component.
    /// @param[in] inY Vertical component.
    [[nodiscard]] constexpr Stick2D(gp::Float32 inX, gp::Float32 inY) noexcept
        : x(inX)
        , y(inY)
    {}

    /// @brief Computes the squared magnitude of the 2D vector ($x^2 + y^2$).
    /// @return Squared length of the vector.
    [[nodiscard]] constexpr gp::Float32 lengthSquared() const noexcept
    {
        return (x * x) + (y * y);
    }

    /// @brief Checks exact equality between two Stick2D instances.
    /// @param[in] other Vector to compare against.
    /// @return True if both X and Y match.
    [[nodiscard]] constexpr bool operator==(const Stick2D& other) const noexcept
    {
        return x == other.x && y == other.y;
    }
};

/// @brief Represents a 3D axial vector used for IMU accelerometer and gyroscope readings.
struct Vector3fInput
{
    gp::Float32 x{ 0.0f };
    gp::Float32 y{ 0.0f };
    gp::Float32 z{ 0.0f };

    /// @brief Default constructor initializing vector to (0, 0, 0).
    [[nodiscard]] constexpr Vector3fInput() noexcept = default;

    /// @brief Constructs a 3D vector with explicit X, Y, and Z components.
    /// @param[in] inX X-axis component.
    /// @param[in] inY Y-axis component.
    /// @param[in] inZ Z-axis component.
    [[nodiscard]] constexpr Vector3fInput(gp::Float32 inX, gp::Float32 inY, gp::Float32 inZ) noexcept
        : x(inX)
        , y(inY)
        , z(inZ)
    {}
};

/// @brief Deadzone and response curve configuration for analog sticks and triggers (ADR-0035).
struct AnalogDeadzoneConfig
{
    gp::Float32 innerDeadzone{ 0.12f };
    gp::Float32 outerDeadzone{ 0.95f };
    DeadzoneMode mode{ DeadzoneMode::ScaledRadial };

    /// @brief Default constructor initializing standard thumbstick radial deadzone parameters.
    [[nodiscard]] constexpr AnalogDeadzoneConfig() noexcept = default;

    /// @brief Constructs a deadzone configuration with explicit inner/outer bounds and evaluation mode.
    /// @param[in] inInner Inner threshold below which input is clamped to 0.0f.
    /// @param[in] inOuter Outer threshold at or above which input saturates to 1.0f.
    /// @param[in] inMode Deadzone evaluation mode.
    [[nodiscard]] constexpr AnalogDeadzoneConfig(
        gp::Float32 inInner, gp::Float32 inOuter, DeadzoneMode inMode = DeadzoneMode::ScaledRadial
    ) noexcept
        : innerDeadzone(inInner)
        , outerDeadzone(inOuter)
        , mode(inMode)
    {}
};

/// @brief Configuration descriptor for dual-motor controller vibration (`IHapticDevice::setRumble`).
struct HapticRumbleDesc
{
    gp::Float32 lowFrequencyIntensity{ 0.0f };
    gp::Float32 highFrequencyIntensity{ 0.0f };
    gp::UInt32 durationMs{ 0u };

    /// @brief Default constructor.
    [[nodiscard]] constexpr HapticRumbleDesc() noexcept = default;

    /// @brief Constructs a dual-motor rumble descriptor.
    /// @param[in] inLowFreq Heavy left motor intensity in `[0.0f, 1.0f]`.
    /// @param[in] inHighFreq Fine right motor intensity in `[0.0f, 1.0f]`.
    /// @param[in] inDurationMs Vibration duration in milliseconds.
    [[nodiscard]] constexpr HapticRumbleDesc(
        gp::Float32 inLowFreq, gp::Float32 inHighFreq, gp::UInt32 inDurationMs
    ) noexcept
        : lowFrequencyIntensity(inLowFreq)
        , highFrequencyIntensity(inHighFreq)
        , durationMs(inDurationMs)
    {}
};

/// @brief Configuration descriptor for impulse trigger vibration (`IHapticDevice::setTriggerRumble`).
struct HapticTriggerRumbleDesc
{
    gp::Float32 leftTriggerIntensity{ 0.0f };
    gp::Float32 rightTriggerIntensity{ 0.0f };
    gp::UInt32 durationMs{ 0u };

    /// @brief Default constructor.
    [[nodiscard]] constexpr HapticTriggerRumbleDesc() noexcept = default;

    /// @brief Constructs an impulse trigger rumble descriptor.
    /// @param[in] inLeft Left trigger motor intensity in `[0.0f, 1.0f]`.
    /// @param[in] inRight Right trigger motor intensity in `[0.0f, 1.0f]`.
    /// @param[in] inDurationMs Vibration duration in milliseconds.
    [[nodiscard]] constexpr HapticTriggerRumbleDesc(
        gp::Float32 inLeft, gp::Float32 inRight, gp::UInt32 inDurationMs
    ) noexcept
        : leftTriggerIntensity(inLeft)
        , rightTriggerIntensity(inRight)
        , durationMs(inDurationMs)
    {}
};

/// @brief RGB color value for programmable controller lightbars (DualShock 4, DualSense, Switch Pro).
struct DeviceLEDColor
{
    gp::UInt8 r{ 0u };
    gp::UInt8 g{ 0u };
    gp::UInt8 b{ 0u };

    /// @brief Default constructor initializes LED color to black/off (0, 0, 0).
    [[nodiscard]] constexpr DeviceLEDColor() noexcept = default;

    /// @brief Constructs an RGB LED color.
    /// @param[in] inR Red channel `[0..255]`.
    /// @param[in] inG Green channel `[0..255]`.
    /// @param[in] inB Blue channel `[0..255]`.
    [[nodiscard]] constexpr DeviceLEDColor(gp::UInt8 inR, gp::UInt8 inG, gp::UInt8 inB) noexcept
        : r(inR)
        , g(inG)
        , b(inB)
    {}
};

/// @brief 3D IMU sensor reading snapshot (Accelerometer in $m/s^2$ or Gyroscope in $\text{rad}/s$).
struct SensorReading
{
    gp::UInt64 timestampNs{ 0ULL };
    Vector3fInput value{};
    GamepadSensorType type{ GamepadSensorType::Unknown };
    ActuatorSide side{ ActuatorSide::Both };
    bool isValid{ false };
};

/// @brief Represents a single active multi-touch finger contact point.
struct TouchContactPoint
{
    gp::UInt64 touchDeviceId{ 0ULL };
    gp::UInt64 fingerId{ 0ULL };
    gp::UInt32 windowId{ 0u };
    gp::Float32 x{ 0.0f };
    gp::Float32 y{ 0.0f };
    gp::Float32 deltaX{ 0.0f };
    gp::Float32 deltaY{ 0.0f };
    gp::Float32 pressure{ 0.0f };
    ButtonStateFlags flags{ ButtonStateFlags::None };
    bool isActive{ false };
};

/// @brief Complete state snapshot for a 6-DoF graphics tablet stylus / pen.
struct PenStateSnapshot
{
    gp::UInt32 windowId{ 0u };
    gp::UInt32 penId{ 0u };
    gp::Float32 x{ 0.0f };
    gp::Float32 y{ 0.0f };
    gp::Float32 deltaX{ 0.0f };
    gp::Float32 deltaY{ 0.0f };
    gp::Float32 axes[static_cast<gp::UInt8>(PenAxis::Count)]{};
    PenButton heldButtons{ PenButton::None };
    ButtonStateFlags tipState{ ButtonStateFlags::None };
    bool isInProximity{ false };
    bool isEraser{ false };
};

}   // namespace gp::hal::input
