// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "platforms/unix/UnixPlatform.hpp"   // IWYU pragma: export

#define GP_PLATFORM_SUPPORTS_BORDERLESS_WINDOW          GP_TRUE

/// @note Undefine the 'linux' macro to avoid conflicts with the 'linux' namespace in gp::platform::linux.
#ifdef linux
    #undef linux
#endif
