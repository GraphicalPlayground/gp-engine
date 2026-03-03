// Copyright (c) - Graphical Playground. All rights reserved.

#include "Container/ContainerFwd.hpp"
#include "Container/Core/Pair.hpp"
#include "Container/Sequential/Array.hpp"
#include "Container/Views/StringView.hpp"
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

    GP::TArray<int> intArray{ 1, 2, 3, 4, 5 };
    std::cout << "TArray size: " << intArray.Size() << ", capacity: " << intArray.Capacity() << std::endl;
    for (GP::SizeT i = 0; i < intArray.Size(); ++i)
    {
        std::cout << "Element " << i << ": " << intArray[i] << std::endl;
    }

    GP::TPair<int, bool> pair{ 42, true };
    std::cout << "TPair first: " << pair.first << ", second: " << std::boolalpha << pair.second << std::endl;

    GP::FStringView strView{ "Hello, FStringView!" };
    GP::FStringView strView2{ "Hello, FStringView!" };
    std::cout << "FStringView: " << strView.Data() << ", size: " << strView.Size() << std::endl;
    std::cout << "FStringView equality: " << std::boolalpha << (strView == strView2) << std::endl;

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
