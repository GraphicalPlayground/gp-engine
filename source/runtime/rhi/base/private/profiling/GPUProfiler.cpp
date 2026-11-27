// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "profiling/GPUProfiler.hpp"
#include "containers/arrays/Vector.hpp"
#include "containers/strings/String.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "memory/GlobalMemory.hpp"
#include "memory/MemoryOrder.hpp"
#include "profiling/GpuProfilerTrace.hpp"
#include "threading/atomics/Atomic.hpp"
#include "threading/sync/Mutex.hpp"
#include "threading/sync/UniqueLock.hpp"
#include <cstdio>
#include <new>   // IWYU pragma: keep

namespace gp::rhi::profiling
{

GPUFrameTimeHistory g_rhiGPUFrameTimeHistory[kMaxNumGPUs]{};

namespace
{

struct ChunkPool
{
    gp::thread::Mutex mutex{};
    EventStream::Chunk* freeHead{ nullptr };

    [[nodiscard]] EventStream::Chunk* acquire() noexcept
    {
        {
            gp::thread::UniqueLock<gp::thread::Mutex> lock(mutex);
            if (freeHead != nullptr)
            {
                EventStream::Chunk* const chunk = freeHead;
                freeHead = chunk->next;
                chunk->next = nullptr;
                chunk->count = 0u;
                return chunk;
            }
        }

        gp::memory::Malloc* const allocator = gp::memory::getGlobalMalloc();
        GP_CHECKF(allocator != nullptr, "Global allocator must be valid for GPUProfiler EventStream::Chunk.");
        void* const mem =
            allocator->allocate(sizeof(EventStream::Chunk), static_cast<gp::UInt32>(alignof(EventStream::Chunk)));
        return ::new (mem) EventStream::Chunk();
    }

    void releaseChain(EventStream::Chunk* first, EventStream::Chunk* last) noexcept
    {
        if (first == nullptr || last == nullptr)
        {
            return;
        }

        gp::thread::UniqueLock<gp::thread::Mutex> lock(mutex);
        last->next = freeHead;
        freeHead = first;
    }

    void drainAll() noexcept
    {
        EventStream::Chunk* list = nullptr;
        {
            gp::thread::UniqueLock<gp::thread::Mutex> lock(mutex);
            list = freeHead;
            freeHead = nullptr;
        }

        gp::memory::Malloc* const allocator = gp::memory::getGlobalMalloc();
        while (list != nullptr)
        {
            EventStream::Chunk* const next = list->next;
            list->~Chunk();
            if (allocator != nullptr)
            {
                allocator->deallocate(list);
            }
            list = next;
        }
    }
};

ChunkPool& getChunkPool() noexcept
{
    static ChunkPool s_pool{};
    return s_pool;
}

struct ActiveBreadcrumbEntry
{
    diagnostics::BreadcrumbNode* node{ nullptr };
    gp::UInt64 startTimestampUs{ 0u };
    gp::UInt64 childDurationUs{ 0u };
    gp::UInt32 draws{ 0u };
    gp::UInt32 primitives{ 0u };
    gp::UInt32 dispatches{ 0u };
};

struct SignaledFenceEntry
{
    gp::UInt64 id{ 0u };
    gp::UInt64 cpuTimestampUs{ 0u };
};

struct QueueState
{
    Queue queue{};
    gp::UInt64 currentFrameNumber{ 0u };
    GPUStat* pendingFrameStat{ nullptr };
    gp::UInt64 activeWorkStartUs{ 0u };
    gp::UInt64 accumulatedFrameBusyUs{ 0u };
    gp::UInt32 frameDraws{ 0u };
    gp::UInt32 framePrimitives{ 0u };
    gp::UInt32 frameDispatches{ 0u };
    bool insideWork{ false };
    gp::Vector<ActiveBreadcrumbEntry> breadcrumbStack{};
    gp::Vector<SignaledFenceEntry> signaledFences{};

    void recordFence(const gp::UInt64 id, const gp::UInt64 cpuTimestampUs) noexcept
    {
        for (SignaledFenceEntry& entry: signaledFences)
        {
            if (entry.id == id)
            {
                entry.cpuTimestampUs = cpuTimestampUs;
                return;
            }
        }
        signaledFences.emplaceBack(SignaledFenceEntry{ id, cpuTimestampUs });
    }

    [[nodiscard]] const SignaledFenceEntry* findFence(const gp::UInt64 id) const noexcept
    {
        for (const SignaledFenceEntry& entry: signaledFences)
        {
            if (entry.id == id)
            {
                return &entry;
            }
        }
        return nullptr;
    }
};

struct CapturedEventRecord
{
    Queue queue{};
    gp::String name{};
    gp::UInt64 startUs{ 0u };
    gp::UInt64 endUs{ 0u };
    gp::UInt32 draws{ 0u };
    gp::UInt32 primitives{ 0u };
    gp::UInt32 dispatches{ 0u };
};

struct ProfilerGlobalState
{
    gp::thread::Mutex mutex{};
    gp::Vector<QueueState> queues{};
    gp::thread::Atomic<bool> requestProfileNextFrame{ false };
    gp::thread::Atomic<bool> currentlyProfiling{ false };
    gp::UInt64 captureTargetFrame{ 0u };
    gp::Vector<CapturedEventRecord> capturedEvents{};

    GlobalGPUTimingObserverFn globalObserver{ nullptr };
    void* globalObserverUserData{ nullptr };

    GPUProfileCaptureFn captureCallback{ nullptr };
    void* captureCallbackUserData{ nullptr };

    gp::thread::Atomic<gp::UInt64> lastFrameBusyUs[kMaxNumGPUs]{};
    gp::thread::Atomic<gp::UInt32> latchedFrameCycles[kMaxNumGPUs]{};

    [[nodiscard]] QueueState* findQueue(const gp::UInt32 queueValue) noexcept
    {
        for (QueueState& q: queues)
        {
            if (q.queue.value == queueValue)
            {
                return &q;
            }
        }
        return nullptr;
    }

    [[nodiscard]] QueueState& findOrAddQueue(const Queue queue) noexcept
    {
        if (QueueState* const existing = findQueue(queue.value); existing != nullptr)
        {
            return *existing;
        }
        QueueState created{};
        created.queue = queue;
        queues.emplaceBack(static_cast<QueueState&&>(created));
        return queues[queues.size() - 1u];
    }
};

ProfilerGlobalState& getProfilerState() noexcept
{
    static ProfilerGlobalState s_state{};
    return s_state;
}

[[nodiscard]] gp::String
    buildCaptureJson(const gp::UInt64 frameNumber, const gp::Vector<CapturedEventRecord>& records) noexcept
{
    gp::String json{};
    json.reserve(128u + records.size() * 128u);
    char buf[512]{};
    const int headerLen = std::snprintf(
        buf, sizeof(buf), "{\"frameNumber\":%llu,\"events\":[", static_cast<unsigned long long>(frameNumber)
    );
    if (headerLen > 0)
    {
        json.append(buf, static_cast<gp::USize>(headerLen));
    }

    for (gp::USize idx = 0u; idx < records.size(); ++idx)
    {
        const CapturedEventRecord& rec = records[idx];
        const int entryLen = std::snprintf(
            buf,
            sizeof(buf),
            "%s{\"queue\":\"%s\",\"gpu\":%u,\"name\":\"%s\",\"startUs\":%llu,\"endUs\":%llu,\"durationUs\":%llu," "\"draws\":%u,\"primitives\":%u,\"dispatches\":%u}",
            (idx > 0u) ? "," : "",
            rec.queue.getTypeString(),
            static_cast<gp::UInt32>(rec.queue.gpu),
            rec.name.cString(),
            static_cast<unsigned long long>(rec.startUs),
            static_cast<unsigned long long>(rec.endUs),
            static_cast<unsigned long long>(rec.endUs >= rec.startUs ? (rec.endUs - rec.startUs) : 0u),
            rec.draws,
            rec.primitives,
            rec.dispatches
        );
        if (entryLen > 0)
        {
            json.append(buf, static_cast<gp::USize>(entryLen));
        }
    }

    json.append("]}");
    return json;
}

}   // namespace

EventStream::EventStream(EventStream&& other) noexcept
    : m_firstChunk(other.m_firstChunk)
    , m_lastChunk(other.m_lastChunk)
    , m_numEvents(other.m_numEvents)
    , m_breadcrumbAllocators(static_cast<diagnostics::BreadcrumbAllocatorArray&&>(other.m_breadcrumbAllocators))
{
    other.m_firstChunk = nullptr;
    other.m_lastChunk = nullptr;
    other.m_numEvents = 0u;
}

EventStream& EventStream::operator=(EventStream&& other) noexcept
{
    if (this != &other)
    {
        reset();
        m_firstChunk = other.m_firstChunk;
        m_lastChunk = other.m_lastChunk;
        m_numEvents = other.m_numEvents;
        m_breadcrumbAllocators = static_cast<diagnostics::BreadcrumbAllocatorArray&&>(other.m_breadcrumbAllocators);
        other.m_firstChunk = nullptr;
        other.m_lastChunk = nullptr;
        other.m_numEvents = 0u;
    }
    return *this;
}

EventStream::~EventStream() noexcept
{
    reset();
}

Event* EventStream::allocEventSlot() noexcept
{
    if (m_lastChunk == nullptr || m_lastChunk->count >= Chunk::kMaxEvents)
    {
        Chunk* const newChunk = getChunkPool().acquire();
        if (m_lastChunk != nullptr)
        {
            m_lastChunk->next = newChunk;
        }
        else
        {
            m_firstChunk = newChunk;
        }
        m_lastChunk = newChunk;
    }

    Event* const slot = &m_lastChunk->events[m_lastChunk->count];
    ++m_lastChunk->count;
    ++m_numEvents;
    return slot;
}

void EventStream::append(EventStream&& other) noexcept
{
    if (other.m_firstChunk == nullptr)
    {
        m_breadcrumbAllocators.appendUnique(other.m_breadcrumbAllocators);
        other.reset();
        return;
    }

    if (m_lastChunk != nullptr)
    {
        m_lastChunk->next = other.m_firstChunk;
        m_lastChunk = other.m_lastChunk;
    }
    else
    {
        m_firstChunk = other.m_firstChunk;
        m_lastChunk = other.m_lastChunk;
    }

    m_numEvents += other.m_numEvents;
    m_breadcrumbAllocators.appendUnique(other.m_breadcrumbAllocators);

    other.m_firstChunk = nullptr;
    other.m_lastChunk = nullptr;
    other.m_numEvents = 0u;
    other.m_breadcrumbAllocators.clear();
}

void EventStream::reset() noexcept
{
    if (m_firstChunk != nullptr)
    {
        getChunkPool().releaseChain(m_firstChunk, m_lastChunk);
        m_firstChunk = nullptr;
        m_lastChunk = nullptr;
    }
    m_numEvents = 0u;
    m_breadcrumbAllocators.clear();
}

OnTimingResultsAction GPUStat::notifyResult(const GPUStatTimingResult& result) noexcept
{
    if (!m_callback)
    {
        return OnTimingResultsAction::Keep;
    }

    const OnTimingResultsAction action = m_callback(result);
    if (action == OnTimingResultsAction::Delete)
    {
        delete this;
    }
    return action;
}

void initializeQueues(const gp::ConstVectorView<Queue> queues) noexcept
{
    ProfilerGlobalState& state = getProfilerState();
    gp::thread::UniqueLock<gp::thread::Mutex> lock(state.mutex);
    for (const Queue& q: queues)
    {
        static_cast<void>(state.findOrAddQueue(q));

        char labelBuf[64]{};
        const int len = std::snprintf(
            labelBuf,
            sizeof(labelBuf),
            "%s_GPU%u_%u",
            q.getTypeString(),
            static_cast<gp::UInt32>(q.gpu),
            static_cast<gp::UInt32>(q.index)
        );
        if (len > 0)
        {
            GpuProfilerTrace::initQueue(q.value, gp::StringView(labelBuf, static_cast<gp::USize>(len)));
        }
    }
}

void processEvents(const Queue queue, EventStream&& stream) noexcept
{
    if (stream.isEmpty())
    {
        return;
    }

    ProfilerGlobalState& state = getProfilerState();
    gp::Vector<GPUStatTimingResult> observerNotifications{};
    gp::String completedCaptureJson{};
    GPUProfileCaptureFn captureCbToInvoke = nullptr;
    void* captureUserDataToInvoke = nullptr;

    {
        gp::thread::UniqueLock<gp::thread::Mutex> lock(state.mutex);
        QueueState* const qState = &state.findOrAddQueue(queue);

        diagnostics::BreadcrumbBuffer scratch{};
        for (EventStream::Chunk* chunk = stream.m_firstChunk; chunk != nullptr; chunk = chunk->next)
        {
            for (gp::UInt32 idx = 0u; idx < chunk->count; ++idx)
            {
                const Event& ev = chunk->events[idx];
                switch (ev.type)
                {
                case Event::Type::BeginWork:
                {
                    qState->insideWork = true;
                    qState->activeWorkStartUs = ev.data.beginWork.gpuTimestampTOP;
                    break;
                }

                case Event::Type::EndWork:
                {
                    if (qState->insideWork)
                    {
                        const gp::UInt64 endUs = ev.data.endWork.gpuTimestampBOP;
                        if (endUs > qState->activeWorkStartUs)
                        {
                            qState->accumulatedFrameBusyUs += (endUs - qState->activeWorkStartUs);
                        }
                        qState->insideWork = false;
                    }
                    break;
                }

                case Event::Type::Stats:
                {
                    qState->frameDraws += ev.data.stats.numDraws;
                    qState->framePrimitives += ev.data.stats.numPrimitives;
                    qState->frameDispatches += ev.data.stats.numDispatches;
                    if (!qState->breadcrumbStack.isEmpty())
                    {
                        ActiveBreadcrumbEntry& top = qState->breadcrumbStack[qState->breadcrumbStack.size() - 1u];
                        top.draws += ev.data.stats.numDraws;
                        top.primitives += ev.data.stats.numPrimitives;
                        top.dispatches += ev.data.stats.numDispatches;
                    }
                    break;
                }

                case Event::Type::BeginBreadcrumb:
                {
                    ActiveBreadcrumbEntry entry{};
                    entry.node = ev.data.beginBreadcrumb.breadcrumb;
                    entry.startTimestampUs = ev.data.beginBreadcrumb.gpuTimestampTOP;
                    qState->breadcrumbStack.emplaceBack(entry);

                    const char* const nameStr =
                        (entry.node != nullptr) ? entry.node->getDebugStr(scratch) : "<unnamed>";
                    GpuProfilerTrace::beginEvent(queue.value, gp::StringView(nameStr), entry.startTimestampUs);
                    break;
                }

                case Event::Type::EndBreadcrumb:
                {
                    const gp::UInt64 endUs = ev.data.endBreadcrumb.gpuTimestampBOP;
                    GpuProfilerTrace::endEvent(queue.value, endUs);

                    if (!qState->breadcrumbStack.isEmpty())
                    {
                        const ActiveBreadcrumbEntry popped =
                            qState->breadcrumbStack[qState->breadcrumbStack.size() - 1u];
                        qState->breadcrumbStack.popBack();

                        const gp::UInt64 inclusiveUs =
                            (endUs > popped.startTimestampUs) ? (endUs - popped.startTimestampUs) : 0u;
                        const gp::UInt64 exclusiveUs =
                            (inclusiveUs > popped.childDurationUs) ? (inclusiveUs - popped.childDurationUs) : 0u;

                        if (!qState->breadcrumbStack.isEmpty())
                        {
                            qState->breadcrumbStack[qState->breadcrumbStack.size() - 1u].childDurationUs += inclusiveUs;
                            qState->breadcrumbStack[qState->breadcrumbStack.size() - 1u].draws += popped.draws;
                            qState->breadcrumbStack[qState->breadcrumbStack.size() - 1u].primitives +=
                                popped.primitives;
                            qState->breadcrumbStack[qState->breadcrumbStack.size() - 1u].dispatches +=
                                popped.dispatches;
                        }

                        const char* const label =
                            (popped.node != nullptr) ? popped.node->getDebugStr(scratch) : "<unnamed>";
                        GPUStatTimingResult res{};
                        res.frameNumber = qState->currentFrameNumber;
                        res.queue = queue;
                        res.statName = gp::Name(label);
                        res.inclusiveMicroseconds = inclusiveUs;
                        res.exclusiveMicroseconds = exclusiveUs;
                        res.numDraws = popped.draws;
                        res.numPrimitives = popped.primitives;
                        res.numDispatches = popped.dispatches;
                        observerNotifications.emplaceBack(res);

                        if (state.currentlyProfiling.load(gp::memory::MemoryOrder::Relaxed))
                        {
                            CapturedEventRecord rec{};
                            rec.queue = queue;
                            rec.name = gp::String(label);
                            rec.startUs = popped.startTimestampUs;
                            rec.endUs = endUs;
                            rec.draws = popped.draws;
                            rec.primitives = popped.primitives;
                            rec.dispatches = popped.dispatches;
                            state.capturedEvents.emplaceBack(static_cast<CapturedEventRecord&&>(rec));
                        }
                    }
                    break;
                }

                case Event::Type::SignalFence:
                {
                    qState->recordFence(ev.data.signalFence.id, ev.data.signalFence.cpuTimestampUs);
                    break;
                }

                case Event::Type::WaitFence:
                {
                    if (const QueueState* const targetTrack = state.findQueue(ev.data.waitFence.targetQueue.value);
                        targetTrack != nullptr)
                    {
                        static_cast<void>(targetTrack->findFence(ev.data.waitFence.id));
                    }
                    break;
                }

                case Event::Type::FrameTime:
                {
                    qState->accumulatedFrameBusyUs = ev.data.frameTime.durationMicroseconds;
                    break;
                }

                case Event::Type::FrameBoundary:
                {
                    const gp::UInt64 completedFrame = qState->currentFrameNumber;
                    const gp::UInt64 frameBusyUs = qState->accumulatedFrameBusyUs;

                    if (queue.gpu < kMaxNumGPUs && queue.type == Queue::Type::Graphics)
                    {
                        state.lastFrameBusyUs[queue.gpu].store(frameBusyUs, gp::memory::MemoryOrder::Relaxed);
                        state.latchedFrameCycles[queue.gpu].store(
                            static_cast<gp::UInt32>(frameBusyUs), gp::memory::MemoryOrder::Relaxed
                        );
                        g_rhiGPUFrameTimeHistory[queue.gpu].pushFrameCycles(1'000'000.0, frameBusyUs);
                    }

                    if (qState->pendingFrameStat != nullptr)
                    {
                        GPUStatTimingResult frameRes{};
                        frameRes.frameNumber = completedFrame;
                        frameRes.queue = queue;
                        frameRes.statName = qState->pendingFrameStat->getName();
                        frameRes.inclusiveMicroseconds = frameBusyUs;
                        frameRes.exclusiveMicroseconds = frameBusyUs;
                        frameRes.numDraws = qState->frameDraws;
                        frameRes.numPrimitives = qState->framePrimitives;
                        frameRes.numDispatches = qState->frameDispatches;
                        static_cast<void>(qState->pendingFrameStat->notifyResult(frameRes));
                        qState->pendingFrameStat = nullptr;
                    }

                    if (ev.data.frameBoundary.stat != nullptr)
                    {
                        qState->pendingFrameStat = ev.data.frameBoundary.stat;
                    }

                    GpuProfilerTrace::endFrame(queue.value);
                    qState->currentFrameNumber = ev.data.frameBoundary.frameNumber;
                    qState->accumulatedFrameBusyUs = 0u;
                    qState->frameDraws = 0u;
                    qState->framePrimitives = 0u;
                    qState->frameDispatches = 0u;
                    GpuProfilerTrace::beginFrame(queue.value, qState->currentFrameNumber);

                    if (queue.type == Queue::Type::Graphics && queue.gpu == 0u)
                    {
                        if (state.currentlyProfiling.load(gp::memory::MemoryOrder::Acquire))
                        {
                            completedCaptureJson = buildCaptureJson(completedFrame, state.capturedEvents);
                            captureCbToInvoke = state.captureCallback;
                            captureUserDataToInvoke = state.captureCallbackUserData;
                            state.capturedEvents.clear();
                            state.currentlyProfiling.store(false, gp::memory::MemoryOrder::Release);
                        }
                        else if (state.requestProfileNextFrame.exchange(false, gp::memory::MemoryOrder::AcqRel))
                        {
                            state.capturedEvents.clear();
                            state.captureTargetFrame = qState->currentFrameNumber;
                            state.currentlyProfiling.store(true, gp::memory::MemoryOrder::Release);
                        }
                    }
                    break;
                }

                case Event::Type::Flip:
                case Event::Type::Vsync:
                default:
                    break;
                }
            }
        }
    }

    // Invoke external observers outside the profiler lock to prevent re-entrancy deadlocks.
    const GlobalGPUTimingObserverFn obs = state.globalObserver;
    void* const obsUser = state.globalObserverUserData;
    if (obs != nullptr)
    {
        for (const GPUStatTimingResult& notification: observerNotifications)
        {
            obs(notification, obsUser);
        }
    }

    if (captureCbToInvoke != nullptr && !completedCaptureJson.isEmpty())
    {
        captureCbToInvoke(completedCaptureJson.view(), captureUserDataToInvoke);
    }

    stream.reset();
}

void profileNextFrame() noexcept
{
    getProfilerState().requestProfileNextFrame.store(true, gp::memory::MemoryOrder::Release);
}

bool consumeShouldProfileNextFrame() noexcept
{
    ProfilerGlobalState& state = getProfilerState();
    if (state.requestProfileNextFrame.exchange(false, gp::memory::MemoryOrder::AcqRel))
    {
        state.currentlyProfiling.store(true, gp::memory::MemoryOrder::Release);
        return true;
    }
    return false;
}

bool isProfiling() noexcept
{
    return getProfilerState().currentlyProfiling.load(gp::memory::MemoryOrder::Acquire);
}

void setGlobalTimingObserver(const GlobalGPUTimingObserverFn observer, void* const userData) noexcept
{
    ProfilerGlobalState& state = getProfilerState();
    gp::thread::UniqueLock<gp::thread::Mutex> lock(state.mutex);
    state.globalObserver = observer;
    state.globalObserverUserData = userData;
}

void setProfileCaptureCallback(const GPUProfileCaptureFn callback, void* const userData) noexcept
{
    ProfilerGlobalState& state = getProfilerState();
    gp::thread::UniqueLock<gp::thread::Mutex> lock(state.mutex);
    state.captureCallback = callback;
    state.captureCallbackUserData = userData;
}

gp::UInt64 getLastGPUFrameMicroseconds(const gp::UInt32 gpuIndex) noexcept
{
    if (gpuIndex >= kMaxNumGPUs)
    {
        return 0u;
    }
    return getProfilerState().lastFrameBusyUs[gpuIndex].load(gp::memory::MemoryOrder::Relaxed);
}

void resetGPUProfiler() noexcept
{
    ProfilerGlobalState& state = getProfilerState();
    {
        gp::thread::UniqueLock<gp::thread::Mutex> lock(state.mutex);
        state.queues.clear();
        state.capturedEvents.clear();
        state.requestProfileNextFrame.store(false, gp::memory::MemoryOrder::Relaxed);
        state.currentlyProfiling.store(false, gp::memory::MemoryOrder::Relaxed);
        state.captureTargetFrame = 0u;
        state.globalObserver = nullptr;
        state.globalObserverUserData = nullptr;
        state.captureCallback = nullptr;
        state.captureCallbackUserData = nullptr;
        for (gp::UInt32 gpu = 0u; gpu < kMaxNumGPUs; ++gpu)
        {
            state.lastFrameBusyUs[gpu].store(0u, gp::memory::MemoryOrder::Relaxed);
            state.latchedFrameCycles[gpu].store(0u, gp::memory::MemoryOrder::Relaxed);
            g_rhiGPUFrameTimeHistory[gpu].clear();
        }
    }
    getChunkPool().drainAll();
}

void GPUFrameTimeHistory::pushFrameCycles(
    const gp::Float64 timestampFrequencyHz, const gp::UInt64 gpuBusyTicks
) noexcept
{
    Sample sample{};
    if (timestampFrequencyHz <= 0.0)
    {
        sample.disjoint = true;
        sample.cycles = 0u;
    }
    else
    {
        const gp::Float64 microseconds = (static_cast<gp::Float64>(gpuBusyTicks) * 1'000'000.0) / timestampFrequencyHz;
        sample.cycles = (microseconds > 0.0) ? static_cast<gp::UInt32>(microseconds + 0.5) : 0u;
        sample.disjoint = false;
    }

    if (m_count == kCapacity)
    {
        m_head = (m_head + 1u) % kCapacity;
        --m_count;
    }

    m_samples[m_tail] = sample;
    m_tail = (m_tail + 1u) % kCapacity;
    ++m_count;
}

GPUFrameTimeHistory::Result GPUFrameTimeHistory::popFrameCycles(gp::UInt32& outCycles) noexcept
{
    outCycles = 0u;
    if (m_count == 0u)
    {
        return Result::Empty;
    }

    const Sample sample = m_samples[m_head];
    m_head = (m_head + 1u) % kCapacity;
    --m_count;

    if (sample.disjoint)
    {
        return Result::Disjoint;
    }

    outCycles = sample.cycles;
    return Result::Ok;
}

void GPUFrameTimeHistory::clear() noexcept
{
    m_head = 0u;
    m_tail = 0u;
    m_count = 0u;
}

gp::UInt32 getGPUFrameCycles(const gp::UInt32 gpuIndex) noexcept
{
    if (gpuIndex >= kMaxNumGPUs)
    {
        return 0u;
    }
    return getProfilerState().latchedFrameCycles[gpuIndex].load(gp::memory::MemoryOrder::Relaxed);
}

}   // namespace gp::rhi::profiling
