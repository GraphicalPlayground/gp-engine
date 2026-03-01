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
    for (size_t i = 0; i < monitors.Size(); i++)
    {
        const auto& monitor = monitors[i];
        auto videoMode = monitor->GetCurrentVideoMode();
        std::cout << "Monitor [" << i << "]:" << std::endl;
        std::cout << "  Name: " << monitor->GetName().Data() << std::endl;
        std::cout << "  UniqueID: " << monitor->GetUniqueID().Data() << std::endl;
        std::cout << "  Manufacturer: " << monitor->GetManufacturerName().Data() << std::endl;
        std::cout << "  Model: " << monitor->GetModelName().Data() << std::endl;
        std::cout << "  SerialNumber: " << monitor->GetSerialNumber().Data() << std::endl;
        std::cout << "  VideoMode: " << videoMode << std::endl;
        std::cout << "  IsPrimary: " << (monitor->IsPrimary() ? "true" : "false") << std::endl;
        std::cout << "  IsConnected: " << (monitor->IsConnected() ? "true" : "false") << std::endl;
        std::cout << "  IsSleeping: " << (monitor->IsSleeping() ? "true" : "false") << std::endl;
        std::cout << "  IsVirtual: " << (monitor->IsVirtual() ? "true" : "false") << std::endl;
        std::cout << "  IsBuiltIn: " << (monitor->IsBuiltIn() ? "true" : "false") << std::endl;
        std::cout << "  IsHighDPI: " << (monitor->IsHighDPI() ? "true" : "false") << std::endl;
        std::cout << "  ConnectorType: " << monitor->GetConnectorType() << std::endl;
        std::cout << "  PanelType: " << monitor->GetPanelType() << std::endl;
        std::cout << "  RefreshRate: " << monitor->GetRefreshRate() << " Hz" << std::endl;
        std::cout << "  MinRefreshRate: " << monitor->GetMinRefreshRate() << " Hz" << std::endl;
        std::cout << "  MaxRefreshRate: " << monitor->GetMaxRefreshRate() << " Hz" << std::endl;
        std::cout << "  PhysicalSize: " << monitor->GetPhysicalSize().widthMM << "x"
                  << monitor->GetPhysicalSize().heightMM << " mm" << std::endl;
        std::cout << "  DPI: " << monitor->GetDPIX() << "x" << monitor->GetDPIY() << " dpi" << std::endl;
        std::cout << "  VRR Type: " << monitor->GetVRRType() << std::endl;
        std::cout << "  VRR Capable: " << (monitor->SupportsVRR() ? "true" : "false") << std::endl;
        std::cout << "  Color Gamut: " << monitor->GetColorGamut() << std::endl;
        std::cout << "  HDR Format: " << monitor->GetHDRFormat() << std::endl;
        std::cout << "  HDR Capable: " << (monitor->SupportsHDR() ? "true" : "false") << std::endl;
        std::cout << "  HDR Enabled: " << (monitor->IsHDREnabled() ? "true" : "false") << std::endl;
        std::cout << "  Max Luminance: " << monitor->GetMaxLuminance() << " nits" << std::endl;
        std::cout << "  Min Luminance: " << monitor->GetMinLuminance() << " nits" << std::endl;
        std::cout << "  Brightness: " << monitor->GetBrightness() << std::endl;
        std::cout << "  Orientation: " << monitor->GetOrientation() << std::endl;
        std::cout << "  Is Portrait: " << (monitor->IsPortrait() ? "true" : "false") << std::endl;
        std::cout << "  Has TouchScreen: " << (monitor->HasTouchScreen() ? "true" : "false") << std::endl;
        std::cout << "  Has BuiltIn Speakers: " << (monitor->HasBuiltInSpeakers() ? "true" : "false") << std::endl;
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
