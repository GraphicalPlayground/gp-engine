// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "memory/pointers/RefCountedPtr.hpp"

namespace gp::rhi::vulkan
{

/// @brief Health of a swapchain after an acquire or a present, negative values asking for a recreation.
enum class VulkanSwapChainStatus : gp::Int32;

/// @brief Transfer function and gamut the swapchain images are displayed with.
enum class VulkanDisplayOutputFormat : gp::UInt8;

/// @brief Swapchain and surface handed from a destroyed swapchain to its replacement.
struct VulkanSwapChainRecreateInfo;

/// @brief Surface format and engine pixel format chosen for a swapchain.
struct VulkanSurfaceFormatSelection;

/// @brief Snapshot of every presentation setting (console variables and command line) read at swapchain creation.
struct VulkanPresentationConfig;

/// @brief Creation parameters of a `VulkanSwapChain`.
struct VulkanSwapChainDesc;

/// @brief Non-owning bundle of the device-level services a `VulkanViewport` presents with.
struct VulkanViewportServices;

/// @brief Platform swapchain wrapper managing presentable images and acquire/present synchronization.
class VulkanSwapChain;

/// @brief RHI texture standing for "the image currently acquired from the swapchain".
class VulkanBackBuffer;

/// @brief Concrete `presentation::Viewport` wrapping a `VulkanSwapChain`.
class VulkanViewport;

/// @brief Non-owning list of the viewports alive on a device, released by the RHI at shutdown.
class VulkanViewportRegistry;

/// @brief Owning reference to a `VulkanBackBuffer`.
using VulkanBackBufferRef = gp::RefCountedPtr<VulkanBackBuffer>;

/// @brief Owning reference to a `VulkanViewport`.
using VulkanViewportRef = gp::RefCountedPtr<VulkanViewport>;

}   // namespace gp::rhi::vulkan
