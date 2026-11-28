// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "diagnostics/DiagnosticBuffer.hpp"
#include "containers/arrays/Vector.hpp"      // IWYU pragma: keep
#include "containers/strings/String.hpp"
#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "RHIDefinitions.hpp"
#include "threading/sync/Mutex.hpp"
#include "threading/sync/UniqueLock.hpp"
#include <cstdio>

namespace gp::rhi::diagnostics
{

namespace
{

struct DiagnosticMessageRegistry
{
    gp::thread::Mutex mutex{};
    gp::Vector<ShaderDiagnosticData> entries{};

    void upsert(const gp::UInt32 id, const gp::StringView msg) noexcept
    {
        for (ShaderDiagnosticData& item: entries)
        {
            if (item.id == id)
            {
                item.message = gp::String(msg);
                return;
            }
        }
        ShaderDiagnosticData added{};
        added.id = id;
        added.message = gp::String(msg);
        entries.emplaceBack(static_cast<ShaderDiagnosticData&&>(added));
    }

    [[nodiscard]] const gp::String* find(const gp::UInt32 id) const noexcept
    {
        for (const ShaderDiagnosticData& item: entries)
        {
            if (item.id == id)
            {
                return &item.message;
            }
        }
        return nullptr;
    }
};

DiagnosticMessageRegistry& getDiagnosticRegistry() noexcept
{
    static DiagnosticMessageRegistry s_registry{};
    return s_registry;
}

}   // namespace

void registerShaderDiagnosticMessages(const gp::ConstVectorView<ShaderDiagnosticData> messages) noexcept
{
    if (messages.isEmpty())
    {
        return;
    }

    DiagnosticMessageRegistry& registry = getDiagnosticRegistry();
    gp::thread::UniqueLock<gp::thread::Mutex> lock(registry.mutex);
    for (const ShaderDiagnosticData& entry: messages)
    {
        registry.upsert(entry.id, entry.message.view());
    }
}

void registerShaderDiagnosticMessage(const gp::UInt32 messageId, const gp::StringView message) noexcept
{
    DiagnosticMessageRegistry& registry = getDiagnosticRegistry();
    gp::thread::UniqueLock<gp::thread::Mutex> lock(registry.mutex);
    registry.upsert(messageId, message);
}

const gp::String* getShaderDiagnosticMessage(const gp::UInt32 messageId) noexcept
{
    DiagnosticMessageRegistry& registry = getDiagnosticRegistry();
    gp::thread::UniqueLock<gp::thread::Mutex> lock(registry.mutex);
    return registry.find(messageId);
}

void clearShaderDiagnosticMessages() noexcept
{
    DiagnosticMessageRegistry& registry = getDiagnosticRegistry();
    gp::thread::UniqueLock<gp::thread::Mutex> lock(registry.mutex);
    registry.entries.clear();
}

gp::String DiagnosticBuffer::getShaderDiagnosticMessages(
    const gp::UInt32 deviceIndex, const gp::UInt32 queueIndex, const char* const queueName
) const noexcept
{
    const char* const resolvedQueueName = (queueName != nullptr) ? queueName : "Unknown";
    char lineBuf[512]{};

    if (m_data == nullptr || queueIndex >= m_numQueues)
    {
        const int len = std::snprintf(
            lineBuf,
            sizeof(lineBuf),
            "Unable to retrieve shader diagnostic messages for Device %u, Queue '%s' (index %u): diagnostic buffer is unmapped.",
            deviceIndex,
            resolvedQueueName,
            queueIndex
        );
        return (len > 0) ? gp::String(gp::StringView(lineBuf, static_cast<gp::USize>(len))) : gp::String{};
    }

    const Queue& queue = m_data[queueIndex];
    gp::UInt32 activeLaneCount = 0u;
    for (gp::UInt32 laneIdx = 0u; laneIdx < Queue::kMaxLanes; ++laneIdx)
    {
        if (queue.lanes[laneIdx].counter != 0u)
        {
            ++activeLaneCount;
        }
    }

    if (activeLaneCount == 0u)
    {
        const int len = std::snprintf(
            lineBuf,
            sizeof(lineBuf),
            "No shader diagnostics found for Device %u, Queue '%s' (markerIn=%u, markerOut=%u).",
            deviceIndex,
            resolvedQueueName,
            queue.markerIn,
            queue.markerOut
        );
        return (len > 0) ? gp::String(gp::StringView(lineBuf, static_cast<gp::USize>(len))) : gp::String{};
    }

    gp::String report{};
    {
        const int headerLen = std::snprintf(
            lineBuf,
            sizeof(lineBuf),
            "Shader diagnostics for Device %u, Queue '%s' (%u active lane(s), markerIn=%u, markerOut=%u):\n",
            deviceIndex,
            resolvedQueueName,
            activeLaneCount,
            queue.markerIn,
            queue.markerOut
        );
        if (headerLen > 0)
        {
            report.append(lineBuf, static_cast<gp::USize>(headerLen));
        }
    }

    for (gp::UInt32 laneIdx = 0u; laneIdx < Queue::kMaxLanes; ++laneIdx)
    {
        const Lane& lane = queue.lanes[laneIdx];
        if (lane.counter == 0u)
        {
            continue;
        }

        const gp::String* const registeredMsg = getShaderDiagnosticMessage(lane.messageId);
        if (registeredMsg != nullptr)
        {
            const int msgLen = std::snprintf(
                lineBuf,
                sizeof(lineBuf),
                "  [Lane %02u | Count %u | ID 0x%08X] %s | Payload uint4=(%u, %u, %u, %u) int4=(%d, %d, %d, %d)\n",
                laneIdx,
                lane.counter,
                lane.messageId,
                registeredMsg->cString(),
                lane.payload.asUint[0],
                lane.payload.asUint[1],
                lane.payload.asUint[2],
                lane.payload.asUint[3],
                lane.payload.asInt[0],
                lane.payload.asInt[1],
                lane.payload.asInt[2],
                lane.payload.asInt[3]
            );
            if (msgLen > 0)
            {
                report.append(lineBuf, static_cast<gp::USize>(msgLen));
            }
        }
        else
        {
            const int rawLen = std::snprintf(
                lineBuf,
                sizeof(lineBuf),
                "  [Lane %02u | Count %u | Unregistered ID 0x%08X] int4=(%d, %d, %d, %d) uint4=(%u, %u, %u, %u) " "hex=(0x%08X, 0x%08X, 0x%08X, 0x%08X) float4=(%.4f, %.4f, %.4f, %.4f)\n",
                laneIdx,
                lane.counter,
                lane.messageId,
                lane.payload.asInt[0],
                lane.payload.asInt[1],
                lane.payload.asInt[2],
                lane.payload.asInt[3],
                lane.payload.asUint[0],
                lane.payload.asUint[1],
                lane.payload.asUint[2],
                lane.payload.asUint[3],
                lane.payload.asUint[0],
                lane.payload.asUint[1],
                lane.payload.asUint[2],
                lane.payload.asUint[3],
                static_cast<double>(lane.payload.asFloat[0]),
                static_cast<double>(lane.payload.asFloat[1]),
                static_cast<double>(lane.payload.asFloat[2]),
                static_cast<double>(lane.payload.asFloat[3])
            );
            if (rawLen > 0)
            {
                report.append(lineBuf, static_cast<gp::USize>(rawLen));
            }
        }
    }

    return report;
}

void DiagnosticBuffer::logShaderDiagnosticMessages(
    const gp::UInt32 deviceIndex, const gp::UInt32 queueIndex, const char* const queueName
) const noexcept
{
    const gp::String report = getShaderDiagnosticMessages(deviceIndex, queueIndex, queueName);
    GP_LOG_ERROR(::gp::LogRHI, "{}", report.view());
}

}   // namespace gp::rhi::diagnostics
