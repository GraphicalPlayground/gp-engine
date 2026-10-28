// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "display/GenericDisplay.hpp"
#include "SDL3HAL.hpp"

namespace gp::hal::sdl3
{

/// @brief Concrete SDL3 implementation of `gp::hal::IDisplay`.
/// @details Queries SDL3 display properties, desktop bounds, usable work areas, HDR white levels,
///          and fullscreen mode tables, caching them into `generic::Display` for lock-free Game Thread reads.
class GP_HAL_SDL3_API Display final : public gp::hal::generic::Display
{
public:
    /// @brief Default-constructs an uninitialized SDL3 display slot (zero allocations).
    [[nodiscard]] constexpr Display() noexcept = default;

    /// @brief Constructs an SDL3 display from a pre-populated `DisplayDesc`.
    /// @param[in] desc Initial display configuration descriptor.
    [[nodiscard]] explicit constexpr Display(const DisplayDesc& desc) noexcept
        : gp::hal::generic::Display(desc)
    {}

    /// @brief Constructs and immediately queries all hardware metrics for the specified `SDL_DisplayID`.
    /// @param[in] sdlDisplayId Native 32-bit SDL3 display identifier.
    /// @param[in] isPrimaryDisplay True if this display matches `SDL_GetPrimaryDisplay()`.
    explicit Display(DisplayId sdlDisplayId, bool isPrimaryDisplay = false) noexcept;

    /// @brief Non-copyable.
    Display(const Display&) = delete;
    Display& operator=(const Display&) = delete;

    /// @brief Move constructor delegating to `generic::Display`'s StringView-safe move constructor.
    [[nodiscard]] constexpr Display(Display&& other) noexcept = default;

    /// @brief Move assignment operator delegating to `generic::Display`'s StringView-safe move assignment.
    constexpr Display& operator=(Display&& other) noexcept = default;

    /// @brief Destructor.
    ~Display() noexcept override = default;

public:
    /// @brief Expose `generic::Display::initialize(const DisplayDesc&)` alongside the SDL ID overload.
    using gp::hal::generic::Display::initialize;

    /// @brief Initializes this display slot with an SDL3 display ID and queries all hardware metrics.
    /// @param[in] sdlDisplayId Native 32-bit SDL3 display identifier.
    /// @param[in] isPrimaryDisplay True if this display is the OS primary monitor.
    void initialize(DisplayId sdlDisplayId, bool isPrimaryDisplay = false) noexcept;

    /// @brief Re-queries SDL3 for current bounds, work area, orientation, DPI scale, HDR metadata, and modes.
    /// @note Must only be invoked on the main OS thread.
    void refresh() noexcept override;
};

}   // namespace gp::hal::sdl3
