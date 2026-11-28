// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "shaders/Shader.hpp"
#include "containers/arrays/Vector.hpp"      // IWYU pragma: keep
#include "containers/strings/Name.hpp"
#include "containers/strings/String.hpp"     // IWYU pragma: keep
#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "containers/views/VectorView.hpp"
#include "core/ResourceDeletionQueue.hpp"
#include "core/ResourceEnums.hpp"
#include "CoreMinimal.hpp"
#include "hashing/HashingTypes.hpp"   // IWYU pragma: keep
#include "memory/MemoryOrder.hpp"
#include "memory/pointers/PointerConcepts.hpp"
#include "memory/pointers/RefCountedPtr.hpp"
#include "memory/pointers/SharedPtr.hpp"
#include "pipeline/ComputePipelineState.hpp"
#include "resources/UniformBufferEnums.hpp"
#include "RHIBase.hpp"
#include "shaders/AmplificationShader.hpp"
#include "shaders/ComputeShader.hpp"
#include "shaders/GeometryShader.hpp"
#include "shaders/GraphicsShader.hpp"   // IWYU pragma: keep
#include "shaders/MeshShader.hpp"
#include "shaders/PixelShader.hpp"
#include "shaders/RayCallableShader.hpp"
#include "shaders/RayGenShader.hpp"
#include "shaders/RayHitGroupShader.hpp"
#include "shaders/RayMissShader.hpp"
#include "shaders/RayTracingShader.hpp"   // IWYU pragma: keep
#include "shaders/ShaderData.hpp"
#include "shaders/ShaderEnums.hpp"
#include "shaders/ShaderTypes.hpp"
#include "shaders/VertexShader.hpp"
#include "shaders/WorkGraphComputeNodeShader.hpp"
#include "shaders/WorkGraphRootShader.hpp"
#include "shaders/WorkGraphShader.hpp"   // IWYU pragma: keep
#include "threading/atomics/Atomic.hpp"
#include <gtest/gtest.h>
#include <thread>
#include <type_traits>
#include <utility>

namespace gp::rhi::shader::tests
{

static_assert(gp::concepts::IsRefCounted<Shader>, "Shader must satisfy IsRefCounted.");
static_assert(gp::concepts::IsRefCounted<VertexShader>, "VertexShader must satisfy IsRefCounted.");
static_assert(gp::concepts::IsRefCounted<MeshShader>, "MeshShader must satisfy IsRefCounted.");
static_assert(gp::concepts::IsRefCounted<AmplificationShader>, "AmplificationShader must satisfy IsRefCounted.");
static_assert(gp::concepts::IsRefCounted<PixelShader>, "PixelShader must satisfy IsRefCounted.");
static_assert(gp::concepts::IsRefCounted<GeometryShader>, "GeometryShader must satisfy IsRefCounted.");
static_assert(gp::concepts::IsRefCounted<RayGenShader>, "RayGenShader must satisfy IsRefCounted.");
static_assert(gp::concepts::IsRefCounted<RayMissShader>, "RayMissShader must satisfy IsRefCounted.");
static_assert(gp::concepts::IsRefCounted<RayCallableShader>, "RayCallableShader must satisfy IsRefCounted.");
static_assert(gp::concepts::IsRefCounted<RayHitGroupShader>, "RayHitGroupShader must satisfy IsRefCounted.");
static_assert(gp::concepts::IsRefCounted<ComputeShader>, "ComputeShader must satisfy IsRefCounted.");
static_assert(gp::concepts::IsRefCounted<WorkGraphRootShader>, "WorkGraphRootShader must satisfy IsRefCounted.");
static_assert(
    gp::concepts::IsRefCounted<WorkGraphComputeNodeShader>, "WorkGraphComputeNodeShader must satisfy IsRefCounted."
);

static_assert(sizeof(ShaderRef) == sizeof(Shader*), "ShaderRef must remain a single pointer.");
static_assert(sizeof(VertexShaderRef) == sizeof(VertexShader*), "VertexShaderRef must remain a single pointer.");
static_assert(sizeof(PixelShaderRef) == sizeof(PixelShader*), "PixelShaderRef must remain a single pointer.");
static_assert(sizeof(ComputeShaderRef) == sizeof(ComputeShader*), "ComputeShaderRef must remain a single pointer.");

static_assert(!std::is_copy_constructible_v<ShaderData>, "ShaderData must not be copy constructible.");
static_assert(!std::is_copy_constructible_v<Shader>, "Shader must not be copy constructible.");
static_assert(!std::is_copy_constructible_v<VertexShader>, "VertexShader must not be copy constructible.");
static_assert(!std::is_copy_constructible_v<ComputeShader>, "ComputeShader must not be copy constructible.");

static_assert(kShaderFrequencyOf<VertexShader> == ShaderFrequency::Vertex);
static_assert(kShaderFrequencyOf<const VertexShader> == ShaderFrequency::Vertex);
static_assert(kShaderFrequencyOf<VertexShader*> == ShaderFrequency::Vertex);
static_assert(kShaderFrequencyOf<const VertexShader*> == ShaderFrequency::Vertex);
static_assert(kShaderFrequencyOf<VertexShaderRef> == ShaderFrequency::Vertex);
static_assert(kShaderFrequencyOf<MeshShader> == ShaderFrequency::Mesh);
static_assert(kShaderFrequencyOf<AmplificationShader> == ShaderFrequency::Amplification);
static_assert(kShaderFrequencyOf<PixelShader> == ShaderFrequency::Pixel);
static_assert(kShaderFrequencyOf<GeometryShader> == ShaderFrequency::Geometry);
static_assert(kShaderFrequencyOf<RayGenShader> == ShaderFrequency::RayGen);
static_assert(kShaderFrequencyOf<RayMissShader> == ShaderFrequency::RayMiss);
static_assert(kShaderFrequencyOf<RayCallableShader> == ShaderFrequency::RayCallable);
static_assert(kShaderFrequencyOf<RayHitGroupShader> == ShaderFrequency::RayHitGroup);
static_assert(kShaderFrequencyOf<ComputeShader> == ShaderFrequency::Compute);
static_assert(kShaderFrequencyOf<WorkGraphRootShader> == ShaderFrequency::WorkGraphRoot);
static_assert(kShaderFrequencyOf<WorkGraphComputeNodeShader> == ShaderFrequency::WorkGraphComputeNode);

namespace
{

class MockTrackedVertexShader final : public VertexShader
{
public:
    static inline gp::thread::Atomic<gp::UInt32> s_destructCount{ 0u };

    MockTrackedVertexShader() noexcept = default;

    virtual ~MockTrackedVertexShader() noexcept override
    {
        s_destructCount.fetchAdd(1u, gp::memory::MemoryOrder::Relaxed);
    }
};

struct BytecodeBuffer
{
    gp::Vector<gp::UInt8> bytes{};
};

/// @brief Tracks function-pointer deleter invocations for CreateShaderDesc tests.
gp::UInt32 g_fnPtrDeleterCalls = 0u;
const void* g_fnPtrDeletedAddress = nullptr;

/// @brief Free-function deleter used to verify non-functor invocable deleters.
/// @param[in] ptr Bytecode pointer passed to the deleter.
void freeFunctionBytecodeDeleter(const void* const ptr) noexcept
{
    ++g_fnPtrDeleterCalls;
    g_fnPtrDeletedAddress = ptr;
}

class ShaderTestFixture : public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        resource::ResourceDeletionQueue::flushPendingDeletes(true);
        MockTrackedVertexShader::s_destructCount.store(0u, gp::memory::MemoryOrder::SeqCst);
    }

    virtual void TearDown() override
    {
        resource::ResourceDeletionQueue::flushPendingDeletes(true);
    }
};

}   // namespace

TEST_F(ShaderTestFixture, ShaderResourceTableEqualityAndEmptyState)
{
    ShaderResourceTable tableA{};
    ShaderResourceTable tableB{};

    EXPECT_TRUE(tableA.isEmpty());
    EXPECT_EQ(tableA, tableB);

    tableA.resourceTableBits = 0x05u;
    tableA.shaderResourceViewMap.emplaceBack(10u);
    tableA.samplerMap.emplaceBack(20u);
    tableA.unorderedAccessViewMap.emplaceBack(30u);
    tableA.resourceTableLayoutHashes.emplaceBack(0xAABBCCDDu);
    tableA.textureMap.emplaceBack(40u);
    tableA.resourceCollectionMap.emplaceBack(50u);
    tableA.descriptorRangeMap.emplaceBack(60u);
    tableA.uniformBlockMap.emplaceBack(70u);

    EXPECT_FALSE(tableA.isEmpty());
    EXPECT_NE(tableA, tableB);

    tableB = tableA;
    EXPECT_EQ(tableA, tableB);

    tableB.uniformBlockMap[0] = 71u;
    EXPECT_NE(tableA, tableB);
}

TEST_F(ShaderTestFixture, CreateShaderDescSupportsNonOwningSharedAndCustomDeleterBytecode)
{
    CreateShaderDesc emptyDesc{};
    EXPECT_FALSE(emptyDesc.isValid());
    EXPECT_EQ(emptyDesc.library, nullptr);
    EXPECT_FALSE(emptyDesc.codeOwner.isValid());

    const gp::UInt8 rawBytes[4]{ 0x44u, 0x58u, 0x42u, 0x43u };
    CreateShaderDesc viewDesc(gp::ConstVectorView<gp::UInt8>(rawBytes, 4u));
    EXPECT_TRUE(viewDesc.isValid());
    EXPECT_EQ(viewDesc.code.size(), 4u);
    EXPECT_EQ(viewDesc.code[0], 0x44u);
    EXPECT_FALSE(viewDesc.codeOwner.isValid());

    gp::SharedPtr<BytecodeBuffer> backing(new BytecodeBuffer());
    backing->bytes.emplaceBack(0x03u);
    backing->bytes.emplaceBack(0x02u);
    backing->bytes.emplaceBack(0x23u);
    backing->bytes.emplaceBack(0x07u);

    const gp::ConstVectorView<gp::UInt8> backingView(backing->bytes);
    CreateShaderDesc ownedDesc(backingView, backing);
    EXPECT_TRUE(ownedDesc.isValid());
    EXPECT_TRUE(ownedDesc.codeOwner.isValid());
    EXPECT_EQ(backing.getSharedReferenceCount(), 2u);

    backing.reset();
    EXPECT_EQ(ownedDesc.codeOwner.getSharedReferenceCount(), 1u);
    EXPECT_EQ(ownedDesc.code.size(), 4u);
    EXPECT_EQ(ownedDesc.code[3], 0x07u);

    // Custom deleter constructor keeps data valid while copies exist and invokes deleter once at final release.
    gp::UInt32 deleterCalls = 0u;
    const void* deletedPtr = nullptr;
    ShaderLibrary* const fakeLibrary = reinterpret_cast<ShaderLibrary*>(static_cast<gp::UIntPtr>(0x12340u));
    {
        CreateShaderDesc deleterDesc(
            gp::ConstVectorView<gp::UInt8>(rawBytes, 4u),
            [&deleterCalls, &deletedPtr](const void* const ptr) noexcept -> void
        {
            ++deleterCalls;
            deletedPtr = ptr;
        },
            fakeLibrary
        );
        EXPECT_TRUE(deleterDesc.isValid());
        EXPECT_TRUE(deleterDesc.codeOwner.isValid());
        EXPECT_EQ(deleterDesc.codeOwner.get(), static_cast<const void*>(rawBytes));
        EXPECT_EQ(deleterDesc.library, fakeLibrary);

        CreateShaderDesc copiedDesc = deleterDesc;
        EXPECT_EQ(deleterCalls, 0u);
        EXPECT_EQ(copiedDesc.codeOwner.getSharedReferenceCount(), 2u);
    }
    EXPECT_EQ(deleterCalls, 1u);
    EXPECT_EQ(deletedPtr, static_cast<const void*>(rawBytes));

    // Free function-pointer deleter also satisfies CreateShaderDesc's invocable constraint.
    g_fnPtrDeleterCalls = 0u;
    g_fnPtrDeletedAddress = nullptr;
    {
        CreateShaderDesc fnPtrDesc(
            gp::ConstVectorView<gp::UInt8>(rawBytes, 4u), &freeFunctionBytecodeDeleter, fakeLibrary
        );
        EXPECT_TRUE(fnPtrDesc.isValid());
        EXPECT_TRUE(fnPtrDesc.codeOwner.isValid());
        EXPECT_EQ(fnPtrDesc.library, fakeLibrary);
    }
    EXPECT_EQ(g_fnPtrDeleterCalls, 1u);
    EXPECT_EQ(g_fnPtrDeletedAddress, static_cast<const void*>(rawBytes));
}

TEST_F(ShaderTestFixture, ShaderMetadataFlagsSpecializationAndPSOCompilationTracking)
{
    VertexShaderRef vs(new VertexShader());
    EXPECT_EQ(vs->getType(), resource::ResourceType::VertexShader);
    EXPECT_EQ(vs->getFrequency(), ShaderFrequency::Vertex);
    EXPECT_TRUE(vs->getHash().isZero());
    EXPECT_FALSE(vs->hasNoDerivativeOps());
    EXPECT_FALSE(vs->hasShaderBundleUsage());
    EXPECT_FALSE(vs->hasSpecializationConstants());
    EXPECT_FALSE(vs->isInUseByPSOCompilation());
    EXPECT_EQ(vs->getPSOCompilationUseCount(), 0);
    EXPECT_FALSE(vs->hasStaticSlots());
    EXPECT_TRUE(vs->getShaderResourceTable().isEmpty());

    const ShaderHash digest(0x0123456789ABCDEFull, 0xFEDCBA9876543210ull);
    vs->setHash(digest);
    EXPECT_EQ(vs->getHash(), digest);

    vs->setNoDerivativeOps(true);
    vs->setShaderBundleUsage(true);
    EXPECT_TRUE(vs->hasNoDerivativeOps());
    EXPECT_TRUE(vs->hasShaderBundleUsage());
    vs->setNoDerivativeOps(false);
    vs->setShaderBundleUsage(false);
    EXPECT_FALSE(vs->hasNoDerivativeOps());
    EXPECT_FALSE(vs->hasShaderBundleUsage());

    gp::Vector<ShaderSpecializationExclusion> exclusions{};
    ShaderSpecializationExclusion entryA{};
    entryA.bindPoint = 2u;
    entryA.specializationHashes.emplaceBack(0x11223344u);
    ShaderSpecializationExclusion entryB{};
    entryB.bindPoint = 2u;
    entryB.specializationHashes.emplaceBack(0x55667788u);
    EXPECT_NE(entryA, entryB);
    EXPECT_EQ(entryA, entryA);
    exclusions.emplaceBack(std::move(entryA));
    exclusions.emplaceBack(std::move(entryB));
    vs->setSpecializationExclusions(std::move(exclusions));

    // Exclusions are ignored while specialization constants are disabled.
    EXPECT_FALSE(vs->isExcludedFromSpecialization(2u, 0x11223344u));

    vs->setSpecializationConstants(true);
    EXPECT_TRUE(vs->hasSpecializationConstants());
    EXPECT_EQ(vs->getSpecializationExclusions().size(), 2u);
    EXPECT_TRUE(vs->isExcludedFromSpecialization(2u, 0x11223344u));
    // Duplicate bindPoint entries must all be scanned.
    EXPECT_TRUE(vs->isExcludedFromSpecialization(2u, 0x55667788u));
    EXPECT_FALSE(vs->isExcludedFromSpecialization(2u, 0x99AABBCCu));
    EXPECT_FALSE(vs->isExcludedFromSpecialization(3u, 0x11223344u));

    vs->setSpecializationConstants(false);
    EXPECT_FALSE(vs->hasSpecializationConstants());
    EXPECT_FALSE(vs->isExcludedFromSpecialization(2u, 0x11223344u));

    // Direct PSO compilation ref-count observation while held.
    vs->setInUseByPSOCompilation(true);
    EXPECT_TRUE(vs->isInUseByPSOCompilation());
    EXPECT_EQ(vs->getPSOCompilationUseCount(), 1);
    vs->setInUseByPSOCompilation(false);
    EXPECT_FALSE(vs->isInUseByPSOCompilation());
    EXPECT_EQ(vs->getPSOCompilationUseCount(), 0);

    ShaderResourceTable table{};
    table.resourceTableBits = 0x01u;
    table.textureMap.emplaceBack(0x00010002u);
    vs->setShaderResourceTable(std::move(table));
    EXPECT_FALSE(vs->getShaderResourceTable().isEmpty());
    EXPECT_EQ(vs->getShaderResourceTable().textureMap.size(), 1u);

    gp::Vector<resource::UniformBufferStaticSlot> slots{};
    slots.emplaceBack(0u);
    slots.emplaceBack(4u);
    vs->setStaticSlots(std::move(slots));
    EXPECT_TRUE(vs->hasStaticSlots());
    EXPECT_EQ(vs->getStaticSlots().size(), 2u);
    EXPECT_EQ(vs->getStaticSlots()[1], 4u);

    // Exercise concurrent PSO compilation ref-count tracking.
    constexpr gp::Int32 kThreadCount = 4;
    constexpr gp::Int32 kIterations = 250;
    std::thread workers[kThreadCount];
    for (gp::Int32 i = 0; i < kThreadCount; ++i)
    {
        workers[i] = std::thread(
            [shaderPtr = vs.get()]() -> void
        {
            for (gp::Int32 iter = 0; iter < kIterations; ++iter)
            {
                shaderPtr->setInUseByPSOCompilation(true);
                shaderPtr->setInUseByPSOCompilation(false);
            }
        }
        );
    }
    for (gp::Int32 i = 0; i < kThreadCount; ++i)
    {
        workers[i].join();
    }

    EXPECT_FALSE(vs->isInUseByPSOCompilation());
    EXPECT_EQ(vs->getPSOCompilationUseCount(), 0);
}

#if GP_RHI_INCLUDE_SHADER_DEBUG_DATA
TEST_F(ShaderTestFixture, ShaderDebugNamesAndValidationMetadataRoundTrip)
{
    PixelShaderRef ps(new PixelShader());
    EXPECT_FALSE(ps->hasShaderName());
    EXPECT_EQ(ps->getShaderName(), gp::StringView("<unknown>"));
    EXPECT_EQ(ps->getUniformBufferName(0u), gp::String("<unknown>"));

    ps->setDebugShaderName("DeferredLightingPS");
    EXPECT_TRUE(ps->hasShaderName());
    EXPECT_EQ(ps->getShaderName(), gp::StringView("DeferredLightingPS"));

    gp::Vector<gp::Name> ubNames{};
    ubNames.emplaceBack(gp::Name("ViewUniformBuffer"));
    ubNames.emplaceBack(gp::Name("MaterialUniformBuffer"));
    ps->setUniformBufferNames(std::move(ubNames));
    EXPECT_EQ(ps->getUniformBufferNames().size(), 2u);
    EXPECT_EQ(ps->getUniformBufferName(0u), gp::String("ViewUniformBuffer"));
    EXPECT_EQ(ps->getUniformBufferName(1u), gp::String("MaterialUniformBuffer"));
    EXPECT_EQ(ps->getUniformBufferName(2u), gp::String("<unknown>"));

    const ShaderCodeValidationStride expectedStride{ 3u, 64u };
    gp::Vector<ShaderCodeValidationStride> strides{};
    strides.emplaceBack(expectedStride);
    ps->setCodeValidationStrides(std::move(strides));
    ASSERT_EQ(ps->getCodeValidationStrides().size(), 1u);
    EXPECT_EQ(ps->getCodeValidationStrides()[0], expectedStride);

    const ShaderCodeValidationType expectedSRVType{ 1u, ShaderCodeResourceBindingType::StructuredBuffer };
    gp::Vector<ShaderCodeValidationType> srvTypes{};
    srvTypes.emplaceBack(expectedSRVType);
    ps->setCodeValidationSRVTypes(std::move(srvTypes));
    ASSERT_EQ(ps->getCodeValidationSRVTypes().size(), 1u);
    EXPECT_EQ(ps->getCodeValidationSRVTypes()[0], expectedSRVType);

    const ShaderCodeValidationType expectedUAVType{ 4u, ShaderCodeResourceBindingType::RWTexture2D };
    gp::Vector<ShaderCodeValidationType> uavTypes{};
    uavTypes.emplaceBack(expectedUAVType);
    ps->setCodeValidationUAVTypes(std::move(uavTypes));
    ASSERT_EQ(ps->getCodeValidationUAVTypes().size(), 1u);
    EXPECT_EQ(ps->getCodeValidationUAVTypes()[0], expectedUAVType);

    const ShaderCodeValidationUBSize expectedUBSize{ 2u, 256u };
    gp::Vector<ShaderCodeValidationUBSize> ubSizes{};
    ubSizes.emplaceBack(expectedUBSize);
    ps->setCodeValidationUBSizes(std::move(ubSizes));
    ASSERT_EQ(ps->getCodeValidationUBSizes().size(), 1u);
    EXPECT_EQ(ps->getCodeValidationUBSizes()[0], expectedUBSize);

    const ShaderCodeValidationStride otherStride{ 3u, 32u };
    const ShaderCodeValidationType otherType{ 1u, ShaderCodeResourceBindingType::Buffer };
    const ShaderCodeValidationUBSize otherUBSize{ 2u, 512u };
    EXPECT_NE(expectedStride, otherStride);
    EXPECT_NE(expectedSRVType, otherType);
    EXPECT_NE(expectedUBSize, otherUBSize);
}
#endif

TEST_F(ShaderTestFixture, AllConcreteShaderStagesReportMatchingTypesAndFrequencies)
{
    VertexShaderRef vs(new VertexShader());
    MeshShaderRef ms(new MeshShader());
    AmplificationShaderRef as(new AmplificationShader());
    PixelShaderRef ps(new PixelShader());
    GeometryShaderRef gs(new GeometryShader());
    RayGenShaderRef rgs(new RayGenShader());
    RayMissShaderRef rms(new RayMissShader());
    RayCallableShaderRef rcs(new RayCallableShader());
    RayHitGroupShaderRef rhs(new RayHitGroupShader());
    ComputeShaderRef cs(new ComputeShader());
    WorkGraphRootShaderRef wgr(new WorkGraphRootShader());
    WorkGraphComputeNodeShaderRef wgc(new WorkGraphComputeNodeShader());

    EXPECT_EQ(vs->getType(), resource::ResourceType::VertexShader);
    EXPECT_EQ(vs->getFrequency(), ShaderFrequency::Vertex);

    EXPECT_EQ(ms->getType(), resource::ResourceType::MeshShader);
    EXPECT_EQ(ms->getFrequency(), ShaderFrequency::Mesh);

    EXPECT_EQ(as->getType(), resource::ResourceType::AmplificationShader);
    EXPECT_EQ(as->getFrequency(), ShaderFrequency::Amplification);

    EXPECT_EQ(ps->getType(), resource::ResourceType::PixelShader);
    EXPECT_EQ(ps->getFrequency(), ShaderFrequency::Pixel);

    EXPECT_EQ(gs->getType(), resource::ResourceType::GeometryShader);
    EXPECT_EQ(gs->getFrequency(), ShaderFrequency::Geometry);

    EXPECT_EQ(rgs->getType(), resource::ResourceType::RayTracingShader);
    EXPECT_EQ(rgs->getFrequency(), ShaderFrequency::RayGen);
    EXPECT_EQ(rgs->getRayTracingPayloadType(), 0u);
    EXPECT_EQ(rgs->getRayTracingPayloadSize(), 0u);
    EXPECT_EQ(rgs->getLocalBindingDataSize(), 0u);
    rgs->setRayTracingPayloadType(0x03u);
    rgs->setRayTracingPayloadSize(48u);
    rgs->setLocalBindingDataSize(96u);
    EXPECT_EQ(rgs->getRayTracingPayloadType(), 0x03u);
    EXPECT_EQ(rgs->getRayTracingPayloadSize(), 48u);
    EXPECT_EQ(rgs->getLocalBindingDataSize(), 96u);

    EXPECT_EQ(rms->getType(), resource::ResourceType::RayTracingShader);
    EXPECT_EQ(rms->getFrequency(), ShaderFrequency::RayMiss);

    EXPECT_EQ(rcs->getType(), resource::ResourceType::RayTracingShader);
    EXPECT_EQ(rcs->getFrequency(), ShaderFrequency::RayCallable);

    EXPECT_EQ(rhs->getType(), resource::ResourceType::RayTracingShader);
    EXPECT_EQ(rhs->getFrequency(), ShaderFrequency::RayHitGroup);

    EXPECT_EQ(cs->getType(), resource::ResourceType::ComputeShader);
    EXPECT_EQ(cs->getFrequency(), ShaderFrequency::Compute);
    EXPECT_EQ(cs->getStats(), nullptr);
    // Updating stats while m_stats is null must remain a safe no-op.
    cs->updateStats(1u);

    pipeline::PipelineStateStats stats{};
    cs->setStats(&stats);
    EXPECT_EQ(cs->getStats(), &stats);
    cs->updateStats(42u);
    cs->updateStats(45u);
    EXPECT_TRUE(stats.hasBeenUsed());
    EXPECT_EQ(stats.firstFrameUsed.load(gp::memory::MemoryOrder::Relaxed), 42u);
    EXPECT_EQ(stats.lastFrameUsed.load(gp::memory::MemoryOrder::Relaxed), 45u);
    EXPECT_EQ(stats.totalBindCount.load(gp::memory::MemoryOrder::Relaxed), 2u);

    EXPECT_EQ(wgr->getType(), resource::ResourceType::WorkGraphShader);
    EXPECT_EQ(wgr->getFrequency(), ShaderFrequency::WorkGraphRoot);

    EXPECT_EQ(wgc->getType(), resource::ResourceType::WorkGraphShader);
    EXPECT_EQ(wgc->getFrequency(), ShaderFrequency::WorkGraphComputeNode);

    // Verify polymorphic upcasts across base shader handles.
    GraphicsShaderRef gfxRef = vs;
    ShaderRef baseRef = gfxRef;
    resource::ResourceRef resRef = baseRef;
    EXPECT_EQ(vs->getRefCount(), 4u);
    resRef.reset();
    baseRef.reset();
    gfxRef.reset();
    EXPECT_EQ(vs->getRefCount(), 1u);
}

TEST_F(ShaderTestFixture, DeferredDeletionLifecycle)
{
    {
        VertexShaderRef tracked(new MockTrackedVertexShader());
        EXPECT_EQ(MockTrackedVertexShader::s_destructCount.load(), 0u);
    }

    EXPECT_EQ(MockTrackedVertexShader::s_destructCount.load(), 0u);
    resource::ResourceDeletionQueue::flushPendingDeletes(true);
    EXPECT_EQ(MockTrackedVertexShader::s_destructCount.load(), 1u);
}

}   // namespace gp::rhi::shader::tests
