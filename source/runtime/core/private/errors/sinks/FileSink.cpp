// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "errors/sinks/FileSink.hpp"
#include "errors/ErrorCode.hpp"
#include "errors/ErrorSeverity.hpp"
#include <ctime>
#include <stdexcept>

namespace gp::error
{

FileSink::FileSink(const gp::String& path)
    : m_file(path.cStr(), std::ios::app | std::ios::out)
{
    setName("FileSink:" + path);
    if (!m_file.is_open())
    {
        // TODO: Use a inner exception or error code instead of std::runtime_error
        throw std::runtime_error(("GP FileSink: cannot open log file: " + path).cStr());
    }

    m_file << gp::String::format(
        "=== GP Engine Log — opened {} ===\n", std::chrono::system_clock::now().time_since_epoch().count()
    );
}

void FileSink::onRecord(const ErrorRecord& record)
{
    auto tt = std::chrono::system_clock::to_time_t(record.wallTime);
    char buf[32] = {};
#if defined(_MSC_VER)
    struct tm tm{};
    gmtime_s(&tm, &tt);
    strftime(buf, sizeof buf, "%Y-%m-%d %T", &tm);
#else
    struct tm tm{};
    gmtime_r(&tt, &tm);
    strftime(buf, sizeof buf, "%Y-%m-%d %T", &tm);
#endif

    std::lock_guard lock(m_mutex);

    m_file << gp::String::format(
        "[{}][{}][{}:0x{:04X}] {} | {}:{}\n",
        buf,
        getSeverityName(record.severity),
        getDomainName(record.code.domain()),
        record.code.code(),
        record.message,
        record.location.file_name(),
        record.location.line()
    );

    for (const auto& m: record.metadata)
    {
        m_file << gp::String::format("  meta: {}={}\n", m.key, m.value);
    }

#if GP_HAS_STACKTRACE
    if (!record.stacktrace.empty())
    {
        m_file << "  stacktrace:\n";
        for (const auto& frame: record.stacktrace)
        {
            m_file << gp::String::format("    {}\n", std::to_string(frame));
        }
    }
#endif
}

void FileSink::flush()
{
    std::lock_guard lock(m_mutex);
    m_file.flush();
}

}   // namespace gp::error
