// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "platforms/base/system/FiberContextTypes.hpp"

namespace gp::thread
{

/// @brief 32-bit identifier for an engine `Fiber` within a scheduler pool or diagnostic trace.
using FiberId = gp::UInt32;

/// @brief Stateless C-style function pointer signature for cooperative fiber tasks.
/// @param[in] userData Opaque caller-managed task payload pointer.
using FiberEntryFunction = void (*)(void* userData);

/// @brief Sentinel identifier indicating an uninitialized or invalid `Fiber`.
inline constexpr FiberId kInvalidFiberId = 0xFFFFFFFFu;

/// @brief Default identifier assigned to an OS hardware thread converted into a root thread-anchor `Fiber`.
inline constexpr FiberId kThreadAnchorFiberId = 0xFFFFFFFEu;

/// @brief Byte size of a `FiberStackTier::Standard64KiB` fiber stack (64 KiB).
inline constexpr gp::USize kStandardFiberStackBytes = gp::platform::kDefaultFiberStackSize;

/// @brief Byte size of a `FiberStackTier::Large512KiB` fiber stack (512 KiB).
inline constexpr gp::USize kLargeFiberStackBytes = gp::platform::kLargeFiberStackSize;

}   // namespace gp::thread
