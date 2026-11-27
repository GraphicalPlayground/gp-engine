// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "profiling/GpuProfilerTrace.hpp"
#include "containers/arrays/Vector.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "memory/MemoryOrder.hpp"
#include "threading/atomics/Atomic.hpp"
#include "threading/sync/Mutex.hpp"
#include "threading/sync/UniqueLock.hpp"
#include <charconv>
#include <cstdio>
#include <system_error>

namespace gp::rhi::profiling
{

void MetadataSerializer::appendSeparatorAndKey(const gp::StringView key) noexcept
{
    if (m_fieldCount > 0u)
    {
        m_buffer.append(", ");
    }
    m_buffer.append(key);
    m_buffer.pushBack('=');
    ++m_fieldCount;
}

void MetadataSerializer::appendValue(const gp::StringView key, const gp::StringView value) noexcept
{
    appendSeparatorAndKey(key);
    m_buffer.pushBack('"');
    m_buffer.append(value);
    m_buffer.pushBack('"');
}

void MetadataSerializer::appendValue(const gp::StringView key, const gp::UInt64 value) noexcept
{
    appendSeparatorAndKey(key);
    char buf[24]{};
    const std::to_chars_result res = std::to_chars(buf, buf + sizeof(buf), value, 10);
    if (res.ec == std::errc{})
    {
        m_buffer.append(buf, static_cast<gp::USize>(res.ptr - buf));
    }
}

void MetadataSerializer::appendValue(const gp::StringView key, const gp::Int64 value) noexcept
{
    appendSeparatorAndKey(key);
    char buf[24]{};
    const std::to_chars_result res = std::to_chars(buf, buf + sizeof(buf), value, 10);
    if (res.ec == std::errc{})
    {
        m_buffer.append(buf, static_cast<gp::USize>(res.ptr - buf));
    }
}

void MetadataSerializer::appendValue(const gp::StringView key, const gp::Float64 value) noexcept
{
    appendSeparatorAndKey(key);
    char buf[48]{};
    const int written = std::snprintf(buf, sizeof(buf), "%.4f", value);
    if (written > 0)
    {
        m_buffer.append(buf, static_cast<gp::USize>(written));
    }
}

gp::String MetadataSerializer::toString() const noexcept
{
    gp::String out{};
    out.reserve(m_buffer.size() + 2u);
    out.pushBack('{');
    out.append(m_buffer.view());
    out.pushBack('}');
    return out;
}

namespace
{

struct OpenSpan
{
    gp::UInt64 startTimestampUs{ 0u };
    gp::String name{};
    gp::String metadata{};
};

struct QueueTraceTrack
{
    gp::UInt32 queueId{ 0u };
    gp::String queueName{};
    gp::UInt64 activeFrame{ 0u };
    gp::Vector<OpenSpan> stack{};
};

struct TraceState
{
    gp::thread::Atomic<bool> enabled{ false };
    gp::thread::Mutex mutex{};
    gp::Vector<QueueTraceTrack> queues{};
    gp::Vector<GpuProfilerTrace::RecordedSpan> completedSpans{};

    [[nodiscard]] QueueTraceTrack* findTrack(const gp::UInt32 queueId) noexcept
    {
        for (QueueTraceTrack& track: queues)
        {
            if (track.queueId == queueId)
            {
                return &track;
            }
        }
        return nullptr;
    }

    [[nodiscard]] QueueTraceTrack& findOrAddTrack(const gp::UInt32 queueId) noexcept
    {
        if (QueueTraceTrack* const existing = findTrack(queueId); existing != nullptr)
        {
            return *existing;
        }
        QueueTraceTrack newTrack{};
        newTrack.queueId = queueId;
        queues.emplaceBack(static_cast<QueueTraceTrack&&>(newTrack));
        return queues[queues.size() - 1u];
    }
};

TraceState& getTraceState() noexcept
{
    static TraceState s_state{};
    return s_state;
}

}   // namespace

void GpuProfilerTrace::setEnabled(const bool enabled) noexcept
{
    getTraceState().enabled.store(enabled, gp::memory::MemoryOrder::Release);
}

bool GpuProfilerTrace::isEnabled() noexcept
{
    return getTraceState().enabled.load(gp::memory::MemoryOrder::Acquire);
}

void GpuProfilerTrace::initQueue(const gp::UInt32 queueId, const gp::StringView queueName) noexcept
{
    TraceState& state = getTraceState();
    gp::thread::UniqueLock<gp::thread::Mutex> lock(state.mutex);
    QueueTraceTrack& track = state.findOrAddTrack(queueId);
    track.queueName = gp::String(queueName);
}

void GpuProfilerTrace::beginFrame(
    const gp::UInt32 queueId,
    const gp::UInt64 frameNumber,
    const gp::UInt64 calibrationCpuUs,
    const gp::UInt64 calibrationGpuUs
) noexcept
{
    static_cast<void>(calibrationCpuUs);
    static_cast<void>(calibrationGpuUs);
    if (!isEnabled())
    {
        return;
    }

    TraceState& state = getTraceState();
    gp::thread::UniqueLock<gp::thread::Mutex> lock(state.mutex);
    QueueTraceTrack& track = state.findOrAddTrack(queueId);
    track.activeFrame = frameNumber;
    track.stack.clear();
}

void GpuProfilerTrace::beginEvent(
    const gp::UInt32 queueId, const gp::StringView name, const gp::UInt64 timestampUs, const gp::StringView metadata
) noexcept
{
    if (!isEnabled())
    {
        return;
    }

    TraceState& state = getTraceState();
    gp::thread::UniqueLock<gp::thread::Mutex> lock(state.mutex);
    QueueTraceTrack& track = state.findOrAddTrack(queueId);
    OpenSpan span{};
    span.startTimestampUs = timestampUs;
    span.name = gp::String(name);
    span.metadata = gp::String(metadata);
    track.stack.emplaceBack(static_cast<OpenSpan&&>(span));
}

void GpuProfilerTrace::endEvent(const gp::UInt32 queueId, const gp::UInt64 timestampUs) noexcept
{
    if (!isEnabled())
    {
        return;
    }

    TraceState& state = getTraceState();
    gp::thread::UniqueLock<gp::thread::Mutex> lock(state.mutex);
    QueueTraceTrack* const track = state.findTrack(queueId);
    if (track == nullptr || track->stack.isEmpty())
    {
        return;
    }

    const gp::UInt32 depth = static_cast<gp::UInt32>(track->stack.size() - 1u);
    OpenSpan open = static_cast<OpenSpan&&>(track->stack[depth]);
    track->stack.popBack();

    RecordedSpan completed{};
    completed.frameNumber = track->activeFrame;
    completed.queueId = queueId;
    completed.startTimestampUs = open.startTimestampUs;
    completed.endTimestampUs = (timestampUs >= open.startTimestampUs) ? timestampUs : open.startTimestampUs;
    completed.depth = depth;
    completed.name = static_cast<gp::String&&>(open.name);
    completed.metadata = static_cast<gp::String&&>(open.metadata);
    state.completedSpans.emplaceBack(static_cast<RecordedSpan&&>(completed));
}

void GpuProfilerTrace::endFrame(const gp::UInt32 queueId) noexcept
{
    if (!isEnabled())
    {
        return;
    }

    TraceState& state = getTraceState();
    gp::thread::UniqueLock<gp::thread::Mutex> lock(state.mutex);
    if (QueueTraceTrack* const track = state.findTrack(queueId); track != nullptr)
    {
        track->stack.clear();
    }
}

gp::USize GpuProfilerTrace::getRecordedSpanCount() noexcept
{
    TraceState& state = getTraceState();
    gp::thread::UniqueLock<gp::thread::Mutex> lock(state.mutex);
    return state.completedSpans.size();
}

void GpuProfilerTrace::getRecordedSpans(gp::Vector<RecordedSpan>& outSpans) noexcept
{
    TraceState& state = getTraceState();
    gp::thread::UniqueLock<gp::thread::Mutex> lock(state.mutex);
    outSpans = state.completedSpans;
}

void GpuProfilerTrace::reset() noexcept
{
    TraceState& state = getTraceState();
    state.enabled.store(false, gp::memory::MemoryOrder::Release);
    gp::thread::UniqueLock<gp::thread::Mutex> lock(state.mutex);
    state.queues.clear();
    state.completedSpans.clear();
}

}   // namespace gp::rhi::profiling
