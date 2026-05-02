// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "container/Array.hpp"
#include "container/Forward.hpp"
#include "CoreMinimal.hpp"
#include "errors/ErrorConfig.hpp"
#include "errors/ErrorContext.hpp"
#include "errors/ErrorRecord.hpp"
#include "errors/ErrorSink.hpp"
#include "errors/sinks/MultiSink.hpp"
#include "memory/UniquePtr.hpp"
#include <atomic>          // TODO: gp::Atomic?
#include <chrono>          // TODO: gp::Chrono?, gp::TimePoint?
#include <mutex>           // TODO: gp::Mutex?
#include <source_location>
#include <thread>          // TODO: gp::Thread, gp::ThreadId?
#include <unordered_map>   // TODO: gp::UnorderedMap?

namespace gp::error
{

/// @brief Statistics for error counts, tracked per severity level. Queryable at any time via ErrorSystem::stats().
struct ErrorStatistics
{
public:
    gp::Array<std::atomic<gp::UInt64>, static_cast<gp::USize>(Severity::COUNT)> counts{};
    std::atomic<gp::UInt64> totalDropped{ 0u };   //<! Records dropped by rate-limiting/dedup.

public:
    /// @brief Constructs an ErrorStatistics instance, initializing all counts to zero.
    ErrorStatistics()
    {
        for (auto& c: counts)
        {
            c.store(0);
        }
    }

    /// @brief Delete copy constructor and copy assignment operator to prevent copying of ErrorStatistics.
    ErrorStatistics(const ErrorStatistics&) = delete;
    ErrorStatistics& operator=(const ErrorStatistics&) = delete;

public:
    /// @brief Returns the count of errors for a given severity level.
    /// @param[in] severity The severity level to get the count for.
    /// @return The count of errors for the specified severity level.
    GP_NODISCARD gp::UInt64 countFor(Severity severity) const noexcept
    {
        return counts[static_cast<gp::USize>(severity)].load(std::memory_order_relaxed);
    }

    /// @brief Increments the count for a given severity level.
    /// @param[in] severity The severity level to increment the count for.
    void increment(Severity severity) noexcept
    {
        counts[static_cast<gp::USize>(severity)].fetch_add(1, std::memory_order_relaxed);
    }
};

/// @brief The central error handling system, implemented as a singleton. Provides APIs for raising errors, managing
///        sinks, and querying statistics.
/// @note
/// ErrorSystem is the single-point dispatcher that:
///   1. Owns the configuration (ErrorSystemConfig).
///   2. Owns the root MultiSink (fan-out to all registered sinks).
///   3. Builds ErrorRecords from raise() parameters.
///   4. Manages deduplication, statistics counters, and crash handlers.
///   5. Provides structured query/inspection APIs used by crash-report UIs.
class ErrorSystem
{
private:
    ErrorSystemConfig m_config;
    gp::UniquePtr<MultiSink> m_rootSink;
    ErrorStatistics m_stats;
    std::chrono::steady_clock::time_point m_initTime;
    std::mutex m_dedupMutex;
    std::unordered_map<gp::USize, std::chrono::steady_clock::time_point> m_dedupTable;

    static std::atomic<ErrorSystem*> s_instance;
    static std::mutex s_initMutex;
    static std::mutex m_sinkMutex;

public:
    /// @brief Key used for deduplication of errors. Combines error code, message hash, and thread ID.
    struct DedupKey
    {
    public:
        gp::UInt32 codeRaw;         //<! The raw error code (without severity bits).
        gp::USize messageHash;      //<! A hash of the error message string, used for deduplication.
        std::thread::id threadId;   //<! The ID of the thread that emitted the error, used for deduplication.

    public:
        /// @brief Equality operator for DedupKey, compares all fields for equality.
        bool operator==(const DedupKey&) const noexcept = default;
    };

private:
    /// @brief Constructs the ErrorSystem singleton with the specified configuration.
    /// @param[in] config The configuration to initialize the ErrorSystem with.
    ErrorSystem(ErrorSystemConfig config);

    /// @brief Destructs the ErrorSystem singleton, performing any necessary cleanup.
    ~ErrorSystem();

    /// @brief Delete copy constructor and copy assignment operator to prevent copying of ErrorSystem.
    ErrorSystem(const ErrorSystem&) = delete;
    ErrorSystem& operator=(const ErrorSystem&) = delete;

public:
    /// @brief Initializes the ErrorSystem with the specified configuration.
    /// @param[in] config The configuration to initialize the ErrorSystem with.
    static void initialize(ErrorSystemConfig config = ErrorSystemConfig::getDevelopmentConfig());

    /// @brief Shuts down the ErrorSystem, performing any necessary cleanup.
    static void shutdown();

    /// @brief Checks if the ErrorSystem is initialized.
    /// @return True if the ErrorSystem is initialized, false otherwise.
    GP_NODISCARD static bool isInitialized() noexcept;

    /// @brief Adds a sink to the ErrorSystem.
    /// @param[in] sink The sink to add.
    static void addSink(std::shared_ptr<Sink> sink);

    /// @brief Removes a sink from the ErrorSystem.
    /// @param[in] name The name of the sink to remove.
    static void removeSink(const gp::String& name);

    /// @brief Clears all sinks, removing them from the ErrorSystem.
    static void clearSinks();

    /// @brief Flushes all sinks, ensuring any buffered errors are written.
    static void flushAll();

    /// @brief Returns a reference to the root MultiSink for advanced configuration.
    GP_NODISCARD static MultiSink& rootSink();

    /// @brief Returns a const reference to the current ErrorSystemConfig.
    /// @return A const reference to the ErrorSystemConfig instance used by the ErrorSystem.
    GP_NODISCARD static const ErrorSystemConfig& config() noexcept;

    /// @brief Set the Global Min Severity object
    /// @param[in] severity The minimum severity level for errors to be processed by the system.
    static void setGlobalMinSeverity(Severity severity) noexcept;

    /// @brief Dispatches an error record to the ErrorSystem, building it from the provided parameters.
    /// @param[in] severity The severity level of the error being dispatched.
    /// @param[in] code The error code associated with the error being dispatched.
    /// @param[in] message The error message string describing the error being dispatched.
    /// @param[in] location The source location where the error was raised.
    /// @param[in] cause An optional shared pointer to another ErrorRecord that caused this error.
    static void dispatch(
        Severity severity,
        ErrorCode code,
        gp::String message,
        std::source_location location,
        std::shared_ptr<const ErrorRecord> cause = nullptr
    );

    /// @brief Returns a reference to the error statistics.
    /// @return A const reference to the ErrorStatistics instance containing error counts.
    GP_NODISCARD static const ErrorStatistics& stats() noexcept;

    /// @brief Resets all error statistics counts to zero.
    static void resetStats() noexcept;

    /// @brief Installs signal handlers for handling system signals that may indicate crashes or other critical events.
    static void installSignalHandlers();

private:
    /// @brief Determines whether an error with the given deduplication key should be emitted or deduplicated based on
    /// recent occurrences.
    /// @param[in] key The deduplication key representing the error being evaluated for deduplication.
    /// @return True if the error should be emitted, false if it should be deduplicated (i.e., suppressed).
    GP_NODISCARD bool shouldDedup(const DedupKey& key);

    /// @brief Internal implementation of the dispatch function, which builds an ErrorRecord and sends it to the sinks.
    ///        This is called by the public static dispatch() method after performing any necessary checks (e.g.,
    ///        deduplication).
    /// @param[in] severity The severity level of the error being dispatched.
    /// @param[in] code The error code associated with the error being dispatched.
    /// @param[in] message The error message string describing the error being dispatched.
    /// @param[in] location The source location where the error was raised.
    /// @param[in] cause The cause of the error, represented as a shared pointer to another ErrorRecord. This allows for
    ///                  chaining of errors to provide more context.
    void dispatchImpl(
        Severity severity,
        ErrorCode code,
        gp::String message,
        std::source_location location,
        std::shared_ptr<const ErrorRecord> cause
    );

    /// @brief Signal handler for handling system signals.
    /// @param[in] sig The signal number.
    static void onSignal(int sig);
};

}   // namespace gp::error
