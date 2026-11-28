// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "resources/BufferInitializerHelpers.hpp"
#include "commands/CommandListExecutor.hpp"
#include "commands/CommandListImmediate.hpp"
#include "containers/arrays/Array.hpp"
#include "containers/arrays/Vector.hpp"   // IWYU pragma: keep
#include "core/ResourceDeletionQueue.hpp"
#include "core/ResourceTypes.hpp"
#include "device/DynamicRHI.hpp"
#include "memory/GlobalMemory.hpp"
#include "memory/Memory.hpp"
#include "resources/InitializerScopedMemory.hpp"
#include <gtest/gtest.h>
#include <utility>

namespace gp::rhi::resource::tests
{

namespace
{

struct MockBuffer final : public Buffer
{
    explicit MockBuffer(const BufferCreateDesc& createDesc) noexcept
        : Buffer(createDesc)
    {
        storage.resize(static_cast<gp::USize>(createDesc.size), 0u);
    }

    gp::Vector<gp::UInt8> storage{};
};

struct MockUploadArray final : public IResourceArrayUpload
{
    MockUploadArray(const void* const inData, const gp::UInt32 inSize) noexcept
        : data(inData)
        , size(inSize)
    {}

    [[nodiscard]] virtual const void* getResourceData() const noexcept override
    {
        return data;
    }

    [[nodiscard]] virtual gp::UInt32 getResourceDataSize() const noexcept override
    {
        return size;
    }

    virtual void discard() noexcept override
    {
        ++discardCount;
    }

    const void* data{ nullptr };
    gp::UInt32 size{ 0u };
    gp::UInt32 discardCount{ 0u };
};

struct MockLockBufferDynamicRHI final : public DynamicRHI
{
    gp::UInt32 lockCalls{ 0u };
    gp::UInt32 unlockCalls{ 0u };
    gp::UInt32 lastLockOffset{ 0u };
    gp::UInt32 lastLockSize{ 0u };
    ResourceLockMode lastLockMode{ ResourceLockMode::ReadOnly };

    virtual void init() noexcept override
    {}

    virtual void shutdown() noexcept override
    {}

    [[nodiscard]] virtual const char* getName() const noexcept override
    {
        return "MockLockBuffer";
    }

    [[nodiscard]] virtual InterfaceType getInterfaceType() const noexcept override
    {
        return InterfaceType::Null;
    }

    [[nodiscard]] virtual pipeline::SamplerStateRef
        createSamplerState(const pipeline::SamplerStateDesc&) noexcept override
    {
        return {};
    }

    [[nodiscard]] virtual pipeline::RasterizerStateRef
        createRasterizerState(const pipeline::RasterizerStateDesc&) noexcept override
    {
        return {};
    }

    [[nodiscard]] virtual pipeline::DepthStencilStateRef
        createDepthStencilState(const pipeline::DepthStencilStateDesc&) noexcept override
    {
        return {};
    }

    [[nodiscard]] virtual pipeline::BlendStateRef createBlendState(const pipeline::BlendStateDesc&) noexcept override
    {
        return {};
    }

    [[nodiscard]] virtual pipeline::VertexDeclarationRef
        createVertexDeclaration(const pipeline::VertexDeclarationElementList&) noexcept override
    {
        return {};
    }

    [[nodiscard]] virtual pipeline::GraphicsPipelineStateRef
        createGraphicsPipelineState(const pipeline::GraphicsPipelineStateInitializer&) noexcept override
    {
        return {};
    }

    [[nodiscard]] virtual pipeline::ComputePipelineStateRef
        createComputePipelineState(shader::ComputeShader*) noexcept override
    {
        return {};
    }

    [[nodiscard]] virtual sync::RenderQueryRef createRenderQuery(sync::RenderQueryType) noexcept override
    {
        return {};
    }

    [[nodiscard]] virtual bool getRenderQueryResult(sync::RenderQuery*, gp::UInt64&, bool, gp::UInt32) noexcept override
    {
        return false;
    }

    [[nodiscard]] virtual shader::VertexShaderRef
        createVertexShader(gp::ConstVectorView<gp::UInt8>, const shader::ShaderHash&) noexcept override
    {
        return {};
    }

    [[nodiscard]] virtual shader::PixelShaderRef
        createPixelShader(gp::ConstVectorView<gp::UInt8>, const shader::ShaderHash&) noexcept override
    {
        return {};
    }

    [[nodiscard]] virtual shader::GeometryShaderRef
        createGeometryShader(gp::ConstVectorView<gp::UInt8>, const shader::ShaderHash&) noexcept override
    {
        return {};
    }

    [[nodiscard]] virtual shader::ComputeShaderRef
        createComputeShader(gp::ConstVectorView<gp::UInt8>, const shader::ShaderHash&) noexcept override
    {
        return {};
    }

    [[nodiscard]] virtual pipeline::BoundShaderStateRef createBoundShaderState(
        pipeline::VertexDeclaration*, shader::VertexShader*, shader::PixelShader*, shader::GeometryShader*
    ) noexcept override
    {
        return {};
    }

    [[nodiscard]] virtual UniformBufferRef createUniformBuffer(
        const void*, const UniformBufferLayout*, UniformBufferUsage, UniformBufferValidation
    ) noexcept override
    {
        return {};
    }

    virtual void updateUniformBuffer(command::CommandListBase&, UniformBuffer*, const void*) noexcept override
    {}

    [[nodiscard]] virtual BufferInitializer
        createBufferInitializer(command::CommandListBase& cmdList, const BufferCreateDesc& createDesc) noexcept override
    {
        return DefaultBufferInitializer(cmdList, new MockBuffer(createDesc));
    }

    [[nodiscard]] virtual void* lockBufferBottomOfPipe(
        command::CommandListBase& cmdList,
        Buffer* const buffer,
        const gp::UInt32 offset,
        const gp::UInt32 size,
        const ResourceLockMode lockMode
    ) noexcept override
    {
        static_cast<void>(cmdList);
        ++lockCalls;
        lastLockOffset = offset;
        lastLockSize = size;
        lastLockMode = lockMode;
        MockBuffer* const mockBuffer = static_cast<MockBuffer*>(buffer);
        return mockBuffer->storage.data() + offset;
    }

    virtual void unlockBufferBottomOfPipe(command::CommandListBase& cmdList, Buffer* const buffer) noexcept override
    {
        static_cast<void>(cmdList);
        static_cast<void>(buffer);
        ++unlockCalls;
    }

    [[nodiscard]] virtual TextureInitializer
        createTextureInitializer(command::CommandListBase& cmdList, const TextureCreateDesc&) noexcept override
    {
        return TextureInitializer(
            cmdList, TextureRef{}, TextureInitializer::SubresourceCallback{}, TextureInitializer::FinalizeCallback{}
        );
    }

    [[nodiscard]] virtual TextureLockResult
        lockTexture(command::CommandListImmediate&, const TextureLockArgs&) noexcept override
    {
        return {};
    }

    virtual void unlockTexture(command::CommandListImmediate&, const TextureLockArgs&) noexcept override
    {}

    virtual void updateTexture2D(
        command::CommandListBase&, Texture*, gp::UInt32, const UpdateTextureRegion2D&, gp::UInt32, const gp::UInt8*
    ) noexcept override
    {}

    virtual void updateTexture3D(
        command::CommandListBase&,
        Texture*,
        gp::UInt32,
        const UpdateTextureRegion3D&,
        gp::UInt32,
        gp::UInt32,
        const gp::UInt8*
    ) noexcept override
    {}

    virtual void readSurfaceData(
        Texture*, const IntRect&, gp::Vector<gp::math::Color>&, const ReadSurfaceDataFlags&
    ) noexcept override
    {}

    [[nodiscard]] virtual view::ShaderResourceViewRef
        createShaderResourceView(command::CommandListBase&, Buffer*, const view::ViewDesc&) noexcept override
    {
        return {};
    }

    [[nodiscard]] virtual view::ShaderResourceViewRef
        createShaderResourceView(command::CommandListBase&, Texture*, const view::ViewDesc&) noexcept override
    {
        return {};
    }

    [[nodiscard]] virtual view::UnorderedAccessViewRef
        createUnorderedAccessView(command::CommandListBase&, Buffer*, const view::ViewDesc&) noexcept override
    {
        return {};
    }

    [[nodiscard]] virtual view::UnorderedAccessViewRef
        createUnorderedAccessView(command::CommandListBase&, Texture*, const view::ViewDesc&) noexcept override
    {
        return {};
    }

    [[nodiscard]] virtual sync::GPUFenceRef createGPUFence(gp::Name) noexcept override
    {
        return {};
    }

    [[nodiscard]] virtual presentation::ViewportRef
        createViewport(void*, gp::UInt32, gp::UInt32, bool, PixelFormat) noexcept override
    {
        return {};
    }

    virtual void resizeViewport(presentation::Viewport*, gp::UInt32, gp::UInt32, bool, PixelFormat) noexcept override
    {}

    virtual void beginDrawingViewport(presentation::Viewport*, Texture*) noexcept override
    {}

    using DynamicRHI::endDrawingViewport;

    virtual void
        endDrawingViewport(presentation::Viewport*, const presentation::PresentArgsWithStats&) noexcept override
    {}

    [[nodiscard]] virtual TextureRef getViewportBackBuffer(presentation::Viewport*) noexcept override
    {
        return {};
    }

    virtual void advanceFrameForGetViewportBackBuffer(presentation::Viewport*) noexcept override
    {}

    [[nodiscard]] virtual command::ICommandContext* getDefaultContext() noexcept override
    {
        return nullptr;
    }

    [[nodiscard]] virtual command::IComputeContext* getDefaultAsyncComputeContext() noexcept override
    {
        return nullptr;
    }

    virtual void blockUntilGPUIdle() noexcept override
    {}
};

struct ScopedBypassDeleteCheck final
{
    bool previousBypass{ false };

    ScopedBypassDeleteCheck() noexcept
        : previousBypass(ResourceDeletionQueue::isBypassingDeletionQueueCheckOnExit())
    {
        ResourceDeletionQueue::setBypassDeletionQueueCheckOnExit(true);
    }

    ~ScopedBypassDeleteCheck() noexcept
    {
        ResourceDeletionQueue::flushPendingDeletes(true);
        ResourceDeletionQueue::setBypassDeletionQueueCheckOnExit(previousBypass);
    }
};

struct ScopedDynamicRHI final
{
    DynamicRHI* previousRHI{ nullptr };

    explicit ScopedDynamicRHI(DynamicRHI* const rhi) noexcept
        : previousRHI(getDynamicRHI())
    {
        setDynamicRHI(rhi);
    }

    ~ScopedDynamicRHI() noexcept
    {
        setDynamicRHI(previousRHI);
    }
};

}   // namespace

TEST(InitializerScopedMemoryTest, AllocateMoveAndRelease)
{
    InitializerScopedMemory zeroAlloc(0u, 16u);
    EXPECT_EQ(zeroAlloc.pointer, nullptr);

    InitializerScopedMemory mem(128u, 16u);
    ASSERT_NE(mem.pointer, nullptr);
    EXPECT_EQ(reinterpret_cast<gp::UIntPtr>(mem.pointer) % 16u, 0u);

    void* const originalPtr = mem.pointer;
    InitializerScopedMemory moved(std::move(mem));
    EXPECT_EQ(mem.pointer, nullptr);
    EXPECT_EQ(moved.pointer, originalPtr);

    InitializerScopedMemory assigned(64u, 16u);
    ASSERT_NE(assigned.pointer, nullptr);
    assigned = std::move(moved);
    EXPECT_EQ(moved.pointer, nullptr);
    EXPECT_EQ(assigned.pointer, originalPtr);

    assigned.release();
    EXPECT_EQ(assigned.pointer, nullptr);
    // Double release is a safe no-op.
    assigned.release();

    // Pointer adoption constructor: nullptr and pre-allocated heap block.
    InitializerScopedMemory nullAdopted(nullptr);
    EXPECT_EQ(nullAdopted.pointer, nullptr);

    gp::memory::Malloc* const allocator = gp::memory::getGlobalMalloc();
    ASSERT_NE(allocator, nullptr);
    void* const rawBlock = allocator->allocate(64u, 16u);
    ASSERT_NE(rawBlock, nullptr);
    InitializerScopedMemory adopted(rawBlock);
    EXPECT_EQ(adopted.pointer, rawBlock);
    adopted.release();
    EXPECT_EQ(adopted.pointer, nullptr);
}

TEST(BufferInitializerHelpersTest, DefaultBufferInitializerWithAndWithoutWritableData)
{
    const ScopedBypassDeleteCheck deleteGuard;
    command::CommandListImmediate cmdList;

    const BufferCreateDesc desc = BufferCreateDesc::createVertex("DefaultBuf", 16u).determineInitialState();

    // Two-arg constructor: no writable pointer, returns buffer on finalize.
    {
        MockBuffer* const rawBuffer = new MockBuffer(desc);
        DefaultBufferInitializer init(cmdList, rawBuffer);
        EXPECT_TRUE(init.isValid());
        EXPECT_EQ(init.getWritableData(), nullptr);
        EXPECT_EQ(init.getWritableDataSize(), 0u);

        const BufferRef finalized = init.finalize();
        EXPECT_EQ(finalized.get(), rawBuffer);
        EXPECT_FALSE(init.isValid());
    }

    // Four-arg constructor: exposes writable span and allows writes before finalize.
    {
        MockBuffer* const rawBuffer = new MockBuffer(desc);
        gp::Array<gp::UInt8, 16u> backing{};
        DefaultBufferInitializer init(cmdList, rawBuffer, backing.data(), backing.size());
        EXPECT_EQ(init.getWritableData(), backing.data());
        EXPECT_EQ(init.getWritableDataSize(), 16u);

        init.fillWithValue(0xABu);
        const BufferRef finalized = init.finalize();
        EXPECT_EQ(finalized.get(), rawBuffer);
        EXPECT_EQ(backing[0], 0xABu);
        EXPECT_EQ(backing[15], 0xABu);
    }
}

TEST(BufferInitializerHelpersTest, LockBufferInitializerLocksWritesAndUnlocksOnFinalize)
{
    const ScopedBypassDeleteCheck deleteGuard;
    MockLockBufferDynamicRHI mockRHI;
    const ScopedDynamicRHI rhiGuard(&mockRHI);

    // Bypass (bottom-of-pipe) mode: locks immediately on construction and unlocks on finalize.
    {
        command::CommandListExecutor& executor = command::getCommandListExecutor();
        const bool previousBypass = executor.bypass();
        executor.setBypass(true);

        command::CommandListImmediate& cmdList = executor.getImmediateCommandList();

        const BufferCreateDesc desc = BufferCreateDesc::createVertex("LockBufBypass", 16u).determineInitialState();
        MockBuffer* const rawBuffer = new MockBuffer(desc);

        {
            LockBufferInitializer init(cmdList, rawBuffer);
            EXPECT_EQ(mockRHI.lockCalls, 1u);
            EXPECT_EQ(mockRHI.unlockCalls, 0u);
            EXPECT_EQ(mockRHI.lastLockOffset, 0u);
            EXPECT_EQ(mockRHI.lastLockSize, 16u);
            EXPECT_EQ(mockRHI.lastLockMode, ResourceLockMode::WriteOnly);
            ASSERT_NE(init.getWritableData(), nullptr);

            const gp::Array<gp::UInt32, 4u> payload{ 1u, 2u, 3u, 4u };
            init.writeArray<gp::UInt32>(payload);

            const BufferRef finalized = init.finalize();
            EXPECT_EQ(finalized.get(), rawBuffer);
            EXPECT_EQ(mockRHI.unlockCalls, 1u);
        }

        const gp::UInt32* const stored = reinterpret_cast<const gp::UInt32*>(rawBuffer->storage.data());
        EXPECT_EQ(stored[0], 1u);
        EXPECT_EQ(stored[1], 2u);
        EXPECT_EQ(stored[2], 3u);
        EXPECT_EQ(stored[3], 4u);

        executor.setBypass(previousBypass);
    }

    // Queued (top-of-pipe) mode: stages writes and commits to backend on executeList.
    {
        mockRHI.lockCalls = 0u;
        mockRHI.unlockCalls = 0u;

        command::CommandListExecutor& executor = command::getCommandListExecutor();
        const bool previousBypass = executor.bypass();
        executor.setBypass(false);

        command::CommandListImmediate cmdList;
        const BufferCreateDesc desc = BufferCreateDesc::createVertex("LockBufQueued", 16u).determineInitialState();
        MockBuffer* const rawBuffer = new MockBuffer(desc);

        BufferRef finalized;
        {
            LockBufferInitializer init(cmdList, rawBuffer);
            ASSERT_NE(init.getWritableData(), nullptr);
            EXPECT_EQ(mockRHI.lockCalls, 0u);

            const gp::Array<gp::UInt32, 4u> payload{ 10u, 20u, 30u, 40u };
            init.writeArray<gp::UInt32>(payload);

            finalized = init.finalize();
            EXPECT_EQ(finalized.get(), rawBuffer);
        }

        executor.executeList(cmdList);
        EXPECT_EQ(mockRHI.lockCalls, 1u);
        EXPECT_EQ(mockRHI.unlockCalls, 1u);
        EXPECT_EQ(mockRHI.lastLockOffset, 0u);
        EXPECT_EQ(mockRHI.lastLockSize, 16u);
        EXPECT_EQ(mockRHI.lastLockMode, ResourceLockMode::WriteOnly);

        const gp::UInt32* const stored = reinterpret_cast<const gp::UInt32*>(rawBuffer->storage.data());
        EXPECT_EQ(stored[0], 10u);
        EXPECT_EQ(stored[1], 20u);
        EXPECT_EQ(stored[2], 30u);
        EXPECT_EQ(stored[3], 40u);

        executor.setBypass(previousBypass);
    }
}

TEST(BufferInitializerHelpersTest, CustomBufferInitializerSupportsMoveOnlyCallback)
{
    const ScopedBypassDeleteCheck deleteGuard;
    command::CommandListImmediate cmdList;

    const BufferCreateDesc desc = BufferCreateDesc::createVertex("CustomBuf", 32u).determineInitialState();
    MockBuffer* const rawBuffer = new MockBuffer(desc);

    InitializerScopedMemory scopedScratch(32u, 16u);
    void* const scratchPtr = scopedScratch.pointer;
    bool callbackRan = false;

    CustomBufferInitializer init(
        cmdList,
        rawBuffer,
        scratchPtr,
        32u,
        [heldBuffer = BufferRef(rawBuffer),
         scratch = std::move(scopedScratch),
         &callbackRan](command::CommandListBase&) mutable noexcept -> BufferRef
    {
        callbackRan = (scratch.pointer != nullptr);
        return std::move(heldBuffer);
    }
    );

    init.fillWithValue(0x5Cu);
    const BufferRef finalized = init.finalize();
    EXPECT_TRUE(callbackRan);
    EXPECT_EQ(finalized.get(), rawBuffer);
}

TEST(BufferInitializerHelpersTest, CreateUnifiedMemoryBufferInitializerAllInitActions)
{
    const ScopedBypassDeleteCheck deleteGuard;
    command::CommandListImmediate cmdList;

    // 1. Default: leaves backing memory untouched.
    {
        gp::Array<gp::UInt8, 16u> backing{};
        backing.fill(0xEEu);
        const BufferCreateDesc desc =
            BufferCreateDesc::createVertex("UnifiedDefault", 16u).setInitActionNone().determineInitialState();
        MockBuffer* const rawBuffer = new MockBuffer(desc);

        BufferInitializer init = createUnifiedMemoryBufferInitializer(cmdList, desc, rawBuffer, backing.data());
        EXPECT_EQ(init.getWritableData(), nullptr);
        EXPECT_EQ(init.finalize().get(), rawBuffer);
        EXPECT_EQ(backing[0], 0xEEu);
    }

    // 2. ResourceArray: copies immediately and discards upload array.
    {
        gp::Array<gp::UInt8, 8u> backing{};
        const gp::Array<gp::UInt8, 8u> src{ 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u };
        MockUploadArray uploadArray(src.data(), static_cast<gp::UInt32>(src.size()));

        const BufferCreateDesc desc = BufferCreateDesc::createVertex("UnifiedArray", 8u)
                                          .setInitActionResourceArray(&uploadArray)
                                          .determineInitialState();
        MockBuffer* const rawBuffer = new MockBuffer(desc);

        BufferInitializer init = createUnifiedMemoryBufferInitializer(cmdList, desc, rawBuffer, backing.data());
        EXPECT_EQ(uploadArray.discardCount, 1u);
        EXPECT_EQ(init.finalize().get(), rawBuffer);
        EXPECT_EQ(backing, src);
    }

    // 3. Zeroed: zeroes memory immediately.
    {
        gp::Array<gp::UInt8, 16u> backing{};
        backing.fill(0xFFu);
        const BufferCreateDesc desc =
            BufferCreateDesc::createVertex("UnifiedZeroed", 16u).setInitActionZeroData().determineInitialState();
        MockBuffer* const rawBuffer = new MockBuffer(desc);

        BufferInitializer init = createUnifiedMemoryBufferInitializer(cmdList, desc, rawBuffer, backing.data());
        EXPECT_EQ(init.finalize().get(), rawBuffer);
        EXPECT_TRUE(gp::memory::isMemoryZeroed(backing.data(), backing.size()));
    }

    // 4. Initializer: exposes writableData and size to caller.
    {
        gp::Array<gp::UInt8, 16u> backing{};
        const BufferCreateDesc desc =
            BufferCreateDesc::createVertex("UnifiedInit", 16u).setInitActionInitializer().determineInitialState();
        MockBuffer* const rawBuffer = new MockBuffer(desc);

        BufferInitializer init = createUnifiedMemoryBufferInitializer(cmdList, desc, rawBuffer, backing.data());
        EXPECT_EQ(init.getWritableData(), backing.data());
        EXPECT_EQ(init.getWritableDataSize(), 16u);
        init.fillWithValue(0x77u);
        EXPECT_EQ(init.finalize().get(), rawBuffer);
        EXPECT_EQ(backing[0], 0x77u);
        EXPECT_EQ(backing[15], 0x77u);
    }
}

TEST(BufferInitializerHelpersTest, DestructorAutoFinalizesDefaultAndLockInitializers)
{
    const ScopedBypassDeleteCheck deleteGuard;
    MockLockBufferDynamicRHI mockRHI;
    const ScopedDynamicRHI rhiGuard(&mockRHI);

    command::CommandListExecutor& executor = command::getCommandListExecutor();
    const bool previousBypass = executor.bypass();
    executor.setBypass(true);

    command::CommandListImmediate& cmdList = executor.getImmediateCommandList();
    const BufferCreateDesc desc = BufferCreateDesc::createVertex("AutoFinalizeBuf", 16u).determineInitialState();

    // DefaultBufferInitializer destroyed without calling finalize() must not leak or crash.
    {
        MockBuffer* const defaultBuf = new MockBuffer(desc);
        const DefaultBufferInitializer init(cmdList, defaultBuf);
        EXPECT_TRUE(init.isValid());
    }

    // LockBufferInitializer destroyed without calling finalize() must still unlock on destruction.
    {
        MockBuffer* const lockBuf = new MockBuffer(desc);
        {
            LockBufferInitializer init(cmdList, lockBuf);
            EXPECT_EQ(mockRHI.lockCalls, 1u);
            EXPECT_EQ(mockRHI.unlockCalls, 0u);
            init.fillWithValue(0x3Au);
        }
        EXPECT_EQ(mockRHI.unlockCalls, 1u);
    }

    // Zero-size ResourceArray and Zeroed paths in createUnifiedMemoryBufferInitializer do not dereference null.
    {
        MockUploadArray emptyUpload(nullptr, 0u);
        const BufferCreateDesc zeroArrayDesc = BufferCreateDesc::createVertex("ZeroArray", 0u)
                                                   .setInitActionResourceArray(&emptyUpload)
                                                   .determineInitialState();
        MockBuffer* const rawBufA = new MockBuffer(zeroArrayDesc);
        BufferInitializer initA = createUnifiedMemoryBufferInitializer(cmdList, zeroArrayDesc, rawBufA, nullptr);
        EXPECT_EQ(emptyUpload.discardCount, 1u);
        EXPECT_EQ(initA.finalize().get(), rawBufA);

        const BufferCreateDesc zeroedDesc =
            BufferCreateDesc::createVertex("ZeroZeroed", 0u).setInitActionZeroData().determineInitialState();
        MockBuffer* const rawBufB = new MockBuffer(zeroedDesc);
        BufferInitializer initB = createUnifiedMemoryBufferInitializer(cmdList, zeroedDesc, rawBufB, nullptr);
        EXPECT_EQ(initB.finalize().get(), rawBufB);
    }

    executor.setBypass(previousBypass);
}

TEST(BufferInitializerHelpersTest, InvalidBufferStoresCreateDesc)
{
    const ScopedBypassDeleteCheck deleteGuard;
    const BufferCreateDesc desc = BufferCreateDesc::createVertex("InvalidFallback", 64u).determineInitialState();
    const BufferRef fallback(new InvalidBuffer(desc));
    ASSERT_TRUE(fallback.isValid());
    EXPECT_EQ(fallback->getSize(), 64u);
    EXPECT_EQ(fallback->getUsage(), BufferUsageFlags::VertexBuffer);
}

}   // namespace gp::rhi::resource::tests
