// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

namespace gp::hal
{

/// @section Hardware information

class IHardwareInfo;
struct OSInfo;
struct PowerStateInfo;
struct MemoryInfo;
struct DiskInfo;
struct CPUInfo;
struct GPUInfo;
struct SIMDInfo;

/// @section Windowing and display

class IWindow;
class Display;
class Monitor;

struct VideoMode;
struct WindowDesc;

/// @section Input devices

class Keyboard;
class Mouse;
class Gamepad;
class TouchDevice;

}   // namespace gp::hal
