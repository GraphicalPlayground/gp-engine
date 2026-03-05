// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Window/BaseWindowSystem.hpp"

namespace GP
{

/// @brief SDL3 implementation of IWindowSystem.
class FSDL3WindowSystem final : public FBaseWindowSystem
{
public:
    virtual TSharedPtr<IWindow> CreateWindow(const FWindowDesc& desc) noexcept override;
    virtual void PollEvents() noexcept override;
    virtual void DispatchEvents() noexcept override;
};

}   // namespace GP
