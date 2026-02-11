# Copyright (c) - Graphical Playground. All rights reserved.

# Include the GPModule functions
include(GPModule)

# Define the Platform module
gp_add_module(
    NAME Platform
    MODULE_TYPE Runtime
    DEPENDENCIES
        GP::Runtime::Core
)
