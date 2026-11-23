// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "core/Resource.hpp"
#include "core/ResourceDeletionQueue.hpp"
#include "core/ResourceEnums.hpp"
#include "CoreMinimal.hpp"
#include "memory/MemoryOrder.hpp"
#include "memory/pointers/PointerConcepts.hpp"
#include "memory/pointers/RefCountedPtr.hpp"
#include "pipeline/BoundShaderState.hpp"
#include "pipeline/ComputePipelineState.hpp"
#include "pipeline/GraphicsPipelineState.hpp"
#include "pipeline/VertexDeclaration.hpp"
#include "shaders/AmplificationShader.hpp"
#include "shaders/ComputeShader.hpp"
#include "shaders/GeometryShader.hpp"
#include "shaders/GraphicsShader.hpp"
#include "shaders/MeshShader.hpp"
#include "shaders/PixelShader.hpp"
#include "shaders/ShaderEnums.hpp"
#include "shaders/VertexShader.hpp"
#include "shaders/WorkGraphRootShader.hpp"
#include "threading/atomics/Atomic.hpp"
#include <gtest/gtest.h>
#include <thread>
#include <type_traits>

namespace gp::rhi::pipeline::tests
{

static_assert(gp::concepts::IsRefCounted<BoundShaderState>, "BoundShaderState must satisfy IsRefCounted.");
static_assert(gp::concepts::IsRefCounted<GraphicsPipelineState>, "GraphicsPipelineState must satisfy IsRefCounted.");
static_assert(gp::concepts::IsRefCounted<ComputePipelineState>, "ComputePipelineState must satisfy IsRefCounted.");
static_assert(std::is_abstract_v<GraphicsPipelineState>, "GraphicsPipelineState must be abstract.");

static_assert(
    sizeof(BoundShaderStateRef) == sizeof(BoundShaderState*), "BoundShaderStateRef must remain a single pointer."
);
static_assert(
    sizeof(GraphicsPipelineStateRef) == sizeof(GraphicsPipelineState*),
    "GraphicsPipelineStateRef must remain a single pointer."
);
static_assert(
    sizeof(ComputePipelineStateRef) == sizeof(ComputePipelineState*),
    "ComputePipelineStateRef must remain a single pointer."
);

static_assert(!std::is_copy_constructible_v<BoundShaderState>, "BoundShaderState must not be copy constructible.");
static_assert(
    !std::is_copy_constructible_v<GraphicsPipelineState>, "GraphicsPipelineState must not be copy constructible."
);
static_assert(
    !std::is_copy_constructible_v<ComputePipelineState>, "ComputePipelineState must not be copy constructible."
);
static_assert(
    !std::is_default_constructible_v<ComputePipelineState>,
    "ComputePipelineState must require a ComputeShader at construction."
);

namespace
{

class MockTrackedComputeShader final : public shader::ComputeShader
{
public:
    static inline gp::thread::Atomic<gp::UInt32> s_destructCount{ 0u };

    MockTrackedComputeShader() noexcept = default;

    virtual ~MockTrackedComputeShader() noexcept override
    {
        s_destructCount.fetchAdd(1u, gp::memory::MemoryOrder::Relaxed);
    }
};

class MockGraphicsPipelineState final : public GraphicsPipelineState
{
public:
    MockGraphicsPipelineState(shader::VertexShader* const vs, shader::PixelShader* const ps) noexcept
        : m_vertexShader(vs)
        , m_pixelShader(ps)
    {}

    virtual ~MockGraphicsPipelineState() noexcept override = default;

    /// @copydoc GraphicsPipelineState::getShader()
    [[nodiscard]] virtual shader::GraphicsShader*
        getShader(const shader::ShaderFrequency frequency) const noexcept override
    {
        switch (frequency)
        {
        case shader::ShaderFrequency::Vertex:
            return m_vertexShader.get();

        case shader::ShaderFrequency::Pixel:
            return m_pixelShader.get();

        default:
            return nullptr;
        }
    }

private:
    shader::VertexShaderRef m_vertexShader{};
    shader::PixelShaderRef m_pixelShader{};
};

class PipelineStateObjectsTestFixture : public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        resource::ResourceDeletionQueue::flushPendingDeletes(true);
        MockTrackedComputeShader::s_destructCount.store(0u, gp::memory::MemoryOrder::SeqCst);
    }

    virtual void TearDown() override
    {
        resource::ResourceDeletionQueue::flushPendingDeletes(true);
    }
};

}   // namespace

TEST_F(PipelineStateObjectsTestFixture, BoundShaderStateInputVertexMeshAndWorkGraphPipelinesManageRefCounts)
{
    BoundShaderStateInput empty{};
    EXPECT_FALSE(empty.usesMeshPipeline());
    EXPECT_EQ(empty.getVertexDeclaration(), nullptr);
    EXPECT_EQ(empty.getShader(shader::ShaderFrequency::Vertex), nullptr);
    EXPECT_EQ(empty.getShader(shader::ShaderFrequency::Pixel), nullptr);
    EXPECT_EQ(empty.getShader(shader::ShaderFrequency::Compute), nullptr);

    VertexDeclarationRef vertexDecl(new VertexDeclaration());
    shader::VertexShaderRef vs(new shader::VertexShader());
    shader::PixelShaderRef ps(new shader::PixelShader());
    shader::GeometryShaderRef gs(new shader::GeometryShader());

    BoundShaderStateInput vertexInput(vertexDecl.get(), vs.get(), ps.get(), gs.get());
    EXPECT_FALSE(vertexInput.usesMeshPipeline());
    EXPECT_EQ(vertexInput.getVertexDeclaration(), vertexDecl.get());
    EXPECT_EQ(vertexInput.getVertexShader(), vs.get());
    EXPECT_EQ(vertexInput.getPixelShader(), ps.get());
    EXPECT_EQ(vertexInput.getGeometryShader(), gs.get());
    EXPECT_EQ(vertexInput.getShader(shader::ShaderFrequency::Vertex), vs.get());
    EXPECT_EQ(vertexInput.getShader(shader::ShaderFrequency::Pixel), ps.get());
    EXPECT_EQ(vertexInput.getShader(shader::ShaderFrequency::Geometry), gs.get());

    vertexInput.addRefResources();
    EXPECT_EQ(vertexDecl->getRefCount(), 2u);
    EXPECT_EQ(vs->getRefCount(), 2u);
    EXPECT_EQ(ps->getRefCount(), 2u);
    EXPECT_EQ(gs->getRefCount(), 2u);

    vertexInput.releaseResources();
    EXPECT_EQ(vertexDecl->getRefCount(), 1u);
    EXPECT_EQ(vs->getRefCount(), 1u);
    EXPECT_EQ(ps->getRefCount(), 1u);
    EXPECT_EQ(gs->getRefCount(), 1u);

    shader::MeshShaderRef ms(new shader::MeshShader());
    shader::AmplificationShaderRef as(new shader::AmplificationShader());

    BoundShaderStateInput meshInput(ms.get(), as.get(), ps.get());
    EXPECT_TRUE(meshInput.usesMeshPipeline());
    EXPECT_EQ(meshInput.getMeshShader(), ms.get());
    EXPECT_EQ(meshInput.getAmplificationShader(), as.get());
    EXPECT_EQ(meshInput.getShader(shader::ShaderFrequency::Mesh), ms.get());
    EXPECT_EQ(meshInput.getShader(shader::ShaderFrequency::Amplification), as.get());

    meshInput.addRefResources();
    EXPECT_EQ(ms->getRefCount(), 2u);
    EXPECT_EQ(as->getRefCount(), 2u);
    EXPECT_EQ(ps->getRefCount(), 2u);
    EXPECT_EQ(vertexDecl->getRefCount(), 1u);

    meshInput.releaseResources();
    EXPECT_EQ(ms->getRefCount(), 1u);
    EXPECT_EQ(as->getRefCount(), 1u);
    EXPECT_EQ(ps->getRefCount(), 1u);

    shader::WorkGraphRootShaderRef wg(new shader::WorkGraphRootShader());
    BoundShaderStateInput workGraphInput{};
    workGraphInput.setWorkGraphShader(wg.get());
    workGraphInput.setPixelShader(ps.get());
    EXPECT_FALSE(workGraphInput.usesMeshPipeline());
    EXPECT_EQ(workGraphInput.getWorkGraphShader(), wg.get());
    EXPECT_EQ(workGraphInput.getShader(shader::ShaderFrequency::WorkGraphRoot), wg.get());

    workGraphInput.addRefResources();
    EXPECT_EQ(wg->getRefCount(), 2u);
    EXPECT_EQ(ps->getRefCount(), 2u);

    workGraphInput.releaseResources();
    EXPECT_EQ(wg->getRefCount(), 1u);
    EXPECT_EQ(ps->getRefCount(), 1u);

    BoundShaderStateInput meshCopy = meshInput;
    EXPECT_EQ(meshInput, meshCopy);
    EXPECT_NE(meshInput, vertexInput);
    EXPECT_NE(meshInput, workGraphInput);
}

TEST_F(PipelineStateObjectsTestFixture, BoundShaderStateAndGraphicsPipelineStateLifecycle)
{
    BoundShaderStateRef bss(new BoundShaderState());
    EXPECT_EQ(bss->getType(), resource::ResourceType::BoundShaderState);

    const gp::UInt64 deletedBefore = resource::Resource::getNumDeletedPipelineStates();

    {
        shader::VertexShaderRef vs(new shader::VertexShader());
        shader::PixelShaderRef ps(new shader::PixelShader());
        GraphicsPipelineStateRef mockPso(new MockGraphicsPipelineState(vs.get(), ps.get()));

        EXPECT_EQ(mockPso->getType(), resource::ResourceType::GraphicsPipelineState);
        EXPECT_EQ(mockPso->getSortKey(), 0u);
        mockPso->setSortKey(0xCAFEBABE12345678ull);
        EXPECT_EQ(mockPso->getSortKey(), 0xCAFEBABE12345678ull);

        EXPECT_EQ(mockPso->getShader(shader::ShaderFrequency::Vertex), vs.get());
        EXPECT_EQ(mockPso->getShader(shader::ShaderFrequency::Pixel), ps.get());
        EXPECT_EQ(mockPso->getShader(shader::ShaderFrequency::Geometry), nullptr);
    }

    resource::ResourceDeletionQueue::flushPendingDeletes(true);
    EXPECT_EQ(resource::Resource::getNumDeletedPipelineStates(), deletedBefore + 1u);
}

TEST_F(PipelineStateObjectsTestFixture, PipelineStateStatsTracksMinMaxFramesAndConcurrentBinds)
{
    PipelineStateStats::updateStats(nullptr, 10u);

    PipelineStateStats stats{};
    EXPECT_FALSE(stats.hasBeenUsed());
    EXPECT_EQ(stats.firstFrameUsed.load(gp::memory::MemoryOrder::Relaxed), PipelineStateStats::kInvalidFrame);
    EXPECT_EQ(stats.lastFrameUsed.load(gp::memory::MemoryOrder::Relaxed), 0u);
    EXPECT_EQ(stats.totalBindCount.load(gp::memory::MemoryOrder::Relaxed), 0u);

    PipelineStateStats::updateStats(&stats, 100u);
    stats.updateStats(50u);
    stats.updateStats(250u);
    EXPECT_TRUE(stats.hasBeenUsed());
    EXPECT_EQ(stats.firstFrameUsed.load(gp::memory::MemoryOrder::Relaxed), 50u);
    EXPECT_EQ(stats.lastFrameUsed.load(gp::memory::MemoryOrder::Relaxed), 250u);
    EXPECT_EQ(stats.totalBindCount.load(gp::memory::MemoryOrder::Relaxed), 3u);

    PipelineStateStats concurrentStats{};
    constexpr gp::UInt64 kThreadCount = 4u;
    constexpr gp::UInt64 kBindsPerThread = 1000u;
    std::thread workers[kThreadCount];

    for (gp::UInt64 t = 0u; t < kThreadCount; ++t)
    {
        workers[t] = std::thread([&concurrentStats, t]()
        {
            const gp::UInt64 baseFrame = 10u + t * kBindsPerThread;
            for (gp::UInt64 i = 0u; i < kBindsPerThread; ++i)
            {
                concurrentStats.updateStats(baseFrame + i);
            }
        });
    }

    for (gp::UInt64 t = 0u; t < kThreadCount; ++t)
    {
        workers[t].join();
    }

    EXPECT_EQ(concurrentStats.firstFrameUsed.load(gp::memory::MemoryOrder::Relaxed), 10u);
    EXPECT_EQ(
        concurrentStats.lastFrameUsed.load(gp::memory::MemoryOrder::Relaxed),
        10u + (kThreadCount * kBindsPerThread) - 1u
    );
    EXPECT_EQ(concurrentStats.totalBindCount.load(gp::memory::MemoryOrder::Relaxed), kThreadCount * kBindsPerThread);
}

TEST_F(PipelineStateObjectsTestFixture, ComputePipelineStateRetainsShaderAndIncrementsDeletedPipelineCount)
{
    const gp::UInt64 deletedBefore = resource::Resource::getNumDeletedPipelineStates();

    ComputePipelineStateRef computePso{};
    {
        shader::ComputeShaderRef cs(new MockTrackedComputeShader());
        computePso = ComputePipelineStateRef(new ComputePipelineState(cs.get()));
        EXPECT_EQ(computePso->getType(), resource::ResourceType::ComputePipelineState);
        EXPECT_EQ(computePso->getComputeShader(), cs.get());
        EXPECT_EQ(computePso->getComputeShaderRef().get(), cs.get());
        EXPECT_EQ(cs->getRefCount(), 2u);
        EXPECT_TRUE(computePso->isValid());
        EXPECT_TRUE(computePso->isPipelineValid());
        EXPECT_FALSE(computePso->isUsed());

        computePso->setPipelineValid(false);
        EXPECT_FALSE(computePso->isPipelineValid());
        EXPECT_TRUE(computePso->isValid());

        computePso->markUsed();
        EXPECT_TRUE(computePso->isUsed());
    }

    // Flushing while computePso is alive must keep the compute shader alive.
    resource::ResourceDeletionQueue::flushPendingDeletes(true);
    EXPECT_EQ(MockTrackedComputeShader::s_destructCount.load(), 0u);

    // Releasing computePso defers destruction until the deletion queue flushes.
    computePso.reset();
    EXPECT_EQ(MockTrackedComputeShader::s_destructCount.load(), 0u);

    // Flushing destroys the ComputePipelineState, increments deleted pipeline count, and cascades to the shader.
    resource::ResourceDeletionQueue::flushPendingDeletes(true);
    EXPECT_EQ(resource::Resource::getNumDeletedPipelineStates(), deletedBefore + 1u);
    EXPECT_EQ(MockTrackedComputeShader::s_destructCount.load(), 1u);
}

}   // namespace gp::rhi::pipeline::tests
