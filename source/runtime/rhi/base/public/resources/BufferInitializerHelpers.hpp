// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "commands/CommandListBase.hpp"
#include "core/ResourceEnums.hpp"
#include "core/ResourceForward.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "resources/Buffer.hpp"
#include "resources/BufferCreateDesc.hpp"
#include "resources/BufferInitializer.hpp"
#include "RHIBase.hpp"
#include <utility>

namespace gp::rhi::resource
{

/// @brief Buffer initializer that returns the created buffer on finalize.
struct DefaultBufferInitializer : public BufferInitializer
{
public:
    /// @brief Constructs a default buffer initializer exposing a writable staging span.
    /// @param[in] inCmdList Command list driving initialization.
    /// @param[in] inBuffer Newly created buffer pointer.
    /// @param[in] inWritableData CPU-writable data pointer, or `nullptr`.
    /// @param[in] inWritableSize Capacity in bytes of `inWritableData`.
    DefaultBufferInitializer(
        command::CommandListBase& inCmdList,
        Buffer* const inBuffer,
        void* const inWritableData,
        const gp::UInt64 inWritableSize
    ) noexcept
        : BufferInitializer(
              inCmdList,
              BufferRef(inBuffer),
              inWritableData,
              inWritableSize,
              [heldBuffer = BufferRef(inBuffer)](command::CommandListBase&) mutable noexcept -> BufferRef
    {
        return std::move(heldBuffer);
    }
          )
    {}

    /// @brief Constructs a default buffer initializer with no writable staging pointer.
    /// @param[in] inCmdList Command list driving initialization.
    /// @param[in] inBuffer Newly created buffer pointer.
    DefaultBufferInitializer(command::CommandListBase& inCmdList, Buffer* const inBuffer) noexcept
        : DefaultBufferInitializer(inCmdList, inBuffer, nullptr, 0u)
    {}
};

/// @brief Buffer initializer that locks the buffer for writing on construction and unlocks it on finalize.
struct LockBufferInitializer : public BufferInitializer
{
public:
    /// @brief Locks `inBuffer` with `ResourceLockMode::WriteOnly` and registers an unlock callback for finalize.
    /// @param[in] inCmdList Command list used to lock and unlock the buffer.
    /// @param[in] inBuffer Newly created buffer pointer.
    LockBufferInitializer(command::CommandListBase& inCmdList, Buffer* const inBuffer) noexcept
        : BufferInitializer(
              inCmdList,
              BufferRef(inBuffer),
              inCmdList.lockBuffer(inBuffer, 0u, inBuffer->getDesc().size, ResourceLockMode::WriteOnly),
              static_cast<gp::UInt64>(inBuffer->getDesc().size),
              [heldBuffer = BufferRef(inBuffer)](command::CommandListBase& execCmdList) mutable noexcept -> BufferRef
    {
        execCmdList.unlockBuffer(heldBuffer.get());
        return std::move(heldBuffer);
    }
          )
    {}
};

/// @brief Buffer initializer allowing a backend to supply a custom finalization callback.
struct CustomBufferInitializer : public BufferInitializer
{
public:
    /// @brief Constructs a buffer initializer backed by a custom finalization functor.
    /// @param[in] inCmdList Command list driving initialization.
    /// @param[in] inBuffer Newly created buffer pointer.
    /// @param[in] inWritableData CPU-writable data pointer, or `nullptr`.
    /// @param[in] inWritableSize Capacity in bytes of `inWritableData`.
    /// @param[in,out] inFinalizeFunc Backend callback invoked on finalize.
    CustomBufferInitializer(
        command::CommandListBase& inCmdList,
        Buffer* const inBuffer,
        void* const inWritableData,
        const gp::UInt64 inWritableSize,
        FinalizeCallback&& inFinalizeFunc
    ) noexcept
        : BufferInitializer(inCmdList, BufferRef(inBuffer), inWritableData, inWritableSize, std::move(inFinalizeFunc))
    {}
};

/// @brief Placeholder concrete `Buffer` returned when an unknown or unhandled `BufferInitAction` is encountered.
class GP_RHI_API InvalidBuffer final : public Buffer
{
public:
    /// @brief Constructs a placeholder buffer from `createDesc`.
    /// @param[in] createDesc Buffer creation descriptor that triggered the fallback.
    explicit InvalidBuffer(const BufferCreateDesc& createDesc) noexcept
        : Buffer(createDesc)
    {}
};

/// @brief Logs and asserts on an unknown or unhandled `BufferInitAction`, returning a fallback `InvalidBuffer`.
/// @param[in] cmdList Command list driving buffer creation.
/// @param[in] createDesc Buffer creation descriptor carrying the unhandled `initAction`.
/// @return `BufferInitializer` wrapping an `InvalidBuffer`.
[[nodiscard]] GP_RHI_API BufferInitializer handleUnknownBufferInitializerInitAction(
    command::CommandListBase& cmdList, const BufferCreateDesc& createDesc
) noexcept;

/// @brief Builds a `BufferInitializer` for unified-memory platforms where CPU memory maps directly to GPU memory.
/// @details Handles `Default`, `ResourceArray` (copies immediately and discards the array), `Zeroed` (zeros memory
///          immediately), and `Initializer` (returns writable pointer to caller). Any other action routes to
///          `handleUnknownBufferInitializerInitAction()`.
/// @param[in] cmdList Command list driving buffer creation.
/// @param[in] createDesc Buffer creation parameters and `initAction`.
/// @param[in] buffer Newly allocated buffer resource.
/// @param[in,out] writableData CPU-accessible pointer to the buffer's unified backing memory.
/// @return Configured `BufferInitializer`.
[[nodiscard]] GP_RHI_API BufferInitializer createUnifiedMemoryBufferInitializer(
    command::CommandListBase& cmdList, const BufferCreateDesc& createDesc, Buffer* buffer, void* writableData
) noexcept;

}   // namespace gp::rhi::resource
