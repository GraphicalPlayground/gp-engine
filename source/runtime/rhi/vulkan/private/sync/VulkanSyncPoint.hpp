// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "memory/pointers/SharedPtr.hpp"
#include "RHIVulkan.hpp"
#include "sync/VulkanSyncEnums.hpp"
#include "threading/sync/JobFence.hpp"

namespace gp::rhi::vulkan
{

class VulkanSyncPoint;

/// @brief Reference-counted handle to a `VulkanSyncPoint`.
using VulkanSyncPointRef = gp::SharedPtr<VulkanSyncPoint>;

/// @brief Optional callback invoked by `VulkanSyncPoint::wait()` to pump the interrupt queue while waiting.
/// @param[in] syncPoint Sync point being awaited.
using VulkanSyncPointWaitCallback = void (*)(const VulkanSyncPoint& syncPoint) noexcept;

/// @brief Logical one-shot point on a GPU queue timeline that can be polled or awaited from the CPU.
/// @details Wraps a zero-allocation `gp::thread::JobFence` completion barrier so submission and interrupt threads can
///          report payload completion to waiting CPU threads or fibers without heap overhead.
class GP_RHI_VULKAN_API VulkanSyncPoint final : public gp::SharedFromThis<VulkanSyncPoint>
{
public:
    /// @brief Constructs a sync point of `type` with an optional diagnostic label.
    /// @param[in] type Scheduling category (`GPUAndCPU` or `Context`).
    /// @param[in] debugName Optional static UTF-8 diagnostic label.
    explicit VulkanSyncPoint(
        VulkanSyncPointType type = VulkanSyncPointType::GPUAndCPU, const char* debugName = nullptr
    ) noexcept
        : m_type(type)
        , m_debugName((debugName != nullptr) ? debugName : "VulkanSyncPoint")
    {}

    /// @brief Default destructor.
    ~VulkanSyncPoint() noexcept = default;

    VulkanSyncPoint(const VulkanSyncPoint&) = delete;
    VulkanSyncPoint& operator=(const VulkanSyncPoint&) = delete;
    VulkanSyncPoint(VulkanSyncPoint&&) = delete;
    VulkanSyncPoint& operator=(VulkanSyncPoint&&) = delete;

    /// @brief Creates a new reference-counted `VulkanSyncPoint`.
    /// @param[in] type Scheduling category (`GPUAndCPU` or `Context`).
    /// @param[in] debugName Optional static UTF-8 diagnostic label.
    /// @return Newly allocated `VulkanSyncPointRef`.
    [[nodiscard]] static VulkanSyncPointRef create(VulkanSyncPointType type, const char* debugName = nullptr) noexcept;

    /// @brief Marks this sync point complete and wakes any waiting CPU threads or fibers.
    void signal() noexcept
    {
        m_cpuEvent.signal();
    }

    /// @brief Non-blocking poll checking whether this sync point has been signaled.
    /// @return True when complete; false otherwise.
    [[nodiscard]] bool isComplete() const noexcept
    {
        return m_cpuEvent.isSignaled();
    }

    /// @brief Blocks the calling thread until this sync point is signaled.
    /// @details Invokes the registered `VulkanSyncPointWaitCallback` (if any) to drive interrupt-queue pumping before
    ///          falling back to `m_cpuEvent.wait()`.
    void wait() const noexcept;

    /// @brief Returns the underlying `gp::thread::JobFence` completion barrier.
    /// @return Mutable reference to `m_cpuEvent`.
    [[nodiscard]] gp::thread::JobFence& getCpuEvent() noexcept
    {
        return m_cpuEvent;
    }

    /// @brief Returns the const underlying `gp::thread::JobFence` completion barrier.
    /// @return Const reference to `m_cpuEvent`.
    [[nodiscard]] const gp::thread::JobFence& getCpuEvent() const noexcept
    {
        return m_cpuEvent;
    }

    /// @brief Returns the sync point scheduling category.
    /// @return `VulkanSyncPointType`.
    [[nodiscard]] VulkanSyncPointType getType() const noexcept
    {
        return m_type;
    }

    /// @brief Returns the diagnostic label attached at creation.
    /// @return Null-terminated UTF-8 label string.
    [[nodiscard]] const char* getDebugName() const noexcept
    {
        return m_debugName;
    }

    /// @brief Installs a process-wide wait pump callback invoked during `wait()` (or `nullptr` to clear).
    /// @param[in] callback Wait pump function pointer, or `nullptr`.
    static void setWaitCallback(VulkanSyncPointWaitCallback callback) noexcept;

    /// @brief Returns the currently installed `VulkanSyncPointWaitCallback`, or `nullptr`.
    /// @return Active wait pump callback.
    [[nodiscard]] static VulkanSyncPointWaitCallback getWaitCallback() noexcept;

private:
    mutable gp::thread::JobFence m_cpuEvent{};
    const VulkanSyncPointType m_type{ VulkanSyncPointType::GPUAndCPU };
    const char* m_debugName{ "VulkanSyncPoint" };
};

}   // namespace gp::rhi::vulkan
