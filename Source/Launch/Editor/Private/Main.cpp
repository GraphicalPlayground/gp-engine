// Copyright (c) - Mallory SCOTTON. All rights reserved.

#include "Core.hpp"
#include <iostream>
#include <string>

#if GP_PLATFORM_WINDOWS
    #include <windows.h>
#endif

int main(int argc, char** argv)
{
    GP_UNUSED(argc);
    GP_UNUSED(argv);

    std::cout << "Hello, Graphical Playground!" << std::endl;
    return 0;
}

#if GP_PLATFORM_WINDOWS

// Entry point for Windows applications
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
    GP_UNUSED(hInstance);
    GP_UNUSED(hPrevInstance);
    GP_UNUSED(lpCmdLine);
    GP_UNUSED(nShowCmd);

    return main(__argc, __argv);
}

#endif
