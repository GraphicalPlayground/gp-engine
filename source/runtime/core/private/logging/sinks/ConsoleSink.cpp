// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "logging/sinks/ConsoleSink.hpp"
#include "containers/strings/FixedString.hpp"   // IWYU pragma: keep
#include "logging/LogFormatter.hpp"
#include <cstdio>

namespace gp::log
{

ConsoleSink::ConsoleSink(const ConsoleSinkDesc& desc) noexcept
    : ILogSink(desc.minLevel)
    , m_stderrThreshold(desc.stderrThreshold)
    , m_flags(desc.flags)
{}

ConsoleSink::~ConsoleSink() noexcept
{
    flush();
}

void ConsoleSink::write(const LogRecordView& record) noexcept
{
    if (!shouldWrite(record.level))
    {
        return;
    }

    LogLineBuffer lineBuffer{};
    const gp::StringView formattedLine = formatLogRecordLine(lineBuffer, record, m_flags);

    std::FILE* const targetStream =
        (static_cast<gp::UInt8>(record.level) >= static_cast<gp::UInt8>(m_stderrThreshold)) ? stderr : stdout;

    std::fwrite(formattedLine.data(), sizeof(char), formattedLine.size(), targetStream);

    const bool flushEvery = (m_flags & LogSinkFlags::FlushEveryRecord) != LogSinkFlags::None;
    if (flushEvery || static_cast<gp::UInt8>(record.level) >= static_cast<gp::UInt8>(LogLevel::Warning))
    {
        std::fflush(targetStream);
    }
}

void ConsoleSink::flush() noexcept
{
    std::fflush(stdout);
    std::fflush(stderr);
}

}   // namespace gp::log
