// Copyright (c) - Graphical Playground. All rights reserved.

#include "Core.hpp"
#include "HAL.hpp"
#include <iostream>
#include <string>

#if GP_PLATFORM_WINDOWS
    #include <windows.h>
    #undef CreateWindow   // Avoid macro pollution from Windows headers
#endif

int main(int argc, char** argv)
{
    GP_UNUSED(argc);
    GP_UNUSED(argv);

    std::cout << "Hello, Graphical Playground!" << std::endl;

    auto windowing = GP::HAL::IWindowingSystem::Create();
    auto monitors = windowing->GetMonitors();
    for (const auto& monitor: monitors)
    {
        auto videoMode = monitor->GetCurrentVideoMode();
        std::cout << "Monitor: " << monitor->GetName().Data() << (monitor->IsPrimary() ? "(primary) " : " ")
                  << videoMode << std::endl;
    }

    auto window = windowing->CreateWindow({ .title = "GP Editor", .width = 1280, .height = 720 });
    if (!window)
    {
        std::cerr << "Failed to create window!" << std::endl;
        return 1;
    }

    window->OnCloseRequested.AddLambda(
        [&](const auto&)
        {
            std::cout << "Close requested!" << std::endl;
            window->Close();
        }
    );

    window->OnClosed.AddLambda([&](const auto&) { std::cout << "Window closed!" << std::endl; });
    window->OnFocusGained.AddLambda([&](const auto&) { std::cout << "Window focused!" << std::endl; });
    window->OnFocusLost.AddLambda([&](const auto&) { std::cout << "Window lost focus!" << std::endl; });
    window->OnHidden.AddLambda([&](const auto&) { std::cout << "Window hidden!" << std::endl; });
    window->OnShown.AddLambda([&](const auto&) { std::cout << "Window shown!" << std::endl; });
    window->OnMaximized.AddLambda([&](const auto&) { std::cout << "Window maximized!" << std::endl; });
    window->OnMinimized.AddLambda([&](const auto&) { std::cout << "Window minimized!" << std::endl; });
    window->OnRestored.AddLambda([&](const auto&) { std::cout << "Window restored!" << std::endl; });
    window->OnResized.AddLambda([&](const auto& event)
                                { std::cout << "Window resized: " << event.width << "x" << event.height << std::endl; }
    );
    window->OnModeChanged.AddLambda(
        [&](const auto& event) { std::cout << "Window mode changed: " << static_cast<int>(event.newMode) << std::endl; }
    );
    window->OnMoved.AddLambda([&](const auto& event)
                              { std::cout << "Window moved: " << event.x << "," << event.y << std::endl; });
    window->OnMonitorChanged.AddLambda(
        [&](const auto& event)
        { std::cout << "Window monitor changed: " << event.newMonitor->GetName().Data() << std::endl; }
    );
    window->OnMouseEntered.AddLambda([&](const auto&) { std::cout << "Mouse entered window!" << std::endl; });
    window->OnMouseLeft.AddLambda([&](const auto&) { std::cout << "Mouse left window!" << std::endl; });
    window->OnOccluded.AddLambda([&](const auto&) { std::cout << "Window occluded!" << std::endl; });
    window->OnRevealed.AddLambda([&](const auto&) { std::cout << "Window revealed!" << std::endl; });
    window->OnStyleChanged.AddLambda(
        [&](const auto& event)
        { std::cout << "Window style changed: " << static_cast<int>(event.newStyle) << std::endl; }
    );

    while (window->IsOpen()) { windowing->PollEvents(); }

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
