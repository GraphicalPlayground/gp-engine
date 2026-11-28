// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "resources/BufferInitializerHelpers.hpp"
#include "core/ResourceTypes.hpp"
#include "diagnostics/AssertionMacros.hpp"
#include "logging/LogMacros.hpp"
#include "memory/Memory.hpp"
#include "resources/BufferEnums.hpp"
#include "RHIDefinitions.hpp"

namespace gp::rhi::resource
{

BufferInitializer handleUnknownBufferInitializerInitAction(
    command::CommandListBase& cmdList, const BufferCreateDesc& createDesc
) noexcept
{
    GP_LOG_FATAL(LogRHI, "Unknown or unhandled BufferInitAction: {}", static_cast<gp::UInt32>(createDesc.initAction));

    InvalidBuffer* const buffer = new InvalidBuffer(createDesc);
    return DefaultBufferInitializer(cmdList, buffer);
}

BufferInitializer createUnifiedMemoryBufferInitializer(
    command::CommandListBase& cmdList,
    const BufferCreateDesc& createDesc,
    Buffer* const buffer,
    void* const writableData
) noexcept
{
    if (createDesc.initAction == BufferInitAction::Default)
    {
        return DefaultBufferInitializer(cmdList, buffer);
    }

    if (createDesc.initAction == BufferInitAction::ResourceArray)
    {
        GP_CHECK(createDesc.initialData != nullptr);

        const gp::UInt32 copySize = createDesc.initialData->getResourceDataSize();
        const void* const srcData = createDesc.initialData->getResourceData();
        if (copySize > 0u && writableData != nullptr && srcData != nullptr)
        {
            gp::memory::copyMemory(writableData, srcData, static_cast<gp::USize>(copySize));
        }

        createDesc.initialData->discard();

        return DefaultBufferInitializer(cmdList, buffer);
    }

    if (createDesc.initAction == BufferInitAction::Zeroed)
    {
        if (createDesc.size > 0u && writableData != nullptr)
        {
            gp::memory::zeroMemory(writableData, static_cast<gp::USize>(createDesc.size));
        }
        return DefaultBufferInitializer(cmdList, buffer);
    }

    if (createDesc.initAction == BufferInitAction::Initializer)
    {
        return DefaultBufferInitializer(cmdList, buffer, writableData, static_cast<gp::UInt64>(createDesc.size));
    }

    return handleUnknownBufferInitializerInitAction(cmdList, createDesc);
}

}   // namespace gp::rhi::resource
