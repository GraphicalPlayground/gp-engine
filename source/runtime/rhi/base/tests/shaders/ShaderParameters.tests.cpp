// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "shaders/ShaderParameters.hpp"
#include "memory/allocators/PagePool.hpp"
#include "pipeline/SamplerState.hpp"
#include "resources/Buffer.hpp"
#include "resources/BufferCreateDesc.hpp"
#include "resources/Texture.hpp"
#include "resources/TextureCreateDesc.hpp"
#include "resources/UniformBuffer.hpp"
#include "resources/UniformBufferLayout.hpp"
#include "resources/UniformBufferLayoutDesc.hpp"
#include "views/ShaderResourceView.hpp"
#include "views/UnorderedAccessView.hpp"
#include "views/ViewDesc.hpp"
#include <gtest/gtest.h>

namespace gp::rhi::shader::tests
{

namespace
{

class TestBuffer final : public resource::Buffer
{
public:
    explicit TestBuffer(const resource::BufferCreateDesc& desc) noexcept
        : Buffer(desc)
    {}

protected:
    virtual ~TestBuffer() noexcept override = default;
};

class TestTexture final : public resource::Texture
{
public:
    explicit TestTexture(const resource::TextureCreateDesc& desc) noexcept
        : Texture(desc)
    {}

protected:
    virtual ~TestTexture() noexcept override = default;
};

class TestUniformBuffer final : public resource::UniformBuffer
{
public:
    explicit TestUniformBuffer(const resource::UniformBufferLayout* const layout) noexcept
        : UniformBuffer(layout)
    {}

protected:
    virtual ~TestUniformBuffer() noexcept override = default;
};

TEST(ShaderParametersTest, StructSizesAndEquality)
{
    static_assert(sizeof(ShaderParameter) == 8u);

    const ShaderParameter p1{ 1u, 16u, 0u, 32u };
    const ShaderParameter p2{ 1u, 16u, 0u, 32u };
    const ShaderParameter p3{ 2u, 16u, 0u, 32u };
    EXPECT_EQ(p1, p2);
    EXPECT_NE(p1, p3);

    const ShaderParameterUnbind u1{ 4u, ShaderParameterUnbind::Type::ResourceView };
    const ShaderParameterUnbind u2{ 4u, ShaderParameterUnbind::Type::UnorderedAccessView };
    EXPECT_NE(u1, u2);
}

TEST(ShaderParametersTest, BatchedShaderParametersRecordsAllBindingTypesAndResets)
{
    gp::memory::PagePool pool(4096u, 8u);
    BatchedShaderParametersAllocator allocator(&pool);
    BatchedShaderParameters batch(allocator);

    EXPECT_FALSE(batch.hasParameters());

    const gp::UInt32 constantA = 0x11223344u;
    const gp::Float32 constantB = 3.5f;
    batch.setShaderParameter(0u, 0u, sizeof(constantA), &constantA);
    batch.setShaderParameter(1u, 16u, sizeof(constantB), &constantB);
    batch.setShaderParameter(2u, 0u, 0u, &constantA);   // Zero-byte write is ignored.

    EXPECT_TRUE(batch.hasParameters());
    ASSERT_EQ(batch.parameters.size(), 2u);
    EXPECT_EQ(batch.parametersData.size(), sizeof(constantA) + sizeof(constantB));
    EXPECT_EQ(batch.parameters[0].bufferIndex, 0u);
    EXPECT_EQ(batch.parameters[0].baseIndex, 0u);
    EXPECT_EQ(batch.parameters[0].byteOffset, 0u);
    EXPECT_EQ(batch.parameters[0].byteSize, sizeof(constantA));
    EXPECT_EQ(batch.parameters[1].bufferIndex, 1u);
    EXPECT_EQ(batch.parameters[1].baseIndex, 16u);
    EXPECT_EQ(batch.parameters[1].byteOffset, sizeof(constantA));
    EXPECT_EQ(batch.parameters[1].byteSize, sizeof(constantB));

    gp::UInt32 readA = 0u;
    gp::Float32 readB = 0.0f;
    gp::memory::copyMemory(&readA, batch.parametersData.data(), sizeof(readA));
    gp::memory::copyMemory(&readB, batch.parametersData.data() + sizeof(readA), sizeof(readB));
    EXPECT_EQ(readA, constantA);
    EXPECT_FLOAT_EQ(readB, constantB);

    const resource::BufferCreateDesc bufDesc = resource::BufferCreateDesc::create(
        "ParamBuf", 64u, 4u, resource::BufferUsageFlags::ShaderResource | resource::BufferUsageFlags::UnorderedAccess
    );
    const resource::TextureCreateDesc texDesc =
        resource::TextureCreateDesc::create2D("ParamTex").setExtent(4, 4).setFormat(PixelFormat::R8G8B8A8_UNorm);
    resource::BufferRef buffer(new TestBuffer(bufDesc));
    resource::TextureRef texture(new TestTexture(texDesc));
    pipeline::SamplerStateRef sampler(new pipeline::SamplerState());
    view::ShaderResourceViewRef srv(new view::ShaderResourceView(
        buffer.get(), view::ViewDesc::createBufferSRV().setType(view::BufferViewType::Raw).asViewDesc()
    ));
    view::UnorderedAccessViewRef uav(new view::UnorderedAccessView(
        buffer.get(), view::ViewDesc::createBufferUAV().setType(view::BufferViewType::Raw).asViewDesc()
    ));

    resource::UniformBufferLayoutDesc layoutDesc("ParamUB");
    layoutDesc.constantBufferSize = 16u;
    resource::UniformBufferLayoutRef layout(new resource::UniformBufferLayout(layoutDesc));
    resource::UniformBufferRef ub(new TestUniformBuffer(layout.get()));

    batch.setShaderUniformBuffer(0u, ub.get());
    batch.setShaderTexture(1u, texture.get());
    batch.setShaderResourceViewParameter(2u, srv.get());
    batch.setShaderSampler(3u, sampler.get());
    batch.setUAVParameter(4u, uav.get());

    ASSERT_EQ(batch.resourceParameters.size(), 5u);
    EXPECT_EQ(batch.resourceParameters[0].type, ShaderParameterResource::Type::UniformBuffer);
    EXPECT_EQ(batch.resourceParameters[0].resource, ub.get());
    EXPECT_EQ(batch.resourceParameters[1].type, ShaderParameterResource::Type::Texture);
    EXPECT_EQ(batch.resourceParameters[1].resource, texture.get());
    EXPECT_EQ(batch.resourceParameters[2].type, ShaderParameterResource::Type::ResourceView);
    EXPECT_EQ(batch.resourceParameters[2].resource, srv.get());
    EXPECT_EQ(batch.resourceParameters[3].type, ShaderParameterResource::Type::Sampler);
    EXPECT_EQ(batch.resourceParameters[3].resource, sampler.get());
    EXPECT_EQ(batch.resourceParameters[4].type, ShaderParameterResource::Type::UnorderedAccessView);
    EXPECT_EQ(batch.resourceParameters[4].resource, uav.get());

    batch.setBindlessTexture(10u, texture.get());
    batch.setBindlessResourceView(11u, srv.get());
    batch.setBindlessSampler(12u, sampler.get());
    batch.setBindlessUAV(13u, uav.get());

    ASSERT_EQ(batch.bindlessParameters.size(), 4u);
    EXPECT_EQ(batch.bindlessParameters[0].index, 10u);
    EXPECT_EQ(batch.bindlessParameters[1].index, 11u);
    EXPECT_EQ(batch.bindlessParameters[2].index, 12u);
    EXPECT_EQ(batch.bindlessParameters[3].index, 13u);

    batch.reset();
    EXPECT_FALSE(batch.hasParameters());
    allocator.reset();
}

TEST(ShaderParametersTest, AllocatorHandlesPageSpilloverContiguously)
{
    gp::memory::PagePool smallPool(256u, 16u);
    BatchedShaderParametersAllocator allocator(&smallPool);
    BatchedShaderParameters batch(allocator);

    // Append enough entries to force multiple page reallocations inside the small 256-byte pool.
    for (gp::UInt32 i = 0u; i < 64u; ++i)
    {
        const gp::UInt32 payload = 0xA0000000u + i;
        batch.setShaderParameter(0u, i * 4u, sizeof(payload), &payload);
    }

    ASSERT_EQ(batch.parameters.size(), 64u);
    ASSERT_EQ(batch.parametersData.size(), 64u * sizeof(gp::UInt32));
    for (gp::UInt32 i = 0u; i < 64u; ++i)
    {
        gp::UInt32 readValue = 0u;
        gp::memory::copyMemory(&readValue, batch.parametersData.data() + (i * sizeof(gp::UInt32)), sizeof(readValue));
        EXPECT_EQ(readValue, 0xA0000000u + i);
    }
}

TEST(ShaderParametersTest, BatchedShaderUnbindsRecordsSRVAndUAV)
{
    BatchedShaderUnbinds unbinds{};
    EXPECT_FALSE(unbinds.hasParameters());

    unbinds.unsetSRV(2u);
    unbinds.unsetUAV(5u);

    EXPECT_TRUE(unbinds.hasParameters());
    ASSERT_EQ(unbinds.unbinds.size(), 2u);
    EXPECT_EQ(unbinds.unbinds[0].index, 2u);
    EXPECT_EQ(unbinds.unbinds[0].type, ShaderParameterUnbind::Type::ResourceView);
    EXPECT_EQ(unbinds.unbinds[1].index, 5u);
    EXPECT_EQ(unbinds.unbinds[1].type, ShaderParameterUnbind::Type::UnorderedAccessView);

    unbinds.reset();
    EXPECT_FALSE(unbinds.hasParameters());
}

TEST(ShaderParametersTest, UniformBufferStaticBindingsAddTryAddOverwriteAndEquality)
{
    resource::UniformBufferLayoutDesc staticDescA("StaticA");
    staticDescA.constantBufferSize = 16u;
    staticDescA.staticSlot = 1u;

    resource::UniformBufferLayoutDesc staticDescB("StaticB");
    staticDescB.constantBufferSize = 16u;
    staticDescB.staticSlot = 3u;

    resource::UniformBufferLayoutDesc dynamicDesc("Dynamic");
    dynamicDesc.constantBufferSize = 16u;
    dynamicDesc.staticSlot = kMaxUniformBufferStaticSlots;

    resource::UniformBufferLayoutRef layoutA(new resource::UniformBufferLayout(staticDescA));
    resource::UniformBufferLayoutRef layoutB(new resource::UniformBufferLayout(staticDescB));
    resource::UniformBufferLayoutRef layoutDyn(new resource::UniformBufferLayout(dynamicDesc));

    resource::UniformBufferRef ubA1(new TestUniformBuffer(layoutA.get()));
    resource::UniformBufferRef ubA2(new TestUniformBuffer(layoutA.get()));
    resource::UniformBufferRef ubB(new TestUniformBuffer(layoutB.get()));
    resource::UniformBufferRef ubDyn(new TestUniformBuffer(layoutDyn.get()));

    UniformBufferStaticBindings bindings{};
    EXPECT_EQ(bindings.getSlotCount(), 0u);
    EXPECT_EQ(bindings.findUniformBuffer(1u), nullptr);

    bindings.tryAddUniformBuffer(nullptr);
    bindings.tryAddUniformBuffer(ubDyn.get());
    EXPECT_EQ(bindings.getSlotCount(), 0u);

    bindings.addUniformBuffer(ubA1.get());
    bindings.tryAddUniformBuffer(ubB.get());
    EXPECT_EQ(bindings.getUniformBufferCount(), 2u);
    EXPECT_EQ(bindings.getSlotCount(), 4u);
    EXPECT_EQ(bindings.getSlot(0u), 1u);
    EXPECT_EQ(bindings.getUniformBuffer(0u), ubA1.get());
    EXPECT_EQ(bindings.findUniformBuffer(3u), ubB.get());

    // Overwriting an existing static slot keeps entry count and slot extent constant.
    bindings.addUniformBufferAtSlot(1u, ubA2.get());
    EXPECT_EQ(bindings.getUniformBufferCount(), 2u);
    EXPECT_EQ(bindings.getSlotCount(), 4u);
    EXPECT_EQ(bindings.findUniformBuffer(1u), ubA2.get());

    // Insertion order must not affect equality or dense bind() layout.
    UniformBufferStaticBindings reverseOrder{};
    reverseOrder.addUniformBuffer(ubB.get());
    reverseOrder.addUniformBuffer(ubA2.get());
    EXPECT_EQ(bindings, reverseOrder);

    gp::Vector<resource::UniformBuffer*> denseTable{};
    bindings.bind(denseTable);
    ASSERT_EQ(denseTable.size(), 4u);
    EXPECT_EQ(denseTable[0], nullptr);
    EXPECT_EQ(denseTable[1], ubA2.get());
    EXPECT_EQ(denseTable[2], nullptr);
    EXPECT_EQ(denseTable[3], ubB.get());

    UniformBufferStaticBindings copy = bindings;
    EXPECT_EQ(bindings, copy);

    copy.addUniformBufferAtSlot(3u, nullptr);
    EXPECT_NE(bindings, copy);

    bindings.reset();
    EXPECT_EQ(bindings.getUniformBufferCount(), 0u);
    EXPECT_EQ(bindings.getSlotCount(), 0u);
}

}   // namespace

}   // namespace gp::rhi::shader::tests
