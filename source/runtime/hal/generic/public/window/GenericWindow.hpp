// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "window/IWindow.hpp"

namespace gp::hal::generic
{

/// @brief
class Window : public gp::hal::IWindow
{
protected:
    WindowDesc m_desc{};
    NativeWindowHandle m_nativeHandle{ NativeWindowBackend::Unknown };
};

}   // namespace gp::hal::generic
