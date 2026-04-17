# Copyright (c) - Graphical Playground. All rights reserved.
# For more information, see https://graphical-playground/legal
# mailto:support AT graphical-playground DOT com

include(gp-build-tool/gp-utils)

macro(__gpValidateCompilerFlagMismatch IN_FLAG)
  # Check if we are NOT using the MSVC compiler
  if (NOT MSVC)
    # Check if the flag starts with a '/' followed by a letter.
    # (The [A-Za-z] prevents accidentally flagging absolute Linux paths like /usr/local/...)
    if ("${IN_FLAG}" MATCHES "^/[A-Za-z]")
      message(FATAL_ERROR "[GPBT] Target '${__GP_TARGET_NAME}': Attempted to pass MSVC-style flag '${IN_FLAG}' to a non-MSVC compiler. Please wrap compiler-specific flags in an 'if(MSVC)' block.")
    endif()
  endif()

  # Strictly speaking, modern MSVC accepts many flags starting with '-' (like -O2 or -I), 
  # but if you want to enforce strict MSVC slash-syntax for Windows, you could add:
  # if (MSVC AND "${IN_FLAG}" MATCHES "^-[a-zA-Z]")
  #   message(WARNING "[GPBT] Target '${__GP_TARGET_NAME}': Passed GCC/Clang-style flag '${IN_FLAG}' to MSVC. Consider using '/${CMAKE_MATCH_1}' instead.")
  # endif()
endmacro()

macro(gpAddPrivateCompileFlag IN_FLAG)
  gpCheckInTarget()
  __gpValidateCompilerFlagMismatch("${IN_FLAG}")

  # Check for visibility conflicts
  if ("${IN_FLAG}" IN_LIST __GP_TARGET_PUB_COMP_FLAGS)
    message(FATAL_ERROR "[GPBT] Flag '${IN_FLAG}' cannot be private because it is already public.")
  endif()

  # Avoid duplicates
  if (NOT "${IN_FLAG}" IN_LIST __GP_TARGET_PRV_COMP_FLAGS)
    list(APPEND __GP_TARGET_PRV_COMP_FLAGS "${IN_FLAG}")
    gpVerbose("Added private compile flag '${IN_FLAG}' to target '${__GP_TARGET_NAME}'")
  endif()
endmacro()

macro(gpAddPublicCompileFlag IN_FLAG)
  gpCheckInTarget()
  __gpValidateCompilerFlagMismatch("${IN_FLAG}")

  # Check for visibility conflicts
  if ("${IN_FLAG}" IN_LIST __GP_TARGET_PRV_COMP_FLAGS)
    message(FATAL_ERROR "[GPBT] Flag '${IN_FLAG}' cannot be public because it is already private.")
  endif()

  # Avoid duplicates
  if (NOT "${IN_FLAG}" IN_LIST __GP_TARGET_PUB_COMP_FLAGS)
    list(APPEND __GP_TARGET_PUB_COMP_FLAGS "${IN_FLAG}")
    gpVerbose("Added public compile flag '${IN_FLAG}' to target '${__GP_TARGET_NAME}'")
  endif()
endmacro()

macro(gpTargetReplaceCompileFlag NEW_FLAG CONFLICT_REGEX VISIBILITY)
  gpCheckInTarget()
  __gpValidateCompilerFlagMismatch("${NEW_FLAG}")

  string(TOUPPER "${VISIBILITY}" _VIS)

  if ("${_VIS}" STREQUAL "PRIVATE")
    # Strip any existing flags that match the conflict regex
    list(FILTER __GP_TARGET_PRV_COMP_FLAGS EXCLUDE REGEX "${CONFLICT_REGEX}")
    # Safely add the new flag
    gpAddPrivateCompileFlag("${NEW_FLAG}")
  elseif ("${_VIS}" STREQUAL "PUBLIC")
    list(FILTER __GP_TARGET_PUB_COMP_FLAGS EXCLUDE REGEX "${CONFLICT_REGEX}")
    gpAddPublicCompileFlag("${NEW_FLAG}")
  else()
    message(FATAL_ERROR "[GPBT] Invalid visibility '${VISIBILITY}' for gpTargetReplaceCompileFlag. Use PRIVATE or PUBLIC.")
  endif()
endmacro()
