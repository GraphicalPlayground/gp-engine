// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "threading/jobs/JobSystemDesc.hpp"
#include "threading/jobs/JobSystemEnums.hpp"
#include "threading/jobs/JobSystemForward.hpp"
#include "threading/jobs/JobSystemTypes.hpp"
#include "threading/queues/JobQueueEnums.hpp"

namespace gp::thread
{

/// @brief Zero-allocation data-parallel range splitter and dispatcher for `gp::thread::JobSystem` ([ADR-0052]).
/// @details Partitions a contiguous index range `[0, elementCount)` into balanced or fixed-size batches across active
///          worker slots and invokes `ParallelForFunction(startIndex, count, userData, const JobContext&)` on each
///          batch. Passes the executing worker's `JobContext` (`context.workerIndex`) so Render Dependency Graph (RDG)
///          passes can record draw and dispatch commands into per-worker `IRHICommandList` slots without locks.
class GP_CORE_API ParallelFor final
{
public:
    ParallelFor() = delete;

    /// @brief Computes the number of batches required to partition `elementCount` elements under `strategy`.
    /// @param[in] elementCount Total number of elements in `[0, elementCount)`.
    /// @param[in] minBatchSize Minimum number of elements per batch (`0u` is clamped to `1u`).
    /// @param[in] maxBatchCount Maximum number of batches allowed (`0u` is clamped to `kMaxParallelForBatches`).
    /// @param[in] totalWorkerSlotCount Total active worker slots (`workerThreadCount + 1`, clamped to `>= 1u`).
    /// @param[in] strategy Range partitioning strategy (`Adaptive`, `FixedBatchSize`, or `SingleBatchPerWorker`).
    /// @return Batch count (`0u` when `elementCount == 0u`, otherwise in `[1u, maxBatchCount]`).
    [[nodiscard]] static constexpr gp::UInt32 computeBatchCount(
        const gp::UInt32 elementCount,
        const gp::UInt32 minBatchSize,
        const gp::UInt32 maxBatchCount,
        const gp::UInt32 totalWorkerSlotCount,
        const JobSplitStrategy strategy = JobSplitStrategy::Adaptive
    ) noexcept
    {
        if (elementCount == 0u)
        {
            return 0u;
        }

        const gp::UInt32 safeMinBatch = (minBatchSize == 0u) ? 1u : minBatchSize;
        const gp::UInt32 safeMaxBatches =
            (maxBatchCount == 0u || maxBatchCount > kMaxParallelForBatches) ? kMaxParallelForBatches : maxBatchCount;
        const gp::UInt32 safeWorkerSlots = (totalWorkerSlotCount == 0u) ? 1u : totalWorkerSlotCount;

        // Upper bound on batches imposed by minBatchSize: ceil(elementCount / safeMinBatch).
        const gp::UInt64 maxBatchesByGrain64 =
            (static_cast<gp::UInt64>(elementCount) + static_cast<gp::UInt64>(safeMinBatch) - 1ULL) /
            static_cast<gp::UInt64>(safeMinBatch);
        const gp::UInt32 maxBatchesByGrain = (maxBatchesByGrain64 > static_cast<gp::UInt64>(safeMaxBatches))
                                                 ? safeMaxBatches
                                                 : static_cast<gp::UInt32>(maxBatchesByGrain64);

        if (maxBatchesByGrain <= 1u)
        {
            return 1u;
        }

        gp::UInt32 targetBatches = maxBatchesByGrain;
        switch (strategy)
        {
        case JobSplitStrategy::FixedBatchSize:
            targetBatches = maxBatchesByGrain;
            break;
        case JobSplitStrategy::SingleBatchPerWorker:
            targetBatches = (safeWorkerSlots < maxBatchesByGrain) ? safeWorkerSlots : maxBatchesByGrain;
            break;
        case JobSplitStrategy::Adaptive:
            // falls through
        default:
        {
            const gp::UInt32 oversubscribedTarget = safeWorkerSlots * 4u;
            targetBatches = (oversubscribedTarget < maxBatchesByGrain) ? oversubscribedTarget : maxBatchesByGrain;
            break;
        }
        }

        if (targetBatches == 0u)
        {
            return 1u;
        }

        return (targetBatches > safeMaxBatches) ? safeMaxBatches : targetBatches;
    }

    /// @brief Computes the `[outStartIndex, outStartIndex + outCount)` sub-range for `batchIndex`.
    /// @param[in] elementCount Total number of elements in `[0, elementCount)`.
    /// @param[in] batchCount Total batch count returned by `computeBatchCount()`.
    /// @param[in] minBatchSize Minimum batch size (used when `strategy == JobSplitStrategy::FixedBatchSize` and
    ///                         `batchCount` was not clamped).
    /// @param[in] strategy Range partitioning strategy.
    /// @param[in] batchIndex Zero-based index of the batch in `[0, batchCount)`.
    /// @param[out] outStartIndex Receives the starting element index of this batch.
    /// @param[out] outCount Receives the number of elements in this batch.
    static constexpr void computeBatchRange(
        const gp::UInt32 elementCount,
        const gp::UInt32 batchCount,
        const gp::UInt32 minBatchSize,
        const JobSplitStrategy strategy,
        const gp::UInt32 batchIndex,
        gp::UInt32& outStartIndex,
        gp::UInt32& outCount
    ) noexcept
    {
        if (elementCount == 0u || batchCount == 0u || batchIndex >= batchCount)
        {
            outStartIndex = 0u;
            outCount = 0u;
            return;
        }

        const gp::UInt32 safeMinBatch = (minBatchSize == 0u) ? 1u : minBatchSize;
        const gp::UInt64 fixedTotalBatches =
            (static_cast<gp::UInt64>(elementCount) + static_cast<gp::UInt64>(safeMinBatch) - 1ULL) /
            static_cast<gp::UInt64>(safeMinBatch);

        if (strategy == JobSplitStrategy::FixedBatchSize && fixedTotalBatches == static_cast<gp::UInt64>(batchCount))
        {
            const gp::UInt32 start = batchIndex * safeMinBatch;
            const gp::UInt32 remaining = (elementCount > start) ? (elementCount - start) : 0u;
            outStartIndex = start;
            outCount = (remaining < safeMinBatch) ? remaining : safeMinBatch;
            return;
        }

        // Distribute the remainder across the first (elementCount % batchCount) batches so batch sizes differ by at
        // most one element.
        const gp::UInt32 baseCount = elementCount / batchCount;
        const gp::UInt32 remainder = elementCount % batchCount;
        const gp::UInt32 offset = (batchIndex < remainder) ? batchIndex : remainder;

        outStartIndex = (batchIndex * baseCount) + offset;
        outCount = baseCount + ((batchIndex < remainder) ? 1u : 0u);
    }

    /// @brief Splits `[0, elementCount)` into batches, dispatches them across `jobSystem`, and waits until all
    ///        elements have been processed.
    /// @param[in,out] jobSystem Target `JobSystem` instance.
    /// @param[in] elementCount Total number of elements to process (`0` returns `JobSystemResult::Success` as a no-op).
    /// @param[in] minBatchSize Minimum elements per batch (`>= 1`).
    /// @param[in] callback Stateless batch callback `void (*)(startIndex, count, userData, const JobContext&)`.
    /// @param[in] userData Opaque caller-managed context pointer forwarded to `callback`.
    /// @param[in] priority Scheduling priority band (`JobPriority::Normal` by default).
    /// @return `JobSystemResult::Success` upon completion of all batches, or an error code.
    [[nodiscard]] static JobSystemResult dispatch(
        JobSystem& jobSystem,
        gp::UInt32 elementCount,
        gp::UInt32 minBatchSize,
        ParallelForFunction callback,
        void* userData = nullptr,
        JobPriority priority = JobPriority::Normal
    ) noexcept;

    /// @brief Splits `[0, desc.elementCount)` using an explicit `ParallelForDesc` and waits until all batches finish.
    /// @param[in,out] jobSystem Target `JobSystem` instance.
    /// @param[in] desc Configuration descriptor (`elementCount`, `minBatchSize`, `strategy`, `priority`, `stackTier`).
    /// @param[in] callback Stateless batch callback `void (*)(startIndex, count, userData, const JobContext&)`.
    /// @param[in] userData Opaque caller-managed context pointer forwarded to `callback`.
    /// @return `JobSystemResult::Success` upon completion of all batches, or an error code.
    [[nodiscard]] static JobSystemResult dispatchWithDesc(
        JobSystem& jobSystem, const ParallelForDesc& desc, ParallelForFunction callback, void* userData = nullptr
    ) noexcept;
};

}   // namespace gp::thread
