# Copyright (c) - Graphical Playground. All rights reserved.

# Include the GPModule functions
include(GPModule)

# Define the Shaders module
gp_add_module(
    NAME Shaders
    MODULE_TYPE Runtime
    DEPENDENCIES
        GP::Runtime::Core
)
