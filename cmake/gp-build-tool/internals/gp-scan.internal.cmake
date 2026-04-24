# Copyright (c) - Graphical Playground. All rights reserved.
# For more information, see https://graphical-playground/legal
# mailto:support AT graphical-playground DOT com

include(gp-build-tool/internals/gp-utils.internal)
include(gp-build-tool/internals/gp-logger.internal)
include(gp-build-tool/internals/gp-scope.internal)

# @brief Internal function to perform a topological sort of the registered targets based on their dependencies. This is used to determine the correct build order of the targets.
#        It uses a depth-first search approach to sort the targets. If it detects a circular dependency, it will report an error and stop the configuration process.
# @param[out] outSortedList The output variable where the sorted list of targets will be stored.
function(_implGpSortTargets outSortedList)
  # Retrieve the master list of all registered targets
  get_property(registeredTargets GLOBAL PROPERTY GPBT_REGISTERED_TARGETS)

  # Verbose logging of the registered targets for debugging purposes
  gpVerbose("Registered targets before sorting: ${registeredTargets}")

  # unsorted starts with all targets. sorted starts empty.
  set(unsorted ${registeredTargets})
  set(sorted "")

  # Loop as long as we have unsorted targets AND we are making progress
  set(progressMade TRUE)
  while(unsorted AND progressMade)
    set(progressMade FALSE)
    set(nextUnsorted "")

    foreach(target IN LISTS unsorted)
      # To upper snake case the target name for consistent property access
      gpToUpperSnakeCase("${target}" targetUpper)

      # Fetch the combined dependencies for the current target
      get_property(allDependencies GLOBAL PROPERTY GPBT_TARGET_${targetUpper}_ALL_DEPS)

      # Check if all of this target's dependencies are already in the sorted list
      set(allDependenciesMet TRUE)
      foreach(dependency IN LISTS allDependencies)
        # We ONLY care about resolving dependencies that are registered GP targets.
        # (This prevents the script from freezing if a target depends on an external OS library).
        if(dependency IN_LIST registeredTargets)
          if(NOT dependency IN_LIST sorted)
            set(allDependenciesMet FALSE)
            break() # Stop checking, we already know we can't sort this target yet
          endif()
        endif()
      endforeach()

      if(allDependenciesMet)
        # Success! All internal dependencies are met. Add to the sorted list.
        list(APPEND sorted ${target})
        set(progressMade TRUE) # We successfully processed at least one target
      else()
        # Not ready yet. Push it to the next pass.
        list(APPEND nextUnsorted ${target})
      endif()
    endforeach()

    # Update the unsorted list for the next iteration of the while loop
    set(unsorted ${nextUnsorted})
  endwhile()

  # Circular Dependency Detection
  # If the loop exited but unsorted is not empty, it means we stopped making progress.
  if(unsorted)
    gpFatal("Circular dependency detected! The following modules form an infinite loop and cannot be ordered: ${unsorted}")
  endif()

  # Return the sorted list to the caller's scope
  set(${outSortedList} "${sorted}" PARENT_SCOPE)
endfunction()

# @brief Internal recursive function to scan the source tree for modules. This function is called by _implGpScanForTargets and should not be called directly.
#        It traverses the directory structure starting from the given directory, looking for CMakeLists.txt files that indicate the presence of a module.
#        When it finds one, it registers the module and stops recursing that branch. If it doesn't find one, it continues to dive deeper into the subdirectories.
# @param[in] currentDirectory The directory to scan for modules. This should initially be called with the root source directory of the project.
function(_implGpScanRecursive currentDirectory)
  file(GLOB entries RELATIVE ${currentDirectory} ${currentDirectory}/*)

  foreach(entry ${entries})
    set(fullPath "${currentDirectory}/${entry}")

    if(IS_DIRECTORY "${fullPath}")
      if(EXISTS "${fullPath}/CMakeLists.txt")
        # Found a module! Register it and STOP recursing this branch
        # add_subdirectory uses paths relative to CMAKE_CURRENT_SOURCE_DIR
        file(RELATIVE_PATH relativePath ${CMAKE_CURRENT_SOURCE_DIR} ${fullPath})
        add_subdirectory(${relativePath})
      else()
        # No CMakeLists here, dive deeper into this subfolder
        _implGpScanRecursive("${fullPath}")
      endif()
    endif()
  endforeach()
endfunction()

# @brief Main function to scan the source tree for modules, register them, sort them based on dependencies, and include their CMakeLists.txt files in the correct order.
#        This function orchestrates the entire scanning and configuration process for the build tool.
function(_implGpScanForTargets)
  # Set the current phase to REGISTRATION to allow modules to register themselves as we scan through the directories.
  _gpSetCurrentPhase("REGISTRATION")

  # Scan the entire source tree for modules.
  # Each module will register itself during this process, and we will build up a master list of all targets and their dependencies.
  _implGpScanRecursive("${CMAKE_CURRENT_SOURCE_DIR}")

  # Now that we've registered all targets, we need to sort them based on their dependencies to determine the correct build order.
  _implGpSortTargets(orderedTargetList)

  # Verbose logging of the final sorted target list for debugging purposes
  gpVerbose("Final sorted target list: ${orderedTargetList}")

  # Finally, we transition to the CONFIGURATION phase, which allows modules to perform any necessary configuration steps now that
  # we have a complete picture of all targets and their dependencies.
  _gpSetCurrentPhase("CONFIGURATION")

  # Include each module's CMakeLists.txt in the correct order. This will allow them to configure their targets,
  # knowing that all dependencies have been registered and sorted.
  foreach(module ${orderedTargetList})
    string(REPLACE "/" "_" moduleClean "${module}")
    gpToUpperSnakeCase("${moduleClean}" moduleUpper)
    get_property(moduleLocation GLOBAL PROPERTY GPBT_TARGET_${moduleUpper}_LOCATION)
    if (NOT moduleLocation)
      gpFatal("Could not find location for module '${module}'. Make sure it is registered correctly.")
    endif()
    gpVerbose("Configuring module '${module}' located at '${moduleLocation}'")
    include(${moduleLocation}/CMakeLists.txt)
  endforeach(module)
endfunction()
