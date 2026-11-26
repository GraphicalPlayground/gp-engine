// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "resources/VulkanTextureReference.hpp"
#include "commands/CommandListBase.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "memory/pointers/RefCountedPtr.hpp"
#include "resources/Texture.hpp"
#include "resources/TextureReference.hpp"
#include <utility>

namespace gp::rhi::vulkan
{

VulkanTextureReference::VulkanTextureReference(resource::Texture* const referencedTexture) noexcept
    : resource::TextureReference(referencedTexture)
{}

VulkanTextureReference::~VulkanTextureReference() noexcept = default;

void VulkanTextureReference::updateReferencedTexture(resource::Texture* const newTexture) noexcept
{
    setReferencedTexture(newTexture);
}

resource::TextureReferenceRef
    createVulkanTextureReference(command::CommandListBase& cmdList, resource::Texture* const referencedTexture) noexcept
{
    static_cast<void>(cmdList);
    return resource::TextureReferenceRef(new VulkanTextureReference(referencedTexture));
}

void updateVulkanTextureReference(
    command::CommandListBase& cmdList,
    resource::TextureReference* const textureReference,
    resource::Texture* const newTexture
) noexcept
{
    if (textureReference == nullptr)
    {
        return;
    }

    VulkanTextureReference* const vulkanReference = static_cast<VulkanTextureReference*>(textureReference);
    if (cmdList.isBottomOfPipe())
    {
        vulkanReference->updateReferencedTexture(newTexture);
        return;
    }

    gp::RefCountedPtr<VulkanTextureReference> heldReference(vulkanReference);
    resource::TextureRef heldTexture(newTexture);
    cmdList.enqueueLambda([heldReference = std::move(heldReference),
                           heldTexture = std::move(heldTexture)](command::CommandListBase& executingCmdList) noexcept
    {
        static_cast<void>(executingCmdList);
        heldReference->updateReferencedTexture(heldTexture.get());
    });
}

}   // namespace gp::rhi::vulkan
