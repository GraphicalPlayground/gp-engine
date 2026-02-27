# Copyright (c) - Graphical Playground. All rights reserved.

# Include the GPModule functions
include(GPModule)

# Register the HAL module
gp_register_module(
    NAME HAL
    DEPENDENCIES
        Core
)

# Define the HAL module
if(GP_MODULE_BUILD_PHASE)
    gp_add_module(
        NAME HAL
        MODULE_TYPE Runtime
    
        PUBLIC_DEPENDENCIES
            GP::Runtime::Core
    
        OPTIONAL_DEPENDENCIES
            SDL3::SDL3-static
    
        FEATURE_DEFINES
            SDL3::SDL3-static GP_HAL_SDL3_SUPPORT
    )
endif()
