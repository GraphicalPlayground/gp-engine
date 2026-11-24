// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "console/ConsoleEnums.hpp"
#include "console/ConsoleTypes.hpp"
#include "console/IConsoleManager.hpp"
#include "containers/arrays/Vector.hpp"   // IWYU pragma: keep
#include "containers/strings/String.hpp"
#include "memory/GlobalMemory.hpp"
#include "modules/Modules.hpp"
#include "platforms/base/Platform.hpp"
#include "platforms/base/system/FileSystem.hpp"
#include "platforms/base/system/Paths.hpp"
#include "platforms/base/system/SharedLibrary.hpp"
#include "threading/atomics/Atomic.hpp"
#include <gtest/gtest.h>
#include <thread>
#include <vector>

namespace gp::modules::tests
{

namespace
{

/// @brief Tracks ordered startup/shutdown lifecycle transitions across mock backend modules.
struct ModuleLifecycleLedger final
{
    static inline gp::UInt32 s_startupSequence[32]{};
    static inline gp::UInt32 s_shutdownSequence[32]{};
    static inline gp::USize s_startupCount{ 0u };
    static inline gp::USize s_shutdownCount{ 0u };
    static inline gp::UInt32 s_preUnloadHits{ 0u };
    static inline gp::UInt32 s_postLoadHits{ 0u };
    static inline gp::UInt32 s_activeInstances{ 0u };

    static void reset() noexcept
    {
        for (gp::USize i = 0u; i < 32u; ++i)
        {
            s_startupSequence[i] = 0u;
            s_shutdownSequence[i] = 0u;
        }
        s_startupCount = 0u;
        s_shutdownCount = 0u;
        s_preUnloadHits = 0u;
        s_postLoadHits = 0u;
        s_activeInstances = 0u;
    }

    static void recordStartup(const gp::UInt32 id) noexcept
    {
        if (s_startupCount < 32u)
        {
            s_startupSequence[s_startupCount++] = id;
        }
    }

    static void recordShutdown(const gp::UInt32 id) noexcept
    {
        if (s_shutdownCount < 32u)
        {
            s_shutdownSequence[s_shutdownCount++] = id;
        }
    }
};

/// @brief Concrete test module representing a dynamically or statically loaded RHI backend (`rhi/mock_d3d12`).
class MockRhiBackendModule final : public IModule
{
public:
    static constexpr gp::UInt32 kBackendTag = 101u;

public:
    MockRhiBackendModule() noexcept
    {
        ++ModuleLifecycleLedger::s_activeInstances;
        m_shadowMallocAtConstruct = gp::memory::getInlineMalloc();
    }

    virtual ~MockRhiBackendModule() noexcept override
    {
        --ModuleLifecycleLedger::s_activeInstances;
    }

    virtual bool onStartupModule() noexcept override
    {
        m_shadowMallocAtStartup = gp::memory::getInlineMalloc();
        // Exercise gp::Vector allocation inside onStartupModule to verify active shadow Malloc works cleanly.
        m_adapterIds.pushBack(0x10DEu);
        m_adapterIds.pushBack(0x1002u);
        ModuleLifecycleLedger::recordStartup(kBackendTag);
        return true;
    }

    virtual void onShutdownModule() noexcept override
    {
        m_adapterIds.clear();
        ModuleLifecycleLedger::recordShutdown(kBackendTag);
    }

    virtual void onPreUnloadCallback() noexcept override
    {
        ++ModuleLifecycleLedger::s_preUnloadHits;
    }

    virtual void onPostLoadCallback() noexcept override
    {
        ++ModuleLifecycleLedger::s_postLoadHits;
    }

    [[nodiscard]] gp::memory::Malloc* shadowMallocAtConstruct() const noexcept
    {
        return m_shadowMallocAtConstruct;
    }

    [[nodiscard]] gp::memory::Malloc* shadowMallocAtStartup() const noexcept
    {
        return m_shadowMallocAtStartup;
    }

    [[nodiscard]] gp::USize adapterCount() const noexcept
    {
        return m_adapterIds.size();
    }

private:
    gp::memory::Malloc* m_shadowMallocAtConstruct{ nullptr };
    gp::memory::Malloc* m_shadowMallocAtStartup{ nullptr };
    gp::Vector<gp::UInt32> m_adapterIds{};
};

/// @brief Concrete test module representing an Audio backend (`audio/mock_openal`) with ordered lifecycle ID `202`.
class MockAudioBackendModule final : public IModule
{
public:
    static constexpr gp::UInt32 kBackendTag = 202u;

public:
    MockAudioBackendModule() noexcept
    {
        ++ModuleLifecycleLedger::s_activeInstances;
    }

    virtual ~MockAudioBackendModule() noexcept override
    {
        --ModuleLifecycleLedger::s_activeInstances;
    }

    virtual bool onStartupModule() noexcept override
    {
        ModuleLifecycleLedger::recordStartup(kBackendTag);
        return true;
    }

    virtual void onShutdownModule() noexcept override
    {
        ModuleLifecycleLedger::recordShutdown(kBackendTag);
    }
};

/// @brief Non-reloadable gameplay module (`supportsDynamicReloading() == false`, `isGameModule() == true`).
class MockPinnedGameModule final : public IModule
{
public:
    static constexpr gp::UInt32 kBackendTag = 303u;

public:
    virtual bool onStartupModule() noexcept override
    {
        ModuleLifecycleLedger::recordStartup(kBackendTag);
        return true;
    }

    virtual void onShutdownModule() noexcept override
    {
        ModuleLifecycleLedger::recordShutdown(kBackendTag);
    }

    [[nodiscard]] virtual bool supportsDynamicReloading() const noexcept override
    {
        return false;
    }

    [[nodiscard]] virtual bool isGameModule() const noexcept override
    {
        return true;
    }
};

/// @brief Module whose `onStartupModule()` intentionally fails (`returns false`) to verify rollback & cleanup.
class MockFailingStartupModule final : public IModule
{
public:
    MockFailingStartupModule() noexcept
    {
        ++ModuleLifecycleLedger::s_activeInstances;
    }

    virtual ~MockFailingStartupModule() noexcept override
    {
        --ModuleLifecycleLedger::s_activeInstances;
    }

    virtual bool onStartupModule() noexcept override
    {
        return false;
    }
};

/// @brief Global static registrant exercising `.bss` pre-main intrusive registration (`GP_REGISTER_STATIC_MODULE`).
GP_REGISTER_STATIC_MODULE(MockRhiBackendModule, "rhi/static_registered_backend")

}   // namespace

}   // namespace gp::modules::tests

// Export real C-ABI dynamic module entry points (`gpInitializeModule` and `gpDestroyModule`) from the test binary
// itself so `SharedLibrary::getHandle(Paths::executablePath())` exercises the real OS dynamic symbol loader, ABI
// cookie handshake, and image-local `g_localShadowMalloc` binding end-to-end!
GP_IMPLEMENT_DYNAMIC_MODULE_ENTRY_POINTS(gp::modules::tests::MockRhiBackendModule)

namespace gp::modules::tests
{

class ModuleManagerTest : public ::testing::Test
{
protected:
    ModuleManager m_manager{};

protected:
    void SetUp() noexcept override
    {
        ModuleLifecycleLedger::reset();
        m_manager.resetForTesting();
    }

    void TearDown() noexcept override
    {
        m_manager.resetForTesting();
        ModuleLifecycleLedger::reset();
    }
};

TEST_F(ModuleManagerTest, NormalizeModuleNameAndFormatLibraryFilenameUnifyAllConventions)
{
    const gp::Name fromSlash = ModuleManager::normalizeModuleName("rhi/d3d12");
    const gp::Name fromBackslash = ModuleManager::normalizeModuleName("rhi\\d3d12");
    const gp::Name fromUnderscore = ModuleManager::normalizeModuleName("RHI_D3D12");
    const gp::Name fromKebab = ModuleManager::normalizeModuleName("rhi-d3d12");
    const gp::Name fromDllFilename = ModuleManager::normalizeModuleName("gp-rhi-d3d12.dll");
    const gp::Name fromSoFilename = ModuleManager::normalizeModuleName("libgp-rhi-d3d12.so");
    const gp::Name fromDylibFilename = ModuleManager::normalizeModuleName("libgp-rhi-d3d12.dylib");

    EXPECT_FALSE(fromSlash.isNone());
    EXPECT_EQ(fromSlash, fromBackslash);
    EXPECT_EQ(fromSlash, fromUnderscore);
    EXPECT_EQ(fromSlash, fromKebab);
    EXPECT_EQ(fromSlash, fromDllFilename);
    EXPECT_EQ(fromSlash, fromSoFilename);
    EXPECT_EQ(fromSlash, fromDylibFilename);
    EXPECT_EQ(fromSlash.baseView(), gp::StringView("rhi-d3d12"));

    EXPECT_TRUE(ModuleManager::normalizeModuleName("").isNone());
    EXPECT_TRUE(ModuleManager::normalizeModuleName("   ").isNone());
    EXPECT_TRUE(ModuleManager::normalizeModuleName("None").isNone());

    const gp::PathBuffer formattedLib = ModuleManager::formatLibraryFilename("audio/openal");
#if GP_PLATFORM_WINDOWS
    EXPECT_EQ(formattedLib.view(), gp::StringView("gp-audio-openal.dll"));
#elif GP_PLATFORM_MACOS
    EXPECT_EQ(formattedLib.view(), gp::StringView("libgp-audio-openal.dylib"));
#elif GP_PLATFORM_LINUX || GP_PLATFORM_UNIX
    EXPECT_EQ(formattedLib.view(), gp::StringView("libgp-audio-openal.so"));
#endif
}

TEST_F(ModuleManagerTest, LocalShadowGlobalMallocSetupAndEntryHandshakeVerification)
{
    gp::memory::Malloc* const canonicalMalloc = gp::memory::getGlobalMalloc();
    ASSERT_NE(canonicalMalloc, nullptr);

    // Synchronize local shadow Malloc and verify getInlineMalloc() and getActiveMalloc() return it directly.
    gp::memory::Malloc* const syncedMalloc = gp::memory::setupLocalShadowMalloc(canonicalMalloc);
    EXPECT_EQ(syncedMalloc, canonicalMalloc);
    EXPECT_TRUE(gp::memory::isLocalShadowMallocInitialized());
    EXPECT_EQ(gp::memory::getInlineMalloc(), canonicalMalloc);
    EXPECT_EQ(gp::memory::getActiveMalloc(), canonicalMalloc);

    // Execute the entry handshake directly and verify ABI validation + shadow Malloc propagation.
    const ModuleInitContext validCtx{ .abiMagic = kModuleAbiMagic,
                                      .abiVersion = kModuleAbiVersion,
                                      .buildFlags = getCurrentModuleBuildFlags(),
                                      .globalMalloc = canonicalMalloc };
    ModuleInitHandshake handshake{};
    executeModuleEntryHandshake<MockRhiBackendModule>(&validCtx, &handshake);

    EXPECT_EQ(handshake.result, ModuleLoadResult::Success);
    ASSERT_NE(handshake.moduleInstance, nullptr);
    EXPECT_EQ(handshake.boundShadowMalloc, canonicalMalloc);

    const MockRhiBackendModule* const typedModule = static_cast<const MockRhiBackendModule*>(handshake.moduleInstance);
    EXPECT_EQ(typedModule->shadowMallocAtConstruct(), canonicalMalloc);

    executeModuleExitTeardown<MockRhiBackendModule, false>(handshake.moduleInstance);
    EXPECT_EQ(ModuleLifecycleLedger::s_activeInstances, 0u);

    // Mismatched ABI magic, ABI version, or buildFlags must be rejected with AbiVersionMismatch and zero allocations.
    ModuleInitContext badVersionCtx = validCtx;
    badVersionCtx.abiVersion = kModuleAbiVersion + 99u;
    ModuleInitHandshake badHandshake{};
    executeModuleEntryHandshake<MockRhiBackendModule>(&badVersionCtx, &badHandshake);
    EXPECT_EQ(badHandshake.result, ModuleLoadResult::AbiVersionMismatch);
    EXPECT_EQ(badHandshake.moduleInstance, nullptr);

    ModuleInitContext badBuildFlagsCtx = validCtx;
    badBuildFlagsCtx.buildFlags ^= 0xFFu;
    executeModuleEntryHandshake<MockRhiBackendModule>(&badBuildFlagsCtx, &badHandshake);
    EXPECT_EQ(badHandshake.result, ModuleLoadResult::AbiVersionMismatch);
    EXPECT_EQ(badHandshake.moduleInstance, nullptr);
}

TEST_F(ModuleManagerTest, BssIntrusiveStaticRegistrantLoadsAndBindsShadowMalloc)
{
    // "rhi/static_registered_backend" was registered at file scope via GP_REGISTER_STATIC_MODULE.
    EXPECT_TRUE(m_manager.isModuleRegistered("rhi/static_registered_backend"));
    EXPECT_FALSE(m_manager.isModuleLoaded("rhi/static_registered_backend"));

    ModuleLoadResult loadRes = ModuleLoadResult::NotFound;
    MockRhiBackendModule* const rhiModule =
        m_manager.loadModulePtr<MockRhiBackendModule>("rhi/static_registered_backend", &loadRes);
    ASSERT_NE(rhiModule, nullptr);
    EXPECT_EQ(loadRes, ModuleLoadResult::Success);
    EXPECT_TRUE(m_manager.isModuleLoaded("rhi_static_registered_backend"));
    EXPECT_EQ(rhiModule->shadowMallocAtConstruct(), gp::memory::getGlobalMalloc());
    EXPECT_EQ(rhiModule->shadowMallocAtStartup(), gp::memory::getGlobalMalloc());
    EXPECT_EQ(rhiModule->adapterCount(), 2u);

    // Loading again under an alternate naming syntax ("RHI-STATIC-REGISTERED-BACKEND") returns AlreadyLoaded.
    ModuleLoadResult secondRes = ModuleLoadResult::NotFound;
    IModule* const duplicatePtr = m_manager.loadModule("RHI-STATIC-REGISTERED-BACKEND", &secondRes);
    EXPECT_EQ(duplicatePtr, rhiModule);
    EXPECT_EQ(secondRes, ModuleLoadResult::AlreadyLoaded);

    ModuleStatusView status{};
    ASSERT_TRUE(m_manager.queryModuleStatus("rhi/static_registered_backend", status));
    EXPECT_EQ(status.state, ModuleState::Loaded);
    EXPECT_EQ(status.boundShadowMalloc, gp::memory::getGlobalMalloc());
    EXPECT_NE(status.flags & ModuleFlags::IsStaticLinked, ModuleFlags::None);
    EXPECT_NE(status.flags & ModuleFlags::ShadowMallocBound, ModuleFlags::None);
    EXPECT_NE(status.flags & ModuleFlags::SupportsDynamicReloading, ModuleFlags::None);
    EXPECT_EQ(status.flags & ModuleFlags::IsGameModule, ModuleFlags::None);

    EXPECT_EQ(m_manager.unloadModule("rhi/static_registered_backend"), ModuleUnloadResult::Success);
    EXPECT_FALSE(m_manager.isModuleLoaded("rhi/static_registered_backend"));
    EXPECT_EQ(ModuleLifecycleLedger::s_activeInstances, 0u);
}

TEST_F(ModuleManagerTest, RealDynamicSharedLibraryLoadAndEntryHandshakeViaExecutableExports)
{
    const gp::StringView selfBinaryPath = gp::platform::Paths::executablePath();
    if (selfBinaryPath.isEmpty())
    {
        GTEST_SKIP() << "Paths::executablePath() is unavailable on this host.";
    }

    // Verify if the current test executable handle exports gpInitializeModule (always true on Windows with DLLEXPORT).
    void* const probeHandle = gp::platform::SharedLibrary::getHandle(selfBinaryPath);
    if (probeHandle == nullptr)
    {
        GTEST_SKIP() << "SharedLibrary::getHandle(executablePath) is unsupported on this host.";
    }
    void* const probeExport = gp::platform::SharedLibrary::getExport(probeHandle, kInitializeModuleSymbolName);
    gp::platform::SharedLibrary::freeHandle(probeHandle);
    if (probeExport == nullptr)
    {
        GTEST_SKIP() << "Host linker did not export gpInitializeModule from executable image.";
    }

    ModuleDesc dynamicDesc{};
    dynamicDesc.name = "rhi/dynamic_d3d12_test";
    dynamicDesc.explicitLibraryPath = selfBinaryPath;

    ModuleLoadResult loadRes = ModuleLoadResult::NotFound;
    MockRhiBackendModule* const dynModule =
        static_cast<MockRhiBackendModule*>(m_manager.loadModuleWithDesc(dynamicDesc, &loadRes));
    ASSERT_NE(dynModule, nullptr);
    EXPECT_EQ(loadRes, ModuleLoadResult::Success);
    EXPECT_EQ(dynModule->shadowMallocAtConstruct(), gp::memory::getGlobalMalloc());
    EXPECT_EQ(dynModule->shadowMallocAtStartup(), gp::memory::getGlobalMalloc());
    EXPECT_EQ(dynModule->adapterCount(), 2u);

    ModuleStatusView status{};
    ASSERT_TRUE(m_manager.queryModuleStatus("rhi/dynamic_d3d12_test", status));
    EXPECT_EQ(status.state, ModuleState::Loaded);
    EXPECT_NE(status.libraryHandle, nullptr);
    EXPECT_EQ(status.boundShadowMalloc, gp::memory::getGlobalMalloc());
    EXPECT_EQ(status.flags & ModuleFlags::IsStaticLinked, ModuleFlags::None);
    EXPECT_NE(status.flags & ModuleFlags::ShadowMallocBound, ModuleFlags::None);

    // Hot-reload the dynamic module and verify onPreUnloadCallback() and onPostLoadCallback() fire in order.
    ModuleLoadResult reloadRes = ModuleLoadResult::NotFound;
    IModule* const reloadedModule = m_manager.reloadModule("rhi/dynamic_d3d12_test", &reloadRes);
    ASSERT_NE(reloadedModule, nullptr);
    EXPECT_EQ(reloadRes, ModuleLoadResult::Success);
    EXPECT_EQ(ModuleLifecycleLedger::s_preUnloadHits, 1u);
    EXPECT_EQ(ModuleLifecycleLedger::s_postLoadHits, 1u);

    EXPECT_EQ(m_manager.unloadModule("rhi/dynamic_d3d12_test"), ModuleUnloadResult::Success);
    EXPECT_FALSE(m_manager.isModuleLoaded("rhi/dynamic_d3d12_test"));
    EXPECT_EQ(ModuleLifecycleLedger::s_activeInstances, 0u);
}

TEST_F(ModuleManagerTest, ReverseLifoShutdownOrderAndNonReloadableModuleProtection)
{
    ASSERT_TRUE(m_manager.registerStaticModule(
        "rhi/d3d12",
        &StaticModuleRegistrant<MockRhiBackendModule>::createInstance,
        &StaticModuleRegistrant<MockRhiBackendModule>::destroyInstance
    ));
    ASSERT_TRUE(m_manager.registerStaticModule(
        "audio/openal",
        &StaticModuleRegistrant<MockAudioBackendModule>::createInstance,
        &StaticModuleRegistrant<MockAudioBackendModule>::destroyInstance
    ));
    ASSERT_TRUE(m_manager.registerStaticModule(
        "game/sandbox",
        &StaticModuleRegistrant<MockPinnedGameModule>::createInstance,
        &StaticModuleRegistrant<MockPinnedGameModule>::destroyInstance
    ));

    // Load in order: 1) rhi/d3d12 (101), 2) audio/openal (202), 3) game/sandbox (303).
    ASSERT_NE(m_manager.loadModule("rhi/d3d12"), nullptr);
    ASSERT_NE(m_manager.loadModule("audio/openal"), nullptr);
    ASSERT_NE(m_manager.loadModule("game/sandbox"), nullptr);
    EXPECT_EQ(m_manager.getLoadedModuleCount(), 3u);

    ASSERT_EQ(ModuleLifecycleLedger::s_startupCount, 3u);
    EXPECT_EQ(ModuleLifecycleLedger::s_startupSequence[0], MockRhiBackendModule::kBackendTag);
    EXPECT_EQ(ModuleLifecycleLedger::s_startupSequence[1], MockAudioBackendModule::kBackendTag);
    EXPECT_EQ(ModuleLifecycleLedger::s_startupSequence[2], MockPinnedGameModule::kBackendTag);

    // Normal runtime unload of a module where supportsDynamicReloading() == false must be rejected with NotReloadable.
    EXPECT_EQ(m_manager.unloadModule("game/sandbox", ModuleShutdownPhase::Normal), ModuleUnloadResult::NotReloadable);
    EXPECT_TRUE(m_manager.isModuleLoaded("game/sandbox"));

    // unloadAllModules(Exit) must shut down all three in strict reverse-LIFO order: 303 -> 202 -> 101.
    m_manager.unloadAllModules(ModuleShutdownPhase::Exit);
    EXPECT_EQ(m_manager.getLoadedModuleCount(), 0u);

    ASSERT_EQ(ModuleLifecycleLedger::s_shutdownCount, 3u);
    EXPECT_EQ(ModuleLifecycleLedger::s_shutdownSequence[0], MockPinnedGameModule::kBackendTag);
    EXPECT_EQ(ModuleLifecycleLedger::s_shutdownSequence[1], MockAudioBackendModule::kBackendTag);
    EXPECT_EQ(ModuleLifecycleLedger::s_shutdownSequence[2], MockRhiBackendModule::kBackendTag);
}

TEST_F(ModuleManagerTest, RecursiveNestedLoadSucceedsAndCircularDependencyIsCaughtDeterministically)
{
    static ModuleManager* s_activeTestManager = nullptr;
    s_activeTestManager = &m_manager;

    // Module A loads Module B during onStartupModule() (valid DAG edge: A -> B).
    class DependentModuleA final : public IModule
    {
    public:
        virtual bool onStartupModule() noexcept override
        {
            return s_activeTestManager->loadModule("test/dag_leaf_b") != nullptr;
        }
    };

    // Cyclic Module X loads Cyclic Module Y, which attempts to load Cyclic Module X (cycle: X -> Y -> X).
    class CyclicModuleX final : public IModule
    {
    public:
        virtual bool onStartupModule() noexcept override
        {
            return s_activeTestManager->loadModule("test/cycle_y") != nullptr;
        }
    };

    static ModuleLoadResult s_cycleAttemptResult = ModuleLoadResult::Success;
    s_cycleAttemptResult = ModuleLoadResult::Success;

    class CyclicModuleY final : public IModule
    {
    public:
        virtual bool onStartupModule() noexcept override
        {
            IModule* const xPtr = s_activeTestManager->loadModule("test/cycle_x", &s_cycleAttemptResult);
            return xPtr != nullptr;
        }
    };

    ASSERT_TRUE(m_manager.registerStaticModule(
        "test/dag_root_a",
        &StaticModuleRegistrant<DependentModuleA>::createInstance,
        &StaticModuleRegistrant<DependentModuleA>::destroyInstance
    ));
    ASSERT_TRUE(m_manager.registerStaticModule(
        "test/dag_leaf_b",
        &StaticModuleRegistrant<MockAudioBackendModule>::createInstance,
        &StaticModuleRegistrant<MockAudioBackendModule>::destroyInstance
    ));

    ModuleLoadResult dagRes = ModuleLoadResult::NotFound;
    EXPECT_NE(m_manager.loadModule("test/dag_root_a", &dagRes), nullptr);
    EXPECT_EQ(dagRes, ModuleLoadResult::Success);
    EXPECT_TRUE(m_manager.isModuleLoaded("test/dag_root_a"));
    EXPECT_TRUE(m_manager.isModuleLoaded("test/dag_leaf_b"));

    // Now test cycle detection: X -> Y -> X must return CircularDependency without deadlocking!
    ASSERT_TRUE(m_manager.registerStaticModule(
        "test/cycle_x",
        &StaticModuleRegistrant<CyclicModuleX>::createInstance,
        &StaticModuleRegistrant<CyclicModuleX>::destroyInstance
    ));
    ASSERT_TRUE(m_manager.registerStaticModule(
        "test/cycle_y",
        &StaticModuleRegistrant<CyclicModuleY>::createInstance,
        &StaticModuleRegistrant<CyclicModuleY>::destroyInstance
    ));

    s_cycleAttemptResult = ModuleLoadResult::Success;
    ModuleLoadResult outerCycleRes = ModuleLoadResult::Success;
    EXPECT_EQ(m_manager.loadModule("test/cycle_x", &outerCycleRes), nullptr);
    EXPECT_EQ(s_cycleAttemptResult, ModuleLoadResult::CircularDependency);
    EXPECT_EQ(outerCycleRes, ModuleLoadResult::StartupFailed);
    EXPECT_FALSE(m_manager.isModuleLoaded("test/cycle_x"));
    EXPECT_FALSE(m_manager.isModuleLoaded("test/cycle_y"));
}

TEST_F(ModuleManagerTest, FailingStartupAndMissingModuleCleanUpAndBroadcastNotifications)
{
    ASSERT_TRUE(m_manager.registerStaticModule(
        "test/failing_startup",
        &StaticModuleRegistrant<MockFailingStartupModule>::createInstance,
        &StaticModuleRegistrant<MockFailingStartupModule>::destroyInstance
    ));

    gp::UInt32 loadedEvents = 0u;
    gp::UInt32 unloadingEvents = 0u;
    gp::UInt32 unloadedEvents = 0u;
    gp::UInt32 failedEvents = 0u;

    const gp::DelegateHandle subHandle = m_manager.subscribeModuleChanged(
        ModuleChangedDelegate::createLambda(
            [&loadedEvents, &unloadingEvents, &unloadedEvents, &failedEvents](
                [[maybe_unused]] const gp::Name modName, const ModuleChangeReason reason
            ) noexcept -> void
    {
        switch (reason)
        {
        case ModuleChangeReason::Loaded:
            ++loadedEvents;
            break;
        case ModuleChangeReason::Unloading:
            ++unloadingEvents;
            break;
        case ModuleChangeReason::Unloaded:
            ++unloadedEvents;
            break;
        case ModuleChangeReason::LoadFailed:
            ++failedEvents;
            break;
        default:
            break;
        }
    }
        )
    );
    EXPECT_TRUE(subHandle.isValid());

    // 1. Failing startup module must destroy its instance and emit LoadFailed.
    ModuleLoadResult failRes = ModuleLoadResult::Success;
    EXPECT_EQ(m_manager.loadModule("test/failing_startup", &failRes), nullptr);
    EXPECT_EQ(failRes, ModuleLoadResult::StartupFailed);
    EXPECT_EQ(ModuleLifecycleLedger::s_activeInstances, 0u);
    EXPECT_EQ(failedEvents, 1u);

    // 2. Non-existent dynamic module must return NotFound and emit LoadFailed.
    ModuleLoadResult missingRes = ModuleLoadResult::Success;
    EXPECT_EQ(m_manager.loadModule("rhi/definitely_missing_backend_xyz", &missingRes), nullptr);
    EXPECT_EQ(missingRes, ModuleLoadResult::NotFound);
    EXPECT_EQ(failedEvents, 2u);

    // 3. Valid module emits Loaded, Unloading, and Unloaded.
    ASSERT_TRUE(m_manager.registerStaticModule(
        "audio/valid_openal",
        &StaticModuleRegistrant<MockAudioBackendModule>::createInstance,
        &StaticModuleRegistrant<MockAudioBackendModule>::destroyInstance
    ));
    ASSERT_NE(m_manager.loadModule("audio/valid_openal"), nullptr);
    EXPECT_EQ(loadedEvents, 1u);

    EXPECT_EQ(m_manager.unloadModule("audio/valid_openal"), ModuleUnloadResult::Success);
    EXPECT_EQ(unloadingEvents, 1u);
    EXPECT_EQ(unloadedEvents, 1u);

    EXPECT_TRUE(m_manager.unsubscribeModuleChanged(subHandle));
}

TEST_F(ModuleManagerTest, MultiThreadedConcurrentLoadAndQueriesAreRaceFree)
{
    constexpr gp::UInt32 kThreadCount = 8u;
    constexpr gp::UInt32 kIterationsPerThread = 256u;

    ASSERT_TRUE(m_manager.registerStaticModule(
        "rhi/concurrent_d3d12",
        &StaticModuleRegistrant<MockRhiBackendModule>::createInstance,
        &StaticModuleRegistrant<MockRhiBackendModule>::destroyInstance
    ));

    gp::thread::Atomic<bool> startLatch(false);
    gp::thread::Atomic<gp::UInt32> readyThreads(0u);
    gp::thread::Atomic<gp::UInt32> firstLoaderCount(0u);
    gp::thread::Atomic<gp::UInt32> alreadyLoadedCount(0u);
    gp::thread::Atomic<IModule*> firstObservedInstance(nullptr);

    std::vector<std::thread> workers;
    workers.reserve(kThreadCount);

    for (gp::UInt32 t = 0u; t < kThreadCount; ++t)
    {
        workers.emplace_back([this,
                              &startLatch,
                              &readyThreads,
                              &firstLoaderCount,
                              &alreadyLoadedCount,
                              &firstObservedInstance]() noexcept
        {
            readyThreads.fetchAdd(1u, gp::MemoryOrder::AcqRel);
            while (!startLatch.load(gp::MemoryOrder::Acquire))
            {
                std::this_thread::yield();
            }

            for (gp::UInt32 i = 0u; i < kIterationsPerThread; ++i)
            {
                ModuleLoadResult res = ModuleLoadResult::NotFound;
                IModule* const mod = m_manager.loadModule("rhi/concurrent_d3d12", &res);
                EXPECT_NE(mod, nullptr);

                if (res == ModuleLoadResult::Success)
                {
                    firstLoaderCount.fetchAdd(1u, gp::MemoryOrder::Relaxed);
                    firstObservedInstance.store(mod, gp::MemoryOrder::Release);
                }
                else if (res == ModuleLoadResult::AlreadyLoaded)
                {
                    alreadyLoadedCount.fetchAdd(1u, gp::MemoryOrder::Relaxed);
                }

                EXPECT_EQ(m_manager.getModule("rhi_concurrent_d3d12"), mod);
            }
        });
    }

    while (readyThreads.load(gp::MemoryOrder::Acquire) < kThreadCount)
    {
        std::this_thread::yield();
    }
    startLatch.store(true, gp::MemoryOrder::Release);

    for (std::thread& worker: workers)
    {
        worker.join();
    }

    // Exactly ONE thread must have performed the initial load (ModuleLoadResult::Success), and all other
    // (kThreadCount * kIterationsPerThread - 1) calls must have observed ModuleLoadResult::AlreadyLoaded.
    EXPECT_EQ(firstLoaderCount.load(gp::MemoryOrder::Acquire), 1u);
    EXPECT_EQ(alreadyLoadedCount.load(gp::MemoryOrder::Acquire), (kThreadCount * kIterationsPerThread) - 1u);
    EXPECT_NE(firstObservedInstance.load(gp::MemoryOrder::Acquire), nullptr);
    EXPECT_EQ(ModuleLifecycleLedger::s_activeInstances, 1u);
}

TEST_F(ModuleManagerTest, PostOrderTopologicalLoadOrderAndLambdaIteration)
{
    static ModuleManager* s_activeTestManager = nullptr;
    s_activeTestManager = &m_manager;

    static constexpr gp::UInt32 kParentTag = 901u;

    class ParentDependentModule final : public IModule
    {
    public:
        virtual bool onStartupModule() noexcept override
        {
            if (s_activeTestManager->loadModule("audio/child_dependency") == nullptr)
            {
                return false;
            }
            ModuleLifecycleLedger::recordStartup(kParentTag);
            return true;
        }

        virtual void onShutdownModule() noexcept override
        {
            ModuleLifecycleLedger::recordShutdown(kParentTag);
        }
    };

    ASSERT_TRUE(m_manager.registerStaticModule(
        "renderer/parent_dependent",
        &StaticModuleRegistrant<ParentDependentModule>::createInstance,
        &StaticModuleRegistrant<ParentDependentModule>::destroyInstance
    ));
    ASSERT_TRUE(m_manager.registerStaticModule(
        "audio/child_dependency",
        &StaticModuleRegistrant<MockAudioBackendModule>::createInstance,
        &StaticModuleRegistrant<MockAudioBackendModule>::destroyInstance
    ));

    ASSERT_NE(m_manager.loadModule("renderer/parent_dependent"), nullptr);

    ModuleStatusView parentView{};
    ModuleStatusView childView{};
    ASSERT_TRUE(m_manager.queryModuleStatus("renderer/parent_dependent", parentView));
    ASSERT_TRUE(m_manager.queryModuleStatus("audio/child_dependency", childView));

    // Child completed onStartupModule() first, so its post-order loadOrder must be strictly less than Parent's.
    EXPECT_LT(childView.loadOrder, parentView.loadOrder);

    gp::USize visitedLoadedCount = 0u;
    m_manager.iterateModules(
        [&visitedLoadedCount](const ModuleStatusView& status) noexcept -> bool
    {
        if (status.state == ModuleState::Loaded)
        {
            ++visitedLoadedCount;
        }
        return true;
    }
    );
    EXPECT_EQ(visitedLoadedCount, 2u);

    // Reverse-LIFO unloadAllModules() must therefore shut down Parent (901) BEFORE Child (202)!
    m_manager.unloadAllModules(ModuleShutdownPhase::Exit);
    ASSERT_EQ(ModuleLifecycleLedger::s_shutdownCount, 2u);
    EXPECT_EQ(ModuleLifecycleLedger::s_shutdownSequence[0], kParentTag);
    EXPECT_EQ(ModuleLifecycleLedger::s_shutdownSequence[1], MockAudioBackendModule::kBackendTag);
}

TEST_F(ModuleManagerTest, BuiltInModuleConsoleCommandsExecuteAndReportTelemetry)
{
    ModuleManager& globalMgr = ModuleManager::get();
    globalMgr.resetForTesting();

    gp::console::IConsoleManager& console = gp::console::IConsoleManager::get();
    gp::String capturedOutput{};
    gp::console::ConsoleOutputDelegate capture = gp::console::ConsoleOutputDelegate::createLambda(
        [&capturedOutput](const gp::StringView line) noexcept -> void
    {
        capturedOutput.append(line);
        capturedOutput.pushBack('\n');
    }
    );

    EXPECT_EQ(
        console.processUserConsoleInput("module.Load rhi/static_registered_backend", &capture),
        gp::console::ConsoleResult::Success
    );
    EXPECT_TRUE(globalMgr.isModuleLoaded("rhi/static_registered_backend"));
    EXPECT_TRUE(capturedOutput.view().contains("result=Success"));

    capturedOutput.clear();
    EXPECT_EQ(console.processUserConsoleInput("module.Status", &capture), gp::console::ConsoleResult::Success);
    EXPECT_TRUE(capturedOutput.view().contains("ModuleManager: loaded=1"));

    capturedOutput.clear();
    EXPECT_EQ(console.processUserConsoleInput("module.List", &capture), gp::console::ConsoleResult::Success);
    EXPECT_TRUE(capturedOutput.view().contains("rhi-static-registered-backend"));

    capturedOutput.clear();
    EXPECT_EQ(
        console.processUserConsoleInput("module.Reload rhi/static_registered_backend", &capture),
        gp::console::ConsoleResult::Success
    );
    EXPECT_TRUE(capturedOutput.view().contains("result=Success"));

    capturedOutput.clear();
    EXPECT_EQ(
        console.processUserConsoleInput("module.Unload rhi/static_registered_backend", &capture),
        gp::console::ConsoleResult::Success
    );
    EXPECT_FALSE(globalMgr.isModuleLoaded("rhi/static_registered_backend"));
    EXPECT_TRUE(capturedOutput.view().contains("result=Success"));

    globalMgr.resetForTesting();
}

TEST_F(ModuleManagerTest, RealExternalBackendDllLoadBindsShadowMallocAcrossProcessImages)
{
    const gp::PathBuffer rhiNullLib = gp::platform::Paths::combine(
        gp::platform::Paths::executableDir(), ModuleManager::formatLibraryFilename("rhi/null").view()
    );
    if (!gp::platform::FileSystem::isFile(rhiNullLib.view()))
    {
        GTEST_SKIP() << "gp-rhi-null shared library has not been built in executableDir yet.";
    }

    m_manager.addModuleSearchDirectory(gp::platform::Paths::executableDir());

    ModuleLoadResult loadRes = ModuleLoadResult::NotFound;
    IModule* const nullRhiModule = m_manager.loadModule("rhi/null", &loadRes);
    ASSERT_NE(nullRhiModule, nullptr);
    EXPECT_EQ(loadRes, ModuleLoadResult::Success);

    ModuleStatusView status{};
    ASSERT_TRUE(m_manager.queryModuleStatus("rhi/null", status));
    EXPECT_EQ(status.state, ModuleState::Loaded);
    EXPECT_NE(status.libraryHandle, nullptr);
    EXPECT_EQ(status.boundShadowMalloc, gp::memory::getGlobalMalloc());
    EXPECT_NE(status.flags & ModuleFlags::ShadowMallocBound, ModuleFlags::None);
    EXPECT_EQ(status.flags & ModuleFlags::IsStaticLinked, ModuleFlags::None);

    EXPECT_EQ(m_manager.unloadModule("rhi/null"), ModuleUnloadResult::Success);
    EXPECT_FALSE(m_manager.isModuleLoaded("rhi/null"));
}

}   // namespace gp::modules::tests
