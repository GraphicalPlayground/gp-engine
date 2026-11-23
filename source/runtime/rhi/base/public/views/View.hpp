// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "core/Resource.hpp"
#include "core/ResourceEnums.hpp"
#include "core/ResourceForward.hpp"
#include "core/ViewableResource.hpp"
#include "CoreMinimal.hpp"
#include "memory/pointers/RefCountedPtr.hpp"   // IWYU pragma: keep
#include "RHIBase.hpp"
#include "views/DescriptorHandle.hpp"
#include "views/ViewDesc.hpp"
#include "views/ViewForward.hpp"   // IWYU pragma: keep

namespace gp::rhi::view
{

/// @brief Base reference-counted view retaining a target `ViewableResource` and its immutable `ViewDesc`.
/// @details Keeps the underlying buffer or texture alive for as long as the view exists so deferred deletion releases
///          resources only after every referencing view has been destroyed.
class GP_RHI_API View : public resource::Resource
{
public:
    View() = delete;

    /// @brief Constructs a view binding `targetResource` under `viewDesc`.
    /// @param[in] resourceType Concrete `ResourceType` (`ShaderResourceView` or `UnorderedAccessView`).
    /// @param[in] targetResource Non-null target `ViewableResource` retained for the lifetime of this view.
    /// @param[in] viewDesc Immutable 16-byte view descriptor.
    View(
        resource::ResourceType resourceType, resource::ViewableResource* targetResource, const ViewDesc& viewDesc
    ) noexcept;

    View(const View&) = delete;
    View& operator=(const View&) = delete;
    View(View&&) = delete;
    View& operator=(View&&) = delete;

protected:
    /// @copydoc resource::Resource::~Resource()
    virtual ~View() noexcept override;

public:
    /// @brief Returns the bindless descriptor handle for this view when bindless rendering is enabled.
    /// @details Base implementation returns an invalid `DescriptorHandle`.
    /// @return Bindless `DescriptorHandle`, or invalid if unsupported.
    [[nodiscard]] virtual DescriptorHandle getBindlessHandle() const noexcept;

    /// @brief Returns the underlying `ViewableResource` retained by this view.
    /// @return Non-owning pointer to the target `ViewableResource`.
    [[nodiscard]] GP_FORCEINLINE_HINT resource::ViewableResource* getResource() const noexcept
    {
        return m_resource.get();
    }

    /// @brief Returns the underlying target resource cast to `resource::Buffer*`.
    /// @note Asserts that `isBuffer()` is true.
    /// @return Non-owning pointer to the target `resource::Buffer`.
    [[nodiscard]] GP_FORCEINLINE_HINT resource::Buffer* getBuffer() const noexcept
    {
        GP_CHECK(m_viewDesc.isBuffer());
        return static_cast<resource::Buffer*>(m_resource.get());
    }

    /// @brief Returns the underlying target resource cast to `resource::Texture*`.
    /// @note Asserts that `isTexture()` is true.
    /// @return Non-owning pointer to the target `resource::Texture`.
    [[nodiscard]] GP_FORCEINLINE_HINT resource::Texture* getTexture() const noexcept
    {
        GP_CHECK(m_viewDesc.isTexture());
        return static_cast<resource::Texture*>(m_resource.get());
    }

    /// @brief Reports whether this view targets a GPU buffer.
    /// @return True when `m_viewDesc.isBuffer()` is true.
    [[nodiscard]] GP_FORCEINLINE_HINT bool isBuffer() const noexcept
    {
        return m_viewDesc.isBuffer();
    }

    /// @brief Reports whether this view targets a GPU texture.
    /// @return True when `m_viewDesc.isTexture()` is true.
    [[nodiscard]] GP_FORCEINLINE_HINT bool isTexture() const noexcept
    {
        return m_viewDesc.isTexture();
    }

    /// @brief Returns the immutable `ViewDesc` used to create this view.
    /// @return Immutable reference to `ViewDesc`.
    [[nodiscard]] GP_FORCEINLINE_HINT const ViewDesc& getDesc() const noexcept
    {
        return m_viewDesc;
    }

private:
    /// @brief Strong reference keeping the underlying buffer or texture alive.
    resource::ViewableResourceRef m_resource{};

protected:
    /// @brief Immutable 16-byte descriptor capturing the view configuration.
    const ViewDesc m_viewDesc{};
};

}   // namespace gp::rhi::view
