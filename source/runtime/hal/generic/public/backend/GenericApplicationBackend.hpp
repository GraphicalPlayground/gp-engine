// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "backend/IApplicationBackend.hpp"

namespace gp::hal::generic
{

/// @brief Generic application backend that serves as a base for platform-specific backends (e.g., SDL, GLFW, Win32).
class ApplicationBackend : public IApplicationBackend
{
public:
    /// @brief Default destructor.
    ~ApplicationBackend() override = default;

public:
    /// @brief Initializes the application backend (e.g., initializing SDL, GLFW, or Win32 systems).
    /// @return true if initialization is successful, false otherwise.
    [[nodiscard]] virtual bool initialize() noexcept override
    {
        return true;
    }

    /// @brief Shuts down the application backend and frees OS-level resources.
    virtual void shutdown() noexcept override
    {}

    /// @brief Creates a new native window with the specified description.
    /// @details This implementation does not create a window and returns an empty unique pointer.
    ///          It serves as a placeholder for backends that do not support window creation.
    /// @param desc The description of the window to create.
    /// @return A unique pointer to the created window.
    [[nodiscard]] virtual UniquePtr<IWindow> createWindow([[maybe_unused]] const WindowDesc& desc) noexcept override
    {
        return {};
    }
};

}   // namespace gp::hal::generic
