// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Window/IWindowSystem.hpp"

namespace GP
{

/// @brief Base implementation of IWindowSystem. Platform-specific window systems should derive from this class.
class FBaseWindowSystem : public IWindowSystem
{
public:
    virtual ~FBaseWindowSystem() = default;

public:
    virtual TSharedPtr<IWindow> CreateWindow(const FWindowDesc& desc) noexcept override;
    virtual void PollEvents() noexcept override;
    virtual void DispatchEvents() noexcept override;
};

}   // namespace GP
