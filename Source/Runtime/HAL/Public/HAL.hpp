// Copyright (c) - Mallory SCOTTON. All rights reserved.

#pragma once

// Main HAL module header file
#include "HALBuild.hpp"

// Module Version
#define GP_HAL_VERSION_MAJOR 1
#define GP_HAL_VERSION_MINOR 0
#define GP_HAL_VERSION_PATCH 0
#define GP_HAL_VERSION GP_VERSION(GP_HAL_VERSION_MAJOR, GP_HAL_VERSION_MINOR, GP_HAL_VERSION_PATCH)

// Import forward export definitions
#include "HALForward.hpp"

// Include all public headers of the HAL module
#include "Application/Application.hpp"
#include "Common/Common.hpp"
#include "Factory/Factory.hpp"
#include "Input/Input.hpp"
#include "Monitor/Monitor.hpp"
#include "Windowing/Windowing.hpp"
