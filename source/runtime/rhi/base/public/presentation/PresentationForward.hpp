// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "memory/MemoryForward.hpp"

namespace gp::rhi::presentation
{

/// @brief Packed color gamut and electro-optical transfer function (EOTF) for swapchain output.
enum class ColorSpaceAndEOTF : gp::UInt8;

/// @brief Presentation parameters controlling swapchain present and vertical sync behavior.
struct PresentArgs;

/// @brief Extended presentation parameters carrying frame counter and input latency timestamps.
struct PresentArgsWithStats;

/// @brief Hook allowing custom presentation backends (e.g., XR runtimes) to override swapchain presentation.
class CustomPresent;

/// @brief Swapchain and backbuffer presentation surface bound to a native OS window.
class Viewport;

/// @brief Intrusive smart pointer handle to a `CustomPresent`.
using CustomPresentRef = gp::RefCountedPtr<CustomPresent, gp::DefaultRefCountPolicy<CustomPresent>>;

/// @brief Intrusive smart pointer handle to a `Viewport`.
using ViewportRef = gp::RefCountedPtr<Viewport, gp::DefaultRefCountPolicy<Viewport>>;

}   // namespace gp::rhi::presentation
