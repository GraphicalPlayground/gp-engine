// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "commands/CommandForward.hpp"
#include "CoreMinimal.hpp"
#include "memory/MemoryBase.hpp"
#include "memory/MemoryOrder.hpp"
#include "RHIBase.hpp"
#include "sync/SyncEnums.hpp"
#include "sync/SyncForward.hpp"   // IWYU pragma: keep
#include "threading/atomics/Atomic.hpp"

namespace gp::rhi::sync
{

/// @brief Backend allocation layout and lifecycle callbacks for trailing `Transition` private data.
struct TransitionBackendHooks
{
    /// @brief Callback invoked after `Transition` construction to populate backend private data.
    using CreateFn = void (*)(Transition* transition, const TransitionCreateInfo& createInfo) noexcept;

    /// @brief Callback invoked before `Transition` destruction to release backend resources.
    using ReleaseFn = void (*)(Transition* transition) noexcept;

    /// @brief Byte size of the backend-specific struct appended after each `Transition`.
    gp::USize privateDataSizeInBytes{ 0u };

    /// @brief Required power-of-two byte alignment for the trailing backend private data (`0` when size is zero).
    gp::USize privateDataAlignInBytes{ 0u };

    /// @brief Optional backend initialization callback invoked by `TransitionController::create()`.
    CreateFn onCreate{ nullptr };

    /// @brief Optional backend teardown callback invoked right before `Transition` cleanup frees memory.
    ReleaseFn onRelease{ nullptr };
};

/// @brief Opaque runtime object tracking the begin and end completion of an RHI pipeline barrier.
/// @details Each `Transition` allocation reserves aligned trailing bytes for the active graphics backend, accessed via
///          `getPrivateData<T>()`. Pending begin and end pipeline bits live in an atomic byte (`m_state`); the thread
///          that clears the final bit runs backend release and frees the allocation automatically.
class GP_RHI_API Transition final
{
    friend class ::gp::rhi::command::CommandListBase;
    friend class ::gp::rhi::command::ComputeCommandList;
    friend struct ::gp::rhi::command::CommandBeginTransitions;
    friend struct ::gp::rhi::command::CommandEndTransitions;
    friend struct ::gp::rhi::command::CommandResourceTransition;
    friend class TransitionController;

public:
    Transition(const Transition&) = delete;
    Transition& operator=(const Transition&) = delete;
    Transition(Transition&&) = delete;
    Transition& operator=(Transition&&) = delete;

public:
    /// @brief Returns a mutable pointer to the backend private data stored immediately after this object.
    /// @tparam InPrivateDataT Backend private-data struct type matching the configured size and alignment.
    /// @return Aligned pointer to the trailing backend payload.
    template <typename InPrivateDataT>
    [[nodiscard]] GP_FORCEINLINE_HINT InPrivateDataT* getPrivateData() noexcept
    {
        const gp::USize privateAlign = getPrivateDataAlignInBytes();
#if GP_DO_GUARD_SLOW
        const gp::USize privateSize = getPrivateDataSizeInBytes();
        GP_CHECK_SLOW(sizeof(InPrivateDataT) == privateSize && privateAlign != 0u);
#endif

        const gp::UIntPtr alignedAddr = gp::memory::align(reinterpret_cast<gp::UIntPtr>(this + 1), privateAlign);
#if GP_DO_GUARD_SLOW
        const gp::UIntPtr baseAddr = reinterpret_cast<gp::UIntPtr>(this);
        GP_CHECK_SLOW((alignedAddr + privateSize - baseAddr) == getTotalAllocationSize());
#endif
        return reinterpret_cast<InPrivateDataT*>(alignedAddr);
    }

    /// @brief Returns a read-only pointer to the backend private data stored immediately after this object.
    /// @tparam InPrivateDataT Backend private-data struct type matching the configured size and alignment.
    /// @return Aligned const pointer to the trailing backend payload.
    template <typename InPrivateDataT>
    [[nodiscard]] GP_FORCEINLINE_HINT const InPrivateDataT* getPrivateData() const noexcept
    {
        return const_cast<Transition*>(this)->getPrivateData<InPrivateDataT>();
    }

    /// @brief Reports whether this transition is permitted while a render pass is active.
    /// @return True when `TransitionCreateFlags::AllowDuringRenderPass` is set.
    [[nodiscard]] GP_FORCEINLINE_HINT bool allowInRenderPass() const noexcept
    {
        return (m_flags & TransitionCreateFlags::AllowDuringRenderPass) != TransitionCreateFlags::None;
    }

    /// @brief Returns the creation flags associated with this transition.
    /// @return `TransitionCreateFlags` bitmask.
    [[nodiscard]] GP_FORCEINLINE_HINT TransitionCreateFlags getFlags() const noexcept
    {
        return m_flags;
    }

    /// @brief Returns the remaining pending begin/end pipeline bitmask for diagnostics and testing.
    /// @return Packed bitmask (`src` bits in `[0..kPipelineStageCount-1]`, `dst` bits shifted left by
    ///         `kPipelineStageCount`).
    [[nodiscard]] GP_FORCEINLINE_HINT gp::UInt8 getPendingStateMask() const noexcept
    {
        return m_state.load(gp::memory::MemoryOrder::Acquire);
    }

private:
    /// @brief Constructs a transition tracking pending begin bits for `srcPipelines` and end bits for `dstPipelines`.
    /// @param[in] srcPipelines Source execution pipelines that must call `markBegin()`.
    /// @param[in] dstPipelines Destination execution pipelines that must call `markEnd()`.
    /// @param[in] inFlags Transition creation flags.
    Transition(
        const PipelineStage srcPipelines, const PipelineStage dstPipelines, const TransitionCreateFlags inFlags
    ) noexcept
        : m_state(
              static_cast<gp::UInt8>(
                  static_cast<gp::UInt32>(srcPipelines) | (static_cast<gp::UInt32>(dstPipelines) << kPipelineStageCount)
              )
          )
        , m_flags(inFlags)
#if GP_DO_CHECK
        , m_allowedSrc(srcPipelines)
        , m_allowedDst(dstPipelines)
#endif
    {}

    /// @brief Private destructor; memory is freed only by `cleanup()` once all begin and end masks clear.
    ~Transition() noexcept = default;

    /// @brief Clears the begin bits for `pipeline` and runs `cleanup()` if no pending bits remain.
    /// @param[in] pipeline Source pipeline stage completing its begin step.
    void markBegin(const PipelineStage pipeline) const noexcept
    {
        GP_CHECKF(pipeline != PipelineStage::None, "Transition::markBegin() requires a non-empty pipeline mask.");
#if GP_DO_CHECK
        GP_CHECKF(
            (m_allowedSrc & pipeline) == pipeline,
            "Transition begin called on a source pipeline that it was not created for."
        );
#endif

        const gp::UInt8 mask = static_cast<gp::UInt8>(pipeline);
        const gp::UInt8 previousValue =
            m_state.fetchAnd(static_cast<gp::UInt8>(~mask), gp::memory::MemoryOrder::AcqRel);
        GP_CHECKF(
            (previousValue & mask) == mask,
            "Transition begin was called twice on this transition for at least one pipeline."
        );

        if (previousValue == mask)
        {
            cleanup();
        }
    }

    /// @brief Clears the end bits for `pipeline` and runs `cleanup()` if no pending bits remain.
    /// @param[in] pipeline Destination pipeline stage completing its end step.
    void markEnd(const PipelineStage pipeline) const noexcept
    {
        GP_CHECKF(pipeline != PipelineStage::None, "Transition::markEnd() requires a non-empty pipeline mask.");
#if GP_DO_CHECK
        GP_CHECKF(
            (m_allowedDst & pipeline) == pipeline,
            "Transition end called on a destination pipeline that it was not created for."
        );
#endif

        const gp::UInt8 mask = static_cast<gp::UInt8>(static_cast<gp::UInt32>(pipeline) << kPipelineStageCount);
        const gp::UInt8 previousValue =
            m_state.fetchAnd(static_cast<gp::UInt8>(~mask), gp::memory::MemoryOrder::AcqRel);
        GP_CHECKF(
            (previousValue & mask) == mask,
            "Transition end was called twice on this transition for at least one pipeline."
        );

        if (previousValue == mask)
        {
            cleanup();
        }
    }

    /// @brief Invokes the registered backend release hook, destroys this object, and frees its allocation.
    void cleanup() const noexcept;

    /// @brief Returns the registered per-transition backend private payload size in bytes.
    /// @return Backend private data size in bytes.
    [[nodiscard]] static gp::USize getPrivateDataSizeInBytes() noexcept;

    /// @brief Returns the registered alignment requirement for backend private data.
    /// @return Backend private data alignment in bytes.
    [[nodiscard]] static gp::USize getPrivateDataAlignInBytes() noexcept;

    /// @brief Returns the combined byte size of a `Transition` header plus its aligned trailing backend payload.
    /// @return Total allocation size in bytes.
    [[nodiscard]] static gp::USize getTotalAllocationSize() noexcept;

    /// @brief Returns the allocation alignment satisfying both `Transition` and trailing backend private data.
    /// @return Allocation alignment in bytes.
    [[nodiscard]] static gp::USize getAlignment() noexcept;

private:
    static_assert(
        (kPipelineStageCount * 2u) <= (sizeof(gp::UInt8) * 8u), "Not enough bits in m_state to hold pipeline state."
    );

    mutable gp::thread::Atomic<gp::UInt8> m_state{ 0u };
    mutable TransitionCreateFlags m_flags{ TransitionCreateFlags::None };

#if GP_DO_CHECK
    mutable PipelineStage m_allowedSrc{ PipelineStage::None };
    mutable PipelineStage m_allowedDst{ PipelineStage::None };
#endif
};

/// @brief Coordinates allocation, backend private-data sizing, and begin/end retirement of `Transition` objects.
class GP_RHI_API TransitionController final
{
public:
    TransitionController() = delete;

public:
    /// @brief Registers backend private-data layout and lifecycle hooks for subsequently created transitions.
    /// @note Call during RHI backend startup when no transitions are in flight.
    /// @param[in] hooks Backend size, alignment, create, and release configuration.
    static void configureBackend(const TransitionBackendHooks& hooks) noexcept;

    /// @brief Clears backend private-data hooks back to zero-byte defaults.
    /// @note Call during RHI backend shutdown after all pending transitions have retired.
    static void resetBackend() noexcept;

    /// @brief Allocates and initializes a new `Transition` along with its aligned backend private data.
    /// @param[in] createInfo Source/destination pipelines, flags, transitions, and aliasing operations.
    /// @return Non-null pointer to the newly allocated `Transition`, or `nullptr` if allocation fails.
    [[nodiscard]] static Transition* create(const TransitionCreateInfo& createInfo) noexcept;

    /// @brief Marks the begin phase complete on `pipeline`; frees `transition` if it was the final pending step.
    /// @param[in] transition Active transition pointer.
    /// @param[in] pipeline Source pipeline stage executing the begin barrier.
    static void markBegin(const Transition* transition, PipelineStage pipeline) noexcept;

    /// @brief Marks the end phase complete on `pipeline`; frees `transition` if it was the final pending step.
    /// @param[in] transition Active transition pointer.
    /// @param[in] pipeline Destination pipeline stage executing the end barrier.
    static void markEnd(const Transition* transition, PipelineStage pipeline) noexcept;

    /// @brief Returns the configured trailing backend private-data size in bytes.
    /// @return Backend private-data byte size.
    [[nodiscard]] static gp::USize getPrivateDataSizeInBytes() noexcept;

    /// @brief Returns the configured trailing backend private-data alignment in bytes.
    /// @return Backend private-data alignment (`0` when private-data size is zero).
    [[nodiscard]] static gp::USize getPrivateDataAlignInBytes() noexcept;

    /// @brief Returns the total allocation byte size covering `Transition`, padding, and backend private data.
    /// @return Allocation size in bytes.
    [[nodiscard]] static gp::USize getTotalAllocationSize() noexcept;

    /// @brief Returns the required allocation alignment for a `Transition` block.
    /// @return Power-of-two alignment in bytes.
    [[nodiscard]] static gp::USize getAlignment() noexcept;
};

}   // namespace gp::rhi::sync
