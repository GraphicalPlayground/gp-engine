// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "CoreMinimal.hpp"
#include "Launch.hpp"
#include <windows.h>

int WINAPI WinMain(
    GP_MAYBE_UNUSED HINSTANCE hInstance,
    GP_MAYBE_UNUSED HINSTANCE hPrevInstance,
    GP_MAYBE_UNUSED LPSTR lpCmdLine,
    GP_MAYBE_UNUSED int nCmdShow
)
{
    return gp::launch(__argc, __argv);
}
