// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "event/input/PenEvent.hpp"
#include "event/input/SensorEvent.hpp"
#include "event/input/TouchEvent.hpp"
#include "input/InputEnums.hpp"
#include "input/InputTypes.hpp"

namespace gp::hal::input
{

/// @brief Capability interface for 3D motion sensors (IMU Gyro/Accel), multi-touch surfaces, and 6-DoF styluses
/// (ADR-0018).
class ISpatialDevice
{
public:
    /// @brief Default constructor.
    [[nodiscard]] constexpr ISpatialDevice() noexcept = default;

    /// @brief Virtual destructor.
    virtual ~ISpatialDevice() noexcept = default;

    ISpatialDevice(const ISpatialDevice&) = delete;
    ISpatialDevice& operator=(const ISpatialDevice&) = delete;
    ISpatialDevice(ISpatialDevice&&) = delete;
    ISpatialDevice& operator=(ISpatialDevice&&) = delete;

public:
    /// @brief Checks whether the device hardware includes the specified 3D motion sensor.
    /// @param[in] type Sensor type (`Accelerometer` or `Gyroscope`).
    /// @return True if present on the hardware, false otherwise.
    [[nodiscard]] virtual bool hasSensor(GamepadSensorType type) const noexcept = 0;

    /// @brief Checks whether polling is currently enabled for the specified 3D motion sensor.
    /// @param[in] type Sensor type (`Accelerometer` or `Gyroscope`).
    /// @return True if enabled and streaming `SensorEvent` packets, false otherwise.
    [[nodiscard]] virtual bool isSensorEnabled(GamepadSensorType type) const noexcept = 0;

    /// @brief Enables or disables hardware IMU sensor streaming.
    /// @param[in] type Sensor type (`Accelerometer` or `Gyroscope`).
    /// @param[in] enabled True to start streaming IMU events, false to power down the sensor.
    /// @return True if the sensor state was changed successfully, false otherwise.
    virtual bool setSensorEnabled(GamepadSensorType type, bool enabled) noexcept = 0;

    /// @brief Retrieves the latest 3D accelerometer reading in $m/s^2$.
    /// @return Const reference to the cached `SensorReading`.
    [[nodiscard]] virtual const SensorReading& getAccelerometerReading() const noexcept = 0;

    /// @brief Retrieves the latest 3D gyroscope angular velocity reading in $\text{rad}/s$.
    /// @return Const reference to the cached `SensorReading`.
    [[nodiscard]] virtual const SensorReading& getGyroscopeReading() const noexcept = 0;

    /// @brief Returns the number of active multi-touch finger contacts on this device.
    /// @return Active touch contact count in `[0, kMaxTouchContacts]`.
    [[nodiscard]] virtual gp::UInt32 getActiveTouchCount() const noexcept = 0;

    /// @brief Retrieves a touch contact slot by index in `[0, kMaxTouchContacts)`.
    /// @param[in] index Contact slot index.
    /// @return Pointer to the `TouchContactPoint`, or `nullptr` if inactive or out of range.
    [[nodiscard]] virtual const TouchContactPoint* getTouchContact(gp::UInt32 index) const noexcept = 0;

    /// @brief Retrieves the current 6-DoF stylus / pen state snapshot.
    /// @return Const reference to `PenStateSnapshot`.
    [[nodiscard]] virtual const PenStateSnapshot& getPenState() const noexcept = 0;

    /// @brief Ingests a decoded HAL `SensorEvent` packet.
    /// @param[in] ev The decoded sensor event packet.
    virtual void onSensorEvent(const event::SensorEvent& ev) noexcept = 0;

    /// @brief Ingests a decoded HAL `TouchEvent` packet.
    /// @param[in] ev The decoded touch event packet.
    virtual void onTouchEvent(const event::TouchEvent& ev) noexcept = 0;

    /// @brief Ingests a decoded HAL `PenEvent` packet.
    /// @param[in] ev The decoded pen event packet.
    virtual void onPenEvent(const event::PenEvent& ev) noexcept = 0;
};

}   // namespace gp::hal::input
