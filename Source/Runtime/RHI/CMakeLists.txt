# Copyright (c) - Graphical Playground. All rights reserved.

# Include the GPModule functions
include(GPModule)

# Define the RHI module
gp_add_module(
    NAME RHI
    MODULE_TYPE Runtime
    DEPENDENCIES
        GP::Runtime::Core
)
