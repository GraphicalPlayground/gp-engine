// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/strings/Name.hpp"
#include "containers/strings/String.hpp"     // IWYU pragma: keep
#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "core/GPUMask.hpp"
#include "CoreMinimal.hpp"
#include "diagnostics/Breadcrumbs.hpp"
#include "profiling/ProfilingForward.hpp"   // IWYU pragma: keep
#include "RHIBase.hpp"
#include "templates/Function.hpp"
#include <new>   // IWYU pragma: keep

namespace gp::rhi::profiling
{

/// @brief Identifies a hardware queue `(Type, gpu, index)` inside the multi-queue GPU profiler.
struct Queue
{
    /// @brief Hardware queue engine family.
    enum class Type : gp::UInt8
    {
        Graphics = 0u,
        Compute = 1u,
        Copy = 2u,
        SwapChain = 3u,
        Count = 4u
    };

    Type type{ Type::Graphics };
    gp::UInt8 gpu{ 0u };
    gp::UInt8 index{ 0u };
    gp::UInt8 padding{ 0u };

    /// @brief Packed 32-bit `(type | gpu << 8 | index << 16)` identifier for O(1) equality and trace queue IDs.
    gp::UInt32 value{ 0u };

    /// @brief Constructs `(Graphics, 0, 0)`.
    constexpr Queue() noexcept = default;

    /// @brief Constructs a queue identifier from `(inType, inGpu, inIndex)`.
    /// @param[in] inType Hardware queue family.
    /// @param[in] inGpu Zero-based GPU adapter index.
    /// @param[in] inIndex Queue instance index within `inType`.
    constexpr Queue(const Type inType, const gp::UInt8 inGpu = 0u, const gp::UInt8 inIndex = 0u) noexcept
        : type(inType)
        , gpu(inGpu)
        , index(inIndex)
        , padding(0u)
        , value(
              static_cast<gp::UInt32>(inType) | (static_cast<gp::UInt32>(inGpu) << 8u) |
              (static_cast<gp::UInt32>(inIndex) << 16u)
          )
    {}

    /// @brief Returns the human-readable string for `type` (`"Graphics"`, `"Compute"`, `"Copy"`, `"SwapChain"`).
    /// @return Static null-terminated string.
    [[nodiscard]] const char* getTypeString() const noexcept
    {
        switch (type)
        {
        case Type::Graphics:
            return "Graphics";
        case Type::Compute:
            return "Compute";
        case Type::Copy:
            return "Copy";
        case Type::SwapChain:
            return "SwapChain";
        default:
            return "Unknown";
        }
    }

    /// @brief Equality comparison on `value`.
    /// @param[in] rhs Queue to compare against.
    /// @return True when type, gpu, and index match.
    [[nodiscard]] constexpr bool operator==(const Queue& rhs) const noexcept
    {
        return value == rhs.value;
    }

    /// @brief Appends `value` to a streaming hasher (`gp::Map` / `gp::Set` support).
    /// @tparam InHasher Streaming hasher type.
    /// @param[in,out] hasher Hasher accumulator.
    template <typename InHasher>
    constexpr void hashAppend(InHasher& hasher) const noexcept
    {
        hasher.append(value);
    }
};

/// @brief Discriminated GPU profiler event record produced by backends on queue submissions and retires.
struct Event
{
    /// @brief Discriminator identifying the active payload inside `Event`.
    enum class Type : gp::UInt8
    {
        FrameBoundary = 0u,
        FrameTime,
        BeginBreadcrumb,
        EndBreadcrumb,
        BeginWork,
        EndWork,
        Stats,
        SignalFence,
        WaitFence,
        Flip,
        Vsync
    };

    /// @brief Frame boundary event emitted at `DynamicRHI::endFrame()`.
    struct FrameBoundary
    {
        gp::UInt64 frameNumber{ 0u };
        diagnostics::BreadcrumbNode* breadcrumb{ nullptr };
        GPUStat* stat{ nullptr };
    };

    /// @brief Explicit GPU frame duration override event (in microseconds).
    struct FrameTime
    {
        gp::UInt64 durationMicroseconds{ 0u };
    };

    /// @brief GPU breadcrumb scope entry timestamp (in microseconds).
    struct BeginBreadcrumb
    {
        diagnostics::BreadcrumbNode* breadcrumb{ nullptr };
        gp::UInt64 gpuTimestampTOP{ 0u };
    };

    /// @brief GPU breadcrumb scope exit timestamp (in microseconds).
    struct EndBreadcrumb
    {
        diagnostics::BreadcrumbNode* breadcrumb{ nullptr };
        gp::UInt64 gpuTimestampBOP{ 0u };
    };

    /// @brief Hardware queue active work burst start (`cpuTimestampUs` when submitted, `gpuTimestampTOP` when begun).
    struct BeginWork
    {
        gp::UInt64 cpuTimestampUs{ 0u };
        gp::UInt64 gpuTimestampTOP{ 0u };
    };

    /// @brief Hardware queue active work burst end (`gpuTimestampBOP` in microseconds).
    struct EndWork
    {
        gp::UInt64 gpuTimestampBOP{ 0u };
    };

    /// @brief Per-scope or per-work-burst draw, primitive, and dispatch counts.
    struct Stats
    {
        gp::UInt32 numDraws{ 0u };
        gp::UInt32 numPrimitives{ 0u };
        gp::UInt32 numDispatches{ 0u };
    };

    /// @brief Cross-queue fence signal event resolved against matching `WaitFence` events.
    struct SignalFence
    {
        gp::UInt64 cpuTimestampUs{ 0u };
        gp::UInt64 id{ 0u };
    };

    /// @brief Cross-queue fence wait event stalling until `targetQueue` signals `id`.
    struct WaitFence
    {
        gp::UInt64 cpuTimestampUs{ 0u };
        gp::UInt64 id{ 0u };
        Queue targetQueue{};
    };

    /// @brief Swapchain flip timestamp in microseconds.
    struct Flip
    {
        gp::UInt64 gpuTimestampUs{ 0u };
    };

    /// @brief Display vertical-blank timestamp in microseconds.
    struct Vsync
    {
        gp::UInt64 gpuTimestampUs{ 0u };
    };

    Type type{ Type::Stats };

    union Payload
    {
        FrameBoundary frameBoundary;
        FrameTime frameTime;
        BeginBreadcrumb beginBreadcrumb;
        EndBreadcrumb endBreadcrumb;
        BeginWork beginWork;
        EndWork endWork;
        Stats stats;
        SignalFence signalFence;
        WaitFence waitFence;
        Flip flip;
        Vsync vsync;

        constexpr Payload() noexcept
            : stats{}
        {}
    } data{};

    constexpr Event() noexcept = default;

    explicit constexpr Event(const FrameBoundary& v) noexcept
        : type(Type::FrameBoundary)
    {
        data.frameBoundary = v;
    }

    explicit constexpr Event(const FrameTime& v) noexcept
        : type(Type::FrameTime)
    {
        data.frameTime = v;
    }

    explicit constexpr Event(const BeginBreadcrumb& v) noexcept
        : type(Type::BeginBreadcrumb)
    {
        data.beginBreadcrumb = v;
    }

    explicit constexpr Event(const EndBreadcrumb& v) noexcept
        : type(Type::EndBreadcrumb)
    {
        data.endBreadcrumb = v;
    }

    explicit constexpr Event(const BeginWork& v) noexcept
        : type(Type::BeginWork)
    {
        data.beginWork = v;
    }

    explicit constexpr Event(const EndWork& v) noexcept
        : type(Type::EndWork)
    {
        data.endWork = v;
    }

    explicit constexpr Event(const Stats& v) noexcept
        : type(Type::Stats)
    {
        data.stats = v;
    }

    explicit constexpr Event(const SignalFence& v) noexcept
        : type(Type::SignalFence)
    {
        data.signalFence = v;
    }

    explicit constexpr Event(const WaitFence& v) noexcept
        : type(Type::WaitFence)
    {
        data.waitFence = v;
    }

    explicit constexpr Event(const Flip& v) noexcept
        : type(Type::Flip)
    {
        data.flip = v;
    }

    explicit constexpr Event(const Vsync& v) noexcept
        : type(Type::Vsync)
    {
        data.vsync = v;
    }
};

GP_RHI_API void processEvents(Queue queue, EventStream&& stream) noexcept;

/// @brief Move-only 16 KiB chunked stream of `Event` records retaining referenced `BreadcrumbAllocator` arenas.
class GP_RHI_API EventStream final
{
    friend GP_RHI_API void processEvents(Queue queue, EventStream&& stream) noexcept;

public:
    /// @brief Byte capacity of each pooled chunk allocation.
    static constexpr gp::USize kChunkSizeBytes = 16u * 1024u;

    struct Chunk
    {
        static constexpr gp::USize kMaxEvents =
            (kChunkSizeBytes - sizeof(Chunk*) - sizeof(gp::UInt32) * 2u) / sizeof(Event);

        Chunk* next{ nullptr };
        gp::UInt32 count{ 0u };
        gp::UInt32 reserved{ 0u };
        Event events[kMaxEvents]{};
    };

public:
    /// @brief Constructs an empty event stream (zero allocations until the first `emplace`).
    EventStream() noexcept = default;

    EventStream(const EventStream&) = delete;
    EventStream& operator=(const EventStream&) = delete;

    /// @brief Move constructor transferring chunk list and breadcrumb allocator references.
    /// @param[in,out] other Source stream left empty.
    EventStream(EventStream&& other) noexcept;

    /// @brief Move assignment operator recycling any existing chunks before adopting `other`.
    /// @param[in,out] other Source stream left empty.
    /// @return Reference to `*this`.
    EventStream& operator=(EventStream&& other) noexcept;

    /// @brief Returns all owned chunks to the global chunk free list and releases breadcrumb allocator references.
    ~EventStream() noexcept;

public:
    /// @brief Appends an `Event` constructed from `payload` (`Event::BeginWork`, `Event::Stats`, etc.).
    /// @tparam InPayloadType Concrete `Event` sub-struct.
    /// @param[in] payload Event payload struct.
    /// @return Mutable reference to the appended `Event`.
    template <typename InPayloadType>
    Event& emplace(const InPayloadType& payload) noexcept
    {
        Event* const slot = allocEventSlot();
        return *(::new (slot) Event(payload));
    }

    /// @brief Concatenates `other`'s chunks and breadcrumb allocator references onto the tail of `*this` in O(1).
    /// @param[in,out] other Source stream consumed and left empty.
    void append(EventStream&& other) noexcept;

    /// @brief Retains all `BreadcrumbAllocator` references in `allocators` for the lifetime of this stream.
    /// @param[in] allocators Breadcrumb allocator reference array.
    void retainBreadcrumbAllocators(const diagnostics::BreadcrumbAllocatorArray& allocators) noexcept
    {
        m_breadcrumbAllocators.appendUnique(allocators);
    }

    /// @brief Reports whether zero events have been recorded in this stream.
    /// @return True when empty.
    [[nodiscard]] GP_FORCEINLINE_HINT bool isEmpty() const noexcept
    {
        return m_numEvents == 0u;
    }

    /// @brief Returns the total number of `Event` records stored across all chunks in this stream.
    /// @return Event count.
    [[nodiscard]] GP_FORCEINLINE_HINT gp::UInt32 getNumEvents() const noexcept
    {
        return m_numEvents;
    }

    /// @brief Returns the retained `BreadcrumbAllocatorArray`.
    /// @return Const reference to `m_breadcrumbAllocators`.
    [[nodiscard]] const diagnostics::BreadcrumbAllocatorArray& getBreadcrumbAllocators() const noexcept
    {
        return m_breadcrumbAllocators;
    }

    /// @brief Recycles all chunks and clears retained breadcrumb allocators.
    void reset() noexcept;

private:
    [[nodiscard]] Event* allocEventSlot() noexcept;

private:
    Chunk* m_firstChunk{ nullptr };
    Chunk* m_lastChunk{ nullptr };
    gp::UInt32 m_numEvents{ 0u };
    diagnostics::BreadcrumbAllocatorArray m_breadcrumbAllocators{};
};

/// @brief Lifetime action returned by a `GPUStat` timing callback after receiving frame results.
enum class OnTimingResultsAction : gp::UInt8
{
    /// @brief Keep the `GPUStat` alive for subsequent frames.
    Keep = 0u,

    /// @brief Delete the heap-allocated `GPUStat` immediately after invoking the callback.
    Delete = 1u
};

/// @brief Timing summary delivered for a completed `GPUStat` frame or scope.
struct GPUStatTimingResult
{
    gp::UInt64 frameNumber{ 0u };
    Queue queue{};
    gp::Name statName{};
    gp::UInt64 inclusiveMicroseconds{ 0u };
    gp::UInt64 exclusiveMicroseconds{ 0u };
    gp::UInt32 numDraws{ 0u };
    gp::UInt32 numPrimitives{ 0u };
    gp::UInt32 numDispatches{ 0u };
};

/// @brief Observer callback invoked for completed GPU stat timing results.
using GPUStatCallback = gp::UniqueFunction<OnTimingResultsAction(const GPUStatTimingResult&)>;

/// @brief Global observer callback invoked for every completed GPU frame/breadcrumb timing result.
using GlobalGPUTimingObserverFn = void (*)(const GPUStatTimingResult& result, void* userData) noexcept;

/// @brief Callback invoked with a JSON profile report when a single-frame `profileNextFrame()` capture completes.
using GPUProfileCaptureFn = void (*)(gp::StringView jsonReport, void* userData) noexcept;

/// @brief Tracks GPU timing and draw/dispatch counters for a named GPU frame or pass scope.
class GP_RHI_API GPUStat final
{
public:
    /// @brief Constructs a GPU stat identified by `name` with optional completion `callback`.
    /// @param[in] name Diagnostic stat identifier.
    /// @param[in,out] callback Optional completion callback returning `Keep` or `Delete`.
    explicit GPUStat(gp::Name name, GPUStatCallback&& callback = {}) noexcept
        : m_name(name)
        , m_callback(static_cast<GPUStatCallback&&>(callback))
    {}

    /// @brief Returns the stat's identifier.
    /// @return `gp::Name`.
    [[nodiscard]] gp::Name getName() const noexcept
    {
        return m_name;
    }

    /// @brief Delivers `result` to this stat's callback (deleting `this` if the callback returns `Delete`).
    /// @param[in] result Completed timing result.
    /// @return Action returned by the callback (`Keep` if no callback was bound).
    OnTimingResultsAction notifyResult(const GPUStatTimingResult& result) noexcept;

private:
    gp::Name m_name{};
    GPUStatCallback m_callback{};
};

/// @brief Registers the hardware queues active on the current device before submitting `EventStream` batches.
/// @param[in] queues Span of active `Queue` descriptors.
GP_RHI_API void initializeQueues(gp::ConstVectorView<Queue> queues) noexcept;

/// @brief Processes a retired `EventStream` for `queue`, resolving cross-queue fences, breadcrumb durations, `GPUStat`
///        notifications, `GpuProfilerTrace` spans, and single-frame profile captures.
/// @param[in] queue Hardware queue that produced `stream`.
/// @param[in,out] stream Retired event stream consumed by the profiler.
GP_RHI_API void processEvents(Queue queue, EventStream&& stream) noexcept;

/// @brief Arms a one-shot GPU profile capture on the next `FrameBoundary` across initialized queues.
GP_RHI_API void profileNextFrame() noexcept;

/// @brief Atomically consumes and returns whether `profileNextFrame()` was requested (called by
/// `endFrameRenderThread`).
/// @return True if a capture was requested for the upcoming frame.
[[nodiscard]] GP_RHI_API bool consumeShouldProfileNextFrame() noexcept;

/// @brief Reports whether the GPU profiler is currently capturing an armed frame.
/// @return True while inside a captured frame.
[[nodiscard]] GP_RHI_API bool isProfiling() noexcept;

/// @brief Installs or clears a global observer receiving every `GPUStatTimingResult` resolved by `processEvents()`.
/// @param[in] observer Observer function pointer (`nullptr` to clear).
/// @param[in] userData Opaque context pointer passed to `observer`.
GP_RHI_API void setGlobalTimingObserver(GlobalGPUTimingObserverFn observer, void* userData = nullptr) noexcept;

/// @brief Installs or clears a callback receiving the formatted JSON report after a `profileNextFrame()` capture ends.
/// @param[in] callback Capture callback (`nullptr` to clear).
/// @param[in] userData Opaque context pointer passed to `callback`.
GP_RHI_API void setProfileCaptureCallback(GPUProfileCaptureFn callback, void* userData = nullptr) noexcept;

/// @brief Returns the last GPU frame busy time in microseconds recorded on `gpuIndex`.
/// @param[in] gpuIndex Zero-based GPU index.
/// @return Busy GPU microseconds on the most recent frame (`0` if none).
[[nodiscard]] GP_RHI_API gp::UInt64 getLastGPUFrameMicroseconds(gp::UInt32 gpuIndex = 0u) noexcept;

/// @brief Resets all GPU profiler queues, pending fence queues, callbacks, and pooled chunks.
GP_RHI_API void resetGPUProfiler() noexcept;

/// @brief Fixed-capacity ring history converting GPU busy timestamps into engine CPU-equivalent frame cycles.
class GP_RHI_API GPUFrameTimeHistory final
{
public:
    /// @brief Status returned when popping a frame cycle sample.
    enum class Result : gp::UInt8
    {
        /// @brief Valid GPU frame cycle sample returned.
        Ok = 0u,

        /// @brief No samples queued in the history ring.
        Empty = 1u,

        /// @brief Hardware reported a disjoint or invalid timestamp frequency.
        Disjoint = 2u
    };

    /// @brief Maximum number of frame samples buffered in the ring.
    static constexpr gp::UInt32 kCapacity = 16u;

public:
    /// @brief Constructs an empty frame time history.
    GPUFrameTimeHistory() noexcept = default;

    /// @brief Pushes a raw GPU busy tick count measured at `timestampFrequencyHz` (`0` marks a disjoint sample).
    /// @param[in] timestampFrequencyHz GPU timestamp counter frequency in Hz (`<= 0.0` records a `Disjoint` entry).
    /// @param[in] gpuBusyTicks Raw GPU busy ticks elapsed during the frame.
    void pushFrameCycles(gp::Float64 timestampFrequencyHz, gp::UInt64 gpuBusyTicks) noexcept;

    /// @brief Pops the oldest buffered frame sample converted into microseconds/cycles.
    /// @param[out] outCycles Receives the frame's busy duration in microseconds (`0` on `Empty` or `Disjoint`).
    /// @return `Result::Ok`, `Result::Empty`, or `Result::Disjoint`.
    [[nodiscard]] Result popFrameCycles(gp::UInt32& outCycles) noexcept;

    /// @brief Clears all buffered frame samples.
    void clear() noexcept;

    /// @brief Reports whether the history ring is empty.
    /// @return True when empty.
    [[nodiscard]] bool isEmpty() const noexcept
    {
        return m_count == 0u;
    }

private:
    struct Sample
    {
        gp::UInt32 cycles{ 0u };
        bool disjoint{ false };
    };

    Sample m_samples[kCapacity]{};
    gp::UInt32 m_head{ 0u };
    gp::UInt32 m_tail{ 0u };
    gp::UInt32 m_count{ 0u };
};

/// @brief Global per-GPU frame time history ring array.
extern GP_RHI_API GPUFrameTimeHistory g_rhiGPUFrameTimeHistory[kMaxNumGPUs];

/// @brief Returns the most recently latched GPU frame busy cycles/microseconds for `gpuIndex`.
/// @param[in] gpuIndex Zero-based GPU adapter index.
/// @return GPU frame busy microseconds.
[[nodiscard]] GP_RHI_API gp::UInt32 getGPUFrameCycles(gp::UInt32 gpuIndex = 0u) noexcept;

}   // namespace gp::rhi::profiling
