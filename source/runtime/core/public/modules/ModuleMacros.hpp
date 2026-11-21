// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "modules/IModule.hpp"   // IWYU pragma: export
#include "modules/ModuleBoilerplate.hpp"

/// @brief Emits the `extern "C"` dynamic shared library entry points (`gpInitializeModule` and `gpDestroyModule`)
///        compiled inside the calling translation unit so `executeModuleEntryHandshake` binds the calling DLL's
///        image-local `g_localShadowMalloc` pointer ([ADR-0061]).
/// @param ModuleClass Concrete `gp::modules::IModule` subclass to instantiate for this shared library.
/// @param ModuleNameLiteral String literal naming the module (e.g., `"rhi/d3d12"`).
#define GP_IMPLEMENT_DYNAMIC_MODULE_ENTRY_POINTS_NAMED(ModuleClass, ModuleNameLiteral)                                 \
    extern "C" GP_DLLEXPORT void gpInitializeModule(                                                                   \
        const gp::modules::ModuleInitContext* const context, gp::modules::ModuleInitHandshake* const outHandshake      \
    ) noexcept                                                                                                         \
    {                                                                                                                  \
        gp::modules::executeModuleEntryHandshake<ModuleClass>(context, outHandshake, ModuleNameLiteral);               \
    }                                                                                                                  \
    extern "C" GP_DLLEXPORT void gpDestroyModule(gp::modules::IModule* const moduleInstance) noexcept                  \
    {                                                                                                                  \
        gp::modules::executeModuleExitTeardown<ModuleClass, true>(moduleInstance);                                     \
    }

/// @brief Emits the `extern "C"` dynamic shared library entry points without an explicit module name literal Stamp.
/// @param ModuleClass Concrete `gp::modules::IModule` subclass to instantiate for this shared library.
#define GP_IMPLEMENT_DYNAMIC_MODULE_ENTRY_POINTS(ModuleClass)                                                          \
    GP_IMPLEMENT_DYNAMIC_MODULE_ENTRY_POINTS_NAMED(ModuleClass, nullptr)

/// @brief Registers a statically linked module class into the `.bss` intrusive module list ([ADR-0061]).
/// @note Uses `__COUNTER__` so Unity Builds merging multiple module translation units never suffer symbol collisions.
/// @param ModuleClass Concrete `gp::modules::IModule` subclass to instantiate when loaded.
/// @param ModuleNameLiteral String literal naming the module (e.g., `"rhi/null"` or `"audio/openal"`).
#define GP_REGISTER_STATIC_MODULE(ModuleClass, ModuleNameLiteral)                                                      \
    namespace                                                                                                          \
    {                                                                                                                  \
    gp::modules::StaticModuleRegistrant<ModuleClass>                                                                   \
        GP_JOIN(s_gpStaticModuleRegistrant_, __COUNTER__)(ModuleNameLiteral);                                          \
    }

/// @brief Primary macro declaring a module's implementation class, per-DLL memory boilerplate, and entry point
///        ([ADR-0061]).
/// @details Automatically adapts to the active build topology:
///          - **Modular Builds (`!GP_IS_MONOLITHIC`):** Injects `GP_PER_MODULE_MEMORY_BOILERPLATE` (`operator new` and
///            `operator delete` routed through `gp::memory::getActiveMalloc()`) and exports `gpInitializeModule` /
///            `gpDestroyModule`, which synchronize the DLL's local `g_localShadowMalloc` pointer prior to module
///            allocation.
///          - **Monolithic Builds (`GP_IS_MONOLITHIC`):** Emits a `.bss` `StaticModuleRegistrant` node so
///            `ModuleManager::loadModule(ModuleNameLiteral)` instantiates `ModuleClass` without OS `SharedLibrary` I/O
///            or duplicate `extern "C"` symbol collisions.
/// @param ModuleClass Concrete `gp::modules::IModule` subclass implementing this module.
/// @param ModuleNameLiteral String literal naming the module (e.g., `"rhi/d3d12"`, `"audio/openal"`).
#if GP_IS_MONOLITHIC
    #define GP_IMPLEMENT_MODULE(ModuleClass, ModuleNameLiteral)                                                        \
        GP_REGISTER_STATIC_MODULE(ModuleClass, ModuleNameLiteral)
#else
    #define GP_IMPLEMENT_MODULE(ModuleClass, ModuleNameLiteral)                                                        \
        GP_PER_MODULE_MEMORY_BOILERPLATE                                                                               \
        GP_IMPLEMENT_DYNAMIC_MODULE_ENTRY_POINTS_NAMED(ModuleClass, ModuleNameLiteral)
#endif

/// @brief Declares a primary gameplay module (`IModule::isGameModule() == true`) ([ADR-0061]).
/// @param ModuleClass Concrete `gp::modules::IModule` subclass implementing the gameplay module.
/// @param ModuleNameLiteral String literal naming the gameplay module.
#define GP_IMPLEMENT_GAME_MODULE(ModuleClass, ModuleNameLiteral) GP_IMPLEMENT_MODULE(ModuleClass, ModuleNameLiteral)

/// @brief Declares a module using `gp::modules::DefaultModuleImpl` when no custom startup/shutdown hooks are needed.
/// @param ModuleNameLiteral String literal naming the module.
#define GP_IMPLEMENT_DEFAULT_MODULE(ModuleNameLiteral)                                                                 \
    GP_IMPLEMENT_MODULE(gp::modules::DefaultModuleImpl, ModuleNameLiteral)

/// @brief Declares a gameplay module using `gp::modules::DefaultGameModuleImpl` when no custom hooks are needed.
/// @param ModuleNameLiteral String literal naming the gameplay module.
#define GP_IMPLEMENT_DEFAULT_GAME_MODULE(ModuleNameLiteral)                                                            \
    GP_IMPLEMENT_GAME_MODULE(gp::modules::DefaultGameModuleImpl, ModuleNameLiteral)
