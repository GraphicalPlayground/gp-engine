// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "logging/sinks/DebugOutputSink.hpp"
#include "containers/strings/FixedString.hpp"
#include "logging/LogFormatter.hpp"

#if GP_PLATFORM_WINDOWS
    #include "platforms/windows/WindowsHeaderWrapper.hpp"   // IWYU pragma: keep
#endif

namespace gp::log
{

DebugOutputSink::DebugOutputSink(const LogLevel minLevel, const LogSinkFlags flags) noexcept
    : ILogSink(minLevel)
    , m_flags(flags)
{}

void DebugOutputSink::write(const LogRecordView& record) noexcept
{
    if (!shouldWrite(record.level))
    {
        return;
    }

    gp::FixedString<640> lineBuffer{};
    (void)formatLogRecordLine(lineBuffer, record, m_flags);

#if GP_PLATFORM_WINDOWS
    ::OutputDebugStringA(lineBuffer.cString());
#else
    (void)lineBuffer;
#endif
}

void DebugOutputSink::flush() noexcept
{}

}   // namespace gp::log
