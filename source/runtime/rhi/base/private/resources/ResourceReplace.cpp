// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "resources/ResourceReplace.hpp"
#include "commands/CommandListBase.hpp"
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep

namespace gp::rhi::resource
{

ResourceReplaceBatcher::ResourceReplaceBatcher(
    command::CommandListBase& cmdList, const gp::UInt32 initialCapacity
) noexcept
    : m_cmdList(cmdList)
{
    if (initialCapacity > 0u)
    {
        m_infos.reserve(static_cast<gp::USize>(initialCapacity));
    }
}

ResourceReplaceBatcher::~ResourceReplaceBatcher() noexcept
{
    flush();
}

void ResourceReplaceBatcher::flush() noexcept
{
    if (!m_infos.isEmpty())
    {
        m_cmdList.replaceResources(gp::ConstVectorView<ResourceReplaceInfo>(m_infos.data(), m_infos.size()));
        m_infos.clear();
    }
}

}   // namespace gp::rhi::resource
