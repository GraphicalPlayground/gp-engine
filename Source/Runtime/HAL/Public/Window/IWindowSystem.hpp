// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Container/Views/ArrayView.hpp"
#include "Container/Views/StringView.hpp"
#include "CoreBuild.hpp"
#include "Display/IDisplay.hpp"
#include "Memory/SharedPtr.hpp"
#include "Memory/UniquePtr.hpp"
#include "Memory/WeakPtr.hpp"
#include "Window/IWindow.hpp"
#include "Window/WindowDesc.hpp"

namespace GP
{

/// @brief
class IWindowSystem
{
public:
    virtual ~IWindowSystem() = default;

public:
    ///
    /// @section Window Management
    ///

    GP_NODISCARD virtual TSharedPtr<IWindow> CreateWindow(const FWindowDesc& desc) noexcept = 0;
    GP_NODISCARD virtual TArrayView<const TSharedPtr<IWindow>> GetWindows() const noexcept = 0;
    GP_NODISCARD virtual TSharedPtr<IWindow> GetFocusedWindow() const noexcept = 0;
    GP_NODISCARD virtual TSharedPtr<IWindow> GetMainWindow() const noexcept = 0;
    GP_NODISCARD virtual SizeT GetWindowCount() const noexcept = 0;
    GP_NODISCARD virtual bool HasWindows() const noexcept = 0;
    GP_NODISCARD virtual TSharedPtr<IWindow> FindWindowByNativeHandle(void* nativeHandle) const noexcept = 0;

    ///
    /// @section Display Management
    ///

    GP_NODISCARD virtual TSharedPtr<IDisplay> GetPrimaryDisplay() const noexcept = 0;
    GP_NODISCARD virtual TArrayView<const TSharedPtr<IDisplay>> GetDisplays() const noexcept = 0;
    GP_NODISCARD virtual SizeT GetDisplayCount() const noexcept = 0;
    GP_NODISCARD virtual bool HasDisplays() const noexcept = 0;
    GP_NODISCARD virtual TSharedPtr<IDisplay> FindDisplayById(SizeT id) const noexcept = 0;
    GP_NODISCARD virtual TSharedPtr<IDisplay> FindDisplayByNativeHandle(void* nativeHandle) const noexcept = 0;
    GP_NODISCARD virtual TSharedPtr<IDisplay> FindDisplayForWindow(const TSharedPtr<IWindow>& window
    ) const noexcept = 0;
    virtual void RefreshDisplayList() noexcept = 0;

    ///
    /// @section Event Handling
    ///

    /// @brief Drains the OS event queue and fills the TEventQueue write buffers.
    ///
    /// @par Threading
    /// MUST be called on the platform thread (the thread that created the windows).
    /// On macOS this is mandatorily the main OS thread.
    /// On Win32 this is the thread that called SDL_Init / created the HWNDs.
    ///
    /// @par What it does
    /// Calls the platform pump (SDL_PollEvent, PeekMessage, wl_display_dispatch…),
    /// routes each raw event to the appropriate IWindow or system TEventQueue via
    /// Emit(). Does NOT dispatch to listeners, that is DispatchEvents()'s job.
    virtual void PumpEvents() noexcept = 0;

    /// @brief Swaps and drains the TEventQueue read buffers, broadcasting all
    ///        accumulated events through the TEvent priority system.
    ///
    /// @par Threading
    /// Called on the game/logic thread. Safe to call concurrently with PumpEvents()
    /// on the platform thread, the double buffer and spinlock handle the race.
    ///
    /// @par Dispatch order
    /// 1. System-level events (display topology, power), must precede window events.
    /// 2. Per-window events in window-registration order.
    virtual void DispatchEvents() noexcept = 0;

    /// @brief Convenience wrapper for single-threaded usage: pump then dispatch.
    ///
    /// Equivalent to PumpEvents() + DispatchEvents() called sequentially on the
    /// same thread. Use this until a dedicated game thread exists. Internally
    /// calls FlushImmediate() to avoid a pointless buffer flip.
    virtual void PollEvents() noexcept = 0;

    ///
    /// @section Utility
    ///

    GP_NODISCARD virtual FStringView GetBackendName() const noexcept = 0;

public:
    GP_NODISCARD static TUniquePtr<IWindowSystem> Create() noexcept;
};

}   // namespace GP
