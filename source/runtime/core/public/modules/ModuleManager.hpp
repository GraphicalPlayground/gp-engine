// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Relationship.hpp"
#include "containers/strings/Name.hpp"
#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "diagnostics/Assert.hpp"            // IWYU pragma: keep
#include "logging/LogMacros.hpp"
#include "modules/IModule.hpp"
#include "modules/ModuleBoilerplate.hpp"   // IWYU pragma: keep
#include "modules/ModuleDesc.hpp"
#include "modules/ModuleEnums.hpp"
#include "modules/ModuleForward.hpp"   // IWYU pragma: export
#include "modules/ModuleTypes.hpp"
#include "platforms/base/system/PathTypes.hpp"
#include "platforms/generic/system/GenericThread.hpp"
#include "templates/Delegate.hpp"
#include "threading/atomics/Atomic.hpp"
#include "threading/sync/SharedMutex.hpp"

namespace gp
{

/// @brief Built-in log category for `gp::modules::ModuleManager` dynamic loading, ABI handshakes, and teardown
///        ([ADR-0054], [ADR-0061]).
GP_DECLARE_EXPORTED_LOG_CATEGORY_EXTERN(GP_CORE_API, LogModules);

}   // namespace gp

namespace gp::modules
{

/// @brief Central runtime registry and loader managing dynamic (`.dll`/`.so`/`.dylib`) and static engine modules
///        ([ADR-0061]).
/// @details Resides in `gp::core` so foundational pluralistic subsystems (`rhi/base`, `audio/base`, `physics/base`,
///          `parser/base`, `hal/`) and high-level orchestrators (`gp::application`) can load and unload backend
///          implementations without circular module dependencies (amending [ADR-0041]):
///          - **Zero-Allocation `.bss` Slot Table:** Tracks up to `kMaxRegisteredModules` (`128`) modules in fixed
///            inline storage guarded by a 64-byte `CacheAlignedSharedMutex` ([ADR-0058]).
///          - **Cross-DLL Local Shadow `Malloc` Handshake:** Passes `gp::memory::getGlobalMalloc()` inside
///            `ModuleInitContext` to each dynamic module's `gpInitializeModule` entry point, binding the target DLL's
///            image-local `g_localShadowMalloc` pointer prior to constructing its `IModule` instance.
///          - **Loader-Lock & Re-Entrancy Safety:** Releases the internal registry lock across OS `SharedLibrary`
///          calls,
///            `gpInitializeModule`, `onStartupModule()`, `onShutdownModule()`, and delegate broadcasts, while catching
///            same-thread recursive load cycles via `ModuleLoadResult::CircularDependency`.
///          - **Deterministic Reverse-LIFO Teardown:** `unloadAllModules()` shuts down active modules in strict
///            descending `loadOrder` so high-level dependents unload before low-level backends.
class GP_CORE_API ModuleManager final
{
private:
    struct ModuleSlot final
    {
        gp::Name canonicalName{};
        gp::PathBuffer explicitLibraryPath{};
        gp::PathBuffer resolvedLibraryPath{};
        IModule* instance{ nullptr };
        void* libraryHandle{ nullptr };
        DestroyModuleFn dynamicDestroyFn{ nullptr };
        StaticModuleFactoryFn staticCreateFn{ nullptr };
        StaticModuleDestroyFn staticDestroyFn{ nullptr };
        gp::memory::Malloc* boundShadowMalloc{ nullptr };
        gp::platform::ThreadId transitionThreadId{ 0ULL };
        gp::UInt32 loadOrder{ 0u };
        ModuleState state{ ModuleState::Unloaded };
        ModuleFlags flags{ ModuleFlags::None };
        ModuleLoadResult lastLoadResult{ ModuleLoadResult::Success };
        bool isOccupied{ false };
    };

public:
    /// @brief Returns the process-wide singleton `ModuleManager` instance residing in `.bss`.
    /// @return Reference to the global `ModuleManager`.
    [[nodiscard]] static ModuleManager& get() noexcept;

    /// @brief Normalizes a raw module identifier into its canonical lower-kebab-case `gp::Name` key.
    /// @details Strips any leading/trailing whitespace, leading `"gp-"` or `"libgp-"` binary prefixes, and platform
    ///          shared library extensions (`".dll"`, `".so"`, `".dylib"`), then converts path separators (`'/'`,
    ///          `'\\'`, `'_'`) to `'-'` in lowercase ASCII. Consequently, `"rhi/d3d12"`, `"RHI_D3D12"`,
    ///          `"rhi-d3d12"`, and `"gp-rhi-d3d12.dll"` all normalize to `gp::Name("rhi-d3d12")`.
    /// @param[in] rawName Input module identifier string view.
    /// @param[out] outCanonicalBuffer Optional `PathBuffer` receiving the normalized lower-kebab-case string
    /// characters.
    /// @return Canonical `gp::Name` (`Name()` / `"None"` if `rawName` is empty).
    [[nodiscard]] static gp::Name
        normalizeModuleName(gp::StringView rawName, gp::PathBuffer* outCanonicalBuffer = nullptr) noexcept;

    /// @brief Formats a module identifier into GPBT's canonical shared library filename for the target OS.
    /// @details Produces `"gp-<kebab-name>.dll"` on Windows, `"libgp-<kebab-name>.so"` on Linux, and
    ///          `"libgp-<kebab-name>.dylib"` on macOS ([ADR-0003]).
    /// @param[in] rawName Input module identifier (e.g., `"rhi/d3d12"` or `"audio/openal"`).
    /// @return Formatted `gp::PathBuffer` containing the default shared library filename, or empty if invalid.
    [[nodiscard]] static gp::PathBuffer formatLibraryFilename(gp::StringView rawName) noexcept;

public:
    /// @brief Constructs an isolated `ModuleManager` instance (useful for sandboxed unit tests).
    [[nodiscard]] ModuleManager() noexcept = default;

    /// @brief Destructor. Unloads all active modules in reverse-LIFO order via `unloadAllModules()`.
    ~ModuleManager() noexcept;

    ModuleManager(const ModuleManager&) = delete;
    ModuleManager& operator=(const ModuleManager&) = delete;
    ModuleManager(ModuleManager&&) = delete;
    ModuleManager& operator=(ModuleManager&&) = delete;

public:
    /// @section Search Path & Static Factory Registration

    /// @brief Registers a persistent filesystem directory to search when resolving dynamic module shared libraries.
    /// @param[in] directory Directory path (UTF-8) forwarded to `gp::platform::SharedLibrary::addDirectory()`.
    void addModuleSearchDirectory(gp::StringView directory) noexcept;

    /// @brief Drains any pending `.bss` `StaticModuleRegistrantNode` entries registered via `GP_REGISTER_STATIC_MODULE`
    ///        or monolithic `GP_IMPLEMENT_MODULE` into this manager's slot table.
    /// @note Invoked automatically by `loadModule()`, `isModuleRegistered()`, and `get()`; exposed publicly for
    ///       deterministic test verification.
    void drainPendingStaticRegistrants() noexcept;

    /// @brief Imperatively registers an in-process static module factory under `name`.
    /// @param[in] name Module name (e.g., `"rhi/null"` or `"audio/mock"`).
    /// @param[in] createFn Non-null factory function creating the `IModule*` instance and binding shadow `Malloc`.
    /// @param[in] destroyFn Non-null teardown function destroying the `IModule*` instance.
    /// @return True if registered or updated successfully; false if arguments are invalid or the slot table is full.
    bool registerStaticModule(
        gp::StringView name, StaticModuleFactoryFn createFn, StaticModuleDestroyFn destroyFn
    ) noexcept;

    /// @brief Unregisters a previously registered static module factory if the module is not currently loaded.
    /// @param[in] name Module name to unregister.
    /// @return True if the static factory registration was removed; false if not found or currently loaded.
    bool unregisterStaticModule(gp::StringView name) noexcept;

public:
    /// @section Loading, Unloading & Hot-Reloading

    /// @brief Loads and initializes a module by name, or returns its existing instance if already loaded.
    /// @param[in] name Module identifier (e.g., `"rhi/d3d12"`, `"audio/openal"`, or `"rhi-d3d12"`).
    /// @param[out] outResult Optional pointer receiving the detailed `ModuleLoadResult` status code.
    /// @return Non-null `IModule*` on success (`Success` or `AlreadyLoaded`), or `nullptr` on failure.
    [[nodiscard]] IModule* loadModule(gp::StringView name, ModuleLoadResult* outResult = nullptr) noexcept;

    /// @brief Loads and initializes a module using an explicit `ModuleDesc` configuration (supporting custom DLL
    /// paths).
    /// @param[in] desc Descriptor specifying the module name, optional explicit shared library path, and flags.
    /// @param[out] outResult Optional pointer receiving the detailed `ModuleLoadResult` status code.
    /// @return Non-null `IModule*` on success (`Success` or `AlreadyLoaded`), or `nullptr` on failure.
    [[nodiscard]] IModule* loadModuleWithDesc(const ModuleDesc& desc, ModuleLoadResult* outResult = nullptr) noexcept;

    /// @brief Loads a module by name and returns a typed pointer to its concrete `IModule` interface.
    /// @tparam InModuleType Target `IModule` subclass or interface type.
    /// @param[in] name Module identifier.
    /// @param[out] outResult Optional pointer receiving the detailed `ModuleLoadResult` status code.
    /// @return Typed pointer to `InModuleType` on success, or `nullptr` on failure.
    template <typename InModuleType = IModule>
    requires gp::concepts::IsDerivedFrom<InModuleType, IModule>
    [[nodiscard]] InModuleType* loadModulePtr(gp::StringView name, ModuleLoadResult* outResult = nullptr) noexcept
    {
        return static_cast<InModuleType*>(loadModule(name, outResult));
    }

    /// @brief Loads a module by name and asserts that loading succeeded, returning a reference to `InModuleType`.
    /// @tparam InModuleType Target `IModule` subclass or interface type.
    /// @param[in] name Module identifier.
    /// @return Reference to the loaded `InModuleType` instance.
    template <typename InModuleType = IModule>
    requires gp::concepts::IsDerivedFrom<InModuleType, IModule>
    [[nodiscard]] InModuleType& loadModuleChecked(gp::StringView name) noexcept
    {
        ModuleLoadResult result = ModuleLoadResult::Success;
        IModule* const modulePtr = loadModule(name, &result);
        GP_ASSERT(modulePtr != nullptr, "ModuleManager::loadModuleChecked failed to load requested module.");
        return *static_cast<InModuleType*>(modulePtr);
    }

    /// @brief Shuts down, destroys, and unloads a currently loaded module.
    /// @param[in] name Module identifier to unload.
    /// @param[in] phase Shutdown context (`Normal` respects `supportsDynamicReloading()`, `Exit` forces unload).
    /// @return `ModuleUnloadResult::Success` if unloaded cleanly, or an explicit error code otherwise.
    ModuleUnloadResult
        unloadModule(gp::StringView name, ModuleShutdownPhase phase = ModuleShutdownPhase::Normal) noexcept;

    /// @brief Performs a full hot-reload cycle (`onPreUnloadCallback` -> unload -> load -> `onPostLoadCallback`).
    /// @param[in] name Module identifier to reload.
    /// @param[out] outLoadResult Optional pointer receiving the `ModuleLoadResult` of the reload step.
    /// @return Non-null `IModule*` of the newly reloaded instance on success, or `nullptr` on failure.
    [[nodiscard]] IModule* reloadModule(gp::StringView name, ModuleLoadResult* outLoadResult = nullptr) noexcept;

    /// @brief Shuts down and unloads all currently loaded modules in strict reverse-LIFO order of `loadOrder`.
    /// @param[in] phase Shutdown phase forwarded to each unload operation (defaults to `ModuleShutdownPhase::Exit`).
    void unloadAllModules(ModuleShutdownPhase phase = ModuleShutdownPhase::Exit) noexcept;

    /// @brief Unloads all modules, clears all slot entries, and unbinds all observer delegates.
    /// @note Primarily intended for deterministic teardown between unit tests.
    void resetForTesting() noexcept;

public:
    /// @section Queries & Telemetry

    /// @brief Retrieves a loaded module's `IModule*` without triggering a load if it is not yet loaded.
    /// @param[in] name Module identifier.
    /// @return Non-null `IModule*` if the module is currently in `ModuleState::Loaded`, or `nullptr` otherwise.
    [[nodiscard]] IModule* getModule(gp::StringView name) const noexcept;

    /// @brief Retrieves a loaded module cast to `InModuleType*` without triggering a load.
    /// @tparam InModuleType Target `IModule` subclass or interface type.
    /// @param[in] name Module identifier.
    /// @return Typed pointer if currently loaded, or `nullptr` otherwise.
    template <typename InModuleType = IModule>
    requires gp::concepts::IsDerivedFrom<InModuleType, IModule>
    [[nodiscard]] InModuleType* getModulePtr(gp::StringView name) const noexcept
    {
        return static_cast<InModuleType*>(getModule(name));
    }

    /// @brief Retrieves an already-loaded module cast to `InModuleType&`, asserting that it is currently loaded.
    /// @tparam InModuleType Target `IModule` subclass or interface type.
    /// @param[in] name Module identifier.
    /// @return Reference to the active `InModuleType` instance.
    template <typename InModuleType = IModule>
    requires gp::concepts::IsDerivedFrom<InModuleType, IModule>
    [[nodiscard]] InModuleType& getModuleChecked(gp::StringView name) const noexcept
    {
        IModule* const modulePtr = getModule(name);
        GP_ASSERT(modulePtr != nullptr, "ModuleManager::getModuleChecked called for an unloaded module.");
        return *static_cast<InModuleType*>(modulePtr);
    }

    /// @brief Checks whether the specified module is currently loaded and active (`ModuleState::Loaded`).
    /// @param[in] name Module identifier.
    /// @return True if loaded, false otherwise.
    [[nodiscard]] bool isModuleLoaded(gp::StringView name) const noexcept;

    /// @brief Checks whether a module slot or static factory is registered under `name`.
    /// @param[in] name Module identifier.
    /// @return True if known to the registry, false otherwise.
    [[nodiscard]] bool isModuleRegistered(gp::StringView name) noexcept;

    /// @brief Copies a thread-safe diagnostic snapshot of the module slot matching `name` into `outStatus`.
    /// @param[in] name Module identifier.
    /// @param[out] outStatus Receives the module slot snapshot.
    /// @return True if a matching slot was found, false otherwise.
    [[nodiscard]] bool queryModuleStatus(gp::StringView name, ModuleStatusView& outStatus) const noexcept;

    /// @brief Returns the number of modules currently in `ModuleState::Loaded`.
    /// @return Active loaded module count.
    [[nodiscard]] gp::USize getLoadedModuleCount() const noexcept;

    /// @brief Returns the total number of occupied module slots in this manager.
    /// @return Occupied slot count.
    [[nodiscard]] gp::USize getRegisteredModuleCount() noexcept;

    /// @brief Visits all occupied module slots via a stateless callback under a shared read lock.
    /// @param[in] visitor Callback invoked for each occupied slot.
    /// @param[in] userData Optional opaque context pointer forwarded to `visitor`.
    void iterateModules(ModuleVisitorFn visitor, void* userData = nullptr) const noexcept;

    /// @brief Visits all occupied module slots using a C++ callable or lambda `(const ModuleStatusView&) -> bool`.
    /// @tparam InVisitor Callable type accepting `const ModuleStatusView&` and returning `bool` (or `void`).
    /// @param[in] visitor Callable invoked for each occupied module slot.
    template <typename InVisitor>
    requires(!gp::concepts::IsSameAs<std::remove_cvref_t<InVisitor>, ModuleVisitorFn>)
    void iterateModules(InVisitor&& visitor) const noexcept
    {
        using DecayedVisitorT = std::remove_cvref_t<InVisitor>;
        DecayedVisitorT* const visitorPtr = &visitor;
        iterateModules(
            [](const ModuleStatusView& status, void* const rawCtx) noexcept -> bool
        {
            DecayedVisitorT& fn = *static_cast<DecayedVisitorT*>(rawCtx);
            if constexpr (std::is_same_v<decltype(fn(status)), bool>)
            {
                return fn(status);
            }
            else
            {
                fn(status);
                return true;
            }
        },
            static_cast<void*>(visitorPtr)
        );
    }

    /// @brief Subscribes a listener delegate to module lifecycle change notifications (`Loaded`, `Unloading`, etc.).
    /// @param[in] callback Listener delegate to bind.
    /// @return Valid `DelegateHandle` token for unsubscription, or invalid handle if unbound or full.
    [[nodiscard]] gp::DelegateHandle subscribeModuleChanged(ModuleChangedDelegate callback) noexcept;

    /// @brief Unsubscribes a previously registered module lifecycle listener.
    /// @param[in] handle Token returned by `subscribeModuleChanged()`.
    /// @return True if the listener was found and removed, false otherwise.
    bool unsubscribeModuleChanged(gp::DelegateHandle handle) noexcept;

private:
    [[nodiscard]] ModuleSlot* findSlotMutable(gp::Name canonicalName) noexcept;
    [[nodiscard]] const ModuleSlot* findSlot(gp::Name canonicalName) const noexcept;
    [[nodiscard]] ModuleSlot* findOrAllocateSlot(gp::Name canonicalName) noexcept;
    static void populateStatusView(const ModuleSlot& slot, ModuleStatusView& outView) noexcept;
    void broadcastChangeUnlocked(gp::Name canonicalName, ModuleChangeReason reason) noexcept;

private:
    mutable gp::thread::CacheAlignedSharedMutex m_mutex{};
    mutable gp::thread::CacheAlignedSharedMutex m_delegateMutex{};
    ModuleSlot m_slots[kMaxRegisteredModules]{};
    ModuleChangedMulticastDelegate m_onModuleChanged{};
    gp::thread::Atomic<gp::UInt32> m_nextLoadOrder{ 1u };
    gp::thread::Atomic<gp::UInt32> m_syncedStaticRegistrantEpoch{ 0u };
    gp::thread::Atomic<bool> m_isShuttingDown{ false };
};

}   // namespace gp::modules
