// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "commands/CommandForward.hpp"
#include "core/Resource.hpp"
#include "core/ResourceForward.hpp"            // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "memory/pointers/RefCountedPtr.hpp"   // IWYU pragma: keep
#include "resources/Buffer.hpp"
#include "RHIBase.hpp"

namespace gp::rhi::resource
{

/// @brief Indirection slot binding a replaceable vertex stream `Buffer` and byte offset.
/// @details Allows mesh draw commands to reference a stable `StreamSourceSlot` while streaming or dynamic geometry
///          systems swap the backing `Buffer` and starting byte offset on the render or RHI thread.
class GP_RHI_API StreamSourceSlot final : public Resource
{
    friend class ::gp::rhi::command::CommandListBase;

public:
    StreamSourceSlot() = delete;
    StreamSourceSlot(const StreamSourceSlot&) = delete;
    StreamSourceSlot& operator=(const StreamSourceSlot&) = delete;
    StreamSourceSlot(StreamSourceSlot&&) = delete;
    StreamSourceSlot& operator=(StreamSourceSlot&&) = delete;

public:
    /// @brief Creates a new `StreamSourceSlot` initially referencing `buffer` (which may be null).
    /// @param[in] buffer Optional initial GPU buffer bound to this slot.
    /// @return Strong `StreamSourceSlotRef` managing the newly allocated slot.
    [[nodiscard]] static StreamSourceSlotRef create(Buffer* buffer = nullptr) noexcept;

    /// @brief Returns the GPU buffer currently bound to this stream slot, or `nullptr` if unbound.
    /// @return Non-owning pointer to the active `Buffer`.
    [[nodiscard]] GP_FORCEINLINE_HINT Buffer* getBuffer() const noexcept
    {
        return m_buffer.get();
    }

    /// @brief Returns the starting byte offset applied when binding `getBuffer()`.
    /// @return Byte offset within the bound buffer.
    [[nodiscard]] GP_FORCEINLINE_HINT gp::UInt32 getOffset() const noexcept
    {
        return m_offset;
    }

    /// @brief Updates both the backing GPU buffer and byte offset bound to this stream slot.
    /// @param[in] buffer New GPU buffer to retain (may be `nullptr`).
    /// @param[in] offset Starting byte offset within `buffer`.
    void update(Buffer* buffer, gp::UInt32 offset = 0u) noexcept;

private:
    /// @brief Constructs a stream source slot tagged with `ResourceType::StreamSourceSlot`.
    /// @param[in] buffer Initial GPU buffer to retain.
    explicit StreamSourceSlot(Buffer* buffer) noexcept;

    /// @copydoc Resource::~Resource()
    virtual ~StreamSourceSlot() noexcept override;

private:
    /// @brief Retained GPU buffer referenced by this stream slot.
    BufferRef m_buffer{};

    /// @brief Byte offset into `m_buffer`.
    gp::UInt32 m_offset{ 0u };
};

}   // namespace gp::rhi::resource
