// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "logging/sinks/TracySink.hpp"
#include "containers/strings/FixedString.hpp"
#include "logging/LogFormatter.hpp"   // IWYU pragma: keep
#include "profiling/Profiler.hpp"

namespace gp::log
{

TracySink::TracySink(const LogLevel minLevel) noexcept
    : ILogSink(minLevel)
{}

void TracySink::write(const LogRecordView& record) noexcept
{
    if (!shouldWrite(record.level))
    {
        return;
    }

#if GP_USE_TRACY_PROFILER
    gp::FixedString<512> tracyBuffer{};
    tracyBuffer.pushBack('[');
    if (!record.categoryName.isEmpty())
    {
        tracyBuffer.append(record.categoryName);
    }
    tracyBuffer.append("] ");
    tracyBuffer.append(record.message);

    gp::UInt32 color = GP_U32_COLOR_WHITE;
    switch (record.level)
    {
    case LogLevel::Trace:
        color = GP_U32_COLOR_GRAY;
        break;
    case LogLevel::Debug:
        color = GP_U32_COLOR_CYAN;
        break;
    case LogLevel::Info:
        color = GP_U32_COLOR_GREEN;
        break;
    case LogLevel::Warning:
        color = GP_U32_COLOR_YELLOW;
        break;
    case LogLevel::Error:
        color = GP_U32_COLOR_ORANGE;
        break;
    case LogLevel::Fatal:
        color = GP_U32_COLOR_RED;
        break;
    case LogLevel::Off:
        // falls through
    default:
        break;
    }

    GP_MESSAGE_C(tracyBuffer.cString(), tracyBuffer.size(), color);
#endif
}

void TracySink::flush() noexcept
{}

}   // namespace gp::log
