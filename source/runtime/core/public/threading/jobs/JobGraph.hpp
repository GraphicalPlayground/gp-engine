// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "memory/allocators/IAllocator.hpp"
#include "memory/HardwareConstants.hpp"
#include "memory/MemoryOrder.hpp"
#include "threading/atomics/Atomic.hpp"
#include "threading/fibers/FiberEnums.hpp"
#include "threading/jobs/JobSystemEnums.hpp"
#include "threading/jobs/JobSystemForward.hpp"
#include "threading/jobs/JobSystemTypes.hpp"
#include "threading/queues/JobQueueDesc.hpp"
#include "threading/queues/JobQueueEnums.hpp"
#include "threading/sync/JobSyncForward.hpp"

namespace gp::thread
{

/// @brief Cache-line-isolated Directed Acyclic Graph (DAG) node for Render Dependency Graph (RDG) pass
///        scheduling ([ADR-0025], [ADR-0052]).
/// @details A waiting `JobGraphNode` resides in arena memory and holds no pooled fiber or queue slot while its
///          upstream prerequisites are running. When the last prerequisite finishes and decrements
///          `unresolvedPrerequisites` to `0`, the completing worker thread enqueues this node into `ownerJobSystem`.
struct alignas(gp::memory::HardwareConstants::cacheLineSize) GP_CORE_API JobGraphNode final
{
    /// @brief 32-byte task declaration (`entry`, `userData`, optional per-node `counter`, `priority`, `stackTier`).
    JobDecl decl{};

    /// @brief Pointer to this node's pre-sliced array of downstream dependent `JobGraphNode*` pointers.
    JobGraphNode** dependents{ nullptr };

    /// @brief Pointer to the `JobSystem` bound when the owning `JobGraph` is submitted.
    JobSystem* ownerJobSystem{ nullptr };

    /// @brief Optional graph-wide completion counter decremented by `1` after this node and its fan-out finish.
    JobCounter* completionCounter{ nullptr };

    /// @brief Atomic countdown of unfinished upstream prerequisite nodes (`0` means ready to execute).
    Atomic<gp::UInt32> unresolvedPrerequisites{ 0u };

    /// @brief Number of upstream prerequisite edges registered on this node, preserved for `JobGraph::reset()`.
    gp::UInt16 initialPrerequisites{ 0u };

    /// @brief Number of downstream dependent nodes stored in `dependents[0 .. dependentCount - 1]`.
    gp::UInt16 dependentCount{ 0u };

    /// @brief Constructs an empty, unconfigured `JobGraphNode`.
    [[nodiscard]] constexpr JobGraphNode() noexcept = default;

    JobGraphNode(const JobGraphNode&) = delete;
    JobGraphNode& operator=(const JobGraphNode&) = delete;
    JobGraphNode(JobGraphNode&&) = delete;
    JobGraphNode& operator=(JobGraphNode&&) = delete;

    /// @brief Destructor.
    ~JobGraphNode() noexcept = default;

    /// @brief Queries whether this node has zero upstream prerequisites (`initialPrerequisites == 0`).
    /// @return True if this node is a root entry pass in the DAG.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool isRootNode() const noexcept
    {
        return initialPrerequisites == 0u;
    }

    /// @brief Queries the current number of unfinished upstream prerequisites.
    /// @param[in] order Memory ordering constraint for the atomic load (defaults to `MemoryOrder::Acquire`).
    /// @return Remaining prerequisite count.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::UInt32
        getUnresolvedPrerequisiteCount(const gp::MemoryOrder order = gp::MemoryOrder::Acquire) const noexcept
    {
        return unresolvedPrerequisites.load(order);
    }
};

static_assert(
    sizeof(JobGraphNode) == gp::memory::HardwareConstants::cacheLineSize,
    "JobGraphNode must occupy one hardware cache line to avoid MESI false sharing."
);

/// @brief Declarative Directed Acyclic Graph (DAG) builder and executor for RDG passes and multi-stage simulation
///        pipelines ([ADR-0052]).
/// @details Uses caller-supplied buffers or any `gp::memory::IAllocator` (such as a per-frame `LinearAllocator` or
///          `FiberScratchAllocator`) to store `JobGraphNode` entries and their outgoing edge lists. Validates
///          topological acyclicity via Kahn's algorithm and dispatches ready passes across `JobSystem` as their
///          prerequisites complete.
class GP_CORE_API JobGraph final
{
private:
    JobGraphNode* m_nodes{ nullptr };
    JobGraphNode** m_edgePool{ nullptr };
    gp::memory::IAllocator* m_allocator{ nullptr };
    gp::UInt32 m_maxNodes{ 0u };
    gp::UInt32 m_nodeCount{ 0u };
    gp::UInt16 m_maxDependentsPerNode{ 0u };
    bool m_ownsMemory{ false };

public:
    /// @brief Constructs an uninitialized `JobGraph` (`isValid() == false`).
    [[nodiscard]] constexpr JobGraph() noexcept = default;

    /// @brief Destroys the graph, releasing backing buffers to `m_allocator` if owned.
    ~JobGraph() noexcept;

    JobGraph(const JobGraph&) = delete;
    JobGraph& operator=(const JobGraph&) = delete;

    /// @brief Move constructor transferring graph storage ownership.
    /// @param[in,out] other Source graph to move from (left uninitialized).
    [[nodiscard]] JobGraph(JobGraph&& other) noexcept;

    /// @brief Move assignment operator transferring graph storage ownership.
    /// @param[in,out] other Source graph to move from (left uninitialized).
    /// @return Reference to this `JobGraph`.
    JobGraph& operator=(JobGraph&& other) noexcept;

public:
    /// @section Initialization & Memory Binding

    /// @brief Allocates backing storage for `maxNodes` cache-aligned `JobGraphNode` entries and their adjacency lists
    ///        from `allocator`.
    /// @param[in,out] allocator Arena or heap allocator (`LinearAllocator`, `FiberScratchAllocator`, etc.).
    /// @param[in] maxNodes Maximum number of DAG nodes supported (`>= 1`).
    /// @param[in] maxDependentsPerNode Maximum outgoing dependent edges per node (`>= 1`, defaults to `16u`).
    /// @return `JobSystemResult::Success` on allocation, or an error code.
    [[nodiscard]] JobSystemResult initialize(
        gp::memory::IAllocator& allocator, gp::UInt32 maxNodes, gp::UInt16 maxDependentsPerNode = 16u
    ) noexcept;

    /// @brief Initializes the graph over caller-supplied node and edge pointer buffers without allocator calls.
    /// @param[in] nodeBuffer Pointer to an array of at least `maxNodes` `JobGraphNode` elements (cache-line aligned).
    /// @param[in] maxNodes Capacity of `nodeBuffer` (`>= 1`).
    /// @param[in] edgePoolBuffer Pointer to an array of at least `maxNodes * maxDependentsPerNode` `JobGraphNode*`
    ///                           entries.
    /// @param[in] maxDependentsPerNode Maximum outgoing dependent edges per node (`>= 1`).
    /// @return `JobSystemResult::Success`, or `JobSystemResult::InvalidArguments` on null or misaligned pointers.
    [[nodiscard]] JobSystemResult initializeWithBuffers(
        JobGraphNode* nodeBuffer, gp::UInt32 maxNodes, JobGraphNode** edgePoolBuffer, gp::UInt16 maxDependentsPerNode
    ) noexcept;

    /// @brief Releases owned memory back to `m_allocator` and resets the graph to an uninitialized state.
    void shutdown() noexcept;

    /// @brief Clears all added nodes and edges while retaining the allocated backing buffers.
    void clear() noexcept;

    /// @brief Restores `unresolvedPrerequisites = initialPrerequisites` across all nodes so the compiled graph can be
    ///        submitted again without rebuilding nodes or edges.
    void reset() noexcept;

public:
    /// @section DAG Node & Dependency Construction

    /// @brief Adds a new task node to the graph from a `JobDecl`.
    /// @param[in] decl 32-byte job declaration (`entry` must be non-null).
    /// @param[out] outNodeId Optional pointer receiving the `0`-based `JobGraphNodeId` of the created node.
    /// @return `JobSystemResult::Success`, `JobSystemResult::InvalidArguments`, or `JobSystemResult::CapacityExceeded`.
    [[nodiscard]] JobSystemResult addNode(const JobDecl& decl, JobGraphNodeId* outNodeId = nullptr) noexcept;

    /// @brief Convenience overload that constructs and adds a task node from callback parameters.
    /// @param[in] entry Stateless task callback `void (*)(void*)`.
    /// @param[in] userData Opaque caller-managed context pointer.
    /// @param[out] outNodeId Optional pointer receiving the created `JobGraphNodeId`.
    /// @param[in] priority Scheduling priority band (`JobPriority::Normal` by default).
    /// @param[in] stackTier Required fiber stack tier (`FiberStackTier::Standard64KiB` by default).
    /// @param[in] flags Optional scheduling flags (`JobFlags::RDGPass` by default for graph nodes).
    /// @return `JobSystemResult::Success`, or an error code.
    [[nodiscard]] GP_FORCEINLINE_HINT JobSystemResult addNode(
        const JobEntryFunction entry,
        void* const userData = nullptr,
        JobGraphNodeId* const outNodeId = nullptr,
        const JobPriority priority = JobPriority::Normal,
        const FiberStackTier stackTier = FiberStackTier::Standard64KiB,
        const JobFlags flags = JobFlags::RDGPass
    ) noexcept
    {
        const JobDecl decl =
            JobDecl::make(entry, userData, nullptr, priority, stackTier, static_cast<gp::UInt16>(flags));
        return addNode(decl, outNodeId);
    }

    /// @brief Registers a directed dependency edge `prerequisiteId -> dependentId` so `dependentId` waits until
    ///        `prerequisiteId` finishes.
    /// @note Idempotent if the edge `prerequisiteId -> dependentId` is already present.
    /// @param[in] prerequisiteId Upstream pass or task node ID that must complete first.
    /// @param[in] dependentId Downstream pass or task node ID that depends on `prerequisiteId`.
    /// @return `JobSystemResult::Success`, `JobSystemResult::InvalidArguments` (out-of-range or self-loop), or
    ///         `JobSystemResult::CapacityExceeded`.
    [[nodiscard]] JobSystemResult addDependency(JobGraphNodeId prerequisiteId, JobGraphNodeId dependentId) noexcept;

    /// @brief Registers multiple upstream prerequisites for a single `dependentId` pass (`prerequisites[i] ->
    ///        dependentId`).
    /// @param[in] prerequisiteIds Array of upstream `JobGraphNodeId` handles.
    /// @param[in] prerequisiteCount Number of elements in `prerequisiteIds`.
    /// @param[in] dependentId Downstream pass or task node ID.
    /// @return `JobSystemResult::Success`, or the first encountered error code.
    [[nodiscard]] JobSystemResult addDependencies(
        const JobGraphNodeId* prerequisiteIds, gp::UInt32 prerequisiteCount, JobGraphNodeId dependentId
    ) noexcept;

    /// @brief Verifies that the directed graph contains no cycles using Kahn's topological in-degree elimination
    ///        without mutating node state.
    /// @return `JobSystemResult::Success` if the graph is a valid DAG, or `JobSystemResult::GraphCycleDetected` if a
    ///         cycle exists.
    [[nodiscard]] JobSystemResult validateTopology() const noexcept;

public:
    /// @section Submission & Execution

    /// @brief Submits the compiled DAG to `jobSystem` for asynchronous execution.
    /// @details Adds `m_nodeCount` to `completionCounter` (if non-null) and dispatches the root nodes
    ///          (`initialPrerequisites == 0u`). Downstream nodes are dispatched automatically as their prerequisites
    ///          complete.
    /// @param[in,out] jobSystem Target `JobSystem` instance to execute the DAG.
    /// @param[in,out] completionCounter Optional counter incremented by `m_nodeCount` on submission and decremented as
    ///                                  each node completes (reaches `0` when all nodes in the graph finish).
    /// @return `JobSystemResult::Success`, or an error code.
    [[nodiscard]] JobSystemResult submit(JobSystem& jobSystem, JobCounter* completionCounter = nullptr) noexcept;

    /// @brief Submits the compiled DAG to `jobSystem` and cooperatively waits (with work-helping) until all nodes in
    ///        the graph have completed execution.
    /// @param[in,out] jobSystem Target `JobSystem` instance.
    /// @return `JobSystemResult::Success` once every node has completed, or an error code.
    [[nodiscard]] JobSystemResult executeAndWait(JobSystem& jobSystem) noexcept;

public:
    /// @section Queries & Diagnostics

    /// @brief Checks whether the graph is initialized with valid backing storage.
    /// @return True if `m_nodes != nullptr && m_edgePool != nullptr && m_maxNodes > 0u`.
    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        return m_nodes != nullptr && m_edgePool != nullptr && m_maxNodes > 0u;
    }

    /// @brief Checks whether the graph currently contains zero nodes (`m_nodeCount == 0u`).
    /// @return True if empty.
    [[nodiscard]] constexpr bool isEmpty() const noexcept
    {
        return m_nodeCount == 0u;
    }

    /// @brief Queries the number of nodes currently registered in the graph.
    /// @return Active node count (`0 .. m_maxNodes`).
    [[nodiscard]] constexpr gp::UInt32 getNodeCount() const noexcept
    {
        return m_nodeCount;
    }

    /// @brief Queries the maximum node capacity of this graph.
    /// @return Maximum node count.
    [[nodiscard]] constexpr gp::UInt32 getMaxNodes() const noexcept
    {
        return m_maxNodes;
    }

    /// @brief Queries the maximum number of outgoing dependent edges allowed per node.
    /// @return Maximum dependents per node.
    [[nodiscard]] constexpr gp::UInt16 getMaxDependentsPerNode() const noexcept
    {
        return m_maxDependentsPerNode;
    }

    /// @brief Counts the number of root nodes (`initialPrerequisites == 0`) currently in the graph.
    /// @return Root node count.
    [[nodiscard]] gp::UInt32 getRootNodeCount() const noexcept;

    /// @brief Retrieves a const pointer to the `JobGraphNode` at `nodeId`, or `nullptr` if out of bounds.
    /// @param[in] nodeId Node identifier in `[0, m_nodeCount)`.
    /// @return Const pointer to the `JobGraphNode`, or `nullptr`.
    [[nodiscard]] constexpr const JobGraphNode* getNode(const JobGraphNodeId nodeId) const noexcept
    {
        return (nodeId < m_nodeCount) ? &m_nodes[nodeId] : nullptr;
    }

private:
    /// @brief Static `JobEntryFunction` trampoline that executes a ready `JobGraphNode` and fans out its dependents.
    /// @details Invokes `node->decl.invoke()`, decrements any per-node `node->decl.counter`, atomically decrements
    ///          `unresolvedPrerequisites` on each downstream dependent node (enqueuing any dependent whose prerequisite
    ///          count reaches `0`), and finally decrements `node->completionCounter`.
    /// @param[in] rawNode Pointer to the ready `JobGraphNode` instance.
    static void executeNodeTrampoline(void* rawNode) noexcept;
};

}   // namespace gp::thread
