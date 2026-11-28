// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "commands/CommandListBase.hpp"
#include "commands/CommandListImmediate.hpp"
#include "core/ResourceEnums.hpp"
#include "core/ResourceForward.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "diagnostics/AssertionMacros.hpp"
#include "memory/Memory.hpp"
#include "resources/InitializerScopedMemory.hpp"
#include "resources/Texture.hpp"
#include "resources/TextureCreateDesc.hpp"
#include "resources/TextureEnums.hpp"
#include "resources/TextureInitializer.hpp"
#include "resources/TextureUtils.hpp"
#include "RHIBase.hpp"
#include <utility>

namespace gp::rhi::resource
{

/// @brief Base helper subclass of `TextureInitializer` exposing writable staging buffer accessors to backends.
struct BaseTextureInitializer : public TextureInitializer
{
public:
    /// @brief Constructs a texture initializer with staging buffer metadata and both callbacks.
    /// @param[in] inCmdList Command list driving initialization.
    /// @param[in] inTexture Newly created texture pointer.
    /// @param[in] inWritableData CPU-writable backing buffer pointer, or `nullptr`.
    /// @param[in] inWritableSize Byte capacity of `inWritableData`.
    /// @param[in,out] inFinalizeCallback Backend finalization callback.
    /// @param[in,out] inGetSubresourceCallback Backend subresource lookup callback.
    BaseTextureInitializer(
        command::CommandListBase& inCmdList,
        Texture* const inTexture,
        void* const inWritableData,
        const gp::UInt64 inWritableSize,
        FinalizeCallback&& inFinalizeCallback,
        SubresourceCallback&& inGetSubresourceCallback
    ) noexcept
        : TextureInitializer(
              inCmdList,
              TextureRef(inTexture),
              inWritableData,
              inWritableSize,
              std::move(inGetSubresourceCallback),
              std::move(inFinalizeCallback)
          )
    {}

    /// @brief Constructs a texture initializer without writable staging memory or a subresource callback.
    /// @param[in] inCmdList Command list driving initialization.
    /// @param[in] inTexture Newly created texture pointer.
    /// @param[in,out] inFinalizeCallback Backend finalization callback.
    BaseTextureInitializer(
        command::CommandListBase& inCmdList, Texture* const inTexture, FinalizeCallback&& inFinalizeCallback
    ) noexcept
        : TextureInitializer(
              inCmdList, TextureRef(inTexture), nullptr, 0u, SubresourceCallback{}, std::move(inFinalizeCallback)
          )
    {}

    /// @brief Returns the CPU-writable backing buffer pointer associated with this initializer.
    /// @return Writable staging pointer, or `nullptr`.
    [[nodiscard]] GP_FORCEINLINE_HINT void* getWritableData() const noexcept
    {
        return writableData;
    }

    /// @brief Returns the byte size of the writable staging buffer associated with this initializer.
    /// @return Writable staging size in bytes.
    [[nodiscard]] GP_FORCEINLINE_HINT gp::UInt64 getWritableSize() const noexcept
    {
        return writableSize;
    }
};

/// @brief Texture initializer that returns the created texture on finalize.
struct DefaultTextureInitializer : public BaseTextureInitializer
{
public:
    /// @brief Constructs a no-op texture initializer returning `inTexture` on finalize.
    /// @param[in] inCmdList Command list driving initialization.
    /// @param[in] inTexture Newly created texture pointer (may be `nullptr` on error fallback).
    DefaultTextureInitializer(command::CommandListBase& inCmdList, Texture* const inTexture) noexcept
        : BaseTextureInitializer(
              inCmdList,
              inTexture,
              [heldTexture = TextureRef(inTexture)](command::CommandListBase&) mutable noexcept -> TextureRef
    {
        return std::move(heldTexture);
    }
          )
    {}
};

/// @brief Texture initializer addressing subresources inside a tightly packed contiguous staging buffer.
/// @details Uses `calculateSubresourceOffset()` to compute byte offsets, row strides, and subresource sizes on demand.
struct DefaultLayoutTextureInitializer : public BaseTextureInitializer
{
public:
    /// @brief Constructs a tightly packed layout texture initializer backed by `[inMemory, inMemory + inMemorySize)`.
    /// @param[in] inCmdList Command list driving initialization.
    /// @param[in] inTexture Newly created texture pointer.
    /// @param[in] inMemory Base pointer of the tightly packed staging buffer.
    /// @param[in] inMemorySize Total byte size of `inMemory`.
    /// @param[in,out] inFinalizeFunc Backend finalization callback uploading `inMemory` to the GPU.
    DefaultLayoutTextureInitializer(
        command::CommandListBase& inCmdList,
        Texture* const inTexture,
        void* const inMemory,
        const gp::UInt64 inMemorySize,
        FinalizeCallback&& inFinalizeFunc
    ) noexcept
        : BaseTextureInitializer(
              inCmdList,
              inTexture,
              inMemory,
              inMemorySize,
              std::move(inFinalizeFunc),
              [heldTexture = TextureRef(inTexture),
               stagedData =
                   inMemory](const TextureSubresourceIndex subresourceIndex) noexcept -> TextureSubresourceInitializer
    {
        const TextureDesc& textureDesc = heldTexture->getDesc();

        gp::UInt64 subresourceStride = 0u;
        gp::UInt64 subresourceSize = 0u;
        const gp::UInt64 offset = calculateSubresourceOffset(
            textureDesc,
            static_cast<gp::UInt32>(subresourceIndex.faceIndex),
            static_cast<gp::UInt32>(subresourceIndex.arrayIndex),
            static_cast<gp::UInt32>(subresourceIndex.mipIndex),
            subresourceStride,
            subresourceSize
        );

        TextureSubresourceInitializer result{};
        result.writableData = static_cast<gp::UInt8*>(stagedData) + offset;
        result.stride = subresourceStride;
        result.size = subresourceSize;
        return result;
    }
          )
    {}
};

/// @brief Texture initializer that uploads staged subresources through `CommandListImmediate::lockTexture()` and
///        `unlockTexture()` on finalize.
struct LockTextureInitializer : public DefaultLayoutTextureInitializer
{
public:
    /// @brief Adopts `inMemory` in an `InitializerScopedMemory` and copies every subresource into the texture on
    ///        finalize.
    /// @param[in] inCmdList Immediate command list driving `lockTexture()` / `unlockTexture()`.
    /// @param[in] inTexture Newly created texture pointer.
    /// @param[in] inMemory Heap allocation obtained from `gp::memory::getGlobalMalloc()`, owned and freed by this
    ///                     initializer on finalize or destruction.
    /// @param[in] inMemorySize Byte size of `inMemory`.
    LockTextureInitializer(
        command::CommandListBase& inCmdList,
        Texture* const inTexture,
        void* const inMemory,
        const gp::UInt64 inMemorySize
    ) noexcept
        : DefaultLayoutTextureInitializer(
              inCmdList,
              inTexture,
              inMemory,
              inMemorySize,
              [heldTexture = TextureRef(inTexture), writableMemory = InitializerScopedMemory(inMemory)](
                  command::CommandListBase& execCmdList
              ) mutable noexcept -> TextureRef
    {
        command::CommandListImmediate& immediateCmdList = execCmdList.getAsImmediate();

        const TextureDesc& textureDesc = heldTexture->getDesc();
        const gp::UInt32 faceCount = textureDesc.isTextureCube() ? kCubeFaceCount : 1u;

        for (gp::UInt32 faceIndex = 0u; faceIndex < faceCount; ++faceIndex)
        {
            for (gp::UInt32 arrayIndex = 0u; arrayIndex < static_cast<gp::UInt32>(textureDesc.arraySize); ++arrayIndex)
            {
                for (gp::UInt32 mipIndex = 0u; mipIndex < static_cast<gp::UInt32>(textureDesc.numMips); ++mipIndex)
                {
                    gp::UInt64 subresourceStride = 0u;
                    gp::UInt64 subresourceSize = 0u;
                    const gp::UInt64 offset = calculateSubresourceOffset(
                        textureDesc, faceIndex, arrayIndex, mipIndex, subresourceStride, subresourceSize
                    );

                    const TextureLockArgs lockArgs = TextureLockArgs::lockCubeFace(
                        heldTexture.get(),
                        static_cast<CubeFace>(faceIndex),
                        arrayIndex,
                        mipIndex,
                        ResourceLockMode::WriteOnly,
                        false
                    );

                    const TextureLockResult lockResult = immediateCmdList.lockTexture(lockArgs);
                    GP_CHECK(lockResult.byteCount <= subresourceSize);
                    GP_CHECK(static_cast<gp::UInt64>(lockResult.stride) == subresourceStride);

                    if (lockResult.data != nullptr && writableMemory.pointer != nullptr && subresourceSize > 0u)
                    {
                        gp::memory::copyMemory(
                            lockResult.data,
                            static_cast<const gp::UInt8*>(writableMemory.pointer) + offset,
                            static_cast<gp::USize>(subresourceSize)
                        );
                    }

                    immediateCmdList.unlockTexture(lockArgs);
                }
            }
        }

        return std::move(heldTexture);
    }
          )
    {}
};

/// @brief Allocates a 16-byte aligned staging buffer sized for `createDesc` and returns a `LockTextureInitializer`.
/// @param[in] cmdList Immediate command list driving texture upload on finalize.
/// @param[in] texture Newly created texture pointer.
/// @param[in] createDesc Texture creation descriptor determining total tightly packed byte size.
/// @return Configured `LockTextureInitializer` owning the staging allocation.
[[nodiscard]] GP_RHI_API LockTextureInitializer createLockTextureInitializer(
    command::CommandListBase& cmdList, Texture* texture, const TextureCreateDesc& createDesc
) noexcept;

/// @brief Populates a writable `TextureInitializer` by pulling every subresource from `provider`.
/// @details Uses `initializer.getWritableSubresource()` so callers remain agnostic to each backend's staging layout.
///          Subresources returning `data == nullptr` are zero-filled so no uninitialized bytes are uploaded.
/// @param[in,out] initializer Writable texture initializer to fill.
/// @param[in] desc Texture descriptor defining subresource dimensions and counts.
/// @param[in] provider Callback pair supplying and optionally discarding subresource data.
/// @param[in] firstMipIndex First resident mip level to populate (defaults to `0`).
GP_RHI_API void fillTextureInitializerFromDataProvider(
    TextureInitializer& initializer,
    const TextureDesc& desc,
    const TextureDataProvider& provider,
    gp::Int32 firstMipIndex = 0
) noexcept;

/// @brief Logs and asserts on an unknown or unhandled `TextureInitAction`, returning an empty fallback initializer.
/// @param[in] cmdList Command list driving texture creation.
/// @param[in] createDesc Texture creation descriptor carrying the unhandled `initAction`.
/// @return `TextureInitializer` wrapping a null texture pointer.
[[nodiscard]] GP_RHI_API TextureInitializer handleUnknownTextureInitializerInitAction(
    command::CommandListBase& cmdList, const TextureCreateDesc& createDesc
) noexcept;

}   // namespace gp::rhi::resource
