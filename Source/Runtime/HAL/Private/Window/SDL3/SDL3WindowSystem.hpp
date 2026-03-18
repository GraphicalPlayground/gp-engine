// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Window/BaseWindowSystem.hpp"

namespace GP
{

/// @brief SDL3 implementation of IWindowSystem.
class FSDL3WindowSystem final : public FBaseWindowSystem
{
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
    virtual void DispatchEvents() noexcept override;

    ///
    /// @section Utility
    ///

    GP_NODISCARD virtual FStringView GetBackendName() const noexcept override;
};

}   // namespace GP
