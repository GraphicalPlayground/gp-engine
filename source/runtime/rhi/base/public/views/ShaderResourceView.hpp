// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "core/ResourceForward.hpp"
#include "RHIBase.hpp"
#include "views/View.hpp"
#include "views/ViewDesc.hpp"
#include "views/ViewForward.hpp"   // IWYU pragma: keep

namespace gp::rhi::view
{

/// @brief Read-only shader resource view (SRV) bound to a GPU buffer or texture.
class GP_RHI_API ShaderResourceView : public View
{
public:
    ShaderResourceView() = delete;

    /// @brief Constructs a shader resource view over `targetResource` described by `viewDesc`.
    /// @param[in] targetResource Non-null target `ViewableResource`.
    /// @param[in] viewDesc Immutable SRV `ViewDesc` (asserts `viewDesc.isSRV()`).
    explicit ShaderResourceView(resource::ViewableResource* targetResource, const ViewDesc& viewDesc) noexcept;

    ShaderResourceView(const ShaderResourceView&) = delete;
    ShaderResourceView& operator=(const ShaderResourceView&) = delete;
    ShaderResourceView(ShaderResourceView&&) = delete;
    ShaderResourceView& operator=(ShaderResourceView&&) = delete;

protected:
    /// @copydoc View::~View()
    virtual ~ShaderResourceView() noexcept override;
};

}   // namespace gp::rhi::view
