// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "resources/VulkanUniformBufferData.hpp"
#include "core/Resource.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "diagnostics/AssertionMacros.hpp"
#include "logging/LogMacros.hpp"
#include "memory/Memory.hpp"
#include "pipeline/SamplerState.hpp"
#include "resources/Texture.hpp"
#include "resources/UniformBufferLayout.hpp"
#include "views/ShaderResourceView.hpp"
#include "views/UnorderedAccessView.hpp"

namespace gp::rhi::vulkan
{

namespace
{

/// @brief Reads a pointer of type `InPointeeType*` stored at `member.memberOffset` inside `contents`.
/// @tparam InPointeeType Pointee type stored in the parameter struct.
/// @param[in] contents Shader parameter struct contents.
/// @param[in] member Layout entry describing the member offset.
/// @return The stored pointer (may be null).
template <typename InPointeeType>
[[nodiscard]] InPointeeType*
    readMemberPointer(const void* const contents, const resource::UniformBufferResource& member) noexcept
{
    InPointeeType* pointer = nullptr;
    gp::memory::copyMemory(
        static_cast<void*>(&pointer),
        static_cast<const gp::UInt8*>(contents) + member.memberOffset,
        sizeof(InPointeeType*)
    );
    return pointer;
}

}   // namespace

bool isVulkanUniformBufferResourceMember(const shader::UniformBufferBaseType memberType) noexcept
{
    switch (memberType)
    {
    case shader::UniformBufferBaseType::Texture:
    case shader::UniformBufferBaseType::SRV:
    case shader::UniformBufferBaseType::UAV:
    case shader::UniformBufferBaseType::Sampler:
    case shader::UniformBufferBaseType::UniformBlock:
        return true;
    default:
        return false;
    }
}

resource::Resource*
    getVulkanUniformBufferResource(const void* const contents, const resource::UniformBufferResource& member) noexcept
{
    if (contents == nullptr)
    {
        return nullptr;
    }

    switch (member.memberType)
    {
    case shader::UniformBufferBaseType::Texture:
        return readMemberPointer<resource::Texture>(contents, member);
    case shader::UniformBufferBaseType::SRV:
    case shader::UniformBufferBaseType::UniformBlock:
        return readMemberPointer<view::ShaderResourceView>(contents, member);
    case shader::UniformBufferBaseType::UAV:
        return readMemberPointer<view::UnorderedAccessView>(contents, member);
    case shader::UniformBufferBaseType::Sampler:
        return readMemberPointer<pipeline::SamplerState>(contents, member);
    default:
        return nullptr;
    }
}

view::DescriptorHandle getVulkanUniformBufferBindlessHandle(
    const void* const contents, const resource::UniformBufferResource& member
) noexcept
{
    GP_CHECK(contents != nullptr);

    switch (member.memberType)
    {
    case shader::UniformBufferBaseType::Texture:
    {
        const resource::Texture* const texture = readMemberPointer<resource::Texture>(contents, member);
        return (texture != nullptr) ? texture->getDefaultBindlessHandle() : view::DescriptorHandle{};
    }
    case shader::UniformBufferBaseType::SRV:
    case shader::UniformBufferBaseType::UniformBlock:
    {
        const view::ShaderResourceView* const srv = readMemberPointer<view::ShaderResourceView>(contents, member);
        return (srv != nullptr) ? srv->getBindlessHandle() : view::DescriptorHandle{};
    }
    case shader::UniformBufferBaseType::UAV:
    {
        const view::UnorderedAccessView* const uav = readMemberPointer<view::UnorderedAccessView>(contents, member);
        return (uav != nullptr) ? uav->getBindlessHandle() : view::DescriptorHandle{};
    }
    case shader::UniformBufferBaseType::Sampler:
    {
        const pipeline::SamplerState* const sampler = readMemberPointer<pipeline::SamplerState>(contents, member);
        return (sampler != nullptr) ? sampler->getBindlessHandle() : view::DescriptorHandle{};
    }
    default:
        return view::DescriptorHandle{};
    }
}

void updateVulkanUniformBufferConstants(
    void* const destination,
    const void* const source,
    const resource::UniformBufferLayout& layout,
    const bool allowBindless
) noexcept
{
    GP_CHECK(destination != nullptr);
    GP_CHECK(source != nullptr);

    // Copy the whole constant block first, then overwrite each resource slot with its bindless index.
    gp::memory::copyMemory(destination, source, layout.getConstantBufferSize());

    if (!allowBindless)
    {
        return;
    }

    gp::UInt8* const destinationBytes = static_cast<gp::UInt8*>(destination);
    const gp::ConstVectorView<resource::UniformBufferResource> members = layout.getResources();
    for (gp::USize index = 0u; index < members.size(); ++index)
    {
        const resource::UniformBufferResource& member = members[index];
        const view::DescriptorHandle handle = getVulkanUniformBufferBindlessHandle(source, member);
        if (!handle.isValid())
        {
            continue;
        }

        GP_CHECK((static_cast<gp::USize>(member.memberOffset) + sizeof(gp::UInt32)) <= layout.getConstantBufferSize());
        const gp::UInt32 bindlessIndex = handle.getIndex();
        gp::memory::copyMemory(destinationBytes + member.memberOffset, &bindlessIndex, sizeof(bindlessIndex));
    }
}

bool validateVulkanUniformBufferResources(
    const resource::UniformBufferLayout& layout, const void* const contents
) noexcept
{
    const gp::ConstVectorView<resource::UniformBufferResource> members = layout.getResources();
    bool isValid = true;
    for (gp::USize index = 0u; index < members.size(); ++index)
    {
        const resource::UniformBufferResource& member = members[index];
        if (!isVulkanUniformBufferResourceMember(member.memberType))
        {
            continue;
        }

        if (getVulkanUniformBufferResource(contents, member) == nullptr)
        {
            GP_LOG_ERROR(
                LogVulkanRHI,
                "Uniform buffer '{}' has a null resource at index {} (offset {}).",
                layout.getDebugName(),
                index,
                member.memberOffset
            );
            isValid = false;
        }
    }
    return isValid;
}

}   // namespace gp::rhi::vulkan
