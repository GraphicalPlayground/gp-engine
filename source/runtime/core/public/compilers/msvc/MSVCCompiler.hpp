// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#define GP_PLATFORM_RETURN_ADDRESS() _ReturnAddress()
#define GP_PLATFORM_RETURN_ADDRESS_POINTER() _AddressOfReturnAddress()

#ifdef __has_cpp_attribute
    #if __has_cpp_attribute(msvc::lifetimebound)
        #define GP_LIFETIMEBOUND [[msvc::lifetimebound]]
    #endif
#endif
