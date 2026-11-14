// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "threading/jobs/JobGraph.hpp"
#include "memory/allocators/FiberScratchAllocator.hpp"
#include "threading/jobs/JobSystem.hpp"
#include "threading/sync/JobCounter.hpp"
#include <new>   // IWYU pragma: keep

namespace gp::thread
{

namespace detail
{

/// @brief Maximum node count validated using stack-resident scratch arrays (`1.5 KiB` total stack footprint) before
///        falling back to `FiberScratchAllocator`.
inline constexpr gp::UInt32 kInlineKahnScratchNodeThreshold = 256u;

/// @brief Executes Kahn's topological in-degree elimination algorithm over `nodes[0 .. nodeCount - 1]` to verify that
///        the graph is a Directed Acyclic Graph (DAG).
/// @param[in] nodes Contiguous array of `nodeCount` `JobGraphNode` instances.
/// @param[in] nodeCount Number of active nodes in `nodes`.
/// @param[out] inDegrees Scratch buffer of at least `nodeCount` `gp::UInt16` elements used to track remaining
///                       in-degrees.
/// @param[out] readyQueue Scratch FIFO queue buffer of at least `nodeCount` `gp::UInt32` node indices.
/// @return `JobSystemResult::Success` if every node reaches in-degree `0`, or `JobSystemResult::GraphCycleDetected` if
///         a cycle prevents one or more nodes from being visited.
[[nodiscard]] static JobSystemResult runKahnTopologicalValidation(
    const JobGraphNode* const nodes,
    const gp::UInt32 nodeCount,
    gp::UInt16* const inDegrees,
    gp::UInt32* const readyQueue
) noexcept
{
    gp::UInt32 queueHead = 0u;
    gp::UInt32 queueTail = 0u;

    for (gp::UInt32 i = 0u; i < nodeCount; ++i)
    {
        inDegrees[i] = nodes[i].initialPrerequisites;
        if (inDegrees[i] == 0u)
        {
            readyQueue[queueTail] = i;
            ++queueTail;
        }
    }

    while (queueHead < queueTail)
    {
        const gp::UInt32 currentIdx = readyQueue[queueHead];
        ++queueHead;

        const JobGraphNode& currentNode = nodes[currentIdx];
        const gp::UInt16 depCount = currentNode.dependentCount;
        for (gp::UInt16 d = 0u; d < depCount; ++d)
        {
            const JobGraphNode* const depPtr = currentNode.dependents[d];
            if (depPtr == nullptr) [[unlikely]]
            {
                continue;
            }

            const gp::USize depIndex = static_cast<gp::USize>(depPtr - nodes);
            if (depIndex < static_cast<gp::USize>(nodeCount) && inDegrees[depIndex] > 0u)
            {
                --inDegrees[depIndex];
                if (inDegrees[depIndex] == 0u)
                {
                    readyQueue[queueTail] = static_cast<gp::UInt32>(depIndex);
                    ++queueTail;
                }
            }
        }
    }

    return (queueTail == nodeCount) ? JobSystemResult::Success : JobSystemResult::GraphCycleDetected;
}

}   // namespace detail

JobGraph::~JobGraph() noexcept
{
    shutdown();
}

JobGraph::JobGraph(JobGraph&& other) noexcept
    : m_nodes(other.m_nodes)
    , m_edgePool(other.m_edgePool)
    , m_allocator(other.m_allocator)
    , m_maxNodes(other.m_maxNodes)
    , m_nodeCount(other.m_nodeCount)
    , m_maxDependentsPerNode(other.m_maxDependentsPerNode)
    , m_ownsMemory(other.m_ownsMemory)
{
    other.m_nodes = nullptr;
    other.m_edgePool = nullptr;
    other.m_allocator = nullptr;
    other.m_maxNodes = 0u;
    other.m_nodeCount = 0u;
    other.m_maxDependentsPerNode = 0u;
    other.m_ownsMemory = false;
}

JobGraph& JobGraph::operator=(JobGraph&& other) noexcept
{
    if (this != &other)
    {
        shutdown();

        m_nodes = other.m_nodes;
        m_edgePool = other.m_edgePool;
        m_allocator = other.m_allocator;
        m_maxNodes = other.m_maxNodes;
        m_nodeCount = other.m_nodeCount;
        m_maxDependentsPerNode = other.m_maxDependentsPerNode;
        m_ownsMemory = other.m_ownsMemory;

        other.m_nodes = nullptr;
        other.m_edgePool = nullptr;
        other.m_allocator = nullptr;
        other.m_maxNodes = 0u;
        other.m_nodeCount = 0u;
        other.m_maxDependentsPerNode = 0u;
        other.m_ownsMemory = false;
    }
    return *this;
}

JobSystemResult JobGraph::initialize(
    gp::memory::IAllocator& allocator, const gp::UInt32 maxNodes, const gp::UInt16 maxDependentsPerNode
) noexcept
{
    shutdown();

    if (maxNodes == 0u || maxDependentsPerNode == 0u)
    {
        return JobSystemResult::InvalidArguments;
    }

    const gp::USize nodesBytes = static_cast<gp::USize>(maxNodes) * sizeof(JobGraphNode);
    const gp::USize totalEdgeSlots = static_cast<gp::USize>(maxNodes) * static_cast<gp::USize>(maxDependentsPerNode);
    const gp::USize edgesBytes = totalEdgeSlots * sizeof(JobGraphNode*);

    void* const rawNodes = allocator.allocate(nodesBytes, static_cast<gp::UInt32>(alignof(JobGraphNode)));
    if (rawNodes == nullptr) [[unlikely]]
    {
        return JobSystemResult::OutOfMemory;
    }

    void* const rawEdges = allocator.allocate(edgesBytes, static_cast<gp::UInt32>(alignof(JobGraphNode*)));
    if (rawEdges == nullptr) [[unlikely]]
    {
        allocator.deallocate(rawNodes, nodesBytes, static_cast<gp::UInt32>(alignof(JobGraphNode)));
        return JobSystemResult::OutOfMemory;
    }

    m_nodes = static_cast<JobGraphNode*>(rawNodes);
    m_edgePool = static_cast<JobGraphNode**>(rawEdges);
    m_allocator = &allocator;
    m_maxNodes = maxNodes;
    m_nodeCount = 0u;
    m_maxDependentsPerNode = maxDependentsPerNode;
    m_ownsMemory = true;

    for (gp::UInt32 i = 0u; i < maxNodes; ++i)
    {
        new (&m_nodes[i]) JobGraphNode();
    }

    for (gp::USize e = 0u; e < totalEdgeSlots; ++e)
    {
        m_edgePool[e] = nullptr;
    }

    return JobSystemResult::Success;
}

JobSystemResult JobGraph::initializeWithBuffers(
    JobGraphNode* const nodeBuffer,
    const gp::UInt32 maxNodes,
    JobGraphNode** const edgePoolBuffer,
    const gp::UInt16 maxDependentsPerNode
) noexcept
{
    shutdown();

    if (nodeBuffer == nullptr || edgePoolBuffer == nullptr || maxNodes == 0u || maxDependentsPerNode == 0u)
    {
        return JobSystemResult::InvalidArguments;
    }

    if ((reinterpret_cast<gp::UIntPtr>(nodeBuffer) % alignof(JobGraphNode)) != 0u ||
        (reinterpret_cast<gp::UIntPtr>(edgePoolBuffer) % alignof(JobGraphNode*)) != 0u)
    {
        return JobSystemResult::InvalidArguments;
    }

    m_nodes = nodeBuffer;
    m_edgePool = edgePoolBuffer;
    m_allocator = nullptr;
    m_maxNodes = maxNodes;
    m_nodeCount = 0u;
    m_maxDependentsPerNode = maxDependentsPerNode;
    m_ownsMemory = false;

    const gp::USize totalEdgeSlots = static_cast<gp::USize>(maxNodes) * static_cast<gp::USize>(maxDependentsPerNode);
    for (gp::UInt32 i = 0u; i < maxNodes; ++i)
    {
        new (&m_nodes[i]) JobGraphNode();
    }

    for (gp::USize e = 0u; e < totalEdgeSlots; ++e)
    {
        m_edgePool[e] = nullptr;
    }

    return JobSystemResult::Success;
}

void JobGraph::shutdown() noexcept
{
    if (m_nodes != nullptr)
    {
        for (gp::UInt32 i = 0u; i < m_maxNodes; ++i)
        {
            m_nodes[i].~JobGraphNode();
        }

        if (m_ownsMemory && m_allocator != nullptr)
        {
            const gp::USize totalEdgeSlots =
                static_cast<gp::USize>(m_maxNodes) * static_cast<gp::USize>(m_maxDependentsPerNode);
            const gp::USize edgesBytes = totalEdgeSlots * sizeof(JobGraphNode*);
            const gp::USize nodesBytes = static_cast<gp::USize>(m_maxNodes) * sizeof(JobGraphNode);

            // Free in reverse allocation order so LIFO arenas can rewind their bump cursor.
            m_allocator->deallocate(m_edgePool, edgesBytes, static_cast<gp::UInt32>(alignof(JobGraphNode*)));
            m_allocator->deallocate(m_nodes, nodesBytes, static_cast<gp::UInt32>(alignof(JobGraphNode)));
        }
    }

    m_nodes = nullptr;
    m_edgePool = nullptr;
    m_allocator = nullptr;
    m_maxNodes = 0u;
    m_nodeCount = 0u;
    m_maxDependentsPerNode = 0u;
    m_ownsMemory = false;
}

void JobGraph::clear() noexcept
{
    for (gp::UInt32 i = 0u; i < m_nodeCount; ++i)
    {
        m_nodes[i].decl = JobDecl{};
        m_nodes[i].dependents = nullptr;
        m_nodes[i].ownerJobSystem = nullptr;
        m_nodes[i].completionCounter = nullptr;
        m_nodes[i].unresolvedPrerequisites.store(0u, gp::MemoryOrder::Relaxed);
        m_nodes[i].initialPrerequisites = 0u;
        m_nodes[i].dependentCount = 0u;
    }
    m_nodeCount = 0u;
}

void JobGraph::reset() noexcept
{
    for (gp::UInt32 i = 0u; i < m_nodeCount; ++i)
    {
        m_nodes[i].unresolvedPrerequisites.store(
            static_cast<gp::UInt32>(m_nodes[i].initialPrerequisites), gp::MemoryOrder::Relaxed
        );
    }
}

JobSystemResult JobGraph::addNode(const JobDecl& decl, JobGraphNodeId* const outNodeId) noexcept
{
    if (!isValid()) [[unlikely]]
    {
        return JobSystemResult::NotInitialized;
    }

    if (!decl.isValid()) [[unlikely]]
    {
        return JobSystemResult::InvalidArguments;
    }

    if (m_nodeCount >= m_maxNodes) [[unlikely]]
    {
        return JobSystemResult::CapacityExceeded;
    }

    const JobGraphNodeId assignedId = m_nodeCount;
    JobGraphNode& node = m_nodes[assignedId];
    node.decl = decl;
    node.dependents = &m_edgePool[static_cast<gp::USize>(assignedId) * static_cast<gp::USize>(m_maxDependentsPerNode)];
    node.ownerJobSystem = nullptr;
    node.completionCounter = nullptr;
    node.unresolvedPrerequisites.store(0u, gp::MemoryOrder::Relaxed);
    node.initialPrerequisites = 0u;
    node.dependentCount = 0u;

    ++m_nodeCount;
    if (outNodeId != nullptr)
    {
        *outNodeId = assignedId;
    }

    return JobSystemResult::Success;
}

JobSystemResult JobGraph::addDependency(const JobGraphNodeId prerequisiteId, const JobGraphNodeId dependentId) noexcept
{
    if (!isValid()) [[unlikely]]
    {
        return JobSystemResult::NotInitialized;
    }

    if (prerequisiteId >= m_nodeCount || dependentId >= m_nodeCount || prerequisiteId == dependentId) [[unlikely]]
    {
        return JobSystemResult::InvalidArguments;
    }

    JobGraphNode& prereq = m_nodes[prerequisiteId];
    JobGraphNode& dep = m_nodes[dependentId];

    for (gp::UInt16 i = 0u; i < prereq.dependentCount; ++i)
    {
        if (prereq.dependents[i] == &dep)
        {
            return JobSystemResult::Success;
        }
    }

    if (prereq.dependentCount >= m_maxDependentsPerNode || dep.initialPrerequisites == 0xFFFFu) [[unlikely]]
    {
        return JobSystemResult::CapacityExceeded;
    }

    prereq.dependents[prereq.dependentCount] = &dep;
    ++prereq.dependentCount;
    ++dep.initialPrerequisites;
    dep.unresolvedPrerequisites.store(static_cast<gp::UInt32>(dep.initialPrerequisites), gp::MemoryOrder::Relaxed);

    return JobSystemResult::Success;
}

JobSystemResult JobGraph::addDependencies(
    const JobGraphNodeId* const prerequisiteIds, const gp::UInt32 prerequisiteCount, const JobGraphNodeId dependentId
) noexcept
{
    if (!isValid()) [[unlikely]]
    {
        return JobSystemResult::NotInitialized;
    }

    if (prerequisiteCount == 0u)
    {
        return JobSystemResult::Success;
    }

    if (prerequisiteIds == nullptr) [[unlikely]]
    {
        return JobSystemResult::InvalidArguments;
    }

    for (gp::UInt32 i = 0u; i < prerequisiteCount; ++i)
    {
        const JobSystemResult res = addDependency(prerequisiteIds[i], dependentId);
        if (res != JobSystemResult::Success) [[unlikely]]
        {
            return res;
        }
    }

    return JobSystemResult::Success;
}

gp::UInt32 JobGraph::getRootNodeCount() const noexcept
{
    gp::UInt32 roots = 0u;
    for (gp::UInt32 i = 0u; i < m_nodeCount; ++i)
    {
        if (m_nodes[i].initialPrerequisites == 0u)
        {
            ++roots;
        }
    }
    return roots;
}

JobSystemResult JobGraph::validateTopology() const noexcept
{
    if (!isValid()) [[unlikely]]
    {
        return JobSystemResult::NotInitialized;
    }

    if (m_nodeCount <= 1u)
    {
        return JobSystemResult::Success;
    }

    if (m_nodeCount <= detail::kInlineKahnScratchNodeThreshold) [[likely]]
    {
        gp::UInt16 stackInDegrees[detail::kInlineKahnScratchNodeThreshold]{};
        gp::UInt32 stackReadyQueue[detail::kInlineKahnScratchNodeThreshold]{};
        return detail::runKahnTopologicalValidation(m_nodes, m_nodeCount, stackInDegrees, stackReadyQueue);
    }

    gp::memory::ScopedFiberScratchMark scratchMark;
    gp::memory::FiberScratchAllocator scratchAlloc;

    gp::UInt16* const scratchInDegrees = static_cast<gp::UInt16*>(
        scratchAlloc.allocate(static_cast<gp::USize>(m_nodeCount) * sizeof(gp::UInt16), alignof(gp::UInt16))
    );
    gp::UInt32* const scratchReadyQueue = static_cast<gp::UInt32*>(
        scratchAlloc.allocate(static_cast<gp::USize>(m_nodeCount) * sizeof(gp::UInt32), alignof(gp::UInt32))
    );
    if (scratchInDegrees == nullptr || scratchReadyQueue == nullptr) [[unlikely]]
    {
        return JobSystemResult::OutOfMemory;
    }

    return detail::runKahnTopologicalValidation(m_nodes, m_nodeCount, scratchInDegrees, scratchReadyQueue);
}

JobSystemResult JobGraph::submit(JobSystem& jobSystem, JobCounter* const completionCounter) noexcept
{
    if (!isValid() || !jobSystem.isInitialized()) [[unlikely]]
    {
        return JobSystemResult::NotInitialized;
    }

    if (m_nodeCount == 0u)
    {
        return JobSystemResult::Success;
    }

    const JobSystemResult topoRes = validateTopology();
    if (topoRes != JobSystemResult::Success) [[unlikely]]
    {
        return topoRes;
    }

    if (completionCounter != nullptr)
    {
        if (completionCounter->add(m_nodeCount) != JobSyncResult::Success) [[unlikely]]
        {
            return JobSystemResult::SyncError;
        }
    }

    for (gp::UInt32 i = 0u; i < m_nodeCount; ++i)
    {
        m_nodes[i].ownerJobSystem = &jobSystem;
        m_nodes[i].completionCounter = completionCounter;
        m_nodes[i].unresolvedPrerequisites.store(
            static_cast<gp::UInt32>(m_nodes[i].initialPrerequisites), gp::MemoryOrder::Release
        );
    }

    // Enqueue only the root passes. Downstream passes stay in arena memory without holding a fiber or queue slot
    // until their upstream prerequisites finish and decrement unresolvedPrerequisites to 0.
    const gp::UInt32 callerSlot =
        (JobSystem::getCurrentJobSystem() == &jobSystem) ? JobSystem::getCurrentWorkerIndex() : kInvalidWorkerIndex;
    gp::UInt32 dispatchedRoots = 0u;

    for (gp::UInt32 i = 0u; i < m_nodeCount; ++i)
    {
        if (m_nodes[i].initialPrerequisites == 0u)
        {
            const JobDecl rootDecl = JobDecl::make(
                &JobGraph::executeNodeTrampoline,
                &m_nodes[i],
                nullptr,
                m_nodes[i].decl.priority,
                m_nodes[i].decl.stackTier,
                m_nodes[i].decl.flags
            );
            const JobQueueItem item = JobQueueItem::fromJob(rootDecl);
            (void)jobSystem.enqueueItem(item, callerSlot, static_cast<JobFlags>(rootDecl.flags));
            ++dispatchedRoots;
        }
    }

    jobSystem.m_totalJobsDispatched.fetchAdd(static_cast<gp::UInt64>(dispatchedRoots), gp::MemoryOrder::Relaxed);
    jobSystem.wakeSleepingWorkers(dispatchedRoots);

    return JobSystemResult::Success;
}

JobSystemResult JobGraph::executeAndWait(JobSystem& jobSystem) noexcept
{
    if (!isValid() || !jobSystem.isInitialized()) [[unlikely]]
    {
        return JobSystemResult::NotInitialized;
    }

    if (m_nodeCount == 0u)
    {
        return JobSystemResult::Success;
    }

    CacheAlignedJobCounter graphCounter(0u);
    const JobSystemResult submitRes = submit(jobSystem, &graphCounter.counter);
    if (submitRes != JobSystemResult::Success) [[unlikely]]
    {
        return submitRes;
    }

    return jobSystem.waitForCounter(graphCounter, 0u);
}

void JobGraph::executeNodeTrampoline(void* const rawNode) noexcept
{
    JobGraphNode* const node = static_cast<JobGraphNode*>(rawNode);
    if (node == nullptr || node->ownerJobSystem == nullptr) [[unlikely]]
    {
        return;
    }

    JobSystem* const sys = node->ownerJobSystem;
    JobCounter* const graphCounter = node->completionCounter;

    // Run the pass callback and decrement any per-pass completion counter.
    (void)node->decl.invoke();
    if (node->decl.counter != nullptr)
    {
        (void)node->decl.counter->decrement(1u);
    }

    // Fan out ready downstream passes before decrementing graphCounter so graphCounter cannot hit 0 while this
    // function is still reading node->dependents.
    const gp::UInt16 depCount = node->dependentCount;
    for (gp::UInt16 i = 0u; i < depCount; ++i)
    {
        JobGraphNode* const dep = node->dependents[i];
        if (dep == nullptr) [[unlikely]]
        {
            continue;
        }

        const gp::UInt32 prevUnresolved = dep->unresolvedPrerequisites.fetchSub(1u, gp::MemoryOrder::AcqRel);
        if (prevUnresolved == 1u)
        {
            const JobDecl depWrapper = JobDecl::make(
                &JobGraph::executeNodeTrampoline, dep, nullptr, dep->decl.priority, dep->decl.stackTier, dep->decl.flags
            );
            (void)sys->dispatch(depWrapper, false);
        }
    }

    if (graphCounter != nullptr)
    {
        (void)graphCounter->decrement(1u);
    }
}

}   // namespace gp::thread
