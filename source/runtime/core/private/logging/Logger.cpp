// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "logging/Logger.hpp"
#include "memory/Memory.hpp"
#include "miscellaneous/CommandLine.hpp"
#include "platforms/base/system/Thread.hpp"
#include "platforms/base/system/Time.hpp"
#include "threading/fibers/Fiber.hpp"

namespace gp::log
{

namespace detail
{

void onInvalidLogFormatString([[maybe_unused]] const char* const reason) noexcept
{
    GP_ASSERT(false && "Invalid log format string.");
}

}   // namespace detail

Logger::Logger() noexcept
{
    (void)m_asyncQueue.initializeWithExternalBuffer(m_ringSlots, kDefaultAsyncLogQueueCapacity);
}

Logger::~Logger() noexcept
{
    shutdown();
}

Logger& Logger::instance() noexcept
{
    static Logger s_instance{};
    return s_instance;
}

LogResult Logger::initialize(const LoggerDesc& desc) noexcept
{
    if (m_isInitialized.load(gp::MemoryOrder::Acquire))
    {
        return LogResult::AlreadyInitialized;
    }

    resetStats();

    LogExecutionMode targetMode = desc.mode;
    LogLevel targetMinLevel = desc.globalMinLevel;

    if (desc.applyCommandLineOverrides && gp::CommandLine::isInitialized())
    {
        if (gp::CommandLine::hasArgument("log-sync"))
        {
            targetMode = LogExecutionMode::Synchronous;
        }
        else if (gp::CommandLine::hasArgument("log-async"))
        {
            targetMode = LogExecutionMode::AsyncRing;
        }

        const gp::Optional<gp::StringView> cliLevel = gp::CommandLine::tryGetString("log-level");
        if (cliLevel.hasValue())
        {
            LogLevel parsedLevel = targetMinLevel;
            if (tryParseLogLevel(cliLevel.value(), parsedLevel))
            {
                targetMinLevel = parsedLevel;
            }
        }
    }

    m_globalMinLevel.store(targetMinLevel, gp::MemoryOrder::Relaxed);
    m_overflowPolicy.store(desc.overflowPolicy, gp::MemoryOrder::Relaxed);
    m_asyncFlushIntervalMicroseconds =
        (desc.asyncFlushIntervalMicroseconds > 0u) ? desc.asyncFlushIntervalMicroseconds : 1000u;
    m_stopWorkerRequested.store(false, gp::MemoryOrder::Release);

    if (!m_asyncQueue.isValid())
    {
        (void)m_asyncQueue.initializeWithExternalBuffer(m_ringSlots, kDefaultAsyncLogQueueCapacity);
    }

    if (targetMode == LogExecutionMode::AsyncRing)
    {
        gp::platform::ThreadDesc threadDesc{};
        threadDesc.name = "GP_LogFlushWorker";
        threadDesc.entry = &Logger::workerThreadEntry;
        threadDesc.userData = this;
        threadDesc.priority = gp::platform::ThreadPriority::BelowNormal;

        const gp::platform::ThreadResult threadRes = m_workerThread.start(threadDesc);
        if (threadRes != gp::platform::ThreadResult::Success)
        {
            m_mode.store(LogExecutionMode::Synchronous, gp::MemoryOrder::Release);
            return LogResult::ThreadCreationFailed;
        }
    }

    m_mode.store(targetMode, gp::MemoryOrder::Release);
    m_isInitialized.store(true, gp::MemoryOrder::Release);
    return LogResult::Success;
}

void Logger::shutdown() noexcept
{
    if (m_workerThread.isJoinable())
    {
        m_stopWorkerRequested.store(true, gp::MemoryOrder::Release);
        (void)m_workerThread.join();
    }

    m_mode.store(LogExecutionMode::Synchronous, gp::MemoryOrder::Release);
    flush();
    m_isInitialized.store(false, gp::MemoryOrder::Release);
}

LogResult Logger::addSink(ILogSink* const sink) noexcept
{
    if (sink == nullptr)
    {
        return LogResult::InvalidArguments;
    }

    const gp::platform::ThreadId callerTid = gp::platform::Thread::currentThreadId();
    acquireDispatchLock(callerTid);
    drainAsyncQueueLocked();

    const gp::UInt32 currentCount = m_sinkCount.load(gp::MemoryOrder::Relaxed);
    for (gp::UInt32 i = 0u; i < currentCount; ++i)
    {
        if (m_sinks[i] == sink)
        {
            releaseDispatchLock();
            return LogResult::Success;
        }
    }

    if (currentCount >= kMaxLogSinks)
    {
        releaseDispatchLock();
        return LogResult::SinkCapacityExceeded;
    }

    m_sinks[currentCount] = sink;
    m_sinkCount.store(currentCount + 1u, gp::MemoryOrder::Release);
    releaseDispatchLock();
    return LogResult::Success;
}

LogResult Logger::removeSink(ILogSink* const sink) noexcept
{
    if (sink == nullptr)
    {
        return LogResult::InvalidArguments;
    }

    const gp::platform::ThreadId callerTid = gp::platform::Thread::currentThreadId();
    acquireDispatchLock(callerTid);
    drainAsyncQueueLocked();

    const gp::UInt32 currentCount = m_sinkCount.load(gp::MemoryOrder::Relaxed);
    for (gp::UInt32 i = 0u; i < currentCount; ++i)
    {
        if (m_sinks[i] == sink)
        {
            for (gp::UInt32 j = i; j + 1u < currentCount; ++j)
            {
                m_sinks[j] = m_sinks[j + 1u];
            }
            m_sinks[currentCount - 1u] = nullptr;
            m_sinkCount.store(currentCount - 1u, gp::MemoryOrder::Release);
            releaseDispatchLock();
            return LogResult::Success;
        }
    }

    releaseDispatchLock();
    return LogResult::SinkNotFound;
}

void Logger::clearSinks() noexcept
{
    const gp::platform::ThreadId callerTid = gp::platform::Thread::currentThreadId();
    acquireDispatchLock(callerTid);
    drainAsyncQueueLocked();
    flushSinksLocked();

    const gp::UInt32 currentCount = m_sinkCount.load(gp::MemoryOrder::Relaxed);
    for (gp::UInt32 i = 0u; i < currentCount; ++i)
    {
        m_sinks[i] = nullptr;
    }
    m_sinkCount.store(0u, gp::MemoryOrder::Release);
    releaseDispatchLock();
}

void Logger::submit(
    const LogCategory& category, const LogLevel level, const LogSourceLocation& location, const gp::StringView message
) noexcept
{
    if (!shouldLog(category, level))
    {
        return;
    }

    const gp::platform::ThreadId callerThreadId = gp::platform::Thread::currentThreadId();

    // Prevent self-deadlock if a custom ILogSink::write() recursively invokes Logger::submit() on the same thread.
    if (callerThreadId != 0ULL && m_dispatchLockOwnerThreadId.load(gp::MemoryOrder::Relaxed) == callerThreadId)
        [[unlikely]]
    {
        m_droppedRecordCount.fetchAdd(1ULL, gp::MemoryOrder::Relaxed);
        return;
    }

    m_totalSubmittedCount.fetchAdd(1ULL, gp::MemoryOrder::Relaxed);

    const gp::UInt64 nowNs = gp::platform::Time::nowNanoseconds();
    const gp::platform::DateTime nowDt = gp::platform::Time::localDateTime();
    const gp::thread::FiberId callerFiberId = gp::thread::Fiber::getCurrentFiberId();
    const gp::Name internedCategory(category.getName());

    const LogExecutionMode activeMode = m_mode.load(gp::MemoryOrder::Acquire);
    const bool isFatal = (level == LogLevel::Fatal);

    if (activeMode == LogExecutionMode::Synchronous || isFatal)
    {
        LogRecordView record{};
        record.timestamp = gp::time::Timestamp(nowNs);
        record.dateTime = nowDt;
        record.category = internedCategory;
        record.categoryName = category.getName();
        record.message = message;
        record.location = location;
        record.threadId = callerThreadId;
        record.fiberId = callerFiberId;
        record.level = level;

        acquireDispatchLock(callerThreadId);
        drainAsyncQueueLocked();
        dispatchRecordLocked(record);
        if (isFatal)
        {
            flushSinksLocked();
        }
        releaseDispatchLock();
        return;
    }

    // AsyncRing path: pack a self-contained 512-byte LogEntry and enqueue lock-free.
    LogEntry entry{};
    entry.timestampNs = nowNs;
    entry.threadId = callerThreadId;
    entry.category = internedCategory;
    entry.categoryLiteral = category.getNameCString();
    entry.categoryLength = category.getNameLength();
    entry.file = location.file;
    entry.function = location.function;
    entry.line = location.line;
    entry.fiberId = callerFiberId;
    entry.dateTime = nowDt;
    entry.level = level;

    const gp::USize clampedLen = (message.size() <= kMaxLogMessageLength) ? message.size() : kMaxLogMessageLength;
    if (clampedLen > 0u)
    {
        (void)gp::memory::copyMemory(entry.message, message.data(), clampedLen);
    }
    entry.message[clampedLen] = '\0';
    entry.messageLength = static_cast<gp::UInt16>(clampedLen);

    if (m_asyncQueue.tryPush(entry) == gp::thread::JobQueueResult::Success)
    {
        return;
    }

    // Queue full backpressure handling.
    const LogOverflowPolicy policy = m_overflowPolicy.load(gp::MemoryOrder::Relaxed);
    if (policy == LogOverflowPolicy::DropOldestNonCritical &&
        static_cast<gp::UInt8>(level) < static_cast<gp::UInt8>(LogLevel::Error))
    {
        m_droppedRecordCount.fetchAdd(1ULL, gp::MemoryOrder::Relaxed);
        return;
    }

    acquireDispatchLock(callerThreadId);
    drainAsyncQueueLocked();
    const LogRecordView fallbackView = entry.toView();
    dispatchRecordLocked(fallbackView);
    releaseDispatchLock();
}

void Logger::flush() noexcept
{
    const gp::platform::ThreadId callerTid = gp::platform::Thread::currentThreadId();
    if (callerTid != 0ULL && m_dispatchLockOwnerThreadId.load(gp::MemoryOrder::Relaxed) == callerTid) [[unlikely]]
    {
        return;
    }

    acquireDispatchLock(callerTid);
    drainAsyncQueueLocked();
    flushSinksLocked();
    releaseDispatchLock();
}

gp::UInt32 Logger::workerThreadEntry(void* const userData) noexcept
{
    Logger* const self = static_cast<Logger*>(userData);
    if (self != nullptr)
    {
        self->workerThreadLoop();
    }
    return 0u;
}

void Logger::workerThreadLoop() noexcept
{
    const gp::UInt64 sleepNs = static_cast<gp::UInt64>(m_asyncFlushIntervalMicroseconds) * 1000ULL;
    const gp::platform::ThreadId workerTid = gp::platform::Thread::currentThreadId();

    while (!m_stopWorkerRequested.load(gp::MemoryOrder::Acquire))
    {
        if (!m_asyncQueue.isEmptyApprox())
        {
            acquireDispatchLock(workerTid);
            drainAsyncQueueLocked();
            releaseDispatchLock();
        }
        else
        {
            gp::platform::Time::sleepNanoseconds(sleepNs);
        }
    }

    acquireDispatchLock(workerTid);
    drainAsyncQueueLocked();
    flushSinksLocked();
    releaseDispatchLock();
}

void Logger::drainAsyncQueueLocked() noexcept
{
    LogEntry entry{};
    while (m_asyncQueue.tryPop(entry) == gp::thread::JobQueueResult::Success)
    {
        const LogRecordView view = entry.toView();
        dispatchRecordLocked(view);
    }
}

void Logger::dispatchRecordLocked(const LogRecordView& record) noexcept
{
    const gp::UInt32 count = m_sinkCount.load(gp::MemoryOrder::Relaxed);
    for (gp::UInt32 i = 0u; i < count; ++i)
    {
        ILogSink* const sink = m_sinks[i];
        if (sink != nullptr && sink->shouldWrite(record.level))
        {
            sink->write(record);
        }
    }
    m_totalDispatchedCount.fetchAdd(1ULL, gp::MemoryOrder::Relaxed);
}

void Logger::flushSinksLocked() noexcept
{
    const gp::UInt32 count = m_sinkCount.load(gp::MemoryOrder::Relaxed);
    for (gp::UInt32 i = 0u; i < count; ++i)
    {
        ILogSink* const sink = m_sinks[i];
        if (sink != nullptr)
        {
            sink->flush();
        }
    }
}

bool Logger::tryAcquireDispatchLock(const gp::platform::ThreadId callerThreadId) noexcept
{
    if (!m_dispatchSpinLock.testAndSet(gp::MemoryOrder::Acquire))
    {
        m_dispatchLockOwnerThreadId.store(callerThreadId, gp::MemoryOrder::Relaxed);
        return true;
    }
    return false;
}

void Logger::acquireDispatchLock(const gp::platform::ThreadId callerThreadId) noexcept
{
    while (!tryAcquireDispatchLock(callerThreadId))
    {
        gp::platform::Thread::spinWait();
    }
}

void Logger::releaseDispatchLock() noexcept
{
    m_dispatchLockOwnerThreadId.store(0ULL, gp::MemoryOrder::Relaxed);
    m_dispatchSpinLock.clear(gp::MemoryOrder::Release);
}

}   // namespace gp::log
