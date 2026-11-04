// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "memory/pointers/RefCountedPtr.hpp"
#include "memory/allocators/PoolAllocator.hpp"
#include "memory/utils/MemoryOps.hpp"
#include <gtest/gtest.h>
#include <thread>
#include <vector>

namespace gp::memory::tests
{

namespace
{

/// @brief Forward-declared incomplete type used to verify that `RefCountedPtr<IncompleteForwardResource>` compiles
///        inside a class definition prior to `IncompleteForwardResource` being completed.
struct IncompleteForwardResource;

/// @brief Holder class embedding a `RefCountedPtr` to an incomplete type (mirroring `RHIForward.hpp` usage).
struct IncompleteResourceHolder
{
    RefCountedPtr<IncompleteForwardResource> m_resource{};

    constexpr IncompleteResourceHolder() noexcept = default;
    constexpr IncompleteResourceHolder(IncompleteResourceHolder&&) noexcept = default;
    ~IncompleteResourceHolder() noexcept;
};

/// @brief Complete definition of the previously forward-declared resource.
struct IncompleteForwardResource
    : public IntrusiveRefCounted<IncompleteForwardResource, RefCountThreadSafety::ThreadSafe>
{
    static inline gp::Int32 s_activeForwardResources{ 0 };
    gp::Int32 tag{ 0 };

    explicit IncompleteForwardResource(const gp::Int32 inTag) noexcept
        : tag(inTag)
    {
        ++s_activeForwardResources;
    }

    ~IncompleteForwardResource() noexcept
    {
        --s_activeForwardResources;
    }
};

IncompleteResourceHolder::~IncompleteResourceHolder() noexcept = default;

/// @brief Tracks active object lifetimes to detect leaks or double-deletes across unit tests.
struct RefLifetimeTracker : public IntrusiveRefCounted<RefLifetimeTracker, RefCountThreadSafety::ThreadSafe>
{
    static inline gp::Int32 s_activeInstances{ 0 };
    static inline gp::Int32 s_totalDestructions{ 0 };

    gp::Int32 value{ 0 };

    static void resetCounters() noexcept
    {
        s_activeInstances = 0;
        s_totalDestructions = 0;
    }

    explicit RefLifetimeTracker(const gp::Int32 inValue = 0) noexcept
        : value(inValue)
    {
        ++s_activeInstances;
    }

    RefLifetimeTracker(const RefLifetimeTracker& other) noexcept
        : IntrusiveRefCounted(other)
        , value(other.value)
    {
        ++s_activeInstances;
    }

    RefLifetimeTracker(RefLifetimeTracker&& other) noexcept
        : IntrusiveRefCounted(std::move(other))
        , value(other.value)
    {
        ++s_activeInstances;
    }

    RefLifetimeTracker& operator=(const RefLifetimeTracker&) noexcept = default;
    RefLifetimeTracker& operator=(RefLifetimeTracker&&) noexcept = default;

    ~RefLifetimeTracker() noexcept
    {
        --s_activeInstances;
        ++s_totalDestructions;
    }
};

/// @brief Single-threaded intrusive object for testing `RefCountThreadSafety::SingleThreaded`.
struct SingleThreadedResource : public IntrusiveRefCounted<SingleThreadedResource, RefCountThreadSafety::SingleThreaded>
{
    static inline gp::Int32 s_activeInstances{ 0 };
    gp::Int32 payload{ 0 };

    explicit SingleThreadedResource(const gp::Int32 inPayload = 0) noexcept
        : payload(inPayload)
    {
        ++s_activeInstances;
    }

    ~SingleThreadedResource() noexcept
    {
        --s_activeInstances;
    }
};

/// @brief Polymorphic hierarchy using `IRefCounted` for upcast/downcast testing.
struct PolymorphicBase : public IRefCounted
{
    static inline gp::Int32 s_activeBases{ 0 };

    PolymorphicBase() noexcept
    {
        ++s_activeBases;
    }

    virtual ~PolymorphicBase() noexcept override
    {
        --s_activeBases;
    }

    [[nodiscard]] virtual gp::Int32 getTypeId() const noexcept
    {
        return 1;
    }
};

struct PolymorphicDerived final : public PolymorphicBase
{
    static inline gp::Int32 s_activeDerived{ 0 };
    gp::Int32 extraData{ 99 };

    explicit PolymorphicDerived(const gp::Int32 inExtra = 99) noexcept
        : extraData(inExtra)
    {
        ++s_activeDerived;
    }

    virtual ~PolymorphicDerived() noexcept override
    {
        --s_activeDerived;
    }

    [[nodiscard]] virtual gp::Int32 getTypeId() const noexcept override
    {
        return 2;
    }
};

/// @brief Hierarchical node used to test indirect self-assignment (`node = node->child`).
struct TreeNode : public IntrusiveRefCounted<TreeNode, RefCountThreadSafety::ThreadSafe>
{
    static inline gp::Int32 s_activeNodes{ 0 };
    gp::Int32 id{ 0 };
    RefCountedPtr<TreeNode> child{};

    explicit TreeNode(const gp::Int32 inId) noexcept
        : id(inId)
    {
        ++s_activeNodes;
    }

    ~TreeNode() noexcept
    {
        --s_activeNodes;
    }
};

/// @brief Simulates an RHI resource that intercepts `onZeroReferences()` to enqueue itself into a frame-fenced
///        deferred deletion queue rather than calling `delete this` immediately.
struct MockRHIResource : public IntrusiveRefCounted<MockRHIResource, RefCountThreadSafety::ThreadSafe>
{
    static inline gp::Int32 s_activeHardwareResources{ 0 };
    static inline std::vector<MockRHIResource*> s_deferredDeletionQueue{};

    gp::Int32 resourceId{ 0 };

    static void resetQueue() noexcept
    {
        s_activeHardwareResources = 0;
        s_deferredDeletionQueue.clear();
    }

    static void flushDeferredDeletionQueue() noexcept
    {
        std::vector<MockRHIResource*> batch;
        batch.swap(s_deferredDeletionQueue);

        for (MockRHIResource* resource: batch)
        {
            if (resource->tryBeginDeleting())
            {
                delete resource;
            }
        }
    }

    explicit MockRHIResource(const gp::Int32 inId) noexcept
        : resourceId(inId)
    {
        ++s_activeHardwareResources;
    }

    ~MockRHIResource() noexcept
    {
        --s_activeHardwareResources;
    }

    void onZeroReferences() noexcept
    {
        if (tryMarkForDelete())
        {
            s_deferredDeletionQueue.push_back(this);
        }
    }
};

/// @brief Intrusive object allocated from a `PoolAllocator` via `allocateRefCounted` that returns its memory block
///        to the pool inside `onZeroReferences()` with zero OS heap allocations.
struct PooledRHIResource : public IntrusiveRefCounted<PooledRHIResource, RefCountThreadSafety::ThreadSafe>
{
    static inline gp::Int32 s_activePooledObjects{ 0 };

    PoolAllocator* owningPool{ nullptr };
    gp::Int32 handleId{ 0 };

    PooledRHIResource(PoolAllocator* inPool, const gp::Int32 inHandleId) noexcept
        : owningPool(inPool)
        , handleId(inHandleId)
    {
        ++s_activePooledObjects;
    }

    ~PooledRHIResource() noexcept
    {
        --s_activePooledObjects;
    }

    void onZeroReferences() noexcept
    {
        if (tryBeginDeleting())
        {
            PoolAllocator* pool = owningPool;
            gp::memory::destructItem(this);
            pool->deallocate(this, sizeof(PooledRHIResource), static_cast<gp::UInt32>(alignof(PooledRHIResource)));
        }
    }
};

/// @brief Simulates a Windows/D3D12 COM interface (`IUnknown`) starting with refCount = 1 on creation and exposing
///        PascalCase `AddRef()` and `Release()` without `getRefCount()`.
struct MockComInterface
{
    static inline gp::Int32 s_activeComObjects{ 0 };
    gp::UInt32 comRefCount{ 1u };
    gp::Int32 descriptorSlot{ 0 };

    explicit MockComInterface(const gp::Int32 inSlot) noexcept
        : descriptorSlot(inSlot)
    {
        ++s_activeComObjects;
    }

    ~MockComInterface() noexcept
    {
        --s_activeComObjects;
    }

    gp::UInt32 AddRef() noexcept
    {
        return ++comRefCount;
    }

    gp::UInt32 Release() noexcept
    {
        const gp::UInt32 remaining = --comRefCount;
        if (remaining == 0u)
        {
            delete this;
        }
        return remaining;
    }
};

/// @brief Custom policy adapting COM-style `AddRef()` and `Release()` for `RefCountedPtr`.
template <typename InComType>
struct MockComRefCountPolicy
{
    static void addRef(InComType* ptr) noexcept
    {
        ptr->AddRef();
    }

    static void release(InComType* ptr) noexcept
    {
        ptr->Release();
    }
};

/// @brief Non-intrusive struct used for negative concept validation.
struct NonRefCountedPod
{
    gp::Int32 x{ 0 };
};

/// @brief Constexpr intrusive type for compile-time evaluation tests.
struct ConstexprResource : public IntrusiveRefCounted<ConstexprResource, RefCountThreadSafety::ThreadSafe>
{
    gp::Int32 value{ 0 };

    constexpr explicit ConstexprResource(const gp::Int32 inValue) noexcept
        : value(inValue)
    {}
};

constexpr bool testConstexprRefCountedPtr()
{
    RefCountedPtr<ConstexprResource> ptr1 = makeRefCounted<ConstexprResource>(42);
    if (!ptr1.isValid() || ptr1->value != 42 || ptr1.getRefCount() != 1u || !ptr1.isUnique() || ptr1.getHash() == 0u)
    {
        return false;
    }

    // Copy construction increments reference count to 2
    RefCountedPtr<ConstexprResource> ptr2(ptr1);
    if (ptr1.getRefCount() != 2u || ptr2.getRefCount() != 2u || ptr1.isUnique())
    {
        return false;
    }

    // Move construction transfers reference without incrementing
    RefCountedPtr<ConstexprResource> ptr3(std::move(ptr2));
    if (ptr2.isValid() || !ptr3.isValid() || ptr1.getRefCount() != 2u)
    {
        return false;
    }

    // Reset drops reference count back to 1
    ptr3.reset();
    if (ptr3.isValid() || ptr1.getRefCount() != 1u || !ptr1.isUnique())
    {
        return false;
    }

    // Detach and adopt round-trip
    ConstexprResource* raw = ptr1.detach();
    if (ptr1.isValid() || raw->getRefCount() != 1u)
    {
        return false;
    }

    RefCountedPtr<ConstexprResource> ptr4 = adoptRef(raw);
    if (!ptr4.isValid() || ptr4.getRefCount() != 1u)
    {
        return false;
    }

    return true;
}

static_assert(testConstexprRefCountedPtr(), "Constexpr RefCountedPtr evaluation failed.");

// Architectural size and alignment invariants from ADR-0045
static_assert(
    sizeof(RefCountedPtr<RefLifetimeTracker>) == sizeof(RefLifetimeTracker*),
    "RefCountedPtr must be exactly the size of a single raw pointer (8 bytes on x64)."
);
static_assert(
    alignof(RefCountedPtr<RefLifetimeTracker>) == alignof(RefLifetimeTracker*),
    "RefCountedPtr alignment must match a raw pointer."
);
static_assert(
    sizeof(AtomicRefStateCounter) == sizeof(gp::UInt32), "AtomicRefStateCounter must occupy exactly 4 bytes."
);
static_assert(
    sizeof(gp::detail::SingleThreadedRefStateCounter) == sizeof(gp::UInt32),
    "SingleThreadedRefStateCounter must occupy exactly 4 bytes."
);

}   // namespace

class RefCountedPtrTest : public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        IncompleteForwardResource::s_activeForwardResources = 0;
        RefLifetimeTracker::resetCounters();
        SingleThreadedResource::s_activeInstances = 0;
        PolymorphicBase::s_activeBases = 0;
        PolymorphicDerived::s_activeDerived = 0;
        TreeNode::s_activeNodes = 0;
        MockRHIResource::resetQueue();
        PooledRHIResource::s_activePooledObjects = 0;
        MockComInterface::s_activeComObjects = 0;
    }

    virtual void TearDown() override
    {
        EXPECT_EQ(IncompleteForwardResource::s_activeForwardResources, 0) << "IncompleteForwardResource leaked!";
        EXPECT_EQ(RefLifetimeTracker::s_activeInstances, 0) << "RefLifetimeTracker leaked!";
        EXPECT_EQ(SingleThreadedResource::s_activeInstances, 0) << "SingleThreadedResource leaked!";
        EXPECT_EQ(PolymorphicBase::s_activeBases, 0) << "PolymorphicBase leaked!";
        EXPECT_EQ(PolymorphicDerived::s_activeDerived, 0) << "PolymorphicDerived leaked!";
        EXPECT_EQ(TreeNode::s_activeNodes, 0) << "TreeNode leaked!";
        EXPECT_EQ(MockRHIResource::s_activeHardwareResources, 0) << "MockRHIResource leaked!";
        EXPECT_EQ(PooledRHIResource::s_activePooledObjects, 0) << "PooledRHIResource leaked!";
        EXPECT_EQ(MockComInterface::s_activeComObjects, 0) << "MockComInterface leaked!";
    }
};

TEST_F(RefCountedPtrTest, ConceptValidation)
{
    // Valid intrusive types
    EXPECT_TRUE(concepts::IsRefCounted<RefLifetimeTracker>);
    EXPECT_TRUE(concepts::IsRefCounted<const RefLifetimeTracker>);
    EXPECT_TRUE(concepts::IsRefCounted<SingleThreadedResource>);
    EXPECT_TRUE(concepts::IsRefCounted<MockRHIResource>);
    EXPECT_TRUE(concepts::IsRefCounted<PooledRHIResource>);
    EXPECT_TRUE((concepts::IsRefCounted<MockComInterface, MockComRefCountPolicy<MockComInterface>>));

    // Negative concept validation for non-intrusive and void types
    EXPECT_FALSE(concepts::IsRefCounted<NonRefCountedPod>);
    EXPECT_FALSE(concepts::IsRefCounted<const NonRefCountedPod>);
    EXPECT_FALSE(concepts::IsRefCounted<gp::Int32>);
    EXPECT_FALSE((concepts::IsRefCountPolicy<DefaultRefCountPolicy<NonRefCountedPod>, NonRefCountedPod>));
    EXPECT_FALSE((concepts::IsRefCountPolicy<DefaultRefCountPolicy<void>, void>));

    // Optional query and zero-reference hook concepts
    EXPECT_TRUE((concepts::HasRefCountQuery<DefaultRefCountPolicy<RefLifetimeTracker>, RefLifetimeTracker>));
    EXPECT_FALSE((concepts::HasRefCountQuery<MockComRefCountPolicy<MockComInterface>, MockComInterface>));
    EXPECT_FALSE((concepts::HasRefCountQuery<DefaultRefCountPolicy<NonRefCountedPod>, NonRefCountedPod>));

    EXPECT_FALSE(concepts::HasZeroReferencesHook<RefLifetimeTracker>);
    EXPECT_TRUE(concepts::HasZeroReferencesHook<MockRHIResource>);
    EXPECT_TRUE(concepts::HasZeroReferencesHook<PooledRHIResource>);
}

TEST_F(RefCountedPtrTest, IncompleteForwardDeclaredTypeSupport)
{
    {
        IncompleteResourceHolder holder;
        EXPECT_FALSE(holder.m_resource.isValid());

        holder.m_resource = makeRefCounted<IncompleteForwardResource>(314);
        EXPECT_TRUE(holder.m_resource.isValid());
        EXPECT_EQ(holder.m_resource->tag, 314);
        EXPECT_EQ(IncompleteForwardResource::s_activeForwardResources, 1);

        IncompleteResourceHolder movedHolder(std::move(holder));
        EXPECT_FALSE(holder.m_resource.isValid());
        EXPECT_TRUE(movedHolder.m_resource.isValid());
        EXPECT_EQ(movedHolder.m_resource->tag, 314);
    }
    EXPECT_EQ(IncompleteForwardResource::s_activeForwardResources, 0);
}

TEST_F(RefCountedPtrTest, DefaultAndNullptrConstruction)
{
    const RefCountedPtr<RefLifetimeTracker> defaultPtr;
    const RefCountedPtr<RefLifetimeTracker> nullPtr(nullptr);

    EXPECT_FALSE(defaultPtr.isValid());
    EXPECT_FALSE(static_cast<bool>(defaultPtr));
    EXPECT_EQ(defaultPtr.get(), nullptr);
    EXPECT_EQ(defaultPtr.getRefCount(), 0u);
    EXPECT_FALSE(defaultPtr.isUnique());
    EXPECT_EQ(defaultPtr.getHash(), 0u);

    EXPECT_FALSE(nullPtr.isValid());
    EXPECT_EQ(nullPtr.get(), nullptr);
    EXPECT_EQ(defaultPtr, nullPtr);
    EXPECT_EQ(defaultPtr, nullptr);
}

TEST_F(RefCountedPtrTest, RetainVsAdoptOwnershipTags)
{
    // RetainRef increments ref count from 0 -> 1
    {
        RefLifetimeTracker* raw = new RefLifetimeTracker(10);
        EXPECT_EQ(raw->getRefCount(), 0u);

        const RefCountedPtr<RefLifetimeTracker> retained = retainRef(raw);
        EXPECT_EQ(retained.getRefCount(), 1u);
        EXPECT_TRUE(retained.isUnique());
        EXPECT_EQ(retained->value, 10);
        EXPECT_EQ(RefLifetimeTracker::s_activeInstances, 1);
    }
    EXPECT_EQ(RefLifetimeTracker::s_activeInstances, 0);

    // AdoptRef takes ownership of an already-incremented pointer without calling addRef()
    {
        RefLifetimeTracker* raw = new RefLifetimeTracker(20);
        raw->addRef();
        EXPECT_EQ(raw->getRefCount(), 1u);

        RefCountedPtr<RefLifetimeTracker> adopted = adoptRef(raw);
        EXPECT_EQ(adopted.getRefCount(), 1u);
        EXPECT_EQ((*adopted).value, 20);

        // Replacing via adopt() releases the previous object and adopts the new one
        RefLifetimeTracker* secondRaw = new RefLifetimeTracker(30);
        secondRaw->addRef();
        adopted.adopt(secondRaw);
        EXPECT_EQ(RefLifetimeTracker::s_activeInstances, 1);
        EXPECT_EQ(adopted.getRefCount(), 1u);
        EXPECT_EQ(adopted->value, 30);
    }
    EXPECT_EQ(RefLifetimeTracker::s_activeInstances, 0);
}

TEST_F(RefCountedPtrTest, CopyMoveAndConstSemantics)
{
    RefCountedPtr<RefLifetimeTracker> ptr1 = makeRefCounted<RefLifetimeTracker>(77);
    ASSERT_TRUE(ptr1.isValid());
    EXPECT_EQ(ptr1.getRefCount(), 1u);
    EXPECT_TRUE(ptr1.isUnique());

    // Copy construction to mutable and const handles
    RefCountedPtr<RefLifetimeTracker> ptr2(ptr1);
    RefCountedPtr<const RefLifetimeTracker> constPtr(ptr1);
    EXPECT_EQ(ptr1.getRefCount(), 3u);
    EXPECT_EQ(constPtr.getRefCount(), 3u);
    EXPECT_EQ(constPtr->value, 77);
    EXPECT_FALSE(ptr1.isUnique());

    // Move construction transfers ownership without changing refCount
    RefCountedPtr<RefLifetimeTracker> ptr3(std::move(ptr2));
    EXPECT_FALSE(ptr2.isValid());
    EXPECT_TRUE(ptr3.isValid());
    EXPECT_EQ(ptr1.getRefCount(), 3u);

    // Underlying IntrusiveRefCounted copy/move never copies the reference count
    const RefLifetimeTracker copiedObject(*ptr1);
    EXPECT_EQ(copiedObject.getRefCount(), 0u);
    EXPECT_EQ(ptr1.getRefCount(), 3u);

    constPtr.reset();
    ptr3 = nullptr;
    EXPECT_EQ(ptr1.getRefCount(), 1u);
    EXPECT_TRUE(ptr1.isUnique());
}

TEST_F(RefCountedPtrTest, SelfAssignmentMoveAndHierarchicalAliasingSafety)
{
    RefCountedPtr<RefLifetimeTracker> ptr = makeRefCounted<RefLifetimeTracker>(55);

    // Direct self-copy assignment and raw pointer self-assignment
    const RefCountedPtr<RefLifetimeTracker>& selfCopyAlias = ptr;
    ptr = selfCopyAlias;
    EXPECT_TRUE(ptr.isValid());
    EXPECT_EQ(ptr.getRefCount(), 1u);
    EXPECT_EQ(ptr->value, 55);

    ptr = ptr.get();
    EXPECT_TRUE(ptr.isValid());
    EXPECT_EQ(ptr.getRefCount(), 1u);

    // Direct self-move assignment
    RefCountedPtr<RefLifetimeTracker>& selfMoveAlias = ptr;
    ptr = std::move(selfMoveAlias);
    EXPECT_TRUE(ptr.isValid());
    EXPECT_EQ(ptr.getRefCount(), 1u);
    EXPECT_EQ(ptr->value, 55);

    // Indirect hierarchical aliasing: `root = root->child` when `root` holds the sole reference to `child`
    RefCountedPtr<TreeNode> root = makeRefCounted<TreeNode>(1);
    root->child = makeRefCounted<TreeNode>(2);
    EXPECT_EQ(TreeNode::s_activeNodes, 2);
    EXPECT_EQ(root->child.getRefCount(), 1u);

    root = root->child;
    ASSERT_TRUE(root.isValid());
    EXPECT_EQ(root->id, 2);
    EXPECT_EQ(root.getRefCount(), 1u);
    EXPECT_EQ(TreeNode::s_activeNodes, 1);
}

TEST_F(RefCountedPtrTest, PolymorphicConversionsAndCastHelpers)
{
    RefCountedPtr<PolymorphicDerived> derived = makeRefCounted<PolymorphicDerived>(123);
    EXPECT_EQ(PolymorphicBase::s_activeBases, 1);
    EXPECT_EQ(PolymorphicDerived::s_activeDerived, 1);

    // Converting copy assignment replacing an existing base object
    RefCountedPtr<PolymorphicBase> base = makeRefCounted<PolymorphicBase>();
    EXPECT_EQ(PolymorphicBase::s_activeBases, 2);
    base = derived;
    EXPECT_EQ(PolymorphicBase::s_activeBases, 1);
    EXPECT_EQ(base.getRefCount(), 2u);
    EXPECT_EQ(base->getTypeId(), 2);

    // Converting move assignment from a second derived instance
    RefCountedPtr<PolymorphicDerived> derived2 = makeRefCounted<PolymorphicDerived>(456);
    EXPECT_EQ(PolymorphicBase::s_activeBases, 2);
    base = std::move(derived2);
    EXPECT_FALSE(derived2.isValid());
    EXPECT_EQ(base->getTypeId(), 2);
    EXPECT_EQ(base.getRefCount(), 1u);

    // Restore `base` to `derived`
    base = derived;
    EXPECT_EQ(PolymorphicBase::s_activeBases, 1);
    EXPECT_EQ(base.getRefCount(), 2u);

    // staticCastRef (lvalue copy cast & rvalue move cast)
    RefCountedPtr<PolymorphicDerived> downcastCopy = staticCastRef<PolymorphicDerived>(base);
    EXPECT_EQ(downcastCopy.getRefCount(), 3u);
    EXPECT_EQ(downcastCopy->extraData, 123);

    RefCountedPtr<PolymorphicBase> movedBase = staticCastRef<PolymorphicBase>(std::move(downcastCopy));
    EXPECT_FALSE(downcastCopy.isValid());
    EXPECT_TRUE(movedBase.isValid());
    EXPECT_EQ(movedBase.getRefCount(), 3u);

    // constCastRef (lvalue copy cast & rvalue move cast)
    RefCountedPtr<const PolymorphicBase> constBase = constCastRef<const PolymorphicBase>(movedBase);
    EXPECT_EQ(constBase.getRefCount(), 4u);

    RefCountedPtr<PolymorphicBase> mutableBase = constCastRef<PolymorphicBase>(std::move(constBase));
    EXPECT_FALSE(constBase.isValid());
    EXPECT_TRUE(mutableBase.isValid());
    EXPECT_EQ(mutableBase.getRefCount(), 4u);

    // reinterpretCastRef (lvalue copy cast & rvalue move cast)
    RefCountedPtr<PolymorphicDerived> reinterpretedCopy = reinterpretCastRef<PolymorphicDerived>(mutableBase);
    EXPECT_EQ(reinterpretedCopy.getRefCount(), 5u);

    RefCountedPtr<PolymorphicBase> reinterpretedMoved =
        reinterpretCastRef<PolymorphicBase>(std::move(reinterpretedCopy));
    EXPECT_FALSE(reinterpretedCopy.isValid());
    EXPECT_TRUE(reinterpretedMoved.isValid());
    EXPECT_EQ(reinterpretedMoved.getRefCount(), 5u);
}

TEST_F(RefCountedPtrTest, SwapAndHashing)
{
    RefCountedPtr<RefLifetimeTracker> a = makeRefCounted<RefLifetimeTracker>(11);
    RefCountedPtr<RefLifetimeTracker> b = makeRefCounted<RefLifetimeTracker>(22);
    const RefLifetimeTracker* rawA = a.get();
    const RefLifetimeTracker* rawB = b.get();

    EXPECT_NE(a.getHash(), 0u);
    EXPECT_NE(b.getHash(), 0u);
    EXPECT_NE(a.getHash(), b.getHash());

    // Member swap
    a.swap(b);
    EXPECT_EQ(a.get(), rawB);
    EXPECT_EQ(b.get(), rawA);
    EXPECT_EQ(a.getRefCount(), 1u);
    EXPECT_EQ(b.getRefCount(), 1u);

    // ADL free-function swap
    swap(a, b);
    EXPECT_EQ(a.get(), rawA);
    EXPECT_EQ(b.get(), rawB);
}

TEST_F(RefCountedPtrTest, InitReferenceAndComPolicyInterop)
{
    const auto createMockComResource = [](const gp::Int32 slot, MockComInterface** outResource) noexcept -> void
    {
        *outResource = new MockComInterface(slot);
    };

    using ComPtr = RefCountedPtr<MockComInterface, MockComRefCountPolicy<MockComInterface>>;

    ComPtr comResource;
    createMockComResource(7, comResource.initReference());
    ASSERT_TRUE(comResource.isValid());
    EXPECT_EQ(comResource->descriptorSlot, 7);
    EXPECT_EQ(comResource->comRefCount, 1u);
    EXPECT_EQ(MockComInterface::s_activeComObjects, 1);

    // Calling initReference() again must release the previous resource (7) before receiving the new one (8)
    createMockComResource(8, comResource.initReference());
    ASSERT_TRUE(comResource.isValid());
    EXPECT_EQ(comResource->descriptorSlot, 8);
    EXPECT_EQ(comResource->comRefCount, 1u);
    EXPECT_EQ(MockComInterface::s_activeComObjects, 1);

    MockComInterface* const* addressOfPtr = comResource.getAddressOf();
    ASSERT_NE(addressOfPtr, nullptr);
    EXPECT_EQ(*addressOfPtr, comResource.get());
    EXPECT_EQ(comResource->comRefCount, 1u);
}

TEST_F(RefCountedPtrTest, RHIDeferredDeletionStateAndResurrectionSafety)
{
    MockRHIResource* rawResource = nullptr;

    {
        RefCountedPtr<MockRHIResource> buffer = makeRefCounted<MockRHIResource>(404);
        rawResource = buffer.get();
        EXPECT_EQ(MockRHIResource::s_activeHardwareResources, 1);
        EXPECT_FALSE(buffer->isMarkedForDelete());
        EXPECT_FALSE(buffer->isDeleting());

        // While live (refCount == 1), both tryAddRefIfLive() and tryAddRefIfNotDeleting() succeed
        EXPECT_TRUE(rawResource->tryAddRefIfLive());
        EXPECT_EQ(rawResource->getRefCount(), 2u);
        rawResource->release();
        EXPECT_EQ(rawResource->getRefCount(), 1u);
    }

    // 1. Dropping to 0 refs triggers `onZeroReferences()`, latching `MarkedForDelete` and queueing instead of deleting
    EXPECT_EQ(MockRHIResource::s_activeHardwareResources, 1);
    ASSERT_EQ(MockRHIResource::s_deferredDeletionQueue.size(), 1u);
    EXPECT_EQ(rawResource->getRefCount(), 0u);
    EXPECT_TRUE(rawResource->isMarkedForDelete());
    EXPECT_FALSE(rawResource->isDeleting());

    // While queued for deletion (`refCount == 0`, `MarkedForDelete` set), `tryAddRefIfLive()` must reject promotion,
    // whereas `tryAddRefIfNotDeleting()` permits cache resurrection!
    EXPECT_FALSE(rawResource->tryAddRefIfLive());
    EXPECT_EQ(rawResource->getRefCount(), 0u);

    // 2. Transient resurrection + release (`0 -> 1 -> 0`) while already queued MUST NOT double-enqueue!
    {
        ASSERT_TRUE(rawResource->tryAddRefIfNotDeleting());
        RefCountedPtr<MockRHIResource> transientHandle = adoptRef(rawResource);
        EXPECT_EQ(transientHandle.getRefCount(), 1u);
        EXPECT_TRUE(transientHandle->isMarkedForDelete());
    }
    EXPECT_EQ(rawResource->getRefCount(), 0u);
    EXPECT_EQ(MockRHIResource::s_deferredDeletionQueue.size(), 1u);

    // 3. Persistent resurrection (`0 -> 1`) before the GPU fence completes aborts deletion and clears `MarkedForDelete`
    ASSERT_TRUE(rawResource->tryAddRefIfNotDeleting());
    RefCountedPtr<MockRHIResource> resurrectedHandle = adoptRef(rawResource);
    EXPECT_EQ(resurrectedHandle.getRefCount(), 1u);

    MockRHIResource::flushDeferredDeletionQueue();
    EXPECT_EQ(MockRHIResource::s_deferredDeletionQueue.size(), 0u);
    EXPECT_EQ(MockRHIResource::s_activeHardwareResources, 1);
    EXPECT_FALSE(resurrectedHandle->isMarkedForDelete());
    EXPECT_FALSE(resurrectedHandle->isDeleting());

    // 4. Releasing the resurrected handle re-marks and re-enqueues the resource; flushing now destroys it
    resurrectedHandle.reset();
    EXPECT_TRUE(rawResource->isMarkedForDelete());
    ASSERT_EQ(MockRHIResource::s_deferredDeletionQueue.size(), 1u);

    MockRHIResource::flushDeferredDeletionQueue();
    EXPECT_EQ(MockRHIResource::s_deferredDeletionQueue.size(), 0u);
    EXPECT_EQ(MockRHIResource::s_activeHardwareResources, 0);
}

TEST_F(RefCountedPtrTest, PoolAllocatorIntegrationViaAllocateRefCounted)
{
    alignas(alignof(PooledRHIResource)) gp::UInt8 stackSlab[sizeof(PooledRHIResource) * 4u]{};
    PoolAllocator pool(
        stackSlab,
        sizeof(stackSlab),
        sizeof(PooledRHIResource),
        static_cast<gp::UInt32>(alignof(PooledRHIResource)),
        "Pool.RHIResources"
    );

    EXPECT_EQ(pool.getUsedBlockCount(), 0u);

    {
        RefCountedPtr<PooledRHIResource> res1 = allocateRefCounted<PooledRHIResource>(pool, &pool, 101);
        RefCountedPtr<PooledRHIResource> res2 = allocateRefCounted<PooledRHIResource>(pool, &pool, 202);

        ASSERT_TRUE(res1.isValid());
        ASSERT_TRUE(res2.isValid());
        EXPECT_EQ(pool.getUsedBlockCount(), 2u);
        EXPECT_EQ(PooledRHIResource::s_activePooledObjects, 2);
        EXPECT_EQ(res1->handleId, 101);
        EXPECT_EQ(res2->handleId, 202);
        EXPECT_TRUE(pool.owns(res1.get()));
        EXPECT_TRUE(pool.owns(res2.get()));
    }

    // Both pooled RHI resources must have returned their blocks to `pool` via `onZeroReferences()`
    EXPECT_EQ(PooledRHIResource::s_activePooledObjects, 0);
    EXPECT_EQ(pool.getUsedBlockCount(), 0u);
}

TEST_F(RefCountedPtrTest, SingleThreadedRefCountedMode)
{
    {
        RefCountedPtr<SingleThreadedResource> res1 = makeRefCounted<SingleThreadedResource>(15);
        EXPECT_FALSE(SingleThreadedResource::kIsThreadSafe);
        EXPECT_EQ(SingleThreadedResource::s_activeInstances, 1);
        EXPECT_EQ(res1.getRefCount(), 1u);

        EXPECT_TRUE(res1->tryAddRefIfLive());
        EXPECT_EQ(res1.getRefCount(), 2u);
        res1->release();
        EXPECT_EQ(res1.getRefCount(), 1u);

        RefCountedPtr<SingleThreadedResource> res2 = res1;
        EXPECT_EQ(res1.getRefCount(), 2u);
        EXPECT_EQ(res2->payload, 15);
    }
    EXPECT_EQ(SingleThreadedResource::s_activeInstances, 0);
}

TEST_F(RefCountedPtrTest, ComparisonsAndOrdering)
{
    RefCountedPtr<RefLifetimeTracker> a = makeRefCounted<RefLifetimeTracker>(1);
    RefCountedPtr<RefLifetimeTracker> b = makeRefCounted<RefLifetimeTracker>(2);
    RefCountedPtr<RefLifetimeTracker> empty;

    EXPECT_EQ(a, a.get());
    EXPECT_NE(a, b);
    EXPECT_NE(a, nullptr);
    EXPECT_EQ(empty, nullptr);
    EXPECT_TRUE((a <=> nullptr) > 0);
    EXPECT_TRUE((empty <=> nullptr) == 0);

    if (a.get() < b.get())
    {
        EXPECT_TRUE((a <=> b) < 0);
        EXPECT_TRUE((a <=> b.get()) < 0);
    }
    else
    {
        EXPECT_TRUE((a <=> b) > 0);
        EXPECT_TRUE((a <=> b.get()) > 0);
    }
}

TEST_F(RefCountedPtrTest, MultiThreadedContendedCopyAndReleaseStress)
{
    constexpr gp::UInt32 kThreadCount = 8u;
    constexpr gp::UInt32 kIterationsPerThread = 10000u;

    RefCountedPtr<RefLifetimeTracker> sharedResource = makeRefCounted<RefLifetimeTracker>(2026);
    gp::thread::Atomic<bool> startGate(false);

    std::vector<std::thread> workers;
    workers.reserve(kThreadCount);

    for (gp::UInt32 t = 0u; t < kThreadCount; ++t)
    {
        workers.emplace_back(
            [&sharedResource, &startGate]() -> void
        {
            while (!startGate.load(gp::memory::MemoryOrder::Acquire))
            {
                std::this_thread::yield();
            }

            for (gp::UInt32 i = 0u; i < kIterationsPerThread; ++i)
            {
                RefCountedPtr<RefLifetimeTracker> localCopy(sharedResource);
                RefCountedPtr<RefLifetimeTracker> movedCopy(std::move(localCopy));
                EXPECT_EQ(movedCopy->value, 2026);
            }
        }
        );
    }

    startGate.store(true, gp::memory::MemoryOrder::Release);

    for (std::thread& worker: workers)
    {
        worker.join();
    }

    EXPECT_EQ(sharedResource.getRefCount(), 1u);
    EXPECT_TRUE(sharedResource.isUnique());
    EXPECT_EQ(RefLifetimeTracker::s_activeInstances, 1);

    sharedResource.reset();
    EXPECT_EQ(RefLifetimeTracker::s_activeInstances, 0);
    EXPECT_EQ(RefLifetimeTracker::s_totalDestructions, 1);
}

TEST_F(RefCountedPtrTest, MultiThreadedTryMarkForDeleteExclusivity)
{
    constexpr gp::UInt32 kThreadCount = 8u;

    AtomicRefStateCounter counter(0u);
    gp::thread::Atomic<bool> startGate(false);
    gp::thread::Atomic<gp::UInt32> winnerCount(0u);

    std::vector<std::thread> workers;
    workers.reserve(kThreadCount);

    for (gp::UInt32 t = 0u; t < kThreadCount; ++t)
    {
        workers.emplace_back(
            [&counter, &startGate, &winnerCount]() -> void
        {
            while (!startGate.load(gp::memory::MemoryOrder::Acquire))
            {
                std::this_thread::yield();
            }

            if (counter.tryMarkForDelete())
            {
                winnerCount.fetchAdd(1u, gp::memory::MemoryOrder::Relaxed);
            }
        }
        );
    }

    startGate.store(true, gp::memory::MemoryOrder::Release);

    for (std::thread& worker: workers)
    {
        worker.join();
    }

    // Exactly one thread must claim the deferred deletion transition
    EXPECT_EQ(winnerCount.load(gp::memory::MemoryOrder::Acquire), 1u);
    EXPECT_TRUE(counter.isMarkedForDelete());
    EXPECT_TRUE(counter.tryBeginDeleting());
    EXPECT_TRUE(counter.isDeleting());
    EXPECT_FALSE(counter.tryAddRefIfNotDeleting());
    EXPECT_FALSE(counter.tryAddRefIfLive());
}

}   // namespace gp::memory::tests
