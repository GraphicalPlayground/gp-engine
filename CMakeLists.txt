# Copyright (c) - Graphical Playground. All rights reserved.
# For more information, see https://graphical-playground/legal
# mailto:support AT graphical-playground DOT com

cmake_minimum_required(VERSION 3.28.0)

# Project Definition
project(GraphicalPlayground
  VERSION 0.1.0
  LANGUAGES CXX C
  DESCRIPTION "Graphical Playground Engine, learning and prototyping platform for graphics programming."
)

# Add an option to allow users to skip the forced update (e.g., if they are developing the build tool locally)
option(GP_SKIP_SUBMODULE_UPDATE "Skip automatic alignment of Git submodules" OFF)

find_package(Git QUIET REQUIRED)

if(NOT GP_SKIP_SUBMODULE_UPDATE)
  message(STATUS "Ensuring submodules (including gp-build-tool) are aligned with the repository...")
  execute_process(
    COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    RESULT_VARIABLE GIT_SUBMODULE_RESULT
  )
  if(NOT GIT_SUBMODULE_RESULT EQUAL 0)
    message(FATAL_ERROR "Failed to update submodules. Please check your network connection or Git permissions.")
  endif()
else()
  message(STATUS "Skipping submodule alignment (GP_SKIP_SUBMODULE_UPDATE is ON).")
  # Fallback safety check: even if skipped, we still need the tool to exist to configure the project
  if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cmake/gp-build-tool/source")
    message(FATAL_ERROR "gp-build-tool is completely missing, but GP_SKIP_SUBMODULE_UPDATE is ON. Please run 'git submodule update --init' manually.")
  endif()
endif()

# Add the source directory to the module path and include the GPBT build tool module
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/gp-build-tool/source")
include(gp-build-tool)

# Apply the default policies and configurations for Graphical Playground targets.
gpApplyGraphicalPlaygroundDefaultPolicy()

# Start the GPBT build tool and auto-scan the source directory for targets to build.
gpStartBuildTool()
  gpBuildToolAutoScan(thirdparty source)
gpEndBuildTool()
