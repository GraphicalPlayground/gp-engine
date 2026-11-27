// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "allocators/AllocatorForward.hpp"        // IWYU pragma: export
#include "commands/CommandForward.hpp"            // IWYU pragma: export
#include "core/ResourceForward.hpp"               // IWYU pragma: export
#include "CoreMinimal.hpp"                        // IWYU pragma: keep
#include "device/DeviceForward.hpp"               // IWYU pragma: export
#include "diagnostics/DiagnosticsForward.hpp"     // IWYU pragma: export
#include "maths/color/PixelFormatForward.hpp"     // IWYU pragma: export
#include "pipeline/PipelineForward.hpp"           // IWYU pragma: export
#include "presentation/PresentationForward.hpp"   // IWYU pragma: export
#include "profiling/ProfilingForward.hpp"         // IWYU pragma: export
#include "shaders/ShaderForward.hpp"              // IWYU pragma: export
#include "sync/SyncForward.hpp"                   // IWYU pragma: export
#include "validation/ValidationForward.hpp"       // IWYU pragma: export
#include "views/ViewForward.hpp"                  // IWYU pragma: export

namespace gp::rhi
{

using gp::math::PixelFormat;
using gp::math::PixelFormatChannelFlags;
using gp::math::PixelFormatComponentType;
using gp::math::PixelFormatCompression;
using gp::math::PixelFormatInfo;

/// @brief Runtime hardware usage capabilities supported by an active RHI device for a given `PixelFormat`.
enum class PixelFormatCapabilities : gp::UInt32;

/// @brief Compact 32-bit bitmask identifying active GPU adapter nodes.
struct GPUMask;

}   // namespace gp::rhi
