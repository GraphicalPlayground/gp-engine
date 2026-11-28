// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "core/ResourceEnums.hpp"
#include "core/ResourceForward.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "hashing/HashMix.hpp"
#include "sync/SyncEnums.hpp"
#include "sync/TransitionDesc.hpp"

namespace gp::rhi::resource
{

/// @brief Compact bitmask wrapper representing exclusive depth and stencil read/write access during a render pass.
/// @details Encodes depth access in the low 4 bits and stencil access in the high 4 bits (`ExclusiveDepthStencilMode`).
///          Supports converting depth/stencil usage into `sync::ResourceAccess` masks, enumerating per-plane
///          subresource transitions, computing minimal read/write delta transitions, and indexing cached DSV tables.
class ExclusiveDepthStencil final
{
public:
    /// @brief Underlying mode enum type.
    using Mode = ExclusiveDepthStencilMode;

    /// @brief Number of distinct DSV access view slots returned by `getIndex()`.
    static constexpr gp::UInt32 kMaxIndex = 4u;

    /// @brief Preset where neither depth nor stencil is used.
    static const ExclusiveDepthStencil kDepthNopStencilNop;

    /// @brief Preset where depth is read-only and stencil is unused.
    static const ExclusiveDepthStencil kDepthReadStencilNop;

    /// @brief Preset where depth is writable and stencil is unused.
    static const ExclusiveDepthStencil kDepthWriteStencilNop;

    /// @brief Preset where depth is unused and stencil is read-only.
    static const ExclusiveDepthStencil kDepthNopStencilRead;

    /// @brief Preset where both depth and stencil are read-only.
    static const ExclusiveDepthStencil kDepthReadStencilRead;

    /// @brief Preset where depth is writable and stencil is read-only.
    static const ExclusiveDepthStencil kDepthWriteStencilRead;

    /// @brief Preset where depth is unused and stencil is writable.
    static const ExclusiveDepthStencil kDepthNopStencilWrite;

    /// @brief Preset where depth is read-only and stencil is writable.
    static const ExclusiveDepthStencil kDepthReadStencilWrite;

    /// @brief Preset where both depth and stencil are writable.
    static const ExclusiveDepthStencil kDepthWriteStencilWrite;

public:
    /// @brief Constructs an `ExclusiveDepthStencil` state (defaults to `DepthNopStencilNop`).
    /// @param[in] inValue Combined depth and stencil access mode.
    constexpr explicit(false) ExclusiveDepthStencil(const Mode inValue = Mode::DepthNopStencilNop) noexcept
        : m_value(inValue)
    {}

    /// @brief Reports whether either depth or stencil is active.
    /// @return True when `m_value != Mode::DepthNopStencilNop`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool isUsingDepthStencil() const noexcept
    {
        return m_value != Mode::DepthNopStencilNop;
    }

    /// @brief Reports whether depth is read or written.
    /// @return True when the depth nibble is not `DepthNop`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool isUsingDepth() const noexcept
    {
        return extractDepth() != Mode::DepthNop;
    }

    /// @brief Reports whether stencil is read or written.
    /// @return True when the stencil nibble is not `StencilNop`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool isUsingStencil() const noexcept
    {
        return extractStencil() != Mode::StencilNop;
    }

    /// @brief Reports whether depth writes are enabled.
    /// @return True when the depth nibble equals `DepthWrite`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool isDepthWrite() const noexcept
    {
        return extractDepth() == Mode::DepthWrite;
    }

    /// @brief Reports whether depth is strictly read-only.
    /// @return True when the depth nibble equals `DepthRead`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool isDepthRead() const noexcept
    {
        return extractDepth() == Mode::DepthRead;
    }

    /// @brief Reports whether stencil writes are enabled.
    /// @return True when the stencil nibble equals `StencilWrite`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool isStencilWrite() const noexcept
    {
        return extractStencil() == Mode::StencilWrite;
    }

    /// @brief Reports whether stencil is strictly read-only.
    /// @return True when the stencil nibble equals `StencilRead`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool isStencilRead() const noexcept
    {
        return extractStencil() == Mode::StencilRead;
    }

    /// @brief Reports whether either depth or stencil enables writes.
    /// @return True when `isDepthWrite() || isStencilWrite()`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool isAnyWrite() const noexcept
    {
        return isDepthWrite() || isStencilWrite();
    }

    /// @brief Enables depth writes while preserving the current stencil mode.
    GP_FORCEINLINE_HINT constexpr void setDepthWrite() noexcept
    {
        m_value = combine(Mode::DepthWrite, extractStencil());
    }

    /// @brief Enables stencil writes while preserving the current depth mode.
    GP_FORCEINLINE_HINT constexpr void setStencilWrite() noexcept
    {
        m_value = combine(extractDepth(), Mode::StencilWrite);
    }

    /// @brief Resets the state and enables depth and/or stencil writes according to the flags.
    /// @param[in] enableDepth True to set `DepthWrite`; false to leave depth as `DepthNop`.
    /// @param[in] enableStencil True to set `StencilWrite`; false to leave stencil as `StencilNop`.
    GP_FORCEINLINE_HINT constexpr void setDepthStencilWrite(const bool enableDepth, const bool enableStencil) noexcept
    {
        m_value = Mode::DepthNopStencilNop;
        if (enableDepth)
        {
            setDepthWrite();
        }
        if (enableStencil)
        {
            setStencilWrite();
        }
    }

    /// @brief Checks whether this requested access state is compatible with the currently bound pass state.
    /// @details Any plane marked `Nop` in `*this` is unconstrained; active planes must match `current` exactly.
    /// @param[in] current Active depth-stencil state of the render pass.
    /// @return True when every non-nop plane matches `current`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool isValid(const ExclusiveDepthStencil& current) const noexcept
    {
        const Mode depth = extractDepth();
        if (depth != Mode::DepthNop && depth != current.extractDepth())
        {
            return false;
        }

        const Mode stencil = extractStencil();
        if (stencil != Mode::StencilNop && stencil != current.extractStencil())
        {
            return false;
        }

        return true;
    }

    /// @brief Computes separate `sync::ResourceAccess` masks for the depth and stencil planes.
    /// @details Read-only planes map to `ResourceAccess::DSVRead` (which may coexist with SRV sampling). Writable
    ///          planes map to `ResourceAccess::DSVRead | ResourceAccess::DSVWrite`. Unused planes map to `None`.
    /// @param[out] outDepthAccess Receives the access mask for the depth plane.
    /// @param[out] outStencilAccess Receives the access mask for the stencil plane.
    GP_FORCEINLINE_HINT constexpr void
        getAccess(sync::ResourceAccess& outDepthAccess, sync::ResourceAccess& outStencilAccess) const noexcept
    {
        constexpr sync::ResourceAccess kDsvReadOnlyMask = sync::ResourceAccess::DSVRead;
        constexpr sync::ResourceAccess kDsvReadWriteMask =
            sync::ResourceAccess::DSVRead | sync::ResourceAccess::DSVWrite;

        outDepthAccess = sync::ResourceAccess::None;
        if (isUsingDepth())
        {
            outDepthAccess = isDepthWrite() ? kDsvReadWriteMask : kDsvReadOnlyMask;
        }

        outStencilAccess = sync::ResourceAccess::None;
        if (isUsingStencil())
        {
            outStencilAccess = isStencilWrite() ? kDsvReadWriteMask : kDsvReadOnlyMask;
        }
    }

    /// @brief Invokes `callback(access, planeSlice)` for the minimal set of subresource planes required.
    /// @details Emits one call with `SubresourceRange::kAllSubresources` when depth and stencil share the same access
    ///          state, or separate calls with `kDepthPlaneSlice` and `kStencilPlaneSlice` when they differ. Emits no
    ///          calls when neither plane is in use.
    /// @tparam InFunction Callable accepting `(sync::ResourceAccess, gp::UInt16)`.
    /// @param[in] callback Visitor invoked for each required plane transition.
    template <typename InFunction>
    GP_FORCEINLINE_HINT constexpr void enumerateSubresources(InFunction&& callback) const noexcept
    {
        if (!isUsingDepthStencil())
        {
            return;
        }

        sync::ResourceAccess depthAccess = sync::ResourceAccess::None;
        sync::ResourceAccess stencilAccess = sync::ResourceAccess::None;
        getAccess(depthAccess, stencilAccess);

        if (depthAccess == stencilAccess)
        {
            callback(depthAccess, sync::SubresourceRange::kAllSubresources);
        }
        else
        {
            if (depthAccess != sync::ResourceAccess::None)
            {
                callback(depthAccess, sync::SubresourceRange::kDepthPlaneSlice);
            }
            if (stencilAccess != sync::ResourceAccess::None)
            {
                callback(stencilAccess, sync::SubresourceRange::kStencilPlaneSlice);
            }
        }
    }

    /// @brief Computes the minimal state needed to transition currently writable planes to readable.
    /// @details Planes that are not currently writable become `Nop` to avoid redundant subresource barriers.
    /// @return Delta `ExclusiveDepthStencil` targeting only planes leaving a write state.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr ExclusiveDepthStencil getReadableTransition() const noexcept
    {
        const Mode newDepth = isDepthWrite() ? Mode::DepthRead : Mode::DepthNop;
        const Mode newStencil = isStencilWrite() ? Mode::StencilRead : Mode::StencilNop;
        return ExclusiveDepthStencil(combine(newDepth, newStencil));
    }

    /// @brief Computes the minimal state needed to transition currently readable planes back to writable.
    /// @details Planes that are not currently read-only become `Nop` to avoid redundant subresource barriers.
    /// @return Delta `ExclusiveDepthStencil` targeting only planes leaving a read-only state.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr ExclusiveDepthStencil getWritableTransition() const noexcept
    {
        const Mode newDepth = isDepthRead() ? Mode::DepthWrite : Mode::DepthNop;
        const Mode newStencil = isStencilRead() ? Mode::StencilWrite : Mode::StencilNop;
        return ExclusiveDepthStencil(combine(newDepth, newStencil));
    }

    /// @brief Maps this state to its compact 0..3 DSV lookup index by collapsing `Nop` and `Read` per plane.
    /// @details Index mapping:
    ///          - `0`: Depth write + stencil write.
    ///          - `1`: Read-only depth + stencil write.
    ///          - `2`: Depth write + read-only stencil.
    ///          - `3`: Read-only depth and stencil.
    /// @return Dense view index in `[0, kMaxIndex - 1]`.
    [[nodiscard]] constexpr gp::UInt32 getIndex() const noexcept
    {
        switch (m_value)
        {
        case Mode::DepthWriteStencilWrite:
            return 0u;

        case Mode::DepthNopStencilWrite:
        case Mode::DepthReadStencilWrite:
            return 1u;

        case Mode::DepthWriteStencilNop:
        case Mode::DepthWriteStencilRead:
            return 2u;

        case Mode::DepthNopStencilNop:
        case Mode::DepthReadStencilNop:
        case Mode::DepthNopStencilRead:
        case Mode::DepthReadStencilRead:
            return 3u;

        default:
            if !consteval
            {
                GP_CHECK_NO_ENTRY();
            }
            return kMaxIndex;
        }
    }

    /// @brief Returns the underlying packed `ExclusiveDepthStencilMode` enum value.
    /// @return Packed mode enum.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr Mode getValue() const noexcept
    {
        return m_value;
    }

    /// @brief Computes a 64-bit hash of the packed access mode.
    /// @return 64-bit hash value.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::UInt64 getHash() const noexcept
    {
        return gp::hashing::hashInt64(static_cast<gp::UInt64>(m_value));
    }

    /// @brief Streams the packed mode into any `gp::hashing` streaming accumulator.
    /// @tparam InHasher Streaming hasher type.
    /// @param[in,out] hasher Target accumulator.
    template <typename InHasher>
    constexpr void hashAppend(InHasher& hasher) const noexcept
    {
        hasher.append(m_value);
    }

    /// @brief Compares two `ExclusiveDepthStencil` states for equality.
    /// @param[in] rhs State to compare against.
    /// @return True when the packed modes match.
    [[nodiscard]] constexpr bool operator==(const ExclusiveDepthStencil& rhs) const noexcept = default;

private:
    /// @brief Extracts the depth nibble from `m_value`.
    /// @return Depth-only `Mode` bits.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr Mode extractDepth() const noexcept
    {
        return static_cast<Mode>(static_cast<gp::UInt8>(m_value) & static_cast<gp::UInt8>(Mode::DepthMask));
    }

    /// @brief Extracts the stencil nibble from `m_value`.
    /// @return Stencil-only `Mode` bits.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr Mode extractStencil() const noexcept
    {
        return static_cast<Mode>(static_cast<gp::UInt8>(m_value) & static_cast<gp::UInt8>(Mode::StencilMask));
    }

    /// @brief Combines separate depth and stencil nibbles into one packed `Mode`.
    /// @param[in] depth Depth nibble.
    /// @param[in] stencil Stencil nibble.
    /// @return Combined `Mode` value.
    [[nodiscard]] GP_FORCEINLINE_HINT static constexpr Mode combine(const Mode depth, const Mode stencil) noexcept
    {
        return static_cast<Mode>(static_cast<gp::UInt8>(depth) | static_cast<gp::UInt8>(stencil));
    }

private:
    /// @brief Packed depth (low nibble) and stencil (high nibble) access mode.
    Mode m_value{ Mode::DepthNopStencilNop };
};

inline constexpr ExclusiveDepthStencil ExclusiveDepthStencil::kDepthNopStencilNop(Mode::DepthNopStencilNop);
inline constexpr ExclusiveDepthStencil ExclusiveDepthStencil::kDepthReadStencilNop(Mode::DepthReadStencilNop);
inline constexpr ExclusiveDepthStencil ExclusiveDepthStencil::kDepthWriteStencilNop(Mode::DepthWriteStencilNop);
inline constexpr ExclusiveDepthStencil ExclusiveDepthStencil::kDepthNopStencilRead(Mode::DepthNopStencilRead);
inline constexpr ExclusiveDepthStencil ExclusiveDepthStencil::kDepthReadStencilRead(Mode::DepthReadStencilRead);
inline constexpr ExclusiveDepthStencil ExclusiveDepthStencil::kDepthWriteStencilRead(Mode::DepthWriteStencilRead);
inline constexpr ExclusiveDepthStencil ExclusiveDepthStencil::kDepthNopStencilWrite(Mode::DepthNopStencilWrite);
inline constexpr ExclusiveDepthStencil ExclusiveDepthStencil::kDepthReadStencilWrite(Mode::DepthReadStencilWrite);
inline constexpr ExclusiveDepthStencil ExclusiveDepthStencil::kDepthWriteStencilWrite(Mode::DepthWriteStencilWrite);

}   // namespace gp::rhi::resource
