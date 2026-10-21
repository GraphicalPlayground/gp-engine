// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "backend/GenericApplicationBackend.hpp"

namespace gp::hal::sdl3
{

/// @brief Represents the SDL3 application backend, responsible for initializing and managing the SDL3 library HAL.
class ApplicationBackend : public generic::ApplicationBackend
{
public:
    /// @brief Default destructor.
    ~ApplicationBackend() override = default;

public:
    /// @brief Initializes the application backend (e.g., initializing SDL, GLFW, or Win32 systems).
    /// @return true if initialization is successful, false otherwise.
    [[nodiscard]] virtual bool initialize() noexcept override;

    /// @brief Shuts down the application backend and frees OS-level resources.
    virtual void shutdown() noexcept override;

    /// @brief Creates a new native window with the specified description.
    /// @param desc The description of the window to create.
    /// @return A unique pointer to the created window.
    [[nodiscard]] virtual UniquePtr<IWindow> createWindow(const WindowDesc& desc) noexcept override;
};

}   // namespace gp::hal::sdl3
