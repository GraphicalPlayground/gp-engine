// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "errors/sinks/MultiSink.hpp"

namespace gp::error
{

MultiSink::MultiSink()
{
    setName("MultiSink");
}

void MultiSink::addSink(std::shared_ptr<Sink> sink)
{
    if (!sink)
    {
        return;
    }

    std::lock_guard lock(m_mutex);
    m_sinks.pushBack(std::move(sink));
}

void MultiSink::removeSink(const gp::String& name)
{
    std::lock_guard lock(m_mutex);
    gp::eraseIf(
        m_sinks,
        [&](const auto& sink)
    {
        return sink->name() == name;
    }
    );
}

GP_NODISCARD gp::USize MultiSink::sinkCount() const noexcept
{
    std::lock_guard lock(m_mutex);
    return m_sinks.size();
}

void MultiSink::onRecord(const ErrorRecord& record)
{
    decltype(m_sinks) sinks;
    {
        std::lock_guard lock(m_mutex);
        sinks = m_sinks;
    }

    for (auto& sink: sinks)
    {
        sink->dispatch(record);
    }
}

void MultiSink::flush()
{
    decltype(m_sinks) sinks;
    {
        std::lock_guard lock(m_mutex);
        sinks = m_sinks;
    }

    for (auto& sink: sinks)
    {
        sink->flush();
    }
}

}   // namespace gp::error
