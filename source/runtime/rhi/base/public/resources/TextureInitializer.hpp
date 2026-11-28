// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "commands/CommandForward.hpp"
#include "core/ResourceEnums.hpp"
#include "core/ResourceForward.hpp"   // IWYU pragma: keep
#include "core/ResourceTypes.hpp"
#include "CoreMinimal.hpp"
#include "diagnostics/AssertionMacros.hpp"
#include "memory/Memory.hpp"
#include "memory/pointers/RefCountedPtr.hpp"   // IWYU pragma: keep
#include "resources/ResourceReplace.hpp"       // IWYU pragma: export
#include "resources/Texture.hpp"
#include "resources/TextureCreateDesc.hpp"
#include "resources/TextureEnums.hpp"
#include "resources/TextureMemoryStats.hpp"   // IWYU pragma: export
#include "RHIBase.hpp"
#include "templates/Function.hpp"
#include <utility>

namespace gp::rhi::resource
{

/// @brief Writable CPU slice returned by `TextureInitializer::getWritableSubresource()`.
struct TextureSubresourceInitializer
{
    /// @brief CPU-writable byte pointer for the requested subresource.
    void* writableData{ nullptr };

    /// @brief Row pitch in bytes for the requested subresource.
    gp::UInt64 stride{ 0u };

    /// @brief Total writable byte size of the requested subresource.
    gp::UInt64 size{ 0u };

    /// @brief Reports whether this subresource view holds a writable pointer.
    /// @return True when `writableData != nullptr` and `size > 0`.
    [[nodiscard]] GP_FORCEINLINE_HINT bool isValid() const noexcept
    {
        return (writableData != nullptr) && (size > 0u);
    }

    /// @brief Copies `numBytes` from `srcData` into this subresource starting at `dstOffset`.
    /// @param[in] srcData Source byte buffer.
    /// @param[in] numBytes Number of bytes to copy.
    /// @param[in] dstOffset Optional destination byte offset within this subresource.
    void writeData(const void* const srcData, const gp::UInt64 numBytes, const gp::UInt64 dstOffset = 0u) const noexcept
    {
        if (numBytes == 0u)
        {
            return;
        }
        GP_CHECKF(writableData != nullptr, "TextureSubresourceInitializer has no writable data pointer.");
        GP_CHECKF(srcData != nullptr, "TextureSubresourceInitializer::writeData requires non-null source data.");
        GP_CHECKF(
            dstOffset <= size && numBytes <= (size - dstOffset),
            "TextureSubresourceInitializer::writeData range exceeds subresource size."
        );
        gp::memory::copyMemory(
            static_cast<gp::UInt8*>(writableData) + dstOffset, srcData, static_cast<gp::USize>(numBytes)
        );
    }
};

/// @brief Move-only RAII handle granting subresource write access during GPU texture creation.
/// @details Returned by `DynamicRHI::createTextureInitializer()` and `CommandListBase::createTextureInitializer()`.
///          Auto-finalizes on destruction if `finalize()` has not yet been called.
struct GP_RHI_API TextureInitializer
{
public:
    /// @brief Backend callback resolving a writable subresource by `TextureSubresourceIndex`.
    using SubresourceCallback = gp::UniqueFunction<TextureSubresourceInitializer(TextureSubresourceIndex)>;

    /// @brief Backend callback committing the texture upload and returning the finalized `TextureRef`.
    using FinalizeCallback = gp::UniqueFunction<TextureRef(command::CommandListBase&)>;

public:
    /// @brief Command list driving texture creation and upload.
    command::CommandListBase* cmdList{ nullptr };

    /// @brief Newly created texture handle (valid until `finalize()`).
    TextureRef texture{};

    /// @brief Optional CPU-writable backing staging buffer owned by the backend or initializer.
    void* writableData{ nullptr };

    /// @brief Byte capacity of `writableData`, or `0` when unused.
    gp::UInt64 writableSize{ 0u };

    /// @brief Subresource lookup callback supplied by the backend.
    SubresourceCallback subresourceCallback{};

    /// @brief Finalization callback supplied by the backend.
    FinalizeCallback finalizeCallback{};

public:
    /// @brief Constructs an empty, already-finalized texture initializer.
    TextureInitializer() noexcept = default;

    /// @brief Constructs an active texture initializer without a direct writable staging buffer pointer.
    /// @param[in] inCmdList Command list driving initialization.
    /// @param[in] inTexture Newly created texture reference.
    /// @param[in] inSubresourceCallback Backend subresource lookup callback.
    /// @param[in] inFinalizeCallback Backend finalization callback.
    TextureInitializer(
        command::CommandListBase& inCmdList,
        TextureRef inTexture,
        SubresourceCallback inSubresourceCallback,
        FinalizeCallback inFinalizeCallback
    ) noexcept
        : cmdList(&inCmdList)
        , texture(std::move(inTexture))
        , subresourceCallback(std::move(inSubresourceCallback))
        , finalizeCallback(std::move(inFinalizeCallback))
    {}

    /// @brief Constructs an active texture initializer with an associated writable staging buffer span.
    /// @param[in] inCmdList Command list driving initialization.
    /// @param[in] inTexture Newly created texture reference.
    /// @param[in] inWritableData CPU-writable backing buffer pointer, or `nullptr`.
    /// @param[in] inWritableSize Byte capacity of `inWritableData`.
    /// @param[in] inSubresourceCallback Backend subresource lookup callback.
    /// @param[in] inFinalizeCallback Backend finalization callback.
    TextureInitializer(
        command::CommandListBase& inCmdList,
        TextureRef inTexture,
        void* const inWritableData,
        const gp::UInt64 inWritableSize,
        SubresourceCallback inSubresourceCallback,
        FinalizeCallback inFinalizeCallback
    ) noexcept
        : cmdList(&inCmdList)
        , texture(std::move(inTexture))
        , writableData(inWritableData)
        , writableSize(inWritableSize)
        , subresourceCallback(std::move(inSubresourceCallback))
        , finalizeCallback(std::move(inFinalizeCallback))
    {}

    TextureInitializer(const TextureInitializer&) = delete;
    TextureInitializer& operator=(const TextureInitializer&) = delete;

    /// @brief Move constructor transferring pending initialization ownership.
    /// @param[in,out] other Source initializer left empty.
    TextureInitializer(TextureInitializer&& other) noexcept
        : cmdList(other.cmdList)
        , texture(std::move(other.texture))
        , writableData(other.writableData)
        , writableSize(other.writableSize)
        , subresourceCallback(std::move(other.subresourceCallback))
        , finalizeCallback(std::move(other.finalizeCallback))
    {
        other.cmdList = nullptr;
        other.writableData = nullptr;
        other.writableSize = 0u;
    }

    /// @brief Move assignment finalizing any active texture before adopting `other`.
    /// @param[in,out] other Source initializer left empty.
    /// @return Reference to `*this`.
    TextureInitializer& operator=(TextureInitializer&& other) noexcept
    {
        if (this != &other)
        {
            if (isValid())
            {
                static_cast<void>(finalize());
            }
            cmdList = other.cmdList;
            texture = std::move(other.texture);
            writableData = other.writableData;
            writableSize = other.writableSize;
            subresourceCallback = std::move(other.subresourceCallback);
            finalizeCallback = std::move(other.finalizeCallback);
            other.cmdList = nullptr;
            other.writableData = nullptr;
            other.writableSize = 0u;
        }
        return *this;
    }

    /// @brief Automatically finalizes any unfinalized texture on destruction.
    ~TextureInitializer() noexcept
    {
        if (isValid())
        {
            static_cast<void>(finalize());
        }
    }

    /// @brief Reports whether this initializer still owns an unfinalized texture.
    /// @return True when `cmdList != nullptr` and `texture.isValid()`.
    [[nodiscard]] GP_FORCEINLINE_HINT bool isValid() const noexcept
    {
        return (cmdList != nullptr) && texture.isValid();
    }

    /// @brief Queries a CPU-writable region for the subresource identified by `index`.
    /// @param[in] index Full `(faceIndex, arrayIndex, mipIndex)` subresource coordinates.
    /// @return `TextureSubresourceInitializer` describing the writable subresource.
    [[nodiscard]] TextureSubresourceInitializer
        getWritableSubresource(const TextureSubresourceIndex index) const noexcept
    {
        if (!subresourceCallback.isBound())
        {
            return TextureSubresourceInitializer{};
        }
        return subresourceCallback(index);
    }

    /// @brief Queries a CPU-writable region for subresource `(mipIndex, arrayIndex)` on face `0`.
    /// @param[in] mipIndex Zero-based mip level.
    /// @param[in] arrayIndex Zero-based array slice (or cube face for single cubemaps).
    /// @return `TextureSubresourceInitializer` describing the writable subresource.
    [[nodiscard]] TextureSubresourceInitializer
        getWritableSubresource(const gp::UInt32 mipIndex, const gp::UInt32 arrayIndex = 0u) const noexcept
    {
        if (texture.isValid() && texture->getDesc().isTextureCube() && texture->getDesc().arraySize == 1u)
        {
            return getWritableSubresource(
                TextureSubresourceIndex{ static_cast<gp::Int32>(arrayIndex), 0, static_cast<gp::Int32>(mipIndex) }
            );
        }
        return getWritableSubresource(
            TextureSubresourceIndex{ 0, static_cast<gp::Int32>(arrayIndex), static_cast<gp::Int32>(mipIndex) }
        );
    }

    /// @brief Queries a writable subresource for a 2D texture at `mipIndex`.
    /// @param[in] mipIndex Zero-based mip level.
    /// @return Writable subresource slice.
    [[nodiscard]] TextureSubresourceInitializer getTexture2DSubresource(const gp::UInt32 mipIndex) const noexcept
    {
        return getWritableSubresource(TextureSubresourceIndex{ 0, 0, static_cast<gp::Int32>(mipIndex) });
    }

    /// @brief Queries a writable subresource for a 2D texture array slice at `(arrayIndex, mipIndex)`.
    /// @param[in] arrayIndex Zero-based array slice index.
    /// @param[in] mipIndex Zero-based mip level.
    /// @return Writable subresource slice.
    [[nodiscard]] TextureSubresourceInitializer
        getTexture2DArraySubresource(const gp::UInt32 arrayIndex, const gp::UInt32 mipIndex) const noexcept
    {
        return getWritableSubresource(
            TextureSubresourceIndex{ 0, static_cast<gp::Int32>(arrayIndex), static_cast<gp::Int32>(mipIndex) }
        );
    }

    /// @brief Queries a writable subresource for a 3D volume texture at `mipIndex`.
    /// @param[in] mipIndex Zero-based mip level.
    /// @return Writable subresource slice.
    [[nodiscard]] TextureSubresourceInitializer getTexture3DSubresource(const gp::UInt32 mipIndex) const noexcept
    {
        return getWritableSubresource(TextureSubresourceIndex{ 0, 0, static_cast<gp::Int32>(mipIndex) });
    }

    /// @brief Queries a writable subresource for a cubemap face at `(faceIndex, mipIndex)`.
    /// @param[in] faceIndex Zero-based cube face index in `[0, 5]`.
    /// @param[in] mipIndex Zero-based mip level.
    /// @return Writable subresource slice.
    [[nodiscard]] TextureSubresourceInitializer
        getTextureCubeSubresource(const gp::UInt32 faceIndex, const gp::UInt32 mipIndex) const noexcept
    {
        return getWritableSubresource(
            TextureSubresourceIndex{ static_cast<gp::Int32>(faceIndex), 0, static_cast<gp::Int32>(mipIndex) }
        );
    }

    /// @brief Queries a writable subresource for a cubemap array slice at `(faceIndex, arrayIndex, mipIndex)`.
    /// @param[in] faceIndex Zero-based cube face index in `[0, 5]`.
    /// @param[in] arrayIndex Zero-based cubemap array index.
    /// @param[in] mipIndex Zero-based mip level.
    /// @return Writable subresource slice.
    [[nodiscard]] TextureSubresourceInitializer getTextureCubeArraySubresource(
        const gp::UInt32 faceIndex, const gp::UInt32 arrayIndex, const gp::UInt32 mipIndex
    ) const noexcept
    {
        return getWritableSubresource(
            TextureSubresourceIndex{
              static_cast<gp::Int32>(faceIndex), static_cast<gp::Int32>(arrayIndex), static_cast<gp::Int32>(mipIndex) }
        );
    }

    /// @brief Commits the initial texture upload and transfers ownership of the finalized `TextureRef`.
    /// @return Finalized `TextureRef` (or null if already finalized).
    [[nodiscard]] TextureRef finalize() noexcept
    {
        if (!isValid())
        {
            return TextureRef{};
        }

        command::CommandListBase* const activeCmdList = cmdList;
        FinalizeCallback callback = std::move(finalizeCallback);
        TextureRef localTexture = std::move(texture);

        cmdList = nullptr;
        writableData = nullptr;
        writableSize = 0u;
        subresourceCallback = SubresourceCallback{};

        if (callback.isBound())
        {
            return callback(*activeCmdList);
        }
        return localTexture;
    }
};

/// @brief Platform-aligned GPU allocation footprint returned by `DynamicRHI::calcTexturePlatformSize()`.
struct CalcTextureSizeResult
{
    /// @brief Total GPU memory footprint in bytes.
    gp::UInt64 size{ 0u };

    /// @brief Required GPU allocation alignment in bytes.
    gp::UInt32 align{ 0u };

    /// @brief Default memberwise equality comparison.
    /// @param[in] rhs Result to compare against.
    /// @return True when size and alignment match.
    [[nodiscard]] constexpr bool operator==(const CalcTextureSizeResult& rhs) const noexcept = default;
};

/// @brief Parameters identifying a texture subresource region to lock via `DynamicRHI::lockTexture()`.
struct TextureLockArgs
{
    /// @brief Target texture resource to lock.
    Texture* texture{ nullptr };

    /// @brief Zero-based mip level to lock.
    gp::UInt32 mipIndex{ 0u };

    /// @brief Zero-based array slice index (or cubemap index for cube arrays).
    gp::UInt32 arrayIndex{ 0u };

    /// @brief Cubemap face selector (`CubeFace::PosX` for non-cube textures).
    CubeFace faceIndex{ CubeFace::PosX };

    /// @brief CPU lock access mode (`ReadOnly`, `WriteOnly`, `WriteOnlyNoOverwrite`).
    ResourceLockMode lockMode{ ResourceLockMode::ReadOnly };

    /// @brief When true, locks within internal mip bounds without triggering driver stalls if supported.
    bool lockWithinMiptail{ false };

    /// @brief When true, allows backend read-only lock to stall on pending GPU writes.
    bool needsRHIFlush{ true };

    /// @brief Builds lock arguments for a 2D texture mip level.
    /// @param[in] inTexture Target 2D texture.
    /// @param[in] inMipIndex Mip level index.
    /// @param[in] inLockMode CPU lock access mode.
    /// @param[in] inLockWithinMiptail Miptail lock hint.
    /// @param[in] inNeedsRHIFlush Whether to flush pending RHI commands.
    /// @return Populated `TextureLockArgs`.
    [[nodiscard]] static constexpr TextureLockArgs lock2D(
        Texture* const inTexture,
        const gp::UInt32 inMipIndex,
        const ResourceLockMode inLockMode,
        const bool inLockWithinMiptail = false,
        const bool inNeedsRHIFlush = true
    ) noexcept
    {
        TextureLockArgs args{};
        args.texture = inTexture;
        args.mipIndex = inMipIndex;
        args.lockMode = inLockMode;
        args.lockWithinMiptail = inLockWithinMiptail;
        args.needsRHIFlush = inNeedsRHIFlush;
        return args;
    }

    /// @brief Builds lock arguments for a 2D texture array slice and mip level.
    /// @param[in] inTexture Target 2D array texture.
    /// @param[in] inArrayIndex Array slice index.
    /// @param[in] inMipIndex Mip level index.
    /// @param[in] inLockMode CPU lock access mode.
    /// @param[in] inLockWithinMiptail Miptail lock hint.
    /// @param[in] inNeedsRHIFlush Whether to flush pending RHI commands.
    /// @return Populated `TextureLockArgs`.
    [[nodiscard]] static constexpr TextureLockArgs lock2DArray(
        Texture* const inTexture,
        const gp::UInt32 inArrayIndex,
        const gp::UInt32 inMipIndex,
        const ResourceLockMode inLockMode,
        const bool inLockWithinMiptail = false,
        const bool inNeedsRHIFlush = true
    ) noexcept
    {
        TextureLockArgs args{};
        args.texture = inTexture;
        args.arrayIndex = inArrayIndex;
        args.mipIndex = inMipIndex;
        args.lockMode = inLockMode;
        args.lockWithinMiptail = inLockWithinMiptail;
        args.needsRHIFlush = inNeedsRHIFlush;
        return args;
    }

    /// @brief Builds lock arguments for a cubemap or cubemap array face and mip level.
    /// @param[in] inTexture Target cube texture.
    /// @param[in] inFaceIndex Cubemap face selector.
    /// @param[in] inArrayIndex Cubemap array index (`0` for single cubemaps).
    /// @param[in] inMipIndex Mip level index.
    /// @param[in] inLockMode CPU lock access mode.
    /// @param[in] inLockWithinMiptail Miptail lock hint.
    /// @param[in] inNeedsRHIFlush Whether to flush pending RHI commands.
    /// @return Populated `TextureLockArgs`.
    [[nodiscard]] static constexpr TextureLockArgs lockCubeFace(
        Texture* const inTexture,
        const CubeFace inFaceIndex,
        const gp::UInt32 inArrayIndex,
        const gp::UInt32 inMipIndex,
        const ResourceLockMode inLockMode,
        const bool inLockWithinMiptail = false,
        const bool inNeedsRHIFlush = true
    ) noexcept
    {
        TextureLockArgs args{};
        args.texture = inTexture;
        args.faceIndex = inFaceIndex;
        args.arrayIndex = inArrayIndex;
        args.mipIndex = inMipIndex;
        args.lockMode = inLockMode;
        args.lockWithinMiptail = inLockWithinMiptail;
        args.needsRHIFlush = inNeedsRHIFlush;
        return args;
    }
};

/// @brief Mapped CPU memory pointer and pitch returned by `DynamicRHI::lockTexture()`.
struct TextureLockResult
{
    /// @brief Mapped CPU pointer to the locked subresource data.
    void* data{ nullptr };

    /// @brief Total locked byte size.
    gp::UInt64 byteCount{ 0u };

    /// @brief Row pitch in bytes.
    gp::UInt32 stride{ 0u };

    /// @brief Reports whether this lock result holds a valid mapped pointer and non-zero byte count.
    /// @return True when `data != nullptr` and `byteCount > 0`.
    [[nodiscard]] GP_FORCEINLINE_HINT bool isValid() const noexcept
    {
        return (data != nullptr) && (byteCount > 0u);
    }
};

/// @brief 2D sub-rectangle specification for `DynamicRHI::updateTexture2D()`.
struct UpdateTextureRegion2D
{
    /// @brief Destination X offset in texels.
    gp::UInt32 destX{ 0u };

    /// @brief Destination Y offset in texels.
    gp::UInt32 destY{ 0u };

    /// @brief Source X offset in texels.
    gp::Int32 srcX{ 0 };

    /// @brief Source Y offset in texels.
    gp::Int32 srcY{ 0 };

    /// @brief Width of the updated region in texels.
    gp::UInt32 width{ 0u };

    /// @brief Height of the updated region in texels.
    gp::UInt32 height{ 0u };

    /// @brief Constructs an empty update region.
    constexpr UpdateTextureRegion2D() noexcept = default;

    /// @brief Constructs a 2D texture update region.
    /// @param[in] inDestX Destination X offset.
    /// @param[in] inDestY Destination Y offset.
    /// @param[in] inSrcX Source X offset.
    /// @param[in] inSrcY Source Y offset.
    /// @param[in] inWidth Region width in texels.
    /// @param[in] inHeight Region height in texels.
    constexpr UpdateTextureRegion2D(
        const gp::UInt32 inDestX,
        const gp::UInt32 inDestY,
        const gp::Int32 inSrcX,
        const gp::Int32 inSrcY,
        const gp::UInt32 inWidth,
        const gp::UInt32 inHeight
    ) noexcept
        : destX(inDestX)
        , destY(inDestY)
        , srcX(inSrcX)
        , srcY(inSrcY)
        , width(inWidth)
        , height(inHeight)
    {}

    /// @brief Default memberwise equality comparison.
    /// @param[in] rhs Region to compare against.
    /// @return True when all fields match.
    [[nodiscard]] constexpr bool operator==(const UpdateTextureRegion2D& rhs) const noexcept = default;
};

/// @brief 3D sub-box specification for `DynamicRHI::updateTexture3D()`.
struct UpdateTextureRegion3D
{
    /// @brief Destination X offset in texels.
    gp::UInt32 destX{ 0u };

    /// @brief Destination Y offset in texels.
    gp::UInt32 destY{ 0u };

    /// @brief Destination Z offset in texels.
    gp::UInt32 destZ{ 0u };

    /// @brief Source X offset in texels.
    gp::Int32 srcX{ 0 };

    /// @brief Source Y offset in texels.
    gp::Int32 srcY{ 0 };

    /// @brief Source Z offset in texels.
    gp::Int32 srcZ{ 0 };

    /// @brief Width of the updated region in texels.
    gp::UInt32 width{ 0u };

    /// @brief Height of the updated region in texels.
    gp::UInt32 height{ 0u };

    /// @brief Depth of the updated region in texels.
    gp::UInt32 depth{ 0u };

    /// @brief Constructs an empty 3D update region.
    constexpr UpdateTextureRegion3D() noexcept = default;

    /// @brief Constructs a 3D texture update region from scalar coordinates.
    /// @param[in] inDestX Destination X offset.
    /// @param[in] inDestY Destination Y offset.
    /// @param[in] inDestZ Destination Z offset.
    /// @param[in] inSrcX Source X offset.
    /// @param[in] inSrcY Source Y offset.
    /// @param[in] inSrcZ Source Z offset.
    /// @param[in] inWidth Region width.
    /// @param[in] inHeight Region height.
    /// @param[in] inDepth Region depth.
    constexpr UpdateTextureRegion3D(
        const gp::UInt32 inDestX,
        const gp::UInt32 inDestY,
        const gp::UInt32 inDestZ,
        const gp::Int32 inSrcX,
        const gp::Int32 inSrcY,
        const gp::Int32 inSrcZ,
        const gp::UInt32 inWidth,
        const gp::UInt32 inHeight,
        const gp::UInt32 inDepth
    ) noexcept
        : destX(inDestX)
        , destY(inDestY)
        , destZ(inDestZ)
        , srcX(inSrcX)
        , srcY(inSrcY)
        , srcZ(inSrcZ)
        , width(inWidth)
        , height(inHeight)
        , depth(inDepth)
    {}

    /// @brief Constructs a 3D texture update region from `IntVector` coordinates.
    /// @param[in] inDest Destination origin `(x, y, z)`.
    /// @param[in] inSrc Source origin `(x, y, z)`.
    /// @param[in] inSize Region extent `(w, h, d)`.
    constexpr UpdateTextureRegion3D(const IntVector inDest, const IntVector inSrc, const IntVector inSize) noexcept
        : destX(static_cast<gp::UInt32>(inDest.x))
        , destY(static_cast<gp::UInt32>(inDest.y))
        , destZ(static_cast<gp::UInt32>(inDest.z))
        , srcX(inSrc.x)
        , srcY(inSrc.y)
        , srcZ(inSrc.z)
        , width(static_cast<gp::UInt32>(inSize.x))
        , height(static_cast<gp::UInt32>(inSize.y))
        , depth(static_cast<gp::UInt32>(inSize.z))
    {}

    /// @brief Default memberwise equality comparison.
    /// @param[in] rhs Region to compare against.
    /// @return True when all fields match.
    [[nodiscard]] constexpr bool operator==(const UpdateTextureRegion3D& rhs) const noexcept = default;
};

/// @brief Staged CPU upload allocation paired with `beginUpdateTexture3D()` / `endUpdateTexture3D()`.
struct UpdateTexture3DData
{
    /// @brief Target 3D volume texture.
    Texture* texture{ nullptr };

    /// @brief Target mip level.
    gp::UInt32 mipIndex{ 0u };

    /// @brief 3D region being updated.
    UpdateTextureRegion3D updateRegion{};

    /// @brief Row pitch in bytes of `data`.
    gp::UInt32 rowPitch{ 0u };

    /// @brief Slice (depth) pitch in bytes of `data`.
    gp::UInt32 depthPitch{ 0u };

    /// @brief Allocated CPU staging buffer written by the caller.
    gp::UInt8* data{ nullptr };

    /// @brief Total allocated byte size of `data`.
    gp::UInt64 dataSizeBytes{ 0u };

    /// @brief Frame number captured at `beginUpdateTexture3D()` for backend lifetime validation.
    gp::UInt32 frameNumber{ 0u };

    /// @brief True when `data` was allocated by `beginUpdateTexture3D()` and must be freed by `endUpdateTexture3D()`.
    bool isManualFree{ false };

    /// @brief Opaque inline scratch bytes reserved for concrete backends to carry staging allocation handles.
    gp::UInt8 platformData[64]{};
};

/// @brief Options controlling cubemap face, mip level, array slice, and GPU index for surface readback.
struct ReadSurfaceDataFlags
{
    /// @brief Target cubemap face (`CubeFace::PosX` for 2D textures).
    CubeFace cubeFace{ CubeFace::PosX };

    /// @brief Target mip level index.
    gp::Int32 mipIndex{ 0 };

    /// @brief Target array slice index.
    gp::Int32 arrayIndex{ 0 };

    /// @brief Target GPU node index in multi-GPU configurations.
    gp::UInt32 gpuIndex{ 0u };

    /// @brief True to keep linear-to-gamma conversion enabled when reading 8-bit color surfaces.
    bool linearToGamma{ true };

    /// @brief True to output the stencil plane instead of depth when reading back a depth-stencil texture.
    bool outputStencil{ false };

    /// @brief Constructs default readback flags for mip 0, slice 0, GPU 0.
    constexpr ReadSurfaceDataFlags() noexcept = default;

    /// @brief Constructs readback flags targeting a specific cubemap face and mip level.
    /// @param[in] inCubeFace Target cubemap face.
    /// @param[in] inMipIndex Target mip level.
    constexpr ReadSurfaceDataFlags(const CubeFace inCubeFace, const gp::Int32 inMipIndex = 0) noexcept
        : cubeFace(inCubeFace)
        , mipIndex(inMipIndex)
    {}
};

}   // namespace gp::rhi::resource
