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

# Add the source directory to the module path and include the GPBT build tool module
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/gp-build-tool/source")
include(gp-build-tool)

# Apply the default policies and configurations for Graphical Playground targets.
gpApplyGraphicalPlaygroundDefaultPolicy()

# Start the GPBT build tool and auto-scan the source directory for targets to build.
gpStartBuildTool()
  gpBuildToolAutoScan(source)
gpEndBuildTool()
