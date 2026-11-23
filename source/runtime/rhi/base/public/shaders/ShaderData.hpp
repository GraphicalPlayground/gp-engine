// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Vector.hpp"      // IWYU pragma: keep
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "resources/UniformBufferEnums.hpp"
#include "RHIBase.hpp"
#include "shaders/ShaderForward.hpp"   // IWYU pragma: keep
#include "shaders/ShaderTypes.hpp"
#include <utility>

namespace gp::rhi::shader
{

/// @brief Reflected shader resource table and static uniform buffer slot metadata shared by shader objects.
/// @details Separated from `Shader` so serialization and binding layers can inspect or populate resource tables and
///          static uniform buffer slots independently of `Resource` reference counting.
class GP_RHI_API ShaderData
{
public:
    /// @brief Constructs an empty shader metadata block with no bindings or static slots.
    ShaderData() noexcept;

    ShaderData(const ShaderData&) = delete;
    ShaderData& operator=(const ShaderData&) = delete;
    ShaderData(ShaderData&&) = delete;
    ShaderData& operator=(ShaderData&&) = delete;

protected:
    /// @brief Destroys the shader metadata block.
    ~ShaderData() noexcept;

public:
    /// @brief Returns the reflected shader resource table describing uniform buffer resource mappings.
    /// @return Immutable reference to this shader's `ShaderResourceTable`.
    [[nodiscard]] GP_FORCEINLINE_HINT const ShaderResourceTable& getShaderResourceTable() const noexcept
    {
        return m_shaderResourceTable;
    }

    /// @brief Returns the list of global static uniform buffer slots bound by this shader.
    /// @return Contiguous view of `UniformBufferStaticSlot` indices in uniform buffer binding order.
    [[nodiscard]] GP_FORCEINLINE_HINT gp::ConstVectorView<resource::UniformBufferStaticSlot>
        getStaticSlots() const noexcept
    {
        return m_staticSlots;
    }

    /// @brief Checks whether this shader binds any static uniform buffer slots.
    /// @return True if `m_staticSlots` is non-empty; false otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT bool hasStaticSlots() const noexcept
    {
        return !m_staticSlots.isEmpty();
    }

    /// @brief Replaces the reflected shader resource table during shader initialization.
    /// @param[in] table Populated `ShaderResourceTable` to store.
    void setShaderResourceTable(ShaderResourceTable table) noexcept
    {
        m_shaderResourceTable = std::move(table);
    }

    /// @brief Replaces the static uniform buffer slot array during shader initialization.
    /// @param[in] staticSlots Ordered static slot indices referenced by this shader.
    void setStaticSlots(gp::Vector<resource::UniformBufferStaticSlot> staticSlots) noexcept
    {
        m_staticSlots = std::move(staticSlots);
    }

protected:
    /// @brief Reflected uniform buffer resource binding maps for this shader.
    ShaderResourceTable m_shaderResourceTable{};

    /// @brief Global static uniform buffer slot indices indexed by uniform buffer bind slot.
    gp::Vector<resource::UniformBufferStaticSlot> m_staticSlots{};
};

}   // namespace gp::rhi::shader
