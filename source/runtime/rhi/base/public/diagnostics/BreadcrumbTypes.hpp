// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Vector.hpp"
#include "containers/strings/String.hpp"
#include "CoreMinimal.hpp"
#include "diagnostics/BreadcrumbEnums.hpp"
#include "diagnostics/DiagnosticsForward.hpp"   // IWYU pragma: keep
#include "RHIBase.hpp"
#include "sync/SyncEnums.hpp"

namespace gp::rhi::diagnostics
{

/// @brief Source code file path and 1-based line number captured at a breadcrumb declaration site.
struct BreadcrumbLocation
{
    /// @brief Null-terminated `__FILE__` string literal, or `nullptr` if unavailable.
    const char* file{ nullptr };

    /// @brief 1-based `__LINE__` number (`0` if unavailable).
    gp::UInt32 line{ 0u };

    /// @brief Constructs an empty source location.
    constexpr BreadcrumbLocation() noexcept = default;

    /// @brief Constructs a source location from `inFile` and `inLine`.
    /// @param[in] inFile Source file literal.
    /// @param[in] inLine Source line number.
    constexpr BreadcrumbLocation(const char* const inFile, const gp::UInt32 inLine) noexcept
        : file(inFile)
        , line(inLine)
    {}

    /// @brief Compares two source locations by pointer/string address and line number.
    /// @param[in] rhs Location to compare against.
    /// @return True when `file` and `line` match.
    [[nodiscard]] constexpr bool operator==(const BreadcrumbLocation& rhs) const noexcept = default;
};

/// @brief Immutable descriptor metadata describing a breadcrumb event's static label, format string, and call site.
struct BreadcrumbData
{
    /// @brief Static unformatted event name used by profiling categories and crash summaries (never null; defaults to
    ///        `"<unnamed>"`).
    const char* staticName{ "<unnamed>" };

    /// @brief Optional `printf`-style format string when the breadcrumb carries dynamic arguments, or `nullptr`.
    const char* dynamicFormat{ nullptr };

    /// @brief Call-site source file and line metadata.
    BreadcrumbLocation location{};

    /// @brief Verbosity tier assigned to this breadcrumb event.
    BreadcrumbVerbosity verbosity{ BreadcrumbVerbosity::Normal };

    /// @brief Constructs default breadcrumb metadata.
    constexpr BreadcrumbData() noexcept = default;

    /// @brief Constructs breadcrumb metadata with an explicit static name, optional format, and source location.
    /// @param[in] inStaticName Static compile-time name string.
    /// @param[in] inDynamicFormat Optional format string (`nullptr` for static-only breadcrumbs).
    /// @param[in] inLocation Source file and line metadata.
    /// @param[in] inVerbosity Verbosity tier.
    constexpr BreadcrumbData(
        const char* const inStaticName,
        const char* const inDynamicFormat = nullptr,
        const BreadcrumbLocation inLocation = {},
        const BreadcrumbVerbosity inVerbosity = BreadcrumbVerbosity::Normal
    ) noexcept
        : staticName(inStaticName != nullptr ? inStaticName : "<unnamed>")
        , dynamicFormat(inDynamicFormat)
        , location(inLocation)
        , verbosity(inVerbosity)
    {}
};

/// @brief Fixed-size stack character buffer used by `BreadcrumbNode::getDebugStr()` without heap allocation.
struct BreadcrumbBuffer
{
    /// @brief Capacity in bytes including the trailing null terminator.
    static constexpr gp::USize kCapacity = 256u;

    /// @brief Inline character storage.
    char data[kCapacity]{};
};

/// @brief Identifies a hardware execution queue by multi-GPU device index and `sync::PipelineStage`.
struct BreadcrumbQueueId
{
    /// @brief Zero-based GPU adapter index.
    gp::UInt32 deviceIndex{ 0u };

    /// @brief Single-bit execution pipeline (`Graphics` or `AsyncCompute`).
    sync::PipelineStage pipeline{ sync::PipelineStage::Graphics };

    /// @brief Constructs a queue identifier for `(0, Graphics)`.
    constexpr BreadcrumbQueueId() noexcept = default;

    /// @brief Constructs a queue identifier for `(inDeviceIndex, inPipeline)`.
    /// @param[in] inDeviceIndex Zero-based GPU index.
    /// @param[in] inPipeline Single-bit execution pipeline.
    constexpr BreadcrumbQueueId(const gp::UInt32 inDeviceIndex, const sync::PipelineStage inPipeline) noexcept
        : deviceIndex(inDeviceIndex)
        , pipeline(inPipeline)
    {}

    /// @brief Default equality comparison.
    /// @param[in] rhs Queue ID to compare against.
    /// @return True when both `deviceIndex` and `pipeline` match.
    [[nodiscard]] constexpr bool operator==(const BreadcrumbQueueId& rhs) const noexcept = default;
};

/// @brief Reconstructs and formats active/finished/unstarted GPU breadcrumb trees across queues during crash analysis.
/// @details Backends populate `BreadcrumbState` either by linking a submitted `BreadcrumbRange` alongside the last
///          GPU-written `markerIn` / `markerOut` IDs (`linkRange()`), or by recording explicit per-node marker states
///          (`setNodeState()`). `dumpActiveBreadcrumbs()` produces a human-readable hierarchical ASCII tree suitable
///          for crash logs and automated test assertions.
class GP_RHI_API BreadcrumbState final
{
public:
    /// @brief Associates a breadcrumb node with its observed marker state on a specific queue.
    struct RecordedNodeState
    {
        /// @brief Queue on which the node was observed.
        BreadcrumbQueueId queue{};

        /// @brief Non-null breadcrumb node pointer.
        const BreadcrumbNode* node{ nullptr };

        /// @brief Observed GPU execution state (`NotStarted`, `Active`, or `Finished`).
        BreadcrumbMarkerState state{ BreadcrumbMarkerState::NotStarted };
    };

public:
    /// @brief Constructs an empty breadcrumb crash state snapshot.
    BreadcrumbState() noexcept = default;

    /// @brief Clears all recorded queue ranges and node states.
    void reset() noexcept;

    /// @brief Records or updates the explicit execution state of `node` on `queue` (and propagates `Active` to
    ///        ancestors if needed).
    /// @param[in] queue Hardware queue identifier.
    /// @param[in] node Breadcrumb node (ignored if null or sentinel).
    /// @param[in] state Observed GPU execution state.
    void setNodeState(BreadcrumbQueueId queue, const BreadcrumbNode* node, BreadcrumbMarkerState state) noexcept;

    /// @brief Classifies every node in `range` on `queue` using monotonically written GPU `markerIn` and `markerOut`
    ///        breadcrumb IDs.
    /// @details Nodes whose `id` was entered (`<= markerIn` or ancestor of `markerIn`) and exited (`<= markerOut` and
    ///          not an ancestor of an active node) are marked `Finished`; nodes entered but not yet exited are marked
    ///          `Active`; nodes after `markerIn` are marked `NotStarted`.
    /// @param[in] queue Hardware queue identifier.
    /// @param[in] range Submitted breadcrumb range on `queue`.
    /// @param[in] markerIn Last breadcrumb node ID written at scope entry (`0` if none).
    /// @param[in] markerOut Last breadcrumb node ID written at scope exit (`0` if none).
    void linkRange(
        BreadcrumbQueueId queue, const BreadcrumbRange& range, gp::UInt32 markerIn, gp::UInt32 markerOut
    ) noexcept;

    /// @brief Reports whether any breadcrumb nodes have been recorded in this state snapshot.
    /// @return True when empty.
    [[nodiscard]] bool isEmpty() const noexcept
    {
        return m_entries.isEmpty();
    }

    /// @brief Formats the reconstructed breadcrumb hierarchy for every recorded queue into a multi-line string.
    /// @param[in] includeFinishedAndNotStarted True to include `[Finished]` and `[NotStarted]` sibling nodes alongside
    ///            `[Active]` paths; false to filter strictly to `[Active]` nodes.
    /// @return Formatted multi-line breadcrumb tree report.
    [[nodiscard]] gp::String dumpActiveBreadcrumbs(bool includeFinishedAndNotStarted = true) const noexcept;

    /// @brief Logs `dumpActiveBreadcrumbs()` to the `LogRHI` error channel.
    /// @param[in] includeFinishedAndNotStarted True to include sibling `[Finished]` and `[NotStarted]` nodes.
    void logActiveBreadcrumbs(bool includeFinishedAndNotStarted = true) const noexcept;

private:
    gp::Vector<RecordedNodeState> m_entries{};
};

}   // namespace gp::rhi::diagnostics
