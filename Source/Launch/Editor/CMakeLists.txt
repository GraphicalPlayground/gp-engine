# Copyright (c) - Mallory SCOTTON. All rights reserved.

# Include the GPExecutable functions
include(GPExecutable)

# Define the Editor executable
gp_add_executable(
    NAME Editor
    MODULE_TYPE Launch
    OUTPUT_NAME GPEditor
    TERMINAL OFF
    ICON ${CMAKE_CURRENT_SOURCE_DIR}/Resources/AppIcon.rc
    DEPENDENCIES
        GP::Runtime::Core
)
