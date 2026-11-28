// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "modules/ModuleManager.hpp"
#include "console/AutoConsoleVariable.hpp"
#include "console/ConsoleEnums.hpp"
#include "console/ConsoleTypes.hpp"
#include "containers/strings/CharTraits.hpp"
#include "containers/strings/FixedString.hpp"
#include "logging/LogFormatter.hpp"
#include "logging/Logger.hpp"
#include "logging/LogMacros.hpp"
#include "memory/GlobalMemory.hpp"
#include "platforms/base/Platform.hpp"
#include "platforms/base/system/Paths.hpp"
#include "platforms/base/system/SharedLibrary.hpp"
#include "platforms/base/system/Thread.hpp"
#include "threading/atomics/AtomicFlag.hpp"
#include "threading/sync/SharedLock.hpp"
#include "threading/sync/UniqueLock.hpp"

namespace gp
{

GP_DEFINE_LOG_CATEGORY(LogModules, gp::log::LogLevel::Info, gp::log::LogLevel::Trace);

}   // namespace gp

namespace gp::modules
{

namespace
{

/// @brief Flushes the asynchronous logger before unmapping a dynamically loaded shared library.
/// @details Draining queued log records before `FreeLibrary`/`dlclose` ensures no background logger worker or sink
///          touches memory or resources owned by the outgoing module image.
/// @param[in] libraryHandle Native OS shared-library handle to release.
void flushLoggerAndFreeLibrary(void* const libraryHandle) noexcept
{
    if (libraryHandle == nullptr)
    {
        return;
    }

    gp::log::Logger::instance().flush();
    gp::platform::SharedLibrary::freeHandle(libraryHandle);
}

/// @brief Global `constinit` spinlock protecting the `.bss` intrusive static registrant list before `main()`.
constinit gp::thread::AtomicFlag g_staticRegistrantListLock{};

/// @brief Head pointer of the `.bss` intrusive singly-linked list of `StaticModuleRegistrantNode` instances.
constinit StaticModuleRegistrantNode* g_staticRegistrantListHead = nullptr;

/// @brief Monotonic epoch counter bumped whenever a `StaticModuleRegistrantNode` is linked or unlinked.
/// @details Allows `ModuleManager::drainPendingStaticRegistrants()` to short-circuit in a single lock-free atomic read
///          when no new static module nodes have been added since the manager's last drain.
constinit gp::thread::Atomic<gp::UInt32> g_staticRegistrantEpoch{ 1u };

/// @brief Tracks whether the process-wide `ModuleManager::get()` singleton has indexed `Paths::executableDir()`.
constinit gp::thread::Atomic<bool> g_defaultSearchDirInitialized{ false };

/// @brief RAII scope guard that pushes a parent directory onto `SharedLibrary::pushDirectory()` across OS module
///        loads so co-located third-party dependent libraries resolve from the module's directory.
class ScopedModuleSearchDirectory final
{
public:
    explicit ScopedModuleSearchDirectory(const gp::StringView fileOrDirPath, const bool isFilePath) noexcept
    {
        const gp::StringView dirView = isFilePath ? gp::platform::Paths::getPath(fileOrDirPath) : fileOrDirPath;
        if (!dirView.isEmpty())
        {
            m_directory = gp::PathBuffer(dirView);
            gp::platform::SharedLibrary::pushDirectory(m_directory.view());
            m_isActive = true;
        }
    }

    ~ScopedModuleSearchDirectory() noexcept
    {
        if (m_isActive)
        {
            gp::platform::SharedLibrary::popDirectory(m_directory.view());
        }
    }

    ScopedModuleSearchDirectory(const ScopedModuleSearchDirectory&) = delete;
    ScopedModuleSearchDirectory& operator=(const ScopedModuleSearchDirectory&) = delete;
    ScopedModuleSearchDirectory(ScopedModuleSearchDirectory&&) = delete;
    ScopedModuleSearchDirectory& operator=(ScopedModuleSearchDirectory&&) = delete;

private:
    gp::PathBuffer m_directory{};
    bool m_isActive{ false };
};

/// @brief Acquires the `.bss` static registrant spinlock with processor pause hints.
GP_FORCEINLINE_HINT void lockStaticRegistrantList() noexcept
{
    while (g_staticRegistrantListLock.testAndSet(gp::MemoryOrder::Acquire))
    {
        gp::platform::Thread::spinWait();
    }
}

/// @brief Releases the `.bss` static registrant spinlock.
GP_FORCEINLINE_HINT void unlockStaticRegistrantList() noexcept
{
    g_staticRegistrantListLock.clear(gp::MemoryOrder::Release);
}

/// @brief Strips an optional case-insensitive suffix from `view` if present.
/// @param[in] view Input string view.
/// @param[in] suffix Suffix to strip (e.g., `".dll"`, `".so"`, `".dylib"`).
/// @return Stripped string view.
[[nodiscard]] constexpr gp::StringView
    stripTrailingSuffixIgnoreCase(const gp::StringView view, const gp::StringView suffix) noexcept
{
    if (view.size() > suffix.size() && view.substr(view.size() - suffix.size(), suffix.size()).equalsIgnoreCase(suffix))
    {
        return view.substr(0u, view.size() - suffix.size());
    }
    return view;
}

}   // namespace

void registerStaticModuleNode(StaticModuleRegistrantNode& node) noexcept
{
    if (node.moduleName == nullptr || node.createFn == nullptr || node.destroyFn == nullptr) [[unlikely]]
    {
        return;
    }

    lockStaticRegistrantList();
    if (!node.isRegistered)
    {
        node.next = g_staticRegistrantListHead;
        g_staticRegistrantListHead = &node;
        node.isRegistered = true;
        g_staticRegistrantEpoch.fetchAdd(1u, gp::MemoryOrder::AcqRel);
    }
    unlockStaticRegistrantList();
}

void unregisterStaticModuleNode(StaticModuleRegistrantNode& node) noexcept
{
    lockStaticRegistrantList();
    if (node.isRegistered)
    {
        StaticModuleRegistrantNode** cursor = &g_staticRegistrantListHead;
        while (*cursor != nullptr)
        {
            if (*cursor == &node)
            {
                *cursor = node.next;
                break;
            }
            cursor = &((*cursor)->next);
        }
        node.next = nullptr;
        node.isRegistered = false;
        g_staticRegistrantEpoch.fetchAdd(1u, gp::MemoryOrder::AcqRel);
    }
    unlockStaticRegistrantList();
}

ModuleManager& ModuleManager::get() noexcept
{
    static ModuleManager s_instance{};
    if (!g_defaultSearchDirInitialized.load(gp::MemoryOrder::Acquire)) [[unlikely]]
    {
        const gp::StringView exeDir = gp::platform::Paths::executableDir();
        if (!exeDir.isEmpty())
        {
            gp::platform::SharedLibrary::addDirectory(exeDir);
        }
        (void)gp::memory::setupLocalShadowMalloc();
        g_defaultSearchDirInitialized.store(true, gp::MemoryOrder::Release);
    }
    s_instance.drainPendingStaticRegistrants();
    return s_instance;
}

gp::Name
    ModuleManager::normalizeModuleName(const gp::StringView rawName, gp::PathBuffer* const outCanonicalBuffer) noexcept
{
    if (outCanonicalBuffer != nullptr)
    {
        outCanonicalBuffer->clear();
    }

    gp::StringView trimmed = rawName.trim();
    if (trimmed.isEmpty() || trimmed.equalsIgnoreCase("None"))
    {
        return gp::Name{};
    }

    // Strip known shared library file extensions.
    trimmed = stripTrailingSuffixIgnoreCase(trimmed, ".dll");
    trimmed = stripTrailingSuffixIgnoreCase(trimmed, ".so");
    trimmed = stripTrailingSuffixIgnoreCase(trimmed, ".dylib");

    // Strip optional GPBT shared library prefixes ("libgp-" or "gp-").
    if (trimmed.size() > 6u &&
        (trimmed.substr(0u, 6u).equalsIgnoreCase("libgp-") || trimmed.substr(0u, 6u).equalsIgnoreCase("libgp_")))
    {
        trimmed = trimmed.substr(6u);
    }
    else if (
        trimmed.size() > 3u &&
        (trimmed.substr(0u, 3u).equalsIgnoreCase("gp-") || trimmed.substr(0u, 3u).equalsIgnoreCase("gp_"))
    )
    {
        trimmed = trimmed.substr(3u);
    }

    gp::PathBuffer normalized{};
    bool lastWasHyphen = true;
    for (gp::USize i = 0u; i < trimmed.size(); ++i)
    {
        const char ch = trimmed[i];
        if (ch == '/' || ch == '\\' || ch == '_' || ch == '-' || ch == '.')
        {
            if (!lastWasHyphen && !normalized.isEmpty())
            {
                normalized.pushBack('-');
                lastWasHyphen = true;
            }
        }
        else if (!gp::container::CharTraits<char>::isWhitespace(ch))
        {
            normalized.pushBack(gp::container::CharTraits<char>::toLower(ch));
            lastWasHyphen = false;
        }
    }

    while (!normalized.isEmpty() && normalized[normalized.size() - 1u] == '-')
    {
        normalized.popBack();
    }

    if (normalized.isEmpty() || normalized.view().equalsIgnoreCase("None"))
    {
        return gp::Name{};
    }

    if (outCanonicalBuffer != nullptr)
    {
        *outCanonicalBuffer = normalized;
    }

    return gp::Name(normalized.view());
}

gp::PathBuffer ModuleManager::formatLibraryFilename(const gp::StringView rawName) noexcept
{
    gp::PathBuffer canonicalBuf{};
    const gp::Name canonicalName = normalizeModuleName(rawName, &canonicalBuf);
    if (canonicalName.isNone() || canonicalBuf.isEmpty())
    {
        return {};
    }

    gp::PathBuffer filename{};
#if GP_PLATFORM_WINDOWS
    filename.append("gp-");
    filename.append(canonicalBuf.view());
    filename.append(".dll");
#elif GP_PLATFORM_MACOS
    filename.append("libgp-");
    filename.append(canonicalBuf.view());
    filename.append(".dylib");
#elif GP_PLATFORM_LINUX || GP_PLATFORM_UNIX
    filename.append("libgp-");
    filename.append(canonicalBuf.view());
    filename.append(".so");
#else
    filename.append("gp-");
    filename.append(canonicalBuf.view());
    filename.append(gp::platform::SharedLibrary::getDefaultExtension());
#endif
    return filename;
}

ModuleManager::~ModuleManager() noexcept
{
    unloadAllModules(ModuleShutdownPhase::Exit);
}

void ModuleManager::addModuleSearchDirectory(const gp::StringView directory) noexcept
{
    if (!directory.isEmpty())
    {
        gp::platform::SharedLibrary::addDirectory(directory);
    }
}

void ModuleManager::drainPendingStaticRegistrants() noexcept
{
    // Lock-free fast path: skip spinlock and write-lock when the global static registrant list epoch has not changed.
    const gp::UInt32 targetEpoch = g_staticRegistrantEpoch.load(gp::MemoryOrder::Acquire);
    if (m_syncedStaticRegistrantEpoch.load(gp::MemoryOrder::Acquire) == targetEpoch) [[likely]]
    {
        return;
    }

    struct EntrySnapshot final
    {
        const char* moduleName{ nullptr };
        StaticModuleFactoryFn createFn{ nullptr };
        StaticModuleDestroyFn destroyFn{ nullptr };
    };

    EntrySnapshot snapshots[kMaxRegisteredModules]{};
    gp::USize snapshotCount = 0u;

    lockStaticRegistrantList();
    const gp::UInt32 lockedEpoch = g_staticRegistrantEpoch.load(gp::MemoryOrder::Relaxed);
    const StaticModuleRegistrantNode* cursor = g_staticRegistrantListHead;
    while (cursor != nullptr && snapshotCount < kMaxRegisteredModules)
    {
        snapshots[snapshotCount].moduleName = cursor->moduleName;
        snapshots[snapshotCount].createFn = cursor->createFn;
        snapshots[snapshotCount].destroyFn = cursor->destroyFn;
        ++snapshotCount;
        cursor = cursor->next;
    }
    unlockStaticRegistrantList();

    gp::thread::UniqueLock<gp::thread::CacheAlignedSharedMutex> writeLock(m_mutex);
    for (gp::USize i = 0u; i < snapshotCount; ++i)
    {
        const gp::Name canonicalName = normalizeModuleName(gp::StringView(snapshots[i].moduleName));
        if (canonicalName.isNone())
        {
            continue;
        }

        ModuleSlot* const slot = findOrAllocateSlot(canonicalName);
        if (slot == nullptr)
        {
            continue;
        }

        slot->staticCreateFn = snapshots[i].createFn;
        slot->staticDestroyFn = snapshots[i].destroyFn;
        slot->flags |= ModuleFlags::IsStaticLinked;
        if (slot->state == ModuleState::Unloaded)
        {
            slot->state = ModuleState::Registered;
        }
    }
    m_syncedStaticRegistrantEpoch.store(lockedEpoch, gp::MemoryOrder::Release);
}

bool ModuleManager::registerStaticModule(
    const gp::StringView name, const StaticModuleFactoryFn createFn, const StaticModuleDestroyFn destroyFn
) noexcept
{
    if (createFn == nullptr || destroyFn == nullptr)
    {
        return false;
    }

    const gp::Name canonicalName = normalizeModuleName(name);
    if (canonicalName.isNone())
    {
        return false;
    }

    gp::thread::UniqueLock<gp::thread::CacheAlignedSharedMutex> writeLock(m_mutex);
    ModuleSlot* const slot = findOrAllocateSlot(canonicalName);
    if (slot == nullptr)
    {
        return false;
    }

    slot->staticCreateFn = createFn;
    slot->staticDestroyFn = destroyFn;
    slot->flags |= ModuleFlags::IsStaticLinked;
    if (slot->state == ModuleState::Unloaded)
    {
        slot->state = ModuleState::Registered;
    }
    return true;
}

bool ModuleManager::unregisterStaticModule(const gp::StringView name) noexcept
{
    const gp::Name canonicalName = normalizeModuleName(name);
    if (canonicalName.isNone())
    {
        return false;
    }

    gp::thread::UniqueLock<gp::thread::CacheAlignedSharedMutex> writeLock(m_mutex);
    ModuleSlot* const slot = findSlotMutable(canonicalName);
    if (slot == nullptr || slot->state == ModuleState::Loaded || slot->state == ModuleState::Loading ||
        slot->state == ModuleState::Unloading)
    {
        return false;
    }

    slot->staticCreateFn = nullptr;
    slot->staticDestroyFn = nullptr;
    slot->flags &= ~ModuleFlags::IsStaticLinked;
    if (slot->libraryHandle == nullptr && slot->instance == nullptr)
    {
        *slot = ModuleSlot{};
    }
    return true;
}

IModule* ModuleManager::loadModule(const gp::StringView name, ModuleLoadResult* const outResult) noexcept
{
    ModuleDesc desc{};
    desc.name = name;
    return loadModuleWithDesc(desc, outResult);
}

IModule* ModuleManager::loadModuleWithDesc(const ModuleDesc& desc, ModuleLoadResult* const outResult) noexcept
{
    const auto setResult = [outResult](const ModuleLoadResult code) noexcept -> void
    {
        if (outResult != nullptr)
        {
            *outResult = code;
        }
    };

    if (m_isShuttingDown.load(gp::MemoryOrder::Acquire)) [[unlikely]]
    {
        setResult(ModuleLoadResult::ShuttingDown);
        return nullptr;
    }

    drainPendingStaticRegistrants();

    gp::PathBuffer canonicalStr{};
    const gp::Name canonicalName = normalizeModuleName(desc.name, &canonicalStr);
    if (canonicalName.isNone())
    {
        setResult(ModuleLoadResult::InvalidName);
        return nullptr;
    }

    const gp::platform::ThreadId callerThreadId = gp::platform::Thread::currentThreadId();

    // Fast shared-read check if already loaded.
    {
        gp::thread::SharedLock<gp::thread::CacheAlignedSharedMutex> readLock(m_mutex);
        const ModuleSlot* const existing = findSlot(canonicalName);
        if (existing != nullptr && existing->state == ModuleState::Loaded && existing->instance != nullptr)
        {
            setResult(ModuleLoadResult::AlreadyLoaded);
            return existing->instance;
        }
    }

    StaticModuleFactoryFn staticCreateFn = nullptr;
    StaticModuleDestroyFn staticDestroyFn = nullptr;
    gp::PathBuffer explicitPathSnapshot{};

    // Acquire exclusive lock to claim the Loading transition for this slot.
    // If another thread is concurrently loading the same module, spin-yield outside the lock until it finishes.
    for (;;)
    {
        gp::thread::UniqueLock<gp::thread::CacheAlignedSharedMutex> writeLock(m_mutex);
        if (m_isShuttingDown.load(gp::MemoryOrder::Acquire)) [[unlikely]]
        {
            setResult(ModuleLoadResult::ShuttingDown);
            return nullptr;
        }

        ModuleSlot* const slot = findOrAllocateSlot(canonicalName);
        if (slot == nullptr) [[unlikely]]
        {
            setResult(ModuleLoadResult::ShuttingDown);
            return nullptr;
        }

        if (slot->state == ModuleState::Loaded && slot->instance != nullptr)
        {
            setResult(ModuleLoadResult::AlreadyLoaded);
            return slot->instance;
        }

        if (slot->state == ModuleState::Loading)
        {
            // Same-thread re-entrant load of a module currently in Loading -> deterministic cycle detection!
            if (slot->transitionThreadId == callerThreadId)
            {
                slot->lastLoadResult = ModuleLoadResult::CircularDependency;
                setResult(ModuleLoadResult::CircularDependency);
                GP_LOG_ERROR(
                    LogModules,
                    "Circular module dependency detected while loading '{}' on thread {}.",
                    canonicalStr.view(),
                    callerThreadId
                );
                return nullptr;
            }

            // Another worker thread is loading this module; release lock and yield before re-checking.
            writeLock.unlock();
            gp::platform::Thread::yieldExecution();
            continue;
        }

        if (slot->state == ModuleState::Unloading)
        {
            writeLock.unlock();
            gp::platform::Thread::yieldExecution();
            continue;
        }

        if (!desc.explicitLibraryPath.isEmpty())
        {
            slot->explicitLibraryPath = gp::PathBuffer(desc.explicitLibraryPath);
        }
        slot->flags |= desc.flags;
        slot->state = ModuleState::Loading;
        slot->transitionThreadId = callerThreadId;

        staticCreateFn = slot->staticCreateFn;
        staticDestroyFn = slot->staticDestroyFn;
        explicitPathSnapshot = slot->explicitLibraryPath;
        break;
    }

    // Execute OS SharedLibrary loading, ABI handshake, shadow Malloc binding, and onStartupModule()
    // COMPLETELY OUTSIDE m_mutex so:
    //  1. We never invert lock ordering against Windows LdrpLoaderLock / POSIX dlopen mutex.
    //  2. onStartupModule() can recursively load dependent modules via ModuleManager.
    gp::memory::Malloc* const hostMalloc = gp::memory::setupLocalShadowMalloc();
    IModule* createdInstance = nullptr;
    void* loadedLibHandle = nullptr;
    DestroyModuleFn dynamicDestroyFn = nullptr;
    gp::memory::Malloc* boundShadowMalloc = nullptr;
    gp::PathBuffer resolvedPath{};
    ModuleFlags resolvedFlags = desc.flags;
    ModuleLoadResult loadStatus = ModuleLoadResult::Success;

    const bool preferDynamicPath = !explicitPathSnapshot.isEmpty();
    if (staticCreateFn != nullptr && staticDestroyFn != nullptr && !preferDynamicPath)
    {
        createdInstance = staticCreateFn(hostMalloc, &boundShadowMalloc);
        if (createdInstance == nullptr)
        {
            loadStatus = ModuleLoadResult::InstantiationFailed;
        }
        else
        {
            resolvedFlags |= ModuleFlags::IsStaticLinked;
            if (boundShadowMalloc != nullptr && boundShadowMalloc == hostMalloc)
            {
                resolvedFlags |= ModuleFlags::ShadowMallocBound;
            }
        }
    }
    else
    {
        // Resolve shared library via explicit path override, GPBT canonical filename ("gp-<name>.dll"), or bare name.
        if (!explicitPathSnapshot.isEmpty())
        {
            const ScopedModuleSearchDirectory scopedDir(explicitPathSnapshot.view(), true);
            resolvedPath = explicitPathSnapshot;
            loadedLibHandle = gp::platform::SharedLibrary::getHandle(explicitPathSnapshot.view());
        }
        else
        {
            const gp::PathBuffer gpbtFilename = formatLibraryFilename(canonicalStr.view());
            resolvedPath = gpbtFilename;
            loadedLibHandle = gp::platform::SharedLibrary::getHandle(gpbtFilename.view());

            if (loadedLibHandle == nullptr)
            {
                gp::PathBuffer bareGpbtName("gp-");
                bareGpbtName.append(canonicalStr.view());
                loadedLibHandle = gp::platform::SharedLibrary::getHandle(bareGpbtName.view());
                if (loadedLibHandle != nullptr)
                {
                    resolvedPath = bareGpbtName;
                }
            }

            if (loadedLibHandle == nullptr)
            {
                loadedLibHandle = gp::platform::SharedLibrary::getHandle(canonicalStr.view());
                if (loadedLibHandle != nullptr)
                {
                    resolvedPath = canonicalStr;
                }
            }
        }

        if (loadedLibHandle == nullptr)
        {
            loadStatus =
                !explicitPathSnapshot.isEmpty() ? ModuleLoadResult::LibraryLoadFailed : ModuleLoadResult::NotFound;
        }
        else
        {
            void* const rawInitSym =
                gp::platform::SharedLibrary::getExport(loadedLibHandle, kInitializeModuleSymbolName);
            void* const rawDestroySym =
                gp::platform::SharedLibrary::getExport(loadedLibHandle, kDestroyModuleSymbolName);

            if (rawInitSym == nullptr || rawDestroySym == nullptr)
            {
                flushLoggerAndFreeLibrary(loadedLibHandle);
                loadedLibHandle = nullptr;
                loadStatus = ModuleLoadResult::EntryPointMissing;
            }
            else
            {
                const InitializeModuleFn initFn = reinterpret_cast<InitializeModuleFn>(rawInitSym);
                dynamicDestroyFn = reinterpret_cast<DestroyModuleFn>(rawDestroySym);

                const ModuleInitContext initCtx{ .abiMagic = kModuleAbiMagic,
                                                 .abiVersion = kModuleAbiVersion,
                                                 .buildFlags = getCurrentModuleBuildFlags(),
                                                 .globalMalloc = hostMalloc };
                ModuleInitHandshake handshake{};
                initFn(&initCtx, &handshake);

                // Verify against POSIX dlsym breadth-first dependency fallthrough when loading by canonical name.
                const bool hasDeclaredName =
                    (handshake.declaredModuleName != nullptr && handshake.declaredModuleName[0] != '\0');
                const bool declaredNameMismatch =
                    explicitPathSnapshot.isEmpty() && hasDeclaredName &&
                    (normalizeModuleName(gp::StringView(handshake.declaredModuleName)) != canonicalName);

                if (declaredNameMismatch)
                {
                    if (handshake.moduleInstance != nullptr)
                    {
                        dynamicDestroyFn(handshake.moduleInstance);
                    }
                    flushLoggerAndFreeLibrary(loadedLibHandle);
                    loadedLibHandle = nullptr;
                    dynamicDestroyFn = nullptr;
                    loadStatus = ModuleLoadResult::EntryPointMissing;
                }
                else if (
                    handshake.abiMagic != kModuleAbiMagic || handshake.abiVersion != kModuleAbiVersion ||
                    handshake.buildFlags != initCtx.buildFlags ||
                    handshake.result == ModuleLoadResult::AbiVersionMismatch
                )
                {
                    if (handshake.moduleInstance != nullptr)
                    {
                        dynamicDestroyFn(handshake.moduleInstance);
                    }
                    flushLoggerAndFreeLibrary(loadedLibHandle);
                    loadedLibHandle = nullptr;
                    dynamicDestroyFn = nullptr;
                    loadStatus = ModuleLoadResult::AbiVersionMismatch;
                }
                else if (handshake.result != ModuleLoadResult::Success || handshake.moduleInstance == nullptr)
                {
                    flushLoggerAndFreeLibrary(loadedLibHandle);
                    loadedLibHandle = nullptr;
                    dynamicDestroyFn = nullptr;
                    loadStatus = ModuleLoadResult::InstantiationFailed;
                }
                else
                {
                    createdInstance = handshake.moduleInstance;
                    boundShadowMalloc = handshake.boundShadowMalloc;
                    resolvedFlags &= ~ModuleFlags::IsStaticLinked;
                    if (boundShadowMalloc != nullptr && boundShadowMalloc == hostMalloc)
                    {
                        resolvedFlags |= ModuleFlags::ShadowMallocBound;
                    }
                }
            }
        }
    }

    // Invoke onStartupModule() if instantiation succeeded.
    if (loadStatus == ModuleLoadResult::Success && createdInstance != nullptr)
    {
        if (!createdInstance->onStartupModule())
        {
            if (dynamicDestroyFn != nullptr)
            {
                dynamicDestroyFn(createdInstance);
            }
            else if (staticDestroyFn != nullptr)
            {
                staticDestroyFn(createdInstance);
            }

            if (loadedLibHandle != nullptr)
            {
                flushLoggerAndFreeLibrary(loadedLibHandle);
                loadedLibHandle = nullptr;
            }

            createdInstance = nullptr;
            dynamicDestroyFn = nullptr;
            boundShadowMalloc = nullptr;
            loadStatus = ModuleLoadResult::StartupFailed;
        }
        else
        {
            if (createdInstance->supportsDynamicReloading())
            {
                resolvedFlags |= ModuleFlags::SupportsDynamicReloading;
            }
            else
            {
                resolvedFlags &= ~ModuleFlags::SupportsDynamicReloading;
            }

            if (createdInstance->isGameModule())
            {
                resolvedFlags |= ModuleFlags::IsGameModule;
            }
            else
            {
                resolvedFlags &= ~ModuleFlags::IsGameModule;
            }
        }
    }

    // Commit final state back into the slot under exclusive lock.
    // Assigning loadOrder AFTER onStartupModule() completes guarantees post-order topological numbering:
    // any child dependency loaded inside onStartupModule() receives a lower loadOrder than its parent dependent.
    gp::UInt32 assignedLoadOrder = 0u;
    {
        gp::thread::UniqueLock<gp::thread::CacheAlignedSharedMutex> writeLock(m_mutex);
        ModuleSlot* const slot = findSlotMutable(canonicalName);
        if (slot != nullptr)
        {
            slot->transitionThreadId = 0ULL;
            slot->lastLoadResult = loadStatus;
            if (loadStatus == ModuleLoadResult::Success && createdInstance != nullptr)
            {
                assignedLoadOrder = m_nextLoadOrder.fetchAdd(1u, gp::MemoryOrder::AcqRel);
                slot->instance = createdInstance;
                slot->libraryHandle = loadedLibHandle;
                slot->dynamicDestroyFn = dynamicDestroyFn;
                slot->boundShadowMalloc = boundShadowMalloc;
                slot->resolvedLibraryPath = resolvedPath;
                slot->flags = resolvedFlags;
                slot->loadOrder = assignedLoadOrder;
                slot->state = ModuleState::Loaded;
            }
            else
            {
                slot->instance = nullptr;
                slot->libraryHandle = nullptr;
                slot->dynamicDestroyFn = nullptr;
                slot->boundShadowMalloc = nullptr;
                slot->loadOrder = 0u;
                slot->state = ModuleState::Failed;
            }
        }
    }

    if (loadStatus == ModuleLoadResult::Success && createdInstance != nullptr)
    {
        GP_LOG_DEBUG(
            LogModules,
            "Loaded module '{}' (order={} mode={} shadowMallocBound={}).",
            canonicalStr.view(),
            assignedLoadOrder,
            (resolvedFlags & ModuleFlags::IsStaticLinked) != ModuleFlags::None ? "Static" : "Dynamic",
            (resolvedFlags & ModuleFlags::ShadowMallocBound) != ModuleFlags::None
        );
        broadcastChangeUnlocked(canonicalName, ModuleChangeReason::Loaded);
        setResult(ModuleLoadResult::Success);
        return createdInstance;
    }

    GP_LOG_WARNING(
        LogModules, "Failed to load module '{}' (reason={}).", canonicalStr.view(), toStringView(loadStatus)
    );
    broadcastChangeUnlocked(canonicalName, ModuleChangeReason::LoadFailed);
    setResult(loadStatus);
    return nullptr;
}

ModuleUnloadResult ModuleManager::unloadModule(const gp::StringView name, const ModuleShutdownPhase phase) noexcept
{
    gp::PathBuffer canonicalStr{};
    const gp::Name canonicalName = normalizeModuleName(name, &canonicalStr);
    if (canonicalName.isNone())
    {
        return ModuleUnloadResult::InvalidName;
    }

    IModule* instanceToDestroy = nullptr;
    void* libraryHandleToFree = nullptr;
    DestroyModuleFn dynamicDestroyFn = nullptr;
    StaticModuleDestroyFn staticDestroyFn = nullptr;
    bool keepStaticRegistration = false;

    {
        gp::thread::UniqueLock<gp::thread::CacheAlignedSharedMutex> writeLock(m_mutex);
        ModuleSlot* const slot = findSlotMutable(canonicalName);
        if (slot == nullptr || (slot->state != ModuleState::Loaded && slot->state != ModuleState::Loading &&
                                slot->state != ModuleState::Unloading))
        {
            return ModuleUnloadResult::NotLoaded;
        }

        if (slot->state == ModuleState::Loading || slot->state == ModuleState::Unloading)
        {
            return ModuleUnloadResult::InUseOrLoading;
        }

        if (phase != ModuleShutdownPhase::Exit && slot->instance != nullptr &&
            !slot->instance->supportsDynamicReloading())
        {
            return ModuleUnloadResult::NotReloadable;
        }

        slot->state = ModuleState::Unloading;
        slot->transitionThreadId = gp::platform::Thread::currentThreadId();

        instanceToDestroy = slot->instance;
        libraryHandleToFree = slot->libraryHandle;
        dynamicDestroyFn = slot->dynamicDestroyFn;
        staticDestroyFn = slot->staticDestroyFn;
        keepStaticRegistration = (slot->staticCreateFn != nullptr && slot->staticDestroyFn != nullptr);
    }

    // Broadcast Unloading and invoke onShutdownModule() outside m_mutex.
    broadcastChangeUnlocked(canonicalName, ModuleChangeReason::Unloading);

    if (instanceToDestroy != nullptr)
    {
        if (phase == ModuleShutdownPhase::HotReload)
        {
            instanceToDestroy->onPreUnloadCallback();
        }
        instanceToDestroy->onShutdownModule();

        if (dynamicDestroyFn != nullptr)
        {
            dynamicDestroyFn(instanceToDestroy);
        }
        else if (staticDestroyFn != nullptr)
        {
            staticDestroyFn(instanceToDestroy);
        }
    }

    if (libraryHandleToFree != nullptr)
    {
        flushLoggerAndFreeLibrary(libraryHandleToFree);
    }

    {
        gp::thread::UniqueLock<gp::thread::CacheAlignedSharedMutex> writeLock(m_mutex);
        ModuleSlot* const slot = findSlotMutable(canonicalName);
        if (slot != nullptr)
        {
            slot->instance = nullptr;
            slot->libraryHandle = nullptr;
            slot->dynamicDestroyFn = nullptr;
            slot->boundShadowMalloc = nullptr;
            slot->transitionThreadId = 0ULL;
            slot->loadOrder = 0u;
            slot->flags &= ~ModuleFlags::ShadowMallocBound;
            slot->state = keepStaticRegistration ? ModuleState::Registered : ModuleState::Unloaded;
        }
    }

    GP_LOG_DEBUG(LogModules, "Unloaded module '{}' (phase={}).", canonicalStr.view(), static_cast<gp::UInt32>(phase));
    broadcastChangeUnlocked(canonicalName, ModuleChangeReason::Unloaded);
    return ModuleUnloadResult::Success;
}

IModule* ModuleManager::reloadModule(const gp::StringView name, ModuleLoadResult* const outLoadResult) noexcept
{
    const auto setResult = [outLoadResult](const ModuleLoadResult code) noexcept -> void
    {
        if (outLoadResult != nullptr)
        {
            *outLoadResult = code;
        }
    };

    const gp::Name canonicalName = normalizeModuleName(name);
    if (canonicalName.isNone())
    {
        setResult(ModuleLoadResult::InvalidName);
        return nullptr;
    }

    ModuleDesc reloadDesc{};
    reloadDesc.name = name;
    gp::PathBuffer preservedExplicitPath{};

    {
        gp::thread::SharedLock<gp::thread::CacheAlignedSharedMutex> readLock(m_mutex);
        const ModuleSlot* const slot = findSlot(canonicalName);
        if (slot != nullptr)
        {
            preservedExplicitPath = slot->explicitLibraryPath;
        }
    }

    if (isModuleLoaded(name))
    {
        const ModuleUnloadResult unloadRes = unloadModule(name, ModuleShutdownPhase::HotReload);
        if (unloadRes != ModuleUnloadResult::Success)
        {
            setResult(ModuleLoadResult::StartupFailed);
            return nullptr;
        }
    }

    reloadDesc.explicitLibraryPath = preservedExplicitPath.view();
    IModule* const reloadedInstance = loadModuleWithDesc(reloadDesc, outLoadResult);
    if (reloadedInstance != nullptr)
    {
        reloadedInstance->onPostLoadCallback();
    }
    return reloadedInstance;
}

void ModuleManager::unloadAllModules(const ModuleShutdownPhase phase) noexcept
{
    bool expectedShuttingDown = false;
    if (!m_isShuttingDown.compareExchangeStrong(
            expectedShuttingDown, true, gp::MemoryOrder::AcqRel, gp::MemoryOrder::Acquire
        ))
    {
        return;
    }

    struct OrderPair final
    {
        gp::Name name{};
        gp::UInt32 loadOrder{ 0u };
    };

    // Repeatedly snapshot remaining loaded modules and unload the highest loadOrder first (strict reverse-LIFO)
    // until zero loaded modules remain.
    for (gp::USize pass = 0u; pass < kMaxRegisteredModules; ++pass)
    {
        OrderPair highest{};
        {
            gp::thread::SharedLock<gp::thread::CacheAlignedSharedMutex> readLock(m_mutex);
            for (gp::USize i = 0u; i < kMaxRegisteredModules; ++i)
            {
                const ModuleSlot& slot = m_slots[i];
                if (slot.isOccupied && slot.state == ModuleState::Loaded && slot.loadOrder >= highest.loadOrder)
                {
                    highest.name = slot.canonicalName;
                    highest.loadOrder = slot.loadOrder;
                }
            }
        }

        if (highest.name.isNone())
        {
            break;
        }

        (void)unloadModule(highest.name.baseView(), phase);
    }

    m_isShuttingDown.store(false, gp::MemoryOrder::Release);
}

void ModuleManager::resetForTesting() noexcept
{
    unloadAllModules(ModuleShutdownPhase::Exit);

    {
        gp::thread::UniqueLock<gp::thread::CacheAlignedSharedMutex> writeLock(m_mutex);
        for (gp::USize i = 0u; i < kMaxRegisteredModules; ++i)
        {
            m_slots[i] = ModuleSlot{};
        }
        m_nextLoadOrder.store(1u, gp::MemoryOrder::Release);
        m_syncedStaticRegistrantEpoch.store(0u, gp::MemoryOrder::Release);
    }

    {
        gp::thread::UniqueLock<gp::thread::CacheAlignedSharedMutex> delegateLock(m_delegateMutex);
        m_onModuleChanged.clear();
    }
}

IModule* ModuleManager::getModule(const gp::StringView name) const noexcept
{
    const gp::Name canonicalName = normalizeModuleName(name);
    if (canonicalName.isNone())
    {
        return nullptr;
    }

    gp::thread::SharedLock<gp::thread::CacheAlignedSharedMutex> readLock(m_mutex);
    const ModuleSlot* const slot = findSlot(canonicalName);
    if (slot != nullptr && slot->state == ModuleState::Loaded)
    {
        return slot->instance;
    }
    return nullptr;
}

bool ModuleManager::isModuleLoaded(const gp::StringView name) const noexcept
{
    return getModule(name) != nullptr;
}

bool ModuleManager::isModuleRegistered(const gp::StringView name) noexcept
{
    drainPendingStaticRegistrants();

    const gp::Name canonicalName = normalizeModuleName(name);
    if (canonicalName.isNone())
    {
        return false;
    }

    gp::thread::SharedLock<gp::thread::CacheAlignedSharedMutex> readLock(m_mutex);
    return findSlot(canonicalName) != nullptr;
}

bool ModuleManager::queryModuleStatus(const gp::StringView name, ModuleStatusView& outStatus) const noexcept
{
    outStatus = ModuleStatusView{};
    const gp::Name canonicalName = normalizeModuleName(name);
    if (canonicalName.isNone())
    {
        return false;
    }

    gp::thread::SharedLock<gp::thread::CacheAlignedSharedMutex> readLock(m_mutex);
    const ModuleSlot* const slot = findSlot(canonicalName);
    if (slot == nullptr)
    {
        return false;
    }

    populateStatusView(*slot, outStatus);
    return true;
}

gp::USize ModuleManager::getLoadedModuleCount() const noexcept
{
    gp::thread::SharedLock<gp::thread::CacheAlignedSharedMutex> readLock(m_mutex);
    gp::USize count = 0u;
    for (gp::USize i = 0u; i < kMaxRegisteredModules; ++i)
    {
        if (m_slots[i].isOccupied && m_slots[i].state == ModuleState::Loaded)
        {
            ++count;
        }
    }
    return count;
}

gp::USize ModuleManager::getRegisteredModuleCount() noexcept
{
    drainPendingStaticRegistrants();

    gp::thread::SharedLock<gp::thread::CacheAlignedSharedMutex> readLock(m_mutex);
    gp::USize count = 0u;
    for (gp::USize i = 0u; i < kMaxRegisteredModules; ++i)
    {
        if (m_slots[i].isOccupied)
        {
            ++count;
        }
    }
    return count;
}

void ModuleManager::iterateModules(const ModuleVisitorFn visitor, void* const userData) const noexcept
{
    if (visitor == nullptr)
    {
        return;
    }

    ModuleStatusView snapshots[kMaxRegisteredModules]{};
    gp::USize snapshotCount = 0u;

    {
        gp::thread::SharedLock<gp::thread::CacheAlignedSharedMutex> readLock(m_mutex);
        for (gp::USize i = 0u; i < kMaxRegisteredModules; ++i)
        {
            if (m_slots[i].isOccupied)
            {
                populateStatusView(m_slots[i], snapshots[snapshotCount]);
                ++snapshotCount;
            }
        }
    }

    for (gp::USize i = 0u; i < snapshotCount; ++i)
    {
        if (!visitor(snapshots[i], userData))
        {
            break;
        }
    }
}

gp::DelegateHandle ModuleManager::subscribeModuleChanged(ModuleChangedDelegate callback) noexcept
{
    gp::thread::UniqueLock<gp::thread::CacheAlignedSharedMutex> delegateLock(m_delegateMutex);
    return m_onModuleChanged.add(std::move(callback));
}

bool ModuleManager::unsubscribeModuleChanged(const gp::DelegateHandle handle) noexcept
{
    gp::thread::UniqueLock<gp::thread::CacheAlignedSharedMutex> delegateLock(m_delegateMutex);
    return m_onModuleChanged.remove(handle);
}

ModuleManager::ModuleSlot* ModuleManager::findSlotMutable(const gp::Name canonicalName) noexcept
{
    for (gp::USize i = 0u; i < kMaxRegisteredModules; ++i)
    {
        if (m_slots[i].isOccupied && m_slots[i].canonicalName == canonicalName)
        {
            return &m_slots[i];
        }
    }
    return nullptr;
}

const ModuleManager::ModuleSlot* ModuleManager::findSlot(const gp::Name canonicalName) const noexcept
{
    for (gp::USize i = 0u; i < kMaxRegisteredModules; ++i)
    {
        if (m_slots[i].isOccupied && m_slots[i].canonicalName == canonicalName)
        {
            return &m_slots[i];
        }
    }
    return nullptr;
}

ModuleManager::ModuleSlot* ModuleManager::findOrAllocateSlot(const gp::Name canonicalName) noexcept
{
    ModuleSlot* const existing = findSlotMutable(canonicalName);
    if (existing != nullptr)
    {
        return existing;
    }

    for (gp::USize i = 0u; i < kMaxRegisteredModules; ++i)
    {
        if (!m_slots[i].isOccupied)
        {
            m_slots[i] = ModuleSlot{};
            m_slots[i].isOccupied = true;
            m_slots[i].canonicalName = canonicalName;
            return &m_slots[i];
        }
    }
    return nullptr;
}

void ModuleManager::populateStatusView(const ModuleSlot& slot, ModuleStatusView& outView) noexcept
{
    outView.name = slot.canonicalName;
    outView.libraryPath = !slot.resolvedLibraryPath.isEmpty() ? slot.resolvedLibraryPath : slot.explicitLibraryPath;
    outView.instance = slot.instance;
    outView.libraryHandle = slot.libraryHandle;
    outView.boundShadowMalloc = slot.boundShadowMalloc;
    outView.loadOrder = slot.loadOrder;
    outView.state = slot.state;
    outView.flags = slot.flags;
    outView.lastLoadResult = slot.lastLoadResult;
}

void ModuleManager::broadcastChangeUnlocked(const gp::Name canonicalName, const ModuleChangeReason reason) noexcept
{
    ModuleChangedMulticastDelegate snapshot{};
    {
        gp::thread::SharedLock<gp::thread::CacheAlignedSharedMutex> delegateLock(m_delegateMutex);
        if (!m_onModuleChanged.isBound())
        {
            return;
        }
        snapshot = m_onModuleChanged;
    }
    snapshot.broadcast(canonicalName, reason);
}

namespace
{

/// @brief Built-in `module.Status` console command reporting module registry and shadow `Malloc` telemetry.
gp::console::AutoConsoleCommand g_cmdModuleStatus(
    "module.Status",
    "Prints ModuleManager loaded/registered counts, ABI version, and shadow Malloc state.",
    [](const gp::console::ConsoleCommandArgs& args) noexcept -> void
{
    ModuleManager& mgr = ModuleManager::get();
    gp::FixedString<383> line{};
    gp::log::formatTo(
        line,
        "ModuleManager: loaded={} registered={}/{} abiVersion={} buildFlags=0x{:02X} shadowMallocInit={}",
        mgr.getLoadedModuleCount(),
        mgr.getRegisteredModuleCount(),
        kMaxRegisteredModules,
        kModuleAbiVersion,
        getCurrentModuleBuildFlags(),
        gp::memory::isLocalShadowMallocInitialized()
    );
    args.writeLine(line.view());
},
    gp::console::ConsoleVarFlags::None
);

/// @brief Built-in `module.List` console command printing all registered and loaded modules.
gp::console::AutoConsoleCommand g_cmdModuleList(
    "module.List",
    "Lists all occupied module slots with state, loadOrder, flags, and binary path.",
    [](const gp::console::ConsoleCommandArgs& args) noexcept -> void
{
    ModuleManager& mgr = ModuleManager::get();
    mgr.drainPendingStaticRegistrants();
    gp::USize count = 0u;
    mgr.iterateModules(
        [&args, &count](const ModuleStatusView& view) noexcept -> void
    {
        const bool isStatic = (view.flags & ModuleFlags::IsStaticLinked) != ModuleFlags::None;
        const bool isShadowOk = (view.flags & ModuleFlags::ShadowMallocBound) != ModuleFlags::None;
        const bool isReloadable = (view.flags & ModuleFlags::SupportsDynamicReloading) != ModuleFlags::None;
        gp::FixedString<511> line{};
        gp::log::formatTo(
            line,
            "  [{}] name='{}' state={} order={} linkage={} shadowMalloc={} reloadable={} path='{}'",
            count,
            view.name.baseView(),
            toStringView(view.state),
            view.loadOrder,
            isStatic ? "Static" : "Dynamic",
            isShadowOk,
            isReloadable,
            view.libraryPath.view()
        );
        args.writeLine(line.view());
        ++count;
    }
    );

    gp::FixedString<127> footer{};
    gp::log::formatTo(footer, "Total modules: {} (loaded={})", count, mgr.getLoadedModuleCount());
    args.writeLine(footer.view());
},
    gp::console::ConsoleVarFlags::None
);

/// @brief Built-in `module.Load` console command to dynamically load a module by name.
gp::console::AutoConsoleCommand g_cmdModuleLoad(
    "module.Load",
    "Loads an engine or backend module by name. Usage: module.Load <name> (e.g., module.Load rhi/null)",
    [](const gp::console::ConsoleCommandArgs& args) noexcept -> void
{
    if (args.getArgCount() < 1u)
    {
        args.writeLine("Usage: module.Load <name>  (e.g., module.Load rhi/null)");
        return;
    }
    const gp::StringView modName = args.getArg(0u);
    ModuleLoadResult loadRes = ModuleLoadResult::NotFound;
    IModule* const instance = ModuleManager::get().loadModule(modName, &loadRes);
    gp::FixedString<255> line{};
    gp::log::formatTo(
        line, "module.Load '{}': result={} loaded={}", modName, toStringView(loadRes), instance != nullptr
    );
    args.writeLine(line.view());
},
    gp::console::ConsoleVarFlags::None
);

/// @brief Built-in `module.Unload` console command to unload a loaded module by name.
gp::console::AutoConsoleCommand g_cmdModuleUnload(
    "module.Unload",
    "Unloads an active module by name. Usage: module.Unload <name>",
    [](const gp::console::ConsoleCommandArgs& args) noexcept -> void
{
    if (args.getArgCount() < 1u)
    {
        args.writeLine("Usage: module.Unload <name>  (e.g., module.Unload rhi/null)");
        return;
    }
    const gp::StringView modName = args.getArg(0u);
    const ModuleUnloadResult unloadRes = ModuleManager::get().unloadModule(modName, ModuleShutdownPhase::Normal);
    gp::FixedString<255> line{};
    gp::log::formatTo(line, "module.Unload '{}': result={}", modName, toStringView(unloadRes));
    args.writeLine(line.view());
},
    gp::console::ConsoleVarFlags::None
);

/// @brief Built-in `module.Reload` console command to hot-reload a module by name.
gp::console::AutoConsoleCommand g_cmdModuleReload(
    "module.Reload",
    "Hot-reloads an active module by name. Usage: module.Reload <name>",
    [](const gp::console::ConsoleCommandArgs& args) noexcept -> void
{
    if (args.getArgCount() < 1u)
    {
        args.writeLine("Usage: module.Reload <name>  (e.g., module.Reload rhi/null)");
        return;
    }
    const gp::StringView modName = args.getArg(0u);
    ModuleLoadResult reloadRes = ModuleLoadResult::NotFound;
    IModule* const instance = ModuleManager::get().reloadModule(modName, &reloadRes);
    gp::FixedString<255> line{};
    gp::log::formatTo(
        line, "module.Reload '{}': result={} loaded={}", modName, toStringView(reloadRes), instance != nullptr
    );
    args.writeLine(line.view());
},
    gp::console::ConsoleVarFlags::None
);

}   // namespace

}   // namespace gp::modules
