// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "container/BasicString.hpp"   // IWYU pragma: keep
#include "container/Forward.hpp"
#include "container/Vector.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "errors/ErrorCode.hpp"
#include "errors/ErrorRecord.hpp"
#include "errors/ErrorSeverity.hpp"

namespace gp::error
{

/// @brief Abstract base class for error sinks. Sinks receive error records that pass the global filter and process them
/// (e.g. log to console, write to file, send telemetry, etc.).
class Sink
{
protected:
    gp::String m_name{ "<unknown>" };
    Severity m_minSeverity{ Severity::Trace };
    gp::Vector<Domain> m_domainFilter;

public:
    /// @brief Default destructor.
    virtual ~Sink() noexcept = default;

public:
    /// @brief Called by the ErrorSystem for every record that passes the global filter.
    /// @param[in] record The error record to process.
    virtual void onRecord(const ErrorRecord& record) = 0;

    /// @brief Flush any buffered output (file handles, network sockets, etc.).
    virtual void flush();

    /// @brief Get the minimum severity level that this sink will process.
    /// @return The minimum severity level.
    GP_NODISCARD Severity minSeverity() const noexcept;

    /// @brief Set the minimum severity level that this sink will process.
    /// @param[in] severity The minimum severity level to set.
    void setMinSeverity(Severity severity) noexcept;

    /// @brief Add a domain filter to this sink.
    /// @param[in] domain The domain to filter.
    void addDomainFilter(Domain domain);

    /// @brief Clear all domain filters from this sink.
    void clearDomainFilter();

    /// @brief Get the name of this sink.
    /// @return The name of the sink.
    GP_NODISCARD const gp::String& name() const noexcept;

    /// @brief Set the name of this sink.
    /// @param[in] name The name to set.
    void setName(gp::String name);

    /// @brief Dispatch an error record to this sink if it meets the severity and domain filters.
    /// @param[in] record The error record to dispatch.
    void dispatch(const ErrorRecord& record);
};

};   // namespace gp::error
