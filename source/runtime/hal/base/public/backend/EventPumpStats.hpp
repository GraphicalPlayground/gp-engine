// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"

namespace gp::hal
{

/// @brief Per-frame telemetry counters output by `IApplicationBackend::pumpEvents()`.
struct EventPumpStats
{
    /// @brief Total number of raw OS events polled from the underlying platform API.
    gp::UInt32 polledCount = 0u;

    /// @brief Number of engine TLV packets successfully committed to the SPSC byte ring buffer.
    gp::UInt32 pushedCount = 0u;

    /// @brief Number of high-frequency continuous events shed due to backpressure reserve protection.
    gp::UInt32 shedContinuousCount = 0u;

    /// @brief Number of critical events dropped because the SPSC ring buffer was 100% exhausted.
    gp::UInt32 droppedCriticalCount = 0u;

    /// @brief Total number of bytes written to the SPSC byte ring buffer during the pump cycle.
    gp::USize bytesWritten = 0u;
};

}   // namespace gp::hal
