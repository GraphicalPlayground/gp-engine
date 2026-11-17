// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "logging/sinks/StringLogSink.hpp"
#include "logging/LogFormatter.hpp"

namespace gp::log
{

StringLogSink::StringLogSink(const LogLevel minLevel, const LogSinkFlags flags) noexcept
    : ILogSink(minLevel)
    , m_flags(flags)
{}

void StringLogSink::write(const LogRecordView& record) noexcept
{
    if (!shouldWrite(record.level))
    {
        return;
    }

    LogLineBuffer lineBuffer{};
    const gp::StringView formattedLine = formatLogRecordLine(lineBuffer, record, m_flags);
    if (!formattedLine.isEmpty())
    {
        const gp::USize freeSpace = kCapacity - m_buffer.size();
        if (formattedLine.size() <= freeSpace)
        {
            m_buffer.append(formattedLine);
        }
        else
        {
            if (freeSpace > 0u)
            {
                m_buffer.append(formattedLine.substr(0u, freeSpace));
            }
            m_isTruncated = true;
        }
    }

    m_writeCount.fetchAdd(1u, gp::MemoryOrder::Release);
}

void StringLogSink::flush() noexcept
{
    m_flushCount.fetchAdd(1u, gp::MemoryOrder::Release);
}

void StringLogSink::clear() noexcept
{
    m_buffer.clear();
    m_isTruncated = false;
    m_writeCount.store(0u, gp::MemoryOrder::Release);
    m_flushCount.store(0u, gp::MemoryOrder::Release);
}

}   // namespace gp::log
