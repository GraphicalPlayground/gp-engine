// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"

#if GP_IS_MONOLITHIC
    #define GP_RHI_API
#else
    #if defined(GP_RHI_EXPORTS)
        #define GP_RHI_API GP_DLLEXPORT
    #else
        #define GP_RHI_API GP_DLLIMPORT
    #endif
#endif
