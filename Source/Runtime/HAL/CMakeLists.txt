# Copyright (c) - Mallory SCOTTON. All rights reserved.

# Include the GPModule functions
include(GPModule)

# Define the HAL module
gp_add_module(
    NAME HAL
    MODULE_TYPE Runtime
    DEPENDENCIES
        GP::Runtime::Core
        SDL3::SDL3-static
)
