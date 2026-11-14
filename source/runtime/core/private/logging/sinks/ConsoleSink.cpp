// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "logging/sinks/ConsoleSink.hpp"
#include "containers/strings/FixedString.hpp"
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

    gp::FixedString<640> lineBuffer{};
    const gp::StringView formattedLine = formatLogRecordLine(lineBuffer, record, m_flags);

    std::FILE* const targetStream =
        (static_cast<gp::UInt8>(record.level) >= static_cast<gp::UInt8>(m_stderrThreshold)) ? stderr : stdout;

    (void)std::fwrite(formattedLine.data(), sizeof(char), formattedLine.size(), targetStream);

    const bool flushEvery = (m_flags & LogSinkFlags::FlushEveryRecord) != LogSinkFlags::None;
    if (flushEvery || static_cast<gp::UInt8>(record.level) >= static_cast<gp::UInt8>(LogLevel::Warning))
    {
        (void)std::fflush(targetStream);
    }
}

void ConsoleSink::flush() noexcept
{
    (void)std::fflush(stdout);
    (void)std::fflush(stderr);
}

}   // namespace gp::log
