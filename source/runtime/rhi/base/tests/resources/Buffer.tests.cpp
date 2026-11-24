// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "resources/Buffer.hpp"
#include "containers/strings/Name.hpp"
#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "core/GPUMask.hpp"
#include "core/Resource.hpp"
#include "core/ResourceDeletionQueue.hpp"
#include "core/ResourceEnums.hpp"
#include "core/ResourceTypes.hpp"
#include "core/ViewableResource.hpp"
#include "CoreMinimal.hpp"
#include "hashing/Fnv1a.hpp"
#include "hashing/HashOf.hpp"
#include "memory/MemoryOrder.hpp"
#include "memory/pointers/PointerConcepts.hpp"
#include "memory/pointers/RefCountedPtr.hpp"
#include "resources/BufferCreateDesc.hpp"
#include "resources/BufferDesc.hpp"
#include "resources/BufferEnums.hpp"
#include "resources/StagingBuffer.hpp"
#include "resources/StreamSourceSlot.hpp"
#include "sync/SyncEnums.hpp"
#include "sync/TransitionDesc.hpp"
#include "threading/atomics/Atomic.hpp"
#include <gtest/gtest.h>
#include <type_traits>

namespace gp::rhi::resource::tests
{

static_assert(std::is_base_of_v<ViewableResource, Buffer>, "Buffer must inherit publicly from ViewableResource.");
static_assert(std::is_base_of_v<Resource, StagingBuffer>, "StagingBuffer must inherit publicly from Resource.");
static_assert(gp::concepts::IsRefCounted<Buffer>, "Buffer must satisfy gp::concepts::IsRefCounted.");
static_assert(gp::concepts::IsRefCounted<StagingBuffer>, "StagingBuffer must satisfy gp::concepts::IsRefCounted.");
static_assert(
    gp::concepts::HasRefCountQuery<gp::DefaultRefCountPolicy<Buffer>, Buffer>,
    "Buffer must satisfy gp::concepts::HasRefCountQuery."
);
static_assert(
    gp::concepts::HasRefCountQuery<gp::DefaultRefCountPolicy<StagingBuffer>, StagingBuffer>,
    "StagingBuffer must satisfy gp::concepts::HasRefCountQuery."
);
static_assert(sizeof(BufferRef) == sizeof(Buffer*), "BufferRef must remain a single pointer.");
static_assert(sizeof(StagingBufferRef) == sizeof(StagingBuffer*), "StagingBufferRef must remain a single pointer.");
static_assert(!std::is_default_constructible_v<Buffer>, "Buffer must not be default constructible.");
static_assert(!std::is_copy_constructible_v<Buffer>, "Buffer must not be copy constructible.");
static_assert(!std::is_copy_assignable_v<Buffer>, "Buffer must not be copy assignable.");
static_assert(!std::is_move_constructible_v<Buffer>, "Buffer must not be move constructible.");
static_assert(!std::is_move_assignable_v<Buffer>, "Buffer must not be move assignable.");
static_assert(!std::is_copy_constructible_v<StagingBuffer>, "StagingBuffer must not be copy constructible.");
static_assert(!std::is_copy_assignable_v<StagingBuffer>, "StagingBuffer must not be copy assignable.");
static_assert(!std::is_move_constructible_v<StagingBuffer>, "StagingBuffer must not be move constructible.");
static_assert(!std::is_move_assignable_v<StagingBuffer>, "StagingBuffer must not be move assignable.");
static_assert(std::is_abstract_v<StagingBuffer>, "StagingBuffer must remain an abstract base class.");

static_assert(!BufferDesc{}.isNull(), "Default BufferDesc must not be a null resource.");
static_assert(BufferDesc::null().isNull(), "BufferDesc::null() must report isNull() == true.");
static_assert(
    BufferDesc(256u, 16u, BufferUsageFlags::StructuredBuffer) ==
        BufferDesc(256u, 16u, BufferUsageFlags::StructuredBuffer, GPUMask::all()),
    "Three-argument BufferDesc constructor must default gpuMask to GPUMask::all()."
);
static_assert(
    gp::hashing::hashOf(BufferDesc(128u, 4u, BufferUsageFlags::IndexBuffer)) ==
        BufferDesc(128u, 4u, BufferUsageFlags::IndexBuffer).getHash(),
    "hashOf must dispatch to BufferDesc::getHash()."
);

namespace
{

/// @brief Sample vertex layout used to verify typed vertex buffer factory sizing.
struct TestVertex
{
    gp::Float32 position[3];
    gp::Float32 uv[2];
};

/// @brief Sample structured element layout used to verify typed structured buffer factory sizing.
struct alignas(16) TestStructuredElement
{
    gp::Float32 matrixRow[4];
    gp::UInt32 materialId;
    gp::UInt32 padding[3];
};

/// @brief Mock upload provider tracking discard calls for IResourceArrayUpload verification.
class MockResourceArrayUpload final : public IResourceArrayUpload
{
public:
    /// @brief Constructs an upload provider referencing caller-owned memory.
    /// @param[in] inData Pointer to upload bytes.
    /// @param[in] inSizeBytes Payload byte count.
    MockResourceArrayUpload(const void* const inData, const gp::UInt32 inSizeBytes) noexcept
        : m_data(inData)
        , m_sizeBytes(inSizeBytes)
    {}

    /// @copydoc IResourceArrayUpload::getResourceData()
    [[nodiscard]] virtual const void* getResourceData() const noexcept override
    {
        return m_data;
    }

    /// @copydoc IResourceArrayUpload::getResourceDataSize()
    [[nodiscard]] virtual gp::UInt32 getResourceDataSize() const noexcept override
    {
        return m_sizeBytes;
    }

    /// @copydoc IResourceArrayUpload::discard()
    virtual void discard() noexcept override
    {
        ++m_discardCount;
        m_data = nullptr;
        m_sizeBytes = 0u;
    }

    /// @brief Returns the number of times discard() has been invoked.
    /// @return Discard call count.
    [[nodiscard]] gp::UInt32 getDiscardCount() const noexcept
    {
        return m_discardCount;
    }

private:
    const void* m_data{ nullptr };
    gp::UInt32 m_sizeBytes{ 0u };
    gp::UInt32 m_discardCount{ 0u };
};

/// @brief Concrete mock subclass of Buffer exposing protected ownership and state hooks.
class MockBuffer final : public Buffer
{
public:
    static inline gp::thread::Atomic<gp::UInt32> s_constructCount{ 0u };
    static inline gp::thread::Atomic<gp::UInt32> s_destructCount{ 0u };

    /// @brief Resets instance construction and destruction counters.
    static void resetCounters() noexcept
    {
        s_constructCount.store(0u, gp::memory::MemoryOrder::SeqCst);
        s_destructCount.store(0u, gp::memory::MemoryOrder::SeqCst);
    }

    /// @brief Constructs a mock buffer from a creation descriptor.
    /// @param[in] createDesc Creation parameters passed to Buffer.
    explicit MockBuffer(const BufferCreateDesc& createDesc) noexcept
        : Buffer(createDesc)
    {
        s_constructCount.fetchAdd(1u, gp::memory::MemoryOrder::Relaxed);
    }

    /// @copydoc Buffer::~Buffer()
    virtual ~MockBuffer() noexcept override
    {
        s_destructCount.fetchAdd(1u, gp::memory::MemoryOrder::Relaxed);
    }

    /// @brief Forwards a tracked access update through the protected context hook.
    /// @param[in] trackedAccess New tracked access and pipeline state.
    void updateTrackedAccessForTest(const sync::TrackedAccess trackedAccess) noexcept
    {
        setTrackedAccessFromContext(trackedAccess);
    }

    /// @brief Exposes protected Buffer::takeOwnership() to tests.
    /// @param[in,out] other Source buffer to adopt from.
    void takeOwnershipForTest(Buffer& other) noexcept
    {
        takeOwnership(other);
    }

    /// @brief Exposes protected Buffer::releaseOwnership() to tests.
    void releaseOwnershipForTest() noexcept
    {
        releaseOwnership();
    }
};

/// @brief Mock staging buffer using the default StagingBuffer::getGPUSizeBytes() implementation.
class MockDefaultStagingBuffer final : public StagingBuffer
{
public:
    static inline gp::thread::Atomic<gp::UInt32> s_constructCount{ 0u };
    static inline gp::thread::Atomic<gp::UInt32> s_destructCount{ 0u };

    static constexpr gp::UInt32 kCapacityBytes = 64u;

    /// @brief Resets instance construction and destruction counters.
    static void resetCounters() noexcept
    {
        s_constructCount.store(0u, gp::memory::MemoryOrder::SeqCst);
        s_destructCount.store(0u, gp::memory::MemoryOrder::SeqCst);
    }

    /// @brief Constructs an unlocked mock staging buffer populated with deterministic byte values.
    MockDefaultStagingBuffer() noexcept
        : StagingBuffer()
    {
        for (gp::UInt32 i = 0u; i < kCapacityBytes; ++i)
        {
            m_storage[i] = static_cast<gp::UInt8>(i);
        }
        s_constructCount.fetchAdd(1u, gp::memory::MemoryOrder::Relaxed);
    }

    /// @copydoc StagingBuffer::~StagingBuffer()
    virtual ~MockDefaultStagingBuffer() noexcept override
    {
        s_destructCount.fetchAdd(1u, gp::memory::MemoryOrder::Relaxed);
    }

    /// @copydoc StagingBuffer::lock()
    [[nodiscard]] virtual void* lock(const gp::UInt32 offset, const gp::UInt32 numBytes) noexcept override
    {
        if (m_isLocked || offset > kCapacityBytes || numBytes > (kCapacityBytes - offset))
        {
            return nullptr;
        }

        m_isLocked = true;
        return m_storage + offset;
    }

    /// @copydoc StagingBuffer::unlock()
    virtual void unlock() noexcept override
    {
        m_isLocked = false;
    }

private:
    gp::UInt8 m_storage[kCapacityBytes]{};
};

/// @brief Mock staging buffer overriding getGPUSizeBytes() to report an explicit backing size.
class MockSizedStagingBuffer final : public StagingBuffer
{
public:
    /// @brief Constructs a staging buffer reporting a fixed GPU footprint.
    /// @param[in] gpuSizeBytes Size reported by getGPUSizeBytes().
    explicit MockSizedStagingBuffer(const gp::UInt64 gpuSizeBytes) noexcept
        : StagingBuffer()
        , m_gpuSizeBytes(gpuSizeBytes)
    {}

    /// @copydoc StagingBuffer::~StagingBuffer()
    virtual ~MockSizedStagingBuffer() noexcept override = default;

    /// @copydoc StagingBuffer::lock()
    [[nodiscard]] virtual void* lock(const gp::UInt32 offset, const gp::UInt32 numBytes) noexcept override
    {
        if (m_isLocked || offset != 0u || numBytes > sizeof(m_payload))
        {
            return nullptr;
        }

        m_isLocked = true;
        return m_payload;
    }

    /// @copydoc StagingBuffer::unlock()
    virtual void unlock() noexcept override
    {
        m_isLocked = false;
    }

    /// @copydoc StagingBuffer::getGPUSizeBytes()
    [[nodiscard]] virtual gp::UInt64 getGPUSizeBytes() const noexcept override
    {
        return m_gpuSizeBytes;
    }

private:
    gp::UInt64 m_gpuSizeBytes{ 0u };
    gp::UInt8 m_payload[16]{};
};

/// @brief Test fixture flushing deferred RHI deletions around each test case.
class BufferTestFixture : public ::testing::Test
{
protected:
    /// @copydoc ::testing::Test::SetUp()
    virtual void SetUp() override
    {
#if GP_RHI_ENABLE_RESOURCE_INFO
        Resource::stopTrackingAllResources();
#endif
        ResourceDeletionQueue::setBypassDeletionQueueCheckOnExit(false);
        ResourceDeletionQueue::flushPendingDeletes(true);
        MockBuffer::resetCounters();
        MockDefaultStagingBuffer::resetCounters();
    }

    /// @copydoc ::testing::Test::TearDown()
    virtual void TearDown() override
    {
        ResourceDeletionQueue::flushPendingDeletes(true);
#if GP_RHI_ENABLE_RESOURCE_INFO
        Resource::stopTrackingAllResources();
#endif
        ResourceDeletionQueue::setBypassDeletionQueueCheckOnExit(false);
    }
};

}   // namespace

TEST(BufferDescTest, ConstructorsNullEqualityAndHashing)
{
    const BufferDesc defaultDesc{};
    EXPECT_EQ(defaultDesc.size, 0u);
    EXPECT_EQ(defaultDesc.stride, 0u);
    EXPECT_EQ(defaultDesc.usage, BufferUsageFlags::None);
    EXPECT_EQ(defaultDesc.gpuMask, GPUMask::all());
    EXPECT_FALSE(defaultDesc.isNull());

    const BufferDesc nullDesc = BufferDesc::null();
    const BufferDesc explicitNull(0u, 0u, BufferUsageFlags::NullResource);
    EXPECT_EQ(nullDesc.size, 0u);
    EXPECT_EQ(nullDesc.stride, 0u);
    EXPECT_EQ(nullDesc.usage, BufferUsageFlags::NullResource);
    EXPECT_EQ(nullDesc.gpuMask, GPUMask::all());
    EXPECT_TRUE(nullDesc.isNull());
    EXPECT_TRUE(explicitNull.isNull());
    EXPECT_EQ(nullDesc, explicitNull);
    EXPECT_NE(defaultDesc, nullDesc);

    const BufferDesc threeArg(512u, 32u, BufferUsageFlags::StructuredBuffer | BufferUsageFlags::ShaderResource);
    EXPECT_EQ(threeArg.size, 512u);
    EXPECT_EQ(threeArg.stride, 32u);
    EXPECT_EQ(threeArg.usage, BufferUsageFlags::StructuredBuffer | BufferUsageFlags::ShaderResource);
    EXPECT_EQ(threeArg.gpuMask, GPUMask::all());
    EXPECT_FALSE(threeArg.isNull());

    const GPUMask dualGpu = GPUMask::fromIndex(0u) | GPUMask::fromIndex(1u);
    const BufferDesc fourArg(512u, 32u, BufferUsageFlags::StructuredBuffer | BufferUsageFlags::ShaderResource, dualGpu);
    EXPECT_EQ(fourArg.gpuMask, dualGpu);
    EXPECT_NE(threeArg, fourArg);

    const BufferDesc sameAsThreeArg(
        512u, 32u, BufferUsageFlags::StructuredBuffer | BufferUsageFlags::ShaderResource, GPUMask::all()
    );
    const BufferDesc diffSize(1024u, 32u, threeArg.usage);
    const BufferDesc diffStride(512u, 16u, threeArg.usage);
    const BufferDesc diffUsage(512u, 32u, BufferUsageFlags::VertexBuffer);

    EXPECT_EQ(threeArg, sameAsThreeArg);
    EXPECT_NE(threeArg, diffSize);
    EXPECT_NE(threeArg, diffStride);
    EXPECT_NE(threeArg, diffUsage);

    EXPECT_EQ(threeArg.getHash(), sameAsThreeArg.getHash());
    EXPECT_EQ(gp::hashing::hashOf(threeArg), threeArg.getHash());
    EXPECT_NE(threeArg.getHash(), diffSize.getHash());
    EXPECT_NE(threeArg.getHash(), diffStride.getHash());
    EXPECT_NE(threeArg.getHash(), diffUsage.getHash());
    EXPECT_NE(threeArg.getHash(), fourArg.getHash());

    gp::hashing::Fnv1a64 hasherA{};
    threeArg.hashAppend(hasherA);
    gp::hashing::Fnv1a64 hasherB{};
    sameAsThreeArg.hashAppend(hasherB);
    gp::hashing::Fnv1a64 hasherDiffMask{};
    fourArg.hashAppend(hasherDiffMask);
    gp::hashing::Fnv1a64 hasherDiffSize{};
    diffSize.hashAppend(hasherDiffSize);

    EXPECT_EQ(hasherA.finalize(), hasherB.finalize());
    EXPECT_NE(hasherA.finalize(), hasherDiffMask.finalize());
    EXPECT_NE(hasherA.finalize(), hasherDiffSize.finalize());
}

TEST(BufferDescTest, GetDefaultResourceStateCoversAllBranches)
{
    // IndexBuffer derives VertexOrIndexBuffer with or without initial data.
    EXPECT_EQ(getDefaultResourceState(BufferUsageFlags::IndexBuffer, false), sync::ResourceAccess::VertexOrIndexBuffer);
    EXPECT_EQ(getDefaultResourceState(BufferUsageFlags::IndexBuffer, true), sync::ResourceAccess::VertexOrIndexBuffer);

    // VertexBuffer derives VertexOrIndexBuffer | SRVMask with or without initial data.
    const sync::ResourceAccess expectedVertex =
        sync::ResourceAccess::VertexOrIndexBuffer | sync::ResourceAccess::SRVMask;
    EXPECT_EQ(getDefaultResourceState(BufferUsageFlags::VertexBuffer, false), expectedVertex);
    EXPECT_EQ(getDefaultResourceState(BufferUsageFlags::VertexBuffer, true), expectedVertex);

    // StructuredBuffer derives SRVMask when initial data is provided.
    EXPECT_EQ(getDefaultResourceState(BufferUsageFlags::StructuredBuffer, true), sync::ResourceAccess::SRVMask);

    // AccelerationStructure resolves to BVHRead with initial data and BVHWrite without initial data.
    EXPECT_EQ(getDefaultResourceState(BufferUsageFlags::AccelerationStructure, true), sync::ResourceAccess::BVHRead);
    EXPECT_EQ(getDefaultResourceState(BufferUsageFlags::AccelerationStructure, false), sync::ResourceAccess::BVHWrite);

    // UnorderedAccess takes priority over other flags when no initial data is uploaded.
    EXPECT_EQ(getDefaultResourceState(BufferUsageFlags::UnorderedAccess, false), sync::ResourceAccess::UAVMask);
    EXPECT_EQ(getDefaultResourceState(BufferUsageFlags::RayTracingScratch, false), sync::ResourceAccess::UAVMask);
    EXPECT_EQ(
        getDefaultResourceState(BufferUsageFlags::VertexBuffer | BufferUsageFlags::UnorderedAccess, false),
        sync::ResourceAccess::UAVMask
    );
    EXPECT_EQ(
        getDefaultResourceState(BufferUsageFlags::AccelerationStructure | BufferUsageFlags::UnorderedAccess, false),
        sync::ResourceAccess::UAVMask
    );

    // ShaderResource without initial data ORs SRVMask into defaultReadingState.
    EXPECT_EQ(getDefaultResourceState(BufferUsageFlags::ShaderResource, false), sync::ResourceAccess::SRVMask);
    EXPECT_EQ(
        getDefaultResourceState(BufferUsageFlags::StructuredBuffer | BufferUsageFlags::ShaderResource, false),
        sync::ResourceAccess::SRVMask
    );
    EXPECT_EQ(
        getDefaultResourceState(BufferUsageFlags::IndexBuffer | BufferUsageFlags::ShaderResource, false), expectedVertex
    );

    // Combined flags accumulate reading states.
    const sync::ResourceAccess expectedIndexPlusBvh =
        sync::ResourceAccess::VertexOrIndexBuffer | sync::ResourceAccess::BVHRead;
    EXPECT_EQ(
        getDefaultResourceState(BufferUsageFlags::IndexBuffer | BufferUsageFlags::AccelerationStructure, true),
        expectedIndexPlusBvh
    );

    const sync::ResourceAccess expectedAllReadable =
        sync::ResourceAccess::VertexOrIndexBuffer | sync::ResourceAccess::SRVMask | sync::ResourceAccess::BVHRead;
    EXPECT_EQ(
        getDefaultResourceState(
            BufferUsageFlags::VertexBuffer | BufferUsageFlags::StructuredBuffer |
                BufferUsageFlags::AccelerationStructure,
            true
        ),
        expectedAllReadable
    );

    // IndexBuffer combined with StructuredBuffer without initial data keeps defaultReadingState because
    // VertexOrIndexBuffer is set.
    EXPECT_EQ(
        getDefaultResourceState(BufferUsageFlags::IndexBuffer | BufferUsageFlags::StructuredBuffer, false),
        expectedVertex
    );
}

TEST(BufferCreateDescTest, AllSixteenStaticFactoriesAndDirectConstructors)
{
    const BufferCreateDesc emptyDesc{};
    EXPECT_TRUE(emptyDesc.debugName.isEmpty());
    EXPECT_EQ(emptyDesc.size, 0u);
    EXPECT_EQ(emptyDesc.stride, 0u);
    EXPECT_EQ(emptyDesc.usage, BufferUsageFlags::None);
    EXPECT_EQ(emptyDesc.initialData, nullptr);
    EXPECT_EQ(emptyDesc.initialState, sync::ResourceAccess::Unknown);
    EXPECT_EQ(emptyDesc.initAction, BufferInitAction::Default);
    EXPECT_TRUE(emptyDesc.className.isNone());
    EXPECT_TRUE(emptyDesc.ownerName.isNone());

    // 1. create(name, usage)
    const BufferCreateDesc c1 = BufferCreateDesc::create("CreateUsage", BufferUsageFlags::DrawIndirect);
    EXPECT_EQ(c1.debugName, gp::StringView("CreateUsage"));
    EXPECT_EQ(c1.size, 0u);
    EXPECT_EQ(c1.stride, 0u);
    EXPECT_EQ(c1.usage, BufferUsageFlags::DrawIndirect);

    // 2. create(name, size, stride, usage)
    const BufferCreateDesc c2 = BufferCreateDesc::create("CreateSized", 256u, 16u, BufferUsageFlags::ShaderResource);
    EXPECT_EQ(c2.debugName, gp::StringView("CreateSized"));
    EXPECT_EQ(c2.size, 256u);
    EXPECT_EQ(c2.stride, 16u);
    EXPECT_EQ(c2.usage, BufferUsageFlags::ShaderResource);

    // 3. create(name, BufferDesc)
    const BufferDesc baseDesc(1024u, 64u, BufferUsageFlags::UnorderedAccess, GPUMask::fromIndex(1u));
    const BufferCreateDesc c3 = BufferCreateDesc::create("CreateFromDesc", baseDesc);
    EXPECT_EQ(c3.debugName, gp::StringView("CreateFromDesc"));
    EXPECT_EQ(c3.size, 1024u);
    EXPECT_EQ(c3.stride, 64u);
    EXPECT_EQ(c3.usage, BufferUsageFlags::UnorderedAccess);
    EXPECT_EQ(c3.gpuMask, GPUMask::fromIndex(1u));

    // 4. createNull(name)
    const BufferCreateDesc cNull = BufferCreateDesc::createNull("NullPlaceholder");
    EXPECT_EQ(cNull.debugName, gp::StringView("NullPlaceholder"));
    EXPECT_TRUE(cNull.isNull());
    EXPECT_EQ(cNull.size, 0u);
    EXPECT_EQ(cNull.stride, 0u);
    EXPECT_EQ(cNull.usage, BufferUsageFlags::NullResource);

    // 5-7. createVertex overloads
    const BufferCreateDesc cVert1 = BufferCreateDesc::createVertex("VertEmpty");
    EXPECT_EQ(cVert1.debugName, gp::StringView("VertEmpty"));
    EXPECT_EQ(cVert1.size, 0u);
    EXPECT_EQ(cVert1.stride, 0u);
    EXPECT_EQ(cVert1.usage, BufferUsageFlags::VertexBuffer);

    const BufferCreateDesc cVert2 = BufferCreateDesc::createVertex("VertSized", 4096u);
    EXPECT_EQ(cVert2.size, 4096u);
    EXPECT_EQ(cVert2.stride, 0u);
    EXPECT_EQ(cVert2.usage, BufferUsageFlags::VertexBuffer);

    const BufferCreateDesc cVert3 = BufferCreateDesc::createVertex<TestVertex>("VertTyped", 100u);
    EXPECT_EQ(cVert3.size, 100u * static_cast<gp::UInt32>(sizeof(TestVertex)));
    EXPECT_EQ(cVert3.stride, 0u);
    EXPECT_EQ(cVert3.usage, BufferUsageFlags::VertexBuffer);

    // 8-10. createIndex overloads
    const BufferCreateDesc cIdx1 = BufferCreateDesc::createIndex("IdxEmpty");
    EXPECT_EQ(cIdx1.size, 0u);
    EXPECT_EQ(cIdx1.stride, 0u);
    EXPECT_EQ(cIdx1.usage, BufferUsageFlags::IndexBuffer);

    const BufferCreateDesc cIdx2 = BufferCreateDesc::createIndex("IdxSized", 1200u, 2u);
    EXPECT_EQ(cIdx2.size, 1200u);
    EXPECT_EQ(cIdx2.stride, 2u);
    EXPECT_EQ(cIdx2.usage, BufferUsageFlags::IndexBuffer);

    const BufferCreateDesc cIdx3U16 = BufferCreateDesc::createIndex<gp::UInt16>("IdxU16", 300u);
    EXPECT_EQ(cIdx3U16.size, 600u);
    EXPECT_EQ(cIdx3U16.stride, 2u);
    EXPECT_EQ(cIdx3U16.usage, BufferUsageFlags::IndexBuffer);

    const BufferCreateDesc cIdx3U32 = BufferCreateDesc::createIndex<gp::UInt32>("IdxU32", 300u);
    EXPECT_EQ(cIdx3U32.size, 1200u);
    EXPECT_EQ(cIdx3U32.stride, 4u);
    EXPECT_EQ(cIdx3U32.usage, BufferUsageFlags::IndexBuffer);

    // 11-13. createStructured overloads
    const BufferCreateDesc cStruct1 = BufferCreateDesc::createStructured("StructEmpty");
    EXPECT_EQ(cStruct1.size, 0u);
    EXPECT_EQ(cStruct1.stride, 0u);
    EXPECT_EQ(cStruct1.usage, BufferUsageFlags::StructuredBuffer);

    const BufferCreateDesc cStruct2 = BufferCreateDesc::createStructured("StructSized", 512u, 32u);
    EXPECT_EQ(cStruct2.size, 512u);
    EXPECT_EQ(cStruct2.stride, 32u);
    EXPECT_EQ(cStruct2.usage, BufferUsageFlags::StructuredBuffer);

    const BufferCreateDesc cStruct3 = BufferCreateDesc::createStructured<TestStructuredElement>("StructTyped", 16u);
    EXPECT_EQ(cStruct3.size, 16u * static_cast<gp::UInt32>(sizeof(TestStructuredElement)));
    EXPECT_EQ(cStruct3.stride, static_cast<gp::UInt32>(sizeof(TestStructuredElement)));
    EXPECT_EQ(cStruct3.usage, BufferUsageFlags::StructuredBuffer);

    // 14-15. createByteAddress overloads
    const BufferCreateDesc cByte1 = BufferCreateDesc::createByteAddress("RawEmpty");
    EXPECT_EQ(cByte1.size, 0u);
    EXPECT_EQ(cByte1.stride, 0u);
    EXPECT_EQ(cByte1.usage, BufferUsageFlags::ByteAddressBuffer);

    const BufferCreateDesc cByte2 = BufferCreateDesc::createByteAddress("RawSized", 2048u, 4u);
    EXPECT_EQ(cByte2.size, 2048u);
    EXPECT_EQ(cByte2.stride, 4u);
    EXPECT_EQ(cByte2.usage, BufferUsageFlags::ByteAddressBuffer);

    // 16. createUniform
    const BufferCreateDesc cUniform = BufferCreateDesc::createUniform("UniformBuf");
    EXPECT_EQ(cUniform.debugName, gp::StringView("UniformBuf"));
    EXPECT_EQ(cUniform.size, 0u);
    EXPECT_EQ(cUniform.stride, 0u);
    EXPECT_EQ(cUniform.usage, BufferUsageFlags::UniformBuffer);
}

TEST(BufferCreateDescTest, FluentBuildersInitActionsAndTraceClassName)
{
    const gp::UInt32 rawPayload[4]{ 10u, 20u, 30u, 40u };
    ResourceArrayUploadView uploadView(rawPayload, static_cast<gp::UInt32>(sizeof(rawPayload)));

    const GPUMask gpu1 = GPUMask::fromIndex(1u);
    BufferCreateDesc desc = BufferCreateDesc::createVertex("InitialName");
    desc.setSize(64u)
        .setStride(16u)
        .setUsage(BufferUsageFlags::StructuredBuffer)
        .addUsage(BufferUsageFlags::ShaderResource | BufferUsageFlags::Dynamic)
        .setGPUMask(gpu1)
        .setDebugName("UpdatedName")
        .setClassName(gp::Name("InstanceBuffer"))
        .setOwnerName(gp::Name("MeshBatcher"))
        .setInitActionZeroData();

    EXPECT_EQ(desc.size, 64u);
    EXPECT_EQ(desc.stride, 16u);
    EXPECT_EQ(
        desc.usage, BufferUsageFlags::StructuredBuffer | BufferUsageFlags::ShaderResource | BufferUsageFlags::Dynamic
    );
    EXPECT_EQ(desc.gpuMask, gpu1);
    EXPECT_EQ(desc.debugName, gp::StringView("UpdatedName"));
    EXPECT_EQ(desc.className, gp::Name("InstanceBuffer"));
    EXPECT_EQ(desc.ownerName, gp::Name("MeshBatcher"));
    EXPECT_EQ(desc.initAction, BufferInitAction::Zeroed);
    EXPECT_EQ(desc.getTraceClassName(), gp::Name("InstanceBuffer"));

    // Default trace class name falls back to "Buffer" when className is unset.
    const BufferCreateDesc unnamedClass = BufferCreateDesc::createVertex("DefaultClass");
    EXPECT_EQ(unnamedClass.getTraceClassName(), gp::Name("Buffer"));

    // Init action helpers switch cleanly between all modes.
    desc.setInitActionInitializer();
    EXPECT_EQ(desc.initAction, BufferInitAction::Initializer);

    desc.setInitActionResourceArray(&uploadView);
    EXPECT_EQ(desc.initAction, BufferInitAction::ResourceArray);
    EXPECT_EQ(desc.initialData, &uploadView);

    desc.setInitActionNone();
    EXPECT_EQ(desc.initAction, BufferInitAction::Default);
    EXPECT_EQ(desc.initialData, nullptr);

    desc.setInitActionResourceArray(&uploadView);
    desc.setInitActionZeroData();
    EXPECT_EQ(desc.initAction, BufferInitAction::Zeroed);
    EXPECT_EQ(desc.initialData, nullptr);

    desc.setInitActionResourceArray(&uploadView);
    desc.setInitActionInitializer();
    EXPECT_EQ(desc.initAction, BufferInitAction::Initializer);
    EXPECT_EQ(desc.initialData, nullptr);

    desc.setInitAction(BufferInitAction::Zeroed);
    EXPECT_EQ(desc.initAction, BufferInitAction::Zeroed);

    // Verify NNE and AnyDynamic usage flags.
    EXPECT_NE(BufferUsageFlags::NNE, BufferUsageFlags::None);
    EXPECT_EQ(BufferUsageFlags::AnyDynamic, BufferUsageFlags::Dynamic | BufferUsageFlags::Volatile);

    // determineInitialState resolves Unknown from usage and preserves explicit states.
    BufferCreateDesc autoState = BufferCreateDesc::createIndex("AutoIndex", 64u, 2u);
    EXPECT_EQ(autoState.initialState, sync::ResourceAccess::Unknown);
    autoState.determineInitialState();
    EXPECT_EQ(autoState.initialState, sync::ResourceAccess::VertexOrIndexBuffer);

    BufferCreateDesc explicitState = BufferCreateDesc::createIndex("ExplicitIndex", 64u, 2u);
    explicitState.setInitialState(sync::ResourceAccess::CopyDest);
    explicitState.determineInitialState();
    EXPECT_EQ(explicitState.initialState, sync::ResourceAccess::CopyDest);
}

TEST(BufferCreateDescTest, ResourceArrayUploadViewAndInterface)
{
    const gp::UInt32 values[5]{ 1u, 2u, 3u, 4u, 5u };

    // Raw pointer constructor exposes exact pointer and byte length.
    ResourceArrayUploadView rawView(values, static_cast<gp::UInt32>(sizeof(values)));
    EXPECT_EQ(rawView.getResourceData(), static_cast<const void*>(values));
    EXPECT_EQ(rawView.getResourceDataSize(), 20u);
    rawView.discard();
    EXPECT_EQ(rawView.getResourceData(), static_cast<const void*>(values));
    EXPECT_EQ(rawView.getResourceDataSize(), 20u);

    // Typed ConstVectorView constructor computes byte size from element count and element size.
    const gp::ConstVectorView<gp::UInt32> span(values, 5u);
    ResourceArrayUploadView typedView(span);
    EXPECT_EQ(typedView.getResourceData(), static_cast<const void*>(values));
    EXPECT_EQ(typedView.getResourceDataSize(), static_cast<gp::UInt32>(5u * sizeof(gp::UInt32)));

    // Polymorphic IResourceArrayUpload discard clears mock storage.
    MockResourceArrayUpload mockUpload(values, static_cast<gp::UInt32>(sizeof(values)));
    IResourceArrayUpload* const iface = &mockUpload;
    EXPECT_EQ(iface->getResourceData(), static_cast<const void*>(values));
    EXPECT_EQ(iface->getResourceDataSize(), 20u);
    iface->discard();
    EXPECT_EQ(mockUpload.getDiscardCount(), 1u);
    EXPECT_EQ(iface->getResourceData(), nullptr);
    EXPECT_EQ(iface->getResourceDataSize(), 0u);
}

TEST_F(BufferTestFixture, BufferPropertiesOwnershipTransferAndDeferredDeletion)
{
    BufferCreateDesc srcDesc = BufferCreateDesc::createStructured("SourceStructured", 512u, 32u);
    srcDesc.addUsage(BufferUsageFlags::ShaderResource)
        .setGPUMask(GPUMask::fromIndex(0u))
        .setInitialState(sync::ResourceAccess::SRVCompute)
        .setOwnerName(gp::Name("ParticleSystem"));

    BufferCreateDesc dstDesc = BufferCreateDesc::createNull("StreamingSlot");
    dstDesc.setOwnerName(gp::Name("MeshStreamer"));

    MockBuffer* const rawSrc = new MockBuffer(srcDesc);
    MockBuffer* const rawDst = new MockBuffer(dstDesc);
    BufferRef srcRef(rawSrc);
    BufferRef dstRef(rawDst);

    EXPECT_EQ(srcRef->getType(), ResourceType::Buffer);
    EXPECT_EQ(srcRef->getSize(), 512u);
    EXPECT_EQ(srcRef->getStride(), 32u);
    EXPECT_EQ(srcRef->getUsage(), BufferUsageFlags::StructuredBuffer | BufferUsageFlags::ShaderResource);
    const BufferDesc expectedSrcDesc = static_cast<const BufferDesc&>(srcDesc);
    EXPECT_EQ(srcRef->getDesc(), expectedSrcDesc);
    EXPECT_EQ(srcRef->getTrackedAccessUnsafe(), sync::ResourceAccess::SRVCompute);
    EXPECT_EQ(srcRef->getTrackedPipelinesUnsafe(), sync::PipelineStage::Graphics);

#if GP_RHI_ENABLE_RESOURCE_INFO
    EXPECT_EQ(srcRef->getName(), gp::Name("SourceStructured"));
    EXPECT_EQ(srcRef->getOwnerName(), gp::Name("ParticleSystem"));
#else
    EXPECT_TRUE(srcRef->getName().isNone());
    EXPECT_TRUE(srcRef->getOwnerName().isNone());
#endif

    srcRef->setName(gp::Name("RenamedStructured"));
    EXPECT_EQ(srcRef->getName(), gp::Name("RenamedStructured"));

    // Update tracked state on source before transferring ownership to destination.
    const sync::TrackedAccess uavCompute(sync::ResourceAccess::UAVCompute, sync::PipelineStage::AsyncCompute);
    rawSrc->updateTrackedAccessForTest(uavCompute);

    rawDst->takeOwnershipForTest(*rawSrc);
    EXPECT_EQ(dstRef->getDesc(), expectedSrcDesc);
    EXPECT_EQ(dstRef->getSize(), 512u);
    EXPECT_EQ(dstRef->getStride(), 32u);
    EXPECT_EQ(dstRef->getUsage(), srcDesc.usage);
    EXPECT_EQ(dstRef->getTrackedAccessUnsafe(), sync::ResourceAccess::UAVCompute);
    EXPECT_EQ(dstRef->getTrackedPipelinesUnsafe(), sync::PipelineStage::AsyncCompute);

    rawSrc->releaseOwnershipForTest();
    const BufferDesc expectedNullDesc = BufferDesc::null();
    EXPECT_EQ(srcRef->getDesc(), expectedNullDesc);
    EXPECT_TRUE(srcRef->getDesc().isNull());
    EXPECT_EQ(srcRef->getSize(), 0u);
    EXPECT_EQ(srcRef->getStride(), 0u);
    EXPECT_EQ(srcRef->getUsage(), BufferUsageFlags::NullResource);
    EXPECT_EQ(srcRef->getTrackedAccessUnsafe(), sync::ResourceAccess::Unknown);
    EXPECT_EQ(srcRef->getTrackedPipelinesUnsafe(), sync::PipelineStage::Graphics);

    // Releasing both references enqueues deferred deletion without inline destruction.
    srcRef.reset();
    dstRef.reset();
    EXPECT_EQ(MockBuffer::s_destructCount.load(), 0u);

    ResourceDeletionQueue::flushPendingDeletes(true);
    EXPECT_EQ(MockBuffer::s_constructCount.load(), 2u);
    EXPECT_EQ(MockBuffer::s_destructCount.load(), 2u);
}

TEST_F(BufferTestFixture, StagingBufferLockUnlockSizeAndDeferredDeletion)
{
    StagingBufferRef defaultStaging(new MockDefaultStagingBuffer());
    EXPECT_EQ(defaultStaging->getType(), ResourceType::StagingBuffer);
    EXPECT_FALSE(defaultStaging->isLocked());
    EXPECT_EQ(defaultStaging->getGPUSizeBytes(), 0u);

    // Out-of-bounds lock fails and leaves the buffer unlocked.
    EXPECT_EQ(defaultStaging->lock(MockDefaultStagingBuffer::kCapacityBytes, 4u), nullptr);
    EXPECT_FALSE(defaultStaging->isLocked());

    // Valid lock maps the requested offset and marks the staging buffer locked.
    void* const mapped = defaultStaging->lock(8u, 16u);
    ASSERT_NE(mapped, nullptr);
    EXPECT_TRUE(defaultStaging->isLocked());

    const gp::UInt8* const bytes = static_cast<const gp::UInt8*>(mapped);
    for (gp::UInt32 i = 0u; i < 16u; ++i)
    {
        EXPECT_EQ(bytes[i], static_cast<gp::UInt8>(8u + i));
    }

    // Re-locking while already locked returns nullptr.
    EXPECT_EQ(defaultStaging->lock(0u, 4u), nullptr);

    defaultStaging->unlock();
    EXPECT_FALSE(defaultStaging->isLocked());

    // Subclass override reports explicit GPU allocation size.
    StagingBufferRef sizedStaging(new MockSizedStagingBuffer(65536u));
    EXPECT_EQ(sizedStaging->getType(), ResourceType::StagingBuffer);
    EXPECT_EQ(sizedStaging->getGPUSizeBytes(), 65536u);

    // Releasing references defers destruction until the deletion queue flushes.
    defaultStaging.reset();
    EXPECT_EQ(MockDefaultStagingBuffer::s_destructCount.load(), 0u);

    ResourceDeletionQueue::flushPendingDeletes(true);
    EXPECT_EQ(MockDefaultStagingBuffer::s_constructCount.load(), 1u);
    EXPECT_EQ(MockDefaultStagingBuffer::s_destructCount.load(), 1u);
}

TEST_F(BufferTestFixture, StreamSourceSlotLifecycleUpdateAndCascadingDeferredDeletion)
{
    StreamSourceSlotRef emptySlot = StreamSourceSlot::create(nullptr);
    ASSERT_TRUE(emptySlot.isValid());
    EXPECT_EQ(emptySlot->getType(), ResourceType::StreamSourceSlot);
    EXPECT_EQ(emptySlot->getBuffer(), nullptr);
    EXPECT_EQ(emptySlot->getOffset(), 0u);

    BufferCreateDesc descA = BufferCreateDesc::createVertex("StreamSlotA", 128u).determineInitialState();
    BufferCreateDesc descB = BufferCreateDesc::createVertex("StreamSlotB", 256u).determineInitialState();
    BufferRef bufA(new MockBuffer(descA));
    BufferRef bufB(new MockBuffer(descB));

    StreamSourceSlotRef slot = StreamSourceSlot::create(bufA.get());
    EXPECT_EQ(slot->getBuffer(), bufA.get());
    EXPECT_EQ(slot->getOffset(), 0u);
    EXPECT_EQ(bufA->getRefCount(), 2u);

    // Release caller ref to bufA; slot keeps bufA alive.
    bufA.reset();
    ResourceDeletionQueue::flushPendingDeletes(true);
    EXPECT_EQ(MockBuffer::s_destructCount.load(), 0u);

    // Updating slot to bufB queues bufA for deferred deletion and stores offset.
    slot->update(bufB.get(), 64u);
    EXPECT_EQ(slot->getBuffer(), bufB.get());
    EXPECT_EQ(slot->getOffset(), 64u);
    EXPECT_EQ(bufB->getRefCount(), 2u);

    ResourceDeletionQueue::flushPendingDeletes(true);
    EXPECT_EQ(MockBuffer::s_destructCount.load(), 1u);

    bufB.reset();
    slot.reset();
    emptySlot.reset();
    ResourceDeletionQueue::flushPendingDeletes(true);
    EXPECT_EQ(MockBuffer::s_destructCount.load(), 2u);
}

}   // namespace gp::rhi::resource::tests
