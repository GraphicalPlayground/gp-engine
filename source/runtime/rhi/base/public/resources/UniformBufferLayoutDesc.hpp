// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Vector.hpp"
#include "containers/strings/String.hpp"
#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "core/ResourceForward.hpp"          // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "hashing/Crc32.hpp"
#include "memory/MemoryBase.hpp"   // IWYU pragma: keep
#include "resources/UniformBufferEnums.hpp"
#include "RHIDefinitions.hpp"
#include "shaders/ShaderEnums.hpp"

namespace gp::rhi::resource
{

/// @brief Sentinel byte offset indicating that a uniform buffer layout carries no render target binding slots.
inline constexpr gp::UInt16 kUniformBufferInvalidOffset = 0xFFFFu;

/// @brief Describes one resource member slot inside a uniform buffer parameter struct.
struct UniformBufferResource
{
    /// @brief Byte offset of the resource pointer inside the uniform buffer parameter structure.
    gp::UInt16 memberOffset{ 0u };

    /// @brief Reflected uniform buffer member type stored at `memberOffset`.
    shader::UniformBufferBaseType memberType{ shader::UniformBufferBaseType::Invalid };

    /// @brief Default memberwise equality comparison.
    /// @param[in] rhs Resource slot entry to compare against.
    /// @return True when both offset and member type match.
    [[nodiscard]] constexpr bool operator==(const UniformBufferResource& rhs) const noexcept = default;
};

/// @brief Mutable creation descriptor used to build an immutable `UniformBufferLayout`.
/// @details Collects constant buffer byte size, static slot assignment, binding flags, and reflected resource arrays.
///          Call `computeHash()` after populating fields so shader resource tables can validate bound uniform buffers
///          and look up parameter structs by layout hash.
struct UniformBufferLayoutDesc
{
    /// @brief Debug name of the uniform buffer shader parameter struct.
    gp::String name{};

    /// @brief All RHI resource members (textures, SRVs, UAVs, samplers, and sub-buffers) sorted by offset.
    gp::Vector<UniformBufferResource> resources{};

    /// @brief Render-graph managed resource entries tracked separately for automatic pass barrier inference.
    gp::Vector<UniformBufferResource> graphResources{};

    /// @brief Render-graph managed texture entries.
    gp::Vector<UniformBufferResource> graphTextures{};

    /// @brief Render-graph managed buffer entries.
    gp::Vector<UniformBufferResource> graphBuffers{};

    /// @brief Render-graph managed nested uniform buffer entries.
    gp::Vector<UniformBufferResource> graphUniformBuffers{};

    /// @brief Non-graph referenced uniform buffer entries.
    gp::Vector<UniformBufferResource> uniformBuffers{};

    /// @brief Precomputed 32-bit layout hash used to match shaders against bound uniform buffers.
    gp::UInt32 hash{ 0u };

    /// @brief Size in bytes of the constant data block uploaded to the GPU.
    gp::UInt32 constantBufferSize{ 0u };

    /// @brief Byte offset of embedded render target binding slots, or `kUniformBufferInvalidOffset` if absent.
    gp::UInt16 renderTargetsOffset{ kUniformBufferInvalidOffset };

    /// @brief Optional global static uniform buffer slot index, or `kMaxUniformBufferStaticSlots` when unbound.
    UniformBufferStaticSlot staticSlot{ kMaxUniformBufferStaticSlots };

    /// @brief Binding modes permitted for uniform buffers created with this layout.
    shader::UniformBufferBindingFlags bindingFlags{ shader::UniformBufferBindingFlags::Shader };

    /// @brief Behavioral and reflection flags controlling uniform buffer allocation and bindless visibility.
    UniformBufferFlags flags{ UniformBufferFlags::None };

    /// @brief Constructs an empty uniform buffer layout descriptor.
    UniformBufferLayoutDesc() noexcept = default;

    /// @brief Constructs a descriptor with a debug name and leaves `hash` uncomputed until `computeHash()` is called.
    /// @param[in] inName Debug name of the parameter struct.
    explicit UniformBufferLayoutDesc(const gp::StringView inName) noexcept
        : name(inName)
    {}

    /// @brief Constructs a descriptor with a debug name and constant buffer size, immediately computing `hash`.
    /// @param[in] inName Debug name of the parameter struct.
    /// @param[in] inConstantBufferSize Constant data size in bytes.
    UniformBufferLayoutDesc(const gp::StringView inName, const gp::UInt32 inConstantBufferSize) noexcept
        : name(inName)
        , constantBufferSize(inConstantBufferSize)
    {
        computeHash();
    }

    /// @brief Returns the debug name of this uniform buffer layout descriptor.
    /// @return Non-owning view of `name`.
    [[nodiscard]] GP_FORCEINLINE_HINT gp::StringView getDebugName() const noexcept
    {
        return name.view();
    }

    /// @brief Returns the precomputed 32-bit layout hash.
    /// @note Triggers a slow-build assertion if `computeHash()` has not been called yet.
    /// @return Precomputed 32-bit layout hash.
    [[nodiscard]] GP_FORCEINLINE_HINT gp::UInt32 getHash() const noexcept
    {
        GP_CHECK_SLOW(hash != 0u);
        return hash;
    }

    /// @brief Checks whether this layout contains embedded render target binding slots.
    /// @return True when `renderTargetsOffset` is not `kUniformBufferInvalidOffset`.
    [[nodiscard]] GP_FORCEINLINE_HINT bool hasRenderTargets() const noexcept
    {
        return renderTargetsOffset != kUniformBufferInvalidOffset;
    }

    /// @brief Checks whether this layout declares outputs managed outside the render graph.
    /// @return True when `UniformBufferFlags::HasNonGraphOutputs` is set.
    [[nodiscard]] GP_FORCEINLINE_HINT bool hasExternalOutputs() const noexcept
    {
        return (flags & UniformBufferFlags::HasNonGraphOutputs) != UniformBufferFlags::None;
    }

    /// @brief Checks whether this layout is bound to a valid global static uniform buffer slot.
    /// @return True when `staticSlot` is strictly less than `kMaxUniformBufferStaticSlots`.
    [[nodiscard]] GP_FORCEINLINE_HINT bool hasStaticSlot() const noexcept
    {
        return isUniformBufferStaticSlotValid(staticSlot);
    }

    /// @brief Computes the 32-bit layout hash and stores it in `hash`.
    /// @details Encodes constant buffer size, binding flags, static-slot presence (not the runtime slot index),
    ///          emulation/reflection flags, aligned resource offsets, packed member types, and the name CRC-32.
    /// @return The newly computed 32-bit layout hash.
    gp::UInt32 computeHash() noexcept
    {
        hash = computeHashValue(*this);
        return hash;
    }

    /// @brief Computes the deterministic 32-bit layout hash for `desc` without mutating it.
    /// @param[in] desc Layout descriptor to hash.
    /// @return Deterministic 32-bit layout hash.
    [[nodiscard]] static gp::UInt32 computeHashValue(const UniformBufferLayoutDesc& desc) noexcept
    {
        gp::UInt32 tmpHash = 0u;

        tmpHash |= (desc.constantBufferSize << 16u);
        tmpHash |= (static_cast<gp::UInt32>(desc.bindingFlags) << 8u);

        // Static slot indices are assigned dynamically at runtime, so only track whether a static slot exists.
        tmpHash |= (desc.staticSlot != kMaxUniformBufferStaticSlots ? (1u << 0u) : 0u);

        // Mix flags that do not derive from the resource list into the hash.
        tmpHash |=
            ((desc.flags & UniformBufferFlags::NoEmulatedUniformBuffer) != UniformBufferFlags::None ? (1u << 1u) : 0u);
        tmpHash |=
            ((desc.flags & UniformBufferFlags::NeedsReflectedMembers) != UniformBufferFlags::None ? (1u << 2u) : 0u);

        for (gp::USize resourceIndex = 0u; resourceIndex < desc.resources.size(); ++resourceIndex)
        {
            const gp::UInt16 offset = desc.resources[resourceIndex].memberOffset;
            GP_CHECK_SLOW(offset == gp::memory::align(offset, kShaderParameterPointerAlignment));
            tmpHash ^= static_cast<gp::UInt32>(offset);
        }

        gp::USize n = desc.resources.size();
        while (n >= 4u)
        {
            tmpHash ^= (static_cast<gp::UInt32>(desc.resources[--n].memberType) << 0u);
            tmpHash ^= (static_cast<gp::UInt32>(desc.resources[--n].memberType) << 8u);
            tmpHash ^= (static_cast<gp::UInt32>(desc.resources[--n].memberType) << 16u);
            tmpHash ^= (static_cast<gp::UInt32>(desc.resources[--n].memberType) << 24u);
        }
        while (n >= 2u)
        {
            tmpHash ^= (static_cast<gp::UInt32>(desc.resources[--n].memberType) << 0u);
            tmpHash ^= (static_cast<gp::UInt32>(desc.resources[--n].memberType) << 16u);
        }
        while (n > 0u)
        {
            tmpHash ^= static_cast<gp::UInt32>(desc.resources[--n].memberType);
        }

        // Mix the name CRC-32 to distinguish distinct uniform buffer declarations that share identical layouts.
        tmpHash ^= gp::hashing::Crc32::hash(desc.name.data(), desc.name.size());

        return tmpHash;
    }

    /// @brief Copies layout contract fields and resource list from `source` into this descriptor.
    /// @param[in] source Source layout descriptor to copy.
    void copyFrom(const UniformBufferLayoutDesc& source) noexcept
    {
        constantBufferSize = source.constantBufferSize;
        staticSlot = source.staticSlot;
        bindingFlags = source.bindingFlags;
        flags = source.flags;
        resources = source.resources;
        name = source.name;
        hash = source.hash;
    }

    /// @brief Compares two layout descriptors for runtime layout compatibility.
    /// @param[in] rhs Descriptor to compare against.
    /// @return True when constant buffer size, static slot, binding flags, and resource arrays match.
    [[nodiscard]] bool operator==(const UniformBufferLayoutDesc& rhs) const noexcept
    {
        return (constantBufferSize == rhs.constantBufferSize) && (staticSlot == rhs.staticSlot) &&
               (bindingFlags == rhs.bindingFlags) && (resources == rhs.resources);
    }
};

}   // namespace gp::rhi::resource
