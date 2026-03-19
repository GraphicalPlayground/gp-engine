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

    virtual void PollEvents() noexcept = 0;

    ///
    /// @section Utility
    ///

    GP_NODISCARD virtual FStringView GetBackendName() const noexcept = 0;

public:
    GP_NODISCARD static TUniquePtr<IWindowSystem> Create() noexcept;
};

}   // namespace GP
