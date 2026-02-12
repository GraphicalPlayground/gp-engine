// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

// Main Shaders module header file
#include "ShadersBuild.hpp"

// Module Version
#define GP_SHADERS_VERSION_MAJOR 1
#define GP_SHADERS_VERSION_MINOR 0
#define GP_SHADERS_VERSION_PATCH 0
#define GP_SHADERS_VERSION GP_VERSION(GP_SHADERS_VERSION_MAJOR, GP_SHADERS_VERSION_MINOR, GP_SHADERS_VERSION_PATCH)

// Forward declarations
#include "ShadersForward.hpp"

// Public API for the Shaders module
#include "Core/ShaderBytecode.hpp"
#include "Core/ShaderDefine.hpp"
#include "Core/ShaderDiagnostic.hpp"
#include "Core/ShaderHash.hpp"
#include "Core/ShaderSource.hpp"
#include "Core/ShaderTypes.hpp"
