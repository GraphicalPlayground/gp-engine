// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Templates/Events/EventQueue.hpp"
#include "Window/BaseWindowSystem.hpp"

namespace GP
{

/// @brief SDL3 implementation of IWindowSystem.
class FSDL3WindowSystem final : public FBaseWindowSystem
{
private:
    FEventQueue m_eventQueue;   //<! Queue for pending system events.

public:
    ///
    /// @section Construction & Destruction
    ///

    FSDL3WindowSystem() noexcept;
    ~FSDL3WindowSystem() noexcept override;

public:
    ///
    /// @section Window Management
    ///

    GP_NODISCARD virtual TSharedPtr<IWindow> CreateWindow(const FWindowDesc& desc) noexcept override;

    ///
    /// @section Display Management
    ///

    virtual void RefreshDisplayList() noexcept override;

    ///
    /// @section Event Handling
    ///

    virtual void PollEvents() noexcept override;

    ///
    /// @section Utility
    ///

    GP_NODISCARD virtual FStringView GetBackendName() const noexcept override;
};

}   // namespace GP
