// File: source/runtime/core/public/platforms/macos/system/MacOSThread.hpp
// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "platforms/apple/system/AppleThread.hpp"

namespace gp::platform::macos
{

/// @brief macOS desktop hardware thread implementation (inherits Darwin/Mach primitives from apple::Thread).
class GP_CORE_API Thread : public gp::platform::apple::Thread
{
public:
    /// @brief Constructs an empty, non-joinable macOS thread object.
    [[nodiscard]] constexpr Thread() noexcept = default;

    /// @brief Destroys the macOS thread object.
    ~Thread() noexcept = default;

    /// @brief Non-copyable.
    Thread(const Thread&) = delete;
    Thread& operator=(const Thread&) = delete;

    /// @brief Move constructor.
    /// @param[in,out] other Source thread object to move from.
    [[nodiscard]] Thread(Thread&& other) noexcept = default;

    /// @brief Move assignment operator.
    /// @param[in,out] other Source thread object to move from.
    /// @return Reference to this thread instance.
    Thread& operator=(Thread&& other) noexcept = default;
};

}   // namespace gp::platform::macos
