// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "../resources/VulkanResourceTestFakes.hpp"
#include "CoreMinimal.hpp"
#include "memory/pointers/RefCountedPtr.hpp"
#include "resources/BufferCreateDesc.hpp"
#include "resources/BufferEnums.hpp"
#include "resources/TextureCreateDesc.hpp"
#include "resources/TextureEnums.hpp"
#include "resources/VulkanBuffer.hpp"
#include "resources/VulkanTexture.hpp"
#include "RHIPixelFormat.hpp"
#include "views/ViewForward.hpp"
#include "views/VulkanShaderResourceView.hpp"
#include "views/VulkanUnorderedAccessView.hpp"

namespace gp::rhi::vulkan::tests
{

using VulkanViewTestBufferRef = gp::RefCountedPtr<VulkanBuffer>;
using VulkanViewTestTextureRef = gp::RefCountedPtr<VulkanTexture>;

/// @brief Live fixture creating real buffers and textures to build views on.
class VulkanViewLiveTest : public VulkanResourceLiveTest
{
protected:
    static constexpr gp::Int32 kTextureSize = 8;
    static constexpr gp::UInt32 kBufferSize = 1024u;

    /// @brief Creates a buffer straight from the constructor.
    /// @param[in] usage Buffer usage flags.
    /// @param[in] size Byte size.
    /// @param[in] stride Element stride (structured buffers).
    /// @return Owning reference.
    [[nodiscard]] VulkanViewTestBufferRef makeBuffer(
        const resource::BufferUsageFlags usage, const gp::UInt32 size = kBufferSize, const gp::UInt32 stride = 0u
    ) noexcept
    {
        const resource::BufferCreateDesc desc = resource::BufferCreateDesc::create("ViewBuffer", size, stride, usage);
        return VulkanViewTestBufferRef(new VulkanBuffer(m_services, desc));
    }

    /// @brief Creates a 2D texture.
    /// @param[in] flags Texture creation flags.
    /// @param[in] format Texel format.
    /// @return Owning reference.
    [[nodiscard]] VulkanViewTestTextureRef makeTexture2D(
        const resource::TextureCreateFlags flags, const PixelFormat format = PixelFormat::R8G8B8A8_UNorm
    ) noexcept
    {
        resource::TextureCreateDesc desc =
            resource::TextureCreateDesc::create2D("ViewTexture2D", kTextureSize, kTextureSize, format);
        desc.setFlags(flags);
        return createTexture(desc);
    }

    /// @brief Creates an RGBA8 cube texture.
    /// @param[in] flags Texture creation flags.
    /// @return Owning reference.
    [[nodiscard]] VulkanViewTestTextureRef makeTextureCube(const resource::TextureCreateFlags flags) noexcept
    {
        resource::TextureCreateDesc desc = resource::TextureCreateDesc::createCube(
            "ViewTextureCube", static_cast<gp::UInt32>(kTextureSize), PixelFormat::R8G8B8A8_UNorm
        );
        desc.setFlags(flags);
        return createTexture(desc);
    }

    /// @brief Creates a texture from `desc`.
    /// @param[in] desc Texture description.
    /// @return Owning reference.
    [[nodiscard]] VulkanViewTestTextureRef createTexture(const resource::TextureCreateDesc& desc) noexcept
    {
        return VulkanViewTestTextureRef(new VulkanTexture(m_services, cmdList(), desc));
    }

    /// @brief Returns the Vulkan side of an SRV reference.
    /// @param[in] view SRV reference.
    /// @return Vulkan SRV.
    [[nodiscard]] static VulkanShaderResourceView* asVulkan(const view::ShaderResourceViewRef& view) noexcept
    {
        return static_cast<VulkanShaderResourceView*>(view.get());
    }

    /// @brief Returns the Vulkan side of a UAV reference.
    /// @param[in] view UAV reference.
    /// @return Vulkan UAV.
    [[nodiscard]] static VulkanUnorderedAccessView* asVulkan(const view::UnorderedAccessViewRef& view) noexcept
    {
        return static_cast<VulkanUnorderedAccessView*>(view.get());
    }
};

}   // namespace gp::rhi::vulkan::tests
