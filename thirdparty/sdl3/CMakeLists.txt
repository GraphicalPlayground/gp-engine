# Copyright (c) - Graphical Playground. All rights reserved.

# Include the GP fetch content function
include(GPFetchContent)

# Configure SDL3 build options before fetching
# These options control how SDL3 is compiled when fetched from Git
option(GP_SDL3_SHARED "Build SDL3 as shared library" OFF)
option(GP_SDL3_STATIC "Build SDL3 as static library" ON)
option(GP_SDL3_TEST "Build SDL3 test programs" OFF)

# Apply configuration to SDL3
set(SDL_SHARED ${GP_SDL3_SHARED} CACHE BOOL "" FORCE)
set(SDL_STATIC ${GP_SDL3_STATIC} CACHE BOOL "" FORCE)
set(SDL_TEST ${GP_SDL3_TEST} CACHE BOOL "" FORCE)

# Fetch SDL3
gp_fetch_content(
    NAME SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.4.0
    PACKAGE_NAME SDL3
    REQUIRED_VERSION 3.0
)

# Post-Fetch Configuration (Optional)

# Verify SDL3 targets are available
if(TARGET SDL3::SDL3)
    message(STATUS "  [+] SDL3::SDL3 target is available")

    # Optional: Set additional properties on SDL3 target
    # set_target_properties(SDL3::SDL3 PROPERTIES
    #     FOLDER "ThirdParty"
    # )
else()
    message(WARNING "  [-] SDL3::SDL3 target not found after fetch")
endif()
