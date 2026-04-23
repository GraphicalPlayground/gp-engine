# Copyright (c) - Graphical Playground. All rights reserved.
# For more information, see https://graphical-playground/legal
# mailto:support AT graphical-playground DOT com

include(gp-build-tool/internals/gp-scope.internal)
include(gp-build-tool/internals/gp-logger.internal)
include(gp-build-tool/internals/gp-stringify.internal)

# List of valid target types for the build system. This can be extended in the future if new target types are added.
set(GPBT_TARGET_TYPES "module;executable" CACHE INTERNAL "List of valid target types for the build system")

# @brief Internal macro to initialize a new target with the given name and type.
# @param[in] targetName The name of the target being defined.
# @param[in] targetType The type of the target (e.g., "module", "executable"). This must be one of the valid types defined in GPBT_TARGET_TYPES.
macro(_implGpStartTarget targetName targetType)
  # Push a named scope for the target, which will track all variables
  # set within it and prevent duplicate target definitions
  gpPushNamedScope("target")

  # Prepare all the necessary variables for the target based on the provided name and type
  gpSetScopedMultiple(
    # Metadata about the target
    __targetName "${targetName}"
    __targetNameUpper ""
    __targetType ""
    __targetOutputName ""
    __targetExportName ""
    __targetAliasName ""

    # Source files for the target
    __targetSources ""
    __targetPCHs ""

    # Include directories categorized by visibility
    __targetPublicIncludes ""
    __targetInternalIncludes ""
    __targetPrivateIncludes ""

    # Dependencies categorized by visibility
    __targetPublicDependencies ""
    __targetInternalDependencies ""
    __targetPrivateDependencies ""

    # Compile definitions categorized by visibility
    __targetPublicCompileDefinitions ""
    __targetInternalCompileDefinitions ""
    __targetPrivateCompileDefinitions ""

    # Compile options categorized by visibility
    __targetPublicCompileOptions ""
    __targetPrivateCompileOptions ""

    # Boolean flags for the target
    __targetIsHeaderOnly FALSE
    __targetEnableTests FALSE
    __targetEnableBenchmarks FALSE
    __targetEnableExamples FALSE
    __targetEnableISPC FALSE
    __targetEnableStrictWarnings FALSE

    # Executable-specific properties
    __targetExecutableEntryPoint ""
    __targetExecutableResourceFiles ""
    __targetExecutableIsGUI FALSE
  )

  # Populate the target metadata variables based on the provided name and type
  string(TOLOWER "${targetType}" __targetType)
  gpToUpperSnakeCase("${targetName}" __targetNameUpper)

  # Verify that the target type is valid
  if(NOT __targetType IN_LIST GPBT_TARGET_TYPES)
    gpFatal("Invalid target type '${targetType}' for target '${targetName}'. Valid types are: ${GPBT_TARGET_TYPES}")
  endif()
endmacro()

# @brief Internal macro to finalize the definition of a target.
macro(_implGpEndTarget)
  # Pop the target scope, which will revert any variables set
  # within it and clean up the named scope tracking
  gpPopNamedScope("target")
endmacro()
