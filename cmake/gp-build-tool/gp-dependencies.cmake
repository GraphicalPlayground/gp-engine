# Copyright (c) - Graphical Playground. All rights reserved.

include(gp-build-tool/gp-utils)

macro(gpAddPrivateDependency IN_DEP)
  # Check that we are currently defining a target
  gpCheckInTarget()

  string(TOLOWER "${IN_DEP}" DEP) # Normalize dependency name to lowercase for consistency

  # Check that the dependency is not already in the public dependencies to avoid conflicts
  if ("${DEP}" IN_LIST __GP_TARGET_PUB_DEPS)
    message(FATAL_ERROR "[GPBT] Dependency '${DEP}' cannot be added as a private dependency because it is already listed as a public dependency.")
  endif()

  # Check that the dependency is not already in the internal dependencies to avoid conflicts
  if ("${DEP}" IN_LIST __GP_TARGET_INT_DEPS)
    message(FATAL_ERROR "[GPBT] Dependency '${DEP}' cannot be added as a private dependency because it is already listed as an internal dependency.")
  endif()

  # Check that the dependency is not already in the private dependencies to avoid duplicates
  if ("${DEP}" IN_LIST __GP_TARGET_PRV_DEPS)
    message(FATAL_ERROR "[GPBT] Dependency '${DEP}' is already listed as a private dependency.")
  endif()

  list(APPEND __GP_TARGET_PRV_DEPS ${DEP})
  gpVerbose("Added private dependency '${DEP}' to target '${__GP_TARGET_NAME}'")
endmacro()

macro(gpAddPublicDependency IN_DEP)
  # Check that we are currently defining a target
  gpCheckInTarget()

  string(TOLOWER "${IN_DEP}" DEP) # Normalize dependency name to lowercase for consistency

  # Check that the dependency is not already in the private dependencies to avoid conflicts
  if ("${DEP}" IN_LIST __GP_TARGET_PRV_DEPS)
    message(FATAL_ERROR "[GPBT] Dependency '${DEP}' cannot be added as a public dependency because it is already listed as a private dependency.")
  endif()

  # Check that the dependency is not already in the internal dependencies to avoid conflicts
  if ("${DEP}" IN_LIST __GP_TARGET_INT_DEPS)
    message(FATAL_ERROR "[GPBT] Dependency '${DEP}' cannot be added as a public dependency because it is already listed as an internal dependency.")
  endif()

  # Check that the dependency is not already in the public dependencies to avoid duplicates
  if ("${DEP}" IN_LIST __GP_TARGET_PUB_DEPS)
    message(FATAL_ERROR "[GPBT] Dependency '${DEP}' is already listed as a public dependency.")
  endif()

  list(APPEND __GP_TARGET_PUB_DEPS ${DEP})
  gpVerbose("Added public dependency '${DEP}' to target '${__GP_TARGET_NAME}'")
endmacro()

macro(gpAddInternalDependency IN_DEP)
  # Check that we are currently defining a target
  gpCheckInTarget()

  string(TOLOWER "${IN_DEP}" DEP) # Normalize dependency name to lowercase for consistency

  # Check that the dependency is not already in the public dependencies to avoid conflicts
  if ("${DEP}" IN_LIST __GP_TARGET_PUB_DEPS)
    message(FATAL_ERROR "[GPBT] Dependency '${DEP}' cannot be added as a internal dependency because it is already listed as a public dependency.")
  endif()

  # Check that the dependency is not already in the private dependencies to avoid conflicts
  if ("${DEP}" IN_LIST __GP_TARGET_PRV_DEPS)
    message(FATAL_ERROR "[GPBT] Dependency '${DEP}' cannot be added as a internal dependency because it is already listed as an private dependency.")
  endif()

  # Check that the dependency is not already in the internal dependencies to avoid duplicates
  if ("${DEP}" IN_LIST __GP_TARGET_INT_DEPS)
    message(FATAL_ERROR "[GPBT] Dependency '${DEP}' is already listed as a internal dependency.")
  endif()

  list(APPEND __GP_TARGET_INT_DEPS ${DEP})
  gpVerbose("Added internal dependency '${DEP}' to target '${__GP_TARGET_NAME}'")
endmacro()
