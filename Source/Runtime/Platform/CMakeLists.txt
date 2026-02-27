# Copyright (c) - Graphical Playground. All rights reserved.

# Include the GPModule functions
include(GPModule)

# Register the Platform module
gp_register_module(
    NAME Platform
    DEPENDENCIES
        Core
)

# Define the Platform module
if(GP_MODULE_BUILD_PHASE)
    gp_add_module(
        NAME Platform
        MODULE_TYPE Runtime
        PUBLIC_DEPENDENCIES
            GP::Runtime::Core
    )
endif()
