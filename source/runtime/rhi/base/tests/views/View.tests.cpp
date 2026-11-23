// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "views/View.hpp"
#include "core/ResourceDeletionQueue.hpp"
#include "core/ResourceEnums.hpp"
#include "core/ResourceTypes.hpp"
#include "CoreMinimal.hpp"
#include "memory/MemoryOrder.hpp"
#include "memory/pointers/PointerConcepts.hpp"
#include "memory/pointers/RefCountedPtr.hpp"
#include "resources/Buffer.hpp"
#include "resources/BufferCreateDesc.hpp"
#include "resources/BufferDesc.hpp"   // IWYU pragma: keep
#include "resources/BufferEnums.hpp"
#include "resources/Texture.hpp"
#include "resources/TextureCreateDesc.hpp"
#include "resources/TextureDesc.hpp"   // IWYU pragma: keep
#include "resources/TextureEnums.hpp"
#include "RHIPixelFormat.hpp"
#include "sync/SyncEnums.hpp"
#include "threading/atomics/Atomic.hpp"
#include "views/DescriptorHandle.hpp"
#include "views/ShaderResourceView.hpp"
#include "views/UnorderedAccessView.hpp"
#include "views/ViewDesc.hpp"
#include "views/ViewEnums.hpp"
#include <gtest/gtest.h>
#include <string_view>
#include <type_traits>

namespace gp::rhi::view::tests
{

static_assert(sizeof(ViewDesc) == 16u, "ViewDesc must be packed to 16 bytes.");
static_assert(std::is_trivially_copyable_v<ViewDesc>, "ViewDesc must be trivially copyable.");
static_assert(gp::concepts::IsRefCounted<View>, "View must satisfy IsRefCounted.");
static_assert(gp::concepts::IsRefCounted<ShaderResourceView>, "ShaderResourceView must satisfy IsRefCounted.");
static_assert(gp::concepts::IsRefCounted<UnorderedAccessView>, "UnorderedAccessView must satisfy IsRefCounted.");
static_assert(sizeof(ViewRef) == sizeof(View*), "ViewRef must remain a single pointer.");
static_assert(
    sizeof(ShaderResourceViewRef) == sizeof(ShaderResourceView*), "ShaderResourceViewRef must remain a single pointer."
);
static_assert(
    sizeof(UnorderedAccessViewRef) == sizeof(UnorderedAccessView*),
    "UnorderedAccessViewRef must remain a single pointer."
);
static_assert(!std::is_copy_constructible_v<View>, "View must not be copy constructible.");
static_assert(!std::is_copy_constructible_v<ShaderResourceView>, "ShaderResourceView must not be copy constructible.");
static_assert(
    !std::is_copy_constructible_v<UnorderedAccessView>, "UnorderedAccessView must not be copy constructible."
);

namespace
{

class MockBuffer final : public resource::Buffer
{
public:
    static inline gp::thread::Atomic<gp::UInt32> s_destructCount{ 0u };

    explicit MockBuffer(const resource::BufferCreateDesc& desc) noexcept
        : resource::Buffer(desc)
    {}

    virtual ~MockBuffer() noexcept override
    {
        s_destructCount.fetchAdd(1u, gp::memory::MemoryOrder::Relaxed);
    }
};

class MockTexture final : public resource::Texture
{
public:
    static inline gp::thread::Atomic<gp::UInt32> s_destructCount{ 0u };

    explicit MockTexture(const resource::TextureCreateDesc& desc) noexcept
        : resource::Texture(desc)
    {}

    virtual ~MockTexture() noexcept override
    {
        s_destructCount.fetchAdd(1u, gp::memory::MemoryOrder::Relaxed);
    }
};

class MockBindlessSRV final : public ShaderResourceView
{
public:
    MockBindlessSRV(
        resource::ViewableResource* const target, const ViewDesc& desc, const DescriptorHandle handle
    ) noexcept
        : ShaderResourceView(target, desc)
        , m_bindlessHandle(handle)
    {}

    virtual ~MockBindlessSRV() noexcept override = default;

    /// @copydoc View::getBindlessHandle()
    [[nodiscard]] virtual DescriptorHandle getBindlessHandle() const noexcept override
    {
        return m_bindlessHandle;
    }

private:
    DescriptorHandle m_bindlessHandle{};
};

class ViewTestFixture : public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        resource::ResourceDeletionQueue::flushPendingDeletes(true);
        MockBuffer::s_destructCount.store(0u, gp::memory::MemoryOrder::SeqCst);
        MockTexture::s_destructCount.store(0u, gp::memory::MemoryOrder::SeqCst);
    }

    virtual void TearDown() override
    {
        resource::ResourceDeletionQueue::flushPendingDeletes(true);
    }
};

}   // namespace

TEST_F(ViewTestFixture, DescriptorTypeAndStringHelpersMatchAllEnumerators)
{
    EXPECT_EQ(descriptorTypeFromViewType(ViewType::BufferSRV), DescriptorType::BufferSRV);
    EXPECT_EQ(descriptorTypeFromViewType(ViewType::BufferUAV), DescriptorType::BufferUAV);
    EXPECT_EQ(descriptorTypeFromViewType(ViewType::TextureSRV), DescriptorType::TextureSRV);
    EXPECT_EQ(descriptorTypeFromViewType(ViewType::TextureUAV), DescriptorType::TextureUAV);

    EXPECT_EQ(std::string_view(ViewDesc::getBufferTypeString(BufferViewType::Unknown)), "Unknown");
    EXPECT_EQ(std::string_view(ViewDesc::getBufferTypeString(BufferViewType::Typed)), "Typed");
    EXPECT_EQ(std::string_view(ViewDesc::getBufferTypeString(BufferViewType::Structured)), "Structured");
    EXPECT_EQ(
        std::string_view(ViewDesc::getBufferTypeString(BufferViewType::AccelerationStructure)), "AccelerationStructure"
    );
    EXPECT_EQ(std::string_view(ViewDesc::getBufferTypeString(BufferViewType::Raw)), "Raw");

    EXPECT_EQ(std::string_view(ViewDesc::getTextureDimensionString(TextureViewDimension::Texture2D)), "Texture2D");
    EXPECT_EQ(
        std::string_view(ViewDesc::getTextureDimensionString(TextureViewDimension::Texture2DArray)), "Texture2DArray"
    );
    EXPECT_EQ(std::string_view(ViewDesc::getTextureDimensionString(TextureViewDimension::Texture3D)), "Texture3D");
    EXPECT_EQ(std::string_view(ViewDesc::getTextureDimensionString(TextureViewDimension::TextureCube)), "TextureCube");
    EXPECT_EQ(
        std::string_view(ViewDesc::getTextureDimensionString(TextureViewDimension::TextureCubeArray)),
        "TextureCubeArray"
    );
}

TEST_F(ViewTestFixture, BufferSRVAndUAVResolveTypedStructuredRawAccelAndNullViews)
{
    // Typed buffer SRV
    resource::BufferCreateDesc typedDesc =
        resource::BufferCreateDesc::create("TypedBuf", 256u, 0u, resource::BufferUsageFlags::ShaderResource);
    typedDesc.setInitialState(sync::ResourceAccess::SRVMask);
    resource::BufferRef typedBuf(new MockBuffer(typedDesc));

    const ViewDesc typedViewDesc = ViewDesc::createBufferSRV()
                                       .setTypeFromBuffer(typedBuf.get())
                                       .setFormat(PixelFormat::R32G32B32A32_Float)
                                       .setOffsetInBytes(32u)
                                       .setNumElements(4u)
                                       .asViewDesc();
    EXPECT_TRUE(typedViewDesc.isSRV());
    EXPECT_FALSE(typedViewDesc.isUAV());
    EXPECT_TRUE(typedViewDesc.isBuffer());
    EXPECT_FALSE(typedViewDesc.isTexture());
    EXPECT_EQ(descriptorTypeFromViewDesc(typedViewDesc), DescriptorType::BufferSRV);

    const ViewDesc::BufferSRV::ViewInfo typedInfo = typedViewDesc.buffer.srv.getViewInfo(typedBuf.get());
    EXPECT_FALSE(typedInfo.isNullView);
    EXPECT_EQ(typedInfo.bufferType, BufferViewType::Typed);
    EXPECT_EQ(typedInfo.format, PixelFormat::R32G32B32A32_Float);
    EXPECT_EQ(typedInfo.offsetInBytes, 32u);
    EXPECT_EQ(typedInfo.strideInBytes, 16u);
    EXPECT_EQ(typedInfo.numElements, 4u);
    EXPECT_EQ(typedInfo.sizeInBytes, 64u);

    // Structured buffer UAV with atomic counter, append flag, and whole-buffer derivation
    resource::BufferCreateDesc structDesc = resource::BufferCreateDesc::createStructured("StructBuf", 320u, 32u)
                                                .addUsage(resource::BufferUsageFlags::UnorderedAccess)
                                                .setInitialState(sync::ResourceAccess::UAVMask);
    resource::BufferRef structBuf(new MockBuffer(structDesc));

    const ViewDesc structUavDesc = ViewDesc::createBufferUAV()
                                       .setTypeFromBuffer(structBuf.get())
                                       .setAtomicCounter(true)
                                       .setAppendBuffer(true)
                                       .asViewDesc();
    EXPECT_TRUE(structUavDesc.isUAV());
    EXPECT_TRUE(structUavDesc.isBuffer());
    EXPECT_EQ(descriptorTypeFromViewDesc(structUavDesc), DescriptorType::BufferUAV);

    const ViewDesc::BufferUAV::ViewInfo structInfo = structUavDesc.buffer.uav.getViewInfo(structBuf.get());
    EXPECT_EQ(structInfo.bufferType, BufferViewType::Structured);
    EXPECT_EQ(structInfo.format, PixelFormat::Unknown);
    EXPECT_EQ(structInfo.offsetInBytes, 0u);
    EXPECT_EQ(structInfo.strideInBytes, 32u);
    EXPECT_EQ(structInfo.numElements, 10u);
    EXPECT_EQ(structInfo.sizeInBytes, 320u);
    EXPECT_TRUE(structInfo.atomicCounter);
    EXPECT_TRUE(structInfo.appendBuffer);

    // Raw byte-address buffer SRV
    resource::BufferCreateDesc rawDesc = resource::BufferCreateDesc::createByteAddress("RawBuf", 128u, 4u)
                                             .addUsage(resource::BufferUsageFlags::ShaderResource)
                                             .setInitialState(sync::ResourceAccess::SRVMask);
    resource::BufferRef rawBuf(new MockBuffer(rawDesc));

    const ViewDesc rawViewDesc = ViewDesc::createBufferSRV()
                                     .setTypeFromBuffer(rawBuf.get())
                                     .setOffsetInBytes(16u)
                                     .setNumElements(8u)
                                     .asViewDesc();
    const ViewDesc::BufferSRV::ViewInfo rawInfo = rawViewDesc.buffer.srv.getViewInfo(rawBuf.get());
    EXPECT_EQ(rawInfo.bufferType, BufferViewType::Raw);
    EXPECT_EQ(rawInfo.strideInBytes, 4u);
    EXPECT_EQ(rawInfo.numElements, 8u);
    EXPECT_EQ(rawInfo.sizeInBytes, 32u);

    // Acceleration structure buffer SRV
    resource::BufferCreateDesc accelDesc =
        resource::BufferCreateDesc::create("AccelBuf", 1024u, 0u, resource::BufferUsageFlags::AccelerationStructure)
            .setInitialState(sync::ResourceAccess::BVHRead);
    resource::BufferRef accelBuf(new MockBuffer(accelDesc));
    resource::RayTracingScene* fakeScene = reinterpret_cast<resource::RayTracingScene*>(0x1234u);

    const ViewDesc accelViewDesc =
        ViewDesc::createBufferSRV().setTypeFromBuffer(accelBuf.get()).setRayTracingScene(fakeScene).asViewDesc();
    const ViewDesc::BufferSRV::ViewInfo accelInfo = accelViewDesc.buffer.srv.getViewInfo(accelBuf.get());
    EXPECT_EQ(accelInfo.bufferType, BufferViewType::AccelerationStructure);
    EXPECT_EQ(accelInfo.strideInBytes, 1u);
    EXPECT_EQ(accelInfo.numElements, 1024u);
    EXPECT_EQ(accelInfo.sizeInBytes, 1024u);

    // Null placeholder buffer view
    resource::BufferCreateDesc nullDesc = resource::BufferCreateDesc::createNull("NullBuf");
    resource::BufferRef nullBuf(new MockBuffer(nullDesc));
    const ViewDesc nullViewDesc = ViewDesc::createBufferSRV().setType(BufferViewType::Raw).asViewDesc();
    const ViewDesc::BufferSRV::ViewInfo nullInfo = nullViewDesc.buffer.srv.getViewInfo(nullBuf.get());
    EXPECT_TRUE(nullInfo.isNullView);
}

TEST_F(ViewTestFixture, TextureSRVAndUAVResolveCubePlanesSRGBAndMipRanges)
{
    // Cubemap SRV viewed as both TextureCube and Texture2DArray (6 slices per cube)
    resource::TextureCreateDesc cubeDesc =
        resource::TextureCreateDesc::createCubeArray("SkyCubes", 128u, 2u, PixelFormat::R8G8B8A8_UNorm)
            .setNumMips(4u)
            .addFlags(resource::TextureCreateFlags::ShaderResource | resource::TextureCreateFlags::SRGB)
            .determineInitialState();
    resource::TextureRef cubeTex(new MockTexture(cubeDesc));

    const ViewDesc cubeSrvDesc = ViewDesc::createTextureSRV().setDimensionFromTexture(cubeTex.get()).asViewDesc();
    EXPECT_TRUE(cubeSrvDesc.isSRV());
    EXPECT_TRUE(cubeSrvDesc.isTexture());
    EXPECT_EQ(descriptorTypeFromViewDesc(cubeSrvDesc), DescriptorType::TextureSRV);

    const ViewDesc::TextureSRV::ViewInfo cubeInfo = cubeSrvDesc.texture.srv.getViewInfo(cubeTex.get());
    EXPECT_EQ(cubeInfo.dimension, TextureViewDimension::TextureCubeArray);
    EXPECT_EQ(cubeInfo.format, PixelFormat::R8G8B8A8_UNorm);
    EXPECT_EQ(cubeInfo.mipRange.first, 0u);
    EXPECT_EQ(cubeInfo.mipRange.num, 4u);
    EXPECT_EQ(cubeInfo.arrayRange.first, 0u);
    EXPECT_EQ(cubeInfo.arrayRange.num, 2u);
    EXPECT_EQ(cubeInfo.allMips, 1u);
    EXPECT_EQ(cubeInfo.allSlices, 1u);
    EXPECT_EQ(cubeInfo.isSRGB, 1u);

    const ViewDesc flatArrayDesc = ViewDesc::createTextureSRV()
                                       .setDimension(resource::TextureDimension::Texture2DArray)
                                       .setMipRange(1u, 2u)
                                       .setDisableSRGB(true)
                                       .asViewDesc();
    const ViewDesc::TextureSRV::ViewInfo flatInfo = flatArrayDesc.texture.srv.getViewInfo(cubeTex.get());
    EXPECT_EQ(flatInfo.dimension, TextureViewDimension::Texture2DArray);
    EXPECT_EQ(flatInfo.arrayRange.num, 12u);
    EXPECT_EQ(flatInfo.allSlices, 1u);
    EXPECT_EQ(flatInfo.mipRange.first, 1u);
    EXPECT_EQ(flatInfo.mipRange.num, 2u);
    EXPECT_EQ(flatInfo.allMips, 0u);
    EXPECT_EQ(flatInfo.isSRGB, 0u);

    // Depth-stencil texture stencil plane & alias format resolution
    resource::TextureCreateDesc dsDesc =
        resource::TextureCreateDesc::create2D("SceneDepth", 512, 512, PixelFormat::D24_UNorm_S8_UInt)
            .addFlags(
                resource::TextureCreateFlags::DepthStencilTargetable | resource::TextureCreateFlags::ShaderResource
            )
            .determineInitialState();
    resource::TextureRef dsTex(new MockTexture(dsDesc));

    const ViewDesc stencilByPlaneDesc = ViewDesc::createTextureSRV()
                                            .setDimensionFromTexture(dsTex.get())
                                            .setPlane(resource::TexturePlane::Stencil)
                                            .asViewDesc();
    const ViewDesc::TextureSRV::ViewInfo stencilByPlaneInfo = stencilByPlaneDesc.texture.srv.getViewInfo(dsTex.get());
    EXPECT_EQ(stencilByPlaneInfo.plane, resource::TexturePlane::Stencil);
    EXPECT_EQ(stencilByPlaneInfo.format, PixelFormat::X24_G8_UInt);

    const ViewDesc stencilByFormatDesc = ViewDesc::createTextureSRV()
                                             .setDimensionFromTexture(dsTex.get())
                                             .setFormat(PixelFormat::X24_G8_UInt)
                                             .asViewDesc();
    const ViewDesc::TextureSRV::ViewInfo stencilByFormatInfo = stencilByFormatDesc.texture.srv.getViewInfo(dsTex.get());
    EXPECT_EQ(stencilByFormatInfo.plane, resource::TexturePlane::Stencil);
    EXPECT_EQ(stencilByFormatInfo.format, PixelFormat::X24_G8_UInt);

    // Texture UAV single-mip binding
    resource::TextureCreateDesc uavTexDesc =
        resource::TextureCreateDesc::create2DArray("ComputeTargets", 256, 256, 4u, PixelFormat::R16G16B16A16_Float)
            .setNumMips(3u)
            .addFlags(resource::TextureCreateFlags::UAV)
            .determineInitialState();
    resource::TextureRef uavTex(new MockTexture(uavTexDesc));

    const ViewDesc uavDesc = ViewDesc::createTextureUAV()
                                 .setDimensionFromTexture(uavTex.get())
                                 .setMipLevel(2u)
                                 .setArrayRange(1u, 2u)
                                 .asViewDesc();
    EXPECT_TRUE(uavDesc.isUAV());
    EXPECT_TRUE(uavDesc.isTexture());
    EXPECT_EQ(descriptorTypeFromViewDesc(uavDesc), DescriptorType::TextureUAV);

    const ViewDesc::TextureUAV::ViewInfo uavInfo = uavDesc.texture.uav.getViewInfo(uavTex.get());
    EXPECT_EQ(uavInfo.mipLevel, 2u);
    EXPECT_EQ(uavInfo.arrayRange.first, 1u);
    EXPECT_EQ(uavInfo.arrayRange.num, 2u);
    EXPECT_EQ(uavInfo.allMips, 0u);
    EXPECT_EQ(uavInfo.allSlices, 0u);

    // ViewDesc bytewise equality
    const ViewDesc uavDescCopy = ViewDesc::createTextureUAV()
                                     .setDimensionFromTexture(uavTex.get())
                                     .setMipLevel(2u)
                                     .setArrayRange(1u, 2u)
                                     .asViewDesc();
    EXPECT_EQ(uavDesc, uavDescCopy);
    EXPECT_NE(uavDesc, cubeSrvDesc);
}

TEST_F(ViewTestFixture, ViewClassesRetainTargetResourcesAndCascadeDeferredDeletion)
{
    ShaderResourceViewRef srv{};
    UnorderedAccessViewRef uav{};
    {
        resource::BufferCreateDesc bufDesc = resource::BufferCreateDesc::createStructured("ViewTargetBuf", 128u, 16u)
                                                 .addUsage(resource::BufferUsageFlags::ShaderResource)
                                                 .setInitialState(sync::ResourceAccess::SRVMask);
        resource::BufferRef buf(new MockBuffer(bufDesc));

        const ViewDesc bufSrvDesc = ViewDesc::createBufferSRV().setTypeFromBuffer(buf.get()).asViewDesc();
        const DescriptorHandle bindless(DescriptorType::BufferSRV, 55u);
        srv = ShaderResourceViewRef(new MockBindlessSRV(buf.get(), bufSrvDesc, bindless));

        EXPECT_EQ(srv->getType(), resource::ResourceType::ShaderResourceView);
        EXPECT_TRUE(srv->isBuffer());
        EXPECT_FALSE(srv->isTexture());
        EXPECT_EQ(srv->getResource(), buf.get());
        EXPECT_EQ(srv->getBuffer(), buf.get());
        EXPECT_EQ(srv->getDesc(), bufSrvDesc);
        EXPECT_EQ(srv->getBindlessHandle(), bindless);
        EXPECT_EQ(buf->getRefCount(), 2u);

        resource::TextureCreateDesc texDesc =
            resource::TextureCreateDesc::create2D("ViewTargetTex", 64, 64, PixelFormat::R8G8B8A8_UNorm)
                .addFlags(resource::TextureCreateFlags::UAV)
                .determineInitialState();
        resource::TextureRef tex(new MockTexture(texDesc));

        const ViewDesc texUavDesc = ViewDesc::createTextureUAV().setDimensionFromTexture(tex.get()).asViewDesc();
        uav = UnorderedAccessViewRef(new UnorderedAccessView(tex.get(), texUavDesc));

        EXPECT_EQ(uav->getType(), resource::ResourceType::UnorderedAccessView);
        EXPECT_TRUE(uav->isTexture());
        EXPECT_FALSE(uav->isBuffer());
        EXPECT_EQ(uav->getResource(), tex.get());
        EXPECT_EQ(uav->getTexture(), tex.get());
        EXPECT_EQ(uav->getDesc(), texUavDesc);
        EXPECT_FALSE(uav->getBindlessHandle().isValid());
        EXPECT_EQ(tex->getRefCount(), 2u);
    }

    // Flushing while views remain alive keeps both underlying resources alive.
    resource::ResourceDeletionQueue::flushPendingDeletes(true);
    EXPECT_EQ(MockBuffer::s_destructCount.load(), 0u);
    EXPECT_EQ(MockTexture::s_destructCount.load(), 0u);

    // Releasing views defers deletion until flush, which cascades to the underlying resources.
    srv.reset();
    uav.reset();
    EXPECT_EQ(MockBuffer::s_destructCount.load(), 0u);
    EXPECT_EQ(MockTexture::s_destructCount.load(), 0u);

    resource::ResourceDeletionQueue::flushPendingDeletes(true);
    EXPECT_EQ(MockBuffer::s_destructCount.load(), 1u);
    EXPECT_EQ(MockTexture::s_destructCount.load(), 1u);
}

TEST_F(ViewTestFixture, RangeHelpersAndExhaustiveViewBranches)
{
    // Range8 / Range16 helpers
    const resource::Range8 emptyRange{};
    EXPECT_EQ(emptyRange.first, 0u);
    EXPECT_EQ(emptyRange.num, 0u);
    EXPECT_EQ(emptyRange.exclusiveLast(), 0u);
    EXPECT_FALSE(emptyRange.isInRange(0u));

    const resource::Range16 sliceRange(2u, 4u);
    EXPECT_EQ(sliceRange.exclusiveLast(), 6u);
    EXPECT_EQ(sliceRange.inclusiveLast(), 5u);
    EXPECT_FALSE(sliceRange.isInRange(1u));
    EXPECT_TRUE(sliceRange.isInRange(2u));
    EXPECT_TRUE(sliceRange.isInRange(5u));
    EXPECT_FALSE(sliceRange.isInRange(6u));
    EXPECT_EQ(sliceRange, resource::Range16(2u, 4u));
    EXPECT_NE(sliceRange, resource::Range16(2u, 3u));

    // Default ViewDesc constructor
    const ViewDesc defaultDescA{};
    const ViewDesc defaultDescB{};
    EXPECT_EQ(defaultDescA, defaultDescB);
    EXPECT_TRUE(defaultDescA.isSRV());
    EXPECT_TRUE(defaultDescA.isBuffer());

    // Typed buffer SRV with 4-byte format and 4-byte aligned offset under default alignment
    resource::BufferCreateDesc floatBufDesc =
        resource::BufferCreateDesc::create("FloatBuf", 64u, 0u, resource::BufferUsageFlags::ShaderResource)
            .setInitialState(sync::ResourceAccess::SRVMask);
    resource::BufferRef floatBuf(new MockBuffer(floatBufDesc));

    const ViewDesc float4OffsetDesc = ViewDesc::createBufferSRV()
                                          .setType(BufferViewType::Typed)
                                          .setFormat(PixelFormat::R32_Float)
                                          .setOffsetInBytes(4u)
                                          .setNumElements(4u)
                                          .asViewDesc();
    const ViewDesc::BufferSRV::ViewInfo float4Info = float4OffsetDesc.buffer.srv.getViewInfo(floatBuf.get());
    EXPECT_EQ(float4Info.offsetInBytes, 4u);
    EXPECT_EQ(float4Info.strideInBytes, 4u);
    EXPECT_EQ(float4Info.numElements, 4u);
    EXPECT_EQ(float4Info.sizeInBytes, 16u);

    // Whole-buffer typed SRV & raw view with supportsRawViewsForAnyBuffer == true
    const ViewDesc wholeTypedDesc =
        ViewDesc::createBufferSRV().setType(BufferViewType::Typed).setFormat(PixelFormat::R32_Float).asViewDesc();
    const ViewDesc::BufferSRV::ViewInfo wholeTypedInfo = wholeTypedDesc.buffer.srv.getViewInfo(floatBuf.get(), 16u);
    EXPECT_EQ(wholeTypedInfo.numElements, 16u);
    EXPECT_EQ(wholeTypedInfo.sizeInBytes, 64u);

    const ViewDesc rawAnyBufDesc = ViewDesc::createBufferSRV().setType(BufferViewType::Raw).asViewDesc();
    const ViewDesc::BufferSRV::ViewInfo rawAnyInfo = rawAnyBufDesc.buffer.srv.getViewInfo(floatBuf.get(), 0u, true);
    EXPECT_EQ(rawAnyInfo.bufferType, BufferViewType::Raw);
    EXPECT_EQ(rawAnyInfo.numElements, 16u);

    // Structured buffer SRV stride override
    resource::BufferCreateDesc structBufDesc = resource::BufferCreateDesc::createStructured("StrideBuf", 256u, 16u)
                                                   .addUsage(resource::BufferUsageFlags::UnorderedAccess)
                                                   .setInitialState(sync::ResourceAccess::UAVMask);
    resource::BufferRef structBuf(new MockBuffer(structBufDesc));

    const ViewDesc customStrideSrv = ViewDesc::createBufferSRV()
                                         .setTypeFromBuffer(structBuf.get())
                                         .setStride(32u)
                                         .setOffsetInBytes(32u)
                                         .setNumElements(3u)
                                         .asViewDesc();
    const ViewDesc::BufferSRV::ViewInfo customStrideInfo = customStrideSrv.buffer.srv.getViewInfo(structBuf.get());
    EXPECT_EQ(customStrideInfo.strideInBytes, 32u);
    EXPECT_EQ(customStrideInfo.numElements, 3u);
    EXPECT_EQ(customStrideInfo.sizeInBytes, 96u);

    // BufferUAV setType / setFormat / setOffsetInBytes / setStride / setNumElements / setTypeFromBuffer variants + null
    const ViewDesc typedUavDesc = ViewDesc::createBufferUAV()
                                      .setTypeFromBuffer(floatBuf.get())
                                      .setFormat(PixelFormat::R32_UInt)
                                      .setOffsetInBytes(16u)
                                      .setNumElements(4u)
                                      .setAtomicCounter(false)
                                      .setAppendBuffer(false)
                                      .asViewDesc();
    const ViewDesc::BufferUAV::ViewInfo typedUavInfo = typedUavDesc.buffer.uav.getViewInfo(floatBuf.get());
    EXPECT_EQ(typedUavInfo.bufferType, BufferViewType::Typed);
    EXPECT_FALSE(typedUavInfo.atomicCounter);
    EXPECT_FALSE(typedUavInfo.appendBuffer);
    EXPECT_EQ(typedUavInfo.sizeInBytes, 16u);

    const ViewDesc structUavOverride = ViewDesc::createBufferUAV()
                                           .setType(BufferViewType::Structured)
                                           .setStride(64u)
                                           .setOffsetInBytes(64u)
                                           .setNumElements(2u)
                                           .asViewDesc();
    const ViewDesc::BufferUAV::ViewInfo structUavInfo = structUavOverride.buffer.uav.getViewInfo(structBuf.get());
    EXPECT_EQ(structUavInfo.strideInBytes, 64u);
    EXPECT_EQ(structUavInfo.sizeInBytes, 128u);

    UnorderedAccessViewRef bufUavObj(new UnorderedAccessView(structBuf.get(), structUavOverride));
    EXPECT_TRUE(bufUavObj->isBuffer());
    EXPECT_EQ(bufUavObj->getBuffer(), structBuf.get());

    resource::BufferCreateDesc nullBufDesc = resource::BufferCreateDesc::createNull("NullUavBuf");
    resource::BufferRef nullBuf(new MockBuffer(nullBufDesc));
    const ViewDesc nullUavDesc = ViewDesc::createBufferUAV().setType(BufferViewType::Raw).asViewDesc();
    EXPECT_TRUE(nullUavDesc.buffer.uav.getViewInfo(nullBuf.get()).isNullView);

    // 3D texture SRV/UAV, single Cube SRV, D32_Float_S8_UInt stencil plane & metadata planes
    resource::TextureCreateDesc volDesc =
        resource::TextureCreateDesc::create3D("VolumeTex", 16, 16, 8, PixelFormat::R16_Float)
            .addFlags(resource::TextureCreateFlags::ShaderResource | resource::TextureCreateFlags::UAV)
            .determineInitialState();
    resource::TextureRef volTex(new MockTexture(volDesc));

    const ViewDesc volSrvDesc = ViewDesc::createTextureSRV()
                                    .setDimension(resource::TextureDimension::Texture3D)
                                    .setFormat(PixelFormat::R16_UNorm)
                                    .asViewDesc();
    const ViewDesc::TextureSRV::ViewInfo volSrvInfo = volSrvDesc.texture.srv.getViewInfo(volTex.get());
    EXPECT_EQ(volSrvInfo.dimension, TextureViewDimension::Texture3D);
    EXPECT_EQ(volSrvInfo.format, PixelFormat::R16_UNorm);
    EXPECT_EQ(volSrvInfo.isSRGB, 0u);
    EXPECT_EQ(volSrvInfo.allMips, 1u);

    ShaderResourceViewRef texSrvObj(new ShaderResourceView(volTex.get(), volSrvDesc));
    EXPECT_TRUE(texSrvObj->isTexture());
    EXPECT_EQ(texSrvObj->getTexture(), volTex.get());

    const ViewDesc volUavDesc = ViewDesc::createTextureUAV()
                                    .setDimension(resource::TextureDimension::Texture3D)
                                    .setFormat(PixelFormat::R16_Float)
                                    .setPlane(resource::TexturePlane::Primary)
                                    .asViewDesc();
    const ViewDesc::TextureUAV::ViewInfo volUavInfo = volUavDesc.texture.uav.getViewInfo(volTex.get());
    EXPECT_EQ(volUavInfo.allMips, 1u);
    EXPECT_EQ(volUavInfo.allSlices, 1u);

    resource::TextureCreateDesc singleCubeDesc =
        resource::TextureCreateDesc::createCube("SingleCube", 32u, PixelFormat::R8G8B8A8_UNorm)
            .addFlags(resource::TextureCreateFlags::ShaderResource)
            .determineInitialState();
    resource::TextureRef singleCubeTex(new MockTexture(singleCubeDesc));
    const ViewDesc singleCubeSrv = ViewDesc::createTextureSRV()
                                       .setDimension(resource::TextureDimension::TextureCube)
                                       .setArrayRange(0u, 1u)
                                       .asViewDesc();
    const ViewDesc::TextureSRV::ViewInfo singleCubeInfo = singleCubeSrv.texture.srv.getViewInfo(singleCubeTex.get());
    EXPECT_EQ(singleCubeInfo.dimension, TextureViewDimension::TextureCube);
    EXPECT_EQ(singleCubeInfo.allSlices, 1u);

    resource::TextureCreateDesc ds32Desc =
        resource::TextureCreateDesc::create2D("SceneDepth32", 64, 64, PixelFormat::D32_Float_S8_UInt)
            .addFlags(
                resource::TextureCreateFlags::DepthStencilTargetable | resource::TextureCreateFlags::ShaderResource
            )
            .determineInitialState();
    resource::TextureRef ds32Tex(new MockTexture(ds32Desc));

    const ViewDesc d32StencilDesc = ViewDesc::createTextureSRV()
                                        .setDimensionFromTexture(ds32Tex.get())
                                        .setPlane(resource::TexturePlane::Stencil)
                                        .asViewDesc();
    EXPECT_EQ(d32StencilDesc.texture.srv.getViewInfo(ds32Tex.get()).format, PixelFormat::X32_G8_UInt);

    const ViewDesc d32PromoteDesc = ViewDesc::createTextureSRV()
                                        .setDimensionFromTexture(ds32Tex.get())
                                        .setPlane(resource::TexturePlane::PrimaryCompressed)
                                        .setFormat(PixelFormat::X32_G8_UInt)
                                        .asViewDesc();
    EXPECT_EQ(d32PromoteDesc.texture.srv.getViewInfo(ds32Tex.get()).plane, resource::TexturePlane::Stencil);

    const ViewDesc htileDesc = ViewDesc::createTextureSRV()
                                   .setDimensionFromTexture(ds32Tex.get())
                                   .setPlane(resource::TexturePlane::HTile)
                                   .asViewDesc();
    EXPECT_EQ(htileDesc.texture.srv.getViewInfo(ds32Tex.get()).plane, resource::TexturePlane::HTile);

    const ViewDesc depthPlaneDesc = ViewDesc::createTextureSRV()
                                        .setDimensionFromTexture(ds32Tex.get())
                                        .setPlane(resource::TexturePlane::Depth)
                                        .asViewDesc();
    EXPECT_EQ(depthPlaneDesc.texture.srv.getViewInfo(ds32Tex.get()).plane, resource::TexturePlane::Depth);
}

}   // namespace gp::rhi::view::tests
