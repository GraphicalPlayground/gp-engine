# Copyright (c) - Graphical Playground. All rights reserved.

# Automatically find all subdirectories with a CMakeLists.txt and add them as modules
file(GLOB GP_RUNTIME_MODULES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} */CMakeLists.txt)
list(TRANSFORM GP_RUNTIME_MODULES REPLACE "/CMakeLists.txt$" "")

message(STATUS "[GP] Configuring Runtime modules...")

foreach(Module IN LISTS GP_RUNTIME_MODULES)
    set(ModulePath "${CMAKE_CURRENT_SOURCE_DIR}/${Module}")

    # Check if directory exists AND contains a CMakeLists.txt
    if(EXISTS "${ModulePath}/CMakeLists.txt")
        add_subdirectory(${Module})
    else()
        message(WARNING "  [-] Skipping Module: ${Module} (Source not found at ${ModulePath})")
    endif()
endforeach()
