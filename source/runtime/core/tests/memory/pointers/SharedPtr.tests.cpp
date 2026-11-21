// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "memory/pointers/SharedPtr.hpp"
#include "hashing/HashOf.hpp"
#include "memory/allocators/PoolAllocator.hpp"   // IWYU pragma: keep
#include "memory/pointers/WeakPtr.hpp"
#include <gtest/gtest.h>
#include <thread>
#include <vector>

namespace gp::memory::tests
{

namespace
{

/// @brief Forward-declared incomplete type used to verify that `SharedPtr<IncompleteSharedNode>` and
///        `WeakPtr<IncompleteSharedNode>` compile inside a class definition prior to `IncompleteSharedNode` being
///        completed.
struct IncompleteSharedNode;

/// @brief Holder class embedding `SharedPtr` and `WeakPtr` to an incomplete type.
struct IncompleteSharedHolder
{
    SharedPtr<IncompleteSharedNode> m_shared{};
    WeakPtr<IncompleteSharedNode> m_weak{};

    constexpr IncompleteSharedHolder() noexcept = default;
    constexpr IncompleteSharedHolder(IncompleteSharedHolder&&) noexcept = default;
    ~IncompleteSharedHolder() noexcept;
};

struct IncompleteSharedNode
{
    static inline gp::Int32 s_activeNodes{ 0 };
    gp::Int32 tag{ 0 };

    explicit IncompleteSharedNode(const gp::Int32 inTag) noexcept
        : tag(inTag)
    {
        ++s_activeNodes;
    }

    ~IncompleteSharedNode() noexcept
    {
        --s_activeNodes;
    }
};

IncompleteSharedHolder::~IncompleteSharedHolder() noexcept = default;

/// @brief Tracks active object lifetimes and constructor/destructor counts across unit tests.
struct SharedLifetimeTracker
{
    static inline gp::Int32 s_activeInstances{ 0 };
    static inline gp::Int32 s_totalConstructions{ 0 };
    static inline gp::Int32 s_totalDestructions{ 0 };

    gp::Int32 value{ 0 };
    gp::Int32 secondary{ 0 };

    static void resetCounters() noexcept
    {
        s_activeInstances = 0;
        s_totalConstructions = 0;
        s_totalDestructions = 0;
    }

    explicit SharedLifetimeTracker(const gp::Int32 inValue = 0, const gp::Int32 inSecondary = 0) noexcept
        : value(inValue)
        , secondary(inSecondary)
    {
        ++s_activeInstances;
        ++s_totalConstructions;
    }

    SharedLifetimeTracker(const SharedLifetimeTracker& other) noexcept
        : value(other.value)
        , secondary(other.secondary)
    {
        ++s_activeInstances;
        ++s_totalConstructions;
    }

    SharedLifetimeTracker(SharedLifetimeTracker&& other) noexcept
        : value(other.value)
        , secondary(other.secondary)
    {
        other.value = -1;
        ++s_activeInstances;
        ++s_totalConstructions;
    }

    ~SharedLifetimeTracker() noexcept
    {
        --s_activeInstances;
        ++s_totalDestructions;
    }
};

/// @brief Polymorphic hierarchy WITHOUT a virtual destructor on `NonVirtualBase`, verifying that `SharedPtr<Base>`
///        invokes `Derived`'s destructor via type-erased `DefaultReferenceController` / `IntrusiveReferenceController`.
struct NonVirtualBase
{
    static inline gp::Int32 s_activeBases{ 0 };
    gp::Int32 baseId{ 10 };

    explicit NonVirtualBase(const gp::Int32 inBaseId = 10) noexcept
        : baseId(inBaseId)
    {
        ++s_activeBases;
    }

    ~NonVirtualBase() noexcept
    {
        --s_activeBases;
    }
};

struct NonVirtualDerived : public NonVirtualBase
{
    static inline gp::Int32 s_activeDerived{ 0 };
    gp::Int32 derivedPayload{ 20 };

    explicit NonVirtualDerived(const gp::Int32 inBaseId = 10, const gp::Int32 inPayload = 20) noexcept
        : NonVirtualBase(inBaseId)
        , derivedPayload(inPayload)
    {
        ++s_activeDerived;
    }

    ~NonVirtualDerived() noexcept
    {
        --s_activeDerived;
    }
};

/// @brief Hierarchical tree node used to test indirect self-assignment (`root = root->child`) and weak parent links.
struct SharedTreeNode
{
    static inline gp::Int32 s_activeTreeNodes{ 0 };
    gp::Int32 id{ 0 };
    SharedPtr<SharedTreeNode> child{};
    WeakPtr<SharedTreeNode> parent{};

    explicit SharedTreeNode(const gp::Int32 inId) noexcept
        : id(inId)
    {
        ++s_activeTreeNodes;
    }

    ~SharedTreeNode() noexcept
    {
        --s_activeTreeNodes;
    }
};

/// @brief Class inheriting from `SharedFromThis` in `ThreadSafe` mode.
struct SharedWidget : public SharedFromThis<SharedWidget, RefCountThreadSafety::ThreadSafe>
{
    static inline gp::Int32 s_activeWidgets{ 0 };
    gp::Int32 widgetId{ 0 };

    explicit SharedWidget(const gp::Int32 inId) noexcept
        : widgetId(inId)
    {
        ++s_activeWidgets;
    }

    SharedWidget(const SharedWidget& other) noexcept
        : SharedFromThis(other)
        , widgetId(other.widgetId)
    {
        ++s_activeWidgets;
    }

    SharedWidget(SharedWidget&& other) noexcept
        : SharedFromThis(std::move(other))
        , widgetId(other.widgetId)
    {
        ++s_activeWidgets;
    }

    SharedWidget& operator=(const SharedWidget&) noexcept = default;
    SharedWidget& operator=(SharedWidget&&) noexcept = default;

    virtual ~SharedWidget() noexcept
    {
        --s_activeWidgets;
    }
};

struct DerivedSharedWidget final : public SharedWidget
{
    static inline gp::Int32 s_activeDerivedWidgets{ 0 };
    gp::Int32 styleCode{ 0 };

    DerivedSharedWidget(const gp::Int32 inId, const gp::Int32 inStyle) noexcept
        : SharedWidget(inId)
        , styleCode(inStyle)
    {
        ++s_activeDerivedWidgets;
    }

    virtual ~DerivedSharedWidget() noexcept override
    {
        --s_activeDerivedWidgets;
    }
};

/// @brief Class inheriting from `SharedFromThis` in `SingleThreaded` mode.
struct SingleThreadedNode : public SharedFromThis<SingleThreadedNode, RefCountThreadSafety::SingleThreaded>
{
    static inline gp::Int32 s_activeSingleNodes{ 0 };
    gp::Int32 data{ 0 };

    explicit SingleThreadedNode(const gp::Int32 inData) noexcept
        : data(inData)
    {
        ++s_activeSingleNodes;
    }

    ~SingleThreadedNode() noexcept
    {
        --s_activeSingleNodes;
    }
};

/// @brief Class with a private constructor to test `gp::makeShareable()` factory wrapping.
class PrivateConstructedService
{
public:
    static inline gp::Int32 s_activeServices{ 0 };
    gp::Int32 port{ 0 };

private:
    explicit PrivateConstructedService(const gp::Int32 inPort) noexcept
        : port(inPort)
    {
        ++s_activeServices;
    }

public:
    ~PrivateConstructedService() noexcept
    {
        --s_activeServices;
    }

    [[nodiscard]] static SharedRef<PrivateConstructedService> create(const gp::Int32 inPort) noexcept
    {
        return makeShareable(new PrivateConstructedService(inPort));
    }
};

/// @brief Counting allocator wrapper used to verify exact control-block allocation/deallocation counts, especially
///        when `WeakPtr` outlives `SharedPtr` on an `allocateShared` block.
struct CountingAllocator
{
    struct State
    {
        gp::UInt32 activeAllocations{ 0u };
        gp::UInt32 totalAllocations{ 0u };
        gp::UInt32 totalDeallocations{ 0u };
        gp::USize lastAllocatedBytes{ 0u };
        bool shouldFailAllocations{ false };
    };

    State* m_state{ nullptr };
    HeapAllocator m_inner{};

    constexpr CountingAllocator() noexcept = default;

    constexpr explicit CountingAllocator(State* inState) noexcept
        : m_state(inState)
    {}

    [[nodiscard]] void* allocate(const gp::USize sizeInBytes, const gp::UInt32 alignment = kDefaultAlignment) noexcept
    {
        if (m_state != nullptr && m_state->shouldFailAllocations)
        {
            return nullptr;
        }
        if (m_state != nullptr)
        {
            ++m_state->activeAllocations;
            ++m_state->totalAllocations;
            m_state->lastAllocatedBytes = sizeInBytes;
        }
        return m_inner.allocate(sizeInBytes, alignment);
    }

    [[nodiscard]] void* reallocate(
        void* ptr,
        const gp::USize oldSizeInBytes,
        const gp::USize newSizeInBytes,
        const gp::UInt32 alignment = kDefaultAlignment
    ) noexcept
    {
        return m_inner.reallocate(ptr, oldSizeInBytes, newSizeInBytes, alignment);
    }

    void deallocate(
        void* ptr, const gp::USize oldSizeInBytes = 0u, const gp::UInt32 alignment = kDefaultAlignment
    ) noexcept
    {
        if (ptr != nullptr && m_state != nullptr)
        {
            --m_state->activeAllocations;
            ++m_state->totalDeallocations;
        }
        m_inner.deallocate(ptr, oldSizeInBytes, alignment);
    }

    [[nodiscard]] gp::USize getActualAllocationSize(
        const gp::USize requestedSize, const gp::UInt32 alignment = kDefaultAlignment
    ) const noexcept
    {
        return m_inner.getActualAllocationSize(requestedSize, alignment);
    }

    [[nodiscard]] constexpr bool owns(const void* ptr) const noexcept
    {
        return m_inner.owns(ptr);
    }

    [[nodiscard]] constexpr bool operator==(const CountingAllocator& rhs) const noexcept
    {
        return m_state == rhs.m_state;
    }
};

static_assert(concepts::IsAllocator<CountingAllocator>, "CountingAllocator must satisfy gp::concepts::IsAllocator.");

/// @brief Constexpr test verifying compile-time evaluation of `SharedRef`, `SharedPtr`, `WeakPtr`, and `makeShared`.
constexpr bool testConstexprSharedPointers()
{
    SharedRef<gp::Int32> ref = makeShared<gp::Int32>(42);
    if (!ref.isValid() || *ref != 42 || ref.getSharedReferenceCount() != 1u || !ref.isUnique())
    {
        return false;
    }

    SharedPtr<gp::Int32> ptr = ref;
    if (!ptr.isValid() || *ptr != 42 || ptr.getSharedReferenceCount() != 2u || ptr.isUnique())
    {
        return false;
    }

    WeakPtr<gp::Int32> weak = ptr;
    if (!weak.isValid() || weak.isExpired() || weak.getSharedReferenceCount() != 2u)
    {
        return false;
    }

    SharedPtr<gp::Int32> pinned = weak.pin();
    if (!pinned.isValid() || *pinned != 42 || ptr.getSharedReferenceCount() != 3u)
    {
        return false;
    }

    pinned.reset();
    ptr.reset();
    if (ref.getSharedReferenceCount() != 1u || !ref.isUnique())
    {
        return false;
    }

    return true;
}

static_assert(testConstexprSharedPointers(), "Constexpr SharedRef / SharedPtr / WeakPtr evaluation failed.");

// Architectural size, alignment, and relocatability invariants from ADR-0059
static_assert(
    sizeof(gp::detail::ReferenceControllerBase<RefCountThreadSafety::ThreadSafe>) == 16u,
    "ReferenceControllerBase<ThreadSafe> must be exactly 16 bytes on 64-bit targets."
);
static_assert(
    sizeof(gp::detail::ReferenceControllerBase<RefCountThreadSafety::SingleThreaded>) == 16u,
    "ReferenceControllerBase<SingleThreaded> must be exactly 16 bytes on 64-bit targets."
);
static_assert(sizeof(SharedRef<SharedLifetimeTracker>) == 16u, "SharedRef<T> must be 16 bytes (two pointers).");
static_assert(sizeof(SharedPtr<SharedLifetimeTracker>) == 16u, "SharedPtr<T> must be 16 bytes (two pointers).");
static_assert(sizeof(WeakPtr<SharedLifetimeTracker>) == 16u, "WeakPtr<T> must be 16 bytes (two pointers).");
static_assert(concepts::IsRelocatable<SharedRef<SharedLifetimeTracker>>, "SharedRef<T> must be bitwise relocatable.");
static_assert(concepts::IsRelocatable<SharedPtr<SharedLifetimeTracker>>, "SharedPtr<T> must be bitwise relocatable.");
static_assert(concepts::IsRelocatable<WeakPtr<SharedLifetimeTracker>>, "WeakPtr<T> must be bitwise relocatable.");
static_assert(
    !concepts::IsDefaultConstructible<SharedRef<SharedLifetimeTracker>>,
    "SharedRef<T> must NOT be default-constructible."
);

}   // namespace

class SharedPtrTest : public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        IncompleteSharedNode::s_activeNodes = 0;
        SharedLifetimeTracker::resetCounters();
        NonVirtualBase::s_activeBases = 0;
        NonVirtualDerived::s_activeDerived = 0;
        SharedTreeNode::s_activeTreeNodes = 0;
        SharedWidget::s_activeWidgets = 0;
        DerivedSharedWidget::s_activeDerivedWidgets = 0;
        SingleThreadedNode::s_activeSingleNodes = 0;
        PrivateConstructedService::s_activeServices = 0;
    }

    virtual void TearDown() override
    {
        EXPECT_EQ(IncompleteSharedNode::s_activeNodes, 0) << "IncompleteSharedNode leaked!";
        EXPECT_EQ(SharedLifetimeTracker::s_activeInstances, 0) << "SharedLifetimeTracker leaked!";
        EXPECT_EQ(NonVirtualBase::s_activeBases, 0) << "NonVirtualBase leaked!";
        EXPECT_EQ(NonVirtualDerived::s_activeDerived, 0) << "NonVirtualDerived leaked!";
        EXPECT_EQ(SharedTreeNode::s_activeTreeNodes, 0) << "SharedTreeNode leaked!";
        EXPECT_EQ(SharedWidget::s_activeWidgets, 0) << "SharedWidget leaked!";
        EXPECT_EQ(DerivedSharedWidget::s_activeDerivedWidgets, 0) << "DerivedSharedWidget leaked!";
        EXPECT_EQ(SingleThreadedNode::s_activeSingleNodes, 0) << "SingleThreadedNode leaked!";
        EXPECT_EQ(PrivateConstructedService::s_activeServices, 0) << "PrivateConstructedService leaked!";
    }
};

TEST_F(SharedPtrTest, IncompleteForwardDeclaredTypeSupport)
{
    {
        IncompleteSharedHolder holder;
        EXPECT_FALSE(holder.m_shared.isValid());
        EXPECT_FALSE(holder.m_weak.isValid());

        holder.m_shared = makeShared<IncompleteSharedNode>(2718);
        holder.m_weak = holder.m_shared;
        EXPECT_TRUE(holder.m_shared.isValid());
        EXPECT_TRUE(holder.m_weak.isValid());
        EXPECT_EQ(holder.m_shared->tag, 2718);
        EXPECT_EQ(IncompleteSharedNode::s_activeNodes, 1);

        IncompleteSharedHolder movedHolder(std::move(holder));
        EXPECT_FALSE(holder.m_shared.isValid());
        EXPECT_TRUE(movedHolder.m_shared.isValid());
        EXPECT_TRUE(movedHolder.m_weak.isValid());
        EXPECT_EQ(movedHolder.m_shared->tag, 2718);
    }
    EXPECT_EQ(IncompleteSharedNode::s_activeNodes, 0);
}

TEST_F(SharedPtrTest, DefaultAndNullptrConstruction)
{
    const SharedPtr<SharedLifetimeTracker> defaultPtr;
    const SharedPtr<SharedLifetimeTracker> nullPtr(nullptr);
    const SharedPtr<SharedLifetimeTracker> rawNullPtr(static_cast<SharedLifetimeTracker*>(nullptr));

    EXPECT_FALSE(defaultPtr.isValid());
    EXPECT_FALSE(static_cast<bool>(defaultPtr));
    EXPECT_EQ(defaultPtr.get(), nullptr);
    EXPECT_EQ(defaultPtr.getSharedReferenceCount(), 0u);
    EXPECT_EQ(defaultPtr.getRefCount(), 0u);
    EXPECT_FALSE(defaultPtr.isUnique());
    EXPECT_EQ(defaultPtr.getHash(), 0u);

    EXPECT_FALSE(nullPtr.isValid());
    EXPECT_FALSE(rawNullPtr.isValid());
    EXPECT_EQ(defaultPtr, nullPtr);
    EXPECT_EQ(defaultPtr, nullptr);
    EXPECT_EQ(nullptr, defaultPtr);
}

TEST_F(SharedPtrTest, SharedRefNonNullableGuaranteesAndConversionToSharedPtr)
{
    {
        SharedRef<SharedLifetimeTracker> ref1 = makeShared<SharedLifetimeTracker>(10, 20);
        EXPECT_TRUE(ref1.isValid());
        EXPECT_TRUE(static_cast<bool>(ref1));
        EXPECT_TRUE(ref1.isUnique());
        EXPECT_EQ(ref1.getSharedReferenceCount(), 1u);
        EXPECT_EQ(ref1->value, 10);
        EXPECT_EQ((*ref1).secondary, 20);
        EXPECT_EQ(SharedLifetimeTracker::s_activeInstances, 1);

        // Copying a SharedRef increments the shared reference count
        SharedRef<SharedLifetimeTracker> ref2(ref1);
        EXPECT_EQ(ref1.getSharedReferenceCount(), 2u);
        EXPECT_EQ(ref2.getSharedReferenceCount(), 2u);
        EXPECT_FALSE(ref1.isUnique());
        EXPECT_EQ(ref1, ref2);
        EXPECT_TRUE(ref1.hasSameOwner(ref2));

        // Moving a SharedRef into another SharedRef keeps both valid (non-null invariant)
        SharedRef<SharedLifetimeTracker> ref3(std::move(ref2));
        EXPECT_TRUE(ref2.isValid());
        EXPECT_TRUE(ref3.isValid());
        EXPECT_EQ(ref1.getSharedReferenceCount(), 3u);

        // Implicitly converting SharedRef to SharedPtr
        SharedPtr<SharedLifetimeTracker> ptrFromCopy = ref1;
        EXPECT_TRUE(ptrFromCopy.isValid());
        EXPECT_EQ(ref1.getSharedReferenceCount(), 4u);
        EXPECT_EQ(ptrFromCopy, ref1);
        EXPECT_EQ(ref1, ptrFromCopy);

        // Promoting SharedPtr back to SharedRef via lvalue and rvalue toSharedRef()
        SharedRef<SharedLifetimeTracker> promotedCopy = ptrFromCopy.toSharedRef();
        EXPECT_EQ(ref1.getSharedReferenceCount(), 5u);

        SharedRef<SharedLifetimeTracker> promotedMove = std::move(ptrFromCopy).toSharedRef();
        EXPECT_FALSE(ptrFromCopy.isValid());
        EXPECT_EQ(ref1.getSharedReferenceCount(), 5u);
        EXPECT_EQ(promotedMove->value, 10);
        EXPECT_EQ(promotedCopy->value, 10);
    }
    EXPECT_EQ(SharedLifetimeTracker::s_activeInstances, 0);
}

TEST_F(SharedPtrTest, MakeShareableAndCustomDeleterSupport)
{
    gp::Int32 customDeleterCallCount = 0;

    {
        SharedRef<PrivateConstructedService> serviceRef = PrivateConstructedService::create(8080);
        EXPECT_EQ(PrivateConstructedService::s_activeServices, 1);
        EXPECT_EQ(serviceRef->port, 8080);

        SharedPtr<SharedLifetimeTracker> customPtr = makeShareable(
            new SharedLifetimeTracker(99, 100),
            [&customDeleterCallCount](SharedLifetimeTracker* ptr) noexcept -> void
        {
            ++customDeleterCallCount;
            delete ptr;
        }
        );
        ASSERT_TRUE(customPtr.isValid());
        EXPECT_EQ(customPtr->value, 99);
        EXPECT_EQ(customDeleterCallCount, 0);
    }

    EXPECT_EQ(PrivateConstructedService::s_activeServices, 0);
    EXPECT_EQ(SharedLifetimeTracker::s_activeInstances, 0);
    EXPECT_EQ(customDeleterCallCount, 1);
}

TEST_F(SharedPtrTest, TypeErasedDeleterDestroysDerivedThroughNonVirtualBase)
{
    {
        SharedPtr<NonVirtualBase> basePtr = makeShared<NonVirtualDerived>(11, 22);
        EXPECT_EQ(NonVirtualBase::s_activeBases, 1);
        EXPECT_EQ(NonVirtualDerived::s_activeDerived, 1);
        EXPECT_EQ(basePtr->baseId, 11);
    }
    // Even though NonVirtualBase has NO virtual destructor, IntrusiveReferenceController<NonVirtualDerived>
    // destroys the full NonVirtualDerived type!
    EXPECT_EQ(NonVirtualDerived::s_activeDerived, 0);
    EXPECT_EQ(NonVirtualBase::s_activeBases, 0);

    {
        SharedPtr<NonVirtualBase> rawBasePtr(new NonVirtualDerived(33, 44));
        EXPECT_EQ(NonVirtualBase::s_activeBases, 1);
        EXPECT_EQ(NonVirtualDerived::s_activeDerived, 1);
    }
    // DefaultReferenceController<NonVirtualDerived> also remembers the original NonVirtualDerived* type!
    EXPECT_EQ(NonVirtualDerived::s_activeDerived, 0);
    EXPECT_EQ(NonVirtualBase::s_activeBases, 0);
}

TEST_F(SharedPtrTest, AliasingConstructorSharesControlBlockForSubobject)
{
    SharedPtr<gp::Int32> aliasedMemberPtr;

    {
        SharedRef<SharedLifetimeTracker> ownerRef = makeShared<SharedLifetimeTracker>(111, 222);
        SharedPtr<SharedLifetimeTracker> ownerPtr = ownerRef;
        EXPECT_EQ(ownerRef.getSharedReferenceCount(), 2u);

        // Construct an aliased SharedPtr<Int32> pointing directly to `ownerRef->secondary`
        aliasedMemberPtr = SharedPtr<gp::Int32>(ownerPtr, &ownerRef->secondary);
        ASSERT_TRUE(aliasedMemberPtr.isValid());
        EXPECT_EQ(*aliasedMemberPtr, 222);
        EXPECT_EQ(ownerRef.getSharedReferenceCount(), 3u);
        EXPECT_TRUE(aliasedMemberPtr.hasSameOwner(ownerPtr));
        EXPECT_FALSE(aliasedMemberPtr.ownerBefore(ownerPtr));
        EXPECT_FALSE(ownerPtr.ownerBefore(aliasedMemberPtr));
    }

    // `ownerRef` and `ownerPtr` are gone, but `aliasedMemberPtr` keeps the SharedLifetimeTracker alive!
    EXPECT_EQ(SharedLifetimeTracker::s_activeInstances, 1);
    EXPECT_EQ(aliasedMemberPtr.getSharedReferenceCount(), 1u);
    EXPECT_EQ(*aliasedMemberPtr, 222);

    aliasedMemberPtr.reset();
    EXPECT_EQ(SharedLifetimeTracker::s_activeInstances, 0);
}

TEST_F(SharedPtrTest, SelfAssignmentMoveAndHierarchicalAliasingSafety)
{
    SharedPtr<SharedLifetimeTracker> ptr = makeShared<SharedLifetimeTracker>(55, 66);

    // Direct self-copy assignment
    const SharedPtr<SharedLifetimeTracker>& selfCopyAlias = ptr;
    ptr = selfCopyAlias;
    ASSERT_TRUE(ptr.isValid());
    EXPECT_EQ(ptr.getSharedReferenceCount(), 1u);
    EXPECT_EQ(ptr->value, 55);

    // Direct self-move assignment
    SharedPtr<SharedLifetimeTracker>& selfMoveAlias = ptr;
    ptr = std::move(selfMoveAlias);
    ASSERT_TRUE(ptr.isValid());
    EXPECT_EQ(ptr.getSharedReferenceCount(), 1u);
    EXPECT_EQ(ptr->value, 55);

    // Indirect hierarchical aliasing: `root = root->child` when `root` holds the sole strong reference to `child`
    SharedPtr<SharedTreeNode> root = makeShared<SharedTreeNode>(1);
    root->child = makeShared<SharedTreeNode>(2);
    root->child->parent = root;
    EXPECT_EQ(SharedTreeNode::s_activeTreeNodes, 2);

    WeakPtr<SharedTreeNode> weakRoot = root;
    root = root->child;
    ASSERT_TRUE(root.isValid());
    EXPECT_EQ(root->id, 2);
    EXPECT_EQ(root.getSharedReferenceCount(), 1u);
    EXPECT_EQ(SharedTreeNode::s_activeTreeNodes, 1);
    EXPECT_TRUE(weakRoot.isExpired());
    EXPECT_TRUE(root->parent.isExpired());
}

TEST_F(SharedPtrTest, WeakPtrLifecycleAndTwoPhaseControlBlockDeallocation)
{
    CountingAllocator::State allocState{};
    CountingAllocator countingAlloc(&allocState);

    WeakPtr<SharedLifetimeTracker> weakObserver;

    {
        SharedPtr<SharedLifetimeTracker> strongOwner = allocateShared<SharedLifetimeTracker>(countingAlloc, 707, 808);
        EXPECT_EQ(allocState.totalAllocations, 1u);
        EXPECT_EQ(allocState.activeAllocations, 1u);
        EXPECT_EQ(SharedLifetimeTracker::s_activeInstances, 1);

        weakObserver = strongOwner.toWeakPtr();
        EXPECT_TRUE(weakObserver.isValid());
        EXPECT_FALSE(weakObserver.isExpired());
        EXPECT_EQ(weakObserver.getSharedReferenceCount(), 1u);
        EXPECT_TRUE(weakObserver.hasSameOwner(strongOwner));

        SharedPtr<SharedLifetimeTracker> pinned = weakObserver.pin();
        ASSERT_TRUE(pinned.isValid());
        EXPECT_EQ(pinned->value, 707);
        EXPECT_EQ(strongOwner.getSharedReferenceCount(), 2u);
    }

    // Strong owner has gone out of scope:
    // 1. ~SharedLifetimeTracker() MUST have run immediately (`s_activeInstances == 0`).
    // 2. The combined control-block memory MUST still be allocated (`activeAllocations == 1`) because `weakObserver`
    //    is still alive!
    EXPECT_EQ(SharedLifetimeTracker::s_activeInstances, 0);
    EXPECT_EQ(allocState.activeAllocations, 1u);
    EXPECT_EQ(allocState.totalDeallocations, 0u);
    EXPECT_TRUE(weakObserver.isExpired());
    EXPECT_FALSE(weakObserver.isValid());
    EXPECT_EQ(weakObserver.pin(), nullptr);
    EXPECT_EQ(weakObserver.lock(), nullptr);

    // Resetting the last WeakPtr releases the control block memory back to `countingAlloc`
    weakObserver.reset();
    EXPECT_EQ(allocState.activeAllocations, 0u);
    EXPECT_EQ(allocState.totalDeallocations, 1u);
}

TEST_F(SharedPtrTest, SharedFromThisBindingAndPolymorphicInheritance)
{
    {
        SharedRef<DerivedSharedWidget> derivedRef = makeShared<DerivedSharedWidget>(42, 900);
        EXPECT_TRUE(derivedRef->doesSharedInstanceExist());
        EXPECT_EQ(derivedRef.getSharedReferenceCount(), 1u);

        // Calling asShared() on the base SharedFromThis<SharedWidget> returns a valid SharedRef<SharedWidget>
        SharedRef<SharedWidget> baseFromThis = derivedRef->asShared();
        EXPECT_EQ(derivedRef.getSharedReferenceCount(), 2u);
        EXPECT_EQ(baseFromThis->widgetId, 42);
        EXPECT_TRUE(baseFromThis.hasSameOwner(derivedRef));

        // Const asShared() and asWeak()
        const SharedWidget& constWidget = *derivedRef;
        SharedRef<const SharedWidget> constFromThis = constWidget.asShared();
        EXPECT_EQ(derivedRef.getSharedReferenceCount(), 3u);
        EXPECT_EQ(constFromThis->widgetId, 42);

        WeakPtr<const SharedWidget> weakConst = constWidget.asWeak();
        EXPECT_TRUE(weakConst.isValid());
        EXPECT_EQ(weakConst.getSharedReferenceCount(), 3u);

        SharedPtr<SharedWidget> ptrFromThis = derivedRef->sharedFromThis();
        SharedPtr<const SharedWidget> constPtrFromThis = constWidget.sharedFromThis();
        EXPECT_TRUE(ptrFromThis.hasSameOwner(derivedRef));
        EXPECT_TRUE(derivedRef.hasSameOwner(ptrFromThis));
        EXPECT_TRUE(derivedRef.hasSameOwner(weakConst));
        EXPECT_EQ(derivedRef.getSharedReferenceCount(), 5u);
    }
    EXPECT_EQ(DerivedSharedWidget::s_activeDerivedWidgets, 0);
    EXPECT_EQ(SharedWidget::s_activeWidgets, 0);
}

TEST_F(SharedPtrTest, SingleThreadedModeAndSharedFromThis)
{
    using STSharedRef = SharedRef<SingleThreadedNode, RefCountThreadSafety::SingleThreaded>;
    using STSharedPtr = SharedPtr<SingleThreadedNode, RefCountThreadSafety::SingleThreaded>;
    using STWeakPtr = WeakPtr<SingleThreadedNode, RefCountThreadSafety::SingleThreaded>;

    EXPECT_FALSE(STSharedRef::kIsThreadSafe);
    EXPECT_FALSE(STSharedPtr::kIsThreadSafe);
    EXPECT_FALSE(STWeakPtr::kIsThreadSafe);

    STWeakPtr weakNode;
    {
        STSharedRef nodeRef = makeShared<SingleThreadedNode, RefCountThreadSafety::SingleThreaded>(314);
        EXPECT_EQ(SingleThreadedNode::s_activeSingleNodes, 1);
        EXPECT_TRUE(nodeRef->doesSharedInstanceExist());

        STSharedRef selfRef = nodeRef->asShared();
        EXPECT_EQ(nodeRef.getSharedReferenceCount(), 2u);
        EXPECT_EQ(selfRef->data, 314);

        weakNode = nodeRef->asWeak();
        EXPECT_TRUE(weakNode.isValid());

        STSharedPtr pinned = weakNode.pin();
        ASSERT_TRUE(pinned.isValid());
        EXPECT_EQ(nodeRef.getSharedReferenceCount(), 3u);
    }

    EXPECT_EQ(SingleThreadedNode::s_activeSingleNodes, 0);
    EXPECT_TRUE(weakNode.isExpired());
    EXPECT_EQ(weakNode.pin(), nullptr);
}

TEST_F(SharedPtrTest, CastHelpersAndHashIntegration)
{
    SharedRef<DerivedSharedWidget> derivedRef = makeShared<DerivedSharedWidget>(7, 99);
    SharedRef<SharedWidget> baseRef = derivedRef;

    SharedRef<DerivedSharedWidget> downcastRef = staticCastSharedRef<DerivedSharedWidget>(baseRef);
    EXPECT_EQ(downcastRef->styleCode, 99);
    EXPECT_EQ(derivedRef.getSharedReferenceCount(), 3u);

    SharedPtr<SharedWidget> basePtr = baseRef;
    SharedPtr<DerivedSharedWidget> downcastPtrCopy = staticCastSharedPtr<DerivedSharedWidget>(basePtr);
    EXPECT_EQ(downcastPtrCopy->styleCode, 99);
    EXPECT_EQ(derivedRef.getSharedReferenceCount(), 5u);

    SharedPtr<DerivedSharedWidget> downcastPtrMove = staticCastSharedPtr<DerivedSharedWidget>(std::move(basePtr));
    EXPECT_FALSE(basePtr.isValid());
    EXPECT_TRUE(downcastPtrMove.isValid());
    EXPECT_EQ(derivedRef.getSharedReferenceCount(), 5u);

    SharedPtr<const DerivedSharedWidget> constPtr = constCastSharedPtr<const DerivedSharedWidget>(downcastPtrMove);
    SharedPtr<DerivedSharedWidget> mutablePtr = constCastSharedPtr<DerivedSharedWidget>(std::move(constPtr));
    EXPECT_FALSE(constPtr.isValid());
    EXPECT_TRUE(mutablePtr.isValid());

    // Rvalue staticCastSharedRef and constCastSharedRef
    SharedRef<SharedWidget> baseRef2 = derivedRef;
    SharedRef<DerivedSharedWidget> movedDowncastRef = staticCastSharedRef<DerivedSharedWidget>(std::move(baseRef2));
    EXPECT_EQ(movedDowncastRef->styleCode, 99);
    SharedRef<const DerivedSharedWidget> constRef = constCastSharedRef<const DerivedSharedWidget>(movedDowncastRef);
    SharedRef<DerivedSharedWidget> mutableRef = constCastSharedRef<DerivedSharedWidget>(std::move(constRef));
    EXPECT_EQ(mutableRef->styleCode, 99);

    // HashOf integration (`gp::hashing::hashOf`)
    EXPECT_NE(gp::hashing::hashOf(derivedRef), 0u);
    EXPECT_EQ(gp::hashing::hashOf(derivedRef), gp::hashing::hashOf(mutablePtr));
    EXPECT_EQ(gp::hashing::hashOf(basePtr), 0u);
}

TEST_F(SharedPtrTest, MultiThreadedContendedCopyPinAndReleaseStress)
{
    constexpr gp::UInt32 kWorkerThreads = 8u;
    constexpr gp::UInt32 kIterationsPerThread = 10000u;

    SharedPtr<SharedLifetimeTracker> sharedRoot = makeShared<SharedLifetimeTracker>(1337, 42);
    WeakPtr<SharedLifetimeTracker> weakRoot = sharedRoot;

    gp::thread::Atomic<gp::UInt32> readyCount{ 0u };
    gp::thread::Atomic<bool> startFlag{ false };
    gp::thread::Atomic<gp::UInt32> successfulPins{ 0u };

    std::vector<std::thread> workers;
    workers.reserve(kWorkerThreads);

    for (gp::UInt32 t = 0u; t < kWorkerThreads; ++t)
    {
        workers.emplace_back(
            [&sharedRoot, &weakRoot, &readyCount, &startFlag, &successfulPins]() noexcept -> void
        {
            readyCount.fetchAdd(1u, gp::memory::MemoryOrder::AcqRel);
            while (!startFlag.load(gp::memory::MemoryOrder::Acquire))
            {
                std::this_thread::yield();
            }

            for (gp::UInt32 i = 0u; i < kIterationsPerThread; ++i)
            {
                // Copy and destroy strong SharedPtr
                SharedPtr<SharedLifetimeTracker> localCopy(sharedRoot);
                EXPECT_EQ(localCopy->value, 1337);

                // Copy and pin WeakPtr
                WeakPtr<SharedLifetimeTracker> localWeak(weakRoot);
                SharedPtr<SharedLifetimeTracker> pinned = localWeak.pin();
                if (pinned.isValid() && pinned->secondary == 42)
                {
                    successfulPins.fetchAdd(1u, gp::memory::MemoryOrder::Relaxed);
                }
            }
        }
        );
    }

    while (readyCount.load(gp::memory::MemoryOrder::Acquire) < kWorkerThreads)
    {
        std::this_thread::yield();
    }
    startFlag.store(true, gp::memory::MemoryOrder::Release);

    for (std::thread& worker: workers)
    {
        worker.join();
    }

    EXPECT_EQ(successfulPins.load(gp::memory::MemoryOrder::Acquire), kWorkerThreads * kIterationsPerThread);
    EXPECT_EQ(sharedRoot.getSharedReferenceCount(), 1u);
    EXPECT_TRUE(sharedRoot.isUnique());

    sharedRoot.reset();
    EXPECT_TRUE(weakRoot.isExpired());
    EXPECT_EQ(SharedLifetimeTracker::s_activeInstances, 0);
}

TEST_F(SharedPtrTest, MultiThreadedConcurrentWeakPinDuringDestructionRace)
{
    constexpr gp::UInt32 kRounds = 400u;
    constexpr gp::UInt32 kPinThreads = 4u;

    for (gp::UInt32 round = 0u; round < kRounds; ++round)
    {
        SharedPtr<SharedLifetimeTracker> owner = makeShared<SharedLifetimeTracker>(static_cast<gp::Int32>(round), 99);
        WeakPtr<SharedLifetimeTracker> weak = owner;

        gp::thread::Atomic<gp::UInt32> ready{ 0u };
        gp::thread::Atomic<bool> go{ false };

        std::vector<std::thread> pinners;
        pinners.reserve(kPinThreads);

        for (gp::UInt32 t = 0u; t < kPinThreads; ++t)
        {
            pinners.emplace_back(
                [&weak, &ready, &go, round]() noexcept -> void
            {
                ready.fetchAdd(1u, gp::memory::MemoryOrder::AcqRel);
                while (!go.load(gp::memory::MemoryOrder::Acquire))
                {
                    std::this_thread::yield();
                }

                for (gp::UInt32 attempt = 0u; attempt < 32u; ++attempt)
                {
                    SharedPtr<SharedLifetimeTracker> pinned = weak.pin();
                    if (pinned.isValid())
                    {
                        EXPECT_EQ(pinned->value, static_cast<gp::Int32>(round));
                        EXPECT_EQ(pinned->secondary, 99);
                    }
                }
            }
            );
        }

        while (ready.load(gp::memory::MemoryOrder::Acquire) < kPinThreads)
        {
            std::this_thread::yield();
        }
        go.store(true, gp::memory::MemoryOrder::Release);

        // Simultaneously reset the owner while pinners race to call `weak.pin()`
        owner.reset();

        for (std::thread& pinner: pinners)
        {
            pinner.join();
        }

        EXPECT_TRUE(weak.isExpired());
        EXPECT_EQ(SharedLifetimeTracker::s_activeInstances, 0);
    }
}

TEST_F(SharedPtrTest, ReentrantDestructorSafetyAndSwapOrderingComparisons)
{
    struct ReentrantNode
    {
        SharedPtr<ReentrantNode>* m_observedSlot{ nullptr };
        bool* m_observedNullDuringDestruction{ nullptr };
        gp::Int32 id{ 0 };

        ReentrantNode(SharedPtr<ReentrantNode>* inSlot, bool* inObservedNull, const gp::Int32 inId) noexcept
            : m_observedSlot(inSlot)
            , m_observedNullDuringDestruction(inObservedNull)
            , id(inId)
        {}

        ~ReentrantNode() noexcept
        {
            // When m_observedSlot->reset() or ~SharedPtr() runs, it must already be null before ~ReentrantNode()
            if (m_observedSlot != nullptr && m_observedNullDuringDestruction != nullptr)
            {
                *m_observedNullDuringDestruction = !m_observedSlot->isValid() && (m_observedSlot->get() == nullptr);
            }
        }
    };

    bool observedNullDuringDestruction = false;
    SharedPtr<ReentrantNode> globalSlot;
    globalSlot = makeShared<ReentrantNode>(&globalSlot, &observedNullDuringDestruction, 101);
    ASSERT_TRUE(globalSlot.isValid());
    globalSlot.reset();
    EXPECT_TRUE(observedNullDuringDestruction);

    // Swap, ownerBefore, and three-way comparisons across SharedRef, SharedPtr, and WeakPtr
    SharedRef<SharedLifetimeTracker> refA = makeShared<SharedLifetimeTracker>(1, 2);
    SharedRef<SharedLifetimeTracker> refB = makeShared<SharedLifetimeTracker>(3, 4);
    const SharedLifetimeTracker* rawA = refA.get();
    const SharedLifetimeTracker* rawB = refB.get();

    swap(refA, refB);
    EXPECT_EQ(refA.get(), rawB);
    EXPECT_EQ(refB.get(), rawA);
    refA.swap(refB);
    EXPECT_EQ(refA.get(), rawA);
    EXPECT_EQ(refB.get(), rawB);

    SharedPtr<SharedLifetimeTracker> ptrA = refA.toSharedPtr();
    SharedPtr<SharedLifetimeTracker> ptrB = std::move(refB).toSharedPtr();
    EXPECT_EQ(ptrA.get(), rawA);
    EXPECT_EQ(ptrB.get(), rawB);

    swap(ptrA, ptrB);
    EXPECT_EQ(ptrA.get(), rawB);
    EXPECT_EQ(ptrB.get(), rawA);
    ptrA.swap(ptrB);

    WeakPtr<SharedLifetimeTracker> weakA = ptrA;
    WeakPtr<SharedLifetimeTracker> weakB = ptrB;
    swap(weakA, weakB);
    EXPECT_EQ(weakA.pin().get(), rawB);
    EXPECT_EQ(weakB.pin().get(), rawA);
    weakA.swap(weakB);

    // Asymmetric strict weak ordering of control blocks (`ownerBefore`)
    const bool aBeforeB = ptrA.ownerBefore(ptrB);
    const bool bBeforeA = ptrB.ownerBefore(ptrA);
    EXPECT_NE(aBeforeB, bBeforeA);
    EXPECT_EQ(refA.ownerBefore(ptrB), aBeforeB);
    EXPECT_EQ(refA.ownerBefore(weakB), aBeforeB);
    EXPECT_EQ(weakA.ownerBefore(weakB), aBeforeB);
    EXPECT_EQ(weakA.ownerBefore(ptrB), aBeforeB);
    EXPECT_EQ(weakA.ownerBefore(refA), false);

    // Raw pointer and three-way address comparisons
    EXPECT_EQ(ptrA, rawA);
    EXPECT_EQ(refA, rawA);
    EXPECT_NE(ptrA <=> ptrB, std::strong_ordering::equal);
    EXPECT_EQ(ptrA <=> refA, std::strong_ordering::equal);
}

TEST_F(SharedPtrTest, OutOfMemoryGracefulCleanupAndTryAllocateShared)
{
    CountingAllocator::State allocState{};
    allocState.shouldFailAllocations = true;
    CountingAllocator failingAlloc(&allocState);

    // 1. tryAllocateShared returns a null SharedPtr without crashing when the allocator is exhausted
    SharedPtr<SharedLifetimeTracker> failedShared = tryAllocateShared<SharedLifetimeTracker>(failingAlloc, 10, 20);
    EXPECT_FALSE(failedShared.isValid());
    EXPECT_EQ(SharedLifetimeTracker::s_activeInstances, 0);

    // 2. Constructing a SharedPtr from a raw pointer when control-block allocation fails MUST invoke the deleter on
    //    the raw pointer immediately so it never leaks!
    gp::Int32 deleterInvocations = 0;
    SharedLifetimeTracker* rawCandidate = new SharedLifetimeTracker(77, 88);
    EXPECT_EQ(SharedLifetimeTracker::s_activeInstances, 1);

    SharedPtr<SharedLifetimeTracker> failedRawWrap(
        rawCandidate,
        [&deleterInvocations](SharedLifetimeTracker* ptr) noexcept -> void
    {
        ++deleterInvocations;
        delete ptr;
    },
        failingAlloc
    );
    EXPECT_FALSE(failedRawWrap.isValid());
    EXPECT_EQ(failedRawWrap.get(), nullptr);
    EXPECT_EQ(deleterInvocations, 1);
    EXPECT_EQ(SharedLifetimeTracker::s_activeInstances, 0);

    // 3. Once the allocator has capacity, tryAllocateShared succeeds
    allocState.shouldFailAllocations = false;
    SharedPtr<SharedLifetimeTracker> succeededShared = tryAllocateShared<SharedLifetimeTracker>(failingAlloc, 30, 40);
    ASSERT_TRUE(succeededShared.isValid());
    EXPECT_EQ(succeededShared->value, 30);
    EXPECT_EQ(succeededShared->secondary, 40);
    EXPECT_EQ(allocState.activeAllocations, 1u);

    succeededShared.reset();
    EXPECT_EQ(allocState.activeAllocations, 0u);
}

TEST_F(SharedPtrTest, SharedFromThisCopyMovePreservesIndependentWeakThis)
{
    SharedRef<SharedWidget> widget1 = makeShared<SharedWidget>(10);
    EXPECT_TRUE(widget1->doesSharedInstanceExist());

    // Copying or moving the value of a SharedWidget onto the stack must NOT copy widget1's m_weakThis
    SharedWidget stackCopy(*widget1);
    EXPECT_FALSE(stackCopy.doesSharedInstanceExist());

    SharedWidget stackMoved(std::move(stackCopy));
    EXPECT_FALSE(stackMoved.doesSharedInstanceExist());

    // Assigning into a live managed widget2 must preserve widget2's own m_weakThis
    SharedRef<SharedWidget> widget2 = makeShared<SharedWidget>(20);
    *widget2 = *widget1;
    EXPECT_TRUE(widget2->doesSharedInstanceExist());
    SharedRef<SharedWidget> widget2Self = widget2->asShared();
    EXPECT_TRUE(widget2Self.hasSameOwner(widget2));
    EXPECT_FALSE(widget2Self.hasSameOwner(widget1));
}

}   // namespace gp::memory::tests
