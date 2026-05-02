// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "macros/DetectArchitecture.hpp"   // IWYU pragma: export
#include "macros/DetectBuild.hpp"          // IWYU pragma: export
#include "macros/DetectCompiler.hpp"       // IWYU pragma: export
#include "macros/DetectConfig.hpp"         // IWYU pragma: export
#include "macros/DetectPlatform.hpp"       // IWYU pragma: export
#include "macros/DetectSimd.hpp"           // IWYU pragma: export
#include "macros/MacroUtilities.hpp"       // IWYU pragma: export

// Enable Tracy profiling in Debug and RelWithDebInfo builds when profiling is enabled
#if (GP_CONFIG_DEBUG || GP_CONFIG_RELWITHDEBINFO) && GP_ENABLE_PROFILING
    #ifndef TRACY_ENABLE
        #define TRACY_ENABLE 1
    #endif
#endif
