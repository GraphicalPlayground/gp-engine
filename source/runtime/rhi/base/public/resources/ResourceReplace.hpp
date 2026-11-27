// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "commands/CommandForward.hpp"
#include "containers/arrays/Vector.hpp"
#include "core/ResourceForward.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "diagnostics/AssertionMacros.hpp"
#include "RHIBase.hpp"
#include <utility>

namespace gp::rhi::resource
{

/// @brief Pairs a destination and source resource for atomic backing-memory replacement via `replaceResources()`.
/// @details Used by pool defragmentation and streaming reallocation to transfer ownership of a newly allocated source
///          resource's backing GPU memory into an existing destination resource handle (or release the destination's
///          backing memory when `src == nullptr`) and refresh all linked shader views on the RHI timeline.
struct ResourceReplaceInfo
{
public:
    /// @brief Typed `(dst, src)` resource pointer pair.
    /// @tparam InResourceType Concrete RHI resource class (`Buffer` or `RayTracingGeometry`).
    template <typename InResourceType>
    struct Pair
    {
        /// @brief Destination resource receiving backing storage from `src`.
        InResourceType* dst{ nullptr };

        /// @brief Source resource donating backing storage to `dst` (or `nullptr` to release `dst` storage).
        InResourceType* src{ nullptr };

        /// @brief Constructs an empty resource pair.
        constexpr Pair() noexcept = default;

        /// @brief Constructs a `(dst, src)` replacement pair.
        /// @param[in] inDst Destination resource pointer.
        /// @param[in] inSrc Source resource pointer (may be `nullptr`).
        constexpr Pair(InResourceType* const inDst, InResourceType* const inSrc) noexcept
            : dst(inDst)
            , src(inSrc)
        {}

        /// @brief Default memberwise equality comparison.
        /// @param[in] rhs Pair to compare against.
        /// @return True when both pointers match.
        [[nodiscard]] constexpr bool operator==(const Pair& rhs) const noexcept = default;
    };

    /// @brief Typed `(dst, src)` pair for GPU buffer replacement.
    using BufferPair = Pair<Buffer>;

    /// @brief Typed `(dst, src)` pair for bottom-level ray tracing geometry replacement.
    using RTGeometryPair = Pair<RayTracingGeometry>;

    /// @brief Concrete resource kind stored by this replacement entry.
    enum class Type : gp::UInt8
    {
        Buffer = 0,
        RTGeometry = 1
    };

    /// @brief Alias retained for backend switch statements inspecting `storageType`.
    using StorageType = Type;

public:
    /// @brief Active resource pair kind.
    StorageType storageType{ StorageType::Buffer };

    /// @brief Destination buffer receiving backing memory when `storageType == Type::Buffer`.
    Buffer* dstBuffer{ nullptr };

    /// @brief Source buffer donating backing memory when `storageType == Type::Buffer`.
    Buffer* srcBuffer{ nullptr };

    /// @brief Destination ray tracing geometry receiving backing memory when `storageType == Type::RTGeometry`.
    RayTracingGeometry* dstGeometry{ nullptr };

    /// @brief Source ray tracing geometry donating backing memory when `storageType == Type::RTGeometry`.
    RayTracingGeometry* srcGeometry{ nullptr };

public:
    /// @brief Constructs an empty buffer replacement record.
    constexpr ResourceReplaceInfo() noexcept = default;

    /// @brief Constructs a GPU buffer replacement record.
    /// @param[in] inDstBuffer Destination buffer handle.
    /// @param[in] inSrcBuffer Source buffer handle (or `nullptr` to release `inDstBuffer` storage).
    constexpr ResourceReplaceInfo(Buffer* const inDstBuffer, Buffer* const inSrcBuffer) noexcept
        : storageType(Type::Buffer)
        , dstBuffer(inDstBuffer)
        , srcBuffer(inSrcBuffer)
    {}

    /// @brief Constructs a ray tracing geometry replacement record.
    /// @param[in] inDstGeometry Destination ray tracing geometry handle.
    /// @param[in] inSrcGeometry Source ray tracing geometry handle (or `nullptr` to release storage).
    constexpr ResourceReplaceInfo(
        RayTracingGeometry* const inDstGeometry, RayTracingGeometry* const inSrcGeometry
    ) noexcept
        : storageType(Type::RTGeometry)
        , dstGeometry(inDstGeometry)
        , srcGeometry(inSrcGeometry)
    {}

    /// @brief Returns the active resource kind stored in this record.
    /// @return `Type::Buffer` or `Type::RTGeometry`.
    [[nodiscard]] constexpr Type getType() const noexcept
    {
        return storageType;
    }

    /// @brief Returns the `(dst, src)` buffer pair (asserts `getType() == Type::Buffer`).
    /// @return `BufferPair` holding `dstBuffer` and `srcBuffer`.
    [[nodiscard]] constexpr BufferPair getBuffer() const noexcept
    {
        GP_CHECK(storageType == Type::Buffer);
        return BufferPair(dstBuffer, srcBuffer);
    }

    /// @brief Returns the `(dst, src)` ray tracing geometry pair (asserts `getType() == Type::RTGeometry`).
    /// @return `RTGeometryPair` holding `dstGeometry` and `srcGeometry`.
    [[nodiscard]] constexpr RTGeometryPair getRTGeometry() const noexcept
    {
        GP_CHECK(storageType == Type::RTGeometry);
        return RTGeometryPair(dstGeometry, srcGeometry);
    }

    /// @brief Default memberwise equality comparison.
    /// @param[in] rhs Replacement record to compare against.
    /// @return True when type and resource pointers match.
    [[nodiscard]] constexpr bool operator==(const ResourceReplaceInfo& rhs) const noexcept = default;
};

/// @brief RAII helper that accumulates `ResourceReplaceInfo` entries and submits them in one `replaceResources()`
/// batch.
/// @details Amortizes command packet recording and linked-view refresh overhead when defragmenting or streaming a batch
///          of buffers or ray tracing geometries. Automatically flushes pending entries on scope exit.
class GP_RHI_API ResourceReplaceBatcher final
{
public:
    /// @brief Constructs a batcher bound to `cmdList` with optional preallocated capacity.
    /// @param[in,out] cmdList Target command list receiving the batched `replaceResources()` command.
    /// @param[in] initialCapacity Optional number of `ResourceReplaceInfo` entries to reserve upfront.
    explicit ResourceReplaceBatcher(command::CommandListBase& cmdList, gp::UInt32 initialCapacity = 0u) noexcept;

    ResourceReplaceBatcher(const ResourceReplaceBatcher&) = delete;
    ResourceReplaceBatcher& operator=(const ResourceReplaceBatcher&) = delete;
    ResourceReplaceBatcher(ResourceReplaceBatcher&&) = delete;
    ResourceReplaceBatcher& operator=(ResourceReplaceBatcher&&) = delete;

    /// @brief Flushes any remaining queued replacement records to `m_cmdList`.
    ~ResourceReplaceBatcher() noexcept;

    /// @brief Appends a replacement pair constructed in-place from `args`.
    /// @tparam InArgs Constructor argument types forwarded to `ResourceReplaceInfo`.
    /// @param[in] args `(dst, src)` arguments for `ResourceReplaceInfo`.
    template <typename... InArgs>
    void enqueueReplace(InArgs&&... args) noexcept
    {
        m_infos.emplaceBack(std::forward<InArgs>(args)...);
    }

    /// @brief Submits all queued replacement entries to the bound command list immediately and clears the queue.
    void flush() noexcept;

    /// @brief Returns the number of queued replacement records awaiting flush.
    /// @return Pending entry count.
    [[nodiscard]] GP_FORCEINLINE_HINT gp::USize size() const noexcept
    {
        return m_infos.size();
    }

    /// @brief Reports whether any replacement records are currently queued.
    /// @return True when `size() == 0`.
    [[nodiscard]] GP_FORCEINLINE_HINT bool isEmpty() const noexcept
    {
        return m_infos.isEmpty();
    }

private:
    command::CommandListBase& m_cmdList;
    gp::Vector<ResourceReplaceInfo> m_infos{};
};

}   // namespace gp::rhi::resource
