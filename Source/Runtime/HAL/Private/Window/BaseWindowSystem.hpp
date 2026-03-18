// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Container/Sequential/Array.hpp"
#include "Memory/SharedPtr.hpp"
#include "Window/IWindowSystem.hpp"

namespace GP
{

/// @brief Base implementation of IWindowSystem. Platform-specific window systems should derive from this class.
class FBaseWindowSystem : public IWindowSystem
{
protected:
    TArray<TSharedPtr<IWindow>> m_windows;           //<! Storage for all created windows.
    TArray<TSharedPtr<const IDisplay>> m_displays;   //<! Storage for all detected displays.

public:
    virtual ~FBaseWindowSystem() = default;

public:
    ///
    /// @section Window Management
    ///

    GP_NODISCARD virtual TSharedPtr<IWindow> CreateWindow(const FWindowDesc& desc) noexcept override;
    GP_NODISCARD virtual TArrayView<const TSharedPtr<IWindow>> GetWindows() const noexcept override;
    GP_NODISCARD virtual TSharedPtr<IWindow> GetFocusedWindow() const noexcept override;
    GP_NODISCARD virtual TSharedPtr<IWindow> GetMainWindow() const noexcept override;
    GP_NODISCARD virtual SizeT GetWindowCount() const noexcept override;
    GP_NODISCARD virtual bool HasWindows() const noexcept override;
    GP_NODISCARD virtual TSharedPtr<IWindow> FindWindowByNativeHandle(void* nativeHandle) const noexcept override;

    ///
    /// @section Display Management
    ///

    GP_NODISCARD virtual TSharedPtr<const IDisplay> GetPrimaryDisplay() const noexcept override;
    GP_NODISCARD virtual TArrayView<const TSharedPtr<const IDisplay>> GetDisplays() const noexcept override;
    GP_NODISCARD virtual SizeT GetDisplayCount() const noexcept override;
    GP_NODISCARD virtual bool HasDisplays() const noexcept override;
    GP_NODISCARD virtual TSharedPtr<const IDisplay> FindDisplayById(SizeT id) const noexcept override;
    GP_NODISCARD virtual TSharedPtr<const IDisplay>
        FindDisplayByNativeHandle(void* nativeHandle) const noexcept override;
    GP_NODISCARD virtual TSharedPtr<const IDisplay>
        FindDisplayForWindow(const TSharedPtr<IWindow>& window) const noexcept override;
    virtual void RefreshDisplayList() noexcept override;

    ///
    /// @section Event Handling
    ///

    virtual void PollEvents() noexcept override;
    virtual void DispatchEvents() noexcept override;

    ///
    /// @section Utility
    ///

    GP_NODISCARD virtual FStringView GetBackendName() const noexcept override;
};

}   // namespace GP
