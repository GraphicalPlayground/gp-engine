# Copyright (c) - Graphical Playground. All rights reserved.

macro(gpCheckInTarget)
  if (NOT __GP_IN_TARGET)
    message(FATAL_ERROR "[GPBT] Target definition macros (gpStartTarget/gpEndTarget) must be used to define targets.")
  endif()
endmacro()

macro(gpCheckNotInTarget)
  if (__GP_IN_TARGET)
    message(FATAL_ERROR "[GPBT] Target definition macros (gpStartTarget/gpEndTarget) cannot be nested.")
  endif()
endmacro()

macro(gpVerbose MESSAGE)
  if(GP_BUILD_TOOL_VERBOSE)
    message(STATUS "[GPBT] ~~ ${MESSAGE}")
  endif()
endmacro()

function(gpSortTargets OUT_SORTED_LIST)
  # Retrieve the master list of all registered targets
  get_property(_ALL_TARGETS GLOBAL PROPERTY GP_REGISTERED_TARGETS)

  # _UNSORTED starts with all targets. _SORTED starts empty.
  set(_UNSORTED ${_ALL_TARGETS})
  set(_SORTED "")

  # Loop as long as we have unsorted targets AND we are making progress
  set(_PROGRESS_MADE TRUE)
  while(_UNSORTED AND _PROGRESS_MADE)
    set(_PROGRESS_MADE FALSE)
    set(_NEXT_UNSORTED "")

    foreach(_TARGET IN LISTS _UNSORTED)
      # Fetch the combined dependencies for the current target
      get_property(_DEPS GLOBAL PROPERTY GP_TARGET_${_TARGET}_ALL_DEPS)

      # Check if all of this target's dependencies are already in the _SORTED list
      set(_ALL_DEPS_MET TRUE)
      foreach(_DEP IN LISTS _DEPS)
        # We ONLY care about resolving dependencies that are registered GP targets.
        # (This prevents the script from freezing if a target depends on an external OS library).
        if(_DEP IN_LIST _ALL_TARGETS)
          if(NOT _DEP IN_LIST _SORTED)
            set(_ALL_DEPS_MET FALSE)
            break() # Stop checking, we already know we can't sort this target yet
          endif()
        endif()
      endforeach()

      if(_ALL_DEPS_MET)
        # Success! All internal dependencies are met. Add to the sorted list.
        list(APPEND _SORTED ${_TARGET})
        set(_PROGRESS_MADE TRUE) # We successfully processed at least one target
      else()
        # Not ready yet. Push it to the next pass.
        list(APPEND _NEXT_UNSORTED ${_TARGET})
      endif()
    endforeach()

    # Update the unsorted list for the next iteration of the while loop
    set(_UNSORTED ${_NEXT_UNSORTED})
  endwhile()

  # Circular Dependency Detection
  # If the loop exited but _UNSORTED is not empty, it means we stopped making progress.
  if(_UNSORTED)
    message(FATAL_ERROR "[GPBT] Circular dependency detected! The following modules form an infinite loop and cannot be ordered: ${_UNSORTED}")
  endif()

  # Return the sorted list to the caller's scope
  set(${OUT_SORTED_LIST} "${_SORTED}" PARENT_SCOPE)
endfunction()

function (gpSetCurrentPhase NEW_PHASE)
  set(ALLOWED_PHASES "REGISTRATION" "CONFIGURATION" "GENERATION")
  if (NOT "${NEW_PHASE}" IN_LIST ALLOWED_PHASES)
    message(FATAL_ERROR "[GPBT] Invalid build phase '${NEW_PHASE}'. Allowed phases are ${ALLOWED_PHASES}")
  endif()
  get_property(CURRENT_PHASE GLOBAL PROPERTY GP_CURRENT_PHASE)
  if ("${CURRENT_PHASE}" STREQUAL "${NEW_PHASE}")
    message(FATAL_ERROR "[GPBT] Attempting to set build phase to '${NEW_PHASE}', but it is already the current phase. This likely indicates a logic error in the GP Build Tool's internal phase management.")
  endif()
  set_property(GLOBAL PROPERTY GP_CURRENT_PHASE "${NEW_PHASE}")
  message(STATUS "[GPBT] Build phase set to '${NEW_PHASE}'")
endfunction()

function (gpGetCurrentPhase RESULT_VAR)
  get_property(_VAL GLOBAL PROPERTY GP_CURRENT_PHASE)
  if (NOT _VAL)
    gpSetCurrentPhase("REGISTRATION")
    set(_VAL "REGISTRATION")
  endif()
  set(${RESULT_VAR} "${_VAL}" PARENT_SCOPE)
endfunction()

function(__gpInternalScanRecursive CURRENT_DIR)
  file(GLOB ENTRIES RELATIVE ${CURRENT_DIR} ${CURRENT_DIR}/*)

  foreach(ENTRY ${ENTRIES})
    set(FULL_PATH "${CURRENT_DIR}/${ENTRY}")

    if(IS_DIRECTORY "${FULL_PATH}")
      if(EXISTS "${FULL_PATH}/CMakeLists.txt")
        # Found a module! Register it and STOP recursing this branch
        # add_subdirectory uses paths relative to CMAKE_CURRENT_SOURCE_DIR
        file(RELATIVE_PATH REL_PATH ${CMAKE_CURRENT_SOURCE_DIR} ${FULL_PATH})
        add_subdirectory(${REL_PATH})
      else()
        # No CMakeLists here, dive deeper into this subfolder
        __gpInternalScanRecursive("${FULL_PATH}")
      endif()
    endif()
  endforeach()
endfunction()

macro(gpScanForModules)
  gpSetCurrentPhase("REGISTRATION")

  # Start the recursive search from the current directory
  __gpInternalScanRecursive("${CMAKE_CURRENT_SOURCE_DIR}")

  gpSortTargets(MY_ORDERED_MODULES)
  message(STATUS "[GPBT] Correct compilation order: ${MY_ORDERED_MODULES}")
  gpSetCurrentPhase("CONFIGURATION")
  foreach(MODULE ${MY_ORDERED_MODULES})
    get_property(MODULE_LOCATION GLOBAL PROPERTY GP_TARGET_${MODULE}_LOCATION)
    if (NOT MODULE_LOCATION)
      message(FATAL_ERROR "[GPBT] Could not find location for module '${MODULE}'. Make sure it is registered correctly.")
    endif()
    gpVerbose("Configuring module '${MODULE}' located at '${MODULE_LOCATION}'")
    include(${MODULE_LOCATION}/CMakeLists.txt)
  endforeach()
endmacro()
