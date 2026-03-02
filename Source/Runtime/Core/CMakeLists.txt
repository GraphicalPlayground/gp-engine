# Copyright (c) - Graphical Playground. All rights reserved.

# Include the GPModule functions
include(GPModule)

# Register the Core module
gp_register_module(
    NAME Core
)

# Define the Core module
if(GP_MODULE_BUILD_PHASE)
    gp_add_module(
        NAME Core
        MODULE_TYPE Runtime
    )
endif()
