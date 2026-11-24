// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "modules/ModuleForward.hpp"   // IWYU pragma: export

namespace gp::modules
{

/// @brief Pure abstract lifecycle contract implemented by all dynamically or statically loaded engine modules
///        ([ADR-0061]).
/// @details Serves as the uniform entry and exit boundary for pluralistic backend plugins (`rhi/d3d12`, `rhi/vulkan`,
///          `audio/openal`, `physics/jolt`, `parser/json`) and gameplay modules. When loaded dynamically, the module's
///          `gpInitializeModule` entry point synchronizes the DLL's image-local `g_localShadowMalloc` pointer *before*
///          allocating the `IModule` subclass or invoking `onStartupModule()`.
class GP_CORE_API IModule
{
public:
    /// @brief Default constructor.
    constexpr IModule() noexcept = default;

    /// @brief Virtual destructor.
    virtual ~IModule() noexcept = default;

    IModule(const IModule&) = delete;
    IModule& operator=(const IModule&) = delete;
    IModule(IModule&&) = delete;
    IModule& operator=(IModule&&) = delete;

public:
    /// @brief Invoked immediately after the module instance is constructed and its local shadow `Malloc` is bound.
    /// @note Executed outside `ModuleManager`'s internal registry lock, so implementations may safely load dependent
    ///       modules via `ModuleManager::get().loadModule(...)` without deadlocking.
    /// @return True if module initialization succeeded; false to abort loading and trigger immediate rollback.
    [[nodiscard]] virtual bool onStartupModule() noexcept
    {
        return true;
    }

    /// @brief Invoked immediately before the module instance is destroyed and its shared library is unmapped.
    /// @note Executed in strict reverse-LIFO load order during `ModuleManager::unloadAllModules()`.
    virtual void onShutdownModule() noexcept
    {}

    /// @brief Invoked prior to `onShutdownModule()` specifically when a module is about to be hot-reloaded via
    ///        `ModuleManager::reloadModule()`.
    virtual void onPreUnloadCallback() noexcept
    {}

    /// @brief Invoked after `onStartupModule()` specifically when a module has just finished hot-reloading via
    ///        `ModuleManager::reloadModule()`.
    virtual void onPostLoadCallback() noexcept
    {}

    /// @brief Queries whether this module permits runtime unloading and hot-reloading during normal execution.
    /// @details Subsystems holding permanent OS process hooks that cannot be safely unmapped until process exit should
    ///          override this method to return `false`.
    /// @return True if dynamic unloading/reloading is supported (default), false otherwise.
    [[nodiscard]] virtual bool supportsDynamicReloading() const noexcept
    {
        return true;
    }

    /// @brief Queries whether this module implements primary gameplay logic rather than an engine/backend subsystem.
    /// @return True for gameplay modules (`GP_IMPLEMENT_GAME_MODULE`), false for engine/backend modules (default).
    [[nodiscard]] virtual bool isGameModule() const noexcept
    {
        return false;
    }

    /// @brief Returns a 64-bit tag identifying a specialized `IModule` sub-interface without C++ RTTI.
    /// @return Non-zero contract tag for specialized module interfaces (such as `IDynamicRHIModule`), or `0` for
    ///         plain `IModule` implementations.
    [[nodiscard]] virtual gp::UInt64 getInterfaceTag() const noexcept
    {
        return 0ULL;
    }
};

/// @brief Minimal concrete `IModule` implementation for modules that require no custom startup or shutdown logic.
/// @details Used by `GP_IMPLEMENT_DEFAULT_MODULE(ModuleName)` and `GP_IMPLEMENT_DEFAULT_GAME_MODULE(ModuleName)`.
class GP_CORE_API DefaultModuleImpl : public IModule
{
public:
    /// @brief Default constructor.
    constexpr DefaultModuleImpl() noexcept = default;

    /// @brief Virtual destructor.
    virtual ~DefaultModuleImpl() noexcept override = default;
};

/// @brief Default concrete `IModule` specialization marking the module as a gameplay module (`isGameModule() == true`).
class GP_CORE_API DefaultGameModuleImpl : public DefaultModuleImpl
{
public:
    /// @brief Default constructor.
    constexpr DefaultGameModuleImpl() noexcept = default;

    /// @brief Virtual destructor.
    virtual ~DefaultGameModuleImpl() noexcept override = default;

    /// @copydoc IModule::isGameModule()
    [[nodiscard]] virtual bool isGameModule() const noexcept override
    {
        return true;
    }
};

}   // namespace gp::modules
