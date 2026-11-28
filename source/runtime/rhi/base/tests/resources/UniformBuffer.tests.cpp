// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "resources/UniformBuffer.hpp"
#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "core/Resource.hpp"
#include "core/ResourceDeletionQueue.hpp"
#include "core/ResourceEnums.hpp"
#include "CoreMinimal.hpp"
#include "memory/Memory.hpp"
#include "memory/MemoryOrder.hpp"
#include "memory/pointers/PointerConcepts.hpp"
#include "memory/pointers/RefCountedPtr.hpp"
#include "resources/UniformBufferEnums.hpp"
#include "resources/UniformBufferLayout.hpp"
#include "resources/UniformBufferLayoutDesc.hpp"
#include "RHIBase.hpp"
#include "RHIDefinitions.hpp"
#include "shaders/ShaderEnums.hpp"
#include "threading/atomics/Atomic.hpp"
#include "views/DescriptorHandle.hpp"
#include "views/ViewEnums.hpp"
#include <gtest/gtest.h>
#include <type_traits>

namespace gp::rhi::resource::tests
{

static_assert(gp::concepts::IsRefCounted<UniformBufferLayout>, "UniformBufferLayout must satisfy IsRefCounted.");
static_assert(gp::concepts::IsRefCounted<UniformBuffer>, "UniformBuffer must satisfy IsRefCounted.");
static_assert(
    sizeof(UniformBufferLayoutRef) == sizeof(UniformBufferLayout*),
    "UniformBufferLayoutRef must remain a single pointer."
);
static_assert(
    sizeof(UniformBufferLayoutConstRef) == sizeof(const UniformBufferLayout*),
    "UniformBufferLayoutConstRef must remain a single pointer."
);
static_assert(sizeof(UniformBufferRef) == sizeof(UniformBuffer*), "UniformBufferRef must remain a single pointer.");
static_assert(
    !std::is_copy_constructible_v<UniformBufferLayout>, "UniformBufferLayout must not be copy constructible."
);
static_assert(!std::is_copy_constructible_v<UniformBuffer>, "UniformBuffer must not be copy constructible.");

namespace
{

/// @brief Concrete UniformBufferLayout subclass tracking destructor calls.
class MockTrackedLayout final : public UniformBufferLayout
{
public:
    static inline gp::thread::Atomic<gp::UInt32> s_destructCount{ 0u };

    /// @brief Constructs a tracked layout from a descriptor.
    /// @param[in] desc Layout descriptor.
    explicit MockTrackedLayout(const UniformBufferLayoutDesc& desc) noexcept
        : UniformBufferLayout(desc)
    {}

    /// @copydoc UniformBufferLayout::~UniformBufferLayout()
    virtual ~MockTrackedLayout() noexcept override
    {
        s_destructCount.fetchAdd(1u, gp::memory::MemoryOrder::Relaxed);
    }
};

/// @brief Minimal Resource subclass tracking destructor calls when retained by a uniform buffer.
class MockBoundResource final : public Resource
{
public:
    static inline gp::thread::Atomic<gp::UInt32> s_destructCount{ 0u };

    /// @brief Constructs a mock bound sampler resource.
    MockBoundResource() noexcept
        : Resource(ResourceType::SamplerState)
    {}

    /// @copydoc Resource::~Resource()
    virtual ~MockBoundResource() noexcept override
    {
        s_destructCount.fetchAdd(1u, gp::memory::MemoryOrder::Relaxed);
    }
};

/// @brief Concrete UniformBuffer subclass exposing resource-table mutation and custom bindless handles.
class MockUniformBuffer final : public UniformBuffer
{
public:
    static inline gp::thread::Atomic<gp::UInt32> s_destructCount{ 0u };

    /// @brief Constructs a mock uniform buffer bound to `layout`.
    /// @param[in] layout Immutable layout descriptor.
    /// @param[in] bindlessHandle Optional bindless descriptor handle.
    MockUniformBuffer(
        const UniformBufferLayout* const layout, const view::DescriptorHandle bindlessHandle = view::DescriptorHandle{}
    ) noexcept
        : UniformBuffer(layout)
        , m_bindlessHandle(bindlessHandle)
    {}

    /// @copydoc UniformBuffer::~UniformBuffer()
    virtual ~MockUniformBuffer() noexcept override
    {
        s_destructCount.fetchAdd(1u, gp::memory::MemoryOrder::Relaxed);
    }

    /// @brief Appends a resource reference into the uniform buffer's internal resource table.
    /// @param[in] resource Resource to retain.
    void addResourceEntry(Resource* const resource)
    {
        m_resourceTable.emplaceBack(resource);
    }

    /// @copydoc UniformBuffer::getBindlessHandle()
    [[nodiscard]] virtual view::DescriptorHandle getBindlessHandle() const noexcept override
    {
        return m_bindlessHandle;
    }

private:
    view::DescriptorHandle m_bindlessHandle{};
};

/// @brief Concrete UniformBuffer subclass inheriting default getBindlessHandle() behavior.
class MockDefaultUniformBuffer final : public UniformBuffer
{
public:
    /// @brief Constructs a default uniform buffer bound to `layout`.
    /// @param[in] layout Immutable layout descriptor.
    explicit MockDefaultUniformBuffer(const UniformBufferLayout* const layout) noexcept
        : UniformBuffer(layout)
    {}

    /// @copydoc UniformBuffer::~UniformBuffer()
    virtual ~MockDefaultUniformBuffer() noexcept override = default;
};

class UniformBufferTestFixture : public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        ResourceDeletionQueue::flushPendingDeletes(true);
#if GP_RHI_INCLUDE_SHADER_DEBUG_DATA
        uniformBufferContentMap::setEnabled(false);
#endif
        MockTrackedLayout::s_destructCount.store(0u, gp::memory::MemoryOrder::SeqCst);
        MockBoundResource::s_destructCount.store(0u, gp::memory::MemoryOrder::SeqCst);
        MockUniformBuffer::s_destructCount.store(0u, gp::memory::MemoryOrder::SeqCst);
    }

    virtual void TearDown() override
    {
        ResourceDeletionQueue::flushPendingDeletes(true);
#if GP_RHI_INCLUDE_SHADER_DEBUG_DATA
        uniformBufferContentMap::setEnabled(false);
#endif
    }
};

}   // namespace

TEST_F(UniformBufferTestFixture, LayoutDescDefaultsHashFormulaAndCopy)
{
    UniformBufferLayoutDesc nameOnly("DeferredInit");
    EXPECT_EQ(nameOnly.getDebugName(), gp::StringView("DeferredInit"));
    EXPECT_EQ(nameOnly.hash, 0u);

    UniformBufferLayoutDesc descA("ViewParameters", 256u);
    EXPECT_EQ(descA.getDebugName(), gp::StringView("ViewParameters"));
    EXPECT_EQ(descA.constantBufferSize, 256u);
    EXPECT_NE(descA.getHash(), 0u);
    EXPECT_EQ(descA.renderTargetsOffset, kUniformBufferInvalidOffset);
    EXPECT_EQ(descA.staticSlot, kMaxUniformBufferStaticSlots);
    EXPECT_EQ(descA.bindingFlags, shader::UniformBufferBindingFlags::Shader);
    EXPECT_EQ(descA.flags, UniformBufferFlags::None);
    EXPECT_FALSE(descA.hasRenderTargets());
    EXPECT_FALSE(descA.hasExternalOutputs());
    EXPECT_FALSE(descA.hasStaticSlot());

    // Populate 7 resources to cover 4-wide, 2-wide, and 1-wide hash packing loops.
    descA.resources.emplaceBack(UniformBufferResource{ 0u, shader::UniformBufferBaseType::Texture });
    descA.resources.emplaceBack(UniformBufferResource{ 8u, shader::UniformBufferBaseType::Sampler });
    descA.resources.emplaceBack(UniformBufferResource{ 16u, shader::UniformBufferBaseType::SRV });
    descA.resources.emplaceBack(UniformBufferResource{ 24u, shader::UniformBufferBaseType::UAV });
    descA.resources.emplaceBack(UniformBufferResource{ 32u, shader::UniformBufferBaseType::ResourceCollection });
    descA.resources.emplaceBack(UniformBufferResource{ 40u, shader::UniformBufferBaseType::DescriptorRange });
    descA.resources.emplaceBack(UniformBufferResource{ 48u, shader::UniformBufferBaseType::UniformBlock });
    const gp::UInt32 hashBase = descA.computeHash();
    EXPECT_NE(hashBase, 0u);
    EXPECT_EQ(descA.getHash(), hashBase);
    EXPECT_EQ(UniformBufferLayoutDesc::computeHashValue(descA), hashBase);

    // Verify hasRenderTargets() and hasExternalOutputs() true branches on UniformBufferLayoutDesc.
    UniformBufferLayoutDesc descFlagsTest("FlagsCheck", 64u);
    descFlagsTest.renderTargetsOffset = 16u;
    descFlagsTest.flags = UniformBufferFlags::HasNonGraphOutputs;
    EXPECT_TRUE(descFlagsTest.hasRenderTargets());
    EXPECT_TRUE(descFlagsTest.hasExternalOutputs());

    // Verify UniformBufferUsage and UniformBufferValidation enumerators are distinct.
    EXPECT_NE(UniformBufferUsage::SingleDraw, UniformBufferUsage::SingleFrame);
    EXPECT_NE(UniformBufferUsage::SingleFrame, UniformBufferUsage::MultiFrame);
    EXPECT_NE(UniformBufferValidation::None, UniformBufferValidation::ValidateResources);

    UniformBufferLayoutDesc descB{};
    descB.copyFrom(descA);
    EXPECT_EQ(descA, descB);
    EXPECT_EQ(descB.computeHash(), hashBase);

    // Changing constantBufferSize changes the hash.
    UniformBufferLayoutDesc diffSize{};
    diffSize.copyFrom(descA);
    diffSize.constantBufferSize = 512u;
    EXPECT_NE(diffSize.computeHash(), hashBase);

    // Static slot presence changes bit 0 of the hash, while distinct valid slot indices hash identically.
    UniformBufferLayoutDesc staticSlot3{};
    staticSlot3.copyFrom(descA);
    staticSlot3.staticSlot = 3u;
    EXPECT_TRUE(staticSlot3.hasStaticSlot());
    const gp::UInt32 hashSlot3 = staticSlot3.computeHash();
    EXPECT_NE(hashSlot3, hashBase);

    UniformBufferLayoutDesc staticSlot9{};
    staticSlot9.copyFrom(descA);
    staticSlot9.staticSlot = 9u;
    EXPECT_EQ(staticSlot9.computeHash(), hashSlot3);
    EXPECT_NE(staticSlot3, staticSlot9);

    // NoEmulatedUniformBuffer and NeedsReflectedMembers each perturb the hash.
    UniformBufferLayoutDesc noEmulated{};
    noEmulated.copyFrom(descA);
    noEmulated.flags = UniformBufferFlags::NoEmulatedUniformBuffer;
    EXPECT_NE(noEmulated.computeHash(), hashBase);

    UniformBufferLayoutDesc reflected{};
    reflected.copyFrom(descA);
    reflected.flags = UniformBufferFlags::NeedsReflectedMembers;
    EXPECT_NE(reflected.computeHash(), hashBase);
    EXPECT_NE(reflected.getHash(), noEmulated.getHash());

    // Changing resource member type or name changes the hash.
    UniformBufferLayoutDesc diffResourceType{};
    diffResourceType.copyFrom(descA);
    diffResourceType.resources[1].memberType = shader::UniformBufferBaseType::SRV;
    EXPECT_NE(diffResourceType.computeHash(), hashBase);

    UniformBufferLayoutDesc diffName{};
    diffName.copyFrom(descA);
    diffName.name = "SceneParameters";
    EXPECT_NE(diffName.computeHash(), hashBase);
    EXPECT_EQ(diffName, descA);
}

TEST_F(UniformBufferTestFixture, ImmutableLayoutExposesAllFieldsAndComputesMissingHash)
{
    UniformBufferLayoutDesc desc("LightingPassParams");
    desc.constantBufferSize = 128u;
    desc.staticSlot = 2u;
    desc.bindingFlags = shader::UniformBufferBindingFlags::StaticAndShader;
    desc.flags = UniformBufferFlags::BindlessAccessible | UniformBufferFlags::HasNonGraphOutputs;
    desc.renderTargetsOffset = 32u;
    desc.resources.emplaceBack(UniformBufferResource{ 0u, shader::UniformBufferBaseType::Texture });
    desc.resources.emplaceBack(UniformBufferResource{ 8u, shader::UniformBufferBaseType::Sampler });
    desc.graphResources.emplaceBack(UniformBufferResource{ 0u, shader::UniformBufferBaseType::Texture });
    desc.graphTextures.emplaceBack(UniformBufferResource{ 0u, shader::UniformBufferBaseType::Texture });
    desc.graphBuffers.emplaceBack(UniformBufferResource{ 16u, shader::UniformBufferBaseType::SRV });
    desc.graphUniformBuffers.emplaceBack(UniformBufferResource{ 24u, shader::UniformBufferBaseType::ReferencedStruct });
    desc.uniformBuffers.emplaceBack(UniformBufferResource{ 32u, shader::UniformBufferBaseType::ReferencedStruct });

    // Leave desc.hash == 0 so UniformBufferLayout computes it automatically.
    UniformBufferLayoutConstRef layoutA(new UniformBufferLayout(desc));
    desc.computeHash();
    UniformBufferLayoutConstRef layoutB(new UniformBufferLayout(desc));

    EXPECT_EQ(layoutA->getType(), ResourceType::UniformBufferLayout);
    EXPECT_EQ(layoutA->getDebugName(), gp::StringView("LightingPassParams"));
    EXPECT_EQ(layoutA->getConstantBufferSize(), 128u);
    EXPECT_EQ(layoutA->getRenderTargetsOffset(), 32u);
    EXPECT_EQ(layoutA->getStaticSlot(), 2u);
    EXPECT_EQ(layoutA->getBindingFlags(), shader::UniformBufferBindingFlags::StaticAndShader);
    EXPECT_EQ(layoutA->getFlags(), UniformBufferFlags::BindlessAccessible | UniformBufferFlags::HasNonGraphOutputs);
    EXPECT_TRUE(layoutA->hasRenderTargets());
    EXPECT_TRUE(layoutA->hasExternalOutputs());
    EXPECT_TRUE(layoutA->hasStaticSlot());
    EXPECT_EQ(layoutA->getResources().size(), 2u);
    EXPECT_EQ(layoutA->getGraphResources().size(), 1u);
    EXPECT_EQ(layoutA->getGraphTextures().size(), 1u);
    EXPECT_EQ(layoutA->getGraphBuffers().size(), 1u);
    EXPECT_EQ(layoutA->getGraphUniformBuffers().size(), 1u);
    EXPECT_EQ(layoutA->getUniformBuffers().size(), 1u);
    EXPECT_EQ(layoutA->getHash(), desc.getHash());
    EXPECT_EQ(*layoutA, *layoutB);

    UniformBufferLayoutDesc differentDesc("OtherParams", 64u);
    UniformBufferLayoutConstRef layoutC(new UniformBufferLayout(differentDesc));
    EXPECT_NE(*layoutA, *layoutC);
}

TEST_F(UniformBufferTestFixture, UniformBufferExposesLayoutSizeResourcesAndBindlessHandle)
{
    UniformBufferLayoutDesc desc("MaterialParams", 192u);
    desc.resources.emplaceBack(UniformBufferResource{ 0u, shader::UniformBufferBaseType::Sampler });
    desc.computeHash();
    UniformBufferLayoutConstRef layout(new UniformBufferLayout(desc));

    UniformBufferRef defaultBuffer(new MockDefaultUniformBuffer(layout.get()));
    EXPECT_EQ(defaultBuffer->getType(), ResourceType::UniformBuffer);
    EXPECT_EQ(defaultBuffer->getSize(), 192u);
    EXPECT_EQ(defaultBuffer->getLayoutPtr(), layout.get());
    EXPECT_EQ(&defaultBuffer->getLayout(), layout.get());
    EXPECT_TRUE(defaultBuffer->getResourceTable().isEmpty());
    EXPECT_FALSE(defaultBuffer->getBindlessHandle().isValid());

    const view::DescriptorHandle bindless(view::DescriptorType::BufferSRV, 123u);
    MockUniformBuffer* rawMock = new MockUniformBuffer(layout.get(), bindless);
    ResourceRef boundSampler(new MockBoundResource());
    rawMock->addResourceEntry(boundSampler.get());
    UniformBufferRef customBuffer(rawMock);

    EXPECT_EQ(customBuffer->getSize(), 192u);
    EXPECT_EQ(customBuffer->getBindlessHandle(), bindless);
    ASSERT_EQ(customBuffer->getResourceTable().size(), 1u);
    EXPECT_EQ(customBuffer->getResourceTable()[0].get(), boundSampler.get());
}

#if GP_RHI_INCLUDE_SHADER_DEBUG_DATA
TEST_F(UniformBufferTestFixture, UniformBufferContentMapTracksAndCleansUpSnapshots)
{
    UniformBufferLayoutDesc desc("DebugSnapshotParams", 16u);
    UniformBufferLayoutConstRef layout(new UniformBufferLayout(desc));
    UniformBufferRef ub(new MockDefaultUniformBuffer(layout.get()));
    const UniformBuffer* rawPtr = ub.get();

    const gp::UInt32 payloadA[4]{ 1u, 2u, 3u, 4u };
    const gp::UInt32 payloadB[4]{ 10u, 20u, 30u, 40u };

    // Disabled by default: updates are ignored and lookups return nullptr.
    EXPECT_FALSE(uniformBufferContentMap::isEnabled());
    uniformBufferContentMap::update(rawPtr, payloadA);
    EXPECT_EQ(uniformBufferContentMap::get(rawPtr), nullptr);

    // Enabling allows storing, updating, and reading back CPU copies.
    uniformBufferContentMap::setEnabled(true);
    EXPECT_TRUE(uniformBufferContentMap::isEnabled());
    uniformBufferContentMap::update(rawPtr, payloadA);
    const void* snapshotA = uniformBufferContentMap::get(rawPtr);
    ASSERT_NE(snapshotA, nullptr);
    EXPECT_EQ(gp::memory::compareMemory(snapshotA, payloadA, sizeof(payloadA)), 0);

    uniformBufferContentMap::update(rawPtr, payloadB);
    const void* snapshotB = uniformBufferContentMap::get(rawPtr);
    ASSERT_NE(snapshotB, nullptr);
    EXPECT_EQ(gp::memory::compareMemory(snapshotB, payloadB, sizeof(payloadB)), 0);

    // Updating with nullptr or zero-sized buffer clears/empties the stored snapshot bytes.
    uniformBufferContentMap::update(rawPtr, nullptr);
    EXPECT_EQ(uniformBufferContentMap::get(rawPtr), nullptr);
    uniformBufferContentMap::update(rawPtr, payloadB);
    EXPECT_NE(uniformBufferContentMap::get(rawPtr), nullptr);

    UniformBufferLayoutDesc zeroDesc("ZeroSizeParams", 0u);
    UniformBufferLayoutConstRef zeroLayout(new UniformBufferLayout(zeroDesc));
    UniformBufferRef zeroUb(new MockDefaultUniformBuffer(zeroLayout.get()));
    uniformBufferContentMap::update(zeroUb.get(), payloadA);
    EXPECT_EQ(uniformBufferContentMap::get(zeroUb.get()), nullptr);

    // Swap-and-pop removal when removing a non-final entry preserves remaining entries.
    UniformBufferRef ubExtraA(new MockDefaultUniformBuffer(layout.get()));
    UniformBufferRef ubExtraB(new MockDefaultUniformBuffer(layout.get()));
    uniformBufferContentMap::update(ubExtraA.get(), payloadA);
    uniformBufferContentMap::update(ubExtraB.get(), payloadB);
    uniformBufferContentMap::remove(ubExtraA.get());
    EXPECT_EQ(uniformBufferContentMap::get(ubExtraA.get()), nullptr);
    const void* remainingB = uniformBufferContentMap::get(ubExtraB.get());
    ASSERT_NE(remainingB, nullptr);
    EXPECT_EQ(gp::memory::compareMemory(remainingB, payloadB, sizeof(payloadB)), 0);

    // clear() removes all stored snapshots while keeping tracking enabled.
    uniformBufferContentMap::clear();
    EXPECT_EQ(uniformBufferContentMap::get(rawPtr), nullptr);
    EXPECT_EQ(uniformBufferContentMap::get(ubExtraB.get()), nullptr);
    EXPECT_TRUE(uniformBufferContentMap::isEnabled());
    uniformBufferContentMap::update(rawPtr, payloadA);
    EXPECT_NE(uniformBufferContentMap::get(rawPtr), nullptr);

    // Destroying the UniformBuffer removes its snapshot automatically.
    ub.reset();
    ResourceDeletionQueue::flushPendingDeletes(true);
    EXPECT_EQ(uniformBufferContentMap::get(rawPtr), nullptr);

    // Disabling clears all remaining snapshots.
    UniformBufferRef ub2(new MockDefaultUniformBuffer(layout.get()));
    uniformBufferContentMap::update(ub2.get(), payloadA);
    EXPECT_NE(uniformBufferContentMap::get(ub2.get()), nullptr);
    uniformBufferContentMap::setEnabled(false);
    EXPECT_EQ(uniformBufferContentMap::get(ub2.get()), nullptr);
}
#endif

TEST_F(UniformBufferTestFixture, CascadingDeferredDeletionKeepsLayoutAndReferencedResourcesAlive)
{
    UniformBufferLayoutDesc desc("CascadingLayout", 64u);

    UniformBufferRef uniformBuffer{};
    {
        UniformBufferLayoutConstRef layout(new MockTrackedLayout(desc));
        ResourceRef referencedResource(new MockBoundResource());

        MockUniformBuffer* rawBuffer = new MockUniformBuffer(layout.get());
        rawBuffer->addResourceEntry(referencedResource.get());
        uniformBuffer = UniformBufferRef(rawBuffer);

        EXPECT_EQ(layout->getRefCount(), 2u);
        EXPECT_EQ(referencedResource->getRefCount(), 2u);
    }

    // Releasing caller references must not destroy the layout or referenced resources while uniformBuffer is alive.
    ResourceDeletionQueue::flushPendingDeletes(true);
    EXPECT_EQ(MockUniformBuffer::s_destructCount.load(), 0u);
    EXPECT_EQ(MockTrackedLayout::s_destructCount.load(), 0u);
    EXPECT_EQ(MockBoundResource::s_destructCount.load(), 0u);

    // Releasing the uniform buffer queues deletion without destroying inline.
    uniformBuffer.reset();
    EXPECT_EQ(MockUniformBuffer::s_destructCount.load(), 0u);
    EXPECT_EQ(MockTrackedLayout::s_destructCount.load(), 0u);
    EXPECT_EQ(MockBoundResource::s_destructCount.load(), 0u);

    // Flushing drains the uniform buffer and cascades through its retained layout and resource table.
    ResourceDeletionQueue::flushPendingDeletes(true);
    EXPECT_EQ(MockUniformBuffer::s_destructCount.load(), 1u);
    EXPECT_EQ(MockTrackedLayout::s_destructCount.load(), 1u);
    EXPECT_EQ(MockBoundResource::s_destructCount.load(), 1u);
}

}   // namespace gp::rhi::resource::tests
