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

/// @brief Read-write unordered access view (UAV) bound to a GPU buffer or texture.
class GP_RHI_API UnorderedAccessView : public View
{
public:
    UnorderedAccessView() = delete;

    /// @brief Constructs an unordered access view over `targetResource` described by `viewDesc`.
    /// @param[in] targetResource Non-null target `ViewableResource`.
    /// @param[in] viewDesc Immutable UAV `ViewDesc` (asserts `viewDesc.isUAV()`).
    explicit UnorderedAccessView(resource::ViewableResource* targetResource, const ViewDesc& viewDesc) noexcept;

    UnorderedAccessView(const UnorderedAccessView&) = delete;
    UnorderedAccessView& operator=(const UnorderedAccessView&) = delete;
    UnorderedAccessView(UnorderedAccessView&&) = delete;
    UnorderedAccessView& operator=(UnorderedAccessView&&) = delete;

protected:
    /// @copydoc View::~View()
    virtual ~UnorderedAccessView() noexcept override;
};

}   // namespace gp::rhi::view
