// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "hashing/HashCombine.hpp"
#include "hashing/HashMix.hpp"
#include "templates/NumericLimits.hpp"
#include "views/ViewEnums.hpp"
#include "views/ViewForward.hpp"   // IWYU pragma: keep

namespace gp::rhi::view
{

/// @brief Compact 8-byte handle addressing a bindless descriptor heap slot and its descriptor type.
struct DescriptorHandle
{
public:
    /// @brief Sentinel heap index marking an unbound or invalid handle.
    static constexpr gp::UInt32 kInvalidIndex = gp::NumericLimits<gp::UInt32>::max();

public:
    /// @brief Constructs an invalid descriptor handle.
    constexpr DescriptorHandle() noexcept = default;

    /// @brief Constructs a descriptor handle for `inType` at heap slot `inIndex`.
    /// @param[in] inType Descriptor type stored at the slot.
    /// @param[in] inIndex Zero-based index inside the bindless descriptor heap.
    constexpr DescriptorHandle(const DescriptorType inType, const gp::UInt32 inIndex) noexcept
        : m_index(inIndex)
        , m_type(inType)
    {}

    /// @brief Returns the zero-based descriptor heap index, or `kInvalidIndex` when invalid.
    /// @return Descriptor heap slot index.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::UInt32 getIndex() const noexcept
    {
        return m_index;
    }

    /// @brief Returns the descriptor type stored in this handle.
    /// @return `DescriptorType` enumerator.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr DescriptorType getType() const noexcept
    {
        return m_type;
    }

    /// @brief Converts this handle's descriptor type into its single-bit `DescriptorTypeMask`.
    /// @return Matching `DescriptorTypeMask`, or `DescriptorTypeMask::None` if invalid.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr DescriptorTypeMask getTypeMask() const noexcept
    {
        return descriptorTypeMaskFromType(m_type);
    }

    /// @brief Reports whether this handle references a valid bindless descriptor slot.
    /// @return True when `m_index != kInvalidIndex` and `m_type != DescriptorType::Invalid`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool isValid() const noexcept
    {
        return (m_index != kInvalidIndex) && (m_type != DescriptorType::Invalid);
    }

    /// @brief Computes a 64-bit hash combining the heap index and descriptor type.
    /// @return 64-bit hash value.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::UInt64 getHash() const noexcept
    {
        return gp::hashing::hashCombine(
            gp::hashing::hashInt64(m_index), gp::hashing::hashInt64(static_cast<gp::UInt64>(m_type))
        );
    }

    /// @brief Compares two descriptor handles for equality.
    /// @param[in] rhs Handle to compare against.
    /// @return True when both index and type match.
    [[nodiscard]] constexpr bool operator==(const DescriptorHandle& rhs) const noexcept = default;

private:
    gp::UInt32 m_index{ kInvalidIndex };
    DescriptorType m_type{ DescriptorType::Invalid };
};

}   // namespace gp::rhi::view
