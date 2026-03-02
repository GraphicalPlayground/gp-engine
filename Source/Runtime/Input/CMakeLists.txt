# Copyright (c) - Graphical Playground. All rights reserved.

# Include the GPModule functions
include(GPModule)

# Register the Input module
gp_register_module(
    NAME Input
    DEPENDENCIES
        Core
)

# Define the Shaders module
if(GP_MODULE_BUILD_PHASE)
    gp_add_module(
        NAME Input
        MODULE_TYPE Runtime
        PUBLIC_DEPENDENCIES
            GP::Runtime::Core
    )
endif()
