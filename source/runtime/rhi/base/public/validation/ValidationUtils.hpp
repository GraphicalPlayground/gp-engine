// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "commands/CommandTypes.hpp"
#include "core/ResourceForward.hpp"
#include "CoreMinimal.hpp"
#include "RHIBase.hpp"
#include "validation/ValidationForward.hpp"   // IWYU pragma: keep
#include "views/ViewDesc.hpp"                 // IWYU pragma: keep

namespace gp::rhi::validation
{

/// @brief Stateless validation utilities checking copy regions, alignments, and view invariants.
class GP_RHI_API ValidationUtils final
{
public:
    ValidationUtils() = delete;

    /// @brief Validates source/destination textures, mip/slice bounds, multisample rules, and block alignment for
    ///        `ICommandContext::copyTexture()`.
    /// @param[in] sourceTexture Source GPU texture.
    /// @param[in] destTexture Destination GPU texture.
    /// @param[in] copyInfo Copy region, mip, and slice parameters.
    static void validateCopyTexture(
        const resource::Texture* sourceTexture,
        const resource::Texture* destTexture,
        const command::CopyTextureInfo& copyInfo
    ) noexcept;

    /// @brief Validates buffer pointers, non-overlapping identity, and `[offset, offset + numBytes)` bounds for
    ///        `ICommandContext::copyBufferRegion()`.
    /// @param[in] destBuffer Destination GPU buffer.
    /// @param[in] dstOffset Destination byte offset.
    /// @param[in] sourceBuffer Source GPU buffer.
    /// @param[in] srcOffset Source byte offset.
    /// @param[in] numBytes Number of bytes to copy.
    static void validateCopyBufferRegion(
        const resource::Buffer* destBuffer,
        gp::UInt64 dstOffset,
        const resource::Buffer* sourceBuffer,
        gp::UInt64 srcOffset,
        gp::UInt64 numBytes
    ) noexcept;
};

}   // namespace gp::rhi::validation
