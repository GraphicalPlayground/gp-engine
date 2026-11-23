// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Vector.hpp"      // IWYU pragma: keep
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "hashing/HashingTypes.hpp"          // IWYU pragma: keep
#include "memory/pointers/SharedPtr.hpp"
#include "shaders/ShaderEnums.hpp"
#include "shaders/ShaderForward.hpp"   // IWYU pragma: keep
#include <type_traits>
#include <utility>

namespace gp::rhi::shader
{

/// @brief Expected element stride at a reflected shader bind point for runtime validation.
struct ShaderCodeValidationStride
{
    /// @brief Hardware binding slot index referenced by the shader.
    gp::UInt16 bindPoint{ 0u };

    /// @brief Expected buffer element stride in bytes.
    gp::UInt16 stride{ 0u };

    /// @brief Default memberwise equality comparison.
    /// @param[in] rhs Validation entry to compare against.
    /// @return True when bind point and stride match.
    [[nodiscard]] constexpr bool operator==(const ShaderCodeValidationStride& rhs) const noexcept = default;
};

/// @brief Expected resource binding kind at a reflected shader slot for runtime validation.
struct ShaderCodeValidationType
{
    /// @brief Hardware binding slot index referenced by the shader.
    gp::UInt16 bindPoint{ 0u };

    /// @brief Reflected binding kind expected by the compiled shader.
    ShaderCodeResourceBindingType bindingType{ ShaderCodeResourceBindingType::Invalid };

    /// @brief Default memberwise equality comparison.
    /// @param[in] rhs Validation entry to compare against.
    /// @return True when bind point and binding kind match.
    [[nodiscard]] constexpr bool operator==(const ShaderCodeValidationType& rhs) const noexcept = default;
};

/// @brief Expected uniform buffer byte size at a reflected shader slot for runtime validation.
struct ShaderCodeValidationUBSize
{
    /// @brief Hardware uniform buffer binding slot index referenced by the shader.
    gp::UInt16 bindPoint{ 0u };

    /// @brief Minimum expected uniform buffer size in bytes.
    gp::UInt32 size{ 0u };

    /// @brief Default memberwise equality comparison.
    /// @param[in] rhs Validation entry to compare against.
    /// @return True when bind point and byte size match.
    [[nodiscard]] constexpr bool operator==(const ShaderCodeValidationUBSize& rhs) const noexcept = default;
};

/// @brief Maps a shader bind point to specialization hashes that must skip constant specialization.
struct ShaderSpecializationExclusion
{
    /// @brief Hardware resource bind point index.
    gp::UInt32 bindPoint{ 0u };

    /// @brief Specialization hashes excluded from specialization at `bindPoint`.
    gp::Vector<gp::UInt32> specializationHashes{};

    /// @brief Memberwise equality comparison.
    /// @param[in] rhs Entry to compare against.
    /// @return True when bind point and excluded hash list match.
    [[nodiscard]] bool operator==(const ShaderSpecializationExclusion& rhs) const noexcept
    {
        return (bindPoint == rhs.bindPoint) && (specializationHashes == rhs.specializationHashes);
    }
};

/// @brief Binding map linking uniform buffer member slots to hardware shader resource registers.
/// @details Built during shader compilation and attached to a shader object so the RHI backend can unpack resources
///          from bound uniform buffers into stage-specific descriptor slots without per-draw reflection lookups.
struct ShaderResourceTable
{
    /// @brief Bitmask indicating which resource tables contain resources bound to this shader.
    gp::UInt32 resourceTableBits{ 0u };

    /// @brief Mapping of bound SRVs to their location in resource tables.
    gp::Vector<gp::UInt32> shaderResourceViewMap{};

    /// @brief Mapping of bound sampler states to their location in resource tables.
    gp::Vector<gp::UInt32> samplerMap{};

    /// @brief Mapping of bound UAVs to their location in resource tables.
    gp::Vector<gp::UInt32> unorderedAccessViewMap{};

    /// @brief Compile-time layout hashes of referenced resource tables, used for runtime validation.
    gp::Vector<gp::UInt32> resourceTableLayoutHashes{};

    /// @brief Mapping of bound textures to their location in resource tables.
    gp::Vector<gp::UInt32> textureMap{};

    /// @brief Mapping of bound resource collections to their location in resource tables.
    gp::Vector<gp::UInt32> resourceCollectionMap{};

    /// @brief Mapping of bound descriptor ranges to their location in resource tables.
    gp::Vector<gp::UInt32> descriptorRangeMap{};

    /// @brief Mapping of bound uniform blocks to their location in resource tables (target stores the cbuffer index).
    gp::Vector<gp::UInt32> uniformBlockMap{};

    /// @brief Checks whether any resource table slots carry bound resources.
    /// @return True when `resourceTableBits` is zero.
    [[nodiscard]] GP_FORCEINLINE_HINT bool isEmpty() const noexcept
    {
        return resourceTableBits == 0u;
    }

    /// @brief Compares two shader resource tables for exact structural equality.
    /// @param[in] rhs Table to compare against.
    /// @return True when bitmasks and all eight binding maps match element for element.
    [[nodiscard]] bool operator==(const ShaderResourceTable& rhs) const noexcept
    {
        return (resourceTableBits == rhs.resourceTableBits) && (shaderResourceViewMap == rhs.shaderResourceViewMap) &&
               (samplerMap == rhs.samplerMap) && (unorderedAccessViewMap == rhs.unorderedAccessViewMap) &&
               (resourceTableLayoutHashes == rhs.resourceTableLayoutHashes) && (textureMap == rhs.textureMap) &&
               (resourceCollectionMap == rhs.resourceCollectionMap) && (descriptorRangeMap == rhs.descriptorRangeMap) &&
               (uniformBlockMap == rhs.uniformBlockMap);
    }
};

/// @brief Input descriptor carrying compiled shader bytecode and optional backing ownership for shader creation.
/// @details Decouples shader factory APIs from raw byte arrays so asynchronous creation paths can keep the backing
///          allocation alive via `codeOwner` or trace shaders back to their source `ShaderLibrary`.
struct CreateShaderDesc
{
    /// @brief Non-owning view over the compiled shader bytecode payload.
    gp::ConstVectorView<gp::UInt8> code{};

    /// @brief Optional type-erased owner keeping the memory backing `code` alive across asynchronous creation.
    gp::SharedPtr<const void> codeOwner{};

    /// @brief Optional shader library that provided this bytecode, or `nullptr` for standalone shaders.
    ShaderLibrary* library{ nullptr };

    /// @brief Constructs an empty shader creation descriptor.
    CreateShaderDesc() noexcept = default;

    /// @brief Constructs a descriptor referencing external bytecode without shared ownership.
    /// @param[in] inCode Non-owning view of compiled shader bytecode.
    /// @param[in] inLibrary Optional owning shader library pointer.
    explicit CreateShaderDesc(
        const gp::ConstVectorView<gp::UInt8> inCode, ShaderLibrary* const inLibrary = nullptr
    ) noexcept
        : code(inCode)
        , codeOwner(nullptr)
        , library(inLibrary)
    {}

    /// @brief Constructs a descriptor referencing bytecode backed by an existing shared owner handle.
    /// @param[in] inCode View over the compiled shader bytecode inside `inCodeOwner`.
    /// @param[in] inCodeOwner Shared handle retaining the backing allocation for `inCode`.
    /// @param[in] inLibrary Optional owning shader library pointer.
    CreateShaderDesc(
        const gp::ConstVectorView<gp::UInt8> inCode,
        gp::SharedPtr<const void> inCodeOwner,
        ShaderLibrary* const inLibrary = nullptr
    ) noexcept
        : code(inCode)
        , codeOwner(std::move(inCodeOwner))
        , library(inLibrary)
    {}

    /// @brief Constructs a descriptor attaching a custom deleter to release the memory backing `inCode`.
    /// @details Passing `inCode.data()` into `codeOwner` pairs the deleter with the view payload and makes
    ///          `codeOwner.isValid()` true so asynchronous shader creation can tell owned memory from transient views.
    /// @tparam InDeleterType Callable invoked with `const void*` when the last shared reference to `codeOwner` drops.
    /// @param[in] inCode View over the compiled shader bytecode.
    /// @param[in] inDeleter Custom deleter responsible for releasing the memory backing `inCode`.
    /// @param[in] inLibrary Optional owning shader library pointer.
    template <typename InDeleterType>
    requires(!std::is_pointer_v<std::decay_t<InDeleterType>> && std::is_invocable_v<InDeleterType, const void*>)
    CreateShaderDesc(
        const gp::ConstVectorView<gp::UInt8> inCode, InDeleterType&& inDeleter, ShaderLibrary* const inLibrary = nullptr
    ) noexcept
        : code(inCode)
        , codeOwner(static_cast<const void*>(inCode.data()), std::forward<InDeleterType>(inDeleter))
        , library(inLibrary)
    {}

    /// @brief Checks whether this descriptor holds a non-empty bytecode payload.
    /// @return True when `code` contains at least one byte.
    [[nodiscard]] GP_FORCEINLINE_HINT bool isValid() const noexcept
    {
        return !code.isEmpty();
    }
};

}   // namespace gp::rhi::shader
