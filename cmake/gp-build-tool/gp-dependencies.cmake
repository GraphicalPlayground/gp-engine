# Copyright (c) - Graphical Playground. All rights reserved.
# For more information, see https://graphical-playground/legal
# mailto:support AT graphical-playground DOT com

include(gp-build-tool/gp-utils)

macro(gpAddPrivateDependency DEP)
  # Check that we are currently defining a target
  gpCheckInTarget()

  # Check that the dependency is not already in the public dependencies to avoid conflicts
  if ("${DEP}" IN_LIST __GP_TARGET_PUB_DEPS)
    message(FATAL_ERROR "[GPBT] Dependency '${DEP}' cannot be added as a private dependency because it is already listed as a public dependency.")
  endif()

  # Check that the dependency is not already in the internal dependencies to avoid conflicts
  if ("${DEP}" IN_LIST __GP_TARGET_INT_DEPS)
    message(FATAL_ERROR "[GPBT] Dependency '${DEP}' cannot be added as a private dependency because it is already listed as an internal dependency.")
  endif()

  # Check that the dependency is not already in the dynamic dependencies to avoid conflicts
  if ("${DEP}" IN_LIST __GP_TARGET_DYN_DEPS)
    message(FATAL_ERROR "[GPBT] Dependency '${DEP}' cannot be added as a private dependency because it is already listed as a dynamic dependency.")
  endif()

  # Check that the dependency is not already in the private dependencies to avoid duplicates
  if ("${DEP}" IN_LIST __GP_TARGET_PRV_DEPS)
    message(FATAL_ERROR "[GPBT] Dependency '${DEP}' is already listed as a private dependency.")
  endif()

  list(APPEND __GP_TARGET_PRV_DEPS ${DEP})
  gpVerbose("Added private dependency '${DEP}' to target '${__GP_TARGET_NAME}'")
endmacro()

macro(gpAddPublicDependency DEP)
  # Check that we are currently defining a target
  gpCheckInTarget()

  # Check that the dependency is not already in the private dependencies to avoid conflicts
  if ("${DEP}" IN_LIST __GP_TARGET_PRV_DEPS)
    message(FATAL_ERROR "[GPBT] Dependency '${DEP}' cannot be added as a public dependency because it is already listed as a private dependency.")
  endif()

  # Check that the dependency is not already in the internal dependencies to avoid conflicts
  if ("${DEP}" IN_LIST __GP_TARGET_INT_DEPS)
    message(FATAL_ERROR "[GPBT] Dependency '${DEP}' cannot be added as a public dependency because it is already listed as an internal dependency.")
  endif()

  # Check that the dependency is not already in the dynamic dependencies to avoid conflicts
  if ("${DEP}" IN_LIST __GP_TARGET_DYN_DEPS)
    message(FATAL_ERROR "[GPBT] Dependency '${DEP}' cannot be added as a public dependency because it is already listed as a dynamic dependency.")
  endif()

  # Check that the dependency is not already in the public dependencies to avoid duplicates
  if ("${DEP}" IN_LIST __GP_TARGET_PUB_DEPS)
    message(FATAL_ERROR "[GPBT] Dependency '${DEP}' is already listed as a public dependency.")
  endif()

  list(APPEND __GP_TARGET_PUB_DEPS ${DEP})
  gpVerbose("Added public dependency '${DEP}' to target '${__GP_TARGET_NAME}'")
endmacro()

macro(gpAddInternalDependency DEP)
  # Check that we are currently defining a target
  gpCheckInTarget()

  # Check that the dependency is not already in the public dependencies to avoid conflicts
  if ("${DEP}" IN_LIST __GP_TARGET_PUB_DEPS)
    message(FATAL_ERROR "[GPBT] Dependency '${DEP}' cannot be added as a internal dependency because it is already listed as a public dependency.")
  endif()

  # Check that the dependency is not already in the private dependencies to avoid conflicts
  if ("${DEP}" IN_LIST __GP_TARGET_PRV_DEPS)
    message(FATAL_ERROR "[GPBT] Dependency '${DEP}' cannot be added as a internal dependency because it is already listed as an private dependency.")
  endif()

  # Check that the dependency is not already in the dynamic dependencies to avoid conflicts
  if ("${DEP}" IN_LIST __GP_TARGET_DYN_DEPS)
    message(FATAL_ERROR "[GPBT] Dependency '${DEP}' cannot be added as a internal dependency because it is already listed as a dynamic dependency.")
  endif()

  # Check that the dependency is not already in the internal dependencies to avoid duplicates
  if ("${DEP}" IN_LIST __GP_TARGET_INT_DEPS)
    message(FATAL_ERROR "[GPBT] Dependency '${DEP}' is already listed as a internal dependency.")
  endif()

  list(APPEND __GP_TARGET_INT_DEPS ${DEP})
  gpVerbose("Added internal dependency '${DEP}' to target '${__GP_TARGET_NAME}'")
endmacro()

macro(gpAddDynamicDependency DEP)
  # Check that we are currently defining a target
  gpCheckInTarget()

  # Check that the dependency is not already in the public dependencies to avoid conflicts
  if ("${DEP}" IN_LIST __GP_TARGET_PUB_DEPS)
    message(FATAL_ERROR "[GPBT] Dependency '${DEP}' cannot be added as a dynamic dependency because it is already listed as a public dependency.")
  endif()

  # Check that the dependency is not already in the private dependencies to avoid conflicts
  if ("${DEP}" IN_LIST __GP_TARGET_PRV_DEPS)
    message(FATAL_ERROR "[GPBT] Dependency '${DEP}' cannot be added as a dynamic dependency because it is already listed as an private dependency.")
  endif()

  # Check that the dependency is not already in the internal dependencies to avoid conflicts
  if ("${DEP}" IN_LIST __GP_TARGET_INT_DEPS)
    message(FATAL_ERROR "[GPBT] Dependency '${DEP}' cannot be added as a dynamic dependency because it is already listed as a internal dependency.")
  endif()

  # Check that the dependency is not already in the dynamic dependencies to avoid duplicates
  if ("${DEP}" IN_LIST __GP_TARGET_DYN_DEPS)
    message(FATAL_ERROR "[GPBT] Dependency '${DEP}' is already listed as a dynamic dependency.")
  endif()

  list(APPEND __GP_TARGET_DYN_DEPS ${DEP})
  gpVerbose("Added dynamic dependency '${DEP}' to target '${__GP_TARGET_NAME}'")
endmacro()
