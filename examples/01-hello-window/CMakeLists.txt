# Copyright (c) - Graphical Playground. All rights reserved.
# For more information, see https://graphical-playground/legal
# mailto:support AT graphical-playground DOT com

cmake_minimum_required(VERSION 3.21.0)

project(01_HelloWindow)

add_executable(01_HelloWindow
  source/Main.cpp
)

# TODO: Add dependencies to Graphical Playground Engine libraries here, e.g.:
# target_link_libraries(01_HelloWindow PRIVATE gp::core)
