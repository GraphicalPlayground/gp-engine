// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "commands/CommandForward.hpp"
#include "core/ResourceForward.hpp"
#include "core/ResourceTypes.hpp"
#include "CoreMinimal.hpp"
#include "device/DeviceForward.hpp"
#include "hashing/Fnv1a.hpp"
#include "memory/Memory.hpp"
#include "resources/Buffer.hpp"
#include "resources/BufferEnums.hpp"
#include "resources/Texture.hpp"
#include "resources/TextureEnums.hpp"
#include "RHIBase.hpp"
#include "RHIPixelFormat.hpp"
#include "views/ViewEnums.hpp"
#include "views/ViewForward.hpp"   // IWYU pragma: keep
#include <type_traits>

namespace gp::rhi::view
{

/// @brief Packed 16-byte descriptor specifying a buffer or texture shader resource view (SRV) or unordered access view
///        (UAV).
/// @details Uses a discriminated union with zeroed padding so view descriptors compare and hash cleanly by raw byte
///          identity. Construct descriptors via the typed fluent factories (`createBufferSRV()`, `createBufferUAV()`,
///          `createTextureSRV()`, `createTextureUAV()`) and resolve backend-ready parameters via `getViewInfo()`.
struct ViewDesc
{
    /// @brief Shared header fields present at the start of every union arm.
    struct Common
    {
        /// @brief High-level view category (BufferSRV, BufferUAV, TextureSRV, or TextureUAV).
        ViewType viewType;

        /// @brief Override pixel format, or `PixelFormat::Unknown` to inherit or use untyped access.
        PixelFormat format;
    };

    /// @brief Shared descriptor layout for buffer SRVs and buffer UAVs.
    struct Buffer : public Common
    {
        /// @brief Element count and stride pair used by typed, structured, and raw buffer views.
        struct ElementRange
        {
            /// @brief Number of elements exposed by the view (`0` means the entire remaining buffer from offset 0).
            gp::UInt32 numElements;

            /// @brief Optional element stride override in bytes (`0` inherits the buffer's own stride).
            gp::UInt32 stride;
        };

        /// @brief Sub-kind controlling how raw buffer memory is interpreted.
        BufferViewType bufferType;

        /// @brief Enables an implicit hardware atomic counter (buffer UAVs only).
        gp::UInt8 atomicCounter : 1;

        /// @brief Enables append/consume counter semantics (buffer UAVs only).
        gp::UInt8 appendBuffer : 1;

        /// @brief Unused padding bits kept zeroed by constructors.
        gp::UInt8 reserved : 6;

        /// @brief Byte offset from the start of the target buffer.
        gp::UInt32 offsetInBytes;

        /// @brief Payload union holding either `(numElements, stride)` or a ray tracing scene pointer.
        union
        {
            ElementRange range;
            resource::RayTracingScene* rayTracingScene;
        };

        struct ViewInfo;

    protected:
        /// @brief Validates and resolves buffer view parameters against `targetBuffer`.
        /// @param[in] targetBuffer Non-null target GPU buffer.
        /// @param[in] minBufferSRVAlignment Minimum byte alignment required for typed buffer views (`0` uses format
        /// `blockBytes`).
        /// @param[in] supportsRawViewsForAnyBuffer True if the backend allows raw views without `ByteAddressBuffer`.
        /// @return Resolved `Buffer::ViewInfo`.
        [[nodiscard]] GP_RHI_API ViewInfo getViewInfo(
            resource::Buffer* targetBuffer,
            gp::UInt32 minBufferSRVAlignment = 0u,
            bool supportsRawViewsForAnyBuffer = false
        ) const noexcept;
    };

    /// @brief Descriptor arm for read-only buffer shader resource views.
    struct BufferSRV : public Buffer
    {
        struct Initializer;
        struct ViewInfo;

        /// @brief Validates and resolves buffer SRV parameters against `targetBuffer`.
        /// @param[in] targetBuffer Non-null target GPU buffer.
        /// @param[in] minBufferSRVAlignment Minimum byte alignment required for typed buffer views (`0` uses format
        /// `blockBytes`).
        /// @param[in] supportsRawViewsForAnyBuffer True if the backend allows raw views without `ByteAddressBuffer`.
        /// @return Resolved `BufferSRV::ViewInfo`.
        [[nodiscard]] GP_RHI_API ViewInfo getViewInfo(
            resource::Buffer* targetBuffer,
            gp::UInt32 minBufferSRVAlignment = 0u,
            bool supportsRawViewsForAnyBuffer = false
        ) const noexcept;
    };

    /// @brief Descriptor arm for read-write buffer unordered access views.
    struct BufferUAV : public Buffer
    {
        struct Initializer;
        struct ViewInfo;

        /// @brief Validates and resolves buffer UAV parameters against `targetBuffer`.
        /// @param[in] targetBuffer Non-null target GPU buffer.
        /// @param[in] minBufferSRVAlignment Minimum byte alignment required for typed buffer views (`0` uses format
        /// `blockBytes`).
        /// @param[in] supportsRawViewsForAnyBuffer True if the backend allows raw views without `ByteAddressBuffer`.
        /// @return Resolved `BufferUAV::ViewInfo`.
        [[nodiscard]] GP_RHI_API ViewInfo getViewInfo(
            resource::Buffer* targetBuffer,
            gp::UInt32 minBufferSRVAlignment = 0u,
            bool supportsRawViewsForAnyBuffer = false
        ) const noexcept;
    };

    /// @brief Shared descriptor layout for texture SRVs and texture UAVs.
    struct Texture : public Common
    {
        /// @brief Texture data or metadata plane accessed by this view.
        resource::TexturePlane plane : resource::kTexturePlaneNumBits;

        /// @brief Disables hardware sRGB-to-linear conversion on sampling (texture SRVs only).
        gp::UInt8 disableSRGB : 1;

        /// @brief Target view dimensionality (`Texture2D`, `Texture2DArray`, `TextureCube`, etc.).
        TextureViewDimension dimension : kTextureViewDimensionNumBits;

        /// @brief Unused padding bit kept zeroed by constructors.
        gp::UInt8 reserved : 1;

        /// @brief Mip slice range (`num == 0` selects all mips for SRVs; UAVs always use `num == 1`).
        resource::Range8 mipRange;

        /// @brief Array slice range (`num == 0` selects all array slices).
        resource::Range16 arrayRange;

        struct ViewInfo;

    protected:
        /// @brief Validates and resolves shared texture view parameters against `targetTexture`.
        /// @param[in] targetTexture Non-null target GPU texture.
        /// @return Resolved `Texture::ViewInfo`.
        [[nodiscard]] GP_RHI_API ViewInfo getViewInfo(resource::Texture* targetTexture) const noexcept;
    };

    /// @brief Descriptor arm for read-only texture shader resource views.
    struct TextureSRV : public Texture
    {
        struct Initializer;
        struct ViewInfo;

        /// @brief Validates and resolves texture SRV parameters against `targetTexture`.
        /// @param[in] targetTexture Non-null target GPU texture.
        /// @return Resolved `TextureSRV::ViewInfo`.
        [[nodiscard]] GP_RHI_API ViewInfo getViewInfo(resource::Texture* targetTexture) const noexcept;
    };

    /// @brief Descriptor arm for read-write texture unordered access views.
    struct TextureUAV : public Texture
    {
        struct Initializer;
        struct ViewInfo;

        /// @brief Validates and resolves texture UAV parameters against `targetTexture`.
        /// @param[in] targetTexture Non-null target GPU texture.
        /// @return Resolved `TextureUAV::ViewInfo`.
        [[nodiscard]] GP_RHI_API ViewInfo getViewInfo(resource::Texture* targetTexture) const noexcept;
    };

    /// @brief Buffer view union exposing SRV and UAV views of the same packed bytes.
    union BufferUnion
    {
        BufferSRV srv;
        BufferUAV uav;
    };

    /// @brief Texture view union exposing SRV and UAV views of the same packed bytes.
    union TextureUnion
    {
        TextureSRV srv;
        TextureUAV uav;
    };

    union
    {
        Common common;
        BufferUnion buffer;
        TextureUnion texture;
    };

public:
    /// @brief Creates a fluent initializer for a read-only buffer SRV.
    /// @return `BufferSRV::Initializer` builder.
    [[nodiscard]] static inline BufferSRV::Initializer createBufferSRV() noexcept;

    /// @brief Creates a fluent initializer for a read-write buffer UAV.
    /// @return `BufferUAV::Initializer` builder.
    [[nodiscard]] static inline BufferUAV::Initializer createBufferUAV() noexcept;

    /// @brief Creates a fluent initializer for a read-only texture SRV.
    /// @return `TextureSRV::Initializer` builder.
    [[nodiscard]] static inline TextureSRV::Initializer createTextureSRV() noexcept;

    /// @brief Creates a fluent initializer for a read-write texture UAV.
    /// @return `TextureUAV::Initializer` builder.
    [[nodiscard]] static inline TextureUAV::Initializer createTextureUAV() noexcept;

    /// @brief Returns a human-readable string name for `bufferType`.
    /// @param[in] bufferType Buffer view sub-kind.
    /// @return Null-terminated static string literal.
    [[nodiscard]] GP_RHI_API static const char* getBufferTypeString(BufferViewType bufferType) noexcept;

    /// @brief Returns a human-readable string name for `dimension`.
    /// @param[in] dimension Texture view dimensionality.
    /// @return Null-terminated static string literal.
    [[nodiscard]] GP_RHI_API static const char* getTextureDimensionString(TextureViewDimension dimension) noexcept;

    /// @brief Reports whether this descriptor describes a shader resource view (`BufferSRV` or `TextureSRV`).
    /// @return True for SRVs; false for UAVs.
    [[nodiscard]] GP_FORCEINLINE_HINT bool isSRV() const noexcept
    {
        return (common.viewType == ViewType::BufferSRV) || (common.viewType == ViewType::TextureSRV);
    }

    /// @brief Reports whether this descriptor describes an unordered access view (`BufferUAV` or `TextureUAV`).
    /// @return True for UAVs; false for SRVs.
    [[nodiscard]] GP_FORCEINLINE_HINT bool isUAV() const noexcept
    {
        return !isSRV();
    }

    /// @brief Reports whether this descriptor targets a GPU buffer (`BufferSRV` or `BufferUAV`).
    /// @return True for buffer views; false for texture views.
    [[nodiscard]] GP_FORCEINLINE_HINT bool isBuffer() const noexcept
    {
        return (common.viewType == ViewType::BufferSRV) || (common.viewType == ViewType::BufferUAV);
    }

    /// @brief Reports whether this descriptor targets a GPU texture (`TextureSRV` or `TextureUAV`).
    /// @return True for texture views; false for buffer views.
    [[nodiscard]] GP_FORCEINLINE_HINT bool isTexture() const noexcept
    {
        return !isBuffer();
    }

    /// @brief Compares two view descriptors byte-for-byte (safe because all padding bytes are zeroed).
    /// @param[in] rhs Descriptor to compare against.
    /// @return True when all 16 bytes match identically.
    [[nodiscard]] bool operator==(const ViewDesc& rhs) const noexcept
    {
        return gp::memory::compareMemory(this, &rhs, sizeof(*this)) == 0;
    }

    /// @brief Computes a deterministic 64-bit hash over the 16 packed descriptor bytes.
    /// @return 64-bit hash digest consistent with bytewise `operator==`.
    [[nodiscard]] gp::UInt64 getHash() const noexcept
    {
        return gp::hashing::Fnv1a64::hash(reinterpret_cast<const gp::UInt8*>(this), sizeof(*this));
    }

    /// @brief Streams the 16 packed descriptor bytes into a streaming hasher.
    /// @tparam InHasher Streaming hasher type.
    /// @param[in,out] hasher Target hasher accumulator.
    template <typename InHasher>
    void hashAppend(InHasher& hasher) const noexcept
    {
        hasher.update(reinterpret_cast<const gp::UInt8*>(this), sizeof(*this));
    }

    /// @brief Constructs a zero-initialized `BufferSRV` descriptor.
    ViewDesc() noexcept
        : ViewDesc(ViewType::BufferSRV)
    {}

protected:
    /// @brief Zero-initializes all 16 bytes and sets `common.viewType` to `viewType`.
    /// @param[in] viewType View category stored in the common header.
    explicit ViewDesc(const ViewType viewType) noexcept
    {
        gp::memory::zeroMemory(this, sizeof(*this));
        common.viewType = viewType;
    }
};

static_assert(sizeof(ViewDesc) == 16u, "ViewDesc must remain packed to exactly 16 bytes.");
static_assert(std::is_trivially_copyable_v<ViewDesc>, "ViewDesc must remain trivially copyable.");
static_assert(std::is_trivially_destructible_v<ViewDesc>, "ViewDesc must remain trivially destructible.");

/// @brief Type-safe fluent builder for `ViewDesc` buffer shader resource views.
struct ViewDesc::BufferSRV::Initializer : private ViewDesc
{
    friend struct ViewDesc;
    friend class View;
    friend class ShaderResourceView;
    friend class ::gp::rhi::DynamicRHI;
    friend class ::gp::rhi::command::CommandListBase;

protected:
    Initializer() noexcept
        : ViewDesc(ViewType::BufferSRV)
    {}

public:
    /// @brief Sets the buffer view interpretation mode (`Typed`, `Structured`, `AccelerationStructure`, or `Raw`).
    /// @param[in] type Non-unknown `BufferViewType`.
    /// @return Reference to `*this`.
    Initializer& setType(const BufferViewType type) noexcept
    {
        GP_CHECK(type != BufferViewType::Unknown);
        buffer.srv.bufferType = type;
        return *this;
    }

    /// @brief Infers `BufferViewType` from `targetBuffer`'s usage flags (`Raw > Structured > AccelerationStructure >
    /// Typed`).
    /// @param[in] targetBuffer Non-null, non-null-resource target GPU buffer.
    /// @return Reference to `*this`.
    Initializer& setTypeFromBuffer(resource::Buffer* const targetBuffer) noexcept
    {
        GP_CHECK(targetBuffer != nullptr);
        GP_CHECKF(
            !targetBuffer->getDesc().isNull(),
            "Cannot infer buffer view type from a null placeholder buffer; call setType() explicitly."
        );

        const resource::BufferUsageFlags usage = targetBuffer->getDesc().usage;
        if ((usage & resource::BufferUsageFlags::ByteAddressBuffer) != resource::BufferUsageFlags::None)
        {
            buffer.srv.bufferType = BufferViewType::Raw;
        }
        else if ((usage & resource::BufferUsageFlags::StructuredBuffer) != resource::BufferUsageFlags::None)
        {
            buffer.srv.bufferType = BufferViewType::Structured;
        }
        else if ((usage & resource::BufferUsageFlags::AccelerationStructure) != resource::BufferUsageFlags::None)
        {
            buffer.srv.bufferType = BufferViewType::AccelerationStructure;
        }
        else
        {
            buffer.srv.bufferType = BufferViewType::Typed;
        }
        return *this;
    }

    /// @brief Sets the typed buffer pixel format.
    /// @param[in] inFormat `PixelFormat` for typed buffer views.
    /// @return Reference to `*this`.
    Initializer& setFormat(const PixelFormat inFormat) noexcept
    {
        buffer.srv.format = inFormat;
        return *this;
    }

    /// @brief Sets the starting byte offset into the target buffer.
    /// @param[in] inOffsetBytes Byte offset from buffer start.
    /// @return Reference to `*this`.
    Initializer& setOffsetInBytes(const gp::UInt32 inOffsetBytes) noexcept
    {
        buffer.srv.offsetInBytes = inOffsetBytes;
        return *this;
    }

    /// @brief Overrides the structured buffer element stride in bytes.
    /// @param[in] inStride Element stride in bytes (`0` inherits from the buffer descriptor).
    /// @return Reference to `*this`.
    Initializer& setStride(const gp::UInt32 inStride) noexcept
    {
        GP_CHECK(
            buffer.srv.bufferType != BufferViewType::Unknown &&
            buffer.srv.bufferType != BufferViewType::AccelerationStructure
        );
        buffer.srv.range.stride = inStride;
        return *this;
    }

    /// @brief Sets the number of elements exposed by the view (`0` exposes the full buffer from offset 0).
    /// @param[in] inNumElements Element count.
    /// @return Reference to `*this`.
    Initializer& setNumElements(const gp::UInt32 inNumElements) noexcept
    {
        GP_CHECK(
            buffer.srv.bufferType != BufferViewType::Unknown &&
            buffer.srv.bufferType != BufferViewType::AccelerationStructure
        );
        buffer.srv.range.numElements = inNumElements;
        return *this;
    }

    /// @brief Binds a ray tracing acceleration structure scene pointer (valid only for `AccelerationStructure`).
    /// @param[in] inRayTracingScene Target `RayTracingScene` pointer.
    /// @return Reference to `*this`.
    Initializer& setRayTracingScene(resource::RayTracingScene* const inRayTracingScene) noexcept
    {
        GP_CHECK(buffer.srv.bufferType == BufferViewType::AccelerationStructure);
        buffer.srv.rayTracingScene = inRayTracingScene;
        return *this;
    }

    /// @brief Exposes the underlying packed `ViewDesc` for RHI view creation and inspection.
    /// @return Immutable reference to `*this` as `ViewDesc`.
    [[nodiscard]] GP_FORCEINLINE_HINT const ViewDesc& asViewDesc() const noexcept
    {
        return *this;
    }
};

/// @brief Type-safe fluent builder for `ViewDesc` buffer unordered access views.
struct ViewDesc::BufferUAV::Initializer : private ViewDesc
{
    friend struct ViewDesc;
    friend class View;
    friend class UnorderedAccessView;
    friend class ::gp::rhi::DynamicRHI;
    friend class ::gp::rhi::command::CommandListBase;

protected:
    Initializer() noexcept
        : ViewDesc(ViewType::BufferUAV)
    {}

public:
    /// @brief Sets the buffer view interpretation mode (`Typed`, `Structured`, `AccelerationStructure`, or `Raw`).
    /// @param[in] type Non-unknown `BufferViewType`.
    /// @return Reference to `*this`.
    Initializer& setType(const BufferViewType type) noexcept
    {
        GP_CHECK(type != BufferViewType::Unknown);
        buffer.uav.bufferType = type;
        return *this;
    }

    /// @brief Infers `BufferViewType` from `targetBuffer`'s usage flags.
    /// @param[in] targetBuffer Non-null, non-null-resource target GPU buffer.
    /// @return Reference to `*this`.
    Initializer& setTypeFromBuffer(resource::Buffer* const targetBuffer) noexcept
    {
        GP_CHECK(targetBuffer != nullptr);
        GP_CHECKF(
            !targetBuffer->getDesc().isNull(),
            "Cannot infer buffer view type from a null placeholder buffer; call setType() explicitly."
        );

        const resource::BufferUsageFlags usage = targetBuffer->getDesc().usage;
        if ((usage & resource::BufferUsageFlags::ByteAddressBuffer) != resource::BufferUsageFlags::None)
        {
            buffer.uav.bufferType = BufferViewType::Raw;
        }
        else if ((usage & resource::BufferUsageFlags::StructuredBuffer) != resource::BufferUsageFlags::None)
        {
            buffer.uav.bufferType = BufferViewType::Structured;
        }
        else if ((usage & resource::BufferUsageFlags::AccelerationStructure) != resource::BufferUsageFlags::None)
        {
            buffer.uav.bufferType = BufferViewType::AccelerationStructure;
        }
        else
        {
            buffer.uav.bufferType = BufferViewType::Typed;
        }
        return *this;
    }

    /// @brief Sets the typed buffer pixel format.
    /// @param[in] inFormat `PixelFormat` for typed buffer UAVs.
    /// @return Reference to `*this`.
    Initializer& setFormat(const PixelFormat inFormat) noexcept
    {
        buffer.uav.format = inFormat;
        return *this;
    }

    /// @brief Sets the starting byte offset into the target buffer.
    /// @param[in] inOffsetBytes Byte offset from buffer start.
    /// @return Reference to `*this`.
    Initializer& setOffsetInBytes(const gp::UInt32 inOffsetBytes) noexcept
    {
        buffer.uav.offsetInBytes = inOffsetBytes;
        return *this;
    }

    /// @brief Overrides the structured buffer element stride in bytes.
    /// @param[in] inStride Element stride in bytes (`0` inherits from the buffer descriptor).
    /// @return Reference to `*this`.
    Initializer& setStride(const gp::UInt32 inStride) noexcept
    {
        buffer.uav.range.stride = inStride;
        return *this;
    }

    /// @brief Sets the number of elements exposed by the view (`0` exposes the full buffer from offset 0).
    /// @param[in] inNumElements Element count.
    /// @return Reference to `*this`.
    Initializer& setNumElements(const gp::UInt32 inNumElements) noexcept
    {
        buffer.uav.range.numElements = inNumElements;
        return *this;
    }

    /// @brief Enables or disables an implicit hardware atomic counter on this buffer UAV.
    /// @param[in] inAtomicCounter True to allocate an atomic counter alongside the UAV.
    /// @return Reference to `*this`.
    Initializer& setAtomicCounter(const bool inAtomicCounter) noexcept
    {
        buffer.uav.atomicCounter = inAtomicCounter ? 1u : 0u;
        return *this;
    }

    /// @brief Enables or disables append/consume buffer semantics on this buffer UAV.
    /// @param[in] inAppendBuffer True to enable append/consume buffer access.
    /// @return Reference to `*this`.
    Initializer& setAppendBuffer(const bool inAppendBuffer) noexcept
    {
        buffer.uav.appendBuffer = inAppendBuffer ? 1u : 0u;
        return *this;
    }

    /// @brief Exposes the underlying packed `ViewDesc` for RHI view creation and inspection.
    /// @return Immutable reference to `*this` as `ViewDesc`.
    [[nodiscard]] GP_FORCEINLINE_HINT const ViewDesc& asViewDesc() const noexcept
    {
        return *this;
    }
};

/// @brief Type-safe fluent builder for `ViewDesc` texture shader resource views.
struct ViewDesc::TextureSRV::Initializer : private ViewDesc
{
    friend struct ViewDesc;
    friend class View;
    friend class ShaderResourceView;
    friend class ::gp::rhi::DynamicRHI;
    friend class ::gp::rhi::command::CommandListBase;

protected:
    Initializer() noexcept
        : ViewDesc(ViewType::TextureSRV)
    {}

public:
    /// @brief Sets the target texture view dimensionality.
    /// @param[in] inDimension Source `resource::TextureDimension`.
    /// @return Reference to `*this`.
    Initializer& setDimension(const resource::TextureDimension inDimension) noexcept
    {
        switch (inDimension)
        {
        case resource::TextureDimension::Texture2D:
            texture.srv.dimension = TextureViewDimension::Texture2D;
            break;

        case resource::TextureDimension::Texture2DArray:
            texture.srv.dimension = TextureViewDimension::Texture2DArray;
            break;

        case resource::TextureDimension::Texture3D:
            texture.srv.dimension = TextureViewDimension::Texture3D;
            break;

        case resource::TextureDimension::TextureCube:
            texture.srv.dimension = TextureViewDimension::TextureCube;
            break;

        case resource::TextureDimension::TextureCubeArray:
            texture.srv.dimension = TextureViewDimension::TextureCubeArray;
            break;

        default:
            GP_CHECKF(false, "Unsupported TextureDimension passed to TextureSRV::Initializer::setDimension.");
            break;
        }
        return *this;
    }

    /// @brief Copies the view dimensionality directly from `targetTexture`'s descriptor.
    /// @param[in] targetTexture Non-null target GPU texture.
    /// @return Reference to `*this`.
    Initializer& setDimensionFromTexture(resource::Texture* const targetTexture) noexcept
    {
        GP_CHECK(targetTexture != nullptr);
        return setDimension(targetTexture->getDesc().dimension);
    }

    /// @brief Overrides the pixel format used to read the texture (`PixelFormat::Unknown` inherits texture format).
    /// @param[in] inFormat View pixel format.
    /// @return Reference to `*this`.
    Initializer& setFormat(const PixelFormat inFormat) noexcept
    {
        texture.srv.format = inFormat;
        return *this;
    }

    /// @brief Selects the texture data or metadata plane to view.
    /// @param[in] inPlane `resource::TexturePlane` selector.
    /// @return Reference to `*this`.
    Initializer& setPlane(const resource::TexturePlane inPlane) noexcept
    {
        texture.srv.plane = inPlane;
        return *this;
    }

    /// @brief Sets the inclusive mip slice range (`inNumMips == 0` selects all mips).
    /// @param[in] inFirstMip First mip level exposed by the view.
    /// @param[in] inNumMips Number of mip levels exposed (`0` for all).
    /// @return Reference to `*this`.
    Initializer& setMipRange(const gp::UInt8 inFirstMip, const gp::UInt8 inNumMips) noexcept
    {
        texture.srv.mipRange.first = inFirstMip;
        texture.srv.mipRange.num = inNumMips;
        return *this;
    }

    /// @brief Sets the inclusive array slice range (`inNumElements == 0` selects all slices).
    /// @param[in] inFirstElement First array slice (or cube index for cube views).
    /// @param[in] inNumElements Number of slices (or cubes; `0` for all).
    /// @return Reference to `*this`.
    Initializer& setArrayRange(const gp::UInt16 inFirstElement, const gp::UInt16 inNumElements) noexcept
    {
        texture.srv.arrayRange.first = inFirstElement;
        texture.srv.arrayRange.num = inNumElements;
        return *this;
    }

    /// @brief Forces linear sampling even when the underlying texture was created with `TextureCreateFlags::SRGB`.
    /// @param[in] inDisableSRGB True to disable sRGB decode; false to respect the texture's SRGB flag.
    /// @return Reference to `*this`.
    Initializer& setDisableSRGB(const bool inDisableSRGB) noexcept
    {
        texture.srv.disableSRGB = inDisableSRGB ? 1u : 0u;
        return *this;
    }

    /// @brief Exposes the underlying packed `ViewDesc` for RHI view creation and inspection.
    /// @return Immutable reference to `*this` as `ViewDesc`.
    [[nodiscard]] GP_FORCEINLINE_HINT const ViewDesc& asViewDesc() const noexcept
    {
        return *this;
    }
};

/// @brief Type-safe fluent builder for `ViewDesc` texture unordered access views.
struct ViewDesc::TextureUAV::Initializer : private ViewDesc
{
    friend struct ViewDesc;
    friend class View;
    friend class UnorderedAccessView;
    friend class ::gp::rhi::DynamicRHI;
    friend class ::gp::rhi::command::CommandListBase;

protected:
    Initializer() noexcept
        : ViewDesc(ViewType::TextureUAV)
    {
        // Texture UAVs always address a single mip level.
        texture.uav.mipRange.num = 1u;
    }

public:
    /// @brief Sets the target texture view dimensionality.
    /// @param[in] inDimension Source `resource::TextureDimension`.
    /// @return Reference to `*this`.
    Initializer& setDimension(const resource::TextureDimension inDimension) noexcept
    {
        switch (inDimension)
        {
        case resource::TextureDimension::Texture2D:
            texture.uav.dimension = TextureViewDimension::Texture2D;
            break;

        case resource::TextureDimension::Texture2DArray:
            texture.uav.dimension = TextureViewDimension::Texture2DArray;
            break;

        case resource::TextureDimension::Texture3D:
            texture.uav.dimension = TextureViewDimension::Texture3D;
            break;

        case resource::TextureDimension::TextureCube:
            texture.uav.dimension = TextureViewDimension::TextureCube;
            break;

        case resource::TextureDimension::TextureCubeArray:
            texture.uav.dimension = TextureViewDimension::TextureCubeArray;
            break;

        default:
            GP_CHECKF(false, "Unsupported TextureDimension passed to TextureUAV::Initializer::setDimension.");
            break;
        }
        return *this;
    }

    /// @brief Copies the view dimensionality directly from `targetTexture`'s descriptor.
    /// @param[in] targetTexture Non-null target GPU texture.
    /// @return Reference to `*this`.
    Initializer& setDimensionFromTexture(resource::Texture* const targetTexture) noexcept
    {
        GP_CHECK(targetTexture != nullptr);
        return setDimension(targetTexture->getDesc().dimension);
    }

    /// @brief Overrides the pixel format used to read and write the texture (`PixelFormat::Unknown` inherits texture
    /// format).
    /// @param[in] inFormat View pixel format.
    /// @return Reference to `*this`.
    Initializer& setFormat(const PixelFormat inFormat) noexcept
    {
        texture.uav.format = inFormat;
        return *this;
    }

    /// @brief Selects the texture data or metadata plane to view.
    /// @param[in] inPlane `resource::TexturePlane` selector.
    /// @return Reference to `*this`.
    Initializer& setPlane(const resource::TexturePlane inPlane) noexcept
    {
        texture.uav.plane = inPlane;
        return *this;
    }

    /// @brief Selects the single mip level bound by this texture UAV.
    /// @param[in] inMipLevel Zero-based mip index.
    /// @return Reference to `*this`.
    Initializer& setMipLevel(const gp::UInt8 inMipLevel) noexcept
    {
        texture.uav.mipRange.first = inMipLevel;
        return *this;
    }

    /// @brief Sets the inclusive array slice range (`inNumElements == 0` selects all slices).
    /// @param[in] inFirstElement First array slice.
    /// @param[in] inNumElements Number of slices (`0` for all).
    /// @return Reference to `*this`.
    Initializer& setArrayRange(const gp::UInt16 inFirstElement, const gp::UInt16 inNumElements) noexcept
    {
        texture.uav.arrayRange.first = inFirstElement;
        texture.uav.arrayRange.num = inNumElements;
        return *this;
    }

    /// @brief Exposes the underlying packed `ViewDesc` for RHI view creation and inspection.
    /// @return Immutable reference to `*this` as `ViewDesc`.
    [[nodiscard]] GP_FORCEINLINE_HINT const ViewDesc& asViewDesc() const noexcept
    {
        return *this;
    }
};

inline ViewDesc::BufferSRV::Initializer ViewDesc::createBufferSRV() noexcept
{
    return BufferSRV::Initializer();
}

inline ViewDesc::BufferUAV::Initializer ViewDesc::createBufferUAV() noexcept
{
    return BufferUAV::Initializer();
}

inline ViewDesc::TextureSRV::Initializer ViewDesc::createTextureSRV() noexcept
{
    return TextureSRV::Initializer();
}

inline ViewDesc::TextureUAV::Initializer ViewDesc::createTextureUAV() noexcept
{
    return TextureUAV::Initializer();
}

/// @brief Backend-ready resolved parameters for a buffer view derived from `ViewDesc` and a target `Buffer`.
struct ViewDesc::Buffer::ViewInfo
{
    /// @brief Byte offset from the start of the buffer.
    gp::UInt32 offsetInBytes{ 0u };

    /// @brief Resolved element stride in bytes (`BlockBytes` for typed, `4` for raw, `1` for acceleration structures).
    gp::UInt32 strideInBytes{ 0u };

    /// @brief Resolved element count covered by the view.
    gp::UInt32 numElements{ 0u };

    /// @brief Total view span in bytes (`numElements * strideInBytes`).
    gp::UInt32 sizeInBytes{ 0u };

    /// @brief Resolved buffer view interpretation mode.
    BufferViewType bufferType{ BufferViewType::Unknown };

    /// @brief Resolved pixel format (`PixelFormat::Unknown` for non-typed buffer views).
    PixelFormat format{ PixelFormat::Unknown };

    /// @brief True when the target buffer is a null placeholder resource (`BufferUsageFlags::NullResource`).
    bool isNullView{ false };
};

/// @brief Backend-ready resolved parameters for a buffer SRV.
struct ViewDesc::BufferSRV::ViewInfo : public ViewDesc::Buffer::ViewInfo
{};

/// @brief Backend-ready resolved parameters for a buffer UAV.
struct ViewDesc::BufferUAV::ViewInfo : public ViewDesc::Buffer::ViewInfo
{
    /// @brief True when the UAV allocates a hardware atomic counter.
    bool atomicCounter{ false };

    /// @brief True when the UAV uses append/consume counter semantics.
    bool appendBuffer{ false };
};

/// @brief Backend-ready resolved parameters for a texture view derived from `ViewDesc` and a target `Texture`.
struct ViewDesc::Texture::ViewInfo
{
    /// @brief Resolved array slice range (`first`, `num`).
    resource::Range16 arrayRange{};

    /// @brief Resolved texture data or metadata plane.
    resource::TexturePlane plane{ resource::TexturePlane::Primary };

    /// @brief Resolved pixel format for the view.
    PixelFormat format{ PixelFormat::Unknown };

    /// @brief Resolved texture view dimensionality.
    TextureViewDimension dimension : kTextureViewDimensionNumBits{ TextureViewDimension::Unknown };

    /// @brief True when the view covers every mip level of the target texture.
    gp::UInt8 allMips : 1 { 0u };

    /// @brief True when the view covers every array slice of the target texture.
    gp::UInt8 allSlices : 1 { 0u };
};

/// @brief Backend-ready resolved parameters for a texture SRV.
struct ViewDesc::TextureSRV::ViewInfo : public ViewDesc::Texture::ViewInfo
{
    /// @brief Resolved inclusive mip range (`first`, `num`).
    resource::Range8 mipRange{};

    /// @brief True when hardware sRGB decode should be enabled for this SRV.
    gp::UInt8 isSRGB : 1 { 0u };
};

/// @brief Backend-ready resolved parameters for a texture UAV.
struct ViewDesc::TextureUAV::ViewInfo : public ViewDesc::Texture::ViewInfo
{
    /// @brief Single mip level bound by this UAV.
    gp::UInt8 mipLevel{ 0u };
};

/// @brief Maps a `ViewDesc` to its corresponding bindless `DescriptorType`.
/// @param[in] viewDesc View descriptor to inspect.
/// @return Matching `DescriptorType` enumerator.
[[nodiscard]] inline constexpr DescriptorType descriptorTypeFromViewDesc(const ViewDesc& viewDesc) noexcept
{
    return descriptorTypeFromViewType(viewDesc.common.viewType);
}

}   // namespace gp::rhi::view
