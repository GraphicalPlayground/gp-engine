// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/strings/Name.hpp"
#include "core/ClearValueBinding.hpp"
#include "core/ResourceEnums.hpp"
#include "core/ResourceForward.hpp"   // IWYU pragma: keep
#include "core/ResourceTypes.hpp"
#include "core/ViewableResource.hpp"
#include "CoreMinimal.hpp"
#include "maths/base/Arithmetic.hpp"
#include "maths/color/LinearColor.hpp"
#include "memory/MemoryOrder.hpp"
#include "resources/TextureDesc.hpp"
#include "resources/TextureEnums.hpp"
#include "RHIBase.hpp"
#include "RHIPixelFormat.hpp"
#include "templates/NumericLimits.hpp"
#include "threading/atomics/Atomic.hpp"
#include "views/DescriptorHandle.hpp"

namespace gp::rhi::resource
{

/// @brief Base GPU texture resource supporting shader views, fast-clear metadata, and barrier state tracking.
/// @details Stores an immutable `TextureDesc` and an atomic `m_lastRenderTime` timestamp for streaming residency
///          heuristics. `getDesc()` is virtual exclusively so `TextureReference` can forward descriptor queries to its
///          currently referenced target texture.
class GP_RHI_API Texture : public ViewableResource
{
    friend class TextureReference;

public:
    /// @brief Sentinel timestamp indicating that this texture has not yet been rendered or sampled.
    static constexpr gp::Float64 kEmptyLastRenderTime =
        -static_cast<gp::Float64>(gp::NumericLimits<gp::Float32>::max());

public:
    Texture() = delete;
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    Texture(Texture&&) = delete;
    Texture& operator=(Texture&&) = delete;

protected:
    /// @brief Constructs a concrete GPU texture initialized from `createDesc`.
    /// @param[in] createDesc Creation parameters carrying layout, initial access state, and debug/owner names.
    explicit Texture(const TextureCreateDesc& createDesc) noexcept;

    /// @brief Special constructor used solely by `TextureReference` (`ResourceType::TextureReference`).
    /// @param[in] resourceType Must be `ResourceType::TextureReference`.
    explicit Texture(ResourceType resourceType) noexcept;

    /// @copydoc ViewableResource::~ViewableResource()
    virtual ~Texture() noexcept override;

public:
    /// @brief Returns the texture layout descriptor describing extent, format, mips, samples, and flags.
    /// @return Immutable reference to `TextureDesc`.
    [[nodiscard]] virtual const TextureDesc& getDesc() const noexcept;

    /// @brief Returns `this` if the object is a `TextureReference`, or `nullptr` for concrete textures.
    /// @return Pointer to `TextureReference`, or `nullptr`.
    [[nodiscard]] virtual TextureReference* getTextureReference() noexcept;

    /// @brief Returns the default bindless descriptor handle for this texture when bindless rendering is active.
    /// @return Bindless `view::DescriptorHandle`, or an invalid handle if unsupported.
    [[nodiscard]] virtual view::DescriptorHandle getDefaultBindlessHandle() const noexcept;

    /// @brief Returns the backend-native GPU texture handle (e.g. `ID3D12Resource*` or `VkImage`), or `nullptr`.
    /// @return Opaque pointer to native texture object.
    [[nodiscard]] virtual void* getNativeResource() const noexcept;

    /// @brief Returns the backend-native default shader resource view handle, or `nullptr`.
    /// @return Opaque pointer to native SRV handle.
    [[nodiscard]] virtual void* getNativeShaderResourceView() const noexcept;

    /// @brief Returns an opaque pointer to the backend's internal base texture object, or `nullptr`.
    /// @return Opaque pointer to backend texture base.
    [[nodiscard]] virtual void* getTextureBaseRHI() noexcept;

    /// @brief Queries platform-specific fast-clear write-mask metadata buffers if supported by the backend.
    /// @param[out] outData Receives pointer to write-mask metadata, or `nullptr`.
    /// @param[out] outSize Receives size in bytes of write-mask metadata, or `0`.
    virtual void getWriteMaskProperties(void*& outData, gp::UInt32& outSize) noexcept;

public:
    /// @brief Returns `(width, height, slicesOrDepth)` according to texture dimensionality.
    /// @details Returns `(X, Y, 1)` for 2D/Cube, `(X, Y, arraySize)` for 2DArray/CubeArray, and `(X, Y, depth)` for 3D.
    /// @return 3D integer extent vector.
    [[nodiscard]] IntVector getSizeXYZ() const noexcept
    {
        const TextureDesc& desc = getDesc();
        switch (desc.dimension)
        {
        case TextureDimension::Texture2D:
        case TextureDimension::TextureCube:
            return IntVector(desc.extent.x, desc.extent.y, 1);

        case TextureDimension::Texture2DArray:
        case TextureDimension::TextureCubeArray:
            return IntVector(desc.extent.x, desc.extent.y, static_cast<gp::Int32>(desc.arraySize));

        case TextureDimension::Texture3D:
            return IntVector(desc.extent.x, desc.extent.y, static_cast<gp::Int32>(desc.depth));

        default:
            return IntVector(0, 0, 0);
        }
    }

    /// @brief Computes texel dimensions `(width, height, depth)` at mip level `mipIndex`, clamped to at least `1`.
    /// @param[in] mipIndex Zero-based mip level index.
    /// @return 3D integer dimensions for `mipIndex`.
    [[nodiscard]] IntVector getMipDimensions(const gp::UInt8 mipIndex) const noexcept
    {
        const TextureDesc& desc = getDesc();
        return IntVector(
            gp::math::max(desc.extent.x >> mipIndex, 1),
            gp::math::max(desc.extent.y >> mipIndex, 1),
            gp::math::max(static_cast<gp::Int32>(desc.depth) >> mipIndex, 1)
        );
    }

    /// @brief Reports whether this texture is multisampled (`numSamples > 1`).
    /// @return True when multisampled.
    [[nodiscard]] GP_FORCEINLINE_HINT bool isMultisampled() const noexcept
    {
        return getDesc().numSamples > 1u;
    }

    /// @brief Reports whether a fast-clear value is bound to this texture.
    /// @return True when `getDesc().clearValue.colorBinding != ClearBinding::NoneBound`.
    [[nodiscard]] GP_FORCEINLINE_HINT bool hasClearValue() const noexcept
    {
        return getDesc().clearValue.colorBinding != ClearBinding::NoneBound;
    }

    /// @brief Returns the linear RGBA fast-clear color bound to this texture.
    /// @return Clear `gp::math::LinearColor`.
    [[nodiscard]] GP_FORCEINLINE_HINT gp::math::LinearColor getClearColor() const noexcept
    {
        return getDesc().clearValue.getClearColor();
    }

    /// @brief Extracts both depth and stencil fast-clear values bound to this texture.
    /// @param[out] outDepth Receives the depth clear value.
    /// @param[out] outStencil Receives the stencil clear value.
    GP_FORCEINLINE_HINT void getDepthStencilClearValue(gp::Float32& outDepth, gp::UInt32& outStencil) const noexcept
    {
        getDesc().clearValue.getDepthStencil(outDepth, outStencil);
    }

    /// @brief Returns the depth fast-clear value bound to this texture.
    /// @return Depth clear value in `[0.0f, 1.0f]`.
    [[nodiscard]] GP_FORCEINLINE_HINT gp::Float32 getDepthClearValue() const noexcept
    {
        gp::Float32 depthValue = 0.0f;
        gp::UInt32 stencilValue = 0u;
        getDesc().clearValue.getDepthStencil(depthValue, stencilValue);
        return depthValue;
    }

    /// @brief Returns the stencil fast-clear value bound to this texture.
    /// @return Stencil clear value.
    [[nodiscard]] GP_FORCEINLINE_HINT gp::UInt32 getStencilClearValue() const noexcept
    {
        gp::Float32 depthValue = 0.0f;
        gp::UInt32 stencilValue = 0u;
        getDesc().clearValue.getDepthStencil(depthValue, stencilValue);
        return stencilValue;
    }

    /// @brief Updates the last-rendered timestamp used by texture streaming residency heuristics.
    /// @param[in] lastRenderTime Engine time in seconds when this texture was last used.
    GP_FORCEINLINE_HINT void setLastRenderTime(const gp::Float64 lastRenderTime) noexcept
    {
        m_lastRenderTime.store(lastRenderTime, gp::memory::MemoryOrder::Relaxed);
    }

    /// @brief Reads the last-rendered timestamp recorded for this texture.
    /// @return Timestamp in seconds, or `kEmptyLastRenderTime` if never updated.
    [[nodiscard]] GP_FORCEINLINE_HINT gp::Float64 getLastRenderTime() const noexcept
    {
        return m_lastRenderTime.load(gp::memory::MemoryOrder::Relaxed);
    }

    /// @brief Replaces the debug name stored on this texture.
    /// @param[in] inName New interned debug identifier.
    void setName(gp::Name inName) noexcept;

    /// @brief Returns `this` if the texture's own descriptor is `Texture2D`, otherwise `nullptr`.
    /// @return Pointer to `this` or `nullptr`.
    [[nodiscard]] GP_FORCEINLINE_HINT Texture* getTexture2D() noexcept
    {
        return (m_textureDesc.dimension == TextureDimension::Texture2D) ? this : nullptr;
    }

    /// @brief Returns `this` if the texture's own descriptor is `Texture2DArray`, otherwise `nullptr`.
    /// @return Pointer to `this` or `nullptr`.
    [[nodiscard]] GP_FORCEINLINE_HINT Texture* getTexture2DArray() noexcept
    {
        return (m_textureDesc.dimension == TextureDimension::Texture2DArray) ? this : nullptr;
    }

    /// @brief Returns `this` if the texture's own descriptor is `Texture3D`, otherwise `nullptr`.
    /// @return Pointer to `this` or `nullptr`.
    [[nodiscard]] GP_FORCEINLINE_HINT Texture* getTexture3D() noexcept
    {
        return (m_textureDesc.dimension == TextureDimension::Texture3D) ? this : nullptr;
    }

    /// @brief Returns `this` if the texture's own descriptor is `TextureCube` or `TextureCubeArray`, otherwise
    /// `nullptr`.
    /// @return Pointer to `this` or `nullptr`.
    [[nodiscard]] GP_FORCEINLINE_HINT Texture* getTextureCube() noexcept
    {
        return m_textureDesc.isTextureCube() ? this : nullptr;
    }

    /// @brief Returns the base mip width in texels.
    /// @return Width in texels.
    [[nodiscard]] GP_FORCEINLINE_HINT gp::UInt32 getSizeX() const noexcept
    {
        return static_cast<gp::UInt32>(getDesc().extent.x);
    }

    /// @brief Returns the base mip height in texels.
    /// @return Height in texels.
    [[nodiscard]] GP_FORCEINLINE_HINT gp::UInt32 getSizeY() const noexcept
    {
        return static_cast<gp::UInt32>(getDesc().extent.y);
    }

    /// @brief Returns the base mip width and height as an `IntPoint`.
    /// @return 2D base extent.
    [[nodiscard]] GP_FORCEINLINE_HINT IntPoint getSizeXY() const noexcept
    {
        return getDesc().extent;
    }

    /// @brief Returns the Z extent (`1` for 2D/cube, `arraySize` for arrays, `depth` for 3D).
    /// @return Third dimension count.
    [[nodiscard]] GP_FORCEINLINE_HINT gp::UInt32 getSizeZ() const noexcept
    {
        return static_cast<gp::UInt32>(getSizeXYZ().z);
    }

    /// @brief Returns the square face size of a cubemap texture.
    /// @note Asserts that `getDesc().isTextureCube()` is true.
    /// @return Cubemap face width in texels.
    [[nodiscard]] GP_FORCEINLINE_HINT gp::UInt32 getSize() const noexcept
    {
        const TextureDesc& desc = getDesc();
        GP_CHECKF(desc.isTextureCube(), "Texture::getSize() is only valid for cubemap textures.");
        return static_cast<gp::UInt32>(desc.extent.x);
    }

    /// @brief Returns the number of mip levels in this texture.
    /// @return Mip count.
    [[nodiscard]] GP_FORCEINLINE_HINT gp::UInt32 getNumMips() const noexcept
    {
        return static_cast<gp::UInt32>(getDesc().numMips);
    }

    /// @brief Returns the primary storage pixel format of this texture.
    /// @return `PixelFormat`.
    [[nodiscard]] GP_FORCEINLINE_HINT PixelFormat getFormat() const noexcept
    {
        return getDesc().format;
    }

    /// @brief Returns the creation flags assigned to this texture.
    /// @return Bitmask of `TextureCreateFlags`.
    [[nodiscard]] GP_FORCEINLINE_HINT TextureCreateFlags getFlags() const noexcept
    {
        return getDesc().flags;
    }

    /// @brief Returns the sample count per pixel (`1` for non-MSAA textures).
    /// @return Sample count.
    [[nodiscard]] GP_FORCEINLINE_HINT gp::UInt32 getNumSamples() const noexcept
    {
        return static_cast<gp::UInt32>(getDesc().numSamples);
    }

    /// @brief Returns the bound clear value descriptor of this texture.
    /// @return Copy of `ClearValueBinding`.
    [[nodiscard]] GP_FORCEINLINE_HINT ClearValueBinding getClearBinding() const noexcept
    {
        return getDesc().clearValue;
    }

private:
    /// @brief Immutable layout descriptor for concrete textures (unused on `TextureReference`).
    TextureDesc m_textureDesc{};

    /// @brief Atomic last-render timestamp updated during frame submission.
    gp::thread::Atomic<gp::Float64> m_lastRenderTime{ kEmptyLastRenderTime };
};

}   // namespace gp::rhi::resource
