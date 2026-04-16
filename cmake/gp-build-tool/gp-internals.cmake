# Copyright (c) - Graphical Playground. All rights reserved.

include(gp-build-tool/gp-utils)

macro(__gpDefineCMakeTarget)
  gpCheckInTarget()

  gpGetCurrentPhase(CURRENT_PHASE)
  if (NOT CURRENT_PHASE STREQUAL "CONFIGURATION")
    message(FATAL_ERROR "[GPBT] Internal error: __gpDefineCMakeTargets should only be called during the CONFIGURATION phase. Current phase: ${CURRENT_PHASE}")
  endif()

  # TODO: Add proper target configuration here

  if(__GP_TARGET_ENABLE_IPSC)
    __gpDefineIPSCTarget()
  endif()

  if (__GP_TARGET_ENABLE_TESTS)
    __gpDefineTestsTarget()
  endif()

  if(__GP_TARGET_ENABLE_BENCHMARKS)
    __gpDefineBenchmarksTarget()
  endif()

  if(__GP_TARGET_ENABLE_EXAMPLES)
    __gpDefineExamplesTarget()
  endif()
endmacro()

macro(__gpDefineIPSCTarget)
  gpCheckInTarget()

  gpGetCurrentPhase(CURRENT_PHASE)
  if (NOT CURRENT_PHASE STREQUAL "CONFIGURATION")
    message(FATAL_ERROR "[GPBT] Internal error: __gpDefineIPSCTarget should only be called during the CONFIGURATION phase. Current phase: ${CURRENT_PHASE}")
  endif()

  message(WARNING "[GPBT] IPSC target generation is not yet implemented for target '${__GP_TARGET_NAME}'.")
endmacro()

macro(__gpDefineTestsTarget)
  gpCheckInTarget()

  gpGetCurrentPhase(CURRENT_PHASE)
  if (NOT CURRENT_PHASE STREQUAL "CONFIGURATION")
    message(FATAL_ERROR "[GPBT] Internal error: __gpDefineTestsTarget should only be called during the CONFIGURATION phase. Current phase: ${CURRENT_PHASE}")
  endif()

  message(WARNING "[GPBT] Tests target generation is not yet implemented for target '${__GP_TARGET_NAME}'.")
endmacro()

macro(__gpDefineBenchmarksTarget)
  gpCheckInTarget()

  gpGetCurrentPhase(CURRENT_PHASE)
  if (NOT CURRENT_PHASE STREQUAL "CONFIGURATION")
    message(FATAL_ERROR "[GPBT] Internal error: __gpDefineBenchmarksTarget should only be called during the CONFIGURATION phase. Current phase: ${CURRENT_PHASE}")
  endif()

  message(WARNING "[GPBT] Benchmarks target generation is not yet implemented for target '${__GP_TARGET_NAME}'.")
endmacro()

macro(__gpDefineExamplesTarget)
  gpCheckInTarget()

  gpGetCurrentPhase(CURRENT_PHASE)
  if (NOT CURRENT_PHASE STREQUAL "CONFIGURATION")
    message(FATAL_ERROR "[GPBT] Internal error: __gpDefineExamplesTarget should only be called during the CONFIGURATION phase. Current phase: ${CURRENT_PHASE}")
  endif()

  message(WARNING "[GPBT] Examples target generation is not yet implemented for target '${__GP_TARGET_NAME}'.")
endmacro()
