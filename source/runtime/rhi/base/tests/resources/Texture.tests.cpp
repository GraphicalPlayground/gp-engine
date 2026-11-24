// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "resources/Texture.hpp"
#include "containers/strings/Name.hpp"
#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "core/ClearValueBinding.hpp"
#include "core/GPUMask.hpp"
#include "core/Resource.hpp"
#include "core/ResourceDeletionQueue.hpp"
#include "core/ResourceEnums.hpp"
#include "core/ResourceTypes.hpp"
#include "core/ViewableResource.hpp"
#include "CoreMinimal.hpp"
#include "hashing/Fnv1a.hpp"
#include "hashing/HashOf.hpp"
#include "maths/color/LinearColor.hpp"
#include "memory/MemoryOrder.hpp"
#include "memory/pointers/PointerConcepts.hpp"
#include "memory/pointers/RefCountedPtr.hpp"
#include "resources/TextureCreateDesc.hpp"
#include "resources/TextureDesc.hpp"
#include "resources/TextureEnums.hpp"
#include "resources/TextureReference.hpp"
#include "RHIPixelFormat.hpp"
#include "sync/SyncEnums.hpp"
#include "sync/TransitionDesc.hpp"
#include "threading/atomics/Atomic.hpp"
#include "views/DescriptorHandle.hpp"
#include "views/ViewEnums.hpp"
#include <gtest/gtest.h>
#include <type_traits>
#include <utility>

namespace gp::rhi::resource::tests
{

static_assert(std::is_base_of_v<ViewableResource, Texture>, "Texture must inherit publicly from ViewableResource.");
static_assert(std::is_base_of_v<Texture, TextureReference>, "TextureReference must inherit publicly from Texture.");
static_assert(gp::concepts::IsRefCounted<Texture>, "Texture must satisfy gp::concepts::IsRefCounted.");
static_assert(
    gp::concepts::IsRefCounted<TextureReference>, "TextureReference must satisfy gp::concepts::IsRefCounted."
);
static_assert(
    gp::concepts::HasRefCountQuery<gp::DefaultRefCountPolicy<Texture>, Texture>,
    "Texture must satisfy gp::concepts::HasRefCountQuery."
);
static_assert(
    gp::concepts::HasRefCountQuery<gp::DefaultRefCountPolicy<TextureReference>, TextureReference>,
    "TextureReference must satisfy gp::concepts::HasRefCountQuery."
);
static_assert(sizeof(TextureRef) == sizeof(Texture*), "TextureRef must remain a single pointer.");
static_assert(
    sizeof(TextureReferenceRef) == sizeof(TextureReference*), "TextureReferenceRef must remain a single pointer."
);
static_assert(!std::is_default_constructible_v<Texture>, "Texture must not be default constructible.");
static_assert(!std::is_copy_constructible_v<Texture>, "Texture must not be copy constructible.");
static_assert(!std::is_copy_assignable_v<Texture>, "Texture must not be copy assignable.");
static_assert(!std::is_move_constructible_v<Texture>, "Texture must not be move constructible.");
static_assert(!std::is_move_assignable_v<Texture>, "Texture must not be move assignable.");
static_assert(
    !std::is_default_constructible_v<TextureReference>, "TextureReference must not be default constructible."
);
static_assert(!std::is_copy_constructible_v<TextureReference>, "TextureReference must not be copy constructible.");
static_assert(!std::is_copy_assignable_v<TextureReference>, "TextureReference must not be copy assignable.");
static_assert(!std::is_move_constructible_v<TextureReference>, "TextureReference must not be move constructible.");
static_assert(!std::is_move_assignable_v<TextureReference>, "TextureReference must not be move assignable.");

namespace
{

/// @brief Mock bulk data provider used to test TextureCreateDesc bulk-data initialization.
class MockResourceBulkData final : public IResourceBulkData
{
public:
    /// @brief Constructs a bulk data provider over caller-owned memory.
    /// @param[in] inData Pointer to bulk payload bytes.
    /// @param[in] inSizeBytes Payload byte size.
    MockResourceBulkData(const void* const inData, const gp::UInt32 inSizeBytes) noexcept
        : m_data(inData)
        , m_sizeBytes(inSizeBytes)
    {}

    /// @copydoc IResourceBulkData::getResourceBulkData()
    [[nodiscard]] virtual const void* getResourceBulkData() const noexcept override
    {
        return m_data;
    }

    /// @copydoc IResourceBulkData::getResourceBulkDataSize()
    [[nodiscard]] virtual gp::UInt32 getResourceBulkDataSize() const noexcept override
    {
        return m_sizeBytes;
    }

    /// @copydoc IResourceBulkData::discard()
    virtual void discard() noexcept override
    {
        m_data = nullptr;
        m_sizeBytes = 0u;
    }

private:
    const void* m_data{ nullptr };
    gp::UInt32 m_sizeBytes{ 0u };
};

/// @brief Minimal concrete Texture subclass inheriting every default virtual method from Texture.
class MockDefaultTexture final : public Texture
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

    /// @brief Constructs a concrete mock texture from a creation descriptor.
    /// @param[in] createDesc Creation parameters passed to Texture.
    explicit MockDefaultTexture(const TextureCreateDesc& createDesc) noexcept
        : Texture(createDesc)
    {
        s_constructCount.fetchAdd(1u, gp::memory::MemoryOrder::Relaxed);
    }

    /// @copydoc Texture::~Texture()
    virtual ~MockDefaultTexture() noexcept override
    {
        s_destructCount.fetchAdd(1u, gp::memory::MemoryOrder::Relaxed);
    }
};

/// @brief Concrete Texture subclass with configurable backend handles for TextureReference forwarding tests.
class MockBackendTexture final : public Texture
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

    /// @brief Constructs a mock backend texture with explicit native handles and metadata.
    /// @param[in] createDesc Creation parameters passed to Texture.
    /// @param[in] bindlessHandle Bindless handle returned by getDefaultBindlessHandle().
    /// @param[in] nativeResource Opaque handle returned by getNativeResource().
    /// @param[in] nativeSRV Opaque handle returned by getNativeShaderResourceView().
    /// @param[in] textureBase Opaque handle returned by getTextureBaseRHI().
    /// @param[in] writeMaskData Pointer returned by getWriteMaskProperties().
    /// @param[in] writeMaskSize Byte size returned by getWriteMaskProperties().
    MockBackendTexture(
        const TextureCreateDesc& createDesc,
        const view::DescriptorHandle bindlessHandle,
        void* const nativeResource,
        void* const nativeSRV,
        void* const textureBase,
        void* const writeMaskData,
        const gp::UInt32 writeMaskSize
    ) noexcept
        : Texture(createDesc)
        , m_bindlessHandle(bindlessHandle)
        , m_nativeResource(nativeResource)
        , m_nativeSRV(nativeSRV)
        , m_textureBase(textureBase)
        , m_writeMaskData(writeMaskData)
        , m_writeMaskSize(writeMaskSize)
    {
        s_constructCount.fetchAdd(1u, gp::memory::MemoryOrder::Relaxed);
    }

    /// @copydoc Texture::~Texture()
    virtual ~MockBackendTexture() noexcept override
    {
        s_destructCount.fetchAdd(1u, gp::memory::MemoryOrder::Relaxed);
    }

    /// @copydoc Texture::getDefaultBindlessHandle()
    [[nodiscard]] virtual view::DescriptorHandle getDefaultBindlessHandle() const noexcept override
    {
        return m_bindlessHandle;
    }

    /// @copydoc Texture::getNativeResource()
    [[nodiscard]] virtual void* getNativeResource() const noexcept override
    {
        return m_nativeResource;
    }

    /// @copydoc Texture::getNativeShaderResourceView()
    [[nodiscard]] virtual void* getNativeShaderResourceView() const noexcept override
    {
        return m_nativeSRV;
    }

    /// @copydoc Texture::getTextureBaseRHI()
    [[nodiscard]] virtual void* getTextureBaseRHI() noexcept override
    {
        return m_textureBase;
    }

    /// @copydoc Texture::getWriteMaskProperties()
    virtual void getWriteMaskProperties(void*& outData, gp::UInt32& outSize) noexcept override
    {
        outData = m_writeMaskData;
        outSize = m_writeMaskSize;
    }

private:
    view::DescriptorHandle m_bindlessHandle{};
    void* m_nativeResource{ nullptr };
    void* m_nativeSRV{ nullptr };
    void* m_textureBase{ nullptr };
    void* m_writeMaskData{ nullptr };
    gp::UInt32 m_writeMaskSize{ 0u };
};

/// @brief Test subclass of TextureReference exposing protected target swap and state tracking hooks.
class MockTextureReference final : public TextureReference
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

    /// @brief Constructs a non-bindless texture reference targeting referencedTexture.
    /// @param[in] referencedTexture Initial target texture, or nullptr to fall back to default texture.
    explicit MockTextureReference(Texture* const referencedTexture) noexcept
        : TextureReference(referencedTexture)
    {
        s_constructCount.fetchAdd(1u, gp::memory::MemoryOrder::Relaxed);
    }

    /// @brief Constructs a bindless texture reference targeting referencedTexture.
    /// @param[in] referencedTexture Initial target texture, or nullptr to fall back to default texture.
    /// @param[in] bindlessHandle Dedicated bindless descriptor slot.
    MockTextureReference(Texture* const referencedTexture, const view::DescriptorHandle bindlessHandle) noexcept
        : TextureReference(referencedTexture, bindlessHandle)
    {
        s_constructCount.fetchAdd(1u, gp::memory::MemoryOrder::Relaxed);
    }

    /// @copydoc TextureReference::~TextureReference()
    virtual ~MockTextureReference() noexcept override
    {
        s_destructCount.fetchAdd(1u, gp::memory::MemoryOrder::Relaxed);
    }

    /// @brief Exposes protected TextureReference::setReferencedTexture() to tests.
    /// @param[in] newTexture Replacement target texture, or nullptr to fall back to default texture.
    void swapTargetForTest(Texture* const newTexture) noexcept
    {
        setReferencedTexture(newTexture);
    }

    /// @brief Exposes protected TextureReference::setTrackedAccessFromContext() to tests.
    /// @param[in] trackedAccess New access mask and pipeline state forwarded to the active target.
    void updateTrackedAccessForTest(const sync::TrackedAccess trackedAccess) noexcept
    {
        setTrackedAccessFromContext(trackedAccess);
    }
};

/// @brief Builds a valid 2D TextureCreateDesc for concise test setup.
/// @param[in] debugName Debug label for the texture.
/// @param[in] width Base mip width in texels.
/// @param[in] height Base mip height in texels.
/// @param[in] format Pixel format.
/// @return Configured TextureCreateDesc with resolved initial state.
[[nodiscard]] TextureCreateDesc makeValid2DCreateDesc(
    const gp::StringView debugName,
    const gp::Int32 width,
    const gp::Int32 height,
    const PixelFormat format = PixelFormat::R8G8B8A8_UNorm
) noexcept
{
    TextureCreateDesc desc = TextureCreateDesc::create2D(debugName, width, height, format);
    desc.setFlags(TextureCreateFlags::ShaderResource).determineInitialState();
    return desc;
}

/// @brief Test fixture clearing global TextureReference default texture and flushing deferred deletions.
class TextureTestFixture : public ::testing::Test
{
protected:
    /// @copydoc ::testing::Test::SetUp()
    virtual void SetUp() override
    {
#if GP_RHI_ENABLE_RESOURCE_INFO
        Resource::stopTrackingAllResources();
#endif
        TextureReference::setDefaultTexture(nullptr);
        ResourceDeletionQueue::setBypassDeletionQueueCheckOnExit(false);
        ResourceDeletionQueue::flushPendingDeletes(true);
        MockDefaultTexture::resetCounters();
        MockBackendTexture::resetCounters();
        MockTextureReference::resetCounters();
    }

    /// @copydoc ::testing::Test::TearDown()
    virtual void TearDown() override
    {
        TextureReference::setDefaultTexture(nullptr);
        ResourceDeletionQueue::flushPendingDeletes(true);
#if GP_RHI_ENABLE_RESOURCE_INFO
        Resource::stopTrackingAllResources();
#endif
        ResourceDeletionQueue::setBypassDeletionQueueCheckOnExit(false);
    }
};

}   // namespace

TEST(TextureDescTest, ConstructorsPredicatesResetAndSize)
{
    const TextureDesc defaultDesc{};
    const ClearValueBinding expectedDefaultClear{};
    const IntPoint expectedUnitExtent(1, 1);
    const IntVector expectedUnitSize(1, 1, 1);
    EXPECT_EQ(defaultDesc.flags, TextureCreateFlags::None);
    EXPECT_EQ(defaultDesc.clearValue, expectedDefaultClear);
    EXPECT_EQ(defaultDesc.gpuMask, GPUMask::all());
    EXPECT_EQ(defaultDesc.extData, 0u);
    EXPECT_EQ(defaultDesc.extent, expectedUnitExtent);
    EXPECT_EQ(defaultDesc.depth, 1u);
    EXPECT_EQ(defaultDesc.arraySize, 1u);
    EXPECT_EQ(defaultDesc.numMips, 1u);
    EXPECT_EQ(defaultDesc.numSamples, 1u);
    EXPECT_EQ(defaultDesc.dimension, TextureDimension::Texture2D);
    EXPECT_EQ(defaultDesc.format, PixelFormat::Unknown);
    EXPECT_EQ(defaultDesc.uavFormat, PixelFormat::Unknown);
    EXPECT_EQ(defaultDesc.fastVRAMPercentage, 0xFFu);
    EXPECT_TRUE(defaultDesc.aliasableFormats.isEmpty());
    EXPECT_EQ(defaultDesc.getSize(), expectedUnitSize);

    // Dimension constructor and dimension predicates across all five texture dimensions.
    const TextureDesc dim2D(TextureDimension::Texture2D);
    EXPECT_TRUE(dim2D.isTexture2D());
    EXPECT_FALSE(dim2D.isTexture3D());
    EXPECT_FALSE(dim2D.isTextureCube());
    EXPECT_FALSE(dim2D.isTextureArray());

    const TextureDesc dim2DArray(TextureDimension::Texture2DArray);
    EXPECT_TRUE(dim2DArray.isTexture2D());
    EXPECT_FALSE(dim2DArray.isTexture3D());
    EXPECT_FALSE(dim2DArray.isTextureCube());
    EXPECT_TRUE(dim2DArray.isTextureArray());

    const TextureDesc dim3D(TextureDimension::Texture3D);
    EXPECT_FALSE(dim3D.isTexture2D());
    EXPECT_TRUE(dim3D.isTexture3D());
    EXPECT_FALSE(dim3D.isTextureCube());
    EXPECT_FALSE(dim3D.isTextureArray());

    const TextureDesc dimCube(TextureDimension::TextureCube);
    EXPECT_FALSE(dimCube.isTexture2D());
    EXPECT_FALSE(dimCube.isTexture3D());
    EXPECT_TRUE(dimCube.isTextureCube());
    EXPECT_FALSE(dimCube.isTextureArray());

    const TextureDesc dimCubeArray(TextureDimension::TextureCubeArray);
    EXPECT_FALSE(dimCubeArray.isTexture2D());
    EXPECT_FALSE(dimCubeArray.isTexture3D());
    EXPECT_TRUE(dimCubeArray.isTextureCube());
    EXPECT_TRUE(dimCubeArray.isTextureArray());

    // Ten-argument constructor populates all core layout fields.
    const IntPoint extent3D(128, 64);
    const TextureDesc fullDesc(
        TextureDimension::Texture3D,
        TextureCreateFlags::ShaderResource | TextureCreateFlags::UAV,
        PixelFormat::R16G16B16A16_Float,
        ClearValueBinding::kBlack,
        extent3D,
        32u,
        1u,
        5u,
        1u,
        0xCAFEu
    );
    const IntVector expectedSize3D(128, 64, 32);
    EXPECT_EQ(fullDesc.dimension, TextureDimension::Texture3D);
    EXPECT_EQ(fullDesc.flags, TextureCreateFlags::ShaderResource | TextureCreateFlags::UAV);
    EXPECT_EQ(fullDesc.format, PixelFormat::R16G16B16A16_Float);
    EXPECT_EQ(fullDesc.clearValue, ClearValueBinding::kBlack);
    EXPECT_EQ(fullDesc.extent, extent3D);
    EXPECT_EQ(fullDesc.depth, 32u);
    EXPECT_EQ(fullDesc.arraySize, 1u);
    EXPECT_EQ(fullDesc.numMips, 5u);
    EXPECT_EQ(fullDesc.numSamples, 1u);
    EXPECT_EQ(fullDesc.extData, 0xCAFEu);
    EXPECT_TRUE(fullDesc.isMipChain());
    EXPECT_FALSE(fullDesc.isMultisample());
    EXPECT_EQ(fullDesc.getSize(), expectedSize3D);

    // reset() restores single-sampled render-target defaults and clears aliasableFormats.
    TextureDesc resetTarget(
        TextureDimension::Texture2D,
        TextureCreateFlags::ShaderResource | TextureCreateFlags::SRGB | TextureCreateFlags::UAV |
            TextureCreateFlags::ResolveTargetable | TextureCreateFlags::DepthStencilResolveTarget |
            TextureCreateFlags::Memoryless,
        PixelFormat::R8G8B8A8_UNorm,
        ClearValueBinding::kWhite,
        IntPoint(256, 256),
        1u,
        1u,
        1u,
        4u,
        0u
    );
    resetTarget.aliasableFormats.emplaceBack(PixelFormat::R32_UInt);
    EXPECT_TRUE(resetTarget.isMultisample());
    EXPECT_FALSE(resetTarget.isMipChain());
    EXPECT_EQ(resetTarget.aliasableFormats.size(), 1u);

    resetTarget.reset();
    EXPECT_EQ(resetTarget.numSamples, 1u);
    EXPECT_FALSE(resetTarget.isMultisample());
    const TextureCreateFlags expectedAfterReset =
        TextureCreateFlags::ShaderResource | TextureCreateFlags::SRGB | TextureCreateFlags::RenderTargetable;
    EXPECT_EQ(resetTarget.flags, expectedAfterReset);
    EXPECT_TRUE(resetTarget.aliasableFormats.isEmpty());
}

TEST(TextureDescTest, EqualityAndHashSensitivity)
{
    TextureDesc base(
        TextureDimension::Texture2DArray,
        TextureCreateFlags::ShaderResource | TextureCreateFlags::UAV,
        PixelFormat::R8G8B8A8_UNorm,
        ClearValueBinding::kBlack,
        IntPoint(256, 128),
        1u,
        4u,
        3u,
        1u,
        42u
    );
    base.uavFormat = PixelFormat::R32_UInt;
    base.fastVRAMPercentage = 128u;
    base.gpuMask = GPUMask::fromIndex(0u);
    base.aliasableFormats.emplaceBack(PixelFormat::B8G8R8A8_UNorm);

    const TextureDesc identical = base;
    EXPECT_EQ(base, identical);
    EXPECT_EQ(base.getHash(), identical.getHash());
    EXPECT_EQ(gp::hashing::hashOf(base), base.getHash());

    TextureDesc copyAssigned{};
    copyAssigned = base;
    EXPECT_EQ(copyAssigned, base);

    TextureDesc moveSource(base);
    TextureDesc moveAssigned{};
    moveAssigned = std::move(moveSource);
    EXPECT_EQ(moveAssigned, base);

    EXPECT_TRUE(base.isValid());
    const TextureDesc unknownFormatDesc{};
    EXPECT_FALSE(unknownFormatDesc.isValid());
    EXPECT_NE(unknownFormatDesc.getHash(), base.getHash());

    gp::hashing::Fnv1a64 baseStream{};
    base.hashAppend(baseStream);
    gp::hashing::Fnv1a64 identicalStream{};
    identical.hashAppend(identicalStream);
    EXPECT_EQ(baseStream.finalize(), identicalStream.finalize());

    const auto expectMutationChangesEqualityAndHash = [&base, &baseStream](const TextureDesc& mutated) -> void
    {
        EXPECT_NE(base, mutated);
        EXPECT_NE(base.getHash(), mutated.getHash());

        gp::hashing::Fnv1a64 mutatedStream{};
        mutated.hashAppend(mutatedStream);
        EXPECT_NE(baseStream.finalize(), mutatedStream.finalize());
    };

    TextureDesc diffDim = base;
    diffDim.dimension = TextureDimension::TextureCubeArray;
    expectMutationChangesEqualityAndHash(diffDim);

    TextureDesc diffFlags = base;
    diffFlags.flags = TextureCreateFlags::RenderTargetable;
    expectMutationChangesEqualityAndHash(diffFlags);

    TextureDesc diffFormat = base;
    diffFormat.format = PixelFormat::R16G16B16A16_Float;
    expectMutationChangesEqualityAndHash(diffFormat);

    TextureDesc diffUavFormat = base;
    diffUavFormat.uavFormat = PixelFormat::R8G8B8A8_UInt;
    expectMutationChangesEqualityAndHash(diffUavFormat);

    TextureDesc diffExtent = base;
    diffExtent.extent = IntPoint(512, 128);
    expectMutationChangesEqualityAndHash(diffExtent);

    TextureDesc diffDepth = base;
    diffDepth.depth = 2u;
    expectMutationChangesEqualityAndHash(diffDepth);

    TextureDesc diffArraySize = base;
    diffArraySize.arraySize = 8u;
    expectMutationChangesEqualityAndHash(diffArraySize);

    TextureDesc diffNumMips = base;
    diffNumMips.numMips = 5u;
    expectMutationChangesEqualityAndHash(diffNumMips);

    TextureDesc diffSamples = base;
    diffSamples.numSamples = 4u;
    expectMutationChangesEqualityAndHash(diffSamples);

    TextureDesc diffFastVram = base;
    diffFastVram.fastVRAMPercentage = 200u;
    expectMutationChangesEqualityAndHash(diffFastVram);

    TextureDesc diffClear = base;
    diffClear.clearValue = ClearValueBinding::kWhite;
    expectMutationChangesEqualityAndHash(diffClear);

    TextureDesc diffExtData = base;
    diffExtData.extData = 99u;
    expectMutationChangesEqualityAndHash(diffExtData);

    TextureDesc diffGpuMask = base;
    diffGpuMask.gpuMask = GPUMask::fromIndex(1u);
    expectMutationChangesEqualityAndHash(diffGpuMask);

    TextureDesc diffAliasValue = base;
    diffAliasValue.aliasableFormats[0] = PixelFormat::R32_UInt;
    expectMutationChangesEqualityAndHash(diffAliasValue);

    TextureDesc diffAliasCount = base;
    diffAliasCount.aliasableFormats.clear();
    expectMutationChangesEqualityAndHash(diffAliasCount);
}

TEST(TextureDescTest, GetSubresourceCountAcrossDimensionsAndDepthStencilFormats)
{
    TextureDesc desc2D(TextureDimension::Texture2D);
    desc2D.format = PixelFormat::R8G8B8A8_UNorm;
    desc2D.numMips = 5u;
    EXPECT_EQ(desc2D.getSubresourceCount(), 5u);

    TextureDesc desc2DArray(TextureDimension::Texture2DArray);
    desc2DArray.format = PixelFormat::R8G8B8A8_UNorm;
    desc2DArray.arraySize = 4u;
    desc2DArray.numMips = 3u;
    EXPECT_EQ(desc2DArray.getSubresourceCount(), 12u);

    TextureDesc desc3D(TextureDimension::Texture3D);
    desc3D.format = PixelFormat::R16_Float;
    desc3D.depth = 16u;
    desc3D.numMips = 4u;
    EXPECT_EQ(desc3D.getSubresourceCount(), 4u);

    TextureDesc descCube(TextureDimension::TextureCube);
    descCube.format = PixelFormat::R8G8B8A8_UNorm;
    descCube.numMips = 3u;
    EXPECT_EQ(descCube.getSubresourceCount(), 18u);

    TextureDesc descCubeArray(TextureDimension::TextureCubeArray);
    descCubeArray.format = PixelFormat::R8G8B8A8_UNorm;
    descCubeArray.arraySize = 2u;
    descCubeArray.numMips = 4u;
    EXPECT_EQ(descCubeArray.getSubresourceCount(), 48u);

    // Depth-only format uses a single plane, while depth-stencil formats use two planes.
    TextureDesc depthOnly(TextureDimension::Texture2D);
    depthOnly.format = PixelFormat::D32_Float;
    depthOnly.numMips = 2u;
    EXPECT_EQ(depthOnly.getSubresourceCount(), 2u);

    TextureDesc depth24Stencil8(TextureDimension::Texture2D);
    depth24Stencil8.format = PixelFormat::D24_UNorm_S8_UInt;
    depth24Stencil8.numMips = 3u;
    EXPECT_EQ(depth24Stencil8.getSubresourceCount(), 6u);

    TextureDesc depth32Stencil8(TextureDimension::Texture2DArray);
    depth32Stencil8.format = PixelFormat::D32_Float_S8_UInt;
    depth32Stencil8.arraySize = 3u;
    depth32Stencil8.numMips = 2u;
    EXPECT_EQ(depth32Stencil8.getSubresourceCount(), 12u);

    TextureDesc depthStencilCubeArray(TextureDimension::TextureCubeArray);
    depthStencilCubeArray.format = PixelFormat::D24_UNorm_S8_UInt;
    depthStencilCubeArray.arraySize = 2u;
    depthStencilCubeArray.numMips = 2u;
    EXPECT_EQ(depthStencilCubeArray.getSubresourceCount(), 48u);
}

TEST(TextureDescTest, IsValidPositiveAndEveryNegativeCase)
{
    TextureValidationLimits limits{};
    limits.maxTextureDimensions = 4096u;
    limits.maxCubeTextureDimensions = 2048u;
    limits.maxTextureArrayLayers = 64u;
    limits.maxTextureMipCount = 12u;
    limits.reservedTextureArrayMinMipDimension = 256u;
    limits.supportsReservedResources = true;
    limits.supportsReservedVolumeTextures = true;

    // Valid baseline descriptors across all five dimensions.
    TextureDesc valid2D(TextureDimension::Texture2D);
    valid2D.format = PixelFormat::R8G8B8A8_UNorm;
    valid2D.extent = IntPoint(512, 256);
    valid2D.numMips = 4u;
    EXPECT_TRUE(valid2D.isValid(limits));
    EXPECT_TRUE(TextureDesc::checkValidity(valid2D, "Valid2D", limits));

    TextureDesc valid2DArray(TextureDimension::Texture2DArray);
    valid2DArray.format = PixelFormat::R8G8B8A8_UNorm;
    valid2DArray.extent = IntPoint(256, 256);
    valid2DArray.arraySize = 8u;
    valid2DArray.numMips = 4u;
    EXPECT_TRUE(valid2DArray.isValid(limits));

    TextureDesc valid3D(TextureDimension::Texture3D);
    valid3D.format = PixelFormat::R16_Float;
    valid3D.extent = IntPoint(64, 64);
    valid3D.depth = 32u;
    valid3D.numMips = 4u;
    EXPECT_TRUE(valid3D.isValid(limits));

    TextureDesc validCube(TextureDimension::TextureCube);
    validCube.format = PixelFormat::R8G8B8A8_UNorm;
    validCube.extent = IntPoint(512, 512);
    validCube.numMips = 4u;
    EXPECT_TRUE(validCube.isValid(limits));

    TextureDesc validCubeArray(TextureDimension::TextureCubeArray);
    validCubeArray.format = PixelFormat::R8G8B8A8_UNorm;
    validCubeArray.extent = IntPoint(512, 512);
    validCubeArray.arraySize = 4u;
    validCubeArray.numMips = 4u;
    EXPECT_TRUE(validCubeArray.isValid(limits));

    TextureDesc validMsaa = valid2D;
    validMsaa.numMips = 1u;
    validMsaa.numSamples = 4u;
    EXPECT_TRUE(validMsaa.isValid(limits));

    // Negative format cases.
    TextureDesc unknownFormat = valid2D;
    unknownFormat.format = PixelFormat::Unknown;
    EXPECT_FALSE(unknownFormat.isValid(limits));

    TextureDesc outOfRangeFormat = valid2D;
    outOfRangeFormat.format = static_cast<PixelFormat>(0xFFu);
    EXPECT_FALSE(outOfRangeFormat.isValid(limits));

    // Negative extent cases.
    TextureDesc zeroWidth = valid2D;
    zeroWidth.extent.x = 0;
    EXPECT_FALSE(zeroWidth.isValid(limits));

    TextureDesc negativeWidth = valid2D;
    negativeWidth.extent.x = -4;
    EXPECT_FALSE(negativeWidth.isValid(limits));

    TextureDesc zeroHeight = valid2D;
    zeroHeight.extent.y = 0;
    EXPECT_FALSE(zeroHeight.isValid(limits));

    TextureDesc oversizedWidth = valid2D;
    oversizedWidth.extent.x = 4097;
    EXPECT_FALSE(oversizedWidth.isValid(limits));

    TextureDesc oversizedHeight = valid2D;
    oversizedHeight.extent.y = 4097;
    EXPECT_FALSE(oversizedHeight.isValid(limits));

    TextureDesc oversizedCubeX = validCube;
    oversizedCubeX.extent.x = 2049;
    EXPECT_FALSE(oversizedCubeX.isValid(limits));

    TextureDesc oversizedCubeY = validCube;
    oversizedCubeY.extent.y = 2049;
    EXPECT_FALSE(oversizedCubeY.isValid(limits));

    // Negative depth cases.
    TextureDesc non3DDepthZero = valid2D;
    non3DDepthZero.depth = 0u;
    EXPECT_FALSE(non3DDepthZero.isValid(limits));

    TextureDesc non3DDepthTwo = valid2D;
    non3DDepthTwo.depth = 2u;
    EXPECT_FALSE(non3DDepthTwo.isValid(limits));

    TextureDesc volumeDepthZero = valid3D;
    volumeDepthZero.depth = 0u;
    EXPECT_FALSE(volumeDepthZero.isValid(limits));

    TextureDesc volumeDepthOversized = valid3D;
    volumeDepthOversized.depth = 4097u;
    EXPECT_FALSE(volumeDepthOversized.isValid(limits));

    // Negative arraySize cases.
    TextureDesc nonArraySizeZero = valid2D;
    nonArraySizeZero.arraySize = 0u;
    EXPECT_FALSE(nonArraySizeZero.isValid(limits));

    TextureDesc nonArraySizeTwo = valid2D;
    nonArraySizeTwo.arraySize = 2u;
    EXPECT_FALSE(nonArraySizeTwo.isValid(limits));

    TextureDesc arraySizeZero = valid2DArray;
    arraySizeZero.arraySize = 0u;
    EXPECT_FALSE(arraySizeZero.isValid(limits));

    TextureDesc arraySizeOversized = valid2DArray;
    arraySizeOversized.arraySize = 65u;
    EXPECT_FALSE(arraySizeOversized.isValid(limits));

    // Negative MSAA and sample count cases.
    TextureDesc zeroSamples2D = valid2D;
    zeroSamples2D.numSamples = 0u;
    EXPECT_FALSE(zeroSamples2D.isValid(limits));

    TextureDesc msaa3D = valid3D;
    msaa3D.numMips = 1u;
    msaa3D.numSamples = 4u;
    EXPECT_FALSE(msaa3D.isValid(limits));

    TextureDesc msaaCube = validCube;
    msaaCube.numMips = 1u;
    msaaCube.numSamples = 4u;
    EXPECT_FALSE(msaaCube.isValid(limits));

    TextureDesc msaaCubeArray = validCubeArray;
    msaaCubeArray.numMips = 1u;
    msaaCubeArray.numSamples = 4u;
    EXPECT_FALSE(msaaCubeArray.isValid(limits));

    TextureDesc msaaWithMips = validMsaa;
    msaaWithMips.numMips = 2u;
    EXPECT_FALSE(msaaWithMips.isValid(limits));

    // Negative mip count cases.
    TextureDesc zeroMips = valid2D;
    zeroMips.numMips = 0u;
    EXPECT_FALSE(zeroMips.isValid(limits));

    TextureDesc oversizedMips = valid2D;
    oversizedMips.numMips = 13u;
    EXPECT_FALSE(oversizedMips.isValid(limits));

    // Reserved resource positive and negative cases.
    TextureDesc reserved2D = valid2D;
    reserved2D.flags = TextureCreateFlags::ReservedResource;
    EXPECT_TRUE(reserved2D.isValid(limits));

    TextureValidationLimits noReservedLimits = limits;
    noReservedLimits.supportsReservedResources = false;
    EXPECT_FALSE(reserved2D.isValid(noReservedLimits));

    TextureDesc reserved3D = valid3D;
    reserved3D.flags = TextureCreateFlags::ReservedResource;
    EXPECT_TRUE(reserved3D.isValid(limits));

    TextureValidationLimits noReservedVolumeLimits = limits;
    noReservedVolumeLimits.supportsReservedVolumeTextures = false;
    EXPECT_FALSE(reserved3D.isValid(noReservedVolumeLimits));

    TextureDesc reservedCube = validCube;
    reservedCube.flags = TextureCreateFlags::ReservedResource;
    EXPECT_FALSE(reservedCube.isValid(limits));

    TextureDesc reservedCubeArray = validCubeArray;
    reservedCubeArray.flags = TextureCreateFlags::ReservedResource;
    EXPECT_FALSE(reservedCubeArray.isValid(limits));

    // Reserved Texture2DArray requires its smallest mip in both X and Y to meet reservedTextureArrayMinMipDimension.
    TextureDesc reserved2DArrayValid = valid2DArray;
    reserved2DArrayValid.flags = TextureCreateFlags::ReservedResource;
    reserved2DArrayValid.extent = IntPoint(1024, 1024);
    reserved2DArrayValid.numMips = 3u;   // Smallest mip is 1024 >> 2 == 256.
    EXPECT_TRUE(reserved2DArrayValid.isValid(limits));

    TextureDesc reserved2DArrayTooSmallX = reserved2DArrayValid;
    reserved2DArrayTooSmallX.extent = IntPoint(512, 1024);   // Smallest mip X is 512 >> 2 == 128 < 256.
    EXPECT_FALSE(reserved2DArrayTooSmallX.isValid(limits));

    TextureDesc reserved2DArrayTooSmallY = reserved2DArrayValid;
    reserved2DArrayTooSmallY.extent = IntPoint(1024, 512);   // Smallest mip Y is 512 >> 2 == 128 < 256.
    EXPECT_FALSE(reserved2DArrayTooSmallY.isValid(limits));
}

TEST(TextureDescTest, CalcMemorySizeEstimateCoversFormatsDimensionsAndPartialRanges)
{
    // Uncompressed 2D mip chain: 8x4 R8G8B8A8_UNorm (4 Bpp), 4 mips -> 128 + 32 + 8 + 4 = 172 bytes.
    TextureDesc desc2D(TextureDimension::Texture2D);
    desc2D.format = PixelFormat::R8G8B8A8_UNorm;
    desc2D.extent = IntPoint(8, 4);
    desc2D.numMips = 4u;
    EXPECT_EQ(desc2D.calcMemorySizeEstimate(), 172u);
    EXPECT_EQ(desc2D.calcMemorySizeEstimate(0u, 3u), 172u);
    EXPECT_EQ(desc2D.calcMemorySizeEstimate(1u, 2u), 40u);
    EXPECT_EQ(desc2D.calcMemorySizeEstimate(1u), 44u);
    EXPECT_EQ(desc2D.calcMemorySizeEstimate(2u, 2u), 8u);

    // BC7 block-compressed (4x4 block, 16 bytes/block): 8x8, 4 mips -> 64 + 16 + 16 + 16 = 112 bytes.
    TextureDesc descBC7(TextureDimension::Texture2D);
    descBC7.format = PixelFormat::BC7_UNorm;
    descBC7.extent = IntPoint(8, 8);
    descBC7.numMips = 4u;
    EXPECT_EQ(descBC7.calcMemorySizeEstimate(), 112u);
    EXPECT_EQ(descBC7.calcMemorySizeEstimate(1u, 3u), 48u);
    EXPECT_EQ(descBC7.calcMemorySizeEstimate(0u, 0u), 64u);

    // 3D volume: 4x4x4 R16_Float (2 Bpp), 3 mips -> 128 + 16 + 2 = 146 bytes.
    TextureDesc desc3D(TextureDimension::Texture3D);
    desc3D.format = PixelFormat::R16_Float;
    desc3D.extent = IntPoint(4, 4);
    desc3D.depth = 4u;
    desc3D.numMips = 3u;
    EXPECT_EQ(desc3D.calcMemorySizeEstimate(), 146u);
    EXPECT_EQ(desc3D.calcMemorySizeEstimate(1u, 2u), 18u);

    // CubeArray: 4x4 R8G8B8A8_UNorm, 2 cubes (12 faces), 2 mips -> (64 + 16) * 2 * 6 = 960 bytes.
    TextureDesc descCubeArray(TextureDimension::TextureCubeArray);
    descCubeArray.format = PixelFormat::R8G8B8A8_UNorm;
    descCubeArray.extent = IntPoint(4, 4);
    descCubeArray.arraySize = 2u;
    descCubeArray.numMips = 2u;
    EXPECT_EQ(descCubeArray.calcMemorySizeEstimate(), 960u);
    EXPECT_EQ(descCubeArray.calcMemorySizeEstimate(1u, 1u), 192u);

    // MSAA 2D: 16x8 R8G8B8A8_UNorm, 4 samples, 1 mip -> 16 * 8 * 4 * 4 = 2048 bytes.
    TextureDesc descMsaa(TextureDimension::Texture2D);
    descMsaa.format = PixelFormat::R8G8B8A8_UNorm;
    descMsaa.extent = IntPoint(16, 8);
    descMsaa.numMips = 1u;
    descMsaa.numSamples = 4u;
    EXPECT_EQ(descMsaa.calcMemorySizeEstimate(), 2048u);

    // 2DArray: 4x4 R8_UNorm (1 Bpp), 3 slices, 2 mips -> (16 + 4) * 3 = 60 bytes.
    TextureDesc desc2DArray(TextureDimension::Texture2DArray);
    desc2DArray.format = PixelFormat::R8_UNorm;
    desc2DArray.extent = IntPoint(4, 4);
    desc2DArray.arraySize = 3u;
    desc2DArray.numMips = 2u;
    EXPECT_EQ(desc2DArray.calcMemorySizeEstimate(), 60u);
    EXPECT_EQ(desc2DArray.calcMemorySizeEstimate(1u, 1u), 12u);
}

TEST(TextureDescTest, GetDefaultResourceStateCoversAllPriorityBranches)
{
    // Any texture uploaded with initial data starts in SRVMask regardless of flags.
    EXPECT_EQ(getDefaultResourceState(TextureCreateFlags::None, true), sync::ResourceAccess::SRVMask);
    EXPECT_EQ(getDefaultResourceState(TextureCreateFlags::RenderTargetable, true), sync::ResourceAccess::SRVMask);
    EXPECT_EQ(getDefaultResourceState(TextureCreateFlags::DepthStencilTargetable, true), sync::ResourceAccess::SRVMask);
    EXPECT_EQ(getDefaultResourceState(TextureCreateFlags::UAV, true), sync::ResourceAccess::SRVMask);
    EXPECT_EQ(getDefaultResourceState(TextureCreateFlags::Presentable, true), sync::ResourceAccess::SRVMask);
    EXPECT_EQ(getDefaultResourceState(TextureCreateFlags::Foveation, true), sync::ResourceAccess::SRVMask);

    // Priority order when hasInitialData is false:
    // RenderTargetable > DepthStencilTargetable > UAV > Presentable > ShaderResource > Foveation > fallback SRVMask.
    EXPECT_EQ(
        getDefaultResourceState(
            TextureCreateFlags::RenderTargetable | TextureCreateFlags::DepthStencilTargetable |
                TextureCreateFlags::UAV | TextureCreateFlags::Presentable | TextureCreateFlags::ShaderResource |
                TextureCreateFlags::Foveation,
            false
        ),
        sync::ResourceAccess::RTV
    );

    const sync::ResourceAccess expectedDsv = sync::ResourceAccess::DSVWrite | sync::ResourceAccess::DSVRead;
    EXPECT_EQ(
        getDefaultResourceState(
            TextureCreateFlags::DepthStencilTargetable | TextureCreateFlags::UAV | TextureCreateFlags::Presentable |
                TextureCreateFlags::ShaderResource | TextureCreateFlags::Foveation,
            false
        ),
        expectedDsv
    );

    EXPECT_EQ(
        getDefaultResourceState(
            TextureCreateFlags::UAV | TextureCreateFlags::Presentable | TextureCreateFlags::ShaderResource |
                TextureCreateFlags::Foveation,
            false
        ),
        sync::ResourceAccess::UAVMask
    );

    EXPECT_EQ(
        getDefaultResourceState(
            TextureCreateFlags::Presentable | TextureCreateFlags::ShaderResource | TextureCreateFlags::Foveation, false
        ),
        sync::ResourceAccess::Present
    );

    EXPECT_EQ(
        getDefaultResourceState(TextureCreateFlags::ShaderResource | TextureCreateFlags::Foveation, false),
        sync::ResourceAccess::SRVMask
    );

    EXPECT_EQ(getDefaultResourceState(TextureCreateFlags::Foveation, false), sync::ResourceAccess::ShadingRateSource);

    EXPECT_EQ(getDefaultResourceState(TextureCreateFlags::None, false), sync::ResourceAccess::SRVMask);
}

TEST(TextureCreateDescTest, SubresourceIndexAndDataProvider)
{
    const TextureSubresourceIndex defaultSub{};
    EXPECT_EQ(defaultSub.faceIndex, 0);
    EXPECT_EQ(defaultSub.arrayIndex, 0);
    EXPECT_EQ(defaultSub.mipIndex, 0);

    const TextureSubresourceIndex subA{ 2, 3, 1 };
    const TextureSubresourceIndex subB{ 2, 3, 1 };
    const TextureSubresourceIndex subDiffFace{ 4, 3, 1 };
    const TextureSubresourceIndex subDiffArray{ 2, 5, 1 };
    const TextureSubresourceIndex subDiffMip{ 2, 3, 2 };
    EXPECT_EQ(subA, subB);
    EXPECT_NE(subA, subDiffFace);
    EXPECT_NE(subA, subDiffArray);
    EXPECT_NE(subA, subDiffMip);

    const TextureDataProvider emptyProvider{};
    EXPECT_FALSE(static_cast<bool>(emptyProvider.requester));
    EXPECT_FALSE(static_cast<bool>(emptyProvider.discarder));

    const gp::UInt32 pixelBlock[4]{ 0x11223344u, 0x55667788u, 0x99AABBCCu, 0xDDEEFF00u };
    TextureSubresourceIndex lastRequested{};
    TextureSubresourceIndex lastDiscarded{};
    gp::UInt32 discardCalls = 0u;

    const TextureDataProvider requestOnlyProvider(
        [&pixelBlock, &lastRequested](const TextureSubresourceIndex index) -> TextureDataProvider::SubresourceData
    {
        lastRequested = index;
        TextureDataProvider::SubresourceData result{};
        result.data = pixelBlock;
        result.size = sizeof(pixelBlock);
        result.rowPitch = 8u;
        return result;
    }
    );
    EXPECT_TRUE(static_cast<bool>(requestOnlyProvider.requester));
    EXPECT_FALSE(static_cast<bool>(requestOnlyProvider.discarder));

    const TextureDataProvider::SubresourceData fetched = requestOnlyProvider.requester(subA);
    EXPECT_EQ(lastRequested, subA);
    EXPECT_EQ(fetched.data, static_cast<const void*>(pixelBlock));
    EXPECT_EQ(fetched.size, sizeof(pixelBlock));
    EXPECT_EQ(fetched.rowPitch, 8u);

    const TextureDataProvider fullProvider(
        [&pixelBlock](const TextureSubresourceIndex index) -> TextureDataProvider::SubresourceData
    {
        TextureDataProvider::SubresourceData result{};
        result.data = pixelBlock;
        result.size = static_cast<gp::UInt64>(index.mipIndex + 1) * 16u;
        result.rowPitch = 16u;
        return result;
    },
        [&lastDiscarded, &discardCalls](const TextureSubresourceIndex index) -> void
    {
        lastDiscarded = index;
        ++discardCalls;
    }
    );
    EXPECT_TRUE(static_cast<bool>(fullProvider.requester));
    EXPECT_TRUE(static_cast<bool>(fullProvider.discarder));

    const TextureDataProvider::SubresourceData fetchedFull = fullProvider.requester(subDiffMip);
    EXPECT_EQ(fetchedFull.size, 48u);
    fullProvider.discarder(subDiffMip);
    EXPECT_EQ(lastDiscarded, subDiffMip);
    EXPECT_EQ(discardCalls, 1u);
}

TEST(TextureCreateDescTest, AllFourteenStaticFactoriesAndDirectConstructors)
{
    const TextureCreateDesc emptyDesc{};
    EXPECT_TRUE(emptyDesc.debugName.isEmpty());
    EXPECT_EQ(emptyDesc.initialState, sync::ResourceAccess::Unknown);
    EXPECT_EQ(emptyDesc.initAction, TextureInitAction::Default);
    EXPECT_EQ(emptyDesc.bulkData, nullptr);
    EXPECT_EQ(emptyDesc.dataProvider, nullptr);
    EXPECT_EQ(emptyDesc.firstVirtualMipLevel, -1);
    EXPECT_TRUE(emptyDesc.className.isNone());
    EXPECT_TRUE(emptyDesc.ownerName.isNone());

    // Direct TextureDesc + initialState + optional bulkData constructor.
    const gp::UInt8 rawBytes[16]{};
    MockResourceBulkData bulkData(rawBytes, 16u);

    TextureDesc baseDesc(TextureDimension::Texture2D);
    baseDesc.format = PixelFormat::R8G8B8A8_UNorm;
    baseDesc.extent = IntPoint(2, 2);

    const TextureCreateDesc fromDescNoBulk(baseDesc, sync::ResourceAccess::RTV, "FromDescNoBulk");
    EXPECT_EQ(fromDescNoBulk.debugName, gp::StringView("FromDescNoBulk"));
    EXPECT_EQ(fromDescNoBulk.initialState, sync::ResourceAccess::RTV);
    EXPECT_EQ(fromDescNoBulk.initAction, TextureInitAction::Default);
    EXPECT_EQ(fromDescNoBulk.bulkData, nullptr);

    const TextureCreateDesc fromDescWithBulk(baseDesc, sync::ResourceAccess::SRVMask, "FromDescWithBulk", &bulkData);
    EXPECT_EQ(fromDescWithBulk.initAction, TextureInitAction::BulkData);
    EXPECT_EQ(fromDescWithBulk.bulkData, &bulkData);
    EXPECT_TRUE(fromDescWithBulk.hasInitialData());

    // 1. create(name, dimension)
    const TextureCreateDesc f1 = TextureCreateDesc::create("CreateGeneric", TextureDimension::Texture3D);
    EXPECT_EQ(f1.debugName, gp::StringView("CreateGeneric"));
    EXPECT_EQ(f1.dimension, TextureDimension::Texture3D);

    // 2-6. Name-only dimension factories
    const TextureCreateDesc f2 = TextureCreateDesc::create2D("NameOnly2D");
    EXPECT_EQ(f2.dimension, TextureDimension::Texture2D);

    const TextureCreateDesc f3 = TextureCreateDesc::create2DArray("NameOnly2DArray");
    EXPECT_EQ(f3.dimension, TextureDimension::Texture2DArray);

    const TextureCreateDesc f4 = TextureCreateDesc::create3D("NameOnly3D");
    EXPECT_EQ(f4.dimension, TextureDimension::Texture3D);

    const TextureCreateDesc f5 = TextureCreateDesc::createCube("NameOnlyCube");
    EXPECT_EQ(f5.dimension, TextureDimension::TextureCube);

    const TextureCreateDesc f6 = TextureCreateDesc::createCubeArray("NameOnlyCubeArray");
    EXPECT_EQ(f6.dimension, TextureDimension::TextureCubeArray);

    // 7-8. create2D sized factories
    const IntPoint size2D(640, 480);
    const TextureCreateDesc f7 = TextureCreateDesc::create2D("Sized2DPoint", size2D, PixelFormat::R8G8B8A8_UNorm);
    EXPECT_EQ(f7.dimension, TextureDimension::Texture2D);
    EXPECT_EQ(f7.extent, size2D);
    EXPECT_EQ(f7.format, PixelFormat::R8G8B8A8_UNorm);

    const TextureCreateDesc f8 = TextureCreateDesc::create2D("Sized2DScalar", 320, 240, PixelFormat::B8G8R8A8_UNorm);
    const IntPoint expected320x240(320, 240);
    EXPECT_EQ(f8.extent, expected320x240);
    EXPECT_EQ(f8.format, PixelFormat::B8G8R8A8_UNorm);

    // 9-10. create2DArray sized factories
    const IntPoint sizeArray(128, 256);
    const TextureCreateDesc f9 =
        TextureCreateDesc::create2DArray("Sized2DArrayPoint", sizeArray, 6u, PixelFormat::R16_Float);
    EXPECT_EQ(f9.dimension, TextureDimension::Texture2DArray);
    EXPECT_EQ(f9.extent, sizeArray);
    EXPECT_EQ(f9.arraySize, 6u);
    EXPECT_EQ(f9.format, PixelFormat::R16_Float);

    const TextureCreateDesc f10 =
        TextureCreateDesc::create2DArray("Sized2DArrayScalar", 64, 32, 12u, PixelFormat::R8_UNorm);
    const IntPoint expected64x32(64, 32);
    EXPECT_EQ(f10.extent, expected64x32);
    EXPECT_EQ(f10.arraySize, 12u);
    EXPECT_EQ(f10.format, PixelFormat::R8_UNorm);

    // 11-12. create3D sized factories
    const IntVector size3D(32, 16, 8);
    const TextureCreateDesc f11 = TextureCreateDesc::create3D("Sized3DVec", size3D, PixelFormat::R16G16_Float);
    const IntPoint expected32x16(32, 16);
    EXPECT_EQ(f11.dimension, TextureDimension::Texture3D);
    EXPECT_EQ(f11.extent, expected32x16);
    EXPECT_EQ(f11.depth, 8u);
    EXPECT_EQ(f11.format, PixelFormat::R16G16_Float);

    const TextureCreateDesc f12 = TextureCreateDesc::create3D("Sized3DScalar", 16, 24, 32, PixelFormat::R32_Float);
    const IntPoint expected16x24(16, 24);
    EXPECT_EQ(f12.extent, expected16x24);
    EXPECT_EQ(f12.depth, 32u);
    EXPECT_EQ(f12.format, PixelFormat::R32_Float);

    // 13. createCube sized factory
    const TextureCreateDesc f13 = TextureCreateDesc::createCube("SizedCube", 256u, PixelFormat::R16G16B16A16_Float);
    const IntPoint expected256Square(256, 256);
    EXPECT_EQ(f13.dimension, TextureDimension::TextureCube);
    EXPECT_EQ(f13.extent, expected256Square);
    EXPECT_EQ(f13.format, PixelFormat::R16G16B16A16_Float);

    // 14. createCubeArray sized factory
    const TextureCreateDesc f14 =
        TextureCreateDesc::createCubeArray("SizedCubeArray", 128u, 5u, PixelFormat::BC7_UNorm);
    const IntPoint expected128Square(128, 128);
    EXPECT_EQ(f14.dimension, TextureDimension::TextureCubeArray);
    EXPECT_EQ(f14.extent, expected128Square);
    EXPECT_EQ(f14.arraySize, 5u);
    EXPECT_EQ(f14.format, PixelFormat::BC7_UNorm);
}

TEST(TextureCreateDescTest, FluentBuildersClampingInitActionsValidityAndTraceClassName)
{
    const GPUMask gpuMask = GPUMask::fromIndex(1u);
    TextureCreateDesc desc = TextureCreateDesc::create2D("FluentTex");
    desc.setDimension(TextureDimension::Texture2DArray)
        .setFlags(TextureCreateFlags::ShaderResource)
        .addFlags(TextureCreateFlags::UAV | TextureCreateFlags::SRGB)
        .setClearValue(ClearValueBinding::kGreen)
        .setExtData(0xBEEFu)
        .setExtent(IntPoint(128, 64))
        .setExtent(256, 128)
        .setExtent(512u)
        .setDepth(1u)
        .setArraySize(4u)
        .setNumMips(3u)
        .setNumSamples(1u)
        .setFormat(PixelFormat::R8G8B8A8_UNorm)
        .setUAVFormat(PixelFormat::R32_UInt)
        .setDebugName("FinalFluentTex")
        .setGPUMask(gpuMask)
        .setFirstVirtualMipLevel(1)
        .setClassName(gp::Name("ShadowAtlas"))
        .setOwnerName(gp::Name("ShadowRenderer"))
        .addAliasableFormat(PixelFormat::B8G8R8A8_UNorm)
        .addAliasableFormat(PixelFormat::R32_UInt);

    const IntPoint expected512Square(512, 512);
    EXPECT_EQ(desc.dimension, TextureDimension::Texture2DArray);
    EXPECT_EQ(desc.flags, TextureCreateFlags::ShaderResource | TextureCreateFlags::UAV | TextureCreateFlags::SRGB);
    EXPECT_EQ(desc.clearValue, ClearValueBinding::kGreen);
    EXPECT_EQ(desc.extData, 0xBEEFu);
    EXPECT_EQ(desc.extent, expected512Square);
    EXPECT_EQ(desc.depth, 1u);
    EXPECT_EQ(desc.arraySize, 4u);
    EXPECT_EQ(desc.numMips, 3u);
    EXPECT_EQ(desc.numSamples, 1u);
    EXPECT_EQ(desc.format, PixelFormat::R8G8B8A8_UNorm);
    EXPECT_EQ(desc.uavFormat, PixelFormat::R32_UInt);
    EXPECT_EQ(desc.debugName, gp::StringView("FinalFluentTex"));
    EXPECT_EQ(desc.gpuMask, gpuMask);
    EXPECT_EQ(desc.firstVirtualMipLevel, 1);
    EXPECT_EQ(desc.className, gp::Name("ShadowAtlas"));
    EXPECT_EQ(desc.ownerName, gp::Name("ShadowRenderer"));
    ASSERT_EQ(desc.aliasableFormats.size(), 2u);
    EXPECT_EQ(desc.aliasableFormats[0], PixelFormat::B8G8R8A8_UNorm);
    EXPECT_EQ(desc.aliasableFormats[1], PixelFormat::R32_UInt);
    EXPECT_EQ(desc.getTraceClassName(), gp::Name("ShadowAtlas"));

    const TextureCreateDesc defaultClassDesc = TextureCreateDesc::create2D("DefaultClass");
    EXPECT_EQ(defaultClassDesc.getTraceClassName(), gp::Name("Texture"));

    // Fast VRAM percentage clamping across out-of-range and mid-range values.
    desc.setFastVRAMPercentage(-0.5f);
    EXPECT_EQ(desc.fastVRAMPercentage, 0u);
    desc.setFastVRAMPercentage(0.0f);
    EXPECT_EQ(desc.fastVRAMPercentage, 0u);
    desc.setFastVRAMPercentage(0.5f);
    EXPECT_EQ(desc.fastVRAMPercentage, 127u);
    desc.setFastVRAMPercentage(1.0f);
    EXPECT_EQ(desc.fastVRAMPercentage, 255u);
    desc.setFastVRAMPercentage(1.5f);
    EXPECT_EQ(desc.fastVRAMPercentage, 255u);

    // Init actions, pointer clearing, and hasInitialData().
    const gp::UInt8 uploadBytes[16]{};
    MockResourceBulkData bulkData(uploadBytes, 16u);
    const TextureDataProvider dataProvider(
        [&uploadBytes](const TextureSubresourceIndex) -> TextureDataProvider::SubresourceData
    {
        TextureDataProvider::SubresourceData result{};
        result.data = uploadBytes;
        result.size = sizeof(uploadBytes);
        result.rowPitch = 16u;
        return result;
    }
    );

    EXPECT_FALSE(desc.hasInitialData());

    desc.setInitActionBulkData(&bulkData);
    EXPECT_EQ(desc.initAction, TextureInitAction::BulkData);
    EXPECT_EQ(desc.bulkData, &bulkData);
    EXPECT_EQ(desc.dataProvider, nullptr);
    EXPECT_TRUE(desc.hasInitialData());

    desc.setInitActionDataProvider(&dataProvider);
    EXPECT_EQ(desc.initAction, TextureInitAction::DataProvider);
    EXPECT_EQ(desc.bulkData, nullptr);
    EXPECT_EQ(desc.dataProvider, &dataProvider);
    EXPECT_TRUE(desc.hasInitialData());

    desc.setInitActionInitializer();
    EXPECT_EQ(desc.initAction, TextureInitAction::Initializer);
    EXPECT_EQ(desc.bulkData, nullptr);
    EXPECT_EQ(desc.dataProvider, nullptr);
    EXPECT_FALSE(desc.hasInitialData());

    desc.setInitActionBulkData(nullptr);
    EXPECT_EQ(desc.initAction, TextureInitAction::Default);
    EXPECT_EQ(desc.bulkData, nullptr);
    EXPECT_FALSE(desc.hasInitialData());

    desc.setInitAction(TextureInitAction::BulkData);
    EXPECT_EQ(desc.initAction, TextureInitAction::BulkData);

    desc.setInitActionNone();
    EXPECT_EQ(desc.initAction, TextureInitAction::Default);
    EXPECT_EQ(desc.bulkData, nullptr);
    EXPECT_EQ(desc.dataProvider, nullptr);

    // determineInitialState uses hasInitialData() when Unknown and preserves explicit states.
    TextureCreateDesc rtvNoData = TextureCreateDesc::create2D("RtvNoData", 64, 64, PixelFormat::R8G8B8A8_UNorm);
    rtvNoData.setFlags(TextureCreateFlags::RenderTargetable).determineInitialState();
    EXPECT_EQ(rtvNoData.initialState, sync::ResourceAccess::RTV);
    rtvNoData.checkValidity();

    TextureCreateDesc rtvWithData = TextureCreateDesc::create2D("RtvWithData", 64, 64, PixelFormat::R8G8B8A8_UNorm);
    rtvWithData.setFlags(TextureCreateFlags::RenderTargetable).setInitActionBulkData(&bulkData).determineInitialState();
    EXPECT_EQ(rtvWithData.initialState, sync::ResourceAccess::SRVMask);
    rtvWithData.checkValidity();

    TextureCreateDesc explicitState = TextureCreateDesc::create2D("ExplicitState", 64, 64, PixelFormat::R8G8B8A8_UNorm);
    explicitState.setFlags(TextureCreateFlags::RenderTargetable)
        .setInitialState(sync::ResourceAccess::CopyDest)
        .determineInitialState();
    EXPECT_EQ(explicitState.initialState, sync::ResourceAccess::CopyDest);
    explicitState.checkValidity();
}

TEST_F(TextureTestFixture, TextureVirtualDefaultsDimensionsClearValuesAndDeferredDeletion)
{
    // Concrete 2D texture with color clear binding and MSAA.
    TextureCreateDesc desc2D = TextureCreateDesc::create2D("ColorTarget", 256, 128, PixelFormat::R8G8B8A8_UNorm);
    desc2D.setFlags(TextureCreateFlags::RenderTargetable | TextureCreateFlags::ShaderResource)
        .setNumMips(1u)
        .setNumSamples(4u)
        .setClearValue(ClearValueBinding(gp::math::LinearColor(0.25f, 0.5f, 0.75f, 1.0f)))
        .setOwnerName(gp::Name("DeferredRenderer"))
        .determineInitialState();

    TextureRef tex2D(new MockDefaultTexture(desc2D));
    EXPECT_EQ(tex2D->getType(), ResourceType::Texture);
    EXPECT_EQ(tex2D->getTextureReference(), nullptr);
    EXPECT_FALSE(tex2D->getDefaultBindlessHandle().isValid());
    EXPECT_EQ(tex2D->getNativeResource(), nullptr);
    EXPECT_EQ(tex2D->getNativeShaderResourceView(), nullptr);
    EXPECT_EQ(tex2D->getTextureBaseRHI(), nullptr);

    gp::UInt8 writeMaskSentinel = 0u;
    void* writeMaskData = &writeMaskSentinel;
    gp::UInt32 writeMaskSize = 99u;
    tex2D->getWriteMaskProperties(writeMaskData, writeMaskSize);
    EXPECT_EQ(writeMaskData, nullptr);
    EXPECT_EQ(writeMaskSize, 0u);

    const IntPoint expectedXY2D(256, 128);
    const IntVector expectedXYZ2D(256, 128, 1);
    EXPECT_EQ(tex2D->getSizeX(), 256u);
    EXPECT_EQ(tex2D->getSizeY(), 128u);
    EXPECT_EQ(tex2D->getSizeXY(), expectedXY2D);
    EXPECT_EQ(tex2D->getSizeZ(), 1u);
    EXPECT_EQ(tex2D->getSizeXYZ(), expectedXYZ2D);
    EXPECT_EQ(tex2D->getNumMips(), 1u);
    EXPECT_EQ(tex2D->getFormat(), PixelFormat::R8G8B8A8_UNorm);
    EXPECT_EQ(tex2D->getFlags(), TextureCreateFlags::RenderTargetable | TextureCreateFlags::ShaderResource);
    EXPECT_EQ(tex2D->getNumSamples(), 4u);
    EXPECT_TRUE(tex2D->isMultisampled());

    EXPECT_TRUE(tex2D->hasClearValue());
    const gp::math::LinearColor clearColor = tex2D->getClearColor();
    EXPECT_FLOAT_EQ(clearColor.r, 0.25f);
    EXPECT_FLOAT_EQ(clearColor.g, 0.5f);
    EXPECT_FLOAT_EQ(clearColor.b, 0.75f);
    EXPECT_FLOAT_EQ(clearColor.a, 1.0f);
    EXPECT_EQ(tex2D->getClearBinding(), desc2D.clearValue);

    EXPECT_DOUBLE_EQ(tex2D->getLastRenderTime(), Texture::kEmptyLastRenderTime);
    tex2D->setLastRenderTime(42.5);
    EXPECT_DOUBLE_EQ(tex2D->getLastRenderTime(), 42.5);

#if GP_RHI_ENABLE_RESOURCE_INFO
    EXPECT_EQ(tex2D->getName(), gp::Name("ColorTarget"));
    EXPECT_EQ(tex2D->getOwnerName(), gp::Name("DeferredRenderer"));
#else
    EXPECT_TRUE(tex2D->getName().isNone());
    EXPECT_TRUE(tex2D->getOwnerName().isNone());
#endif
    tex2D->setName(gp::Name("RenamedColorTarget"));
    EXPECT_EQ(tex2D->getName(), gp::Name("RenamedColorTarget"));

    EXPECT_EQ(tex2D->getTexture2D(), tex2D.get());
    EXPECT_EQ(tex2D->getTexture2DArray(), nullptr);
    EXPECT_EQ(tex2D->getTexture3D(), nullptr);
    EXPECT_EQ(tex2D->getTextureCube(), nullptr);

    // Depth-stencil texture clear value queries and no-clear-value predicate.
    TextureCreateDesc descDepth = TextureCreateDesc::create2D("SceneDepth", 512, 512, PixelFormat::D32_Float_S8_UInt);
    descDepth.setFlags(TextureCreateFlags::DepthStencilTargetable)
        .setClearValue(ClearValueBinding(0.75f, 9u))
        .determineInitialState();
    TextureRef texDepth(new MockDefaultTexture(descDepth));
    EXPECT_FALSE(texDepth->isMultisampled());
    EXPECT_TRUE(texDepth->hasClearValue());

    gp::Float32 outDepth = 0.0f;
    gp::UInt32 outStencil = 0u;
    texDepth->getDepthStencilClearValue(outDepth, outStencil);
    EXPECT_FLOAT_EQ(outDepth, 0.75f);
    EXPECT_EQ(outStencil, 9u);
    EXPECT_FLOAT_EQ(texDepth->getDepthClearValue(), 0.75f);
    EXPECT_EQ(texDepth->getStencilClearValue(), 9u);

    TextureCreateDesc descNoClear = makeValid2DCreateDesc("NoClear", 16, 16);
    descNoClear.setClearValue(ClearValueBinding::kNone);
    TextureRef texNoClear(new MockDefaultTexture(descNoClear));
    EXPECT_FALSE(texNoClear->hasClearValue());

    // Remaining four dimensions: 2DArray, 3D, Cube, CubeArray plus getMipDimensions clamping.
    TextureCreateDesc desc2DArray =
        TextureCreateDesc::create2DArray("ArrayTex", 64, 32, 6u, PixelFormat::R8G8B8A8_UNorm);
    desc2DArray.setFlags(TextureCreateFlags::ShaderResource).determineInitialState();
    TextureRef tex2DArray(new MockDefaultTexture(desc2DArray));
    const IntVector expectedXYZ2DArray(64, 32, 6);
    EXPECT_EQ(tex2DArray->getSizeXYZ(), expectedXYZ2DArray);
    EXPECT_EQ(tex2DArray->getSizeZ(), 6u);
    EXPECT_EQ(tex2DArray->getTexture2D(), nullptr);
    EXPECT_EQ(tex2DArray->getTexture2DArray(), tex2DArray.get());
    EXPECT_EQ(tex2DArray->getTexture3D(), nullptr);
    EXPECT_EQ(tex2DArray->getTextureCube(), nullptr);

    TextureCreateDesc desc3D = TextureCreateDesc::create3D("VolumeTex", 16, 8, 4, PixelFormat::R16_Float);
    desc3D.setNumMips(5u).setFlags(TextureCreateFlags::ShaderResource).determineInitialState();
    TextureRef tex3D(new MockDefaultTexture(desc3D));
    const IntVector expectedXYZ3D(16, 8, 4);
    const IntVector expectedMip0(16, 8, 4);
    const IntVector expectedMip1(8, 4, 2);
    const IntVector expectedMip2(4, 2, 1);
    const IntVector expectedMip4(1, 1, 1);
    const IntVector expectedMip10(1, 1, 1);
    EXPECT_EQ(tex3D->getSizeXYZ(), expectedXYZ3D);
    EXPECT_EQ(tex3D->getSizeZ(), 4u);
    EXPECT_EQ(tex3D->getMipDimensions(0u), expectedMip0);
    EXPECT_EQ(tex3D->getMipDimensions(1u), expectedMip1);
    EXPECT_EQ(tex3D->getMipDimensions(2u), expectedMip2);
    EXPECT_EQ(tex3D->getMipDimensions(4u), expectedMip4);
    EXPECT_EQ(tex3D->getMipDimensions(10u), expectedMip10);
    EXPECT_EQ(tex3D->getTexture2D(), nullptr);
    EXPECT_EQ(tex3D->getTexture2DArray(), nullptr);
    EXPECT_EQ(tex3D->getTexture3D(), tex3D.get());
    EXPECT_EQ(tex3D->getTextureCube(), nullptr);

    TextureCreateDesc descCube = TextureCreateDesc::createCube("SkyCube", 128u, PixelFormat::R16G16B16A16_Float);
    descCube.setFlags(TextureCreateFlags::ShaderResource).determineInitialState();
    TextureRef texCube(new MockDefaultTexture(descCube));
    const IntVector expectedXYZCube(128, 128, 1);
    EXPECT_EQ(texCube->getSizeXYZ(), expectedXYZCube);
    EXPECT_EQ(texCube->getSizeZ(), 1u);
    EXPECT_EQ(texCube->getSize(), 128u);
    EXPECT_EQ(texCube->getTexture2D(), nullptr);
    EXPECT_EQ(texCube->getTexture2DArray(), nullptr);
    EXPECT_EQ(texCube->getTexture3D(), nullptr);
    EXPECT_EQ(texCube->getTextureCube(), texCube.get());

    TextureCreateDesc descCubeArray =
        TextureCreateDesc::createCubeArray("ProbeArray", 64u, 3u, PixelFormat::R16G16B16A16_Float);
    descCubeArray.setFlags(TextureCreateFlags::ShaderResource).determineInitialState();
    TextureRef texCubeArray(new MockDefaultTexture(descCubeArray));
    const IntVector expectedXYZCubeArray(64, 64, 3);
    EXPECT_EQ(texCubeArray->getSizeXYZ(), expectedXYZCubeArray);
    EXPECT_EQ(texCubeArray->getSizeZ(), 3u);
    EXPECT_EQ(texCubeArray->getSize(), 64u);
    EXPECT_EQ(texCubeArray->getTextureCube(), texCubeArray.get());

    // Deferred deletion destroys all textures only when flushed.
    tex2D.reset();
    texDepth.reset();
    texNoClear.reset();
    tex2DArray.reset();
    tex3D.reset();
    texCube.reset();
    texCubeArray.reset();
    EXPECT_EQ(MockDefaultTexture::s_destructCount.load(), 0u);

    ResourceDeletionQueue::flushPendingDeletes(true);
    EXPECT_EQ(MockDefaultTexture::s_constructCount.load(), 7u);
    EXPECT_EQ(MockDefaultTexture::s_destructCount.load(), 7u);
}

TEST_F(TextureTestFixture, TextureReferenceDefaultFallbackAndNonNullForwarding)
{
    // Register a global fallback texture and verify null-target fallback.
    gp::UInt8 defaultWriteMask[4]{};
    gp::UInt8 targetWriteMask[8]{};
    gp::UInt32 nativeResTokenA = 1u;
    gp::UInt32 nativeSrvTokenA = 2u;
    gp::UInt32 baseTokenA = 3u;
    gp::UInt32 nativeResTokenB = 4u;
    gp::UInt32 nativeSrvTokenB = 5u;
    gp::UInt32 baseTokenB = 6u;

    const view::DescriptorHandle defaultBindless(view::DescriptorType::TextureSRV, 10u);
    const view::DescriptorHandle targetBindless(view::DescriptorType::TextureSRV, 20u);
    const view::DescriptorHandle dedicatedBindless(view::DescriptorType::TextureSRV, 77u);

    TextureRef fallbackTex(new MockBackendTexture(
        makeValid2DCreateDesc("FallbackTex", 16, 16, PixelFormat::R8G8B8A8_UNorm),
        defaultBindless,
        &nativeResTokenA,
        &nativeSrvTokenA,
        &baseTokenA,
        defaultWriteMask,
        sizeof(defaultWriteMask)
    ));
    EXPECT_EQ(TextureReference::getDefaultTexture(), nullptr);
    TextureReference::setDefaultTexture(fallbackTex.get());
    EXPECT_EQ(TextureReference::getDefaultTexture(), fallbackTex.get());

    // Null target falls back to the registered default texture.
    TextureReferenceRef fallbackRef(new TextureReference(nullptr));
    EXPECT_EQ(fallbackRef->getType(), ResourceType::TextureReference);
    EXPECT_EQ(fallbackRef->getTextureReference(), fallbackRef.get());
    EXPECT_EQ(fallbackRef->getReferencedTexture(), fallbackTex.get());
    EXPECT_FALSE(fallbackRef->isBindless());
    EXPECT_FALSE(fallbackRef->getBindlessHandle().isValid());
    EXPECT_EQ(fallbackRef->getSizeX(), 16u);
    EXPECT_EQ(fallbackRef->getSizeY(), 16u);
    EXPECT_EQ(fallbackRef->getDefaultBindlessHandle(), defaultBindless);
    EXPECT_EQ(fallbackRef->getNativeResource(), &nativeResTokenA);
    EXPECT_EQ(fallbackRef->getNativeShaderResourceView(), &nativeSrvTokenA);
    EXPECT_EQ(fallbackRef->getTextureBaseRHI(), &baseTokenA);

    void* outMaskData = nullptr;
    gp::UInt32 outMaskSize = 0u;
    fallbackRef->getWriteMaskProperties(outMaskData, outMaskSize);
    EXPECT_EQ(outMaskData, static_cast<void*>(defaultWriteMask));
    EXPECT_EQ(outMaskSize, static_cast<gp::UInt32>(sizeof(defaultWriteMask)));

    // Non-null target without dedicated bindless handle forwards every query to target.
    TextureRef targetTex(new MockBackendTexture(
        makeValid2DCreateDesc("StreamedTex", 512, 256, PixelFormat::R16G16B16A16_Float),
        targetBindless,
        &nativeResTokenB,
        &nativeSrvTokenB,
        &baseTokenB,
        targetWriteMask,
        sizeof(targetWriteMask)
    ));

    MockTextureReference* const rawRefNoBindless = new MockTextureReference(targetTex.get());
    TextureReferenceRef refNoBindless(rawRefNoBindless);
    EXPECT_EQ(refNoBindless->getReferencedTexture(), targetTex.get());
    EXPECT_EQ(refNoBindless->getTextureReference(), refNoBindless.get());
    EXPECT_EQ(refNoBindless->getDesc(), targetTex->getDesc());
    EXPECT_EQ(refNoBindless->getSizeX(), 512u);
    EXPECT_EQ(refNoBindless->getSizeY(), 256u);
    EXPECT_EQ(refNoBindless->getFormat(), PixelFormat::R16G16B16A16_Float);
    EXPECT_FALSE(refNoBindless->isBindless());
    EXPECT_EQ(refNoBindless->getDefaultBindlessHandle(), targetBindless);
    EXPECT_EQ(refNoBindless->getNativeResource(), &nativeResTokenB);
    EXPECT_EQ(refNoBindless->getNativeShaderResourceView(), &nativeSrvTokenB);
    EXPECT_EQ(refNoBindless->getTextureBaseRHI(), &baseTokenB);

    outMaskData = nullptr;
    outMaskSize = 0u;
    refNoBindless->getWriteMaskProperties(outMaskData, outMaskSize);
    EXPECT_EQ(outMaskData, static_cast<void*>(targetWriteMask));
    EXPECT_EQ(outMaskSize, static_cast<gp::UInt32>(sizeof(targetWriteMask)));

    // setTrackedAccessFromContext forwards barrier state updates directly to the referenced texture.
    const sync::TrackedAccess computeUav(sync::ResourceAccess::UAVCompute, sync::PipelineStage::AsyncCompute);
    rawRefNoBindless->updateTrackedAccessForTest(computeUav);
    EXPECT_EQ(targetTex->getTrackedAccessUnsafe(), sync::ResourceAccess::UAVCompute);
    EXPECT_EQ(targetTex->getTrackedPipelinesUnsafe(), sync::PipelineStage::AsyncCompute);

    // Bindless TextureReference returns its dedicated handle instead of the target's handle.
    TextureReferenceRef refBindless(new TextureReference(targetTex.get(), dedicatedBindless));
    EXPECT_TRUE(refBindless->isBindless());
    EXPECT_EQ(refBindless->getBindlessHandle(), dedicatedBindless);
    EXPECT_EQ(refBindless->getDefaultBindlessHandle(), dedicatedBindless);
    EXPECT_EQ(refBindless->getNativeResource(), &nativeResTokenB);
}

TEST_F(TextureTestFixture, TextureReferenceSwapAndCascadingDeferredDeletion)
{
    TextureRef fallbackTex(new MockDefaultTexture(makeValid2DCreateDesc("Fallback", 8, 8)));
    TextureReference::setDefaultTexture(fallbackTex.get());

    MockTextureReference* rawRef = nullptr;
    TextureReferenceRef texRef{};
    {
        TextureRef targetA(new MockDefaultTexture(makeValid2DCreateDesc("MipLow", 64, 64)));
        rawRef = new MockTextureReference(targetA.get());
        texRef = TextureReferenceRef(rawRef);
        EXPECT_EQ(targetA->getRefCount(), 2u);
    }

    // TextureReference retains targetA even after the caller drops its handle.
    ResourceDeletionQueue::flushPendingDeletes(true);
    EXPECT_EQ(MockDefaultTexture::s_destructCount.load(), 0u);
    EXPECT_EQ(texRef->getSizeX(), 64u);

    // Swapping to targetB queues targetA for deferred deletion and keeps targetB alive.
    {
        TextureRef targetB(new MockDefaultTexture(makeValid2DCreateDesc("MipHigh", 1024, 1024)));
        rawRef->swapTargetForTest(targetB.get());
        EXPECT_EQ(targetB->getRefCount(), 2u);
    }
    EXPECT_EQ(texRef->getSizeX(), 1024u);
    EXPECT_EQ(MockDefaultTexture::s_destructCount.load(), 0u);

    ResourceDeletionQueue::flushPendingDeletes(true);
    EXPECT_EQ(MockDefaultTexture::s_destructCount.load(), 1u);

    // Swapping to nullptr falls back to the registered default texture and queues targetB for deletion.
    rawRef->swapTargetForTest(nullptr);
    EXPECT_EQ(texRef->getReferencedTexture(), fallbackTex.get());
    EXPECT_EQ(texRef->getSizeX(), 8u);

    ResourceDeletionQueue::flushPendingDeletes(true);
    EXPECT_EQ(MockDefaultTexture::s_destructCount.load(), 2u);

    // Swapping to a dedicated targetC and releasing texRef cascades deletion to both texRef and targetC.
    {
        TextureRef targetC(new MockDefaultTexture(makeValid2DCreateDesc("MipFinal", 2048, 2048)));
        rawRef->swapTargetForTest(targetC.get());
        EXPECT_EQ(targetC->getRefCount(), 2u);
    }

    texRef.reset();
    EXPECT_EQ(MockTextureReference::s_destructCount.load(), 0u);
    EXPECT_EQ(MockDefaultTexture::s_destructCount.load(), 2u);

    ResourceDeletionQueue::flushPendingDeletes(true);
    EXPECT_EQ(MockTextureReference::s_constructCount.load(), 1u);
    EXPECT_EQ(MockTextureReference::s_destructCount.load(), 1u);
    EXPECT_EQ(MockDefaultTexture::s_destructCount.load(), 3u);

    // Null target with dedicated bindless handle falls back to the registered default texture.
    {
        const view::DescriptorHandle nullFallbackBindless(view::DescriptorType::TextureSRV, 902u);
        TextureReferenceRef bindlessNullRef(new MockTextureReference(nullptr, nullFallbackBindless));
        EXPECT_EQ(bindlessNullRef->getReferencedTexture(), fallbackTex.get());
        EXPECT_TRUE(bindlessNullRef->isBindless());
        EXPECT_EQ(bindlessNullRef->getDefaultBindlessHandle(), nullFallbackBindless);
    }

    // Clearing the default texture destroys the remaining fallback texture on flush.
    fallbackTex.reset();
    TextureReference::setDefaultTexture(nullptr);
    ResourceDeletionQueue::flushPendingDeletes(true);
    EXPECT_EQ(MockTextureReference::s_constructCount.load(), 2u);
    EXPECT_EQ(MockTextureReference::s_destructCount.load(), 2u);
    EXPECT_EQ(MockDefaultTexture::s_constructCount.load(), 4u);
    EXPECT_EQ(MockDefaultTexture::s_destructCount.load(), 4u);
}

TEST_F(TextureTestFixture, AdditionalFlagsAndTexture2DArrayMipDimensions)
{
    EXPECT_NE(TextureCreateFlags::OfflineProcessed, TextureCreateFlags::None);
    EXPECT_NE(TextureCreateFlags::HideInVisualizeTexture, TextureCreateFlags::None);
    EXPECT_EQ(
        (TextureCreateFlags::OfflineProcessed | TextureCreateFlags::HideInVisualizeTexture) &
            TextureCreateFlags::OfflineProcessed,
        TextureCreateFlags::OfflineProcessed
    );

    TextureCreateDesc arrDesc = TextureCreateDesc::create2DArray("MipArray", 64, 32, 6u, PixelFormat::R8G8B8A8_UNorm)
                                    .setNumMips(4u)
                                    .addFlags(TextureCreateFlags::ShaderResource)
                                    .determineInitialState();
    TextureRef arrTex(new MockDefaultTexture(arrDesc));

    EXPECT_EQ(arrTex->getMipDimensions(0u), IntVector(64, 32, 1));
    EXPECT_EQ(arrTex->getMipDimensions(1u), IntVector(32, 16, 1));
    EXPECT_EQ(arrTex->getMipDimensions(3u), IntVector(8, 4, 1));
}

}   // namespace gp::rhi::resource::tests
