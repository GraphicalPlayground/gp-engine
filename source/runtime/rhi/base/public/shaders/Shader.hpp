// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Vector.hpp"
#include "containers/strings/Name.hpp"
#include "containers/strings/String.hpp"
#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "core/Resource.hpp"
#include "core/ResourceEnums.hpp"
#include "CoreMinimal.hpp"
#include "hashing/HashingTypes.hpp"   // IWYU pragma: keep
#include "memory/MemoryForward.hpp"
#include "memory/MemoryOrder.hpp"
#include "RHIBase.hpp"
#include "shaders/ShaderData.hpp"
#include "shaders/ShaderEnums.hpp"
#include "shaders/ShaderForward.hpp"   // IWYU pragma: keep
#include "shaders/ShaderTypes.hpp"
#include "threading/atomics/Atomic.hpp"
#include <type_traits>
#include <utility>

namespace gp::rhi::shader
{

/// @brief Base reference-counted RHI resource for all compiled programmable shader stages.
/// @details Pairs `resource::Resource` lifetime management with `ShaderData` binding metadata, a 128-bit bytecode hash,
///          stage frequency tag, capability flags, specialization exclusion records, and an atomic counter that guards
///          against freeing a shader while a background pipeline state compilation job still references it.
class GP_RHI_API Shader
    : public resource::Resource
    , public ShaderData
{
public:
    Shader() = delete;
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    Shader(Shader&&) = delete;
    Shader& operator=(Shader&&) = delete;

protected:
    /// @brief Constructs a shader resource with its runtime resource tag and pipeline stage frequency.
    /// @param[in] resourceType Concrete `resource::ResourceType` tag for this shader subclass.
    /// @param[in] frequency Pipeline stage represented by this shader.
    Shader(resource::ResourceType resourceType, ShaderFrequency frequency) noexcept;

    /// @copydoc resource::Resource::~Resource()
    /// @details Verifies in checked builds that no pipeline state compilation job still holds an active use count on
    ///          this shader.
    virtual ~Shader() noexcept override;

public:
    /// @brief Assigns the 128-bit content hash identifying this shader's compiled bytecode.
    /// @param[in] hash Bytecode content hash.
    GP_FORCEINLINE_HINT void setHash(const ShaderHash& hash) noexcept
    {
        m_hash = hash;
    }

    /// @brief Returns the 128-bit content hash identifying this shader's compiled bytecode.
    /// @return Immutable reference to the stored `ShaderHash`.
    [[nodiscard]] GP_FORCEINLINE_HINT const ShaderHash& getHash() const noexcept
    {
        return m_hash;
    }

    /// @brief Returns the programmable pipeline stage executed by this shader.
    /// @return `ShaderFrequency` assigned at construction.
    [[nodiscard]] GP_FORCEINLINE_HINT ShaderFrequency getFrequency() const noexcept
    {
        return m_frequency;
    }

    /// @brief Records whether this shader avoids screen-space derivative operations.
    /// @param[in] noDerivativeOps True when the shader contains no `ddx`/`ddy` or implicit-LOD texture samples.
    GP_FORCEINLINE_HINT void setNoDerivativeOps(const bool noDerivativeOps) noexcept
    {
        m_noDerivativeOps = noDerivativeOps ? 1u : 0u;
    }

    /// @brief Reports whether this shader guarantees no screen-space derivative operations.
    /// @return True when derivative operations are absent; false otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT bool hasNoDerivativeOps() const noexcept
    {
        return m_noDerivativeOps != 0u;
    }

    /// @brief Records whether this shader may be dispatched through a shader bundle.
    /// @param[in] usedInShaderBundle True when shader bundle execution is enabled for this shader.
    GP_FORCEINLINE_HINT void setShaderBundleUsage(const bool usedInShaderBundle) noexcept
    {
        m_shaderBundleUsage = usedInShaderBundle ? 1u : 0u;
    }

    /// @brief Reports whether this shader is flagged for shader bundle dispatch.
    /// @return True when shader bundle usage is enabled; false otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT bool hasShaderBundleUsage() const noexcept
    {
        return m_shaderBundleUsage != 0u;
    }

    /// @brief Records whether this shader declares pipeline specialization constants.
    /// @param[in] hasSpecializationConstants True when specialization constants are present.
    GP_FORCEINLINE_HINT void setSpecializationConstants(const bool hasSpecializationConstants) noexcept
    {
        m_hasSpecializationConstants = hasSpecializationConstants ? 1u : 0u;
    }

    /// @brief Reports whether this shader declares pipeline specialization constants.
    /// @return True when specialization constants are enabled on this shader.
    [[nodiscard]] GP_FORCEINLINE_HINT bool hasSpecializationConstants() const noexcept
    {
        return m_hasSpecializationConstants != 0u;
    }

    /// @brief Returns all bind-point specialization exclusion records stored on this shader.
    /// @return Contiguous view of `ShaderSpecializationExclusion` entries.
    [[nodiscard]] GP_FORCEINLINE_HINT gp::ConstVectorView<ShaderSpecializationExclusion>
        getSpecializationExclusions() const noexcept
    {
        return m_specializationExclusions;
    }

    /// @brief Replaces the bind-point specialization exclusion table for this shader.
    /// @param[in] exclusions Exclusion records mapping bind points to skipped specialization hashes.
    void setSpecializationExclusions(gp::Vector<ShaderSpecializationExclusion> exclusions) noexcept
    {
        m_specializationExclusions = std::move(exclusions);
    }

    /// @brief Checks whether `specializationHash` is excluded from specialization at `bindPoint`.
    /// @param[in] bindPoint Shader resource bind point index.
    /// @param[in] specializationHash Candidate specialization constant hash.
    /// @return True when specialization constants are active and `(bindPoint, specializationHash)` is excluded.
    [[nodiscard]] bool isExcludedFromSpecialization(gp::UInt32 bindPoint, gp::UInt32 specializationHash) const noexcept;

    /// @brief Increments or decrements the active background pipeline compilation user count.
    /// @details Backends and precache tasks pair `setInUseByPSOCompilation(true)` and `setInUseByPSOCompilation(false)`
    ///          around asynchronous PSO builds so `~Shader()` catches premature destruction in checked builds.
    /// @param[in] inUse True when a compilation task starts using this shader; false when it finishes.
    void setInUseByPSOCompilation(bool inUse) noexcept;

    /// @brief Reports whether one or more pipeline compilation tasks currently reference this shader.
    /// @return True when the active compilation use count is greater than zero.
    [[nodiscard]] GP_FORCEINLINE_HINT bool isInUseByPSOCompilation() const noexcept
    {
        return m_inUseByPSOCompilation.load(gp::memory::MemoryOrder::Acquire) > 0;
    }

    /// @brief Returns the current number of active pipeline compilation tasks referencing this shader.
    /// @return Non-negative active compilation reference count.
    [[nodiscard]] GP_FORCEINLINE_HINT gp::Int32 getPSOCompilationUseCount() const noexcept
    {
        return m_inUseByPSOCompilation.load(gp::memory::MemoryOrder::Acquire);
    }

#if GP_RHI_INCLUDE_SHADER_DEBUG_DATA
    /// @brief Assigns a human-readable debug name for diagnostics, profiler markers, and crash logs.
    /// @param[in] name Shader debug name string.
    void setDebugShaderName(const gp::StringView name) noexcept
    {
        m_debugInfo.shaderName = gp::String(name);
    }

    /// @brief Reports whether a non-empty debug shader name has been assigned.
    /// @return True when `m_debugInfo.shaderName` is non-empty.
    [[nodiscard]] GP_FORCEINLINE_HINT bool hasShaderName() const noexcept
    {
        return !m_debugInfo.shaderName.isEmpty();
    }

    /// @brief Returns the human-readable debug name assigned to this shader, or `"<unknown>"` when unset.
    /// @return Non-owning view of the debug name or `"<unknown>"`.
    [[nodiscard]] GP_FORCEINLINE_HINT gp::StringView getShaderName() const noexcept
    {
        return !m_debugInfo.shaderName.isEmpty() ? m_debugInfo.shaderName.view() : gp::StringView("<unknown>");
    }

    /// @brief Replaces the table of interned uniform buffer names indexed by shader bind slot.
    /// @param[in] names Uniform buffer names corresponding to this shader's uniform buffer slots.
    void setUniformBufferNames(gp::Vector<gp::Name> names) noexcept
    {
        m_debugInfo.uniformBufferNames = std::move(names);
    }

    /// @brief Returns all debug uniform buffer names recorded for this shader.
    /// @return Contiguous view of interned `gp::Name` uniform buffer identifiers.
    [[nodiscard]] GP_FORCEINLINE_HINT gp::ConstVectorView<gp::Name> getUniformBufferNames() const noexcept
    {
        return m_debugInfo.uniformBufferNames;
    }

    /// @brief Returns the debug uniform buffer name at slot `index`, or `"<unknown>"` if out of range.
    /// @param[in] index Uniform buffer slot index.
    /// @return Formatted uniform buffer name string, or `"<unknown>"` when `index` is out of bounds.
    [[nodiscard]] gp::String getUniformBufferName(const gp::UInt32 index) const noexcept
    {
        if (static_cast<gp::USize>(index) < m_debugInfo.uniformBufferNames.size())
        {
            return m_debugInfo.uniformBufferNames[index].toString();
        }
        return gp::String("<unknown>");
    }

    /// @brief Stores expected buffer element strides for runtime binding validation.
    /// @param[in] strides Validation stride entries extracted from shader reflection.
    void setCodeValidationStrides(gp::Vector<ShaderCodeValidationStride> strides) noexcept
    {
        m_debugStrideValidationData = std::move(strides);
    }

    /// @brief Returns expected buffer element strides for runtime binding validation.
    /// @return Contiguous view of `ShaderCodeValidationStride` entries.
    [[nodiscard]] GP_FORCEINLINE_HINT gp::ConstVectorView<ShaderCodeValidationStride>
        getCodeValidationStrides() const noexcept
    {
        return m_debugStrideValidationData;
    }

    /// @brief Stores expected SRV resource binding types for runtime binding validation.
    /// @param[in] bindingTypes Reflected SRV validation entries.
    void setCodeValidationSRVTypes(gp::Vector<ShaderCodeValidationType> bindingTypes) noexcept
    {
        m_debugSRVTypeValidationData = std::move(bindingTypes);
    }

    /// @brief Returns expected SRV resource binding types for runtime binding validation.
    /// @return Contiguous view of SRV `ShaderCodeValidationType` entries.
    [[nodiscard]] GP_FORCEINLINE_HINT gp::ConstVectorView<ShaderCodeValidationType>
        getCodeValidationSRVTypes() const noexcept
    {
        return m_debugSRVTypeValidationData;
    }

    /// @brief Stores expected UAV resource binding types for runtime binding validation.
    /// @param[in] bindingTypes Reflected UAV validation entries.
    void setCodeValidationUAVTypes(gp::Vector<ShaderCodeValidationType> bindingTypes) noexcept
    {
        m_debugUAVTypeValidationData = std::move(bindingTypes);
    }

    /// @brief Returns expected UAV resource binding types for runtime binding validation.
    /// @return Contiguous view of UAV `ShaderCodeValidationType` entries.
    [[nodiscard]] GP_FORCEINLINE_HINT gp::ConstVectorView<ShaderCodeValidationType>
        getCodeValidationUAVTypes() const noexcept
    {
        return m_debugUAVTypeValidationData;
    }

    /// @brief Stores expected uniform buffer byte sizes for runtime binding validation.
    /// @param[in] ubSizes Validation uniform buffer size entries extracted from shader reflection.
    void setCodeValidationUBSizes(gp::Vector<ShaderCodeValidationUBSize> ubSizes) noexcept
    {
        m_debugUBSizeValidationData = std::move(ubSizes);
    }

    /// @brief Returns expected uniform buffer byte sizes for runtime binding validation.
    /// @return Contiguous view of `ShaderCodeValidationUBSize` entries.
    [[nodiscard]] GP_FORCEINLINE_HINT gp::ConstVectorView<ShaderCodeValidationUBSize>
        getCodeValidationUBSizes() const noexcept
    {
        return m_debugUBSizeValidationData;
    }
#else
    /// @brief No-op when `GP_RHI_INCLUDE_SHADER_DEBUG_DATA` is disabled.
    /// @param[in] name Ignored debug name.
    GP_FORCEINLINE_HINT void setDebugShaderName(const gp::StringView name) noexcept
    {
        static_cast<void>(name);
    }

    /// @brief Always returns false when `GP_RHI_INCLUDE_SHADER_DEBUG_DATA` is disabled.
    /// @return False.
    [[nodiscard]] GP_FORCEINLINE_HINT bool hasShaderName() const noexcept
    {
        return false;
    }

    /// @brief Always returns `"<unknown>"` when `GP_RHI_INCLUDE_SHADER_DEBUG_DATA` is disabled.
    /// @return `gp::StringView("<unknown>")`.
    [[nodiscard]] GP_FORCEINLINE_HINT gp::StringView getShaderName() const noexcept
    {
        return gp::StringView("<unknown>");
    }

    /// @brief Always returns `"<unknown>"` when `GP_RHI_INCLUDE_SHADER_DEBUG_DATA` is disabled.
    /// @param[in] index Ignored slot index.
    /// @return `gp::String("<unknown>")`.
    [[nodiscard]] GP_FORCEINLINE_HINT gp::String getUniformBufferName(const gp::UInt32 index) const noexcept
    {
        static_cast<void>(index);
        return gp::String("<unknown>");
    }
#endif

private:
#if GP_RHI_INCLUDE_SHADER_DEBUG_DATA
    /// @brief Debug shader name and reflected uniform buffer slot identifiers.
    struct DebugInfo
    {
        /// @brief Human-readable debug shader identifier.
        gp::String shaderName{};

        /// @brief Interned uniform buffer names indexed by bind slot.
        gp::Vector<gp::Name> uniformBufferNames{};
    };

    /// @brief Optional shader debug names retained when `GP_RHI_INCLUDE_SHADER_DEBUG_DATA` is active.
    DebugInfo m_debugInfo{};

    /// @brief Expected buffer element strides used for runtime binding validation.
    gp::Vector<ShaderCodeValidationStride> m_debugStrideValidationData{};

    /// @brief Expected SRV binding types used for runtime binding validation.
    gp::Vector<ShaderCodeValidationType> m_debugSRVTypeValidationData{};

    /// @brief Expected UAV binding types used for runtime binding validation.
    gp::Vector<ShaderCodeValidationType> m_debugUAVTypeValidationData{};

    /// @brief Expected uniform buffer byte sizes used for runtime binding validation.
    gp::Vector<ShaderCodeValidationUBSize> m_debugUBSizeValidationData{};
#endif

    /// @brief Per-bind-point specialization constant hashes excluded from specialization.
    gp::Vector<ShaderSpecializationExclusion> m_specializationExclusions{};

    /// @brief 128-bit bytecode hash used for pipeline cache keys and deduplication.
    ShaderHash m_hash{};

    /// @brief Active count of asynchronous pipeline compilation tasks referencing this shader.
    gp::thread::Atomic<gp::Int32> m_inUseByPSOCompilation{ 0 };

    /// @brief Immutable programmable pipeline stage represented by this shader.
    const ShaderFrequency m_frequency{ ShaderFrequency::Vertex };

    /// @brief True when the shader uses no screen-space derivative operations.
    gp::UInt8 m_noDerivativeOps : 1 { 0u };

    /// @brief True when the shader is marked for shader bundle dispatch.
    gp::UInt8 m_shaderBundleUsage : 1 { 0u };

    /// @brief True when the shader uses pipeline specialization constants.
    gp::UInt8 m_hasSpecializationConstants : 1 { 0u };
};

/// @brief Compile-time trait mapping a concrete shader class, pointer, or `RefCountedPtr` to its `ShaderFrequency`.
/// @tparam InShaderType Concrete `Shader` subclass, raw pointer, or `gp::RefCountedPtr` handle.
template <typename InShaderType>
struct ShaderFrequencyOf
{
    /// @brief Pipeline stage corresponding to `InShaderType`.
    static constexpr ShaderFrequency kValue = std::remove_cv_t<InShaderType>::kStaticFrequency;
};

/// @brief Partial specialization unwrapping raw shader pointers.
/// @tparam InShaderType Pointed-to `Shader` subclass.
template <typename InShaderType>
struct ShaderFrequencyOf<InShaderType*> : ShaderFrequencyOf<InShaderType>
{};

/// @brief Partial specialization unwrapping intrusive `gp::RefCountedPtr` shader handles.
/// @tparam InShaderType Managed `Shader` subclass.
/// @tparam InPolicy Reference counting policy type.
template <typename InShaderType, typename InPolicy>
struct ShaderFrequencyOf<gp::RefCountedPtr<InShaderType, InPolicy>> : ShaderFrequencyOf<InShaderType>
{};

/// @brief Shorthand variable template for `ShaderFrequencyOf<std::remove_cvref_t<InShaderType>>::kValue`.
/// @tparam InShaderType Concrete `Shader` subclass, raw pointer, or `gp::RefCountedPtr` handle.
template <typename InShaderType>
inline constexpr ShaderFrequency kShaderFrequencyOf = ShaderFrequencyOf<std::remove_cvref_t<InShaderType>>::kValue;

}   // namespace gp::rhi::shader
