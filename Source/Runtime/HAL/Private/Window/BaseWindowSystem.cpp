// Copyright (c) - Graphical Playground. All rights reserved.

#include "Window/BaseWindowSystem.hpp"

namespace GP
{

GP_NODISCARD TSharedPtr<IWindow> FBaseWindowSystem::CreateWindow(const FWindowDesc& desc) noexcept
{
    GP_UNUSED(desc);
    return nullptr;
}

GP_NODISCARD TArrayView<const TSharedPtr<IWindow>> FBaseWindowSystem::GetWindows() const noexcept
{
    return m_windows.AsView();
}

GP_NODISCARD TSharedPtr<IWindow> FBaseWindowSystem::GetFocusedWindow() const noexcept
{
    for (const auto& window: m_windows)
    {
        if (window->IsValid() && window->IsFocused()) { return window; }
    }
    return nullptr;
}

GP_NODISCARD TSharedPtr<IWindow> FBaseWindowSystem::GetMainWindow() const noexcept
{
    // By default, we consider the first created window as the main window.
    for (const auto& window: m_windows)
    {
        if (window->IsValid()) { return window; }
    }
    return nullptr;
}

GP_NODISCARD SizeT FBaseWindowSystem::GetWindowCount() const noexcept { return m_windows.Size(); }

GP_NODISCARD bool FBaseWindowSystem::HasWindows() const noexcept { return m_windows.Size() != 0; }

GP_NODISCARD TSharedPtr<IWindow> FBaseWindowSystem::FindWindowByNativeHandle(void* nativeHandle) const noexcept
{
    for (const auto& window: m_windows)
    {
        if (window->IsValid() && window->GetNativeHandle() == nativeHandle) { return window; }
    }
    return nullptr;
}

GP_NODISCARD TSharedPtr<IDisplay> FBaseWindowSystem::GetPrimaryDisplay() const noexcept
{
    // TODO: Implement this properly once we have a real display system in place.
    return nullptr;
}

GP_NODISCARD TArrayView<const TSharedPtr<IDisplay>> FBaseWindowSystem::GetDisplays() const noexcept
{
    return m_displays.AsView();
}

GP_NODISCARD SizeT FBaseWindowSystem::GetDisplayCount() const noexcept { return m_displays.Size(); }

GP_NODISCARD bool FBaseWindowSystem::HasDisplays() const noexcept { return m_displays.Size() != 0; }

GP_NODISCARD TSharedPtr<IDisplay> FBaseWindowSystem::FindDisplayById(SizeT id) const noexcept
{
    // TODO: Implement this properly once we have a real display system in place.
    GP_UNUSED(id);
    return nullptr;
}

GP_NODISCARD TSharedPtr<IDisplay> FBaseWindowSystem::FindDisplayByNativeHandle(void* nativeHandle) const noexcept
{
    // TODO: Implement this properly once we have a real display system in place.
    GP_UNUSED(nativeHandle);
    return nullptr;
}

GP_NODISCARD TSharedPtr<IDisplay> FBaseWindowSystem::FindDisplayForWindow(const TSharedPtr<IWindow>& window
) const noexcept
{
    // TODO: Implement this properly once we have a real display system in place.
    GP_UNUSED(window);
    for (const auto& display: m_displays)
    {
        if (/* display->IsValid() && */ window->GetCurrentDisplay() == display.Get()) { return display; }
    }
    return nullptr;
}

void FBaseWindowSystem::RefreshDisplayList() noexcept
{
    // Does nothing by default.
}

GP_NODISCARD FStringView FBaseWindowSystem::GetBackendName() const noexcept { return "None"; }

void FBaseWindowSystem::PollEvents() noexcept
{
    // Does nothing by default.
}

void FBaseWindowSystem::DispatchEvents() noexcept
{
    // Does nothing by default.
}

}   // namespace GP
