// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "core/Resource.hpp"
#include "CoreMinimal.hpp"
#include "memory/pointers/RefCountedPtr.hpp"   // IWYU pragma: keep
#include "RHIBase.hpp"
#include "sync/RenderQuery.hpp"
#include "sync/SyncForward.hpp"   // IWYU pragma: keep

namespace gp::rhi::sync
{

/// @brief Move-only RAII lease for a `RenderQuery` allocated from a `RenderQueryPool`.
/// @details Automatically returns the owned `RenderQuery` to its originating pool on destruction or move assignment so
///          callers cannot leak pooled query slots.
class GP_RHI_API PooledRenderQuery final
{
public:
    /// @brief Constructs an empty handle owning no query.
    PooledRenderQuery() noexcept;

    /// @brief Constructs a lease binding `query` to its owning `queryPool`.
    /// @param[in] queryPool Owning pool that receives `query` upon release (may be null for unpooled queries).
    /// @param[in,out] query Intrusive reference to the leased `RenderQuery`, moved into this handle.
    PooledRenderQuery(RenderQueryPool* queryPool, RenderQueryRef&& query) noexcept;

    /// @brief Destructor that returns any held query back to its owning pool.
    ~PooledRenderQuery() noexcept;

    PooledRenderQuery(const PooledRenderQuery&) = delete;
    PooledRenderQuery& operator=(const PooledRenderQuery&) = delete;

    /// @brief Move constructor transferring query ownership from `other`.
    /// @param[in,out] other Source handle to move from; left empty with a null pool pointer.
    PooledRenderQuery(PooledRenderQuery&& other) noexcept;

    /// @brief Move assignment operator releasing any currently held query before taking over `other`.
    /// @param[in,out] other Source handle to move from; left empty with a null pool pointer.
    /// @return Reference to `*this`.
    PooledRenderQuery& operator=(PooledRenderQuery&& other) noexcept;

public:
    /// @brief Reports whether this handle currently owns a valid `RenderQuery`.
    /// @return True if the underlying `RenderQueryRef` is non-null; false otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT bool isValid() const noexcept
    {
        return m_query.isValid();
    }

    /// @brief Returns the raw pointer to the leased `RenderQuery`.
    /// @return Pointer to the owned `RenderQuery`, or `nullptr` if empty.
    [[nodiscard]] GP_FORCEINLINE_HINT RenderQuery* getQuery() const noexcept
    {
        return m_query.get();
    }

    /// @brief Returns the held `RenderQuery` to its owning pool and clears this handle.
    /// @details Safe to call on an already-empty handle. Asserts that the pool consumes the moved `RenderQueryRef`.
    void releaseQuery() noexcept;

private:
    /// @brief Intrusive reference to the leased query object.
    RenderQueryRef m_query{};

    /// @brief Non-owning pointer to the pool responsible for recycling `m_query`.
    RenderQueryPool* m_queryPool{ nullptr };
};

/// @brief Pool resource managing allocation and reuse of `RenderQuery` objects.
/// @details Backends derive from this class to recycle native query slots across frames without per-frame heap
///          allocations or API object churn.
class GP_RHI_API RenderQueryPool : public resource::Resource
{
    friend class PooledRenderQuery;

public:
    /// @brief Constructs a query pool tagged with `ResourceType::RenderQueryPool`.
    RenderQueryPool() noexcept;

    RenderQueryPool(const RenderQueryPool&) = delete;
    RenderQueryPool& operator=(const RenderQueryPool&) = delete;
    RenderQueryPool(RenderQueryPool&&) = delete;
    RenderQueryPool& operator=(RenderQueryPool&&) = delete;

public:
    /// @brief Leases an available `RenderQuery` from the pool, allocating a new one if none are free.
    /// @return Move-only `PooledRenderQuery` that returns the query to this pool when released.
    [[nodiscard]] virtual PooledRenderQuery allocateQuery() noexcept = 0;

protected:
    /// @copydoc resource::Resource::~Resource()
    virtual ~RenderQueryPool() noexcept override;

private:
    /// @brief Returns a leased `RenderQuery` back to the pool for subsequent reuse.
    /// @note Invoked exclusively by `PooledRenderQuery::releaseQuery()`. Implementations must move from or reset
    ///       `query` before returning.
    /// @param[in,out] query Intrusive reference to the query being recycled.
    virtual void releaseQuery(RenderQueryRef&& query) noexcept = 0;
};

}   // namespace gp::rhi::sync
