// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "window/WindowTypes.hpp"
#include "window/CursorEnums.hpp"
#include "window/NativeWindow.hpp"
#include "window/WindowDesc.hpp"
#include "window/WindowEnums.hpp"
#include <gtest/gtest.h>

namespace gp::hal::tests
{

TEST(WindowDescTest, DefaultInitialization)
{
    const WindowDesc desc;

    EXPECT_EQ(desc.title, "Graphical Playground");
    EXPECT_EQ(desc.size.width, 1280u);
    EXPECT_EQ(desc.size.height, 720u);
    EXPECT_EQ(desc.minimumSize.width, 0u);
    EXPECT_EQ(desc.minimumSize.height, 0u);
    EXPECT_EQ(desc.maximumSize.width, 0u);
    EXPECT_EQ(desc.maximumSize.height, 0u);
    EXPECT_EQ(desc.mode, WindowMode::Windowed);
    EXPECT_TRUE(desc.startFocused);
    EXPECT_TRUE(desc.startVisible);
    EXPECT_EQ(desc.style, WindowStyle::Default);
    EXPECT_FALSE(desc.useHeadless);
    EXPECT_FLOAT_EQ(desc.opacity, 1.0f);
    EXPECT_EQ(desc.cursorMode, CursorMode::Normal);
    EXPECT_EQ(desc.cursorShape, CursorShape::Arrow);
}

TEST(NativeWindowHandleTest, ConstructionAndBackendTags)
{
    const NativeWindowHandle unknownHandle;
    EXPECT_EQ(unknownHandle.backend, NativeWindowBackend::Unknown);
    EXPECT_EQ(unknownHandle._storage[0], nullptr);

    NativeWindowHandle winHandle(NativeWindowBackend::Win32);
    EXPECT_EQ(winHandle.backend, NativeWindowBackend::Win32);

    void* const dummyPtr = reinterpret_cast<void*>(0x12345678);
    winHandle.win32.hwnd = dummyPtr;
    EXPECT_EQ(winHandle.win32.hwnd, dummyPtr);
    EXPECT_EQ(winHandle._storage[0], dummyPtr);
}

TEST(WindowEnumsTest, WindowModes)
{
    EXPECT_EQ(static_cast<gp::UInt8>(WindowMode::Windowed), 0u);
    EXPECT_EQ(static_cast<gp::UInt8>(WindowMode::Borderless), 1u);
    EXPECT_EQ(static_cast<gp::UInt8>(WindowMode::Fullscreen), 2u);
    EXPECT_EQ(static_cast<gp::UInt8>(WindowMode::FullscreenDesktop), 3u);
}

TEST(WindowEnumsTest, CursorEnums)
{
    EXPECT_EQ(static_cast<gp::UInt8>(CursorMode::Normal), 0u);
    EXPECT_EQ(static_cast<gp::UInt8>(CursorMode::Hidden), 1u);
    EXPECT_EQ(static_cast<gp::UInt8>(CursorMode::Locked), 2u);
    EXPECT_EQ(static_cast<gp::UInt8>(CursorMode::Confined), 3u);

    EXPECT_EQ(static_cast<gp::UInt8>(CursorShape::Default), 0u);
    EXPECT_EQ(static_cast<gp::UInt8>(CursorShape::Arrow), 1u);
}

}   // namespace gp::hal::tests
