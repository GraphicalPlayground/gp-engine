// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "resources/VulkanResourceReplace.hpp"
#include "allocators/PoolMoveRequest.hpp"
#include "commands/CommandListBase.hpp"
#include "containers/arrays/Vector.hpp"
#include "CoreMinimal.hpp"
#include "diagnostics/AssertionMacros.hpp"
#include "resources/TextureInitializer.hpp"
#include "resources/VulkanBuffer.hpp"
#include <utility>

namespace gp::rhi::vulkan
{

void replaceVulkanResources(
    command::CommandListBase& cmdList, const gp::ConstVectorView<resource::ResourceReplaceInfo> replaceInfos
) noexcept
{
    if (replaceInfos.isEmpty())
    {
        return;
    }

    // Copy the infos: the lambda may run after the caller's array is gone.
    gp::Vector<resource::ResourceReplaceInfo> infos{};
    infos.reserve(replaceInfos.size());
    for (const resource::ResourceReplaceInfo& info: replaceInfos)
    {
        infos.pushBack(info);
    }

    cmdList.enqueueLambdaMultiPipe([infos = std::move(infos)](const allocator::ContextArray& contexts) noexcept
    {
        for (const resource::ResourceReplaceInfo& info: infos)
        {
            switch (info.storageType)
            {
            case resource::ResourceReplaceInfo::StorageType::Buffer:
            {
                GP_CHECK(info.dstBuffer != nullptr);
                VulkanBuffer* const dstBuffer = static_cast<VulkanBuffer*>(info.dstBuffer);
                VulkanBuffer* const srcBuffer = static_cast<VulkanBuffer*>(info.srcBuffer);

                if (srcBuffer != nullptr)
                {
                    // Views of the source would be left pointing at memory it no longer owns.
                    GP_CHECKF(!srcBuffer->hasLinkedViews(), "The source buffer of a replace must have no views.");
                    dstBuffer->takeOwnership(*srcBuffer);
                }
                else
                {
                    dstBuffer->releaseOwnership();
                }

                dstBuffer->updateLinkedViews(contexts);
                break;
            }
            case resource::ResourceReplaceInfo::StorageType::RTGeometry:
            {
                GP_CHECKF(false, "Replacing ray tracing geometry is not supported by the Vulkan backend yet.");
                break;
            }
            default:
            {
                GP_CHECK_NO_ENTRY();
                break;
            }
            }
        }
    });
}

}   // namespace gp::rhi::vulkan

