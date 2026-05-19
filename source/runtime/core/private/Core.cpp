// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "platforms/base/Platform.hpp"   // IWYU pragma: keep

int somethingToCompile()
{
#if GP_PLATFORM_WINDOWS
    return 42;
#else
    return 0;
#endif
}
