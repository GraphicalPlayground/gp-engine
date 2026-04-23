# Copyright (c) - Graphical Playground. All rights reserved.
# For more information, see https://graphical-playground/legal
# mailto:support AT graphical-playground DOT com

include(gp-build-tool/internals/gp-scope.internal)
include(gp-build-tool/internals/gp-logger.internal)
include(gp-build-tool/gp-utils)
include(gp-build-tool/gp-includes)
include(gp-build-tool/gp-definitions)
include(gp-build-tool/gp-dependencies)
include(gp-build-tool/gp-internals)
include(gp-build-tool/gp-flags)

# Header guard to prevent multiple inclusions of this file
if (DEFINED GP_BUILD_TOOL_TARGET_INCLUDED)
  return()
else()
  set(GP_BUILD_TOOL_TARGET_INCLUDED ON)
endif()

# Valid target types for validation
set(GP_VALID_TARGET_TYPES "module" "executable")

macro(gpStartTarget TARGET_NAME TARGET_TYPE)
  # Check that we are not already in a target definition
  gpCheckNotInTarget()

  gpPushScope()

  gpSetScoped(__GP_IN_TARGET ON)

  # Normalize target name and type to lowercase for consistency
  gpSetScoped(__GP_TARGET_NAME "${TARGET_NAME}")
  string(TOLOWER "${TARGET_NAME}" __GP_TARGET_NAME)
  gpSetScoped(__GP_TARGET_NAME_UPPER "${TARGET_NAME}")
  string(TOUPPER "${TARGET_NAME}" __GP_TARGET_NAME_UPPER)
  gpSetScoped(__GP_TARGET_TYPE "${TARGET_TYPE}")
  string(TOLOWER "${TARGET_TYPE}" __GP_TARGET_TYPE)
  gpSetScoped(__GP_TARGET_OUTPUT_NAME "gp_${__GP_TARGET_NAME}")

  # Validate that target name is not empty
  if ("${__GP_TARGET_NAME}" STREQUAL "")
    gpFatal("Target name cannot be empty.")
  endif()

  gpGetCurrentPhase(__GP_CURRENT_PHASE)
  get_property(_CURRENT_TARGETS GLOBAL PROPERTY GP_REGISTERED_TARGETS)

  # Check that target name is unique
  if (__GP_CURRENT_PHASE STREQUAL "REGISTRATION")
    # Store location of the target for potential use in custom build steps or tools that need to know where the source files are
    set_property(GLOBAL PROPERTY GP_TARGET_${__GP_TARGET_NAME}_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}")

    if ("${__GP_TARGET_NAME}" IN_LIST _CURRENT_TARGETS)
      gpFatal("Target name '${__GP_TARGET_NAME}' is already registered. Target names must be unique.")
    else()
      set_property(GLOBAL APPEND PROPERTY GP_REGISTERED_TARGETS "${__GP_TARGET_NAME}")
    endif()
  endif()

  get_property(__GP_TARGET_LOCATION GLOBAL PROPERTY GP_TARGET_${__GP_TARGET_NAME}_LOCATION)

  # Validate target type
  if (NOT "${__GP_TARGET_TYPE}" IN_LIST GP_VALID_TARGET_TYPES)
    gpFatal("Invalid target type '${__GP_TARGET_TYPE}' for target '${__GP_TARGET_NAME}'. Valid types are: ${GP_VALID_TARGET_TYPES}")
  endif()

  # Sources
  set(__GP_TARGET_SOURCES)

  # Executable specific properties
  if ("${__GP_TARGET_TYPE}" STREQUAL "executable")
    gpSetScoped(__GP_EXEC_RESOURCES)
    gpSetScoped(__GP_EXEC_IS_TERMINAL OFF)
  endif()

  # Dependencies
  gpSetScoped(__GP_TARGET_PUB_DEPS)
  gpSetScoped(__GP_TARGET_PRV_DEPS)
  gpSetScoped(__GP_TARGET_INT_DEPS)
  gpSetScoped(__GP_TARGET_DYN_DEPS)

  # Include Directories
  gpSetScoped(__GP_TARGET_PUB_INCLUDES)
  gpSetScoped(__GP_TARGET_PRV_INCLUDES)
  gpSetScoped(__GP_TARGET_INT_INCLUDES)

  # Compile Definitions
  gpSetScoped(__GP_TARGET_PUB_DEFS)
  gpSetScoped(__GP_TARGET_PRV_DEFS)
  gpSetScoped(__GP_TARGET_INT_DEFS)

  # PCHs
  gpSetScoped(__GP_TARGET_PCH_HEADERS)

  # Build Flags
  gpSetScoped(__GP_TARGET_PUB_COMP_FLAGS)
  gpSetScoped(__GP_TARGET_PRV_COMP_FLAGS)

  # Options
  gpSetScoped(__GP_TARGET_ENABLE_TESTS OFF)
  gpSetScoped(__GP_TARGET_ENABLE_BENCHMARKS OFF)
  gpSetScoped(__GP_TARGET_ENABLE_EXAMPLES OFF)
  gpSetScoped(__GP_TARGET_ENABLE_IPSC OFF)
  gpSetScoped(__GP_TARGET_ENABLE_STRICT_WARNING OFF)

  # Log the start of a new target definition
  gpLog("Starting definition of target '${__GP_TARGET_NAME}' of type '${__GP_TARGET_TYPE}'")

  # Default include paths
  gpAddPublicIncludesPath(${__GP_TARGET_LOCATION}/public)
  gpAddPrivateIncludesPath(${__GP_TARGET_LOCATION}/private)
  gpAddInternalIncludesPath(${__GP_TARGET_LOCATION}/internal)

  # Default source files
  gpTargetAddSourcesFromDirectory(${__GP_TARGET_LOCATION}/private)

  # Set strict warnings by default
  gpTargetSetStrictWarningEnabled(ON)
endmacro()

macro(gpEndTarget)
  # Check that we are currently in a target definition
  gpCheckInTarget()

  # Get current process phase, if not set, default to REGISTRATION.
  gpGetCurrentPhase(__GP_CURRENT_PHASE)

  # Combine all dependencies into a single list for easy graph resolution later
  set(_ALL_DEPS ${__GP_TARGET_PUB_DEPS} ${__GP_TARGET_PRV_DEPS}  ${__GP_TARGET_INT_DEPS} ${__GP_TARGET_DYN_DEPS})
  # Store specific and combined dependencies using dynamic property names
  set_property(GLOBAL PROPERTY GP_TARGET_${__GP_TARGET_NAME}_PUB_DEPS "${__GP_TARGET_PUB_DEPS}")
  set_property(GLOBAL PROPERTY GP_TARGET_${__GP_TARGET_NAME}_PRV_DEPS "${__GP_TARGET_PRV_DEPS}")
  set_property(GLOBAL PROPERTY GP_TARGET_${__GP_TARGET_NAME}_INT_DEPS "${__GP_TARGET_INT_DEPS}")
  set_property(GLOBAL PROPERTY GP_TARGET_${__GP_TARGET_NAME}_DYN_DEPS "${__GP_TARGET_DYN_DEPS}")
  set_property(GLOBAL PROPERTY GP_TARGET_${__GP_TARGET_NAME}_ALL_DEPS "${_ALL_DEPS}")

  if ("${__GP_CURRENT_PHASE}" STREQUAL "CONFIGURATION")
    __gpDefineCMakeTarget()
  endif()

  # Endup message
  gpLog("Finished definition of target '${__GP_TARGET_NAME}'")

  # Clean up target-specific variables to avoid accidental reuse in subsequent targets
  gpPopScope()
endmacro()

# Convenience macros for common target types

macro(gpStartModule TARGET_NAME)
  gpStartTarget(${TARGET_NAME} module)
endmacro()

macro(gpStartExecutable TARGET_NAME)
  gpStartTarget(${TARGET_NAME} executable)
endmacro()

macro(gpEndModule)
  gpEndTarget()
endmacro()

macro(gpEndExecutable)
  gpEndTarget()
endmacro()

macro(gpTargetSetTestsEnabled VALUE)
  gpCheckInTarget()
  gpVerbose("Setting tests enabled for target '${__GP_TARGET_NAME}' to ${VALUE}")
  set(__GP_TARGET_ENABLE_TESTS ${VALUE})
endmacro()

macro(gpTargetSetBenchmarksEnabled VALUE)
  gpCheckInTarget()
  gpVerbose("Setting benchmarks enabled for target '${__GP_TARGET_NAME}' to ${VALUE}")
  set(__GP_TARGET_ENABLE_BENCHMARKS ${VALUE})
endmacro()

macro(gpTargetSetExamplesEnabled VALUE)
  gpCheckInTarget()
  gpVerbose("Setting examples enabled for target '${__GP_TARGET_NAME}' to ${VALUE}")
  set(__GP_TARGET_ENABLE_EXAMPLES ${VALUE})
endmacro()

macro(gpTargetSetIPSCEnabled VALUE)
  gpCheckInTarget()
  gpVerbose("Setting IPSC enabled for target '${__GP_TARGET_NAME}' to ${VALUE}")
  set(__GP_TARGET_ENABLE_IPSC ${VALUE})
endmacro()

macro(gpTargetSetStrictWarningEnabled VALUE)
  gpCheckInTarget()
  gpVerbose("Setting strict warning enabled for target '${__GP_TARGET_NAME}' to ${VALUE}")
  set(__GP_TARGET_ENABLE_STRICT_WARNING ${VALUE})
endmacro()

macro(gpTargetAddSources)
  gpCheckInTarget()
  set(__GP_TARGET_SOURCES ${__GP_TARGET_SOURCES} ${ARGN})
endmacro()

macro(gpTargetExcludeSourcesRegex REGEX_PATTERN)
  gpCheckInTarget()
  # list(FILTER ... EXCLUDE REGEX ...) removes items matching the regex
  list(FILTER __GP_TARGET_SOURCES EXCLUDE REGEX "${REGEX_PATTERN}")
  gpVerbose("Excluded sources matching '${REGEX_PATTERN}' from target '${__GP_TARGET_NAME}'")
endmacro()

macro(gpTargetExcludeDirectory DIRECTORY_NAME)
  gpCheckInTarget()
  # By wrapping the directory name in slashes, we ensure we match the exact folder name 
  # anywhere in the absolute path (e.g., "/private/linux/file.cpp")
  gpTargetExcludeSourcesRegex("/${DIRECTORY_NAME}/")
endmacro()

macro(gpTargetExcludeFile FILE_NAME)
  gpCheckInTarget()
  # Matches the specific file name at the end of a path
  gpTargetExcludeSourcesRegex("/${FILE_NAME}$")
endmacro()

macro(gpTargetAddPCHHeaders HEADER_PATH)
  gpCheckInTarget()
  list(APPEND __GP_TARGET_PCH_HEADERS ${HEADER_PATH})
endmacro()

macro(gpTargetAddSourcesFromDirectory DIRECTORY)
  gpCheckInTarget()
  file(GLOB_RECURSE _SOURCES_FROM_DIR "${DIRECTORY}/*.cpp" "${DIRECTORY}/*.c" "${DIRECTORY}/*.cc")
  set(__GP_TARGET_SOURCES ${__GP_TARGET_SOURCES} ${_SOURCES_FROM_DIR})
endmacro()

macro(gpSetTargetOutputName OUTPUT_NAME)
  gpCheckInTarget()
  set(__GP_TARGET_OUTPUT_NAME "${OUTPUT_NAME}")
endmacro()

macro(gpExecutableAddResource RESOURCE_PATH)
  gpCheckInTarget()
  if (NOT "${__GP_TARGET_TYPE}" STREQUAL "executable")
    gpFatal("gpExecutableAddResource can only be used within an executable target. Current target '${__GP_TARGET_NAME}' is of type '${__GP_TARGET_TYPE}'.")
  endif()
  set(__GP_EXEC_RESOURCES ${__GP_EXEC_RESOURCES} ${RESOURCE_PATH})
endmacro()

macro(gpExecutableSetIsTerminal IS_TERMINAL)
  gpCheckInTarget()
  if (NOT "${__GP_TARGET_TYPE}" STREQUAL "executable")
    gpFatal("gpExecutableSetIsTerminal can only be used within an executable target. Current target '${__GP_TARGET_NAME}' is of type '${__GP_TARGET_TYPE}'.")
  endif()
  set(__GP_EXEC_IS_TERMINAL ${IS_TERMINAL})
endmacro()
