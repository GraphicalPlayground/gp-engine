# Copyright (c) - Mallory SCOTTON. All rights reserved.

# List of modules in the preferred order of dependency
set(GP_LAUNCH_MODULES
    "Editor"
)

message(STATUS "[GP] Configuring Launch modules...")

foreach(Module IN LISTS GP_LAUNCH_MODULES)
    set(ModulePath "${CMAKE_CURRENT_SOURCE_DIR}/${Module}")

    # Check if directory exists AND contains a CMakeLists.txt
    if(EXISTS "${ModulePath}/CMakeLists.txt")
        add_subdirectory(${Module})
    else()
        message(WARNING "  [-] Skipping Module: ${Module} (Source not found at ${ModulePath})")
    endif()
endforeach()
