// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "container/BasicString.hpp"       // IWYU pragma: keep
#include "container/BasicStringView.hpp"   // IWYU pragma: keep
#include "container/Forward.hpp"
#include "container/Vector.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "errors/ErrorCode.hpp"
#include "errors/ErrorSeverity.hpp"
#include <chrono>   // TODO: gp::TimePoint?, gp::Duration?, etc.
#include <memory>   // TODO: gp::SharedPtr?
#include <source_location>
#include <stacktrace>
#include <thread>   // TODO: gp::Thread?

namespace gp::error
{

/// @brief A typed key/value pair attached to an ErrorRecord.
/// Stored inline (small-string optimized) to avoid heap allocations on the hot path when only a handful of tags are
/// attached.
struct MetaEntry
{
    gp::String key;
    gp::String value;
};

/// @brief A vector of metadata entries attached to an ErrorRecord.
using MetaBag = gp::Vector<MetaEntry>;

/// @brief The canonical error snapshot.
class ErrorRecord
{
public:
    Severity severity{ Severity::Error };
    ErrorCode code{ ErrorCode::ok() };

    // Pre-formatted message string.
    gp::String message;
    // Optional subsystem tag ("RHI", "Audio"…).
    gp::String subsystem;

    // Source location (zero overhead in release, inlined by the compiler)
    std::source_location location{ std::source_location::current() };

    // Empty when tracing is disabled.
    std::stacktrace stacktrace;

    std::thread::id threadId{ std::this_thread::get_id() };
    gp::String threadName;

    // Engine uptime tick.
    std::chrono::steady_clock::time_point engineTime{ std::chrono::steady_clock::now() };
    // Wall-clock instant.
    std::chrono::system_clock::time_point wallTime{ std::chrono::system_clock::now() };

    MetaBag metadata;

    // Causal error, allows wrapping lower-level failures with higher-level context.
    std::shared_ptr<const ErrorRecord> cause;

public:
    /// @brief Adds metadata to the error record.
    /// @param[in] key The metadata key (e.g. "filename", "userId", etc.)
    /// @param[in] value The metadata value (arbitrary string, e.g. "foo.txt", "12345", etc.)
    void addMetadata(gp::String key, gp::String value);

    /// @brief Looks up a metadata value by key.
    /// @param[in] key The metadata key to look up.
    /// @return The metadata value associated with the key, or empty string view if not found.
    GP_NODISCARD gp::StringView getMetadata(gp::StringView key) const noexcept;

    /// @brief Checks if the error has a causal error.
    /// @return True if the error has a causal error, false otherwise.
    GP_NODISCARD bool hasCause() const noexcept;

    /// @brief Gets the depth of the causal error chain.
    /// @return The number of levels in the causal error chain.
    GP_NODISCARD gp::USize causeDepth() const noexcept;

    /// @brief Generates a summary of the error.
    /// @return The error summary as a string.
    GP_NODISCARD gp::String summary() const;

    /// @brief Generates a full report of the error.
    /// @return The full error report as a string.
    GP_NODISCARD gp::String fullReport() const;
};

};   // namespace gp::error
