// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "modules/ModuleForward.hpp"         // IWYU pragma: export
#include "templates/Enums.hpp"

namespace gp::modules
{

/// @brief Explicit outcome status codes returned by `ModuleManager::loadModule()` ([ADR-0061]).
enum class ModuleLoadResult : gp::UInt8
{
    /// @brief The module was loaded (or was already loaded) and its `IModule` instance is active.
    Success = 0u,

    /// @brief The requested module name was empty or normalized to `"None"`.
    InvalidName = 1u,

    /// @brief The module was already loaded prior to this call (non-fatal informational code).
    AlreadyLoaded = 2u,

    /// @brief Neither a shared library on disk nor a static registrant matched the module name.
    NotFound = 3u,

    /// @brief The OS dynamic loader (`LoadLibraryW` / `dlopen`) failed to map the shared library image.
    LibraryLoadFailed = 4u,

    /// @brief The loaded shared library did not export `gpInitializeModule` or `gpDestroyModule`.
    EntryPointMissing = 5u,

    /// @brief The module's ABI magic, ABI version, or build-configuration flags did not match the host process.
    AbiVersionMismatch = 6u,

    /// @brief The module factory or `gpInitializeModule` entry point returned a null `IModule*` instance.
    InstantiationFailed = 7u,

    /// @brief The module instance's `onStartupModule()` hook returned `false`.
    StartupFailed = 8u,

    /// @brief A recursive `loadModule()` call on the same thread targeted a module already in `ModuleState::Loading`.
    CircularDependency = 9u,

    /// @brief The `ModuleManager` is currently executing `unloadAllModules()` or has exhausted its slot capacity.
    ShuttingDown = 10u
};

/// @brief Explicit outcome status codes returned by `ModuleManager::unloadModule()` ([ADR-0061]).
enum class ModuleUnloadResult : gp::UInt8
{
    /// @brief The module was cleanly shut down, destroyed, and (if dynamic) unmapped from the process.
    Success = 0u,

    /// @brief The requested module name was empty or normalized to `"None"`.
    InvalidName = 1u,

    /// @brief The module is not currently loaded in the registry.
    NotLoaded = 2u,

    /// @brief The module reported `supportsDynamicReloading() == false` during a `Normal` or `HotReload` unload.
    NotReloadable = 3u,

    /// @brief The module is currently transitioning (`ModuleState::Loading` or `ModuleState::Unloading`).
    InUseOrLoading = 4u,

    /// @brief A recursive unload collided with an active shutdown sequence.
    ShuttingDown = 5u
};

/// @brief Lifecycle state of a registered module slot inside `ModuleManager` ([ADR-0061]).
enum class ModuleState : gp::UInt8
{
    /// @brief Slot is empty or the module has been completely unloaded.
    Unloaded = 0u,

    /// @brief Static factory or descriptor is registered, but the module has not been loaded yet.
    Registered = 1u,

    /// @brief Shared library load, ABI handshake, or `onStartupModule()` is currently in progress.
    Loading = 2u,

    /// @brief Module is active, `onStartupModule()` succeeded, and `getModule()` returns its `IModule*`.
    Loaded = 3u,

    /// @brief `onShutdownModule()` and instance teardown are currently in progress.
    Unloading = 4u,

    /// @brief The most recent load attempt failed (inspect `ModuleStatusView::lastLoadResult`).
    Failed = 5u
};

/// @brief Capability and provenance bitflags associated with a module slot ([ADR-0061]).
enum class ModuleFlags : gp::UInt8
{
    /// @brief Default empty flag set.
    None = 0x00u,

    /// @brief The module supports runtime unloading and hot-reloading (`IModule::supportsDynamicReloading()`).
    SupportsDynamicReloading = 0x01u,

    /// @brief The module contains primary gameplay logic (`IModule::isGameModule()`).
    IsGameModule = 0x02u,

    /// @brief The module was instantiated from an in-process static registrant rather than a dynamic OS handle.
    IsStaticLinked = 0x04u,

    /// @brief The module confirmed that its local shadow `Malloc*` pointer was synchronized during handshake.
    ShadowMallocBound = 0x08u
};

/// @brief Notification event kind broadcast by `ModuleManager::onModuleChanged()` ([ADR-0061]).
enum class ModuleChangeReason : gp::UInt8
{
    /// @brief A module finished `onStartupModule()` and transitioned to `ModuleState::Loaded`.
    Loaded = 0u,

    /// @brief A loaded module is about to execute `onShutdownModule()` and unload.
    Unloading = 1u,

    /// @brief A module finished unloading and its OS shared library handle (if any) was released.
    Unloaded = 2u,

    /// @brief A module load attempt failed.
    LoadFailed = 3u
};

/// @brief Context phase passed to `ModuleManager::unloadModule()` and `unloadAllModules()` ([ADR-0061]).
enum class ModuleShutdownPhase : gp::UInt8
{
    /// @brief Standard runtime unload request; respects `IModule::supportsDynamicReloading()`.
    Normal = 0u,

    /// @brief Process-exit or manager teardown; forcefully unloads all modules in reverse-LIFO order.
    Exit = 1u,

    /// @brief Hot-reload cycle (`reloadModule()`); invokes `onPreUnloadCallback()` and `onPostLoadCallback()`.
    HotReload = 2u
};

/// @brief Converts a `ModuleLoadResult` enumerator into a human-readable diagnostic `gp::StringView`.
/// @param[in] result Load result code to format.
/// @return Static ASCII `StringView` describing `result`.
[[nodiscard]] constexpr gp::StringView toStringView(const ModuleLoadResult result) noexcept
{
    switch (result)
    {
    case ModuleLoadResult::Success:
        return "Success";
    case ModuleLoadResult::InvalidName:
        return "InvalidName";
    case ModuleLoadResult::AlreadyLoaded:
        return "AlreadyLoaded";
    case ModuleLoadResult::NotFound:
        return "NotFound";
    case ModuleLoadResult::LibraryLoadFailed:
        return "LibraryLoadFailed";
    case ModuleLoadResult::EntryPointMissing:
        return "EntryPointMissing";
    case ModuleLoadResult::AbiVersionMismatch:
        return "AbiVersionMismatch";
    case ModuleLoadResult::InstantiationFailed:
        return "InstantiationFailed";
    case ModuleLoadResult::StartupFailed:
        return "StartupFailed";
    case ModuleLoadResult::CircularDependency:
        return "CircularDependency";
    case ModuleLoadResult::ShuttingDown:
        return "ShuttingDown";
    default:
        break;
    }
    return "Unknown";
}

/// @brief Converts a `ModuleUnloadResult` enumerator into a human-readable diagnostic `gp::StringView`.
/// @param[in] result Unload result code to format.
/// @return Static ASCII `StringView` describing `result`.
[[nodiscard]] constexpr gp::StringView toStringView(const ModuleUnloadResult result) noexcept
{
    switch (result)
    {
    case ModuleUnloadResult::Success:
        return "Success";
    case ModuleUnloadResult::InvalidName:
        return "InvalidName";
    case ModuleUnloadResult::NotLoaded:
        return "NotLoaded";
    case ModuleUnloadResult::NotReloadable:
        return "NotReloadable";
    case ModuleUnloadResult::InUseOrLoading:
        return "InUseOrLoading";
    case ModuleUnloadResult::ShuttingDown:
        return "ShuttingDown";
    default:
        break;
    }
    return "Unknown";
}

/// @brief Converts a `ModuleState` enumerator into a human-readable diagnostic `gp::StringView`.
/// @param[in] state Lifecycle state to format.
/// @return Static ASCII `StringView` describing `state`.
[[nodiscard]] constexpr gp::StringView toStringView(const ModuleState state) noexcept
{
    switch (state)
    {
    case ModuleState::Unloaded:
        return "Unloaded";
    case ModuleState::Registered:
        return "Registered";
    case ModuleState::Loading:
        return "Loading";
    case ModuleState::Loaded:
        return "Loaded";
    case ModuleState::Unloading:
        return "Unloading";
    case ModuleState::Failed:
        return "Failed";
    default:
        break;
    }
    return "Unknown";
}

}   // namespace gp::modules

GP_ENABLE_ENUM_BITWISE_OPERATIONS(gp::modules::ModuleFlags);
