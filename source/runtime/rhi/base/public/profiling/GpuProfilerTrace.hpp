// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Vector.hpp"      // IWYU pragma: keep
#include "containers/strings/String.hpp"     // IWYU pragma: keep
#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "profiling/ProfilingForward.hpp"    // IWYU pragma: keep
#include "RHIBase.hpp"

namespace gp::rhi::profiling
{

/// @brief Builds structured key-value diagnostic metadata strings for GPU profiler trace events.
class GP_RHI_API MetadataSerializer final
{
public:
    /// @brief Constructs an empty metadata serializer.
    MetadataSerializer() noexcept = default;

    /// @brief Appends a string key-value pair (`key=value`).
    /// @param[in] key Field name.
    /// @param[in] value String value.
    void appendValue(gp::StringView key, gp::StringView value) noexcept;

    /// @brief Appends an unsigned 64-bit integer key-value pair.
    /// @param[in] key Field name.
    /// @param[in] value Unsigned integer value.
    void appendValue(gp::StringView key, gp::UInt64 value) noexcept;

    /// @brief Appends a signed 64-bit integer key-value pair.
    /// @param[in] key Field name.
    /// @param[in] value Signed integer value.
    void appendValue(gp::StringView key, gp::Int64 value) noexcept;

    /// @brief Appends a 64-bit floating-point key-value pair.
    /// @param[in] key Field name.
    /// @param[in] value Floating-point value.
    void appendValue(gp::StringView key, gp::Float64 value) noexcept;

    /// @brief Clears all serialized metadata entries.
    void clear() noexcept
    {
        m_buffer.clear();
        m_fieldCount = 0u;
    }

    /// @brief Reports whether zero fields have been appended.
    /// @return True when empty.
    [[nodiscard]] bool isEmpty() const noexcept
    {
        return m_fieldCount == 0u;
    }

    /// @brief Returns the number of appended key-value fields.
    /// @return Field count.
    [[nodiscard]] gp::UInt32 getFieldCount() const noexcept
    {
        return m_fieldCount;
    }

    /// @brief Returns the formatted metadata string (`"{k1=v1, k2=v2}"` or `"{}"`).
    /// @return Formatted `gp::String`.
    [[nodiscard]] gp::String toString() const noexcept;

private:
    void appendSeparatorAndKey(gp::StringView key) noexcept;

private:
    gp::String m_buffer{};
    gp::UInt32 m_fieldCount{ 0u };
};

/// @brief Captures structured GPU timeline spans (frames, queues, and hierarchical events) for trace analysis.
class GP_RHI_API GpuProfilerTrace final
{
public:
    /// @brief Recorded GPU trace span with queue identity, microsecond timestamps, and optional metadata.
    struct RecordedSpan
    {
        gp::UInt64 frameNumber{ 0u };
        gp::UInt32 queueId{ 0u };
        gp::UInt64 startTimestampUs{ 0u };
        gp::UInt64 endTimestampUs{ 0u };
        gp::UInt32 depth{ 0u };
        gp::String name{};
        gp::String metadata{};
    };

public:
    GpuProfilerTrace() = delete;

    /// @brief Enables or disables GPU profiler trace recording.
    /// @param[in] enabled True to record trace spans.
    static void setEnabled(bool enabled) noexcept;

    /// @brief Reports whether GPU profiler trace recording is currently active.
    /// @return True when enabled.
    [[nodiscard]] static bool isEnabled() noexcept;

    /// @brief Registers a human-readable label for `queueId`.
    /// @param[in] queueId Packed `Queue::value` identifier.
    /// @param[in] queueName Display label (e.g., `"Graphics0"`).
    static void initQueue(gp::UInt32 queueId, gp::StringView queueName) noexcept;

    /// @brief Begins a traced GPU frame on `queueId`.
    /// @param[in] queueId Packed queue identifier.
    /// @param[in] frameNumber Frame index.
    /// @param[in] calibrationCpuUs Reference CPU timestamp in microseconds.
    /// @param[in] calibrationGpuUs Reference GPU timestamp in microseconds.
    static void beginFrame(
        gp::UInt32 queueId, gp::UInt64 frameNumber, gp::UInt64 calibrationCpuUs = 0u, gp::UInt64 calibrationGpuUs = 0u
    ) noexcept;

    /// @brief Pushes a traced GPU event span onto `queueId`'s open stack.
    /// @param[in] queueId Packed queue identifier.
    /// @param[in] name Event label.
    /// @param[in] timestampUs GPU start timestamp in microseconds.
    /// @param[in] metadata Optional serialized metadata string.
    static void beginEvent(
        gp::UInt32 queueId, gp::StringView name, gp::UInt64 timestampUs, gp::StringView metadata = {}
    ) noexcept;

    /// @brief Pops the innermost open GPU event span on `queueId` and commits a `RecordedSpan`.
    /// @param[in] queueId Packed queue identifier.
    /// @param[in] timestampUs GPU end timestamp in microseconds.
    static void endEvent(gp::UInt32 queueId, gp::UInt64 timestampUs) noexcept;

    /// @brief Finalizes the current traced GPU frame on `queueId`.
    /// @param[in] queueId Packed queue identifier.
    static void endFrame(gp::UInt32 queueId) noexcept;

    /// @brief Returns the number of completed `RecordedSpan` records captured since startup or `reset()`.
    /// @return Completed span count.
    [[nodiscard]] static gp::USize getRecordedSpanCount() noexcept;

    /// @brief Copies all completed `RecordedSpan` records into `outSpans`.
    /// @param[out] outSpans Destination vector populated with completed trace spans.
    static void getRecordedSpans(gp::Vector<RecordedSpan>& outSpans) noexcept;

    /// @brief Clears all recorded trace spans, open stacks, and queue registrations.
    static void reset() noexcept;
};

}   // namespace gp::rhi::profiling
