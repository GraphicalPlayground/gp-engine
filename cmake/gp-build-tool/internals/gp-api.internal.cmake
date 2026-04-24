# Copyright (c) - Graphical Playground. All rights reserved.
# For more information, see https://graphical-playground/legal
# mailto:support AT graphical-playground DOT com

include(gp-build-tool/internals/gp-logger.internal)
include(gp-build-tool/internals/gp-scope.internal)
include(gp-build-tool/internals/gp-stringify.internal)

# @brief Internal macro to add a compile definition to the target with the specified visibility. This is used to implement gpAddPublicDefinitions, gpAddPrivateDefinitions, and gpAddInternalDefinitions.
# @param[in] visibility The visibility level for the definition (public, private, or internal).
# @param[in] definitions The compile definition(s) to add to the target. Can be a single definition or a list of definitions.
#            Each definition can be in the form 'KEY' or 'KEY=VALUE'. If only 'KEY' is provided, it will be treated as 'KEY=1'.
macro(_implGpAddDefinitions visibility definitions)
  # Check that we're in a target scope
  gpFatalIfNotInNamedScope("target" "gpAddDefinitions can only be called within a target scope.")

  # Set the available visibility levels for definitions
  set(GPBT_AVAILABLE_VISIBILITY "public;private;internal")

  # Verify that the provided visibility is valid
  if(NOT "${visibility}" IN_LIST GPBT_AVAILABLE_VISIBILITY)
    gpFatal("Invalid visibility '${visibility}' for gpAddDefinitions. Valid options are: ${GPBT_AVAILABLE_VISIBILITY}")
  endif()

  # Validate the format of each definition. Valid formats are:
  # - KEY
  # - KEY=VALUE
  if("${definitions}" MATCHES "^[A-Za-z_][A-Za-z0-9_]*(=.+)?$")
    # If the definition does not contain an '=', append '=1' to it
    if(NOT "${definitions}" MATCHES "=")
      set(definitions "${definitions}=1")
    endif()
  else()
    gpFatal("Invalid format for definition '${definitions}'. Definitions must be in the form 'KEY' or 'KEY=VALUE', where KEY is a valid C preprocessor identifier.")
  endif()

  # Append the definitions to the appropriate list based on visibility
  if("${visibility}" STREQUAL "public")
    list(APPEND __targetPublicCompileDefinitions ${definitions})
  elseif("${visibility}" STREQUAL "private")
    list(APPEND __targetPrivateCompileDefinitions ${definitions})
  else() # internal
    list(APPEND __targetInternalCompileDefinitions ${definitions})
  endif()

  # Log the added definition for debugging purposes
  gpVerbose("Added compile definition '${definitions}' with visibility '${visibility}' to target '${__targetName}'")
endmacro()

# @brief Internal macro to add a dependency to the target with the specified visibility. This is used to implement gpAddPublicDependency, gpAddPrivateDependency, gpAddInternalDependency, and gpAddDynamicDependency.
# @param[in] visibility The visibility level for the dependency (public, private, internal, or dynamic).
# @param[in] dependency The name of the dependency to add to the target.
macro(_implGpAddDependency visibility dependency)
  # Check that we're in a target scope
  gpFatalIfNotInNamedScope("target" "gpAddDependency can only be called within a target scope.")

  # Set the available visibility levels for dependencies
  set(GPBT_AVAILABLE_VISIBILITY "public;private;internal;dynamic")

  # Verify that the provided visibility is valid
  if(NOT "${visibility}" IN_LIST GPBT_AVAILABLE_VISIBILITY)
    gpFatal("Invalid visibility '${visibility}' for gpAddDependency. Valid options are: ${GPBT_AVAILABLE_VISIBILITY}")
  endif()

  # Check if the dependency is already listed in any of the dependency lists to prevent duplicates
  if("${dependency}" IN_LIST __targetPublicDependencies)
    gpFatal("Dependency '${dependency}' is already in the public dependencies list.")
  elseif("${dependency}" IN_LIST __targetInternalDependencies)
    gpFatal("Dependency '${dependency}' is already in the internal dependencies list.")
  elseif("${dependency}" IN_LIST __targetPrivateDependencies)
    gpFatal("Dependency '${dependency}' is already in the private dependencies list.")
  elseif("${dependency}" IN_LIST __targetDynamicDependencies)
    gpFatal("Dependency '${dependency}' is already in the dynamic dependencies list.")
  endif()

  # Append the dependency to the appropriate list based on visibility
  if("${visibility}" STREQUAL "public")
    list(APPEND __targetPublicDependencies ${dependency})
  elseif("${visibility}" STREQUAL "private")
    list(APPEND __targetPrivateDependencies ${dependency})
  elseif("${visibility}" STREQUAL "internal")
    list(APPEND __targetInternalDependencies ${dependency})
  else() # dynamic
    list(APPEND __targetDynamicDependencies ${dependency})
  endif()

  # Log the added dependency for debugging purposes
  gpVerbose("Added dependency '${dependency}' with visibility '${visibility}' to target '${__targetName}'")
endmacro()

# @brief internal macro to validate the format of a compile flag.
# @param[in] flag The compile flag to validate.
macro(_implGpValidateCompileFlagFormat flag)
  # Validate the format of the compile flag. It should be a non-empty string without spaces.
  if("${flag}" NOT MATCHES "^[^\\s]+$")
    gpFatal("Invalid format for compile flag '${flag}'. Compile flags must be non-empty strings without spaces.")
  endif()

  # Validate that MSVC specific compile flags that start with '/' are not used in non-MSVC environments.
  if(NOT MSVC AND "${flag}" MATCHES "^/[A-Za-z]")
    gpFatal("Compile flag '${flag}' appears to be an MSVC-specific flag (starts with '/'), but the current compiler is not MSVC. Please use appropriate flags for your compiler.")
  endif()
endmacro()

# @brief Internal macro to add a compile flag to the target with the specified visibility. This is used to implement gpAddPublicCompileFlag and gpAddPrivateCompileFlag.
# @param[in] visibility The visibility level for the compile flag (public or private).
# @param[in] flag The compile flag to add to the target.
macro(_implGpAddCompileFlag visibility flag)
  # Check that we're in a target scope
  gpFatalIfNotInNamedScope("target" "gpAddCompileFlag can only be called within a target scope.")

  # Set the available visibility levels for compile flags
  set(GPBT_AVAILABLE_VISIBILITY "public;private")

  # Verify that the provided visibility is valid
  if(NOT "${visibility}" IN_LIST GPBT_AVAILABLE_VISIBILITY)
    gpFatal("Invalid visibility '${visibility}' for gpAddCompileFlag. Valid options are: ${GPBT_AVAILABLE_VISIBILITY}")
  endif()

  # Validate the format of the compile flag before adding it to the target's properties
  _implGpValidateCompileFlagFormat("${flag}")

  # Check if the compile flag is already listed in the appropriate list to prevent duplicates
  if("${visibility}" STREQUAL "public")
    if("${flag}" IN_LIST __targetPublicCompileFlags)
      gpFatal("Compile flag '${flag}' is already in the public compile flags list.")
    endif()
    list(APPEND __targetPublicCompileFlags "${flag}")
  else() # private
    if("${flag}" IN_LIST __targetPrivateCompileFlags)
      gpFatal("Compile flag '${flag}' is already in the private compile flags list.")
    endif()
    list(APPEND __targetPrivateCompileFlags "${flag}")
  endif()

  # Log the added compile flag for debugging purposes
  gpVerbose("Added compile flag '${flag}' with visibility '${visibility}' to target '${__targetName}'")
endmacro()

# @brief Internal macro to replace existing compile flags that match a given regex with a new flag, based on visibility. This is used to implement gpTargetReplaceCompileFlag.
# @param[in] newFlag The new compile flag to add.
# @param[in] conflictRegex A regex pattern to identify which existing flags should be replaced by the new flag.
# @param[in] visibility The visibility level for the new compile flag (public or private).
macro(_implGpTargetReplaceCompileFlag newFlag conflicRegex visibility)
  # Check that we're in a target scope
  gpFatalIfNotInNamedScope("target" "gpTargetReplaceCompileFlag can only be called within a target scope.")

  # Validate the format of the compile flag before adding it to the target's properties
  _implGpValidateCompileFlagFormat("${newFlag}")

  # Normalize visibility to lowercase for consistent comparison
  string(TOLOWER "${visibility}" _visibility)

  # Set the available visibility levels for compile flags
  set(GPBT_AVAILABLE_VISIBILITY "public;private")

  # Verify that the provided visibility is valid
  if(NOT "${visibility}" IN_LIST GPBT_AVAILABLE_VISIBILITY)
    gpFatal("Invalid visibility '${visibility}' for gpTargetReplaceCompileFlag. Valid options are: ${GPBT_AVAILABLE_VISIBILITY}")
  endif()

  # Set the available visibility levels for compile flags
  if ("${_visibility}" STREQUAL "private")
    # Strip any existing flags that match the conflict regex
    list(FILTER __targetPrivateCompileOptions EXCLUDE REGEX "${conflicRegex}")
    # Safely add the new flag
    gpAddPrivateCompileFlag("${newFlag}")
  elseif ("${_visibility}" STREQUAL "public")
    list(FILTER __targetPublicCompileOptions EXCLUDE REGEX "${conflicRegex}")
    gpAddPublicCompileFlag("${newFlag}")
  endif()

  # Log the replacement action for debugging purposes
  gpVerbose("Replaced compile flags matching regex '${conflicRegex}' with '${newFlag}' for visibility '${visibility}' in target '${__targetName}'")
endmacro()

# @brief Internal macro to add an include path to the target with the specified visibility. This is used to implement gpAddPublicIncludesPath, gpAddPrivateIncludesPath, and gpAddInternalIncludesPath.
# @param[in] visibility The visibility level for the include path (public, private, or internal).
# @param[in] includePath The include path to add to the target.
macro(_implGpAddIncludesPath visibility includePath)
  # Check that we're in a target scope
  gpFatalIfNotInNamedScope("target" "gpAddIncludesPath can only be called within a target scope.")

  # Set the available visibility levels for dependencies
  set(GPBT_AVAILABLE_VISIBILITY "public;private;internal")

  # Verify that the provided visibility is valid
  if(NOT "${visibility}" IN_LIST GPBT_AVAILABLE_VISIBILITY)
    gpFatal("Invalid visibility '${visibility}' for gpAddIncludesPath. Valid options are: ${GPBT_AVAILABLE_VISIBILITY}")
  endif()

  # Check if the include path is already listed in any of the include path lists to prevent duplicates
  if("${includePath}" IN_LIST __targetPublicIncludes)
    gpFatal("Include path '${includePath}' is already in the public include paths list.")
  elseif("${includePath}" IN_LIST __targetInternalIncludes)
    gpFatal("Include path '${includePath}' is already in the internal include paths list.")
  elseif("${includePath}" IN_LIST __targetPrivateIncludes)
    gpFatal("Include path '${includePath}' is already in the private include paths list.")
  endif()

  # Append the include path to the appropriate list based on visibility
  if("${visibility}" STREQUAL "public")
    list(APPEND __targetPublicIncludes "${includePath}")
  elseif("${visibility}" STREQUAL "private")
    list(APPEND __targetPrivateIncludes "${includePath}")
  elseif("${visibility}" STREQUAL "internal")
    list(APPEND __targetInternalIncludes "${includePath}")
  endif()

  # Log the added include path for debugging purposes
  gpVerbose("Added include path '${includePath}' with visibility '${visibility}' to target '${__targetName}'")
endmacro()

# @brief Internal macro to create a boolean target property based on a command.
#        This is used to implement commands like gpTargetSetTestsEnabled, gpTargetSetBenchmarksEnabled, etc.
# @param[in] commandName The name of the command being implemented (for error messages and logging).
# @param[in] targetVariable The variable name where the boolean value will be stored for the target.
# @param[in] value The value to set (expected to be ON or OFF).
macro(_implGpSetBooleanTargetValue commandName targetVariable value)
  # Check that we're in a target scope
  gpFatalIfNotInNamedScope("target" "${commandName} can only be called within a target scope.")

  # Validate that the value is either ON or OFF (TRUE or FALSE)
  set(GPBT_AVAILABLE_OPTIONS "ON;OFF;TRUE;FALSE")
  if(NOT "${value}" IN_LIST GPBT_AVAILABLE_OPTIONS)
    gpFatal("Invalid value '${value}' for ${commandName}. Valid options are: ON, OFF")
  endif()

  # Set the target property based on the command
  set(${targetVariable} "${value}")

  # Verbose logging for debugging purposes
  gpVerbose("Set '${commandName}' to '${value}' for target '${__targetName}'")
endmacro()
