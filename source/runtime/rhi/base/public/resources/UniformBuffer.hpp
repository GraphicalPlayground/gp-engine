// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Vector.hpp"
#include "containers/views/VectorView.hpp"
#include "core/Resource.hpp"
#include "core/ResourceForward.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "memory/pointers/RefCountedPtr.hpp"
#include "resources/UniformBufferLayout.hpp"
#include "RHIBase.hpp"
#include "views/DescriptorHandle.hpp"

namespace gp::rhi::resource
{

/// @brief Base GPU uniform buffer resource binding constant data and referenced RHI resources under a fixed layout.
/// @details Retains a strong `UniformBufferLayoutConstRef` so layout metadata outlives every referencing uniform buffer
///          across deferred deletion. Concrete RHI backends derive from `UniformBuffer` to store API-specific constant
///          buffer allocations and populate `m_resourceTable` with the referenced textures, views, and samplers.
class GP_RHI_API UniformBuffer : public Resource
{
public:
    UniformBuffer() = delete;
    UniformBuffer(const UniformBuffer&) = delete;
    UniformBuffer& operator=(const UniformBuffer&) = delete;
    UniformBuffer(UniformBuffer&&) = delete;
    UniformBuffer& operator=(UniformBuffer&&) = delete;

protected:
    /// @brief Constructs a uniform buffer bound to `layout`.
    /// @param[in] layout Non-null immutable layout describing this uniform buffer's constant block and resource slots.
    explicit UniformBuffer(const UniformBufferLayout* layout) noexcept;

    /// @copydoc Resource::~Resource()
    /// @details Removes any CPU-side diagnostic snapshot from `uniformBufferContentMap` when enabled.
    virtual ~UniformBuffer() noexcept override;

public:
    /// @brief Returns the byte size of this uniform buffer's constant data payload.
    /// @return Cached constant buffer size in bytes.
    [[nodiscard]] GP_FORCEINLINE_HINT gp::UInt32 getSize() const noexcept
    {
        GP_CHECK(m_layoutConstantBufferSize == m_layout->getConstantBufferSize());
        return m_layoutConstantBufferSize;
    }

    /// @brief Returns the immutable layout describing this uniform buffer's contents.
    /// @return Immutable reference to the retained `UniformBufferLayout`.
    [[nodiscard]] GP_FORCEINLINE_HINT const UniformBufferLayout& getLayout() const noexcept
    {
        return *m_layout;
    }

    /// @brief Returns a non-owning pointer to the retained `UniformBufferLayout`.
    /// @return Pointer to the immutable layout (always non-null for constructed instances).
    [[nodiscard]] GP_FORCEINLINE_HINT const UniformBufferLayout* getLayoutPtr() const noexcept
    {
        return m_layout.get();
    }

    /// @brief Returns the resource handles referenced by this uniform buffer's resource table.
    /// @return Contiguous view of retained `ResourceRef` entries aligned with `getLayout().getResources()`.
    [[nodiscard]] GP_FORCEINLINE_HINT gp::ConstVectorView<ResourceRef> getResourceTable() const noexcept
    {
        return m_resourceTable;
    }

    /// @brief Returns the bindless descriptor handle for this uniform buffer when bindless access is enabled.
    /// @details The base implementation returns an invalid `view::DescriptorHandle`.
    /// @return Bindless descriptor handle, or an invalid handle if unsupported or non-bindless.
    [[nodiscard]] virtual view::DescriptorHandle getBindlessHandle() const noexcept;

protected:
    /// @brief RHI resources referenced by this uniform buffer, kept alive for the buffer's lifetime.
    gp::Vector<ResourceRef> m_resourceTable{};

private:
    /// @brief Retained immutable layout shared by all uniform buffers matching this parameter struct.
    UniformBufferLayoutConstRef m_layout{};

    /// @brief Cached copy of `m_layout->getConstantBufferSize()` to avoid pointer chasing in bind hot paths.
    gp::UInt32 m_layoutConstantBufferSize{ 0u };
};

#if GP_RHI_INCLUDE_SHADER_DEBUG_DATA
/// @brief Optional CPU-side mirror of uniform buffer constant payloads for GPU debugging and diagnostics.
/// @details Disabled by default so normal frames incur zero overhead. When enabled via `setEnabled(true)`, backends
///          record CPU copies on creation and update, and `~UniformBuffer()` automatically purges entries on teardown.
namespace uniformBufferContentMap
{

/// @brief Enables or disables CPU-side uniform buffer payload mirroring.
/// @details Turning tracking off clears all currently stored snapshots.
/// @param[in] enabled True to record uniform buffer contents; false to disable and clear existing entries.
GP_RHI_API void setEnabled(bool enabled) noexcept;

/// @brief Reports whether CPU-side uniform buffer payload mirroring is currently active.
/// @return True when enabled; false otherwise.
[[nodiscard]] GP_RHI_API bool isEnabled() noexcept;

/// @brief Stores or replaces a CPU copy of `contents` (`uniformBuffer->getSize()` bytes) for `uniformBuffer`.
/// @param[in] uniformBuffer Non-null uniform buffer key.
/// @param[in] contents Pointer to constant buffer data, or `nullptr` to store an empty snapshot.
GP_RHI_API void update(const UniformBuffer* uniformBuffer, const void* contents) noexcept;

/// @brief Removes any stored CPU snapshot associated with `uniformBuffer`.
/// @param[in] uniformBuffer Uniform buffer key to remove.
GP_RHI_API void remove(const UniformBuffer* uniformBuffer) noexcept;

/// @brief Retrieves the stored CPU snapshot pointer for `uniformBuffer`, or `nullptr` if none is recorded.
/// @param[in] uniformBuffer Uniform buffer key to look up.
/// @return Pointer to copied constant bytes, or `nullptr`.
[[nodiscard]] GP_RHI_API const void* get(const UniformBuffer* uniformBuffer) noexcept;

/// @brief Clears all stored CPU uniform buffer snapshots.
GP_RHI_API void clear() noexcept;

}   // namespace uniformBufferContentMap
#endif

}   // namespace gp::rhi::resource
