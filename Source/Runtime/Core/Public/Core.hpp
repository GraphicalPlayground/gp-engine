// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

// Main Core module header file
#include "CoreBuild.hpp"

// Module Version
#define GP_CORE_VERSION_MAJOR 1
#define GP_CORE_VERSION_MINOR 0
#define GP_CORE_VERSION_PATCH 0
#define GP_CORE_VERSION GP_VERSION(GP_CORE_VERSION_MAJOR, GP_CORE_VERSION_MINOR, GP_CORE_VERSION_PATCH)

// Include submodule headers
#include "Container/Container.hpp"
#include "Crypto/Crypto.hpp"
#include "Math/Math.hpp"
#include "Memory/Memory.hpp"
#include "Serialization/Serialization.hpp"
#include "Templates/Templates.hpp"
