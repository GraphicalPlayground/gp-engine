// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "container/BasicString.hpp"       // IWYU pragma: keep
#include "container/BasicStringView.hpp"   // IWYU pragma: keep
#include "container/Forward.hpp"
#include "CoreMinimal.hpp"
#include "errors/ErrorRecord.hpp"
#include "errors/ErrorRegistry.hpp"
#include "errors/ErrorSeverity.hpp"
#include <atomic>       // TODO: gp::Atomic?
#include <chrono>       // TODO: gp::TimePoint?, gp::Duration?, etc.
#include <functional>   // TODO: gp::Predicate? gp::Function?
#include <memory>       // TODO: gp::SharedPtr?
#include <regex>        // TODO: gp::Regex?

namespace gp::error
{

/// @brief A value-semantic, composable predicate for filtering ErrorRecords.
class FilterPredicate
{
public:
    using FunctionType = std::function<bool(const ErrorRecord&)>;

private:
    FunctionType m_func;

public:
    /// @brief Construct a FilterPredicate from any callable with the appropriate signature.
    /// @param[in] func A callable that takes a const ErrorRecord& and returns bool.
    explicit FilterPredicate(FunctionType func)
        : m_func(std::move(func))
    {}

public:
    /// @brief Evaluate the predicate on a given ErrorRecord.
    /// @param[in] record The ErrorRecord to evaluate.
    /// @return true if the record matches the predicate, false otherwise.
    GP_NODISCARD bool operator()(const ErrorRecord& record) const
    {
        return m_func(record);
    }

    /// @brief Combine this predicate with another using logical AND.
    /// @param[in] other Another FilterPredicate to combine with.
    /// @return A new FilterPredicate that represents the logical AND of this and the other predicate.
    GP_NODISCARD FilterPredicate operator&&(const FilterPredicate& other) const
    {
        auto lhs = m_func, rhs = other.m_func;
        return FilterPredicate{ [lhs, rhs](const ErrorRecord& record)
                                {
                                    return lhs(record) && rhs(record);
                                } };
    }

    /// @brief Combine this predicate with another using logical OR.
    /// @param[in] other Another FilterPredicate to combine with.
    /// @return A new FilterPredicate that represents the logical OR of this and the other predicate.
    GP_NODISCARD FilterPredicate operator||(const FilterPredicate& other) const
    {
        auto lhs = m_func, rhs = other.m_func;
        return FilterPredicate{ [lhs, rhs](const ErrorRecord& record)
                                {
                                    return lhs(record) || rhs(record);
                                } };
    }

    /// @brief Negate this predicate.
    /// @return A new FilterPredicate that represents the logical NOT of this predicate.
    GP_NODISCARD FilterPredicate operator!() const
    {
        auto lhs = m_func;
        return FilterPredicate{ [lhs](const ErrorRecord& record)
                                {
                                    return !lhs(record);
                                } };
    }
};

namespace filters
{

/// @brief Predicate that returns true for records with severity at least @p severity.
/// @param[in] severity The minimum severity level to match.
/// @return A FilterPredicate that matches records with severity >= severity.
GP_NODISCARD inline FilterPredicate atLeast(Severity severity)
{
    return FilterPredicate{ [severity](const ErrorRecord& record)
                            {
                                return record.severity >= severity;
                            } };
}

/// @brief Predicate that returns true for records with severity at most @p severity.
/// @param[in] severity The maximum severity level to match.
/// @return A FilterPredicate that matches records with severity <= severity.
GP_NODISCARD inline FilterPredicate atMost(Severity severity)
{
    return FilterPredicate{ [severity](const ErrorRecord& record)
                            {
                                return record.severity <= severity;
                            } };
}

/// @brief Predicate that returns true for records with severity exactly @p severity.
/// @param[in] severity The severity level to match.
/// @return A FilterPredicate that matches records with severity == severity.
GP_NODISCARD inline FilterPredicate exactly(Severity severity)
{
    return FilterPredicate{ [severity](const ErrorRecord& record)
                            {
                                return record.severity == severity;
                            } };
}

/// @brief Predicate that returns true for records with severity in the range [minimumSeverity, maximumSeverity].
/// @param[in] minimumSeverity The minimum severity level to match (inclusive).
/// @param[in] maximumSeverity The maximum severity level to match (inclusive).
/// @return A FilterPredicate that matches records with severity in the specified range.
GP_NODISCARD inline FilterPredicate severityRange(Severity minimumSeverity, Severity maximumSeverity)
{
    return FilterPredicate{ [minimumSeverity, maximumSeverity](const ErrorRecord& record)
                            {
                                return record.severity >= minimumSeverity && record.severity <= maximumSeverity;
                            } };
}

/// @brief Predicate that returns true for records with a specific domain.
/// @param[in] domain The domain to match.
/// @return A FilterPredicate that matches records with the specified domain.
GP_NODISCARD inline FilterPredicate domain(Domain domain)
{
    return FilterPredicate{ [domain](const ErrorRecord& record)
                            {
                                return record.code.domain() == domain;
                            } };
}

/// @brief Predicate that returns true for records with a specific error code.
/// @param[in] code The error code to match.
/// @return A FilterPredicate that matches records with the specified error code.
GP_NODISCARD inline FilterPredicate code(ErrorCode code)
{
    return FilterPredicate{ [code](const ErrorRecord& record)
                            {
                                return record.code == code;
                            } };
}

/// @brief Predicate that returns true for records with any of the specified error codes.
/// @param[in] codes An initializer list of error codes to match.
/// @return A FilterPredicate that matches records with any of the specified error codes.
GP_NODISCARD inline FilterPredicate anyCode(std::initializer_list<ErrorCode> codes)
{
    std::vector<ErrorCode> v{ codes };
    return FilterPredicate{ [v](const ErrorRecord& record)
                            {
                                for (const auto& c: v)
                                {
                                    if (record.code == c)
                                    {
                                        return true;
                                    }
                                }
                                return false;
                            } };
}

/// @brief Predicate that returns true for records originating from a specific subsystem.
/// @param[in] name The name of the subsystem to match.
/// @return A FilterPredicate that matches records from the specified subsystem.
GP_NODISCARD inline FilterPredicate subsystem(gp::String name)
{
    return FilterPredicate{ [name](const ErrorRecord& record)
                            {
                                return record.subsystem == name;
                            } };
}

/// @brief Predicate that returns true for records originating from a specific thread.
/// @param[in] name The name of the thread to match.
/// @return A FilterPredicate that matches records from the specified thread.
GP_NODISCARD inline FilterPredicate thread(gp::String name)
{
    return FilterPredicate{ [name](const ErrorRecord& record)
                            {
                                return record.threadName == name;
                            } };
}

/// @brief Predicate that returns true for records whose message contains a specific substring.
/// @param[in] needle The substring to search for in the message.
/// @return A FilterPredicate that matches records whose message contains the specified substring.
GP_NODISCARD inline FilterPredicate messageContains(gp::String needle)
{
    return FilterPredicate{ [needle](const ErrorRecord& record)
                            {
                                return record.message.find(needle) != gp::String::npos;
                            } };
}

/// @brief Predicate that returns true for records whose message matches a regex pattern.
/// @param[in] pattern The regex pattern to match against the message.
/// @return A FilterPredicate that matches records whose message matches the specified regex pattern.
GP_NODISCARD inline FilterPredicate messageMatches(const gp::String& pattern)
{
    auto re = std::make_shared<std::regex>(pattern.cStr());
    return FilterPredicate{ [re](const ErrorRecord& record)
                            {
                                return std::regex_search(record.message.cStr(), *re);
                            } };
}

/// @brief Predicate that returns true for records originating from a source file whose path contains a specific
/// substring.
/// @param[in] path The substring to search for in the source file path.
/// @return A FilterPredicate that matches records originating from source files whose path contains the specified
/// substring.
GP_NODISCARD inline FilterPredicate sourceFile(gp::String path)
{
    return FilterPredicate{ [path](const ErrorRecord& record)
                            {
                                return gp::StringView{ record.location.file_name() }.find(path) != gp::StringView::npos;
                            } };
}

/// @brief Predicate that returns true for records that have a specific metadata key-value pair.
/// @param[in] key The metadata key to check for.
/// @param[in] value The metadata value to match for the specified key.
/// @return A FilterPredicate that matches records that have the specified metadata key-value pair.
GP_NODISCARD inline FilterPredicate hasMeta(gp::String key, gp::String value)
{
    return FilterPredicate{ [key, value](const ErrorRecord& r)
                            {
                                return r.getMetadata(key) == value;
                            } };
}

/// @brief Predicate that returns true for records that have a specific metadata key, regardless of its value.
/// @param[in] key The metadata key to check for.
/// @return A FilterPredicate that matches records that have the specified metadata key.
GP_NODISCARD inline FilterPredicate hasMetaKey(gp::String key)
{
    return FilterPredicate{ [key](const ErrorRecord& record)
                            {
                                return !record.getMetadata(key).isEmpty();
                            } };
}

/// @brief Predicate that returns true for records raised after a specific time point (wall clock).
/// @param[in] when The time point to compare against (records raised after this time will match).
/// @return A FilterPredicate that matches records raised after the specified time point.
GP_NODISCARD inline FilterPredicate after(std::chrono::system_clock::time_point when)
{
    return FilterPredicate{ [when](const ErrorRecord& record)
                            {
                                return record.wallTime > when;
                            } };
}

/// @brief Predicate that returns true for records raised before a specific time point (wall clock).
/// @param[in] when The time point to compare against (records raised before this time will match).
/// @return A FilterPredicate that matches records raised before the specified time point.
GP_NODISCARD inline FilterPredicate before(std::chrono::system_clock::time_point when)
{
    return FilterPredicate{ [when](const ErrorRecord& record)
                            {
                                return record.wallTime < when;
                            } };
}

/// @brief Predicate that matches all records (always returns true).
/// @return A FilterPredicate that matches all records.
GP_NODISCARD inline FilterPredicate acceptAll()
{
    return FilterPredicate{ [](const ErrorRecord&)
                            {
                                return true;
                            } };
}

/// @brief Predicate that matches no records (always returns false).
/// @return A FilterPredicate that matches no records.
GP_NODISCARD inline FilterPredicate rejectAll()
{
    return FilterPredicate{ [](const ErrorRecord&)
                            {
                                return false;
                            } };
}

/// @brief Predicate that returns true for records that have a non-empty cause (i.e., were raised with a cause).
/// @return A FilterPredicate that matches records that have a cause.
GP_NODISCARD inline FilterPredicate hasCause()
{
    return FilterPredicate{ [](const ErrorRecord& record)
                            {
                                return record.hasCause();
                            } };
}

/// @brief Predicate that returns true for records that are marked as expected in the registry.
/// @return A FilterPredicate that matches records that are expected according to the registry.
GP_NODISCARD inline FilterPredicate isExpected()
{
    return FilterPredicate{ [](const ErrorRecord& record)
                            {
                                return ErrorRegistry::instance().isExpected(record.code);
                            } };
}

/// @brief Predicate that returns true for a random sample of records, with a sampling rate of 1 in N.
/// @param[in] oneInN The sampling rate, where 1 in N records will be accepted on average.
/// @return A FilterPredicate that matches a random sample of records at the specified rate.
GP_NODISCARD inline FilterPredicate sample(gp::UInt32 oneInN)
{
    auto counter = std::make_shared<std::atomic<gp::UInt32>>(0u);
    return FilterPredicate{ [counter, oneInN](const ErrorRecord&)
                            {
                                return counter->fetch_add(1, std::memory_order_relaxed) % oneInN == 0;
                            } };
}

}   // namespace filters

}   // namespace gp::error
