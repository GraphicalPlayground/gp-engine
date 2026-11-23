// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "core/ResourceForward.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "device/DeviceForward.hpp"
#include "memory/pointers/RefCountedPtr.hpp"   // IWYU pragma: keep
#include "resources/Texture.hpp"
#include "resources/TextureDesc.hpp"
#include "RHIBase.hpp"
#include "sync/TransitionDesc.hpp"
#include "views/DescriptorHandle.hpp"

namespace gp::rhi::resource
{

/// @brief Indirection wrapper forwarding texture queries and state updates to a swappable target `Texture`.
/// @details Used by streaming textures so materials, uniform buffers, and bindless slots hold one stable
///          `TextureReference` handle while the underlying mip-chain `Texture` is asynchronously replaced. If
///          constructed or updated with `nullptr`, falls back to the globally registered fallback from
///          `getDefaultTexture()`.
class GP_RHI_API TextureReference : public Texture
{
    friend class ::gp::rhi::DynamicRHI;

public:
    TextureReference() = delete;

    /// @brief Constructs a texture reference initially pointing to `referencedTexture` (or `getDefaultTexture()`).
    /// @param[in] referencedTexture Initial target texture, or `nullptr` to fall back to `getDefaultTexture()`.
    explicit TextureReference(Texture* referencedTexture) noexcept;

    /// @brief Constructs a bindless-capable texture reference with a dedicated bindless descriptor slot.
    /// @param[in] referencedTexture Initial target texture, or `nullptr` to fall back to `getDefaultTexture()`.
    /// @param[in] bindlessHandle Dedicated bindless descriptor handle representing this indirection slot.
    TextureReference(Texture* referencedTexture, view::DescriptorHandle bindlessHandle) noexcept;

    TextureReference(const TextureReference&) = delete;
    TextureReference& operator=(const TextureReference&) = delete;
    TextureReference(TextureReference&&) = delete;
    TextureReference& operator=(TextureReference&&) = delete;

protected:
    /// @copydoc Texture::~Texture()
    virtual ~TextureReference() noexcept override;

public:
    /// @copydoc Texture::getDesc()
    [[nodiscard]] virtual const TextureDesc& getDesc() const noexcept override;

    /// @copydoc Texture::getTextureReference()
    [[nodiscard]] virtual TextureReference* getTextureReference() noexcept override;

    /// @copydoc Texture::getDefaultBindlessHandle()
    [[nodiscard]] virtual view::DescriptorHandle getDefaultBindlessHandle() const noexcept override;

    /// @copydoc Texture::getNativeResource()
    [[nodiscard]] virtual void* getNativeResource() const noexcept override;

    /// @copydoc Texture::getNativeShaderResourceView()
    [[nodiscard]] virtual void* getNativeShaderResourceView() const noexcept override;

    /// @copydoc Texture::getTextureBaseRHI()
    [[nodiscard]] virtual void* getTextureBaseRHI() noexcept override;

    /// @copydoc Texture::getWriteMaskProperties()
    virtual void getWriteMaskProperties(void*& outData, gp::UInt32& outSize) noexcept override;

public:
    /// @brief Returns the target `Texture` currently referenced by this indirection wrapper.
    /// @return Non-owning pointer to the active target `Texture`.
    [[nodiscard]] GP_FORCEINLINE_HINT Texture* getReferencedTexture() const noexcept
    {
        return m_referencedTexture.get();
    }

    /// @brief Returns the dedicated bindless descriptor handle stored on this reference.
    /// @return Dedicated `view::DescriptorHandle` (invalid if non-bindless).
    [[nodiscard]] GP_FORCEINLINE_HINT view::DescriptorHandle getBindlessHandle() const noexcept
    {
        return m_bindlessHandle;
    }

    /// @brief Reports whether this texture reference owns a valid dedicated bindless descriptor slot.
    /// @return True when `getBindlessHandle().isValid()` is true.
    [[nodiscard]] GP_FORCEINLINE_HINT bool isBindless() const noexcept
    {
        return m_bindlessHandle.isValid();
    }

    /// @brief Returns the global fallback texture used when a `TextureReference` targets `nullptr`.
    /// @return Pointer to the registered fallback `Texture`, or `nullptr` if uninitialized.
    [[nodiscard]] static Texture* getDefaultTexture() noexcept;

    /// @brief Registers or clears the global fallback texture used by `TextureReference`.
    /// @param[in] defaultTexture Fallback `Texture` instance (retained by reference count), or `nullptr` on shutdown.
    static void setDefaultTexture(Texture* defaultTexture) noexcept;

protected:
    /// @copydoc ViewableResource::setTrackedAccessFromContext()
    virtual void setTrackedAccessFromContext(sync::TrackedAccess trackedAccess) noexcept override;

    /// @brief Swaps the underlying target texture to `newTexture` (falling back to `getDefaultTexture()` if null).
    /// @param[in] newTexture New target texture, or `nullptr` to fall back to the default texture.
    void setReferencedTexture(Texture* newTexture) noexcept;

protected:
    /// @brief Strong reference keeping the active target texture alive.
    TextureRef m_referencedTexture{};

    /// @brief Optional immutable bindless descriptor slot owned by this texture reference.
    const view::DescriptorHandle m_bindlessHandle{};
};

}   // namespace gp::rhi::resource
