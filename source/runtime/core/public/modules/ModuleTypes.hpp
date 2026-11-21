// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/strings/Name.hpp"
#include "CoreMinimal.hpp"
#include "memory/MemoryForward.hpp"
#include "modules/ModuleEnums.hpp"
#include "modules/ModuleForward.hpp"   // IWYU pragma: export
#include "templates/Delegate.hpp"

namespace gp::modules
{

/// @brief 64-bit ASCII magic identifier (`"GPMODULE"`) verifying a valid `gp-engine` module handshake ([ADR-0061]).
inline constexpr gp::UInt64 kModuleAbiMagic = 0x47504D4F44554C45ULL;

/// @brief Monotonic ABI contract version checked across the `gpInitializeModule` entry boundary ([ADR-0061]).
inline constexpr gp::UInt32 kModuleAbiVersion = 1u;

/// @brief Exported C symbol name queried by `ModuleManager` to initialize a dynamic module and bind shadow `Malloc`.
inline constexpr const char* kInitializeModuleSymbolName = "gpInitializeModule";

/// @brief Exported C symbol name queried by `ModuleManager` to destroy a dynamic module and reset shadow `Malloc`.
inline constexpr const char* kDestroyModuleSymbolName = "gpDestroyModule";

/// @brief Maximum number of modules simultaneously tracked inside a `ModuleManager` registry instance.
inline constexpr gp::USize kMaxRegisteredModules = 128u;

/// @brief Maximum number of observer callbacks subscribed to `ModuleManager::onModuleChanged()`.
inline constexpr gp::USize kMaxModuleChangeListeners = 32u;

/// @brief Computes a 32-bit bitmask encoding the active translation unit's build configuration and linkage mode.
/// @details Embedded into `ModuleInitContext::buildFlags` and verified inside the target module's entry point so
///          accidentally loading a `Debug` DLL into a `Development` or `Shipping` host binary fails deterministically
///          with `ModuleLoadResult::AbiVersionMismatch` prior to heap allocation.
/// @return Bitmask representing the active compile-time build configuration.
[[nodiscard]] constexpr gp::UInt32 getCurrentModuleBuildFlags() noexcept
{
    gp::UInt32 flags = 0u;
#if GP_BUILD_DEBUG
    flags |= (1u << 0u);
#endif
#if GP_BUILD_DEVELOPMENT
    flags |= (1u << 1u);
#endif
#if GP_BUILD_PROFILE
    flags |= (1u << 2u);
#endif
#if GP_BUILD_SHIPPING
    flags |= (1u << 3u);
#endif
#if GP_IS_MONOLITHIC
    flags |= (1u << 4u);
#endif
#if GP_FORCE_ANSI_ALLOCATOR
    flags |= (1u << 5u);
#endif
    return flags;
}

/// @brief POD context passed by `ModuleManager` across the C ABI boundary into `gpInitializeModule` ([ADR-0061]).
struct ModuleInitContext final
{
    /// @brief Must equal `kModuleAbiMagic` (`0x47504D4F44554C45ULL`).
    gp::UInt64 abiMagic{ kModuleAbiMagic };

    /// @brief Must equal `kModuleAbiVersion`.
    gp::UInt32 abiVersion{ kModuleAbiVersion };

    /// @brief Host build configuration bitmask produced by `getCurrentModuleBuildFlags()`.
    gp::UInt32 buildFlags{ getCurrentModuleBuildFlags() };

    /// @brief Host process's canonical `gp::memory::Malloc*` instance to bind into the module's local shadow slot.
    gp::memory::Malloc* globalMalloc{ nullptr };
};

/// @brief POD output structure populated by `gpInitializeModule` inside the loaded module ([ADR-0061]).
struct ModuleInitHandshake final
{
    /// @brief Module's compiled `kModuleAbiMagic` token.
    gp::UInt64 abiMagic{ 0ULL };

    /// @brief Module's compiled `kModuleAbiVersion` number.
    gp::UInt32 abiVersion{ 0u };

    /// @brief Module's compiled `getCurrentModuleBuildFlags()` bitmask.
    gp::UInt32 buildFlags{ 0u };

    /// @brief Result status of the entry-point handshake (`ModuleLoadResult::Success` on success).
    ModuleLoadResult result{ ModuleLoadResult::InstantiationFailed };

    /// @brief Newly constructed `IModule*` instance allocated via the module's synchronized shadow `Malloc`.
    IModule* moduleInstance{ nullptr };

    /// @brief Value of the module's image-local `getInlineMalloc()` / `getActiveMalloc()` after binding.
    gp::memory::Malloc* boundShadowMalloc{ nullptr };

    /// @brief Null-terminated module name literal compiled into the target module's entry point
    /// (`GP_IMPLEMENT_MODULE`).
    /// @details Prevents POSIX `dlsym` breadth-first dependency fallthrough from accidentally resolving a parent
    ///          shared library's `gpInitializeModule` symbol when a child library omits `GP_IMPLEMENT_MODULE`.
    const char* declaredModuleName{ nullptr };
};

/// @brief C-ABI entry-point function pointer signature exported by dynamic modules as `gpInitializeModule`.
/// @param[in] context Host initialization context carrying ABI tokens and the canonical `Malloc*`.
/// @param[out] outHandshake Receives the module's ABI verification tokens, `IModule*` instance, and shadow `Malloc*`.
using InitializeModuleFn = void (*)(const ModuleInitContext* context, ModuleInitHandshake* outHandshake) noexcept;

/// @brief C-ABI teardown function pointer signature exported by dynamic modules as `gpDestroyModule`.
/// @param[in] moduleInstance Module instance previously created by `gpInitializeModule`.
using DestroyModuleFn = void (*)(IModule* moduleInstance) noexcept;

/// @brief Factory function signature used by statically linked or manually registered modules.
/// @param[in] hostMalloc Host process's canonical `Malloc*` instance.
/// @param[out] outBoundShadowMalloc Optional pointer receiving the synchronized shadow `Malloc*`.
/// @return Newly allocated `IModule*` instance, or `nullptr` on allocation failure.
using StaticModuleFactoryFn = IModule* (*)(gp::memory::Malloc * hostMalloc,
                                           gp::memory::Malloc** outBoundShadowMalloc) noexcept;

/// @brief Teardown function signature used by statically linked or manually registered modules.
/// @param[in] moduleInstance Module instance previously created by `StaticModuleFactoryFn`.
using StaticModuleDestroyFn = void (*)(IModule* moduleInstance) noexcept;

/// @brief Single-cast observer delegate signature for module lifecycle events.
using ModuleChangedDelegate = gp::Delegate<void(gp::Name, ModuleChangeReason)>;

/// @brief Fixed-capacity multicast delegate broadcasting module lifecycle transitions.
using ModuleChangedMulticastDelegate =
    gp::FixedMulticastDelegate<kMaxModuleChangeListeners, gp::Name, ModuleChangeReason>;

/// @brief Stateless C callback signature for `ModuleManager::iterateModules()`.
/// @param[in] status Read-only snapshot of the visited module slot.
/// @param[in] userData Opaque caller context pointer.
/// @return True to continue iteration, false to stop early.
using ModuleVisitorFn = bool (*)(const ModuleStatusView& status, void* userData) noexcept;

}   // namespace gp::modules
