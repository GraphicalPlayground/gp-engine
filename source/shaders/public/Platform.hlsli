// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

// Platform specific includes
#if GP_PLATFORM_D3D11
    #include "platforms/d3d11/D3D11Common.hlsli"
#elif GP_PLATFORM_D3D12
    #include "platforms/d3d12/D3D12Common.hlsli"
#elif GP_PLATFORM_OPENGL
    #include "platforms/opengl/OpenGLCommon.hlsli"
#elif GP_PLATFORM_VULKAN
    #include "platforms/vulkan/VulkanCommon.hlsli"
#elif GP_PLATFORM_METAL
    #include "platforms/metal/MetalCommon.hlsli"
#endif

#if (GP_PLATFORM_D3D11 + GP_PLATFORM_D3D12 + GP_PLATFORM_OPENGL + GP_PLATFORM_VULKAN + GP_PLATFORM_METAL) > 1
    #error "Multiple platforms defined. Please define only one platform."
#endif

#ifndef GP_PLATFORM_D3D11
    #define GP_PLATFORM_D3D11 0
#endif

#ifndef GP_PLATFORM_D3D12
    #define GP_PLATFORM_D3D12 0
#endif

#ifndef GP_PLATFORM_OPENGL
    #define GP_PLATFORM_OPENGL 0
#endif

#ifndef GP_PLATFORM_VULKAN
    #define GP_PLATFORM_VULKAN 0
#endif

#ifndef GP_PLATFORM_METAL
    #define GP_PLATFORM_METAL 0
#endif
