# Copyright (c) - Graphical Playground. All rights reserved.
# For more information, see https://graphical-playground/legal
# mailto:support AT graphical-playground DOT com

# Options
option(GP_BUILD_TOOL_VERBOSE "Enable verbose output for the GP Build Tool" OFF)

# Version control
set(GP_BUILD_TOOL_VERSION_MAJOR 0)
set(GP_BUILD_TOOL_VERSION_MINOR 1)
set(GP_BUILD_TOOL_VERSION_PATCH 0)
set(GP_BUILD_TOOL_VERSION "${GP_BUILD_TOOL_VERSION_MAJOR}.${GP_BUILD_TOOL_VERSION_MINOR}.${GP_BUILD_TOOL_VERSION_PATCH}")

# Include all the dependencies
include(gp-build-tool/internals/gp-logger.internal)
include(gp-build-tool/internals/gp-scope.internal)
include(gp-build-tool/gp-target)
include(gp-build-tool/gp-dependencies)
include(gp-build-tool/gp-includes)
include(gp-build-tool/gp-definitions)
include(gp-build-tool/gp-thirdparty)
include(gp-build-tool/gp-flags)

# Startup banner
gpExecuteOnce("GPBT_STARTUP_BANNER" _shouldPrintBanner)
if(_shouldPrintBanner)
  gpLog("${GP_MAGENTA}=======================================${GP_RESET}")
  gpLog("${GP_MAGENTA}    Graphical Playground Build Tool     ${GP_RESET}")
  gpLog("${GP_MAGENTA}                v${GP_BUILD_TOOL_VERSION}${GP_RESET}")
  gpLog("${GP_MAGENTA}=======================================${GP_RESET}")
  gpLog("Initializing build environment...")
endif()

# TODO:
#  - Add support for custom build steps (e.g., code generation, asset processing)
#  - Add different type of dependencies: static, shared, interface
#  - Add support for custom build configurations (e.g., Debug, Release, etc.)
#  - Add support for custom build targets (e.g., tests, benchmarks, etc.)
#  - Add support for custom build tools (e.g., code formatters, linters, etc.)
#  - Write documentation for the GP Build Tool, including usage examples and best practices
