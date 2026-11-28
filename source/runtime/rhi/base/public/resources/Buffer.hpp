// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/strings/Name.hpp"
#include "core/ResourceForward.hpp"   // IWYU pragma: keep
#include "core/ViewableResource.hpp"
#include "CoreMinimal.hpp"
#include "resources/BufferDesc.hpp"
#include "resources/BufferEnums.hpp"
#include "RHIBase.hpp"
#if GP_ENABLE_RHI_VALIDATION
    #include "validation/ValidationResource.hpp"
#endif

namespace gp::rhi::resource
{

/// @brief Base GPU buffer resource supporting shader views, barrier state tracking, and ownership transfer.
/// @details Stores an immutable `BufferDesc` initialized from `BufferCreateDesc`. Backends derive from `Buffer` to
///          hold API-specific buffer handles and GPU memory allocations, calling `takeOwnership()` and
///          `releaseOwnership()` during streaming or reallocation swaps.
class GP_RHI_API Buffer : public ViewableResource
{
public:
    Buffer() = delete;
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;
    Buffer(Buffer&&) = delete;
    Buffer& operator=(Buffer&&) = delete;

protected:
    /// @brief Constructs a GPU buffer initialized from `createDesc`.
    /// @param[in] createDesc Creation parameters carrying layout, initial access state, and debug/owner names.
    explicit Buffer(const BufferCreateDesc& createDesc) noexcept;

    /// @copydoc ViewableResource::~ViewableResource()
    virtual ~Buffer() noexcept override;

public:
    /// @brief Returns the buffer layout descriptor describing size, stride, usage, and GPU affinity.
    /// @return Immutable reference to `BufferDesc`.
    [[nodiscard]] GP_FORCEINLINE_HINT const BufferDesc& getDesc() const noexcept
    {
        return m_desc;
    }

    /// @brief Returns the total size of this buffer in bytes.
    /// @return Buffer byte size.
    [[nodiscard]] GP_FORCEINLINE_HINT gp::UInt32 getSize() const noexcept
    {
        return m_desc.size;
    }

    /// @brief Returns the element stride of this buffer in bytes.
    /// @return Element stride in bytes, or zero for untyped buffers.
    [[nodiscard]] GP_FORCEINLINE_HINT gp::UInt32 getStride() const noexcept
    {
        return m_desc.stride;
    }

    /// @brief Returns the usage and binding flags assigned to this buffer.
    /// @return Bitmask of `BufferUsageFlags`.
    [[nodiscard]] GP_FORCEINLINE_HINT BufferUsageFlags getUsage() const noexcept
    {
        return m_desc.usage;
    }

    /// @brief Replaces the debug name stored on this buffer.
    /// @param[in] inName New interned debug identifier.
    GP_FORCEINLINE_HINT void setName(const gp::Name inName) noexcept
    {
        ViewableResource::setName(inName);
#if GP_ENABLE_RHI_VALIDATION
        m_validationResource.setDebugName(inName.toString().view());
#endif
    }

#if GP_ENABLE_RHI_VALIDATION
    /// @copydoc ViewableResource::getValidationTrackerResource()
    [[nodiscard]] virtual validation::Resource* getValidationTrackerResource() noexcept override
    {
        return &m_validationResource;
    }
#endif

protected:
    /// @brief Adopts tracked barrier state and `BufferDesc` from `other` during backing-allocation transfer.
    /// @param[in,out] other Source buffer whose GPU allocation is being adopted.
    void takeOwnership(Buffer& other) noexcept;

    /// @brief Resets tracked barrier state to `Unknown` and clears `m_desc` to `BufferDesc::null()`.
    void releaseOwnership() noexcept;

private:
    /// @brief Layout descriptor describing this buffer's GPU allocation.
    BufferDesc m_desc{};

#if GP_ENABLE_RHI_VALIDATION
    validation::BufferResource m_validationResource{};
#endif
};

}   // namespace gp::rhi::resource
