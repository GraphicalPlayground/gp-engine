// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "errors/ErrorSystem.hpp"
#include "container/BasicString.hpp"       // IWYU pragma: keep
#include "container/BasicStringView.hpp"   // IWYU pragma: keep
#include "container/Forward.hpp"
#include "CoreMinimal.hpp"
#include "errors/ErrorSeverity.hpp"
#include "errors/sinks/ConsoleSink.hpp"
#include "memory/UniquePtr.hpp"
#if GP_PLATFORM_WINDOWS
    #include <windows.h>
#elif GP_PLATFORM_LINUX || GP_PLATFORM_APPLE
    #include <execinfo.h>
#endif
#include <csignal>

namespace gp::error
{

std::atomic<ErrorSystem*> ErrorSystem::s_instance{ nullptr };
std::mutex ErrorSystem::s_initMutex;

ErrorSystem::ErrorSystem(ErrorSystemConfig config)
    : m_config(std::move(config))
    , m_rootSink(gp::makeUnique<MultiSink>())
    , m_initTime(std::chrono::steady_clock::now())
{
    if (m_config.addDefaultConsoleSink)
    {
        auto console = std::make_shared<ConsoleSink>(/*useAnsiColor=*/true);
        console->setMinSeverity(m_config.filter.globalMinSeverity);
        m_rootSink->addSink(std::move(console));
    }

    if (!m_config.defaultLogFilePath.isEmpty())
    {
        // try
        // {
        //     auto file = std::make_shared<FileSink>(m_config.defaultLogFilePath);
        //     file->setMinSeverity(m_config.filter.globalMinSeverity);
        //     m_rootSink->addSink(std::move(file));
        // }
        // catch (const std::exception& e)
        // {
        //     std::fprintf(stderr, "[GP ErrorSystem] WARNING: Could not open log file: %s\n", e.what());
        // }
    }

#if GP_BUILD_DEBUG
    {
        //     auto bp = std::make_shared<BreakpointSink>(m_config.breakpoint.breakFrom);
        //     m_rootSink->addSink(std::move(bp));
    }
#endif

    {
        // auto abort_sink = std::make_shared<AbortSink>(m_config.abort.abortFrom,
        //     m_config.abort.useTerminate ? AbortSink::Mode::Terminate : AbortSink::Mode::Abort);
        // m_rootSink->addSink(std::move(abort_sink));
    }
}

ErrorSystem::~ErrorSystem()
{
    m_rootSink->flush();
}

void ErrorSystem::initialize(ErrorSystemConfig config)
{
    std::lock_guard lock(s_initMutex);

    if (s_instance.load(std::memory_order_acquire) != nullptr)
    {
        ErrorSystem::dispatch(
            Severity::Warning,
            codes::kAlreadyExists,
            "ErrorSystem::initialize() called more than once, ignoring.",
            std::source_location::current()
        );
        return;
    }

    auto* instance = new ErrorSystem(std::move(config));
    s_instance.store(instance, std::memory_order_release);

    if (instance->m_config.printBannerOnInit)
    {
        // TODO: Replace with internal logging once available
        std::fprintf(
            stderr,
            "\033[36m[GP ErrorSystem] Initialized - build: %s %s" " | stacktrace: %s | globalMin: %s\033[0m\n",
            __DATE__,
            __TIME__,
#if GP_HAS_STACKTRACE
            instance->m_config.stacktrace.enabled ? "on" : "off",
#else
            "n/a",
#endif
            getSeverityDisplay(instance->m_config.filter.globalMinSeverity).data()
        );
    }
}

void ErrorSystem::shutdown()
{
    std::lock_guard lock(s_initMutex);
    ErrorSystem* instance = s_instance.exchange(nullptr, std::memory_order_acq_rel);
    if (instance)
    {
        instance->m_rootSink->flush();
        delete instance;
    }
}

bool ErrorSystem::isInitialized() noexcept
{
    return s_instance.load(std::memory_order_acquire) != nullptr;
}

void ErrorSystem::addSink(std::shared_ptr<Sink> sink)
{
    if (auto* instance = s_instance.load(std::memory_order_acquire))
    {
        instance->m_rootSink->addSink(std::move(sink));
    }
}

void ErrorSystem::removeSink(const gp::String& name)
{
    if (auto* instance = s_instance.load(std::memory_order_acquire))
    {
        instance->m_rootSink->removeSink(name);
    }
}

void ErrorSystem::clearSinks()
{
    if (auto* instance = s_instance.load(std::memory_order_acquire))
    {
        instance->m_rootSink = gp::makeUnique<MultiSink>();
    }
}

void ErrorSystem::flushAll()
{
    if (auto* instance = s_instance.load(std::memory_order_acquire))
    {
        instance->m_rootSink->flush();
    }
}

GP_NODISCARD MultiSink& ErrorSystem::rootSink()
{
    return *s_instance.load(std::memory_order_acquire)->m_rootSink;
}

const ErrorSystemConfig& ErrorSystem::config() noexcept
{
    return s_instance.load(std::memory_order_acquire)->m_config;
}

void ErrorSystem::setGlobalMinSeverity(Severity severity) noexcept
{
    if (auto* inst = s_instance.load(std::memory_order_acquire))
    {
        inst->m_config.filter.globalMinSeverity = severity;
    }
}

const ErrorStatistics& ErrorSystem::stats() noexcept
{
    return s_instance.load(std::memory_order_acquire)->m_stats;
}

void ErrorSystem::resetStats() noexcept
{
    if (auto* instance = s_instance.load(std::memory_order_acquire))
    {
        for (auto& counter: instance->m_stats.counts)
        {
            counter.store(0, std::memory_order_relaxed);
        }
    }
}

void ErrorSystem::dispatch(
    Severity severity,
    ErrorCode code,
    gp::String message,
    std::source_location location,
    std::shared_ptr<const ErrorRecord> cause
)
{
    ErrorSystem* inst = s_instance.load(std::memory_order_acquire);
    if (!inst)
    {
        // Fallback: write to stderr if system not initialized
        std::fprintf(
            stderr,
            "[GP] [%s] %s (%s:%u)\n",
            getSeverityDisplay(severity).data(),
            message.cStr(),
            location.file_name(),
            location.line()
        );
        return;
    }
    inst->dispatchImpl(severity, code, std::move(message), location, std::move(cause));
}

void ErrorSystem::dispatchImpl(
    Severity severity,
    ErrorCode code,
    gp::String message,
    std::source_location location,
    std::shared_ptr<const ErrorRecord> cause
)
{
    if (severity < m_config.filter.globalMinSeverity)
    {
        return;
    }

    if (m_config.filter.deduplicate)
    {
        // std::size_t hashIndex = std::hash<gp::String>{}(message) ^ (std::hash<gp::UInt32>{}(code.raw()) << 17u);
        // auto now = std::chrono::steady_clock::now();
        // {
        //     std::lock_guard lock(m_dedupMutex);
        //     auto it = m_dedupTable.find(hashIndex);
        //     if (it != m_dedupTable.end())
        //     {
        //         auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second).count();
        //         if (static_cast<std::size_t>(elapsed) < m_config.filter.dedupWindowMs)
        //         {
        //             m_stats.totalDropped.fetch_add(1, std::memory_order_relaxed);
        //             return;
        //         }
        //     }
        //     m_dedupTable[hashIndex] = now;
        // }
    }

    auto record = std::make_shared<ErrorRecord>();
    record->severity = severity;
    record->code = code;
    record->message = std::move(message);
    record->location = location;
    record->cause = std::move(cause);
    record->engineTime = std::chrono::steady_clock::now();
    record->wallTime = std::chrono::system_clock::now();
    record->threadId = std::this_thread::get_id();
    record->threadName = ErrorContext::current().threadName();

    {
        gp::StringView sub = ErrorContext::current().currentSubsystem();
        if (!sub.isEmpty())
        {
            record->subsystem = gp::String(sub);
        }
    }

    {
        // MetaBag ctxMeta = ErrorContext::current().flatten();
        // record->metadata.insert(
        //     record->metadata.end(), std::make_move_iterator(ctxMeta.begin()), std::make_move_iterator(ctxMeta.end())
        // );
    }

#if GP_HAS_STACKTRACE
    if (m_config.stacktrace.enabled && severity >= m_config.stacktrace.captureFrom)
    {
        record->stacktrace = std::stacktrace::current(m_config.stacktrace.skipFrames, m_config.stacktrace.maxFrames);
    }
#endif

    m_stats.increment(severity);

    m_rootSink->dispatch(*record);

    if (m_config.abort.flushBeforeAbort && severity >= Severity::Fatal)
    {
        m_rootSink->flush();
    }
}

bool ErrorSystem::shouldDedup(const DedupKey&)
{
    // Full implementation in dispatchImpl.
    return false;
}

void ErrorSystem::onSignal(int sig)
{
    const char* name = "SIGUNKNOWN";
    switch (sig)
    {
#if defined(SIGSEGV)
    case SIGSEGV:
        name = "SIGSEGV (segmentation fault)";
        break;
#endif
#if defined(SIGABRT)
    case SIGABRT:
        name = "SIGABRT (abort)";
        break;
#endif
#if defined(SIGFPE)
    case SIGFPE:
        name = "SIGFPE (floating-point exception)";
        break;
#endif
#if defined(SIGILL)
    case SIGILL:
        name = "SIGILL (illegal instruction)";
        break;
#endif
#if defined(SIGBUS)
    case SIGBUS:
        name = "SIGBUS (bus error)";
        break;
#endif
    }

    ErrorSystem::dispatch(
        Severity::Critical,
        codes::kUnknown,
        gp::String::format("Fatal signal received: {} ({})", name, sig),
        std::source_location::current()
    );

    std::signal(sig, SIG_DFL);
    std::raise(sig);
}

void ErrorSystem::installSignalHandlers()
{
#if GP_PLATFORM_WINDOWS || GP_PLATFORM_LINUX || GP_PLATFORM_APPLE
    std::signal(SIGSEGV, onSignal);
    std::signal(SIGABRT, onSignal);
    std::signal(SIGFPE, onSignal);
    std::signal(SIGILL, onSignal);
    #if defined(SIGBUS)
    std::signal(SIGBUS, onSignal);
    #endif
#endif
}

}   // namespace gp::error
