// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"

namespace gp::platform
{

/// @brief Stateless C-style function pointer signature for platform fiber context entry points.
/// @note A fiber entry function must never return to the bottom of its call stack without switching back to a valid
///       caller or thread-anchor context.
/// @param[in] userData Opaque caller-managed context pointer supplied in `FiberContextDesc`.
using FiberContextEntryFunction = void (*)(void* userData);

/// @brief Minimum usable stack size in bytes enforced for any child fiber context (16 KiB).
inline constexpr gp::USize kMinFiberStackSize = 16u * 1024u;

/// @brief Default standard stack size in bytes for job system worker fibers (64 KiB, Naughty Dog GDC 2015 tier 1).
inline constexpr gp::USize kDefaultFiberStackSize = 64u * 1024u;

/// @brief Large stack size in bytes for deep recursive call trees or third-party middleware jobs (512 KiB, tier 2).
inline constexpr gp::USize kLargeFiberStackSize = 512u * 1024u;

/// @brief Maximum allowed virtual stack reservation size in bytes for a single fiber context (64 MiB).
inline constexpr gp::USize kMaxFiberStackSize = 64u * 1024u * 1024u;

/// @brief Mandatory byte alignment for ABI stack pointers across x86-64 (Win64 / SysV) and ARM64 (AAPCS64).
inline constexpr gp::USize kFiberStackAlignment = 16u;

}   // namespace gp::platform
