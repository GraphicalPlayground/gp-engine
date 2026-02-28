// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Container/Array.hpp"
#include "HALBuild.hpp"

namespace GP::HAL
{

/// \brief A single entry in the monitor's gamma ramp lookup table.
/// Red, green, and blue values are normalized to [0.0, 1.0]; the underlying driver maps them to the hardware's
/// native precision (typically 8-bit or 10-bit).
struct FGammaRampEntry
{
    Float32 red{ 1.0f };     //<! Red channel multiplier in [0.0, 1.0].
    Float32 green{ 1.0f };   //<! Green channel multiplier in [0.0, 1.0].
    Float32 blue{ 1.0f };    //<! Blue channel multiplier in [0.0, 1.0].
};

/// \brief Represents a complete gamma ramp for a monitor.
/// The ramp is a lookup table indexed by input luminance; each entry remaps that luminance to the desired output.
/// Standard ramp size is 256 entries (8-bit input). Some platforms support 1024 or 4096 entries for higher
/// precision; query IMonitor::GetGammaRampSize() before reading or writing.
struct FGammaRamp
{
    Container::TArray<FGammaRampEntry> entries;   //<! Per-entry output mapping.
};

}   // namespace GP::HAL
