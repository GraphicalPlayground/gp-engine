// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "commands/CommandForward.hpp"
#include "containers/arrays/Vector.hpp"
#include "containers/strings/Name.hpp"
#include "containers/strings/String.hpp"
#include "containers/views/StringView.hpp"
#include "CoreMinimal.hpp"
#include "diagnostics/BreadcrumbEnums.hpp"
#include "diagnostics/BreadcrumbTypes.hpp"
#include "diagnostics/DiagnosticsForward.hpp"   // IWYU pragma: keep
#include "memory/allocators/PagedLinearAllocator.hpp"
#include "memory/pointers/SharedPtr.hpp"
#include "RHIBase.hpp"
#include "sync/SyncEnums.hpp"
#include <cstdio>
#include <new>   // IWYU pragma: keep
#include <type_traits>
#include <utility>

namespace gp::rhi::diagnostics
{

/// @brief Base node in the RHI CPU/GPU breadcrumb hierarchy.
/// @details Allocated linearly inside a `BreadcrumbAllocator` arena with zero per-node heap allocations. Each node
///          receives a monotonically increasing non-zero `id` (`getSentinel()` uses `id == 0`) so GPU diagnostic
///          buffers and vendor crash dump SDKs can record compact 32-bit marker IDs on hardware queues and resolve
///          them back to full `/` paths on crash.
struct GP_RHI_API BreadcrumbNode
{
    friend class BreadcrumbAllocator;
    friend struct BreadcrumbList;
    friend struct BreadcrumbRange;
    friend class ::gp::rhi::command::CommandListBase;
    friend class ::gp::rhi::command::ComputeCommandList;

public:
    /// @brief Reserved ID used by `getSentinel()`.
    static constexpr gp::UInt32 kSentinelId = 0u;

    /// @brief Sentinel ID representing an invalid or unset marker slot.
    static constexpr gp::UInt32 kInvalidId = 0xFFFFFFFFu;

public:
    /// @brief Static call-site and label metadata for this breadcrumb node.
    BreadcrumbData data{};

    /// @brief Unique non-zero monotonic identifier assigned on construction (`0` only for `getSentinel()`).
    const gp::UInt32 id{ kSentinelId };

    /// @brief Bitmask of `sync::PipelineStage` queues on which `beginBreadcrumbGPU` has been emitted for this node.
    sync::PipelineStage begunPipelines{ sync::PipelineStage::None };

    /// @brief Bitmask of `sync::PipelineStage` queues on which `endBreadcrumbGPU` has been emitted for this node.
    sync::PipelineStage endedPipelines{ sync::PipelineStage::None };

public:
    BreadcrumbNode(const BreadcrumbNode&) = delete;
    BreadcrumbNode& operator=(const BreadcrumbNode&) = delete;
    BreadcrumbNode(BreadcrumbNode&&) = delete;
    BreadcrumbNode& operator=(BreadcrumbNode&&) = delete;

    /// @brief Protected/trivial virtual destructor (nodes live in `BreadcrumbAllocator` linear pages).
    virtual ~BreadcrumbNode() noexcept = default;

protected:
    /// @brief Constructs a breadcrumb node with `inData` and assigns the next unique monotonic ID.
    /// @param[in] inData Static metadata copied into this node.
    explicit BreadcrumbNode(const BreadcrumbData& inData) noexcept;

    /// @brief Special sentinel constructor assigning `id = kSentinelId`.
    struct SentinelTag
    {
        explicit constexpr SentinelTag() noexcept = default;
    };

    /// @brief Constructs the singleton sentinel node.
    /// @param[in] tag Sentinel construction tag.
    explicit BreadcrumbNode(SentinelTag tag) noexcept;

public:
    /// @brief Formats or returns the human-readable label for this node.
    /// @param[out] buffer Scratch stack buffer used if dynamic formatting is required.
    /// @return Null-terminated string valid at least as long as `buffer` and `*this`.
    [[nodiscard]] virtual const char* getDebugStr(BreadcrumbBuffer& buffer) const noexcept
    {
        static_cast<void>(buffer);
        return (data.staticName != nullptr) ? data.staticName : "<unnamed>";
    }

    /// @brief Returns the parent breadcrumb node (`nullptr` at the root, or `getSentinel()` for unattached subtrees).
    /// @return Parent `BreadcrumbNode*`.
    [[nodiscard]] GP_FORCEINLINE_HINT BreadcrumbNode* getParent() const noexcept
    {
        return m_parent;
    }

    /// @brief Assigns `parent` on a root or `getSentinel()`-parented subtree node.
    /// @param[in] parent New parent breadcrumb node.
    void setParent(BreadcrumbNode* parent) noexcept;

    /// @brief Returns the next node in the flat execution/submission list (`BreadcrumbRange` / `BreadcrumbList`).
    /// @return Next `BreadcrumbNode*`, or `nullptr`.
    [[nodiscard]] GP_FORCEINLINE_HINT BreadcrumbNode* getNextPtr() const noexcept
    {
        return m_nextPtr;
    }

    /// @brief Returns the owning `BreadcrumbAllocator` that allocated this node (`nullptr` for `getSentinel()`).
    /// @return Non-owning `BreadcrumbAllocator*`.
    [[nodiscard]] GP_FORCEINLINE_HINT BreadcrumbAllocator* getAllocator() const noexcept
    {
        return m_allocator;
    }

    /// @brief Builds the full `/`-separated path from the outermost root down to this node.
    /// @return Formatted path string (e.g., `"Frame 1/ShadowDepths/Cascade0"`).
    [[nodiscard]] gp::String getFullPath() const noexcept;

public:
    /// @brief Returns the global sentinel node representing an unattached parallel command-list breadcrumb root.
    /// @return Non-null pointer to the global sentinel `BreadcrumbNode`.
    [[nodiscard]] static BreadcrumbNode* getSentinel() noexcept;

    /// @brief Computes the 1-based depth of `node` (`0` when `node == nullptr` or `node == getSentinel()`).
    /// @param[in] node Breadcrumb node to inspect.
    /// @return Nesting depth (`>= 1` for valid nodes, `0` for null/sentinel).
    [[nodiscard]] static gp::UInt32 getLevel(const BreadcrumbNode* node) noexcept;

    /// @brief Walks parent pointers to return the highest non-null, non-sentinel ancestor of `node`.
    /// @param[in] node Starting node (returns `nullptr` if `node` is null or sentinel).
    /// @return Outermost non-null, non-sentinel root node.
    [[nodiscard]] static BreadcrumbNode* getNonNullRoot(BreadcrumbNode* node) noexcept;

    /// @brief Const overload of `getNonNullRoot()`.
    /// @param[in] node Starting node.
    /// @return Outermost non-null, non-sentinel root node.
    [[nodiscard]] static const BreadcrumbNode* getNonNullRoot(const BreadcrumbNode* node) noexcept;

    /// @brief Nullptr overload of `getNonNullRoot()` preventing pointer overload ambiguity at literal `nullptr` call
    /// sites.
    /// @return Always `nullptr`.
    [[nodiscard]] static constexpr gp::NullPtrT getNonNullRoot(gp::NullPtrT) noexcept
    {
        return nullptr;
    }

    /// @brief Finds the lowest common ancestor shared by `node0` and `node1`.
    /// @param[in] node0 First breadcrumb node.
    /// @param[in] node1 Second breadcrumb node.
    /// @return Lowest common ancestor `BreadcrumbNode*`, or `nullptr` if disjoint or rooted at `nullptr`.
    [[nodiscard]] static const BreadcrumbNode*
        findCommonAncestor(const BreadcrumbNode* node0, const BreadcrumbNode* node1) noexcept;

private:
    BreadcrumbNode* m_parent{ nullptr };
    BreadcrumbNode* m_nextPtr{ nullptr };
    BreadcrumbAllocator* m_allocator{ nullptr };
};

/// @brief Singly-linked list builder linking `BreadcrumbNode` instances through `m_nextPtr`.
struct GP_RHI_API BreadcrumbList
{
    /// @brief First node in the linked list, or `nullptr` when empty.
    BreadcrumbNode* first{ nullptr };

    /// @brief Last node in the linked list, or `nullptr` when empty.
    BreadcrumbNode* last{ nullptr };

    /// @brief Appends `node` to the tail of this list (asserts `node != nullptr`, `node != Sentinel`, and unlinked).
    /// @param[in,out] node Breadcrumb node to append.
    void append(BreadcrumbNode* node) noexcept;

    /// @brief Returns the `[first, last]` span as a `BreadcrumbRange`.
    /// @return `BreadcrumbRange` covering this list.
    [[nodiscard]] BreadcrumbRange getRange() const noexcept;

    /// @brief Walks every node in the list, unlinking `m_nextPtr` before invoking `visitor(node)`, then clears `*this`.
    /// @tparam InVisitor Callable invocable with `(BreadcrumbNode*)`.
    /// @param[in,out] visitor Callback invoked once per node in FIFO order.
    template <typename InVisitor>
    void iterateAndUnlink(InVisitor&& visitor) noexcept
    {
        BreadcrumbNode* current = first;
        first = nullptr;
        last = nullptr;
        while (current != nullptr)
        {
            BreadcrumbNode* const next = current->m_nextPtr;
            current->m_nextPtr = nullptr;
            visitor(current);
            current = next;
        }
    }
};

/// @brief Inclusive `[first, last]` sequence of breadcrumb nodes linked through `BreadcrumbNode::m_nextPtr`.
struct GP_RHI_API BreadcrumbRange
{
    /// @brief First node in the range, or `nullptr` when empty.
    BreadcrumbNode* first{ nullptr };

    /// @brief Last node in the range, or `nullptr` when empty.
    BreadcrumbNode* last{ nullptr };

    /// @brief Constructs an empty breadcrumb range.
    constexpr BreadcrumbRange() noexcept = default;

    /// @brief Constructs an inclusive range `[inFirst, inLast]`.
    /// @param[in] inFirst First node in the range.
    /// @param[in] inLast Last node in the range.
    constexpr BreadcrumbRange(BreadcrumbNode* const inFirst, BreadcrumbNode* const inLast) noexcept
        : first(inFirst)
        , last(inLast)
    {}

    /// @brief Reports whether this range is non-empty (`first != nullptr`).
    /// @return True when non-empty.
    [[nodiscard]] constexpr explicit operator bool() const noexcept
    {
        return first != nullptr;
    }

    /// @brief Default equality comparison.
    /// @param[in] rhs Range to compare against.
    /// @return True when `first` and `last` match.
    [[nodiscard]] constexpr bool operator==(const BreadcrumbRange& rhs) const noexcept = default;

    /// @brief Splices non-empty `other` into this range immediately after `prev` (or at the head when `prev ==
    ///        nullptr`).
    /// @param[in] other Sub-range to splice in (no-op if empty).
    /// @param[in] prev Node in `*this` after which `other` is inserted, or `nullptr` to prepend at head.
    void insertAfter(const BreadcrumbRange& other, BreadcrumbNode* prev) noexcept;

    /// @brief Visits every node in `[first, last]` inclusive via `getNextPtr()`.
    /// @tparam InVisitor Callable invocable with `(BreadcrumbNode*)`.
    /// @param[in,out] visitor Callback invoked for each node in the range.
    template <typename InVisitor>
    void enumerate(InVisitor&& visitor) const noexcept
    {
        if (first == nullptr)
        {
            return;
        }

        BreadcrumbNode* current = first;
        while (current != nullptr)
        {
            BreadcrumbNode* const next = (current == last) ? nullptr : current->getNextPtr();
            visitor(current);
            current = next;
        }
    }
};

/// @brief Deduplicated list of `BreadcrumbAllocator` shared pointers retained by platform command lists and profiler
///        streams.
class GP_RHI_API BreadcrumbAllocatorArray
{
public:
    /// @brief Constructs an empty allocator reference array.
    BreadcrumbAllocatorArray() noexcept = default;

    /// @brief Appends `allocator` if non-null and not already present in this array.
    /// @param[in] allocator Shared pointer to a `BreadcrumbAllocator`.
    void addUnique(const gp::SharedPtr<BreadcrumbAllocator>& allocator) noexcept;

    /// @brief Appends `allocator` if non-null and not already present in this array.
    /// @param[in] allocator Raw `BreadcrumbAllocator*` backed by `SharedFromThis`.
    void addUnique(BreadcrumbAllocator* allocator) noexcept;

    /// @brief Merges all unique entries from `other` into this array.
    /// @param[in] other Source allocator array.
    void appendUnique(const BreadcrumbAllocatorArray& other) noexcept;

    /// @brief Clears all retained allocator references.
    void clear() noexcept
    {
        m_allocators.clear();
    }

    /// @brief Reports whether the array holds zero allocator references.
    /// @return True when empty.
    [[nodiscard]] bool isEmpty() const noexcept
    {
        return m_allocators.isEmpty();
    }

    /// @brief Returns the number of unique allocators retained.
    /// @return Element count.
    [[nodiscard]] gp::USize size() const noexcept
    {
        return m_allocators.size();
    }

    /// @brief Indexing operator returning retained allocator `index`.
    /// @param[in] index Zero-based index.
    /// @return Const reference to `gp::SharedPtr<BreadcrumbAllocator>`.
    [[nodiscard]] const gp::SharedPtr<BreadcrumbAllocator>& operator[](const gp::USize index) const noexcept
    {
        return m_allocators[index];
    }

    /// @brief Range begin iterator.
    /// @return Begin pointer.
    [[nodiscard]] const gp::SharedPtr<BreadcrumbAllocator>* begin() const noexcept
    {
        return m_allocators.begin();
    }

    /// @brief Range end iterator.
    /// @return End pointer.
    [[nodiscard]] const gp::SharedPtr<BreadcrumbAllocator>* end() const noexcept
    {
        return m_allocators.end();
    }

private:
    gp::Vector<gp::SharedPtr<BreadcrumbAllocator>> m_allocators{};
};

namespace detail
{

/// @brief Breadcrumb node holding an arena-copied static or pre-formatted label string.
struct LiteralBreadcrumbNode final : public BreadcrumbNode
{
    /// @brief Arena-copied formatted message, or `nullptr` to fall back to `data.staticName`.
    const char* formattedText{ nullptr };

    /// @brief Constructs a literal or pre-formatted breadcrumb node.
    /// @param[in] inData Call-site metadata.
    /// @param[in] inFormattedText Optional arena-owned formatted label.
    LiteralBreadcrumbNode(const BreadcrumbData& inData, const char* const inFormattedText) noexcept
        : BreadcrumbNode(inData)
        , formattedText(inFormattedText)
    {}

    /// @copydoc BreadcrumbNode::getDebugStr()
    [[nodiscard]] virtual const char* getDebugStr(BreadcrumbBuffer& buffer) const noexcept override
    {
        static_cast<void>(buffer);
        if (formattedText != nullptr)
        {
            return formattedText;
        }
        return (data.staticName != nullptr) ? data.staticName : "<unnamed>";
    }
};

/// @brief Normalizes printf arguments so `gp::String`, `gp::StringView`, and `gp::Name` convert cleanly to C strings.
[[nodiscard]] GP_FORCEINLINE_HINT inline const char* normalizePrintfArg(const gp::String& str) noexcept
{
    return str.cString();
}

/// @brief Normalizes `gp::Name` to its interned null-terminated base C string.
[[nodiscard]] GP_FORCEINLINE_HINT inline const char* normalizePrintfArg(const gp::Name name) noexcept
{
    return name.baseCString();
}

/// @brief Forwards scalar and pointer printf arguments unchanged.
template <typename InArg>
requires(
    !std::is_same_v<std::decay_t<InArg>, gp::String> && !std::is_same_v<std::decay_t<InArg>, gp::Name> &&
    !std::is_same_v<std::decay_t<InArg>, gp::StringView>
)
[[nodiscard]] GP_FORCEINLINE_HINT constexpr InArg normalizePrintfArg(InArg value) noexcept
{
    return value;
}

}   // namespace detail

/// @brief Linear page-pooled allocator owning `BreadcrumbNode` objects and keeping parent allocators alive.
/// @details Backed by `CommandListBase::getSharedPagePool()` so allocating hundreds of breadcrumbs per frame incurs
///          zero OS/heap allocations once pages warm up. Inherits `gp::SharedFromThis<BreadcrumbAllocator>` so command
///          lists, `IPlatformCommandList` submissions, and `GPUProfiler` event streams retain strong references until
///          GPU execution and crash/profile consumers have retired.
class GP_RHI_API BreadcrumbAllocator final : public gp::SharedFromThis<BreadcrumbAllocator>
{
public:
    /// @brief Creates a new shared `BreadcrumbAllocator` instance backed by the RHI command-list `PagePool`.
    /// @return Non-nullable `gp::SharedRef<BreadcrumbAllocator>`.
    [[nodiscard]] static gp::SharedRef<BreadcrumbAllocator> create() noexcept;

    /// @brief Constructs a breadcrumb allocator (use `BreadcrumbAllocator::create()` so `SharedFromThis` is bound).
    BreadcrumbAllocator() noexcept;

    BreadcrumbAllocator(const BreadcrumbAllocator&) = delete;
    BreadcrumbAllocator& operator=(const BreadcrumbAllocator&) = delete;
    BreadcrumbAllocator(BreadcrumbAllocator&&) = delete;
    BreadcrumbAllocator& operator=(BreadcrumbAllocator&&) = delete;

    /// @brief Iteratively releases chained parent allocator references without risking stack overflow on deep trees.
    ~BreadcrumbAllocator() noexcept;

public:
    /// @brief Allocates raw aligned bytes from this allocator's linear arena.
    /// @param[in] sizeInBytes Number of bytes to allocate.
    /// @param[in] alignment Power-of-two byte alignment.
    /// @return Pointer to arena memory valid for the lifetime of this allocator.
    [[nodiscard]] void* alloc(gp::USize sizeInBytes, gp::UInt32 alignment) noexcept;

    /// @brief Copies a string view into a null-terminated buffer inside this allocator's arena.
    /// @param[in] text Source text to copy.
    /// @return Non-null arena-owned C string.
    [[nodiscard]] const char* allocString(gp::StringView text) noexcept;

    /// @brief Retains a strong reference to `other` so parent/child breadcrumb pointers across allocators never dangle.
    /// @param[in] other Referenced allocator (ignored if null or `this`).
    void addParentReference(BreadcrumbAllocator* other) noexcept;

    /// @brief Returns the parent allocator references held by this allocator.
    /// @return Const reference to `BreadcrumbAllocatorArray`.
    [[nodiscard]] const BreadcrumbAllocatorArray& getParents() const noexcept
    {
        return m_parents;
    }

    /// @brief Returns the total bytes currently used in this allocator's linear arena.
    /// @return Used bytes.
    [[nodiscard]] gp::USize getUsedBytes() const noexcept
    {
        return m_arena.getUsedBytes();
    }

    /// @brief Allocates a static-name `BreadcrumbNode` in this allocator's arena.
    /// @param[in] inData Static breadcrumb metadata (`staticName` copied if `copyStaticName` is true).
    /// @param[in] copyStaticName True to copy `inData.staticName` into the arena (for runtime strings).
    /// @return Newly allocated `BreadcrumbNode*`.
    [[nodiscard]] BreadcrumbNode* allocBreadcrumb(const BreadcrumbData& inData, bool copyStaticName = false) noexcept;

    /// @brief Allocates a formatted `BreadcrumbNode` in this allocator's arena, eagerly formatting `args` into arena
    ///        memory so temporary stack strings never dangle when inspected asynchronously on crash or in
    ///        `GPUProfiler`.
    /// @tparam InArgs Printf-compatible argument types (`gp::String` and `gp::Name` are automatically unwrapped).
    /// @param[in] inData Breadcrumb metadata whose `dynamicFormat` specifies the printf pattern.
    /// @param[in] args Values formatted into `inData.dynamicFormat`.
    /// @return Newly allocated `BreadcrumbNode*`.
    template <typename... InArgs>
    [[nodiscard]] BreadcrumbNode* allocFormattedBreadcrumb(const BreadcrumbData& inData, InArgs&&... args) noexcept
    {
        const char* formattedCopy = nullptr;
        if (inData.dynamicFormat != nullptr)
        {
            BreadcrumbBuffer scratch{};
            if constexpr (sizeof...(InArgs) == 0u)
            {
                formattedCopy = allocString(gp::StringView(inData.dynamicFormat));
            }
            else
            {
                const int written = std::snprintf(
                    scratch.data,
                    BreadcrumbBuffer::kCapacity,
                    inData.dynamicFormat,
                    detail::normalizePrintfArg(std::forward<InArgs>(args))...
                );
                if (written > 0)
                {
                    const gp::USize clampedLen = (static_cast<gp::USize>(written) < BreadcrumbBuffer::kCapacity)
                                                     ? static_cast<gp::USize>(written)
                                                     : (BreadcrumbBuffer::kCapacity - 1u);
                    formattedCopy = allocString(gp::StringView(scratch.data, clampedLen));
                }
                else
                {
                    formattedCopy = "";
                }
            }
        }

        void* const mem = alloc(
            sizeof(detail::LiteralBreadcrumbNode), static_cast<gp::UInt32>(alignof(detail::LiteralBreadcrumbNode))
        );
        detail::LiteralBreadcrumbNode* const node = ::new (mem) detail::LiteralBreadcrumbNode(inData, formattedCopy);
        node->m_allocator = this;
        return node;
    }

private:
    gp::memory::PagedLinearAllocator m_arena;
    BreadcrumbAllocatorArray m_parents{};
};

/// @brief Smart reference pairing a `BreadcrumbNode*` with a strong `SharedPtr<BreadcrumbAllocator>` reference.
/// @details Used when exporting the current CPU breadcrumb from an outer command list into parallel worker tasks so the
///          node's backing arena stays alive until all child command lists finish attaching or executing.
class GP_RHI_API BreadcrumbNodeRef final
{
public:
    /// @brief Constructs an empty null breadcrumb reference.
    BreadcrumbNodeRef() noexcept = default;

    /// @brief Constructs a reference to `node`, automatically retaining `node->getAllocator()` if present.
    /// @param[in] node Target breadcrumb node (may be `nullptr` or `BreadcrumbNode::getSentinel()`).
    explicit(false) BreadcrumbNodeRef(BreadcrumbNode* node) noexcept;

    /// @brief Constructs a reference from an explicit `node` and `allocator`.
    /// @param[in] node Target breadcrumb node.
    /// @param[in] allocator Owning allocator shared pointer.
    BreadcrumbNodeRef(BreadcrumbNode* node, gp::SharedPtr<BreadcrumbAllocator> allocator) noexcept
        : m_node(node)
        , m_allocatorRef(std::move(allocator))
    {}

    /// @brief Returns the referenced `BreadcrumbNode*`.
    /// @return Raw node pointer.
    [[nodiscard]] GP_FORCEINLINE_HINT BreadcrumbNode* get() const noexcept
    {
        return m_node;
    }

    /// @brief Implicit conversion to `BreadcrumbNode*`.
    /// @return Raw node pointer.
    [[nodiscard]] GP_FORCEINLINE_HINT explicit(false) operator BreadcrumbNode*() const noexcept
    {
        return m_node;
    }

    /// @brief Dereferences the underlying `BreadcrumbNode`.
    /// @return Pointer to `BreadcrumbNode`.
    [[nodiscard]] GP_FORCEINLINE_HINT BreadcrumbNode* operator->() const noexcept
    {
        return m_node;
    }

    /// @brief Reports whether this handle points to a non-null `BreadcrumbNode`.
    /// @return True when `m_node != nullptr`.
    [[nodiscard]] GP_FORCEINLINE_HINT explicit operator bool() const noexcept
    {
        return m_node != nullptr;
    }

    /// @brief Returns the retained `BreadcrumbAllocator` shared pointer.
    /// @return Const reference to `gp::SharedPtr<BreadcrumbAllocator>`.
    [[nodiscard]] GP_FORCEINLINE_HINT const gp::SharedPtr<BreadcrumbAllocator>& getAllocatorRef() const noexcept
    {
        return m_allocatorRef;
    }

private:
    BreadcrumbNode* m_node{ nullptr };
    gp::SharedPtr<BreadcrumbAllocator> m_allocatorRef{};
};

/// @brief RAII scope pushing a CPU + GPU breadcrumb onto a `CommandListBase` and popping it on scope exit.
class GP_RHI_API BreadcrumbScope final
{
public:
    /// @brief Enters a static-name breadcrumb scope on `cmdList` when `condition` is true.
    /// @param[in,out] cmdList Target command list.
    /// @param[in] inData Static breadcrumb metadata.
    /// @param[in] condition Runtime condition enabling or skipping this scope.
    BreadcrumbScope(command::CommandListBase& cmdList, const BreadcrumbData& inData, bool condition = true) noexcept;

    /// @brief Enters an already-allocated `node` on `cmdList`.
    /// @param[in,out] cmdList Target command list.
    /// @param[in] node Pre-allocated breadcrumb node (no-op if `nullptr`).
    BreadcrumbScope(command::CommandListBase& cmdList, BreadcrumbNode* node) noexcept;

    /// @brief Enters a printf-formatted breadcrumb scope on `cmdList` when `condition` is true.
    /// @tparam InArgs Printf argument types.
    /// @param[in,out] cmdList Target command list.
    /// @param[in] inData Breadcrumb metadata containing `dynamicFormat`.
    /// @param[in] condition Runtime enable condition.
    /// @param[in] args Printf arguments formatted into `inData.dynamicFormat`.
    template <typename... InArgs>
    BreadcrumbScope(
        command::CommandListBase& cmdList, const BreadcrumbData& inData, const bool condition, InArgs&&... args
    ) noexcept
        : m_cmdList(cmdList)
    {
#if GP_WITH_RHI_BREADCRUMBS
        if (condition)
        {
            BreadcrumbAllocator& allocator = acquireCommandListAllocator(cmdList);
            BreadcrumbNode* const allocatedNode =
                allocator.allocFormattedBreadcrumb(inData, std::forward<InArgs>(args)...);
            enterNode(allocatedNode);
        }
#else
        static_cast<void>(inData);
        static_cast<void>(condition);
        (static_cast<void>(args), ...);
#endif
    }

    BreadcrumbScope(const BreadcrumbScope&) = delete;
    BreadcrumbScope& operator=(const BreadcrumbScope&) = delete;
    BreadcrumbScope(BreadcrumbScope&&) = delete;
    BreadcrumbScope& operator=(BreadcrumbScope&&) = delete;

    /// @brief Pops the breadcrumb scope from `m_cmdList` if active.
    ~BreadcrumbScope() noexcept;

    /// @brief Returns the `BreadcrumbNode*` managed by this scope (`nullptr` if disabled or skipped).
    /// @return Active `BreadcrumbNode*`.
    [[nodiscard]] GP_FORCEINLINE_HINT BreadcrumbNode* getNode() const noexcept
    {
        return m_node;
    }

private:
    [[nodiscard]] static BreadcrumbAllocator& acquireCommandListAllocator(command::CommandListBase& cmdList) noexcept;
    void enterNode(BreadcrumbNode* node) noexcept;

private:
    command::CommandListBase& m_cmdList;
    BreadcrumbNode* m_node{ nullptr };
    sync::PipelineStage m_pipeline{ sync::PipelineStage::None };
};

/// @brief Manually ended breadcrumb scope for non-lexical spans (such as `beginRenderPass` .. `endRenderPass`).
class GP_RHI_API BreadcrumbEventManual final
{
public:
    /// @brief Constructs an inactive manual breadcrumb handle.
    BreadcrumbEventManual() noexcept = default;

    /// @brief Starts a static-name manual breadcrumb event on `cmdList`.
    /// @param[in,out] cmdList Target command list.
    /// @param[in] inData Static breadcrumb metadata.
    /// @param[in] copyStaticName True to copy `inData.staticName` into the command list's breadcrumb allocator.
    BreadcrumbEventManual(
        command::CommandListBase& cmdList, const BreadcrumbData& inData, bool copyStaticName = false
    ) noexcept;

    /// @brief Starts a formatted manual breadcrumb event on `cmdList`.
    /// @tparam InArgs Printf argument types.
    /// @param[in,out] cmdList Target command list.
    /// @param[in] inData Breadcrumb metadata with `dynamicFormat`.
    /// @param[in] args Printf arguments.
    template <typename... InArgs>
    BreadcrumbEventManual(command::CommandListBase& cmdList, const BreadcrumbData& inData, InArgs&&... args) noexcept
    {
#if GP_WITH_RHI_BREADCRUMBS
        BreadcrumbAllocator& allocator = acquireAllocator(cmdList);
        BreadcrumbNode* const allocatedNode = allocator.allocFormattedBreadcrumb(inData, std::forward<InArgs>(args)...);
        beginInternal(cmdList, allocatedNode);
#else
        static_cast<void>(cmdList);
        static_cast<void>(inData);
        (static_cast<void>(args), ...);
#endif
    }

    BreadcrumbEventManual(const BreadcrumbEventManual&) = delete;
    BreadcrumbEventManual& operator=(const BreadcrumbEventManual&) = delete;

    /// @brief Move constructor transferring ownership of an in-flight manual breadcrumb event.
    /// @param[in,out] other Source manual event.
    BreadcrumbEventManual(BreadcrumbEventManual&& other) noexcept;

    /// @brief Move assignment operator (asserts `*this` was already ended).
    /// @param[in,out] other Source manual event.
    /// @return Reference to `*this`.
    BreadcrumbEventManual& operator=(BreadcrumbEventManual&& other) noexcept;

    /// @brief Destructor asserting that `end()` was called if this manual event was started.
    ~BreadcrumbEventManual() noexcept;

    /// @brief Ends the manual breadcrumb event on `cmdList`.
    /// @param[in,out] cmdList Command list matching the current execution timeline.
    void end(command::CommandListBase& cmdList) noexcept;

    /// @brief Reports whether this manual event is currently active and awaiting `end()`.
    /// @return True when active.
    [[nodiscard]] GP_FORCEINLINE_HINT bool isActive() const noexcept
    {
        return m_node != nullptr;
    }

    /// @brief Returns the active `BreadcrumbNode*`, or `nullptr` if inactive.
    /// @return `BreadcrumbNode*`.
    [[nodiscard]] GP_FORCEINLINE_HINT BreadcrumbNode* getNode() const noexcept
    {
        return m_node;
    }

private:
    [[nodiscard]] static BreadcrumbAllocator& acquireAllocator(command::CommandListBase& cmdList) noexcept;
    void beginInternal(command::CommandListBase& cmdList, BreadcrumbNode* node) noexcept;

private:
    BreadcrumbNode* m_node{ nullptr };
    sync::PipelineStage m_pipeline{ sync::PipelineStage::None };
#if GP_DO_CHECK
    gp::SharedPtr<BreadcrumbAllocator> m_debugAllocatorRef{};
#endif
};

}   // namespace gp::rhi::diagnostics

#define GP_RHI_BREADCRUMB_JOIN_INNER(a, b) a##b
#define GP_RHI_BREADCRUMB_JOIN(a, b) GP_RHI_BREADCRUMB_JOIN_INNER(a, b)

#if GP_WITH_RHI_BREADCRUMBS
    /// @brief Emits a lexical CPU/GPU breadcrumb scope with a compile-time static name.
    #define GP_RHI_BREADCRUMB_EVENT(cmdList, staticName)                                                \
        ::gp::rhi::diagnostics::BreadcrumbScope GP_RHI_BREADCRUMB_JOIN(_rhiBreadcrumbScope_, __LINE__)( \
            (cmdList),                                                                                  \
            ::gp::rhi::diagnostics::BreadcrumbData(                                                     \
                (staticName),                                                                           \
                nullptr,                                                                                \
                ::gp::rhi::diagnostics::BreadcrumbLocation(__FILE__, static_cast<::gp::UInt32>(__LINE__)) \
            ),                                                                                          \
            true                                                                                        \
        )

    /// @brief Emits a lexical CPU/GPU breadcrumb scope with a static name and printf-formatted dynamic text.
    #define GP_RHI_BREADCRUMB_EVENT_F(cmdList, staticName, formatStr, ...)                              \
        ::gp::rhi::diagnostics::BreadcrumbScope GP_RHI_BREADCRUMB_JOIN(_rhiBreadcrumbScope_, __LINE__)( \
            (cmdList),                                                                                  \
            ::gp::rhi::diagnostics::BreadcrumbData(                                                     \
                (staticName),                                                                           \
                (formatStr),                                                                            \
                ::gp::rhi::diagnostics::BreadcrumbLocation(__FILE__, static_cast<::gp::UInt32>(__LINE__)) \
            ),                                                                                          \
            true,                                                                                       \
            __VA_ARGS__                                                                                 \
        )

    /// @brief Conditionally emits a lexical CPU/GPU breadcrumb scope with a static name.
    #define GP_RHI_BREADCRUMB_EVENT_CONDITIONAL(cmdList, condition, staticName)                         \
        ::gp::rhi::diagnostics::BreadcrumbScope GP_RHI_BREADCRUMB_JOIN(_rhiBreadcrumbScope_, __LINE__)( \
            (cmdList),                                                                                  \
            ::gp::rhi::diagnostics::BreadcrumbData(                                                     \
                (staticName),                                                                           \
                nullptr,                                                                                \
                ::gp::rhi::diagnostics::BreadcrumbLocation(__FILE__, static_cast<::gp::UInt32>(__LINE__)) \
            ),                                                                                          \
            static_cast<bool>(condition)                                                                \
        )

    /// @brief Conditionally emits a lexical CPU/GPU breadcrumb scope with printf-formatted dynamic text.
    #define GP_RHI_BREADCRUMB_EVENT_CONDITIONAL_F(cmdList, condition, staticName, formatStr, ...)       \
        ::gp::rhi::diagnostics::BreadcrumbScope GP_RHI_BREADCRUMB_JOIN(_rhiBreadcrumbScope_, __LINE__)( \
            (cmdList),                                                                                  \
            ::gp::rhi::diagnostics::BreadcrumbData(                                                     \
                (staticName),                                                                           \
                (formatStr),                                                                            \
                ::gp::rhi::diagnostics::BreadcrumbLocation(__FILE__, static_cast<::gp::UInt32>(__LINE__)) \
            ),                                                                                          \
            static_cast<bool>(condition),                                                               \
            __VA_ARGS__                                                                                 \
        )
#else
    #define GP_RHI_BREADCRUMB_EVENT(cmdList, staticName) static_cast<void>(cmdList)
    #define GP_RHI_BREADCRUMB_EVENT_F(cmdList, staticName, formatStr, ...) static_cast<void>(cmdList)
    #define GP_RHI_BREADCRUMB_EVENT_CONDITIONAL(cmdList, condition, staticName) \
        do                                                                      \
        {                                                                       \
            static_cast<void>(cmdList);                                         \
            static_cast<void>(condition);                                       \
        }                                                                       \
        while (false)
    #define GP_RHI_BREADCRUMB_EVENT_CONDITIONAL_F(cmdList, condition, staticName, formatStr, ...) \
        do                                                                                        \
        {                                                                                         \
            static_cast<void>(cmdList);                                                           \
            static_cast<void>(condition);                                                         \
        }                                                                                         \
        while (false)
#endif
