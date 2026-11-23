// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "core/ViewableResource.hpp"

namespace gp::rhi::resource
{

ViewableResource::ViewableResource(
    const ResourceType resourceType,
    const sync::ResourceAccess initialAccess,
    const gp::StringView debugName,
    const gp::Name ownerName
) noexcept
    : Resource(resourceType)
    , m_trackedAccess(initialAccess)
{
#if GP_RHI_ENABLE_RESOURCE_INFO
    if (!debugName.isEmpty())
    {
        m_name = gp::Name(debugName);
    }
    setOwnerName(ownerName);
#else
    static_cast<void>(debugName);
    static_cast<void>(ownerName);
#endif
}

ViewableResource::~ViewableResource() noexcept = default;

void ViewableResource::setTrackedAccessFromContext(const sync::TrackedAccess trackedAccess) noexcept
{
    m_trackedAccess = trackedAccess;
}

void ViewableResource::takeOwnership(ViewableResource& other) noexcept
{
    m_trackedAccess = other.m_trackedAccess;
}

void ViewableResource::releaseOwnership() noexcept
{
    m_trackedAccess = sync::TrackedAccess(sync::ResourceAccess::Unknown);
}

}   // namespace gp::rhi::resource
