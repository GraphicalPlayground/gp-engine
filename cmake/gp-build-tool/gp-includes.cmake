# Copyright (c) - Graphical Playground. All rights reserved.

include(gp-build-tool/gp-utils)

macro(gpAddPrivateIncludesPath INCLUDES)
  # Check that we are currently defining a target
  gpCheckInTarget()

  # Check that the includes path is not already in the public includes paths to avoid conflicts
  if ("${INCLUDES}" IN_LIST __GP_TARGET_PUB_INCLUDES)
    message(FATAL_ERROR "[GPBT] includes path '${INCLUDES}' cannot be added as a private includes path because it is already listed as a public includes path.")
  endif()

  # Check that the includes path is not already in the internal includes paths to avoid conflicts
  if ("${INCLUDES}" IN_LIST __GP_TARGET_INT_INCLUDES)
    message(FATAL_ERROR "[GPBT] includes path '${INCLUDES}' cannot be added as a private includes path because it is already listed as an internal includes path.")
  endif()

  # Check that the includes path is not already in the private includes paths to avoid duplicates
  if ("${INCLUDES}" IN_LIST __GP_TARGET_PRV_INCLUDES)
    message(FATAL_ERROR "[GPBT] includes path '${INCLUDES}' is already listed as a private includes path.")
  endif()

  list(APPEND __GP_TARGET_PRV_INCLUDES ${INCLUDES})
  gpVerbose("Added private includes path '${INCLUDES}' to target '${__GP_TARGET_NAME}'")
endmacro()

macro(gpAddPublicIncludesPath INCLUDES)
  # Check that we are currently defining a target
  gpCheckInTarget()

  # Check that the includes path is not already in the private includes paths to avoid conflicts
  if ("${INCLUDES}" IN_LIST __GP_TARGET_PRV_INCLUDES)
    message(FATAL_ERROR "[GPBT] includes path '${INCLUDES}' cannot be added as a public includes path because it is already listed as a private includes path.")
  endif()

  # Check that the includes path is not already in the internal includes paths to avoid conflicts
  if ("${INCLUDES}" IN_LIST __GP_TARGET_INT_INCLUDES)
    message(FATAL_ERROR "[GPBT] includes path '${INCLUDES}' cannot be added as a public includes path because it is already listed as an internal includes path.")
  endif()

  # Check that the includes path is not already in the public includes paths to avoid duplicates
  if ("${INCLUDES}" IN_LIST __GP_TARGET_PUB_INCLUDES)
    message(FATAL_ERROR "[GPBT] includes path '${INCLUDES}' is already listed as a public includes path.")
  endif()

  list(APPEND __GP_TARGET_PUB_INCLUDES ${INCLUDES})
  gpVerbose("Added public includes path '${INCLUDES}' to target '${__GP_TARGET_NAME}'")
endmacro()

macro(gpAddInternalIncludesPath INCLUDES)
  # Check that we are currently defining a target
  gpCheckInTarget()

  # Check that the includes path is not already in the public includes paths to avoid conflicts
  if ("${INCLUDES}" IN_LIST __GP_TARGET_PUB_INCLUDES)
    message(FATAL_ERROR "[GPBT] includes path '${INCLUDES}' cannot be added as a internal includes path because it is already listed as a public includes path.")
  endif()

  # Check that the includes path is not already in the private includes paths to avoid conflicts
  if ("${INCLUDES}" IN_LIST __GP_TARGET_PRV_INCLUDES)
    message(FATAL_ERROR "[GPBT] includes path '${INCLUDES}' cannot be added as a internal includes path because it is already listed as an private includes path.")
  endif()

  # Check that the includes path is not already in the internal includes paths to avoid duplicates
  if ("${INCLUDES}" IN_LIST __GP_TARGET_INT_INCLUDES)
    message(FATAL_ERROR "[GPBT] includes path '${INCLUDES}' is already listed as a internal includes path.")
  endif()

  list(APPEND __GP_TARGET_INT_INCLUDES ${INCLUDES})
  gpVerbose("Added internal includes path '${INCLUDES}' to target '${__GP_TARGET_NAME}'")
endmacro()
