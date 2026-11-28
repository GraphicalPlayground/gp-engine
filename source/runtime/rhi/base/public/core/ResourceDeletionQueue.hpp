// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/ContainerForward.hpp"
#include "core/ResourceForward.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "RHIBase.hpp"

namespace gp::rhi::resource
{

/// @brief Coordinates deferred RHI resource deletion queues and render-thread lifetime extensions.
/// @details Exposes the batch gather, destroy, flush, and lifetime-extension operations used by `DynamicRHI`,
///          `CommandListImmediate`, and `CommandListExecutor`. Uses a two-queue retirement split: resources that allow
///          lifetime extension wait until `getLifetimeRefCount() == 0`, while resources that called
///          `disableLifetimeExtension()` drain on every flush.
class GP_RHI_API ResourceDeletionQueue final
{
public:
    /// @brief Deleted default constructor; use the static deletion queue operations.
    ResourceDeletionQueue() = delete;

public:
    /// @brief Drains pending RHI resources into `outResources` in LIFO order.
    /// @param[in,out] outResources Destination vector appended with queued resource pointers.
    /// @param[in] includeExtendedLifetimeResources When true, drains both the immediate and extended-lifetime queues.
    static void
        gatherResourcesToDelete(gp::Vector<Resource*>& outResources, bool includeExtendedLifetimeResources) noexcept;

    /// @brief Destroys every resource in `resources` whose reference count remained zero.
    /// @details Resources resurrected by a cache before this call have `MarkedForDelete` cleared and stay alive.
    /// @param[in] resources Batch of candidate resources returned by `gatherResourcesToDelete()`.
    static void deleteResources(gp::ConstVectorView<Resource*> resources) noexcept;

    /// @brief Repeatedly gathers and deletes pending resources until cascading destructors enqueue no more work.
    /// @details Deleting a composite resource (such as a pipeline state or uniform buffer) can release its last
    ///          references to child resources. Looping until empty drains the full dependency chain cleanly.
    /// @param[in] includeExtendedLifetimeResources When true, also drains the extended-lifetime queue on each pass.
    static void flushPendingDeletes(bool includeExtendedLifetimeResources = true) noexcept;

    /// @brief Increments the render-thread lifetime-extension reference count.
    /// @details While the count is positive, resources with `allowsLifetimeExtension() == true` stay in the
    ///          extended-lifetime pending queue instead of being collected by ordinary flushes.
    /// @note Must be called on the render thread (`gp::thread::isInRenderThread()`).
    /// @param[in] numRefs Positive number of lifetime references to add (defaults to `1`).
    static void addLifetimeRef(gp::Int32 numRefs = 1) noexcept;

    /// @brief Decrements the render-thread lifetime-extension reference count.
    /// @note Must be called on the render thread (`gp::thread::isInRenderThread()`).
    /// @param[in] numRefs Positive number of lifetime references to release (defaults to `1`).
    /// @return True if the reference count reached zero and the caller should dispatch a deletion flush.
    [[nodiscard]] static bool releaseLifetimeRef(gp::Int32 numRefs = 1) noexcept;

    /// @brief Returns the current render-thread lifetime-extension reference count.
    /// @return Non-negative active lifetime extension count.
    [[nodiscard]] static gp::Int32 getLifetimeRefCount() noexcept;

    /// @brief Reports whether a deletion flush should drain extended-lifetime resources.
    /// @return True when `getLifetimeRefCount() == 0`; false while at least one extension scope is active.
    [[nodiscard]] static bool shouldIncludeExtendedLifetimeResources() noexcept;

    /// @brief Enables or disables the engine-exit bypass for `Resource::~Resource()` deletion-queue checks.
    /// @param[in] bypass True during final process teardown; false during normal execution.
    static void setBypassDeletionQueueCheckOnExit(bool bypass) noexcept;

    /// @brief Reports whether the deletion-queue destructor guard is currently bypassed for shutdown.
    /// @return True if exit bypass is active; false otherwise.
    [[nodiscard]] static bool isBypassingDeletionQueueCheckOnExit() noexcept;
};

/// @brief RAII scope guard that extends the lifetime of eligible RHI resources on the render thread.
/// @details Pairs `ResourceDeletionQueue::addLifetimeRef()` on construction with
///          `ResourceDeletionQueue::releaseLifetimeRef()` on destruction.
/// @note Must be constructed and destroyed on the render thread.
class GP_RHI_API ScopedResourceLifetimeExtension final
{
public:
    /// @brief Acquires one lifetime-extension reference on the render thread.
    ScopedResourceLifetimeExtension() noexcept;

    /// @brief Releases the lifetime-extension reference acquired by this scope.
    ~ScopedResourceLifetimeExtension() noexcept;

    ScopedResourceLifetimeExtension(const ScopedResourceLifetimeExtension&) = delete;
    ScopedResourceLifetimeExtension& operator=(const ScopedResourceLifetimeExtension&) = delete;
    ScopedResourceLifetimeExtension(ScopedResourceLifetimeExtension&&) = delete;
    ScopedResourceLifetimeExtension& operator=(ScopedResourceLifetimeExtension&&) = delete;
};

}   // namespace gp::rhi::resource
