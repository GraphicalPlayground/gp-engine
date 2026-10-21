// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "memory/pointers/UniquePtr.hpp"
#include "window/IWindow.hpp"

namespace gp::hal
{

/// @brief Interface for application backends that manage the initialization and shutdown of platform-specific systems
/// (e.g., SDL, GLFW, Win32) and provide resources creation functionality.
class IApplicationBackend
{
public:
    /// @brief Default destructor.
    virtual ~IApplicationBackend() = default;

public:
    /// @brief Initializes the application backend (e.g., initializing SDL, GLFW, or Win32 systems).
    /// @return true if initialization is successful, false otherwise.
    [[nodiscard]] virtual bool initialize() noexcept = 0;

    /// @brief Shuts down the application backend and frees OS-level resources.
    virtual void shutdown() noexcept = 0;

    /// @brief Creates a new native window with the specified description.
    /// @param desc The description of the window to create.
    /// @return A unique pointer to the created window.
    [[nodiscard]] virtual UniquePtr<IWindow> createWindow(const WindowDesc& desc) noexcept = 0;
};

}   // namespace gp::hal
