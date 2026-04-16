# Copyright (c) - Graphical Playground. All rights reserved.

include(gp-build-tool/gp-utils)

macro(gpAddPrivateDefinitions IN_DEFS)
  # Check that we are currently defining a target
  gpCheckInTarget()

  string(TOLOWER "${IN_DEFS}" DEF) # Normalize definitions name to lowercase for consistency

  # Check that the definitions is not already in the public definitions to avoid conflicts
  if ("${DEF}" IN_LIST __GP_TARGET_PUB_DEFS)
    message(FATAL_ERROR "[GPBT] definitions '${DEF}' cannot be added as a private definitions because it is already listed as a public definitions.")
  endif()

  # Check that the definitions is not already in the internal definitions to avoid conflicts
  if ("${DEF}" IN_LIST __GP_TARGET_INT_DEFS)
    message(FATAL_ERROR "[GPBT] definitions '${DEF}' cannot be added as a private definitions because it is already listed as an internal definitions.")
  endif()

  # Check that the definitions is not already in the private definitions to avoid duplicates
  if ("${DEF}" IN_LIST __GP_TARGET_PRV_DEFS)
    message(FATAL_ERROR "[GPBT] definitions '${DEF}' is already listed as a private definitions.")
  endif()

  list(APPEND __GP_TARGET_PRV_DEFS ${DEF})
  gpVerbose("Added private definitions '${DEF}' to target '${__GP_TARGET_NAME}'")
endmacro()

macro(gpAddPublicDefinitions IN_DEFS)
  # Check that we are currently defining a target
  gpCheckInTarget()

  string(TOLOWER "${IN_DEFS}" DEF) # Normalize definitions name to lowercase for consistency

  # Check that the definitions is not already in the private definitions to avoid conflicts
  if ("${DEF}" IN_LIST __GP_TARGET_PRV_DEFS)
    message(FATAL_ERROR "[GPBT] definitions '${DEF}' cannot be added as a public definitions because it is already listed as a private definitions.")
  endif()

  # Check that the definitions is not already in the internal definitions to avoid conflicts
  if ("${DEF}" IN_LIST __GP_TARGET_INT_DEFS)
    message(FATAL_ERROR "[GPBT] definitions '${DEF}' cannot be added as a public definitions because it is already listed as an internal definitions.")
  endif()

  # Check that the definitions is not already in the public definitions to avoid duplicates
  if ("${DEF}" IN_LIST __GP_TARGET_PUB_DEFS)
    message(FATAL_ERROR "[GPBT] definitions '${DEF}' is already listed as a public definitions.")
  endif()

  list(APPEND __GP_TARGET_PUB_DEFS ${DEF})
  gpVerbose("Added public definitions '${DEF}' to target '${__GP_TARGET_NAME}'")
endmacro()

macro(gpAddInternalDefinitions IN_DEFS)
  # Check that we are currently defining a target
  gpCheckInTarget()

  string(TOLOWER "${IN_DEFS}" DEF) # Normalize definitions name to lowercase for consistency

  # Check that the definitions is not already in the public definitions to avoid conflicts
  if ("${DEF}" IN_LIST __GP_TARGET_PUB_DEFS)
    message(FATAL_ERROR "[GPBT] definitions '${DEF}' cannot be added as a internal definitions because it is already listed as a public definitions.")
  endif()

  # Check that the definitions is not already in the private definitions to avoid conflicts
  if ("${DEF}" IN_LIST __GP_TARGET_PRV_DEFS)
    message(FATAL_ERROR "[GPBT] definitions '${DEF}' cannot be added as a internal definitions because it is already listed as an private definitions.")
  endif()

  # Check that the definitions is not already in the internal definitions to avoid duplicates
  if ("${DEF}" IN_LIST __GP_TARGET_INT_DEFS)
    message(FATAL_ERROR "[GPBT] definitions '${DEF}' is already listed as a internal definitions.")
  endif()

  list(APPEND __GP_TARGET_INT_DEFS ${DEF})
  gpVerbose("Added internal definitions '${DEF}' to target '${__GP_TARGET_NAME}'")
endmacro()
