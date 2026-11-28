// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "commands/CommandForward.hpp"
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "core/ResourceForward.hpp"          // IWYU pragma: keep
#include "CoreMinimal.hpp"                   // IWYU pragma: keep
#include "RHIVulkan.hpp"

namespace gp::rhi::vulkan
{

/// @brief Moves the memory of each source resource into its destination, on the RHI timeline.
/// @details For buffers, the destination releases its own memory and takes the source's, or just releases it when the
///          source is null; views of the destination are then refreshed. Sources must have no views. The infos are
///          copied, so the caller's array may go away, but the resources must stay alive until the command runs.
/// @note Backs `DynamicRHI::replaceResources()`. Ray tracing geometry is not supported yet and asserts.
/// @param[in,out] cmdList Command list the swap is recorded on.
/// @param[in] replaceInfos Destination and source pairs.
GP_RHI_VULKAN_API void replaceVulkanResources(
    command::CommandListBase& cmdList, gp::ConstVectorView<resource::ResourceReplaceInfo> replaceInfos
) noexcept;

}   // namespace gp::rhi::vulkan

