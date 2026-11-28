// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "commands/CommandForward.hpp"
#include "core/ResourceForward.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"            // IWYU pragma: keep
#include "resources/TextureReference.hpp"
#include "RHIVulkan.hpp"

namespace gp::rhi::vulkan
{

/// @brief Vulkan texture reference: an indirection that shaders bind while the referenced texture is swapped.
/// @details Descriptor binding resolves the referenced texture when a set is written, so retargeting only swaps the
///          held reference. A bindless descriptor would also be rewritten here once bindless descriptors exist.
class GP_RHI_VULKAN_API VulkanTextureReference final : public resource::TextureReference
{
public:
    /// @brief Constructs a reference targeting `referencedTexture`.
    /// @param[in] referencedTexture Initial target, or `nullptr` for the default texture.
    explicit VulkanTextureReference(resource::Texture* referencedTexture) noexcept;

    /// @brief Retargets the reference.
    /// @note Bottom of pipe: call from the RHI timeline (or a bypassed command list).
    /// @param[in] newTexture New target, or `nullptr` for the default texture.
    void updateReferencedTexture(resource::Texture* newTexture) noexcept;

protected:
    /// @brief Releases the held reference.
    virtual ~VulkanTextureReference() noexcept override;
};

/// @brief Creates a texture reference.
/// @note Backs `DynamicRHI::createTextureReference()`.
/// @param[in,out] cmdList Active command list (unused: creation records no GPU work).
/// @param[in] referencedTexture Initial target, or `nullptr` for the default texture.
/// @return Owning reference.
[[nodiscard]] GP_RHI_VULKAN_API resource::TextureReferenceRef
    createVulkanTextureReference(command::CommandListBase& cmdList, resource::Texture* referencedTexture) noexcept;

/// @brief Retargets a texture reference on the RHI timeline.
/// @details Applied immediately on a bottom-of-pipe list; otherwise deferred in a lambda holding both resources.
/// @note Backs `DynamicRHI::updateTextureReference()`.
/// @param[in,out] cmdList Command list ordering the update.
/// @param[in] textureReference Reference to retarget (a `VulkanTextureReference`); `nullptr` is ignored.
/// @param[in] newTexture New target, or `nullptr` for the default texture.
GP_RHI_VULKAN_API void updateVulkanTextureReference(
    command::CommandListBase& cmdList, resource::TextureReference* textureReference, resource::Texture* newTexture
) noexcept;

}   // namespace gp::rhi::vulkan
