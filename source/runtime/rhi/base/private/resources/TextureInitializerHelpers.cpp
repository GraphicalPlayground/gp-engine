// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "resources/TextureInitializerHelpers.hpp"
#include "core/ResourceTypes.hpp"
#include "diagnostics/AssertionMacros.hpp"
#include "logging/LogMacros.hpp"
#include "memory/GlobalMemory.hpp"
#include "memory/Memory.hpp"
#include "resources/TextureEnums.hpp"
#include "resources/TextureSubresourceHelpers.hpp"
#include "resources/TextureUtils.hpp"
#include "RHIDefinitions.hpp"

namespace gp::rhi::resource
{

LockTextureInitializer createLockTextureInitializer(
    command::CommandListBase& cmdList, Texture* const texture, const TextureCreateDesc& createDesc
) noexcept
{
    const gp::UInt64 writableMemorySize = calculateTextureSize(createDesc);
    void* writableMemory = nullptr;
    if (writableMemorySize > 0u)
    {
        gp::memory::Malloc* const allocator = gp::memory::getGlobalMalloc();
        GP_CHECKF(allocator != nullptr, "Global engine allocator must be valid for createLockTextureInitializer().");
        writableMemory = allocator->allocate(static_cast<gp::USize>(writableMemorySize), 16u);
    }

    return LockTextureInitializer(cmdList, texture, writableMemory, writableMemorySize);
}

void fillTextureInitializerFromDataProvider(
    TextureInitializer& initializer,
    const TextureDesc& desc,
    const TextureDataProvider& provider,
    const gp::Int32 firstMipIndex
) noexcept
{
    enumerateSubresources(
        desc,
        firstMipIndex,
        [&initializer, &desc, &provider](
            const TextureSubresourceIndex providerIndex, const gp::UInt32 /*combinedArrayIndex*/
        ) noexcept -> void
    {
        TextureSubresourceIndex subresourceIndex{};
        subresourceIndex.mipIndex = providerIndex.mipIndex;
        subresourceIndex.arrayIndex = providerIndex.arrayIndex;
        subresourceIndex.faceIndex = providerIndex.faceIndex;

        const TextureSubresourceInitializer dest = initializer.getWritableSubresource(subresourceIndex);

        const TextureDataProvider::SubresourceData source = provider.requester(providerIndex);
        if (source.data != nullptr)
        {
            const IntVector mipExtent = calculateMipExtent(desc, static_cast<gp::UInt32>(providerIndex.mipIndex));
            copyDataProviderSubresource(
                source,
                dest.writableData,
                dest.size,
                static_cast<gp::UInt32>(dest.stride),
                static_cast<gp::UInt32>(mipExtent.y),
                static_cast<gp::UInt32>(mipExtent.z),
                desc.format
            );
        }
        else if (dest.writableData != nullptr && dest.size > 0u)
        {
            // Ensure the entire staged buffer is initialized before handing it to the backend.
            gp::memory::zeroMemory(dest.writableData, static_cast<gp::USize>(dest.size));
        }

        if (provider.discarder.isBound())
        {
            provider.discarder(providerIndex);
        }
    }
    );
}

TextureInitializer handleUnknownTextureInitializerInitAction(
    command::CommandListBase& cmdList, const TextureCreateDesc& createDesc
) noexcept
{
    GP_LOG_FATAL(LogRHI, "Unknown or unhandled TextureInitAction: {}", static_cast<gp::UInt32>(createDesc.initAction));

    Texture* const texture = nullptr;
    return DefaultTextureInitializer(cmdList, texture);
}

}   // namespace gp::rhi::resource
