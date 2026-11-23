// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "core/ResourceDeletionQueue.hpp"
#include "core/ResourceEnums.hpp"
#include "memory/pointers/PointerConcepts.hpp"
#include "memory/pointers/RefCountedPtr.hpp"
#include "pipeline/BlendState.hpp"
#include "pipeline/BlendStateDesc.hpp"
#include "pipeline/DepthStencilState.hpp"
#include "pipeline/DepthStencilStateDesc.hpp"
#include "pipeline/PipelineStateUtils.hpp"
#include "pipeline/RasterizerState.hpp"
#include "pipeline/RasterizerStateDesc.hpp"
#include "pipeline/SamplerState.hpp"
#include "pipeline/VertexDeclaration.hpp"
#include "pipeline/VertexDeclarationDesc.hpp"
#include "threading/atomics/Atomic.hpp"
#include "views/DescriptorHandle.hpp"
#include <gtest/gtest.h>
#include <type_traits>

namespace gp::rhi::pipeline::tests
{

static_assert(gp::concepts::IsRefCounted<SamplerState>, "SamplerState must satisfy IsRefCounted.");
static_assert(gp::concepts::IsRefCounted<RasterizerState>, "RasterizerState must satisfy IsRefCounted.");
static_assert(gp::concepts::IsRefCounted<DepthStencilState>, "DepthStencilState must satisfy IsRefCounted.");
static_assert(gp::concepts::IsRefCounted<BlendState>, "BlendState must satisfy IsRefCounted.");
static_assert(gp::concepts::IsRefCounted<VertexDeclaration>, "VertexDeclaration must satisfy IsRefCounted.");

static_assert(sizeof(SamplerStateRef) == sizeof(SamplerState*), "SamplerStateRef must remain a single pointer.");
static_assert(
    sizeof(RasterizerStateRef) == sizeof(RasterizerState*), "RasterizerStateRef must remain a single pointer."
);
static_assert(
    sizeof(DepthStencilStateRef) == sizeof(DepthStencilState*), "DepthStencilStateRef must remain a single pointer."
);
static_assert(sizeof(BlendStateRef) == sizeof(BlendState*), "BlendStateRef must remain a single pointer.");
static_assert(
    sizeof(VertexDeclarationRef) == sizeof(VertexDeclaration*), "VertexDeclarationRef must remain a single pointer."
);

static_assert(!std::is_copy_constructible_v<SamplerState>, "SamplerState must not be copy constructible.");
static_assert(!std::is_copy_constructible_v<RasterizerState>, "RasterizerState must not be copy constructible.");
static_assert(!std::is_copy_constructible_v<DepthStencilState>, "DepthStencilState must not be copy constructible.");
static_assert(!std::is_copy_constructible_v<BlendState>, "BlendState must not be copy constructible.");
static_assert(!std::is_copy_constructible_v<VertexDeclaration>, "VertexDeclaration must not be copy constructible.");

namespace
{

class MockSamplerState final : public SamplerState
{
public:
    static inline gp::thread::Atomic<gp::UInt32> s_destructCount{ 0u };

    MockSamplerState(const bool immutable, const view::DescriptorHandle handle) noexcept
        : m_isImmutable(immutable)
        , m_bindlessHandle(handle)
    {}

    virtual ~MockSamplerState() noexcept override
    {
        s_destructCount.fetchAdd(1u, gp::memory::MemoryOrder::Relaxed);
    }

    /// @copydoc SamplerState::isImmutable()
    [[nodiscard]] virtual bool isImmutable() const noexcept override
    {
        return m_isImmutable;
    }

    /// @copydoc SamplerState::getBindlessHandle()
    [[nodiscard]] virtual view::DescriptorHandle getBindlessHandle() const noexcept override
    {
        return m_bindlessHandle;
    }

private:
    bool m_isImmutable{ false };
    view::DescriptorHandle m_bindlessHandle{};
};

class MockRasterizerState final : public RasterizerState
{
public:
    explicit MockRasterizerState(const RasterizerStateDesc& desc) noexcept
        : m_desc(desc)
    {}

    virtual ~MockRasterizerState() noexcept override = default;

    /// @copydoc RasterizerState::getInitializer()
    [[nodiscard]] virtual bool getInitializer(RasterizerStateDesc& outDesc) const noexcept override
    {
        outDesc = m_desc;
        return true;
    }

private:
    RasterizerStateDesc m_desc{};
};

class MockDepthStencilState final : public DepthStencilState
{
public:
    explicit MockDepthStencilState(const DepthStencilStateDesc& desc) noexcept
        : m_desc(desc)
    {}

    virtual ~MockDepthStencilState() noexcept override = default;

    /// @copydoc DepthStencilState::getInitializer()
    [[nodiscard]] virtual bool getInitializer(DepthStencilStateDesc& outDesc) const noexcept override
    {
        outDesc = m_desc;
        return true;
    }

private:
    DepthStencilStateDesc m_desc{};
};

class MockBlendState final : public BlendState
{
public:
    explicit MockBlendState(const BlendStateDesc& desc) noexcept
        : m_desc(desc)
    {}

    virtual ~MockBlendState() noexcept override = default;

    /// @copydoc BlendState::getInitializer()
    [[nodiscard]] virtual bool getInitializer(BlendStateDesc& outDesc) const noexcept override
    {
        outDesc = m_desc;
        return true;
    }

private:
    BlendStateDesc m_desc{};
};

class MockVertexDeclaration final : public VertexDeclaration
{
public:
    MockVertexDeclaration(const VertexDeclarationElementList& elements, const gp::UInt32 precacheHash) noexcept
        : m_elements(elements)
        , m_precacheHash(precacheHash)
    {}

    virtual ~MockVertexDeclaration() noexcept override = default;

    /// @copydoc VertexDeclaration::getInitializer()
    [[nodiscard]] virtual bool getInitializer(VertexDeclarationElementList& outElements) const noexcept override
    {
        outElements = m_elements;
        return true;
    }

    /// @copydoc VertexDeclaration::getPrecachePSOHash()
    [[nodiscard]] virtual gp::UInt32 getPrecachePSOHash() const noexcept override
    {
        return m_precacheHash;
    }

private:
    VertexDeclarationElementList m_elements{};
    gp::UInt32 m_precacheHash{ 0u };
};

class PipelineStatesTestFixture : public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        resource::ResourceDeletionQueue::flushPendingDeletes(true);
        MockSamplerState::s_destructCount.store(0u, gp::memory::MemoryOrder::SeqCst);
    }

    virtual void TearDown() override
    {
        resource::ResourceDeletionQueue::flushPendingDeletes(true);
    }
};

}   // namespace

TEST_F(PipelineStatesTestFixture, BaseStateClassesReportCorrectTypesAndDefaults)
{
    SamplerStateRef sampler(new SamplerState());
    RasterizerStateRef rasterizer(new RasterizerState());
    DepthStencilStateRef depthStencil(new DepthStencilState());
    BlendStateRef blend(new BlendState());
    VertexDeclarationRef vertexDecl(new VertexDeclaration());

    EXPECT_EQ(sampler->getType(), resource::ResourceType::SamplerState);
    EXPECT_FALSE(sampler->isImmutable());
    EXPECT_FALSE(sampler->getBindlessHandle().isValid());

    EXPECT_EQ(rasterizer->getType(), resource::ResourceType::RasterizerState);
    RasterizerStateDesc rastDesc{};
    EXPECT_FALSE(rasterizer->getInitializer(rastDesc));

    EXPECT_EQ(depthStencil->getType(), resource::ResourceType::DepthStencilState);
    DepthStencilStateDesc dsDesc{};
    EXPECT_FALSE(depthStencil->getInitializer(dsDesc));

    EXPECT_EQ(blend->getType(), resource::ResourceType::BlendState);
    BlendStateDesc blendDesc{};
    EXPECT_FALSE(blend->getInitializer(blendDesc));

    EXPECT_EQ(vertexDecl->getType(), resource::ResourceType::VertexDeclaration);
    VertexDeclarationElementList elements{};
    EXPECT_FALSE(vertexDecl->getInitializer(elements));
    EXPECT_EQ(vertexDecl->getPrecachePSOHash(), 0u);

    // Upcast to ResourceRef exercises shared intrusive ref counting and deferred deletion.
    resource::ResourceRef baseRef = sampler;
    EXPECT_EQ(sampler->getRefCount(), 2u);
    baseRef.reset();
    EXPECT_EQ(sampler->getRefCount(), 1u);
}

TEST_F(PipelineStatesTestFixture, BackendOverridesAndDeferredDeletionLifecycle)
{
    const view::DescriptorHandle bindlessSlot(view::DescriptorType::Sampler, 77u);
    {
        SamplerStateRef sampler(new MockSamplerState(true, bindlessSlot));
        EXPECT_TRUE(sampler->isImmutable());
        EXPECT_TRUE(sampler->getBindlessHandle().isValid());
        EXPECT_EQ(sampler->getBindlessHandle(), bindlessSlot);
        EXPECT_EQ(MockSamplerState::s_destructCount.load(), 0u);
    }

    // Releasing the last SamplerStateRef queues deletion instead of destroying inline.
    EXPECT_EQ(MockSamplerState::s_destructCount.load(), 0u);

    resource::ResourceDeletionQueue::flushPendingDeletes(true);
    EXPECT_EQ(MockSamplerState::s_destructCount.load(), 1u);
}

TEST_F(PipelineStatesTestFixture, MatchPipelineStateComparesRetrievedInitializers)
{
    const RasterizerStateDesc solidCw(RasterizerFillMode::Solid, RasterizerCullMode::CW, true);
    const RasterizerStateDesc wireframe(RasterizerFillMode::Wireframe, RasterizerCullMode::None, false);

    RasterizerStateRef rastA(new MockRasterizerState(solidCw));
    RasterizerStateRef rastB(new MockRasterizerState(solidCw));
    RasterizerStateRef rastC(new MockRasterizerState(wireframe));
    RasterizerStateRef rastDefault(new MockRasterizerState(RasterizerStateDesc{}));

    EXPECT_TRUE((matchPipelineState<RasterizerState, RasterizerStateDesc>(rastA.get(), rastB.get())));
    EXPECT_FALSE((matchPipelineState<RasterizerState, RasterizerStateDesc>(rastA.get(), rastC.get())));
    EXPECT_TRUE((matchPipelineState<RasterizerState, RasterizerStateDesc>(nullptr, rastDefault.get())));
    EXPECT_FALSE((matchPipelineState<RasterizerState, RasterizerStateDesc>(nullptr, rastA.get())));

    const DepthStencilStateDesc dsReadOnly(false, CompareFunction::GreaterEqual);
    DepthStencilStateRef dsA(new MockDepthStencilState(dsReadOnly));
    DepthStencilStateRef dsB(new MockDepthStencilState(dsReadOnly));
    EXPECT_TRUE((matchPipelineState<DepthStencilState, DepthStencilStateDesc>(dsA.get(), dsB.get())));
    EXPECT_FALSE((matchPipelineState<DepthStencilState, DepthStencilStateDesc>(dsA.get(), nullptr)));

    const BlendStateDesc alphaBlend(
        RenderTargetBlendDesc(BlendOperation::Add, BlendFactor::SourceAlpha, BlendFactor::InverseSourceAlpha)
    );
    BlendStateRef blendA(new MockBlendState(alphaBlend));
    BlendStateRef blendB(new MockBlendState(alphaBlend));
    EXPECT_TRUE((matchPipelineState<BlendState, BlendStateDesc>(blendA.get(), blendB.get())));
    EXPECT_FALSE((matchPipelineState<BlendState, BlendStateDesc>(blendA.get(), nullptr)));

    VertexDeclarationElementList layout{};
    layout.emplaceBack(0u, 0u, VertexElementType::Float3, 0u, 12u, false);
    VertexDeclarationRef vdA(new MockVertexDeclaration(layout, 0x12345678u));
    VertexDeclarationRef vdB(new MockVertexDeclaration(layout, 0x12345678u));

    EXPECT_EQ(vdA->getPrecachePSOHash(), 0x12345678u);
    EXPECT_TRUE((matchPipelineState<VertexDeclaration, VertexDeclarationElementList>(vdA.get(), vdB.get())));
    EXPECT_FALSE((matchPipelineState<VertexDeclaration, VertexDeclarationElementList>(vdA.get(), nullptr)));
}

}   // namespace gp::rhi::pipeline::tests
