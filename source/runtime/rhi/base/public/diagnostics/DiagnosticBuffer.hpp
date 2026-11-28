// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/strings/String.hpp"
#include "containers/views/StringView.hpp"
#include "containers/views/VectorView.hpp"
#include "CoreMinimal.hpp"
#include "diagnostics/DiagnosticsForward.hpp"   // IWYU pragma: keep
#include "RHIBase.hpp"

namespace gp::rhi::diagnostics
{

/// @brief Associates a compiled shader diagnostic/assertion ID with its human-readable message string.
struct ShaderDiagnosticData
{
    /// @brief Deterministic hash/identifier emitted into `DiagnosticBuffer::Lane::messageId` by shader assertions.
    gp::UInt32 id{ 0u };

    /// @brief Human-readable file, line, and assertion description string.
    gp::String message{};
};

/// @brief Registers a batch of shader diagnostic assertion messages for crash/hang decoding.
/// @param[in] messages Contiguous view of `(id, message)` pairs.
GP_RHI_API void registerShaderDiagnosticMessages(gp::ConstVectorView<ShaderDiagnosticData> messages) noexcept;

/// @brief Registers or updates a single shader diagnostic assertion message.
/// @param[in] messageId Non-zero message identifier.
/// @param[in] message Human-readable message text.
GP_RHI_API void registerShaderDiagnosticMessage(gp::UInt32 messageId, gp::StringView message) noexcept;

/// @brief Looks up a previously registered shader diagnostic message by `messageId`.
/// @param[in] messageId Message identifier read from `DiagnosticBuffer::Lane::messageId`.
/// @return Pointer to the registered `gp::String`, or `nullptr` if unregistered.
[[nodiscard]] GP_RHI_API const gp::String* getShaderDiagnosticMessage(gp::UInt32 messageId) noexcept;

/// @brief Clears all registered shader diagnostic messages (used during shutdown and unit tests).
GP_RHI_API void clearShaderDiagnosticMessages() noexcept;

/// @brief Host-mapped GPU diagnostic buffer layout shared by hardware backends for shader assertions and queue markers.
/// @details Backends allocate persistently mapped readback memory holding one `Queue` per hardware pipeline/queue and
///          assign `m_data`. Shaders atomically claim or write into `Queue::lanes[0..63]` on GPU-side assertion/printf
///          events while command buffers write breadcrumb IDs into `Queue::markerIn` and `Queue::markerOut`.
class GP_RHI_API DiagnosticBuffer
{
public:
    /// @brief Fixed 24-byte (`6 * uint32`) per-lane shader diagnostic payload written by GPU waves/warps.
    struct Lane
    {
        /// @brief Non-zero counter indicating how many diagnostic events wrote to this lane.
        gp::UInt32 counter{ 0u };

        /// @brief Registered assertion message ID (`getShaderDiagnosticMessage`) or raw shader format tag.
        gp::UInt32 messageId{ 0u };

        /// @brief Four 32-bit payload registers interpreted as signed ints, unsigned ints, or IEEE-754 floats.
        union Payload
        {
            gp::Int32 asInt[4];
            gp::UInt32 asUint[4];
            gp::Float32 asFloat[4];

            constexpr Payload() noexcept
                : asUint{ 0u, 0u, 0u, 0u }
            {}
        } payload{};
    };

    static_assert(
        sizeof(Lane) == 6u * sizeof(gp::UInt32), "DiagnosticBuffer::Lane must match the 6x32-bit GPU shader layout."
    );

    /// @brief Per-queue GPU diagnostic block combining 64 shader assertion lanes with entry/exit breadcrumb markers.
    struct Queue
    {
        /// @brief Number of shader wave/lane diagnostic slots per hardware queue.
        static constexpr gp::UInt32 kMaxLanes = 64u;

        /// @brief Shader diagnostic lanes indexed by wave/lane slot `[0, kMaxLanes)`.
        Lane lanes[kMaxLanes]{};

        /// @brief Last GPU breadcrumb node ID begun on this queue.
        gp::UInt32 markerIn{ 0u };

        /// @brief Last GPU breadcrumb node ID ended on this queue.
        gp::UInt32 markerOut{ 0u };
    };

public:
    /// @brief Constructs an unmapped diagnostic buffer (`m_data == nullptr`).
    DiagnosticBuffer() noexcept = default;

    /// @brief Constructs a diagnostic buffer backed by mapped `queues` memory (`numQueues` elements).
    /// @param[in] queues Persistently mapped array of `Queue` blocks.
    /// @param[in] numQueues Number of contiguous `Queue` elements in `queues`.
    DiagnosticBuffer(Queue* const queues, const gp::UInt32 numQueues) noexcept
        : m_data(queues)
        , m_numQueues(numQueues)
    {}

    DiagnosticBuffer(const DiagnosticBuffer&) = delete;
    DiagnosticBuffer& operator=(const DiagnosticBuffer&) = delete;
    DiagnosticBuffer(DiagnosticBuffer&&) = delete;
    DiagnosticBuffer& operator=(DiagnosticBuffer&&) = delete;

    /// @brief Virtual destructor for backend-derived diagnostic buffers.
    virtual ~DiagnosticBuffer() noexcept = default;

public:
    /// @brief Binds or clears the CPU-mapped `Queue` array backing this diagnostic buffer.
    /// @param[in] queues Mapped `Queue` array pointer (`nullptr` to detach).
    /// @param[in] numQueues Number of `Queue` records at `queues`.
    void setMappedQueues(Queue* const queues, const gp::UInt32 numQueues) noexcept
    {
        m_data = queues;
        m_numQueues = numQueues;
    }

    /// @brief Returns a mutable pointer to mapped `Queue` at `queueIndex`, or `nullptr` if out of bounds/unmapped.
    /// @param[in] queueIndex Zero-based queue index.
    /// @return Pointer to `Queue`, or `nullptr`.
    [[nodiscard]] Queue* getQueue(const gp::UInt32 queueIndex) noexcept
    {
        return (m_data != nullptr && queueIndex < m_numQueues) ? &m_data[queueIndex] : nullptr;
    }

    /// @brief Returns a read-only pointer to mapped `Queue` at `queueIndex`, or `nullptr` if out of bounds/unmapped.
    /// @param[in] queueIndex Zero-based queue index.
    /// @return Const pointer to `Queue`, or `nullptr`.
    [[nodiscard]] const Queue* getQueue(const gp::UInt32 queueIndex) const noexcept
    {
        return (m_data != nullptr && queueIndex < m_numQueues) ? &m_data[queueIndex] : nullptr;
    }

    /// @brief Returns the number of mapped `Queue` blocks in `m_data`.
    /// @return Queue count.
    [[nodiscard]] GP_FORCEINLINE_HINT gp::UInt32 getNumQueues() const noexcept
    {
        return m_numQueues;
    }

    /// @brief Formats all triggered shader diagnostic lanes for `(deviceIndex, queueIndex)` into a human-readable
    ///        report.
    /// @param[in] deviceIndex Multi-GPU device index for diagnostic labeling.
    /// @param[in] queueIndex Zero-based index into `m_data`.
    /// @param[in] queueName Human-readable queue label (e.g., `"Graphics"`, `"AsyncCompute"`).
    /// @return Multi-line diagnostic report string.
    [[nodiscard]] gp::String getShaderDiagnosticMessages(
        gp::UInt32 deviceIndex, gp::UInt32 queueIndex, const char* queueName
    ) const noexcept;

    /// @brief Formats and logs shader diagnostic messages for `(deviceIndex, queueIndex)` to `LogRHI`.
    /// @param[in] deviceIndex Multi-GPU device index.
    /// @param[in] queueIndex Zero-based queue index.
    /// @param[in] queueName Human-readable queue label.
    void logShaderDiagnosticMessages(
        gp::UInt32 deviceIndex, gp::UInt32 queueIndex, const char* queueName
    ) const noexcept;

protected:
    /// @brief Persistently mapped array of per-queue diagnostic structures.
    Queue* m_data{ nullptr };

    /// @brief Number of valid `Queue` entries at `m_data`.
    gp::UInt32 m_numQueues{ 0u };
};

}   // namespace gp::rhi::diagnostics
