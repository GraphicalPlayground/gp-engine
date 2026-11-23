// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/strings/Name.hpp"
#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "core/GPUMask.hpp"
#include "core/ResourceForward.hpp"          // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "resources/BufferDesc.hpp"
#include "resources/BufferEnums.hpp"
#include "sync/SyncEnums.hpp"

namespace gp::rhi::resource
{

/// @brief Fluent creation parameters for allocating and initializing a GPU `Buffer`.
/// @details Extends `BufferDesc` with debug/owner names, initial barrier state, and memory initialization policy.
///          Prefer the typed static factories (`createVertex`, `createIndex`, `createStructured`, `createByteAddress`,
///          `createUniform`, `createNull`) followed by chained `set*` / `add*` calls.
struct BufferCreateDesc : public BufferDesc
{
    /// @brief Human-readable debug label attached to the GPU buffer.
    gp::StringView debugName{};

    /// @brief Optional CPU upload array consumed when `initAction == BufferInitAction::ResourceArray`.
    IResourceArrayUpload* initialData{ nullptr };

    /// @brief Initial GPU resource access state recorded for barrier tracking.
    sync::ResourceAccess initialState{ sync::ResourceAccess::Unknown };

    /// @brief Policy governing how the buffer's initial bytes are populated.
    BufferInitAction initAction{ BufferInitAction::Default };

    /// @brief Diagnostic class name reported to memory profilers and trace tools.
    gp::Name className{};

    /// @brief Diagnostic owner identifier forwarded to `Resource::setOwnerName()`.
    gp::Name ownerName{};

    /// @brief Constructs an empty buffer creation descriptor.
    BufferCreateDesc() noexcept = default;

    /// @brief Constructs a creation descriptor with a debug name and usage flags.
    /// @param[in] inDebugName Debug label for diagnostics.
    /// @param[in] inUsage Initial buffer usage flags.
    BufferCreateDesc(const gp::StringView inDebugName, const BufferUsageFlags inUsage) noexcept
        : BufferDesc(0u, 0u, inUsage)
        , debugName(inDebugName)
    {}

    /// @brief Constructs a creation descriptor with explicit name, byte size, stride, and usage flags.
    /// @param[in] inDebugName Debug label for diagnostics.
    /// @param[in] inSize Total allocation size in bytes.
    /// @param[in] inStride Element stride in bytes.
    /// @param[in] inUsage Buffer usage flags.
    BufferCreateDesc(
        const gp::StringView inDebugName,
        const gp::UInt32 inSize,
        const gp::UInt32 inStride,
        const BufferUsageFlags inUsage
    ) noexcept
        : BufferDesc(inSize, inStride, inUsage)
        , debugName(inDebugName)
    {}

    /// @brief Constructs a creation descriptor from a debug name and an existing `BufferDesc`.
    /// @param[in] inDebugName Debug label for diagnostics.
    /// @param[in] inDesc Base buffer layout descriptor to copy.
    BufferCreateDesc(const gp::StringView inDebugName, const BufferDesc& inDesc) noexcept
        : BufferDesc(inDesc)
        , debugName(inDebugName)
    {}

    /// @brief Creates a buffer creation descriptor with `inUsage`.
    /// @param[in] inDebugName Debug label.
    /// @param[in] inUsage Buffer usage flags.
    /// @return Initialized `BufferCreateDesc`.
    [[nodiscard]] static BufferCreateDesc
        create(const gp::StringView inDebugName, const BufferUsageFlags inUsage) noexcept
    {
        return BufferCreateDesc(inDebugName, inUsage);
    }

    /// @brief Creates a buffer creation descriptor with explicit size, stride, and usage.
    /// @param[in] inDebugName Debug label.
    /// @param[in] inSize Total byte size.
    /// @param[in] inStride Element stride in bytes.
    /// @param[in] inUsage Buffer usage flags.
    /// @return Initialized `BufferCreateDesc`.
    [[nodiscard]] static BufferCreateDesc create(
        const gp::StringView inDebugName,
        const gp::UInt32 inSize,
        const gp::UInt32 inStride,
        const BufferUsageFlags inUsage
    ) noexcept
    {
        return BufferCreateDesc(inDebugName, inSize, inStride, inUsage);
    }

    /// @brief Creates a buffer creation descriptor copying `inDesc`.
    /// @param[in] inDebugName Debug label.
    /// @param[in] inDesc Source buffer layout descriptor.
    /// @return Initialized `BufferCreateDesc`.
    [[nodiscard]] static BufferCreateDesc create(const gp::StringView inDebugName, const BufferDesc& inDesc) noexcept
    {
        return BufferCreateDesc(inDebugName, inDesc);
    }

    /// @brief Creates a descriptor for a null placeholder buffer without GPU memory backing.
    /// @param[in] inDebugName Debug label.
    /// @return Null-resource `BufferCreateDesc`.
    [[nodiscard]] static BufferCreateDesc createNull(const gp::StringView inDebugName) noexcept
    {
        return create(inDebugName, 0u, 0u, BufferUsageFlags::NullResource);
    }

    /// @brief Creates a vertex buffer descriptor with zero initial size.
    /// @param[in] inDebugName Debug label.
    /// @return Vertex buffer `BufferCreateDesc`.
    [[nodiscard]] static BufferCreateDesc createVertex(const gp::StringView inDebugName) noexcept
    {
        return create(inDebugName, BufferUsageFlags::VertexBuffer);
    }

    /// @brief Creates a vertex buffer descriptor with an explicit byte size.
    /// @param[in] inDebugName Debug label.
    /// @param[in] inSize Total vertex buffer size in bytes.
    /// @return Vertex buffer `BufferCreateDesc`.
    [[nodiscard]] static BufferCreateDesc
        createVertex(const gp::StringView inDebugName, const gp::UInt32 inSize) noexcept
    {
        return create(inDebugName, inSize, 0u, BufferUsageFlags::VertexBuffer);
    }

    /// @brief Creates a vertex buffer descriptor sized for `inCount` elements of `InVertexType`.
    /// @tparam InVertexType Vertex structure type.
    /// @param[in] inDebugName Debug label.
    /// @param[in] inCount Number of vertices.
    /// @return Vertex buffer `BufferCreateDesc`.
    template <typename InVertexType>
    [[nodiscard]] static BufferCreateDesc
        createVertex(const gp::StringView inDebugName, const gp::UInt32 inCount) noexcept
    {
        return create(
            inDebugName, inCount * static_cast<gp::UInt32>(sizeof(InVertexType)), 0u, BufferUsageFlags::VertexBuffer
        );
    }

    /// @brief Creates an index buffer descriptor with zero initial size.
    /// @param[in] inDebugName Debug label.
    /// @return Index buffer `BufferCreateDesc`.
    [[nodiscard]] static BufferCreateDesc createIndex(const gp::StringView inDebugName) noexcept
    {
        return create(inDebugName, BufferUsageFlags::IndexBuffer);
    }

    /// @brief Creates an index buffer descriptor with explicit byte size and index stride.
    /// @param[in] inDebugName Debug label.
    /// @param[in] inSize Total index buffer size in bytes.
    /// @param[in] inStride Size of one index in bytes (typically 2 or 4).
    /// @return Index buffer `BufferCreateDesc`.
    [[nodiscard]] static BufferCreateDesc
        createIndex(const gp::StringView inDebugName, const gp::UInt32 inSize, const gp::UInt32 inStride) noexcept
    {
        return create(inDebugName, inSize, inStride, BufferUsageFlags::IndexBuffer);
    }

    /// @brief Creates an index buffer descriptor sized for `inCount` indices of `InIndexType`.
    /// @tparam InIndexType Index integer type (`gp::UInt16` or `gp::UInt32`).
    /// @param[in] inDebugName Debug label.
    /// @param[in] inCount Number of indices.
    /// @return Index buffer `BufferCreateDesc`.
    template <typename InIndexType>
    [[nodiscard]] static BufferCreateDesc
        createIndex(const gp::StringView inDebugName, const gp::UInt32 inCount) noexcept
    {
        return create(
            inDebugName,
            inCount * static_cast<gp::UInt32>(sizeof(InIndexType)),
            static_cast<gp::UInt32>(sizeof(InIndexType)),
            BufferUsageFlags::IndexBuffer
        );
    }

    /// @brief Creates a structured buffer descriptor with zero initial size.
    /// @param[in] inDebugName Debug label.
    /// @return Structured buffer `BufferCreateDesc`.
    [[nodiscard]] static BufferCreateDesc createStructured(const gp::StringView inDebugName) noexcept
    {
        return create(inDebugName, BufferUsageFlags::StructuredBuffer);
    }

    /// @brief Creates a structured buffer descriptor with explicit byte size and element stride.
    /// @param[in] inDebugName Debug label.
    /// @param[in] inSize Total buffer size in bytes.
    /// @param[in] inStride Size of one structured element in bytes.
    /// @return Structured buffer `BufferCreateDesc`.
    [[nodiscard]] static BufferCreateDesc
        createStructured(const gp::StringView inDebugName, const gp::UInt32 inSize, const gp::UInt32 inStride) noexcept
    {
        return create(inDebugName, inSize, inStride, BufferUsageFlags::StructuredBuffer);
    }

    /// @brief Creates a structured buffer descriptor sized for `inCount` elements of `InStructureType`.
    /// @tparam InStructureType Structured element type.
    /// @param[in] inDebugName Debug label.
    /// @param[in] inCount Number of structured elements.
    /// @return Structured buffer `BufferCreateDesc`.
    template <typename InStructureType>
    [[nodiscard]] static BufferCreateDesc
        createStructured(const gp::StringView inDebugName, const gp::UInt32 inCount) noexcept
    {
        return create(
            inDebugName,
            inCount * static_cast<gp::UInt32>(sizeof(InStructureType)),
            static_cast<gp::UInt32>(sizeof(InStructureType)),
            BufferUsageFlags::StructuredBuffer
        );
    }

    /// @brief Creates a raw byte-address buffer descriptor with zero initial size.
    /// @param[in] inDebugName Debug label.
    /// @return Byte-address buffer `BufferCreateDesc`.
    [[nodiscard]] static BufferCreateDesc createByteAddress(const gp::StringView inDebugName) noexcept
    {
        return create(inDebugName, BufferUsageFlags::ByteAddressBuffer);
    }

    /// @brief Creates a raw byte-address buffer descriptor with explicit byte size and stride.
    /// @param[in] inDebugName Debug label.
    /// @param[in] inSize Total buffer size in bytes.
    /// @param[in] inStride Element stride in bytes.
    /// @return Byte-address buffer `BufferCreateDesc`.
    [[nodiscard]] static BufferCreateDesc
        createByteAddress(const gp::StringView inDebugName, const gp::UInt32 inSize, const gp::UInt32 inStride) noexcept
    {
        return create(inDebugName, inSize, inStride, BufferUsageFlags::ByteAddressBuffer);
    }

    /// @brief Creates a uniform-buffer-usage GPU buffer descriptor.
    /// @param[in] inDebugName Debug label.
    /// @return Uniform buffer `BufferCreateDesc`.
    [[nodiscard]] static BufferCreateDesc createUniform(const gp::StringView inDebugName) noexcept
    {
        return create(inDebugName, BufferUsageFlags::UniformBuffer);
    }

    /// @brief Sets the buffer allocation size in bytes.
    /// @param[in] inSize Total size in bytes.
    /// @return Reference to `*this`.
    BufferCreateDesc& setSize(const gp::UInt32 inSize) noexcept
    {
        size = inSize;
        return *this;
    }

    /// @brief Sets the element stride in bytes.
    /// @param[in] inStride Element stride in bytes.
    /// @return Reference to `*this`.
    BufferCreateDesc& setStride(const gp::UInt32 inStride) noexcept
    {
        stride = inStride;
        return *this;
    }

    /// @brief Replaces all usage flags on this descriptor.
    /// @param[in] inUsage New usage flag bitmask.
    /// @return Reference to `*this`.
    BufferCreateDesc& setUsage(const BufferUsageFlags inUsage) noexcept
    {
        usage = inUsage;
        return *this;
    }

    /// @brief Adds usage flags into the existing `usage` bitmask.
    /// @param[in] inUsage Additional flags to OR into `usage`.
    /// @return Reference to `*this`.
    BufferCreateDesc& addUsage(const BufferUsageFlags inUsage) noexcept
    {
        usage |= inUsage;
        return *this;
    }

    /// @brief Sets the multi-GPU node affinity mask.
    /// @param[in] inGPUMask GPU node mask.
    /// @return Reference to `*this`.
    BufferCreateDesc& setGPUMask(const GPUMask inGPUMask) noexcept
    {
        gpuMask = inGPUMask;
        return *this;
    }

    /// @brief Sets the debug label associated with the buffer.
    /// @param[in] inDebugName Debug label string view.
    /// @return Reference to `*this`.
    BufferCreateDesc& setDebugName(const gp::StringView inDebugName) noexcept
    {
        debugName = inDebugName;
        return *this;
    }

    /// @brief Sets the explicit initial GPU access state for barrier tracking.
    /// @param[in] inInitialState Initial `sync::ResourceAccess` mask.
    /// @return Reference to `*this`.
    BufferCreateDesc& setInitialState(const sync::ResourceAccess inInitialState) noexcept
    {
        initialState = inInitialState;
        return *this;
    }

    /// @brief Resolves `initialState` from `usage` when `initialState` is still `ResourceAccess::Unknown`.
    /// @return Reference to `*this`.
    BufferCreateDesc& determineInitialState() noexcept
    {
        if (initialState == sync::ResourceAccess::Unknown)
        {
            initialState = getDefaultResourceState(usage, false);
        }
        return *this;
    }

    /// @brief Sets the initialization action policy.
    /// @param[in] inInitAction Desired `BufferInitAction`.
    /// @return Reference to `*this`.
    BufferCreateDesc& setInitAction(const BufferInitAction inInitAction) noexcept
    {
        initAction = inInitAction;
        return *this;
    }

    /// @brief Sets the diagnostic class name for asset/memory tracing.
    /// @param[in] inClassName Class identifier.
    /// @return Reference to `*this`.
    BufferCreateDesc& setClassName(const gp::Name inClassName) noexcept
    {
        className = inClassName;
        return *this;
    }

    /// @brief Sets the diagnostic owner name for resource tracking.
    /// @param[in] inOwnerName Owner identifier.
    /// @return Reference to `*this`.
    BufferCreateDesc& setOwnerName(const gp::Name inOwnerName) noexcept
    {
        ownerName = inOwnerName;
        return *this;
    }

    /// @brief Selects default initialization (contents left undefined until written).
    /// @return Reference to `*this`.
    BufferCreateDesc& setInitActionNone() noexcept
    {
        return setInitAction(BufferInitAction::Default);
    }

    /// @brief Selects zero-initialization for the entire buffer allocation.
    /// @return Reference to `*this`.
    BufferCreateDesc& setInitActionZeroData() noexcept
    {
        return setInitAction(BufferInitAction::Zeroed);
    }

    /// @brief Selects caller-driven initialization through a buffer initializer handle.
    /// @return Reference to `*this`.
    BufferCreateDesc& setInitActionInitializer() noexcept
    {
        return setInitAction(BufferInitAction::Initializer);
    }

    /// @brief Selects upload from `inInitialData` and switches `initAction` to `BufferInitAction::ResourceArray`.
    /// @param[in] inInitialData Upload data provider (discarded by the backend after upload).
    /// @return Reference to `*this`.
    BufferCreateDesc& setInitActionResourceArray(IResourceArrayUpload* const inInitialData) noexcept
    {
        initialData = inInitialData;
        return setInitAction(BufferInitAction::ResourceArray);
    }

    /// @brief Returns `className` when set, or `"Buffer"` as a default trace class identifier.
    /// @return Non-empty diagnostic `gp::Name`.
    [[nodiscard]] gp::Name getTraceClassName() const noexcept
    {
        return !className.isNone() ? className : gp::Name("Buffer");
    }
};

}   // namespace gp::rhi::resource
