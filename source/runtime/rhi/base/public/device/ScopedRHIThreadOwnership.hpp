// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"            // IWYU pragma: keep
#include "device/DeviceForward.hpp"   // IWYU pragma: keep
#include "device/DynamicRHI.hpp"

namespace gp::rhi
{

/// @brief RAII scope acquiring and releasing backend thread ownership on the active `DynamicRHI`.
class ScopedRHIThreadOwnership final
{
public:
    /// @brief Acquires thread ownership on `inRHI` (or `getDynamicRHI()` when null).
    /// @param[in] inRHI Optional explicit backend pointer.
    explicit ScopedRHIThreadOwnership(DynamicRHI* const inRHI = getDynamicRHI()) noexcept
        : m_rhi(inRHI)
    {
        if (m_rhi != nullptr)
        {
            m_rhi->acquireThreadOwnership();
        }
    }

    /// @brief Releases thread ownership on `m_rhi`.
    ~ScopedRHIThreadOwnership() noexcept
    {
        if (m_rhi != nullptr)
        {
            m_rhi->releaseThreadOwnership();
        }
    }

    ScopedRHIThreadOwnership(const ScopedRHIThreadOwnership&) = delete;
    ScopedRHIThreadOwnership& operator=(const ScopedRHIThreadOwnership&) = delete;
    ScopedRHIThreadOwnership(ScopedRHIThreadOwnership&&) = delete;
    ScopedRHIThreadOwnership& operator=(ScopedRHIThreadOwnership&&) = delete;

private:
    /// @brief Owned backend reference whose thread ownership is held for this scope.
    DynamicRHI* m_rhi{ nullptr };
};

}   // namespace gp::rhi
