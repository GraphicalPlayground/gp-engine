// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"

// When using intelissense in Visual Studio, it may not pick up the CMake definitions. In that case, you can define the
// backend manually here for code browsing purposes.
#if defined(__INTELLISENSE__)
    #if GP_PLATFORM_WINDOWS
        #define GP_RHI_BACKEND_D3D12 1
        #define GP_RHI_BACKEND_D3D11 1
        #define GP_RHI_BACKEND_VULKAN 1
        #define GP_RHI_BACKEND_OPENGL 1
        #define GP_RHI_BACKEND_OPENGLES 1
    #elif GP_PLATFORM_APPLE
        #define GP_RHI_BACKEND_METAL 1
        #define GP_RHI_BACKEND_OPENGL 1
        #define GP_RHI_BACKEND_OPENGLES 1
        #define GP_RHI_BACKEND_VULKAN 1
    #elif GP_PLATFORM_LINUX
        #define GP_RHI_BACKEND_VULKAN 1
        #define GP_RHI_BACKEND_OPENGL 1
        #define GP_RHI_BACKEND_OPENGLES 1
    #elif GP_PLATFORM_ANDROID
        #define GP_RHI_BACKEND_VULKAN 1
        #define GP_RHI_BACKEND_OPENGLES 1
    #elif GP_PLATFORM_WEB
        #define GP_RHI_BACKEND_WEBGPU 1
    #endif
#endif

// Exactly one backend must be selected at build time. CMake sets these defines.

#ifndef GP_RHI_BACKEND_VULKAN
    #define GP_RHI_BACKEND_VULKAN 0
#endif
#ifndef GP_RHI_BACKEND_D3D12
    #define GP_RHI_BACKEND_D3D12 0
#endif
#ifndef GP_RHI_BACKEND_D3D11
    #define GP_RHI_BACKEND_D3D11 0
#endif
#ifndef GP_RHI_BACKEND_METAL
    #define GP_RHI_BACKEND_METAL 0
#endif
#ifndef GP_RHI_BACKEND_OPENGL
    #define GP_RHI_BACKEND_OPENGL 0
#endif
#ifndef GP_RHI_BACKEND_OPENGLES
    #define GP_RHI_BACKEND_OPENGLES 0
#endif
#ifndef GP_RHI_BACKEND_WEBGPU
    #define GP_RHI_BACKEND_WEBGPU 0
#endif

#if !(                                                                                               \
    GP_RHI_BACKEND_VULKAN || GP_RHI_BACKEND_D3D12 || GP_RHI_BACKEND_D3D11 || GP_RHI_BACKEND_METAL || \
    GP_RHI_BACKEND_OPENGL || GP_RHI_BACKEND_OPENGLES || GP_RHI_BACKEND_WEBGPU                        \
)
    #error "[GP] No RHI backend defined. Please define one."
#endif

#if GP_BUILD_DEBUG || GP_BUILD_DEVELOPMENT
    #define GP_RHI_ENABLE_VALIDATION 1
#else
    #define GP_RHI_ENABLE_VALIDATION 0
#endif

// These are enabled by default on desktop Vulkan / D3D12 and can be overridden by CMake.

#if !defined(GP_RHI_ENABLE_RAY_TRACING)
    #if defined(GP_RHI_BACKEND_VULKAN) || defined(GP_RHI_BACKEND_D3D12)
        #define GP_RHI_ENABLE_RAY_TRACING 1
    #else
        #define GP_RHI_ENABLE_RAY_TRACING 0
    #endif
#endif

#if !defined(GP_RHI_ENABLE_MESH_SHADERS)
    #if defined(GP_RHI_BACKEND_VULKAN) || defined(GP_RHI_BACKEND_D3D12) || defined(GP_RHI_BACKEND_METAL)
        #define GP_RHI_ENABLE_MESH_SHADERS 1
    #else
        #define GP_RHI_ENABLE_MESH_SHADERS 0
    #endif
#endif

#if !defined(GP_RHI_ENABLE_VARIABLE_RATE_SHADING)
    #if defined(GP_RHI_BACKEND_VULKAN) || defined(GP_RHI_BACKEND_D3D12)
        #define GP_RHI_ENABLE_VARIABLE_RATE_SHADING 1
    #else
        #define GP_RHI_ENABLE_VARIABLE_RATE_SHADING 0
    #endif
#endif

#if !defined(GP_RHI_ENABLE_WORK_GRAPHS)
    #if defined(GP_RHI_BACKEND_D3D12)
        #define GP_RHI_ENABLE_WORK_GRAPHS 1
    #else
        #define GP_RHI_ENABLE_WORK_GRAPHS 0
    #endif
#endif
