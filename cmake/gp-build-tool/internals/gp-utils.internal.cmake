# Copyright (c) - Graphical Playground. All rights reserved.
# For more information, see https://graphical-playground/legal
# mailto:support AT graphical-playground DOT com

include(gp-build-tool/internals/gp-logger.internal)

# @brief Internal function to set the current build phase. This is used to control the flow of the
#        build process and ensure that certain operations only happen during the appropriate phase.
# @param[in] newPhase The new build phase to transition to. Must be one of the allowed phases defined in GPBT_ALLOWED_PHASES.
function(_gpSetCurrentPhase newPhase)
  set(GPBT_ALLOWED_PHASES "REGISTRATION" "CONFIGURATION" "GENERATION")
  if (NOT "${newPhase}" IN_LIST GPBT_ALLOWED_PHASES)
    gpFatal("Invalid build phase '${newPhase}'. Allowed phases are ${GPBT_ALLOWED_PHASES}")
  endif()
  get_property(currentPhase GLOBAL PROPERTY GPBT_CURRENT_PHASE)
  if ("${currentPhase}" STREQUAL "${newPhase}")
    gpFatal("Attempting to set build phase to '${newPhase}', but it is already the current phase. This likely indicates a logic error in the GP Build Tool's internal phase management.")
  endif()
  set_property(GLOBAL PROPERTY GPBT_CURRENT_PHASE "${newPhase}")
  gpLog("Build phase set to '${newPhase}'")
endfunction()

# @brief Internal function to retrieve the current build phase.
# @param[out] outVar The variable to store the current build phase in.
function(_gpGetCurrentPhase outVar)
  get_property(currentPhase GLOBAL PROPERTY GPBT_CURRENT_PHASE)
  if (NOT currentPhase)
    _gpSetCurrentPhase("REGISTRATION")
    set(currentPhase "REGISTRATION")
  endif()
  set(${outVar} "${currentPhase}" PARENT_SCOPE)
endfunction()
