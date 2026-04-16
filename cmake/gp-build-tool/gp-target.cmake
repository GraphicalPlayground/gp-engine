# Copyright (c) - Graphical Playground. All rights reserved.

include(gp-build-tool/gp-utils)
include(gp-build-tool/gp-includes)
include(gp-build-tool/gp-definitions)
include(gp-build-tool/gp-dependencies)
include(gp-build-tool/gp-internals)

# Header guard to prevent multiple inclusions of this file
if (DEFINED GP_BUILD_TOOL_TARGET_INCLUDED)
  return()
else()
  set(GP_BUILD_TOOL_TARGET_INCLUDED ON)
endif()

# Valid target types for validation
set(GP_VALID_TARGET_TYPES "module" "executable" "thirdparty")

macro(gpStartTarget TARGET_NAME TARGET_TYPE)
  # Check that we are not already in a target definition
  gpCheckNotInTarget()
  set(__GP_IN_TARGET ON)

  # Normalize target name and type to lowercase for consistency
  string(TOLOWER "${TARGET_NAME}" __GP_TARGET_NAME)
  string(TOLOWER "${TARGET_TYPE}" __GP_TARGET_TYPE)

  # Validate that target name is not empty
  if ("${__GP_TARGET_NAME}" STREQUAL "")
    message(FATAL_ERROR "[GPBT] Target name cannot be empty.")
  endif()

  gpGetCurrentPhase(__GP_CURRENT_PHASE)
  get_property(_CURRENT_TARGETS GLOBAL PROPERTY GP_REGISTERED_TARGETS)

  # Check that target name is unique
  if (__GP_CURRENT_PHASE STREQUAL "REGISTRATION")
    if ("${__GP_TARGET_NAME}" IN_LIST _CURRENT_TARGETS)
      message(FATAL_ERROR "[GPBT] Target name '${__GP_TARGET_NAME}' is already registered. Target names must be unique.")
    else()
      set_property(GLOBAL APPEND PROPERTY GP_REGISTERED_TARGETS "${__GP_TARGET_NAME}")
    endif()
  endif()

  # Validate target type
  if (NOT "${__GP_TARGET_TYPE}" IN_LIST GP_VALID_TARGET_TYPES)
    message(FATAL_ERROR "[GPBT] Invalid target type '${__GP_TARGET_TYPE}' for target '${__GP_TARGET_NAME}'. Valid types are: ${GP_VALID_TARGET_TYPES}")
  endif()

  # Dependencies
  set(__GP_TARGET_PUB_DEPS)
  set(__GP_TARGET_PRV_DEPS)
  set(__GP_TARGET_INT_DEPS)

  # Include Directories
  set(__GP_TARGET_PUB_INCLUDES)
  set(__GP_TARGET_PRV_INCLUDES)
  set(__GP_TARGET_INT_INCLUDES)

  # Compile Definitions
  set(__GP_TARGET_PUB_DEFS)
  set(__GP_TARGET_PRV_DEFS)
  set(__GP_TARGET_INT_DEFS)

  # Options
  set(__GP_TARGET_ENABLE_TESTS OFF)
  set(__GP_TARGET_ENABLE_BENCHMARKS OFF)
  set(__GP_TARGET_ENABLE_EXAMPLES OFF)
  set(__GP_TARGET_ENABLE_IPSC OFF)

  # Log the start of a new target definition
  message(STATUS "[GPBT] Starting definition of target '${__GP_TARGET_NAME}' of type '${__GP_TARGET_TYPE}'")

  if ("${__GP_TARGET_TYPE}" STREQUAL "module")
    gpAddPublicIncludesPath(${CMAKE_CURRENT_SOURCE_DIR}/public)
    gpAddPrivateIncludesPath(${CMAKE_CURRENT_SOURCE_DIR}/private)
    gpAddInternalIncludesPath(${CMAKE_CURRENT_SOURCE_DIR}/internal)
  elseif("${__GP_TARGET_TYPE}" STREQUAL "executable")
  elseif("${__GP_TARGET_TYPE}" STREQUAL "thirdparty")
  endif()
endmacro()

macro(gpEndTarget)
  # Check that we are currently in a target definition
  gpCheckInTarget()
  set(__GP_IN_TARGET OFF)

  # Get current process phase, if not set, default to REGISTRATION.
  gpGetCurrentPhase(__GP_CURRENT_PHASE)

  # Combine all dependencies into a single list for easy graph resolution later
  set(_ALL_DEPS ${__GP_TARGET_PUB_DEPS} ${__GP_TARGET_PRV_DEPS}  ${__GP_TARGET_INT_DEPS})
  # Store specific and combined dependencies using dynamic property names
  set_property(GLOBAL PROPERTY GP_TARGET_${__GP_TARGET_NAME}_PUB_DEPS "${__GP_TARGET_PUB_DEPS}")
  set_property(GLOBAL PROPERTY GP_TARGET_${__GP_TARGET_NAME}_PRV_DEPS "${__GP_TARGET_PRV_DEPS}")
  set_property(GLOBAL PROPERTY GP_TARGET_${__GP_TARGET_NAME}_INT_DEPS "${__GP_TARGET_INT_DEPS}")
  set_property(GLOBAL PROPERTY GP_TARGET_${__GP_TARGET_NAME}_ALL_DEPS "${_ALL_DEPS}")
  # Store location of the target for potential use in custom build steps or tools that need to know where the source files are
  set_property(GLOBAL PROPERTY GP_TARGET_${__GP_TARGET_NAME}_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}")

  if ("${__GP_CURRENT_PHASE}" STREQUAL "CONFIGURATION")
    __gpDefineCMakeTarget()
  endif()

  # Endup message
  message(STATUS "[GPBT] Finished definition of target '${__GP_TARGET_NAME}'")

  # Clean up target-specific variables to avoid accidental reuse in subsequent targets
  # Metadata
  unset(__GP_TARGET_NAME)
  unset(__GP_TARGET_TYPE)

  # Dependencies
  unset(__GP_TARGET_PUB_DEPS)
  unset(__GP_TARGET_PRV_DEPS)
  unset(__GP_TARGET_INT_DEPS)

  # Include Directories
  unset(__GP_TARGET_PUB_INCLUDES)
  unset(__GP_TARGET_PRV_INCLUDES)
  unset(__GP_TARGET_INT_INCLUDES)

  # Compile Definitions
  unset(__GP_TARGET_PUB_DEFS)
  unset(__GP_TARGET_PRV_DEFS)
  unset(__GP_TARGET_INT_DEFS)

  # Options
  unset(__GP_TARGET_ENABLE_TESTS)
  unset(__GP_TARGET_ENABLE_BENCHMARKS)
  unset(__GP_TARGET_ENABLE_EXAMPLES)
  unset(__GP_TARGET_ENABLE_IPSC)
endmacro()

# Convenience macros for common target types

macro(gpStartModule TARGET_NAME)
  gpStartTarget(${TARGET_NAME} module)
endmacro()

macro(gpStartExecutable TARGET_NAME)
  gpStartTarget(${TARGET_NAME} executable)
endmacro()

macro(gpStartThirdParty TARGET_NAME)
  gpStartTarget(${TARGET_NAME} thirdparty)
endmacro()

macro(gpEndModule)
  gpEndTarget()
endmacro()

macro(gpEndExecutable)
  gpEndTarget()
endmacro()

macro(gpEndThirdParty)
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
