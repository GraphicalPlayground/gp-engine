# Copyright (c) - Graphical Playground. All rights reserved.

cmake_minimum_required(VERSION 3.20)

# Project Definition
project(GraphicalPlayground
    VERSION 0.1.0
    LANGUAGES CXX C
    DESCRIPTION "Graphical Playground Engine, learning and prototyping platform for graphics programming."
)

# Enforce C++20 globally
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF) # Forces standard C++ (no GCC extensions)

# If not specified, build type is Release
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "[GP] Setting build type to 'Release' as none was specified.")
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
    # Set the possible values of build type for cmake-gui
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

# If not specified, export compile commands for IDEs
if(NOT CMAKE_EXPORT_COMPILE_COMMANDS)
    message(STATUS "[GP] Enabling export of compile commands for IDEs.")
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "Export compile commands." FORCE)
endif()

# Build Shared Libraries (DLLs) by default?
# Set to OFF for Monolithic builds (Static Linking)
option(GP_BUILD_SHARED "Build Engine as DLLs" ON)
if(GP_BUILD_SHARED)
    set(BUILD_SHARED_LIBS ON)
    add_compile_definitions(GP_BUILD_DLL) # Global define for DLL logic
endif()

# Output Directories (Clean bin/ folder structure)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/Binaries/Lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/Binaries/Lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/Binaries/Bin)

# Enable Position Independent Code (PIC) for static libraries to allow linking into shared libraries on all platforms
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Put Debug/Release in subfolders (Bin/Debug, Bin/Release)
foreach(OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_SOURCE_DIR}/Binaries/Bin/${OUTPUTCONFIG})
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_SOURCE_DIR}/Binaries/Lib/${OUTPUTCONFIG})
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_SOURCE_DIR}/Binaries/Lib/${OUTPUTCONFIG})
endforeach()

# Include the module creation function
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/CMake")

# Include Options
include(GPOptions)

# Apply Options
gp_apply_options()

# Add Subdirectories
add_subdirectory(ThirdParty)
add_subdirectory(Source)
add_subdirectory(Examples)
