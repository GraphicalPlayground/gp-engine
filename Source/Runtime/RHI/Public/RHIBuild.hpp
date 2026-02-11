// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"

// Define export/import macros for the RHI module
#define GP_RHI_API GP_API

// Graphics API options (can be set via build system)
#ifndef GP_GRAPHICS_API_OPENGL
    #define GP_GRAPHICS_API_OPENGL 0
#endif

#ifndef GP_GRAPHICS_API_VULKAN
    #define GP_GRAPHICS_API_VULKAN 0
#endif

#ifndef GP_GRAPHICS_API_DIRECTX11
    #define GP_GRAPHICS_API_DIRECTX11 0
#endif

#ifndef GP_GRAPHICS_API_DIRECTX12
    #define GP_GRAPHICS_API_DIRECTX12 0
#endif

#ifndef GP_GRAPHICS_API_METAL
    #define GP_GRAPHICS_API_METAL 0
#endif

#ifndef GP_GRAPHICS_API_WEBGPU
    #define GP_GRAPHICS_API_WEBGPU 0
#endif

// Ray tracing support
#ifndef GP_ENABLE_RAYTRACING
    #define GP_ENABLE_RAYTRACING 0
#endif

// Mesh shaders support
#ifndef GP_ENABLE_MESH_SHADERS
    #define GP_ENABLE_MESH_SHADERS 0
#endif

// Variable rate shading
#ifndef GP_ENABLE_VRS
    #define GP_ENABLE_VRS 0
#endif

// Physics options
#ifndef GP_ENABLE_PHYSICS_DEBUG_DRAW
    #define GP_ENABLE_PHYSICS_DEBUG_DRAW GP_BUILD_DEBUG
#endif

namespace GP::Build
{

namespace Graphics
{

inline constexpr bool HasOpenGL = GP_GRAPHICS_API_OPENGL;
inline constexpr bool HasVulkan = GP_GRAPHICS_API_VULKAN;
inline constexpr bool HasDirectX11 = GP_GRAPHICS_API_DIRECTX11;
inline constexpr bool HasDirectX12 = GP_GRAPHICS_API_DIRECTX12;
inline constexpr bool HasMetal = GP_GRAPHICS_API_METAL;
inline constexpr bool HasWebGPU = GP_GRAPHICS_API_WEBGPU;

}   // namespace Graphics

namespace Features
{

inline constexpr bool HasPhysicsDebugDraw = GP_ENABLE_PHYSICS_DEBUG_DRAW;
inline constexpr bool HasRayTracing = GP_ENABLE_RAYTRACING;
inline constexpr bool HasMeshShaders = GP_ENABLE_MESH_SHADERS;
inline constexpr bool HasVRS = GP_ENABLE_VRS;

}   // namespace Features

}   // namespace GP::Build
