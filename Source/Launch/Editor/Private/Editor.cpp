// Copyright (c) - Graphical Playground. All rights reserved.

#include "CoreBuild.hpp"
#include "CoreTypes.hpp"
#include "Templates/Templates.hpp"
#if GP_PLATFORM_WINDOWS
    #include <stdlib.h>
    #include <windows.h>
#endif
#include <iostream>

template <GP::CFloatingPoint T>
struct TestStructure
{
    T value;
};

int main(int argc, char* argv[])
{
    GP_UNUSED(argc);
    GP_UNUSED(argv);

    std::cout << "Hello, Graphical Playground Editor!" << std::endl;

    TestStructure test{ 3.14f };
    std::cout << "TestStructure value: " << test.value << std::endl;

    return 0;
}

#if GP_PLATFORM_WINDOWS

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
    GP_UNUSED(hInstance);
    GP_UNUSED(hPrevInstance);
    GP_UNUSED(lpCmdLine);
    GP_UNUSED(nShowCmd);

    return main(__argc, __argv);
}

#endif
