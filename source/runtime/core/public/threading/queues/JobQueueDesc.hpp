// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "concepts/Memory.hpp"
#include "threading/fibers/FiberEnums.hpp"
#include "threading/fibers/FiberForward.hpp"
#include "threading/queues/JobQueueEnums.hpp"
#include "threading/queues/JobQueueForward.hpp"
#include "threading/queues/JobQueueTypes.hpp"
#include "threading/sync/JobSyncForward.hpp"

namespace gp::thread
{

/// @brief 32-byte trivially copyable POD descriptor defining a unit of work for the Fiber Job System ([ADR-0051]).
/// @details Fits two descriptors per 64-byte hardware cache line (`sizeof(JobDecl) == 32`, `alignof(JobDecl) == 16`).
///          Adheres strictly to [ADR-0020] and [ADR-0022] by pairing a stateless `JobEntryFunction` with an opaque
///          `void* userData` payload, an optional completion `JobCounter*`, priority band, and required fiber stack
///          sizing tier.
struct alignas(16) JobDecl
{
    /// @brief Stateless task entry callback (`void (*)(void*)`). Must be non-null for a valid job.
    JobEntryFunction entry{ nullptr };

    /// @brief Opaque caller-managed context pointer forwarded to `entry(userData)`.
    void* userData{ nullptr };

    /// @brief Optional dependency counter automatically decremented by `1` when `entry` completes.
    JobCounter* counter{ nullptr };

    /// @brief Scheduling priority band (`JobPriority::Normal` by default).
    JobPriority priority{ JobPriority::Normal };

    /// @brief Required cooperative fiber stack tier (`FiberStackTier::Standard64KiB` by default).
    FiberStackTier stackTier{ FiberStackTier::Standard64KiB };

    /// @brief Reserved 16-bit user/scheduler flags for affinity or profiling tags.
    gp::UInt16 flags{ 0u };

    /// @brief Explicit 32-bit zero padding guaranteeing a deterministic 32-byte POD footprint.
    gp::UInt32 reserved{ 0u };

    /// @brief Checks whether this job declaration holds a valid non-null entry callback.
    /// @return True if `entry != nullptr`, false otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool isValid() const noexcept
    {
        return entry != nullptr;
    }

    /// @brief Invokes `entry(userData)` if `entry != nullptr`.
    /// @return True if `entry` was non-null and invoked, false otherwise.
    GP_FORCEINLINE_HINT bool invoke() const noexcept
    {
        if (entry == nullptr) [[unlikely]]
        {
            return false;
        }
        entry(userData);
        return true;
    }

    /// @brief Constructs a `JobDecl` with explicit parameters.
    /// @param[in] inEntry Stateless task function pointer.
    /// @param[in] inUserData Opaque task context pointer.
    /// @param[in] inCounter Optional `JobCounter*` associated with this job.
    /// @param[in] inPriority Scheduling priority band.
    /// @param[in] inStackTier Required fiber stack tier (`Standard64KiB` or `Large512KiB`).
    /// @param[in] inFlags Optional 16-bit scheduling flags.
    /// @return Populated 32-byte `JobDecl` instance.
    [[nodiscard]] GP_FORCEINLINE_HINT static constexpr JobDecl make(
        const JobEntryFunction inEntry,
        void* const inUserData = nullptr,
        JobCounter* const inCounter = nullptr,
        const JobPriority inPriority = JobPriority::Normal,
        const FiberStackTier inStackTier = FiberStackTier::Standard64KiB,
        const gp::UInt16 inFlags = 0u
    ) noexcept
    {
        JobDecl decl{};
        decl.entry = inEntry;
        decl.userData = inUserData;
        decl.counter = inCounter;
        decl.priority = inPriority;
        decl.stackTier = inStackTier;
        decl.flags = inFlags;
        decl.reserved = 0u;
        return decl;
    }
};

static_assert(sizeof(JobDecl) == 32u, "JobDecl must occupy exactly 32 bytes (2 per cache line).");
static_assert(alignof(JobDecl) == 16u, "JobDecl must be 16-byte aligned.");
static_assert(gp::concepts::IsTriviallyCopyable<JobDecl>, "JobDecl must be trivially copyable.");
static_assert(gp::concepts::IsStandardLayout<JobDecl>, "JobDecl must have standard layout.");

/// @brief 32-byte unified queue payload capable of representing either a fresh `JobDecl` or a woken `Fiber*`
///        ([ADR-0051]).
/// @details Allows `WorkStealingDeque<JobQueueItem>` and `MPMCRingQueue<JobQueueItem>` to transport both newly spawned
///          tasks (`JobQueueItemKind::JobEntry`) and suspended fibers unblocked by `JobCounter` / `JobFence`
///          (`JobQueueItemKind::ResumedFiber`) without dynamic allocation or separate queue instantiations.
struct alignas(16) JobQueueItem
{
    /// @brief Task entry function when `kind == JobQueueItemKind::JobEntry` (`nullptr` for `ResumedFiber`).
    JobEntryFunction entry{ nullptr };

    /// @brief Task context pointer when `kind == JobQueueItemKind::JobEntry` (`nullptr` for `ResumedFiber`).
    void* userData{ nullptr };

    /// @brief Completion counter (`JobEntry`) or woken `Fiber*` handle (`ResumedFiber`).
    union
    {
        /// @brief Optional completion counter for `JobQueueItemKind::JobEntry`.
        JobCounter* counter{ nullptr };

        /// @brief Ready-to-resume cooperative fiber for `JobQueueItemKind::ResumedFiber`.
        Fiber* resumedFiber;
    };

    /// @brief Scheduling priority band.
    JobPriority priority{ JobPriority::Normal };

    /// @brief Required fiber stack tier for `JobQueueItemKind::JobEntry`.
    FiberStackTier stackTier{ FiberStackTier::Standard64KiB };

    /// @brief Payload discriminator (`JobEntry` vs. `ResumedFiber`).
    JobQueueItemKind kind{ JobQueueItemKind::JobEntry };

    /// @brief Explicit 8-bit zero padding.
    gp::UInt8 reserved8{ 0u };

    /// @brief Optional 16-bit scheduling flags (low 16 bits) + 16-bit padding.
    gp::UInt32 flags{ 0u };

    /// @brief Creates a `JobQueueItem` wrapping a fresh `JobDecl`.
    /// @param[in] decl Job declaration to wrap.
    /// @return Populated `JobQueueItem` with `kind == JobQueueItemKind::JobEntry`.
    [[nodiscard]] GP_FORCEINLINE_HINT static constexpr JobQueueItem fromJob(const JobDecl& decl) noexcept
    {
        JobQueueItem item{};
        item.entry = decl.entry;
        item.userData = decl.userData;
        item.counter = decl.counter;
        item.priority = decl.priority;
        item.stackTier = decl.stackTier;
        item.kind = JobQueueItemKind::JobEntry;
        item.reserved8 = 0u;
        item.flags = static_cast<gp::UInt32>(decl.flags);
        return item;
    }

    /// @brief Creates a `JobQueueItem` wrapping a suspended `gp::thread::Fiber*` ready to be resumed.
    /// @param[in] fiber Pointer to the unblocked child `Fiber` to resume.
    /// @param[in] inPriority Scheduling priority for the resumed fiber (defaults to `JobPriority::High`).
    /// @return Populated `JobQueueItem` with `kind == JobQueueItemKind::ResumedFiber`.
    [[nodiscard]] GP_FORCEINLINE_HINT static constexpr JobQueueItem fromResumedFiber(
        Fiber* const fiber, const JobPriority inPriority = JobPriority::High
    ) noexcept
    {
        JobQueueItem item{};
        item.entry = nullptr;
        item.userData = nullptr;
        item.resumedFiber = fiber;
        item.priority = inPriority;
        item.stackTier = FiberStackTier::Standard64KiB;
        item.kind = JobQueueItemKind::ResumedFiber;
        item.reserved8 = 0u;
        item.flags = 0u;
        return item;
    }

    /// @brief Queries whether this item represents a fresh `JobDecl` task entry.
    /// @return True if `kind == JobQueueItemKind::JobEntry`, false otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool isJobEntry() const noexcept
    {
        return kind == JobQueueItemKind::JobEntry;
    }

    /// @brief Queries whether this item represents a woken `gp::thread::Fiber*` ready for resumption.
    /// @return True if `kind == JobQueueItemKind::ResumedFiber`, false otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool isResumedFiber() const noexcept
    {
        return kind == JobQueueItemKind::ResumedFiber;
    }

    /// @brief Checks whether the active payload of this queue item is non-null.
    /// @return True if `entry != nullptr` (for `JobEntry`) or `resumedFiber != nullptr` (for `ResumedFiber`).
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool isValid() const noexcept
    {
        if (kind == JobQueueItemKind::ResumedFiber)
        {
            return resumedFiber != nullptr;
        }
        return entry != nullptr;
    }

    /// @brief Extracts the underlying `JobDecl` when `isJobEntry()` is true.
    /// @return Reconstructed 32-byte `JobDecl`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr JobDecl toJobDecl() const noexcept
    {
        JobDecl decl{};
        decl.entry = entry;
        decl.userData = userData;
        decl.counter = (kind == JobQueueItemKind::JobEntry) ? counter : nullptr;
        decl.priority = priority;
        decl.stackTier = stackTier;
        decl.flags = static_cast<gp::UInt16>(flags & 0xFFFFu);
        decl.reserved = 0u;
        return decl;
    }

    /// @brief Returns the `Fiber*` payload when `isResumedFiber()` is true, or `nullptr` otherwise.
    /// @return Pointer to the woken `Fiber`, or `nullptr`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr Fiber* getResumedFiber() const noexcept
    {
        return (kind == JobQueueItemKind::ResumedFiber) ? resumedFiber : nullptr;
    }

    /// @brief Returns the `JobCounter*` payload when `isJobEntry()` is true, or `nullptr` otherwise.
    /// @return Pointer to the job's completion `JobCounter`, or `nullptr`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr JobCounter* getJobCounter() const noexcept
    {
        return (kind == JobQueueItemKind::JobEntry) ? counter : nullptr;
    }
};

static_assert(sizeof(JobQueueItem) == 32u, "JobQueueItem must occupy exactly 32 bytes (2 per cache line).");
static_assert(alignof(JobQueueItem) == 16u, "JobQueueItem must be 16-byte aligned.");
static_assert(gp::concepts::IsTriviallyCopyable<JobQueueItem>, "JobQueueItem must be trivially copyable.");
static_assert(gp::concepts::IsStandardLayout<JobQueueItem>, "JobQueueItem must have standard layout.");

}   // namespace gp::thread
