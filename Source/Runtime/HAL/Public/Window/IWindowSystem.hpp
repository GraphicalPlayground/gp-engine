// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include "Memory/SharedPtr.hpp"
#include "Memory/UniquePtr.hpp"
#include "Memory/WeakPtr.hpp"
#include "Window/IWindow.hpp"
#include "Window/WindowDesc.hpp"

namespace GP
{

/// @brief
class IWindowSystem
{
public:
    virtual ~IWindowSystem() = default;

public:
    virtual TSharedPtr<IWindow> CreateWindow(const FWindowDesc& desc) noexcept = 0;
    virtual void PollEvents() noexcept = 0;
    virtual void DispatchEvents() noexcept = 0;

public:
    static TUniquePtr<IWindowSystem> Create() noexcept;
};

}   // namespace GP
