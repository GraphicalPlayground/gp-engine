// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "logging/ILogSink.hpp"
#include "logging/LogCategory.hpp"
#include "logging/LogDesc.hpp"
#include "logging/LogEnums.hpp"
#include "logging/LogFormatter.hpp"
#include "logging/LogForward.hpp"
#include "logging/LogTypes.hpp"
#include "memory/HardwareConstants.hpp"
#include "memory/MemoryOrder.hpp"
#include "platforms/base/system/Thread.hpp"
#include "threading/atomics/Atomic.hpp"
#include "threading/atomics/AtomicFlag.hpp"
#include "threading/queues/MPMCRingQueue.hpp"

namespace gp::log
{

/// @brief Central zero-allocation, lock-free multi-sink logger router ([ADR-0054]).
/// @details Supports two execution modes:
///          1. **`Synchronous` Mode (Default Pre-Init, Unit Tests, `-log-sync`, and `LogLevel::Fatal`):**
///             Formats records on the calling stack and broadcasts directly to registered `ILogSink` instances under
///             an atomic spinlock with same-thread re-entrancy protection.
///          2. **`AsyncRing` Mode (Default Runtime Mode after `initialize()`):**
///             Formats records into a stack-resident `LogEntry` and enqueues into an inline, `.bss`-backed lock-free
///             `MPMCRingQueue<LogEntry>` (`256` slots, zero heap allocations). A background consumer thread drains the
///             queue and dispatches records to all registered sinks without stalling worker fibers or the Render
///             Thread.
class GP_CORE_API alignas(gp::memory::HardwareConstants::cacheLineSize) Logger final
{
private:
    using AsyncQueueType = gp::thread::MPMCRingQueue<LogEntry>;
    using QueueSlotType = AsyncQueueType::SlotType;

    alignas(gp::memory::HardwareConstants::cacheLineSize) QueueSlotType m_ringSlots[kDefaultAsyncLogQueueCapacity]{};
    AsyncQueueType m_asyncQueue{};
    gp::platform::Thread m_workerThread{};
    ILogSink* m_sinks[kMaxLogSinks]{};
    gp::thread::Atomic<gp::UInt32> m_sinkCount{ 0u };
    gp::thread::Atomic<LogLevel> m_globalMinLevel{ LogLevel::Trace };
    gp::thread::Atomic<LogExecutionMode> m_mode{ LogExecutionMode::Synchronous };
    gp::thread::Atomic<LogOverflowPolicy> m_overflowPolicy{ LogOverflowPolicy::FlushSynchronously };
    gp::thread::Atomic<gp::UInt64> m_totalSubmittedCount{ 0ULL };
    gp::thread::Atomic<gp::UInt64> m_totalDispatchedCount{ 0ULL };
    gp::thread::Atomic<gp::UInt64> m_droppedRecordCount{ 0ULL };
    gp::thread::Atomic<gp::platform::ThreadId> m_dispatchLockOwnerThreadId{ 0ULL };
    gp::thread::Atomic<bool> m_isInitialized{ false };
    gp::thread::Atomic<bool> m_stopWorkerRequested{ false };
    gp::thread::AtomicFlag m_dispatchSpinLock{};
    gp::UInt32 m_asyncFlushIntervalMicroseconds{ 1000u };

public:
    /// @brief Constructs an uninitialized `Logger` operating in `LogExecutionMode::Synchronous` pre-init mode.
    [[nodiscard]] Logger() noexcept;

    /// @brief Destructor that flushes all pending asynchronous records and stops the background worker thread.
    ~Logger() noexcept;

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    Logger(Logger&&) = delete;
    Logger& operator=(Logger&&) = delete;

public:
    /// @brief Returns the process-wide default `Logger` singleton instance.
    /// @return Reference to the global `Logger`.
    [[nodiscard]] static Logger& instance() noexcept;

public:
    /// @section Lifecycle & Configuration

    /// @brief Initializes the logger with the specified configuration and starts the background thread if
    ///        `desc.mode == LogExecutionMode::AsyncRing`.
    /// @param[in] desc Logger configuration parameters.
    /// @return `LogResult::Success`, `LogResult::AlreadyInitialized`, or `LogResult::ThreadCreationFailed`.
    [[nodiscard]] LogResult initialize(const LoggerDesc& desc = {}) noexcept;

    /// @brief Drains all queued log records, flushes all registered sinks, and stops the background flush thread.
    void shutdown() noexcept;

    /// @brief Checks whether `initialize()` has been called and the logger is active.
    /// @return True if initialized, false otherwise.
    [[nodiscard]] bool isInitialized() const noexcept
    {
        return m_isInitialized.load(gp::MemoryOrder::Acquire);
    }

    /// @brief Queries the currently active execution mode (`Synchronous` or `AsyncRing`).
    /// @return Active `LogExecutionMode`.
    [[nodiscard]] LogExecutionMode getExecutionMode() const noexcept
    {
        return m_mode.load(gp::MemoryOrder::Acquire);
    }

    /// @brief Queries the global minimum severity level across all categories.
    /// @return Global minimum `LogLevel`.
    [[nodiscard]] LogLevel getGlobalMinLevel() const noexcept
    {
        return m_globalMinLevel.load(gp::MemoryOrder::Relaxed);
    }

    /// @brief Atomically updates the global minimum severity level across all categories.
    /// @param[in] level New global minimum `LogLevel`.
    void setGlobalMinLevel(const LogLevel level) noexcept
    {
        m_globalMinLevel.store(level, gp::MemoryOrder::Relaxed);
    }

    /// @brief Checks whether a log message for `category` at `level` should be emitted.
    /// @param[in] category Target `LogCategory`.
    /// @param[in] level Candidate severity level.
    /// @return True if both `category.isEnabled(level)` and `level >= getGlobalMinLevel()` pass.
    [[nodiscard]] GP_FORCEINLINE_HINT bool shouldLog(const LogCategory& category, const LogLevel level) const noexcept
    {
        if (!category.isEnabled(level))
        {
            return false;
        }
        const LogLevel globalMin = m_globalMinLevel.load(gp::MemoryOrder::Relaxed);
        if (globalMin == LogLevel::Off) [[unlikely]]
        {
            return false;
        }
        return static_cast<gp::UInt8>(level) >= static_cast<gp::UInt8>(globalMin);
    }

public:
    /// @section Sink Registration (`ILogSink`)

    /// @brief Registers a non-owning `ILogSink` pointer to receive log records.
    /// @note The caller retains ownership of `sink` and must ensure it outlives its registration (or call `removeSink`
    ///       before destroying it). Automatically drains any pending async records prior to mutating the sink table.
    /// @param[in] sink Non-null pointer to an `ILogSink` implementation.
    /// @return `LogResult::Success`, `LogResult::InvalidArguments`, or `LogResult::SinkCapacityExceeded`.
    [[nodiscard]] LogResult addSink(ILogSink* const sink) noexcept;

    /// @brief Unregisters a previously registered `ILogSink` pointer after draining pending async records.
    /// @param[in] sink Pointer to the sink to remove.
    /// @return `LogResult::Success` or `LogResult::SinkNotFound`.
    [[nodiscard]] LogResult removeSink(ILogSink* const sink) noexcept;

    /// @brief Unregisters all currently attached sinks after draining pending async records.
    void clearSinks() noexcept;

    /// @brief Queries the number of currently registered `ILogSink` instances (`<= kMaxLogSinks`).
    /// @return Active sink count.
    [[nodiscard]] gp::UInt32 getSinkCount() const noexcept
    {
        return m_sinkCount.load(gp::MemoryOrder::Acquire);
    }

public:
    /// @section Record Submission & Flushing

    /// @brief Submits a pre-formatted UTF-8 log message to the router.
    /// @param[in] category Originating `LogCategory`.
    /// @param[in] level Severity level of the message.
    /// @param[in] location Call-site source file, function, and line number.
    /// @param[in] message Pre-formatted UTF-8 message string view.
    void submit(
        const LogCategory& category,
        const LogLevel level,
        const LogSourceLocation& location,
        const gp::StringView message
    ) noexcept;

    /// @brief Formats a `CheckedLogFormatString` and variadic arguments on the stack (zero heap allocations) and
    ///        submits the record if `shouldLog(category, level)` is true.
    /// @tparam InArgs Variadic format argument types.
    /// @param[in] category Originating `LogCategory`.
    /// @param[in] level Severity level of the message.
    /// @param[in] fmt Compile-time validated format string and source location.
    /// @param[in] args Arguments to format.
    template <typename... InArgs>
    void
        log(const LogCategory& category,
            const LogLevel level,
            const CheckedLogFormatString<InArgs...> fmt,
            const InArgs&... args) noexcept
    {
        if (!shouldLog(category, level))
        {
            return;
        }

        if constexpr (sizeof...(InArgs) == 0u)
        {
            submit(category, level, fmt.location, fmt.format);
        }
        else
        {
            LogMessageBuffer formattedBuffer{};
            const gp::StringView formattedView = formatTo(formattedBuffer, fmt.format, args...);
            submit(category, level, fmt.location, formattedView);
        }
    }

    /// @brief Drains all pending entries from the asynchronous MPMC ring queue and invokes `ILogSink::flush()` on all
    ///        registered sinks.
    void flush() noexcept;

public:
    /// @section Telemetry & Diagnostics

    /// @brief Resets `getTotalSubmittedCount()`, `getTotalDispatchedCount()`, and `getDroppedRecordCount()` to zero.
    void resetStats() noexcept
    {
        m_totalSubmittedCount.store(0ULL, gp::MemoryOrder::Relaxed);
        m_totalDispatchedCount.store(0ULL, gp::MemoryOrder::Relaxed);
        m_droppedRecordCount.store(0ULL, gp::MemoryOrder::Relaxed);
    }

    /// @brief Queries the total number of log records submitted to `submit()` since initialization or `resetStats()`.
    /// @return Cumulative submitted record count.
    [[nodiscard]] gp::UInt64 getTotalSubmittedCount() const noexcept
    {
        return m_totalSubmittedCount.load(gp::MemoryOrder::Relaxed);
    }

    /// @brief Queries the total number of log records dispatched to the sink table.
    /// @return Cumulative dispatched record count.
    [[nodiscard]] gp::UInt64 getTotalDispatchedCount() const noexcept
    {
        return m_totalDispatchedCount.load(gp::MemoryOrder::Relaxed);
    }

    /// @brief Queries the number of records dropped due to `LogOverflowPolicy::DropOldestNonCritical`.
    /// @return Cumulative dropped record count.
    [[nodiscard]] gp::UInt64 getDroppedRecordCount() const noexcept
    {
        return m_droppedRecordCount.load(gp::MemoryOrder::Relaxed);
    }

private:
    static gp::UInt32 workerThreadEntry(void* userData) noexcept;
    void workerThreadLoop() noexcept;
    void drainAsyncQueueLocked() noexcept;
    void dispatchRecordLocked(const LogRecordView& record) noexcept;
    void flushSinksLocked() noexcept;
    [[nodiscard]] bool tryAcquireDispatchLock(gp::platform::ThreadId callerThreadId) noexcept;
    void acquireDispatchLock(gp::platform::ThreadId callerThreadId) noexcept;
    void releaseDispatchLock() noexcept;
};

/// @section Global C++23 `consteval` Logging Functions (Routing to `Logger::instance()`)

/// @brief Emits a formatted log record at `level` to the global `Logger::instance()`.
/// @tparam InArgs Variadic argument types.
/// @param[in] category Target `LogCategory`.
/// @param[in] level Severity level.
/// @param[in] fmt Compile-time validated format string and captured `std::source_location`.
/// @param[in] args Format replacement arguments.
template <typename... InArgs>
GP_FORCEINLINE_HINT void
    log(const LogCategory& category,
        const LogLevel level,
        const CheckedLogFormatString<InArgs...> fmt,
        const InArgs&... args) noexcept
{
#if !GP_NO_LOGGING
    Logger::instance().log(category, level, fmt, args...);
#else
    (void)category;
    (void)level;
    (void)fmt;
    ((void)args, ...);
#endif
}

/// @brief Emits a `LogLevel::Trace` message to the global `Logger::instance()`.
/// @tparam InArgs Variadic argument types.
/// @param[in] category Target `LogCategory`.
/// @param[in] fmt Compile-time validated format string.
/// @param[in] args Format replacement arguments.
template <typename... InArgs>
GP_FORCEINLINE_HINT void
    trace(const LogCategory& category, const CheckedLogFormatString<InArgs...> fmt, const InArgs&... args) noexcept
{
    log(category, LogLevel::Trace, fmt, args...);
}

/// @brief Emits a `LogLevel::Debug` message to the global `Logger::instance()`.
/// @tparam InArgs Variadic argument types.
/// @param[in] category Target `LogCategory`.
/// @param[in] fmt Compile-time validated format string.
/// @param[in] args Format replacement arguments.
template <typename... InArgs>
GP_FORCEINLINE_HINT void
    debug(const LogCategory& category, const CheckedLogFormatString<InArgs...> fmt, const InArgs&... args) noexcept
{
    log(category, LogLevel::Debug, fmt, args...);
}

/// @brief Emits a `LogLevel::Info` message to the global `Logger::instance()`.
/// @tparam InArgs Variadic argument types.
/// @param[in] category Target `LogCategory`.
/// @param[in] fmt Compile-time validated format string.
/// @param[in] args Format replacement arguments.
template <typename... InArgs>
GP_FORCEINLINE_HINT void
    info(const LogCategory& category, const CheckedLogFormatString<InArgs...> fmt, const InArgs&... args) noexcept
{
    log(category, LogLevel::Info, fmt, args...);
}

/// @brief Emits a `LogLevel::Warning` message to the global `Logger::instance()`.
/// @tparam InArgs Variadic argument types.
/// @param[in] category Target `LogCategory`.
/// @param[in] fmt Compile-time validated format string.
/// @param[in] args Format replacement arguments.
template <typename... InArgs>
GP_FORCEINLINE_HINT void
    warning(const LogCategory& category, const CheckedLogFormatString<InArgs...> fmt, const InArgs&... args) noexcept
{
    log(category, LogLevel::Warning, fmt, args...);
}

/// @brief Emits a `LogLevel::Error` message to the global `Logger::instance()`.
/// @tparam InArgs Variadic argument types.
/// @param[in] category Target `LogCategory`.
/// @param[in] fmt Compile-time validated format string.
/// @param[in] args Format replacement arguments.
template <typename... InArgs>
GP_FORCEINLINE_HINT void
    error(const LogCategory& category, const CheckedLogFormatString<InArgs...> fmt, const InArgs&... args) noexcept
{
    log(category, LogLevel::Error, fmt, args...);
}

/// @brief Emits a `LogLevel::Fatal` message to the global `Logger::instance()` and flushes all sinks synchronously.
/// @tparam InArgs Variadic argument types.
/// @param[in] category Target `LogCategory`.
/// @param[in] fmt Compile-time validated format string.
/// @param[in] args Format replacement arguments.
template <typename... InArgs>
GP_FORCEINLINE_HINT void
    fatal(const LogCategory& category, const CheckedLogFormatString<InArgs...> fmt, const InArgs&... args) noexcept
{
    log(category, LogLevel::Fatal, fmt, args...);
}

}   // namespace gp::log
