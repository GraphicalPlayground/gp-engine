// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "states/VulkanVertexDeclarationKey.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "hashing/Crc32.hpp"
#include "memory/Memory.hpp"

namespace gp::rhi::vulkan
{

namespace
{

/// @brief Strict weak ordering by stream index, then offset, then attribute index.
/// @param[in] lhs Left element.
/// @param[in] rhs Right element.
/// @return True when `lhs` sorts before `rhs`.
[[nodiscard]] constexpr bool
    isElementLess(const pipeline::VertexElement& lhs, const pipeline::VertexElement& rhs) noexcept
{
    if (lhs.streamIndex != rhs.streamIndex)
    {
        return lhs.streamIndex < rhs.streamIndex;
    }
    if (lhs.offset != rhs.offset)
    {
        return lhs.offset < rhs.offset;
    }
    return lhs.attributeIndex < rhs.attributeIndex;
}

}   // namespace

VulkanVertexDeclarationKey::VulkanVertexDeclarationKey(
    const pipeline::VertexDeclarationElementList& inElements
) noexcept
    : elements(inElements)
{
    // `VertexElement` has no padding, so hashing its bytes is deterministic.
    gp::hashing::Crc32 fullHasher;
    fullHasher.update(static_cast<const void*>(elements.data()), elements.size() * sizeof(pipeline::VertexElement));
    hash = fullHasher.finalize();

    pipeline::VertexDeclarationElementList sortedElements = elements;
    sortElements(sortedElements);

    gp::hashing::Crc32 strideLessHasher;
    for (gp::USize index = 0u; index < sortedElements.size(); ++index)
    {
        pipeline::VertexElement element = sortedElements[index];
        element.stride = 0u;
        strideLessHasher.update(static_cast<const void*>(&element), sizeof(element));
    }
    hashNoStrides = strideLessHasher.finalize();
}

bool VulkanVertexDeclarationKey::operator==(const VulkanVertexDeclarationKey& rhs) const noexcept
{
    if (elements.size() != rhs.elements.size())
    {
        return false;
    }
    return gp::memory::compareMemory(
               elements.data(), rhs.elements.data(), elements.size() * sizeof(pipeline::VertexElement)
           ) == 0;
}

void VulkanVertexDeclarationKey::sortElements(pipeline::VertexDeclarationElementList& inOutElements) noexcept
{
    // Insertion sort: at most `kMaxVertexElementCount` elements, stable, and no allocation.
    const gp::USize count = inOutElements.size();
    for (gp::USize index = 1u; index < count; ++index)
    {
        const pipeline::VertexElement current = inOutElements[index];
        gp::USize position = index;
        while ((position > 0u) && isElementLess(current, inOutElements[position - 1u]))
        {
            inOutElements[position] = inOutElements[position - 1u];
            --position;
        }
        inOutElements[position] = current;
    }
}

}   // namespace gp::rhi::vulkan
