// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "container/Forward.hpp"
#include "container/Vector.hpp"
#include "CoreMinimal.hpp"
#include "errors/ErrorSink.hpp"
#include <mutex>   // TODO: gp::Mutex?

namespace gp::error
{

/// @brief MultiSink is the default root sink for the ErrorSystem.  It fans out to any number of child sinks, which can
/// be added and removed at runtime.
/// @note
/// MultiSink is thread-safe, but individual child sinks are not protected by any internal synchronization.
/// If you need to share a sink across threads, consider wrapping it in a RateLimitedSink or using your own
/// synchronization.
class MultiSink final : public Sink
{
private:
    mutable std::mutex m_mutex;
    gp::Vector<std::shared_ptr<Sink>> m_sinks;

public:
    /// @brief Constructor that initializes the MultiSink with a default name and minimum severity level.
    explicit MultiSink();

public:
    /// @brief Add a sink to this MultiSink.
    /// @param[in] sink The sink to add.
    void addSink(std::shared_ptr<Sink> sink);

    /// @brief Remove a sink from this MultiSink by name.
    /// @param[in] name The name of the sink to remove.
    void removeSink(const gp::String& name);

    /// @brief Get the number of sinks currently added to this MultiSink.
    /// @return The number of sinks.
    GP_NODISCARD gp::USize sinkCount() const noexcept;

    /// @brief Called by the ErrorSystem for every record that passes the global filter.
    /// @param[in] record The error record to process.
    void onRecord(const ErrorRecord& record) override;

    /// @brief Flush any buffered output (file handles, network sockets, etc.).
    void flush() override;
};

}   // namespace gp::error
