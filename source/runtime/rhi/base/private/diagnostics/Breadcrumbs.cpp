// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "diagnostics/Breadcrumbs.hpp"
#include "commands/CommandListBase.hpp"
#include "containers/arrays/Vector.hpp"      // IWYU pragma: keep
#include "containers/strings/String.hpp"     // IWYU pragma: keep
#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "core/RHIStrings.hpp"
#include "CoreMinimal.hpp"
#include "memory/Memory.hpp"
#include "memory/MemoryOrder.hpp"
#include "RHIDefinitions.hpp"
#include "sync/SyncEnums.hpp"
#include "threading/atomics/Atomic.hpp"
#include <charconv>
#include <new>   // IWYU pragma: keep
#include <system_error>
#include <utility>

namespace gp::rhi::diagnostics
{

namespace
{

gp::thread::Atomic<gp::UInt32> g_nextBreadcrumbNodeId{ 1u };

[[nodiscard]] gp::UInt32 allocateBreadcrumbNodeId() noexcept
{
    gp::UInt32 nextId = g_nextBreadcrumbNodeId.fetchAdd(1u, gp::memory::MemoryOrder::Relaxed);
    if (nextId == BreadcrumbNode::kSentinelId || nextId == BreadcrumbNode::kInvalidId) [[unlikely]]
    {
        nextId = g_nextBreadcrumbNodeId.fetchAdd(2u, gp::memory::MemoryOrder::Relaxed);
        if (nextId == BreadcrumbNode::kSentinelId || nextId == BreadcrumbNode::kInvalidId)
        {
            nextId = 1u;
        }
    }
    return nextId;
}

void appendUInt32ToString(gp::String& out, const gp::UInt32 value) noexcept
{
    char buf[16]{};
    const std::to_chars_result res = std::to_chars(buf, buf + sizeof(buf), value, 10);
    if (res.ec == std::errc{})
    {
        out.append(buf, static_cast<gp::USize>(res.ptr - buf));
    }
}

[[nodiscard]] const char* markerStateToString(const BreadcrumbMarkerState state) noexcept
{
    switch (state)
    {
    case BreadcrumbMarkerState::Active:
        return "Active";

    case BreadcrumbMarkerState::Finished:
        return "Finished";

    case BreadcrumbMarkerState::NotStarted:
    default:
        return "NotStarted";
    }
}

[[nodiscard]] bool isAncestorOf(const BreadcrumbNode* const candidateAncestor, const BreadcrumbNode* node) noexcept
{
    if (candidateAncestor == nullptr || candidateAncestor == BreadcrumbNode::getSentinel())
    {
        return false;
    }

    const BreadcrumbNode* current = node;
    while (current != nullptr && current != BreadcrumbNode::getSentinel())
    {
        if (current == candidateAncestor)
        {
            return true;
        }
        current = current->getParent();
    }
    return false;
}

}   // namespace

BreadcrumbNode::BreadcrumbNode(const BreadcrumbData& inData) noexcept
    : data(inData)
    , id(allocateBreadcrumbNodeId())
{}

BreadcrumbNode::BreadcrumbNode([[maybe_unused]] const SentinelTag tag) noexcept
    : data(BreadcrumbData("<sentinel>", nullptr, BreadcrumbLocation{}, BreadcrumbVerbosity::Minimal))
    , id(kSentinelId)
{}

BreadcrumbNode* BreadcrumbNode::getSentinel() noexcept
{
    static BreadcrumbNode s_sentinel(SentinelTag{});
    return &s_sentinel;
}

void BreadcrumbNode::setParent(BreadcrumbNode* const parent) noexcept
{
    GP_CHECKF(this != getSentinel(), "Cannot assign a parent to the global BreadcrumbNode::getSentinel() singleton.");
    GP_CHECKF(
        m_parent == nullptr || m_parent == getSentinel(),
        "BreadcrumbNode::setParent() may only be called on root or Sentinel-parented nodes."
    );
    m_parent = parent;
}

gp::UInt32 BreadcrumbNode::getLevel(const BreadcrumbNode* node) noexcept
{
    gp::UInt32 level = 0u;
    const BreadcrumbNode* const sentinel = getSentinel();
    while (node != nullptr && node != sentinel)
    {
        ++level;
        node = node->m_parent;
    }
    return level;
}

BreadcrumbNode* BreadcrumbNode::getNonNullRoot(BreadcrumbNode* node) noexcept
{
    BreadcrumbNode* const sentinel = getSentinel();
    if (node == nullptr || node == sentinel)
    {
        return nullptr;
    }

    while (node->m_parent != nullptr && node->m_parent != sentinel)
    {
        node = node->m_parent;
    }
    return node;
}

const BreadcrumbNode* BreadcrumbNode::getNonNullRoot(const BreadcrumbNode* node) noexcept
{
    const BreadcrumbNode* const sentinel = getSentinel();
    if (node == nullptr || node == sentinel)
    {
        return nullptr;
    }

    while (node->m_parent != nullptr && node->m_parent != sentinel)
    {
        node = node->m_parent;
    }
    return node;
}

const BreadcrumbNode*
    BreadcrumbNode::findCommonAncestor(const BreadcrumbNode* node0, const BreadcrumbNode* node1) noexcept
{
    const BreadcrumbNode* const sentinel = getSentinel();
    if (node0 == sentinel)
    {
        node0 = nullptr;
    }
    if (node1 == sentinel)
    {
        node1 = nullptr;
    }

    gp::UInt32 level0 = getLevel(node0);
    gp::UInt32 level1 = getLevel(node1);

    while (level0 > level1 && node0 != nullptr)
    {
        node0 = node0->m_parent;
        if (node0 == sentinel)
        {
            node0 = nullptr;
        }
        --level0;
    }

    while (level1 > level0 && node1 != nullptr)
    {
        node1 = node1->m_parent;
        if (node1 == sentinel)
        {
            node1 = nullptr;
        }
        --level1;
    }

    while (node0 != node1)
    {
        node0 = (node0 != nullptr) ? node0->m_parent : nullptr;
        node1 = (node1 != nullptr) ? node1->m_parent : nullptr;
        if (node0 == sentinel)
        {
            node0 = nullptr;
        }
        if (node1 == sentinel)
        {
            node1 = nullptr;
        }
    }

    return node0;
}

gp::String BreadcrumbNode::getFullPath() const noexcept
{
    const BreadcrumbNode* const sentinel = getSentinel();
    if (this == sentinel)
    {
        return gp::String("<sentinel>");
    }

    gp::Vector<const BreadcrumbNode*> chain{};
    const BreadcrumbNode* current = this;
    while (current != nullptr && current != sentinel)
    {
        chain.emplaceBack(current);
        current = current->m_parent;
    }

    gp::String result{};
    BreadcrumbBuffer scratch{};
    for (gp::USize idx = chain.size(); idx > 0u; --idx)
    {
        if (idx != chain.size())
        {
            result.pushBack('/');
        }
        const char* const label = chain[idx - 1u]->getDebugStr(scratch);
        if (label != nullptr)
        {
            result.append(gp::StringView(label));
        }
    }
    return result;
}

void BreadcrumbList::append(BreadcrumbNode* const node) noexcept
{
    GP_CHECKF(
        node != nullptr && node != BreadcrumbNode::getSentinel(), "Invalid node passed to BreadcrumbList::append."
    );
    GP_CHECKF(node->m_nextPtr == nullptr, "BreadcrumbNode is already linked into another BreadcrumbList or range.");

    if (first == nullptr)
    {
        first = node;
        last = node;
    }
    else
    {
        last->m_nextPtr = node;
        last = node;
    }
}

BreadcrumbRange BreadcrumbList::getRange() const noexcept
{
    return BreadcrumbRange(first, last);
}

void BreadcrumbRange::insertAfter(const BreadcrumbRange& other, BreadcrumbNode* const prev) noexcept
{
    if (!other)
    {
        return;
    }

    if (first == nullptr)
    {
        GP_CHECKF(prev == nullptr, "Cannot insertAfter a non-null prev into an empty BreadcrumbRange.");
        first = other.first;
        last = other.last;
        return;
    }

    if (prev == nullptr)
    {
        other.last->m_nextPtr = first;
        first = other.first;
        return;
    }

    BreadcrumbNode* const oldNext = (prev == last) ? nullptr : prev->m_nextPtr;
    prev->m_nextPtr = other.first;
    other.last->m_nextPtr = oldNext;
    if (prev == last)
    {
        last = other.last;
    }
}

void BreadcrumbAllocatorArray::addUnique(const gp::SharedPtr<BreadcrumbAllocator>& allocator) noexcept
{
    if (!allocator.isValid())
    {
        return;
    }

    for (const gp::SharedPtr<BreadcrumbAllocator>& existing: m_allocators)
    {
        if (existing.get() == allocator.get())
        {
            return;
        }
    }

    m_allocators.emplaceBack(allocator);
}

void BreadcrumbAllocatorArray::addUnique(BreadcrumbAllocator* const allocator) noexcept
{
    if (allocator == nullptr || !allocator->doesSharedInstanceExist())
    {
        return;
    }
    addUnique(gp::SharedPtr<BreadcrumbAllocator>(allocator->asShared()));
}

void BreadcrumbAllocatorArray::appendUnique(const BreadcrumbAllocatorArray& other) noexcept
{
    for (const gp::SharedPtr<BreadcrumbAllocator>& entry: other.m_allocators)
    {
        addUnique(entry);
    }
}

gp::SharedRef<BreadcrumbAllocator> BreadcrumbAllocator::create() noexcept
{
    return gp::makeShared<BreadcrumbAllocator>();
}

BreadcrumbAllocator::BreadcrumbAllocator() noexcept
    : m_arena(&command::CommandListBase::getSharedPagePool(), nullptr, true, "RHI.Breadcrumbs")
{}

BreadcrumbAllocator::~BreadcrumbAllocator() noexcept
{
    // Drain deep parent-allocator graphs iteratively when strong refcount == 1 so arbitrarily deep parallel
    // command-list chains never overflow the call stack during recursive SharedPtr destruction.
    if (!m_parents.isEmpty())
    {
        gp::Vector<gp::SharedPtr<BreadcrumbAllocator>> workStack{};
        for (const gp::SharedPtr<BreadcrumbAllocator>& parent: m_parents)
        {
            if (parent.isValid())
            {
                workStack.emplaceBack(parent);
            }
        }
        m_parents.clear();

        while (!workStack.isEmpty())
        {
            gp::SharedPtr<BreadcrumbAllocator> current = std::move(workStack[workStack.size() - 1u]);
            workStack.popBack();

            if (current.isValid() && current.getSharedReferenceCount() == 1u && !current->m_parents.isEmpty())
            {
                for (const gp::SharedPtr<BreadcrumbAllocator>& nested: current->m_parents)
                {
                    if (nested.isValid())
                    {
                        workStack.emplaceBack(nested);
                    }
                }
                current->m_parents.clear();
            }
        }
    }

    m_arena.reset();
}

void* BreadcrumbAllocator::alloc(const gp::USize sizeInBytes, const gp::UInt32 alignment) noexcept
{
    if (sizeInBytes == 0u)
    {
        return nullptr;
    }
    return m_arena.allocate(sizeInBytes, alignment);
}

const char* BreadcrumbAllocator::allocString(const gp::StringView text) noexcept
{
    if (text.isEmpty())
    {
        return "";
    }

    const gp::USize len = text.size();
    char* const dst = static_cast<char*>(alloc(len + 1u, 1u));
    GP_CHECKF(dst != nullptr, "BreadcrumbAllocator::allocString failed to allocate arena bytes.");
    gp::memory::copyMemory(dst, text.data(), len);
    dst[len] = '\0';
    return dst;
}

void BreadcrumbAllocator::addParentReference(BreadcrumbAllocator* const other) noexcept
{
    if (other == nullptr || other == this)
    {
        return;
    }
    m_parents.addUnique(other);
}

BreadcrumbNode* BreadcrumbAllocator::allocBreadcrumb(const BreadcrumbData& inData, const bool copyStaticName) noexcept
{
    BreadcrumbData storedData = inData;
    if (copyStaticName && inData.staticName != nullptr)
    {
        storedData.staticName = allocString(gp::StringView(inData.staticName));
    }

    void* const mem =
        alloc(sizeof(detail::LiteralBreadcrumbNode), static_cast<gp::UInt32>(alignof(detail::LiteralBreadcrumbNode)));
    detail::LiteralBreadcrumbNode* const node = ::new (mem) detail::LiteralBreadcrumbNode(storedData, nullptr);
    node->m_allocator = this;
    return node;
}

BreadcrumbNodeRef::BreadcrumbNodeRef(BreadcrumbNode* const node) noexcept
    : m_node(node)
{
    if (node != nullptr && node != BreadcrumbNode::getSentinel())
    {
        BreadcrumbAllocator* const alloc = node->getAllocator();
        if (alloc != nullptr && alloc->doesSharedInstanceExist())
        {
            m_allocatorRef = gp::SharedPtr<BreadcrumbAllocator>(alloc->asShared());
        }
    }
}

BreadcrumbAllocator& BreadcrumbScope::acquireCommandListAllocator(command::CommandListBase& cmdList) noexcept
{
#if GP_WITH_RHI_BREADCRUMBS
    return cmdList.getBreadcrumbAllocator();
#else
    static_cast<void>(cmdList);
    GP_UNREACHABLE();
#endif
}

BreadcrumbScope::BreadcrumbScope(
    command::CommandListBase& cmdList, const BreadcrumbData& inData, const bool condition
) noexcept
    : m_cmdList(cmdList)
{
#if GP_WITH_RHI_BREADCRUMBS
    if (condition)
    {
        BreadcrumbNode* const allocatedNode = cmdList.getBreadcrumbAllocator().allocBreadcrumb(inData, false);
        enterNode(allocatedNode);
    }
#else
    static_cast<void>(inData);
    static_cast<void>(condition);
#endif
}

BreadcrumbScope::BreadcrumbScope(command::CommandListBase& cmdList, BreadcrumbNode* const node) noexcept
    : m_cmdList(cmdList)
{
#if GP_WITH_RHI_BREADCRUMBS
    if (node != nullptr && node != BreadcrumbNode::getSentinel())
    {
        enterNode(node);
    }
#else
    static_cast<void>(node);
#endif
}

void BreadcrumbScope::enterNode(BreadcrumbNode* const node) noexcept
{
#if GP_WITH_RHI_BREADCRUMBS
    m_node = node;
    m_pipeline = m_cmdList.getPipeline();
    m_cmdList.beginBreadcrumbCPU(m_node, true);
    if (m_pipeline != sync::PipelineStage::None)
    {
        m_cmdList.beginBreadcrumbGPU(m_node, m_pipeline);
    }
#else
    static_cast<void>(node);
#endif
}

BreadcrumbScope::~BreadcrumbScope() noexcept
{
#if GP_WITH_RHI_BREADCRUMBS
    if (m_node != nullptr)
    {
        if (m_pipeline != sync::PipelineStage::None)
        {
            m_cmdList.endBreadcrumbGPU(m_node, m_pipeline);
        }
        m_cmdList.endBreadcrumbCPU(m_node, true);
    }
#endif
}

BreadcrumbAllocator& BreadcrumbEventManual::acquireAllocator(command::CommandListBase& cmdList) noexcept
{
#if GP_WITH_RHI_BREADCRUMBS
    return cmdList.getBreadcrumbAllocator();
#else
    static_cast<void>(cmdList);
    GP_UNREACHABLE();
#endif
}

BreadcrumbEventManual::BreadcrumbEventManual(
    command::CommandListBase& cmdList, const BreadcrumbData& inData, const bool copyStaticName
) noexcept
{
#if GP_WITH_RHI_BREADCRUMBS
    BreadcrumbNode* const node = cmdList.getBreadcrumbAllocator().allocBreadcrumb(inData, copyStaticName);
    beginInternal(cmdList, node);
#else
    static_cast<void>(cmdList);
    static_cast<void>(inData);
    static_cast<void>(copyStaticName);
#endif
}

void BreadcrumbEventManual::beginInternal(command::CommandListBase& cmdList, BreadcrumbNode* const node) noexcept
{
#if GP_WITH_RHI_BREADCRUMBS
    m_node = node;
    m_pipeline = cmdList.getPipeline();
    #if GP_DO_CHECK
    if (node != nullptr && node->getAllocator() != nullptr && node->getAllocator()->doesSharedInstanceExist())
    {
        m_debugAllocatorRef = gp::SharedPtr<BreadcrumbAllocator>(node->getAllocator()->asShared());
    }
    #endif
    if (m_node != nullptr)
    {
        cmdList.beginBreadcrumbCPU(m_node, true);
        if (m_pipeline != sync::PipelineStage::None)
        {
            cmdList.beginBreadcrumbGPU(m_node, m_pipeline);
        }
    }
#else
    static_cast<void>(cmdList);
    static_cast<void>(node);
#endif
}

BreadcrumbEventManual::BreadcrumbEventManual(BreadcrumbEventManual&& other) noexcept
    : m_node(other.m_node)
    , m_pipeline(other.m_pipeline)
#if GP_DO_CHECK
    , m_debugAllocatorRef(std::move(other.m_debugAllocatorRef))
#endif
{
    other.m_node = nullptr;
    other.m_pipeline = sync::PipelineStage::None;
}

BreadcrumbEventManual& BreadcrumbEventManual::operator=(BreadcrumbEventManual&& other) noexcept
{
    if (this != &other)
    {
        GP_CHECKF(m_node == nullptr, "BreadcrumbEventManual overwritten before calling end().");
        m_node = other.m_node;
        m_pipeline = other.m_pipeline;
#if GP_DO_CHECK
        m_debugAllocatorRef = std::move(other.m_debugAllocatorRef);
#endif
        other.m_node = nullptr;
        other.m_pipeline = sync::PipelineStage::None;
    }
    return *this;
}

BreadcrumbEventManual::~BreadcrumbEventManual() noexcept
{
    GP_CHECKF(m_node == nullptr, "BreadcrumbEventManual was destroyed without calling end().");
}

void BreadcrumbEventManual::end(command::CommandListBase& cmdList) noexcept
{
#if GP_WITH_RHI_BREADCRUMBS
    if (m_node == nullptr)
    {
        return;
    }

    if (m_pipeline != sync::PipelineStage::None)
    {
        cmdList.endBreadcrumbGPU(m_node, m_pipeline);
    }
    cmdList.endBreadcrumbCPU(m_node, true);
    m_node = nullptr;
    m_pipeline = sync::PipelineStage::None;
    #if GP_DO_CHECK
    m_debugAllocatorRef.reset();
    #endif
#else
    static_cast<void>(cmdList);
#endif
}

void BreadcrumbState::reset() noexcept
{
    m_entries.clear();
}

void BreadcrumbState::setNodeState(
    const BreadcrumbQueueId queue, const BreadcrumbNode* const node, const BreadcrumbMarkerState state
) noexcept
{
    if (node == nullptr || node == BreadcrumbNode::getSentinel())
    {
        return;
    }

    for (RecordedNodeState& entry: m_entries)
    {
        if (entry.queue == queue && entry.node == node)
        {
            if (state == BreadcrumbMarkerState::Active || entry.state != BreadcrumbMarkerState::Active)
            {
                entry.state = state;
            }
            return;
        }
    }

    m_entries.emplaceBack(RecordedNodeState{ queue, node, state });

    // Ensure every ancestor of a recorded node exists in the queue's tree; ancestors of Active nodes are Active.
    const BreadcrumbNode* parent = node->getParent();
    while (parent != nullptr && parent != BreadcrumbNode::getSentinel())
    {
        const BreadcrumbMarkerState parentState =
            (state == BreadcrumbMarkerState::Active) ? BreadcrumbMarkerState::Active : state;

        bool foundParent = false;
        for (RecordedNodeState& entry: m_entries)
        {
            if (entry.queue == queue && entry.node == parent)
            {
                if (parentState == BreadcrumbMarkerState::Active)
                {
                    entry.state = BreadcrumbMarkerState::Active;
                }
                else if (
                    entry.state == BreadcrumbMarkerState::NotStarted && parentState == BreadcrumbMarkerState::Finished
                )
                {
                    entry.state = BreadcrumbMarkerState::Finished;
                }
                foundParent = true;
                break;
            }
        }

        if (!foundParent)
        {
            m_entries.emplaceBack(RecordedNodeState{ queue, parent, parentState });
        }

        parent = parent->getParent();
    }
}

void BreadcrumbState::linkRange(
    const BreadcrumbQueueId queue, const BreadcrumbRange& range, const gp::UInt32 markerIn, const gp::UInt32 markerOut
) noexcept
{
    if (!range)
    {
        return;
    }

    gp::Vector<const BreadcrumbNode*> rangeNodes{};
    const BreadcrumbNode* markerInNode = nullptr;
    const BreadcrumbNode* markerOutNode = nullptr;

    range.enumerate(
        [&rangeNodes, &markerInNode, &markerOutNode, markerIn, markerOut](BreadcrumbNode* const node) noexcept -> void
    {
        if (node == nullptr || node == BreadcrumbNode::getSentinel())
        {
            return;
        }
        rangeNodes.emplaceBack(node);
        if (node->id == markerIn)
        {
            markerInNode = node;
        }
        if (node->id == markerOut)
        {
            markerOutNode = node;
        }
    }
    );

    bool seenMarkerIn = false;
    bool seenMarkerOut = false;

    for (gp::USize idx = 0u; idx < rangeNodes.size(); ++idx)
    {
        const BreadcrumbNode* const node = rangeNodes[idx];

        BreadcrumbMarkerState state = BreadcrumbMarkerState::NotStarted;
        if (markerIn == 0u)
        {
            state = BreadcrumbMarkerState::NotStarted;
        }
        else if (markerInNode != nullptr)
        {
            if (!seenMarkerIn)
            {
                if (node == markerInNode)
                {
                    seenMarkerIn = true;
                    state = (markerOutNode == markerInNode) ? BreadcrumbMarkerState::Finished
                                                            : BreadcrumbMarkerState::Active;
                }
                else if (isAncestorOf(node, markerInNode) && markerOutNode != markerInNode)
                {
                    state = BreadcrumbMarkerState::Active;
                }
                else if (markerOutNode != nullptr && !seenMarkerOut)
                {
                    state = BreadcrumbMarkerState::Finished;
                    if (node == markerOutNode)
                    {
                        seenMarkerOut = true;
                    }
                }
                else
                {
                    state = isAncestorOf(node, markerInNode) ? BreadcrumbMarkerState::Active
                                                             : BreadcrumbMarkerState::Finished;
                }
            }
            else
            {
                state = BreadcrumbMarkerState::NotStarted;
            }
        }
        else
        {
            // Fallback ID-order comparison when markerIn/markerOut come from monotonic IDs.
            if (node->id > markerIn)
            {
                state = BreadcrumbMarkerState::NotStarted;
            }
            else if (node->id <= markerOut)
            {
                state = BreadcrumbMarkerState::Finished;
            }
            else
            {
                state = BreadcrumbMarkerState::Active;
            }
        }

        setNodeState(queue, node, state);
    }
}

gp::String BreadcrumbState::dumpActiveBreadcrumbs(const bool includeFinishedAndNotStarted) const noexcept
{
    if (m_entries.isEmpty())
    {
        return gp::String("No active GPU breadcrumbs recorded.\n");
    }

    gp::Vector<BreadcrumbQueueId> queues{};
    for (const RecordedNodeState& entry: m_entries)
    {
        bool exists = false;
        for (const BreadcrumbQueueId& q: queues)
        {
            if (q == entry.queue)
            {
                exists = true;
                break;
            }
        }
        if (!exists)
        {
            queues.emplaceBack(entry.queue);
        }
    }

    gp::String output{};
    output.append("Active GPU Breadcrumbs:\n");

    for (const BreadcrumbQueueId& queue: queues)
    {
        output.append("  Device ");
        appendUInt32ToString(output, queue.deviceIndex);
        output.append(", Pipeline ");
        const gp::String pipeName = getRHIPipelineName(queue.pipeline);
        output.append(pipeName.view());
        output.append(":\n");

        // Gather all nodes belonging to this queue sorted roughly top-down (root-first, preserving encounter order).
        gp::Vector<RecordedNodeState> queueNodes{};
        for (const RecordedNodeState& entry: m_entries)
        {
            if (entry.queue == queue)
            {
                if (includeFinishedAndNotStarted || entry.state == BreadcrumbMarkerState::Active)
                {
                    queueNodes.emplaceBack(entry);
                }
            }
        }

        // Recursive-free DFS stack starting from roots in `queueNodes`.
        struct StackItem
        {
            const BreadcrumbNode* node{ nullptr };
            BreadcrumbMarkerState state{ BreadcrumbMarkerState::NotStarted };
            gp::UInt32 depth{ 0u };
        };

        gp::Vector<StackItem> dfsStack{};
        for (gp::USize idx = queueNodes.size(); idx > 0u; --idx)
        {
            const RecordedNodeState& candidate = queueNodes[idx - 1u];
            const BreadcrumbNode* const parent = candidate.node->getParent();
            bool parentInQueue = false;
            if (parent != nullptr && parent != BreadcrumbNode::getSentinel())
            {
                for (const RecordedNodeState& other: queueNodes)
                {
                    if (other.node == parent)
                    {
                        parentInQueue = true;
                        break;
                    }
                }
            }

            if (!parentInQueue)
            {
                dfsStack.emplaceBack(StackItem{ candidate.node, candidate.state, 0u });
            }
        }

        BreadcrumbBuffer scratch{};
        while (!dfsStack.isEmpty())
        {
            const StackItem current = dfsStack[dfsStack.size() - 1u];
            dfsStack.popBack();

            output.append("    ");
            for (gp::UInt32 d = 0u; d < current.depth; ++d)
            {
                output.append("  ");
            }
            output.pushBack('[');
            output.append(markerStateToString(current.state));
            output.append("] ");
            output.append(current.node->getDebugStr(scratch));
            output.append(" (ID: ");
            appendUInt32ToString(output, current.node->id);
            output.append(")\n");

            for (gp::USize idx = queueNodes.size(); idx > 0u; --idx)
            {
                const RecordedNodeState& child = queueNodes[idx - 1u];
                if (child.node->getParent() == current.node)
                {
                    dfsStack.emplaceBack(StackItem{ child.node, child.state, current.depth + 1u });
                }
            }
        }
    }

    return output;
}

void BreadcrumbState::logActiveBreadcrumbs(const bool includeFinishedAndNotStarted) const noexcept
{
    const gp::String report = dumpActiveBreadcrumbs(includeFinishedAndNotStarted);
    GP_LOG_ERROR(::gp::LogRHI, "{}", report.view());
}

}   // namespace gp::rhi::diagnostics
