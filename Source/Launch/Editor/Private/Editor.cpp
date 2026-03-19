// Copyright (c) - Graphical Playground. All rights reserved.

#include "CoreBuild.hpp"
#include "CoreTypes.hpp"
#include "Window/IWindow.hpp"
#include "Window/IWindowSystem.hpp"
#include "Window/WindowDesc.hpp"
#if GP_PLATFORM_WINDOWS
    #include <stdlib.h>
    #include <windows.h>
    #undef CreateWindow
#endif
#include <iostream>

int main(int argc, char* argv[])
{
    GP_UNUSED(argc);
    GP_UNUSED(argv);

    std::cout << "Hello, Graphical Playground Editor!" << std::endl;

    auto windowSystem = GP::IWindowSystem::Create();
    if (!windowSystem)
    {
        std::cerr << "Failed to create window system!" << std::endl;
        return 1;
    }

    auto window = windowSystem->CreateWindow({
      .title = "Graphical Playground Editor", .size = { 1280, 720 }
    });
    if (!window)
    {
        std::cerr << "Failed to create window!" << std::endl;
        return 1;
    }

    auto displays = windowSystem->GetDisplays();
    std::cout << "Detected " << displays.Size() << " display(s):" << std::endl;
    for (const auto& display: displays)
    {
        std::cout << display->GetName().Data() << " - " << display->GetCurrentMode() << std::endl;
    }

    // window->OnClosed      += [](const auto&) { Platform::RequestExit(); };
    // window->OnFocusLost   += [](const auto&) { Audio::MuteAll(); };
    // window->OnFocusGained += [](const auto&) { Audio::Restore(); };

    // window->OnCloseRequested.Add([](const auto&) { return true; }, GP::EEventPriority::Critical);

    while (window->IsOpen())
    {
        windowSystem->PollEvents();

        // Do something here

        windowSystem->DispatchEvents();

        // Sleep or wait for vsync here to avoid busy-waiting
    }

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
