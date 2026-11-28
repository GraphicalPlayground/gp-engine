// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/strings/Name.hpp"
#include "core/GPUMask.hpp"
#include "CoreMinimal.hpp"
#include "profiling/ProfilingForward.hpp"   // IWYU pragma: keep
#include "RHIBase.hpp"
#include "threading/atomics/Atomic.hpp"

namespace gp::rhi::profiling
{

/// @brief Video and system memory capacity, budget, and active usage reported by `DynamicRHI::getMemoryStats()`.
struct RHIMemoryStats
{
    /// @brief Total dedicated GPU video memory installed on the adapter in bytes.
    gp::UInt64 dedicatedVideoMemory{ 0u };

    /// @brief Dedicated system memory reserved exclusively for the GPU in bytes.
    gp::UInt64 dedicatedSystemMemory{ 0u };

    /// @brief Host system memory shared with the GPU in bytes.
    gp::UInt64 sharedSystemMemory{ 0u };

    /// @brief Total graphics-accessible memory (`0` if unknown).
    gp::UInt64 totalGraphicsMemory{ 0u };

    /// @brief Bytes of video memory demoted to system RAM due to OS memory pressure.
    gp::UInt64 demotedVideoMemory{ 0u };

    /// @brief OS-assigned video memory budget for this process in bytes.
    gp::UInt64 budgetVideoMemory{ 0u };

    /// @brief Current video memory resident/used by this process in bytes.
    gp::UInt64 usedVideoMemory{ 0u };

    /// @brief Remaining video memory available under `budgetVideoMemory` in bytes.
    gp::UInt64 availableVideoMemory{ 0u };

    /// @brief Reports whether process video memory usage exceeds the OS-assigned `budgetVideoMemory` or incurred
    ///        demotion.
    /// @return True when over budget (`demotedVideoMemory > 0` or `(budgetVideoMemory > 0 && usedVideoMemory >
    ///         budgetVideoMemory)`).
    [[nodiscard]] constexpr bool isOverBudget() const noexcept
    {
        return (demotedVideoMemory > 0u) || ((budgetVideoMemory > 0u) && (usedVideoMemory > budgetVideoMemory));
    }

    /// @brief Reports whether the backend populated meaningful capacity or usage metrics.
    /// @return True when any capacity or budget field is non-zero.
    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        return (dedicatedVideoMemory > 0u) || (sharedSystemMemory > 0u) || (totalGraphicsMemory > 0u) ||
               (budgetVideoMemory > 0u) || (usedVideoMemory > 0u);
    }

    /// @brief Default equality comparison.
    /// @param[in] rhs Stats to compare against.
    /// @return True when all eight fields match.
    [[nodiscard]] constexpr bool operator==(const RHIMemoryStats& rhs) const noexcept = default;
};

/// @brief Named draw-call statistics category accumulating per-GPU draw and primitive counts across frames.
class GP_RHI_API DrawStatsCategory final
{
public:
    /// @brief Maximum number of named draw stats categories supported simultaneously.
    static constexpr gp::UInt32 kMaxCategories = 31u;

    /// @brief Fallback slot index used for uncategorized draw calls.
    static constexpr gp::UInt32 kUncategorizedIndex = kMaxCategories;

    /// @brief Total number of category slots (`kMaxCategories + 1`).
    static constexpr gp::UInt32 kNumCategorySlots = kMaxCategories + 1u;

    /// @brief Central registry and double-buffered frame accumulator for all `DrawStatsCategory` instances.
    struct GP_RHI_API Manager
    {
        /// @brief Per-slot, per-GPU draw and primitive counters.
        struct Counters
        {
            gp::UInt32 draws{ 0u };
            gp::UInt32 primitives{ 0u };
        };

        /// @brief Registered category pointers (`nullptr` for unused slots).
        DrawStatsCategory* categories[kMaxCategories]{};

        /// @brief Number of registered named categories in `[0, kMaxCategories]`.
        gp::UInt32 numCategories{ 0u };

        /// @brief Live atomic accumulators written during command replay (`[slot][gpuIndex]`).
        gp::thread::Atomic<gp::UInt32> liveDraws[kNumCategorySlots][kMaxNumGPUs]{};
        gp::thread::Atomic<gp::UInt32> livePrimitives[kNumCategorySlots][kMaxNumGPUs]{};

        /// @brief Latched display counters from the most recently completed frame (`[slot][gpuIndex]`).
        Counters displayCounts[kNumCategorySlots][kMaxNumGPUs]{};

        /// @brief Copies and zeroes `liveDraws` / `livePrimitives` into `displayCounts` at frame end.
        void latchFrameCounters() noexcept;

        /// @brief Clears both live and latched display counters across all categories and GPUs.
        void resetAllCounters() noexcept;

        /// @brief Sums latched `displayCounts` draws across all categories for `gpuIndex`.
        /// @param[in] gpuIndex Zero-based GPU index.
        /// @return Total latched draw count.
        [[nodiscard]] gp::UInt32 getTotalDisplayDraws(gp::UInt32 gpuIndex = 0u) const noexcept;

        /// @brief Sums latched `displayCounts` primitives across all categories for `gpuIndex`.
        /// @param[in] gpuIndex Zero-based GPU index.
        /// @return Total latched primitive count.
        [[nodiscard]] gp::UInt32 getTotalDisplayPrimitives(gp::UInt32 gpuIndex = 0u) const noexcept;
    };

public:
    /// @brief Constructs an unregistered category assigned to `kUncategorizedIndex`.
    DrawStatsCategory() noexcept;

    /// @brief Constructs and registers a named draw stats category in `getManager()`.
    /// @param[in] inName Category identifier (e.g., `gp::Name("BasePass")`).
    explicit DrawStatsCategory(gp::Name inName) noexcept;

    DrawStatsCategory(const DrawStatsCategory&) = delete;
    DrawStatsCategory& operator=(const DrawStatsCategory&) = delete;
    DrawStatsCategory(DrawStatsCategory&&) = delete;
    DrawStatsCategory& operator=(DrawStatsCategory&&) = delete;

public:
    /// @brief Returns the category's interned name.
    /// @return `gp::Name`.
    [[nodiscard]] GP_FORCEINLINE_HINT gp::Name getName() const noexcept
    {
        return m_name;
    }

    /// @brief Returns the slot index in `[0, kUncategorizedIndex]`.
    /// @return Dense slot index.
    [[nodiscard]] GP_FORCEINLINE_HINT gp::UInt32 getIndex() const noexcept
    {
        return m_index;
    }

    /// @brief Reports whether this category should accumulate draw statistics.
    /// @return True when draw stats are compiled in and active.
    [[nodiscard]] GP_FORCEINLINE_HINT bool shouldCountDraws() const noexcept
    {
        return m_index <= kUncategorizedIndex;
    }

    /// @brief Returns the process-wide `DrawStatsCategory::Manager` singleton.
    /// @return Mutable reference to `Manager`.
    [[nodiscard]] static Manager& getManager() noexcept;

private:
    gp::Name m_name{};
    gp::UInt32 m_index{ kUncategorizedIndex };
};

/// @brief Global per-GPU draw call counter array updated by `recordDrawCallStats()`.
extern GP_RHI_API gp::thread::Atomic<gp::Int32> g_numDrawCallsRHI[kMaxNumGPUs];

/// @brief Global per-GPU rendered primitive counter array updated by `recordDrawCallStats()`.
extern GP_RHI_API gp::thread::Atomic<gp::Int32> g_numPrimitivesDrawnRHI[kMaxNumGPUs];

/// @brief Global per-GPU compute/mesh dispatch counter array updated by `recordDispatchCallStats()`.
extern GP_RHI_API gp::thread::Atomic<gp::Int32> g_numDispatchCallsRHI[kMaxNumGPUs];

/// @brief Accumulates draw and primitive counts across all GPUs selected in `gpuMask` and optional `category`.
/// @param[in] primitives Number of primitives rendered per instance/draw.
/// @param[in] numDraws Number of draw calls issued (`1` default).
/// @param[in] gpuMask Multi-GPU execution mask.
/// @param[in] category Optional `DrawStatsCategory` bucket (`nullptr` routes to `kUncategorizedIndex`).
GP_RHI_API void recordDrawCallStats(
    gp::UInt32 primitives,
    gp::UInt32 numDraws = 1u,
    GPUMask gpuMask = GPUMask::gpu0(),
    const DrawStatsCategory* category = nullptr
) noexcept;

/// @brief Accumulates compute/mesh dispatch counts across all GPUs selected in `gpuMask`.
/// @param[in] numDispatches Number of dispatches issued (`1` default).
/// @param[in] gpuMask Multi-GPU execution mask.
GP_RHI_API void recordDispatchCallStats(gp::UInt32 numDispatches = 1u, GPUMask gpuMask = GPUMask::gpu0()) noexcept;

/// @brief Resets `g_numDrawCallsRHI`, `g_numPrimitivesDrawnRHI`, `g_numDispatchCallsRHI`, and `DrawStatsCategory`
///        accumulators.
GP_RHI_API void resetDrawCallStats() noexcept;

/// @brief Queries and returns the latest `RHIMemoryStats` from the active `DynamicRHI`.
/// @return Populated `RHIMemoryStats` (zeroed if no `DynamicRHI` is active).
[[nodiscard]] GP_RHI_API RHIMemoryStats updateRHIMemoryStats() noexcept;

}   // namespace gp::rhi::profiling

#if GP_HAS_GPU_STATS
    /// @brief Records primitive and draw call stats for `primitiveType` (`numPrimitives` primitives).
    #define GP_RHI_DRAW_CALL_STATS(gpuMask, categoryPtr, numPrimitives) \
        ::gp::rhi::profiling::recordDrawCallStats((numPrimitives), 1u, (gpuMask), (categoryPtr))

    /// @brief Increments the draw call counter by 1 with zero primitive count (e.g., indirect draws).
    #define GP_RHI_DRAW_CALL_INC(gpuMask, categoryPtr) \
        ::gp::rhi::profiling::recordDrawCallStats(0u, 1u, (gpuMask), (categoryPtr))

    /// @brief Increments the dispatch call counter by 1 across `gpuMask`.
    #define GP_RHI_DISPATCH_CALL_INC(gpuMask) ::gp::rhi::profiling::recordDispatchCallStats(1u, (gpuMask))
#else
    #define GP_RHI_DRAW_CALL_STATS(gpuMask, categoryPtr, numPrimitives) \
        do                                                              \
        {                                                               \
            static_cast<void>(gpuMask);                                 \
            static_cast<void>(categoryPtr);                             \
            static_cast<void>(numPrimitives);                           \
        }                                                               \
        while (false)
    #define GP_RHI_DRAW_CALL_INC(gpuMask, categoryPtr) \
        do                                             \
        {                                              \
            static_cast<void>(gpuMask);                \
            static_cast<void>(categoryPtr);            \
        }                                              \
        while (false)
    #define GP_RHI_DISPATCH_CALL_INC(gpuMask) static_cast<void>(gpuMask)
#endif
