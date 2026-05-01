// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "errors/sinks/ConsoleSink.hpp"
#include <ctime>

namespace gp::error
{

ConsoleSink::ConsoleSink(bool useAnsiColor, bool toStdout)
    : m_useColor(useAnsiColor)
    , m_stream(toStdout ? stdout : stderr)
{
    setName("ConsoleSink");
}

void ConsoleSink::onRecord(const ErrorRecord& record)
{
    // Format:  [WRN][Window:0x0041] Window creation failed  (main.cpp:42)
    const bool color = m_useColor;
    const auto ansi = color ? getSeverityAnsiColor(record.severity) : "";
    const auto reset = color ? "\033[0m" : "";
    const auto bold = color ? "\033[1m" : "";

    // Wall clock ISO 8601
    auto tt = std::chrono::system_clock::to_time_t(record.wallTime);
    char timebuf[32] = {};
    // Thread-safe strftime equivalent (no gmtime_r on MSVC without CRT ext)
#if defined(_MSC_VER)
    struct tm tmval{};
    gmtime_s(&tmval, &tt);
    strftime(timebuf, sizeof timebuf, "%T", &tmval);
#else
    struct tm tmval{};
    gmtime_r(&tt, &tmval);
    strftime(timebuf, sizeof timebuf, "%T", &tmval);
#endif

    gp::String line;
    line.reserve(256);
    line += ansi;
    line += gp::String::format("[{}] ", getSeverityName(record.severity));
    line += reset;
    line += gp::String::format("{} ", timebuf);

    if (!record.subsystem.isEmpty())
    {
        line += gp::String::format("[{}] ", record.subsystem);
    }

    line += bold;
    line += record.message;
    line += reset;

    // Source location (short form, basename only)
    std::string_view file = record.location.file_name();
    if (auto pos = file.rfind('/'); pos != std::string_view::npos)
    {
        file = file.substr(pos + 1);
    }
    if (auto pos = file.rfind('\\'); pos != std::string_view::npos)
    {
        file = file.substr(pos + 1);
    }

    line += gp::String::format("  ({}:{})", file, record.location.line());

    if (!record.threadName.isEmpty())
    {
        line += gp::String::format(" [{}]", record.threadName);
    }

    line += '\n';

    // Stacktrace (only for Error and above, configurable)
    if (m_printStacktrace && record.severity >= Severity::Error)
    {
#if GP_HAS_STACKTRACE
        if (!record.stacktrace.empty())
        {
            line += gp::String::format("{}  Stack trace:\n{}", ansi, reset);
            for (const auto& frame: record.stacktrace)
            {
                line += gp::String::format("    {}\n", std::to_string(frame));
            }
        }
#endif
    }

    // Cause chain
    if (record.hasCause() && m_printCause)
    {
        const ErrorRecord* cause = record.cause.get();
        int depth = 0;
        while (cause)
        {
            line += gp::String::format(
                "  {}Caused by [{}]{}: {}\n", ansi, getSeverityName(cause->severity), reset, cause->message
            );
            cause = cause->cause.get();
            if (++depth > 8)
            {
                line += "  ... (truncated)\n";
                break;
            }
        }
    }

    std::lock_guard lock(m_mutex);
    std::fwrite(line.data(), 1, line.size(), m_stream);
}

void ConsoleSink::flush()
{
    std::fflush(m_stream);
}

void ConsoleSink::setPrintStacktrace(bool enabled) noexcept
{
    m_printStacktrace = enabled;
}

void ConsoleSink::setPrintCause(bool enabled) noexcept
{
    m_printCause = enabled;
}

}   // namespace gp::error
