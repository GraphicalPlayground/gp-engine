# Copyright (c) - Mallory SCOTTON. All rights reserved.

message(STATUS "[GP] Configuring Source modules...")

option(GP_BUILD_RUNTIME "Build the Runtime modules" ON)
if(GP_BUILD_RUNTIME)
    add_subdirectory(Runtime)
else()
    message(STATUS "[GP] Skipping Runtime modules...")
endif()

option(GP_BUILD_EDITOR "Build the Editor modules" ON)
if(GP_BUILD_EDITOR)
    add_subdirectory(Editor)
else()
    message(STATUS "[GP] Skipping Editor modules...")
endif()

option(GP_BUILD_PROGRAMS "Build the Programs modules" ON)
if(GP_BUILD_PROGRAMS)
    add_subdirectory(Programs)
else()
    message(STATUS "[GP] Skipping Programs modules...")
endif()

option(GP_BUILD_DEVELOPER "Build the Developer modules" ON)
if(GP_BUILD_DEVELOPER)
    add_subdirectory(Developer)
else()
    message(STATUS "[GP] Skipping Developer modules...")
endif()

option(GP_BUILD_LAUNCH "Build the Launch modules" ON)
if(GP_BUILD_LAUNCH)
    add_subdirectory(Launch)
else()
    message(STATUS "[GP] Skipping Launch modules...")
endif()

option(GP_BUILD_SHADERS "Build the Shaders modules" ON)
if(GP_BUILD_SHADERS)
    add_subdirectory(Shaders)
else()
    message(STATUS "[GP] Skipping Shaders modules...")
endif()

message(STATUS "[GP] Source modules configuration complete.")
