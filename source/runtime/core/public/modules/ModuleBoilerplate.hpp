// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Relationship.hpp"
#include "CoreMinimal.hpp"
#include "memory/GlobalMemory.hpp"
#include "memory/MemoryOverrides.hpp"   // IWYU pragma: keep
#include "modules/IModule.hpp"
#include "modules/ModuleEnums.hpp"
#include "modules/ModuleTypes.hpp"
#include <memory>

namespace gp::modules
{

/// @brief Intrusive singly-linked `.bss` node used to register statically linked modules before `main()` without heap
///        allocations ([ADR-0057], [ADR-0061]).
struct StaticModuleRegistrantNode final
{
    /// @brief Raw module identifier literal (e.g., `"rhi/null"` or `"audio/openal"`).
    const char* moduleName{ nullptr };

    /// @brief Stateless factory function constructing the module instance and binding shadow `Malloc`.
    StaticModuleFactoryFn createFn{ nullptr };

    /// @brief Stateless teardown function destroying the module instance.
    StaticModuleDestroyFn destroyFn{ nullptr };

    /// @brief Intrusive pointer to the next node in the global `.bss` deferred registration list.
    StaticModuleRegistrantNode* next{ nullptr };

    /// @brief True while linked into the global static module registrant list.
    bool isRegistered{ false };
};

/// @brief Links a `StaticModuleRegistrantNode` into the process-wide `.bss` intrusive list (zero heap allocations).
/// @note Thread-safe and Static Initialization Order Fiasco (SIOF) immune; guarded by a `constinit` `AtomicFlag`.
/// @param[in,out] node Persistent registrant node residing in `.bss` / static storage.
GP_CORE_API void registerStaticModuleNode(StaticModuleRegistrantNode& node) noexcept;

/// @brief Unlinks a `StaticModuleRegistrantNode` from the process-wide `.bss` intrusive list on static destruction.
/// @param[in,out] node Registrant node to unlink.
GP_CORE_API void unregisterStaticModuleNode(StaticModuleRegistrantNode& node) noexcept;

/// @brief Executes the in-module C-ABI initialization handshake inside the target module's translation unit
///        ([ADR-0061]).
/// @details Because this function template is instantiated directly inside the target module's `.cpp` file (via
///          `GP_IMPLEMENT_MODULE`), calling `gp::memory::setupLocalShadowMalloc(context->globalMalloc)` updates that
///          specific `.dll`/`.so`/`.dylib` binary's private `gp::memory::detail::g_localShadowMalloc` pointer *before*
///          allocating `InModuleClass`.
/// @tparam InModuleClass Concrete `IModule` subclass to instantiate.
/// @param[in] context Host initialization context containing ABI tokens and the canonical `Malloc*`.
/// @param[out] outHandshake Receives ABI verification metadata, the constructed `IModule*`, and the bound shadow
///                          `Malloc*`.
/// @param[in] declaredModuleName Optional null-terminated module identifier literal compiled into the target binary.
template <typename InModuleClass>
requires gp::concepts::IsDerivedFrom<InModuleClass, IModule> GP_FORCEINLINE_HINT void executeModuleEntryHandshake(
    const ModuleInitContext* const context,
    ModuleInitHandshake* const outHandshake,
    const char* const declaredModuleName = nullptr
) noexcept
{
    if (outHandshake == nullptr) [[unlikely]]
    {
        return;
    }

    const gp::UInt32 localBuildFlags = getCurrentModuleBuildFlags();
    outHandshake->abiMagic = kModuleAbiMagic;
    outHandshake->abiVersion = kModuleAbiVersion;
    outHandshake->buildFlags = localBuildFlags;
    outHandshake->result = ModuleLoadResult::InstantiationFailed;
    outHandshake->moduleInstance = nullptr;
    outHandshake->boundShadowMalloc = nullptr;
    outHandshake->declaredModuleName = declaredModuleName;

    if (context == nullptr) [[unlikely]]
    {
        outHandshake->result = ModuleLoadResult::AbiVersionMismatch;
        return;
    }

    if (context->abiMagic != kModuleAbiMagic || context->abiVersion != kModuleAbiVersion ||
        context->buildFlags != localBuildFlags) [[unlikely]]
    {
        outHandshake->result = ModuleLoadResult::AbiVersionMismatch;
        return;
    }

    // Bind this shared library image's local shadow Malloc pointer before performing any allocation!
    gp::memory::Malloc* const boundMalloc = gp::memory::setupLocalShadowMalloc(context->globalMalloc);
    if (boundMalloc == nullptr) [[unlikely]]
    {
        outHandshake->result = ModuleLoadResult::InstantiationFailed;
        return;
    }

    outHandshake->boundShadowMalloc =
        (gp::memory::getInlineMalloc() != nullptr) ? gp::memory::getInlineMalloc() : boundMalloc;

    void* const rawStorage =
        boundMalloc->tryAllocate(sizeof(InModuleClass), static_cast<gp::UInt32>(alignof(InModuleClass)));
    if (rawStorage == nullptr) [[unlikely]]
    {
        outHandshake->result = ModuleLoadResult::InstantiationFailed;
        return;
    }

    InModuleClass* const instance = std::construct_at(static_cast<InModuleClass*>(rawStorage));
    outHandshake->moduleInstance = instance;
    outHandshake->result = ModuleLoadResult::Success;
}

/// @brief Destroys a module instance inside the target module's translation unit and resets its local shadow `Malloc`
///        when `InResetShadowMalloc` is true ([ADR-0061]).
/// @tparam InModuleClass Concrete `IModule` subclass to destroy.
/// @tparam InResetShadowMalloc When true (for standalone dynamic DLLs), clears `detail::g_localShadowMalloc` after
///                             deallocating the module instance.
/// @param[in] moduleInstance Module instance pointer previously returned by `executeModuleEntryHandshake`.
template <typename InModuleClass, bool InResetShadowMalloc = true>
requires gp::concepts::IsDerivedFrom<InModuleClass, IModule>
GP_FORCEINLINE_HINT void executeModuleExitTeardown(IModule* const moduleInstance) noexcept
{
    if (moduleInstance != nullptr) [[likely]]
    {
        gp::memory::Malloc* const activeMalloc = gp::memory::getActiveMalloc();
        InModuleClass* const typedInstance = static_cast<InModuleClass*>(moduleInstance);
        std::destroy_at(typedInstance);
        if (activeMalloc != nullptr) [[likely]]
        {
            activeMalloc->deallocate(typedInstance);
        }
    }

    if constexpr (InResetShadowMalloc)
    {
#if !GP_IS_MONOLITHIC && !defined(GP_CORE_EXPORTS)
        gp::memory::resetLocalShadowMalloc();
#endif
    }
}

/// @brief RAII static registrant helper template that links a module factory into the `.bss` static module list on
///        construction and unlinks it on destruction ([ADR-0061]).
/// @tparam InModuleClass Concrete `IModule` subclass to register.
template <typename InModuleClass>
requires gp::concepts::IsDerivedFrom<InModuleClass, IModule> class StaticModuleRegistrant final
{
public:
    /// @brief Constructs the registrant and pushes its node onto the global `.bss` static module list.
    /// @param[in] moduleName Null-terminated module name literal (e.g., `"rhi/null"`).
    explicit StaticModuleRegistrant(const char* const moduleName) noexcept
    {
        m_node.moduleName = moduleName;
        m_node.createFn = &StaticModuleRegistrant::createInstance;
        m_node.destroyFn = &StaticModuleRegistrant::destroyInstance;
        m_node.next = nullptr;
        m_node.isRegistered = false;
        registerStaticModuleNode(m_node);
    }

    /// @brief Unlinks the registrant node from the global `.bss` static module list.
    ~StaticModuleRegistrant() noexcept
    {
        unregisterStaticModuleNode(m_node);
    }

    StaticModuleRegistrant(const StaticModuleRegistrant&) = delete;
    StaticModuleRegistrant& operator=(const StaticModuleRegistrant&) = delete;
    StaticModuleRegistrant(StaticModuleRegistrant&&) = delete;
    StaticModuleRegistrant& operator=(StaticModuleRegistrant&&) = delete;

    /// @brief Stateless factory function matching `StaticModuleFactoryFn`.
    /// @param[in] hostMalloc Host process's canonical `Malloc*` instance.
    /// @param[out] outBoundShadowMalloc Optional pointer receiving the synchronized shadow `Malloc*`.
    /// @return Constructed `IModule*` instance, or `nullptr` on failure.
    [[nodiscard]] static IModule*
        createInstance(gp::memory::Malloc* const hostMalloc, gp::memory::Malloc** const outBoundShadowMalloc) noexcept
    {
        const ModuleInitContext ctx{ .abiMagic = kModuleAbiMagic,
                                     .abiVersion = kModuleAbiVersion,
                                     .buildFlags = getCurrentModuleBuildFlags(),
                                     .globalMalloc = hostMalloc };
        ModuleInitHandshake handshake{};
        executeModuleEntryHandshake<InModuleClass>(&ctx, &handshake);
        if (outBoundShadowMalloc != nullptr)
        {
            *outBoundShadowMalloc = handshake.boundShadowMalloc;
        }
        return handshake.moduleInstance;
    }

    /// @brief Stateless teardown function matching `StaticModuleDestroyFn`.
    /// @param[in] moduleInstance Module instance to destroy.
    static void destroyInstance(IModule* const moduleInstance) noexcept
    {
        executeModuleExitTeardown<InModuleClass, false>(moduleInstance);
    }

private:
    StaticModuleRegistrantNode m_node{};
};

}   // namespace gp::modules

/// @brief Injects per-DLL `operator new` and `operator delete` overrides into non-`core` dynamic shared library targets
///        ([ADR-0061]).
/// @details In modular builds (`!GP_IS_MONOLITHIC`), every shared library must route its local `operator new` and
///          `operator delete` through `gp::memory::getActiveMalloc()` (`g_localShadowMalloc`) so C++ allocations do not
///          fall back to a private per-DLL CRT heap. `gp-core` (`GP_CORE_EXPORTS`) already compiles these overrides in
///          `MemoryOverrides.cpp`, so the macro expands to nothing when compiled inside `gp-core` or monolithic
///          targets.
#if !GP_IS_MONOLITHIC && !defined(GP_CORE_EXPORTS) && !GP_WITH_TESTS
    #define GP_PER_MODULE_MEMORY_BOILERPLATE GP_OVERLOAD_OPERATOR_NEW_AND_DELETE
#else
    #define GP_PER_MODULE_MEMORY_BOILERPLATE
#endif
