# Copyright (c) - Graphical Playground. All rights reserved.
# For more information, see https://graphical-playground/legal
# mailto:support AT graphical-playground DOT com

include(gp-build-tool/internals/gp-scope.internal)
include(gp-build-tool/internals/gp-logger.internal)
include(gp-build-tool/internals/gp-stringify.internal)
include(gp-build-tool/internals/gp-utils.internal)

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
    __targetCleanName ""
    __targetNameUpper ""
    __targetType ""
    __targetOutputName ""
    __targetExportName ""
    __targetAliasName ""
    __targetLocation ""

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
    __targetDynamicDependencies ""

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
  string(REPLACE "/" "_" __targetCleanName "${targetName}")
  gpToUpperSnakeCase("${__targetCleanName}" __targetNameUpper)
  gpToLowerSnakeCase("${__targetCleanName}" __targetOutputName)
  gpToLowerSnakeCase("${__targetCleanName}" __targetExportName)
  set(__targetExportName "gp_${__targetExportName}")
  set(__targetOutputName "gp_${__targetOutputName}")
  set(__targetAliasName "gp::${__targetCleanName}")

  # Verbose naming of the target for better visibility in the build output
  gpVerbose("Initializing target '${__targetName}' of type '${__targetType}' with export name '${__targetExportName}' and alias '${__targetAliasName}'")

  # Log the start of the target definition for better visibility in the build output
  gpLog("Defining target '${targetName}' of type '${targetType}'")

  # Get the list of currently registered targets to check for duplicates during the registration phase
  get_property(currentTargets GLOBAL PROPERTY GPBT_REGISTERED_TARGETS)
  _gpGetCurrentPhase(GPBT_CURRENT_PHASE)

  # Setup the target for the registration phase.
  # This includes validating the target name and type, storing the target location,
  # and registering the target in the global list of targets.
  if(GPBT_CURRENT_PHASE STREQUAL "REGISTRATION")
    # Store location of the target for potential use in custom build steps or tools that need to know where the source files are
    set_property(GLOBAL PROPERTY GPBT_TARGET_${__targetNameUpper}_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}")

    # Verify that the target name is unique during the registration phase
    if("${__targetName}" IN_LIST currentTargets)
      gpFatal("Target name '${__targetName}' is already registered. Target names must be unique.")
    else()
      set_property(GLOBAL APPEND PROPERTY GPBT_REGISTERED_TARGETS "${__targetName}")
    endif()
  endif()

  # Retrieve the target location for use in later phases.
  # This ensures that the variable is always populated regardless of the current phase.
  get_property(__targetLocation GLOBAL PROPERTY GPBT_TARGET_${__targetNameUpper}_LOCATION)

  # Verify that the target type is valid
  if(NOT __targetType IN_LIST GPBT_TARGET_TYPES)
    gpFatal("Invalid target type '${targetType}' for target '${targetName}'. Valid types are: ${GPBT_TARGET_TYPES}")
  endif()

  # Append initial properties to the target's properties
  list(APPEND __targetPublicIncludes "${__targetLocation}/public")
  list(APPEND __targetInternalIncludes "${__targetLocation}/internal")
  list(APPEND __targetPrivateIncludes "${__targetLocation}/private")
  foreach(directory "${__targetLocation}/private" "${__targetLocation}/internal")
    if(EXISTS "${directory}")
      file(GLOB_RECURSE sourcesFromDir "${directory}/*.cpp" "${directory}/*.cc" "${directory}/*.cxx")
      list(APPEND __targetSources ${sourcesFromDir})
    endif()
  endforeach()
  set(__targetEnableStrictWarnings TRUE)
endmacro()

# @brief Internal macro to finalize the definition of a target.
macro(_implGpEndTarget)
  # Check being in a target scope before trying to end it.
  gpFatalIfNotInNamedScope("target" "gpEndTarget() called without a matching gpStartTarget(). Each target definition must begin with gpStartTarget() and end with gpEndTarget().")

  # Get the curent phase
  _gpGetCurrentPhase(GPBT_CURRENT_PHASE)

  # During the registration phase, we need to store the collected properties for this target in global properties so that
  # they can be accessed during the configuration phase when we actually create the CMake targets.
  if(GPBT_CURRENT_PHASE STREQUAL "REGISTRATION")
    # Combine all dependencies into a single property for easier access during the generation phase.
    gpSetScoped(allDependencies ${__targetPublicDependencies} ${__targetInternalDependencies} ${__targetPrivateDependencies} ${__targetDynamicDependencies})
    # Store specific and combined  dependencies using dynamic property names.
    set_property(GLOBAL PROPERTY GPBT_TARGET_${__targetNameUpper}_PUBLIC_DEPS "${__targetPublicDependencies}")
    set_property(GLOBAL PROPERTY GPBT_TARGET_${__targetNameUpper}_INTERNAL_DEPS "${__targetInternalDependencies}")
    set_property(GLOBAL PROPERTY GPBT_TARGET_${__targetNameUpper}_PRIVATE_DEPS "${__targetPrivateDependencies}")
    set_property(GLOBAL PROPERTY GPBT_TARGET_${__targetNameUpper}_DYNAMIC_DEPS "${__targetDynamicDependencies}")
    set_property(GLOBAL PROPERTY GPBT_TARGET_${__targetNameUpper}_ALL_DEPS "${allDependencies}")
  elseif(GPBT_CURRENT_PHASE STREQUAL "CONFIGURATION")
    # During the configuration phase, we can perform the actual CMake target creation and configuration
    # based on the properties collected during registration.
    _implGpDefineCMakeTarget()
  endif()

  # Log the completion of the target definition for better visibility in the build output
  gpLog("Finished defining target '${__targetName}'")

  # Pop the target scope, which will revert any variables set
  # within it and clean up the named scope tracking
  gpPopNamedScope("target")
endmacro()

# @brief Cleans the name of a dependency by removing any leading or trailing whitespace, adding proper prefix/suffix if needed.
# @param[in] inName The variable containing the dependency name to clean.
# @param[out] outName The variable containing the cleaned dependency name.
function(_implGpCleanDependencyName inName outName)
  # TODO: Implement more robust cleaning logic if needed
  # (e.g., handling version suffixes, platform-specific variants, etc.)
  get_property(allRegisteredTargets GLOBAL PROPERTY GPBT_REGISTERED_TARGETS)
  if("${inName}" IN_LIST allRegisteredTargets AND NOT "${inName}" MATCHES "^gp::")
    string(REPLACE "/" "_" cleanedName "${inName}")
    # Dereference outName and export it to the caller
    set(${outName} "gp::${cleanedName}" PARENT_SCOPE) 
  else()
    # Dereference outName and export it to the caller
    set(${outName} "${inName}" PARENT_SCOPE)
  endif()
endfunction()

# @brief Internal function to clean a list of dependency names by applying the cleaning logic to each item in the list.
# @param[in] inList The variable containing the list of dependency names to clean.
# @param[out] outList The variable containing the list of cleaned dependency names.
function(_implGpGetCleanedDependencyList inList outList)
  set(_tempList "")

  foreach(item IN LISTS ${inList})
    _implGpCleanDependencyName("${item}" cleanedItem)
    list(APPEND _tempList "${cleanedItem}")
  endforeach()

  # Push the final compiled list back to the caller's scope
  set(${outList} "${_tempList}" PARENT_SCOPE)
endfunction()

# @brief Internal macro to define the actual CMake target based on the properties collected during the registration phase.
#        This is called during the configuration phase after all targets have been registered.
macro(_implGpDefineCMakeTarget)
  # Check being in a target scope before trying to end it.
  gpFatalIfNotInNamedScope("target" "Internal error: _implGpDefineCMakeTarget() called without a matching gpStartTarget(). This indicates a logic error in the build tool implementation.")

  # Get current phase
  _gpGetCurrentPhase(GPBT_CURRENT_PHASE)

  # Check the current phase to ensure this function is only called during the configuration phase
  if(NOT GPBT_CURRENT_PHASE STREQUAL "CONFIGURATION")
    gpFatal("Internal error: _implGpDefineCMakeTarget() called during phase '${GPBT_CURRENT_PHASE}'. This function should only be called during the CONFIGURATION phase.")
  endif()

  # Prepare the CMake target name and type based on the collected properties
  gpVerbose("Defining CMake target '${__targetExportName}' of type '${__targetType}' with alias '${__targetAliasName}'")

  # If no sources were added, and the target is not header-only, we need to add a dummy source file to ensure CMake creates the target correctly.
  list(LENGTH __targetSources numSources)
  if(numSources EQUAL 0 AND NOT __targetIsHeaderOnly)
    gpSetScoped(dummySource "${CMAKE_CURRENT_BINARY_DIR}/dummy.cpp")
    if(NOT EXISTS "${dummySource}")
      file(WRITE "${dummySource}" "// Auto-generated keep file for link language detection\n")
    endif()
    list(APPEND __targetSources "${dummySource}")
    gpLog("No sources specified for target '${__targetName}'. Added dummy source to ensure target creation.")
  endif()

  # Gather the header files for IDE integration and better project organization.
  # This is optional but improves the developer experience.
  gpSetScoped(headerFiles "")
  foreach(directory IN LISTS __targetPublicIncludes __targetInternalIncludes __targetPrivateIncludes)
    if(EXISTS "${directory}")
      file(GLOB_RECURSE headers "${directory}/*.h" "${directory}/*.hpp" "${directory}/*.hh" "${directory}/*.hxx")
      list(APPEND headerFiles ${headers})
    endif()
  endforeach()

  # Create the CMake target based on the collected properties and the target type
  if(__targetType STREQUAL "module")
    add_library(${__targetExportName} ${__targetSources} ${headerFiles})
    add_library(${__targetAliasName} ALIAS ${__targetExportName})
  elseif(__targetType STREQUAL "executable")
    add_executable(${__targetExportName} ${__targetSources} ${headerFiles} ${__targetExecutableResourceFiles} ${__targetExecutableEntryPoint})
    add_executable(${__targetAliasName} ALIAS ${__targetExportName})

    if(WIN32)
      set_target_properties(${__targetExportName} PROPERTIES
        WIN32_EXECUTABLE ${__targetExecutableIsGUI}
      )
    endif()

    target_compile_definitions(${__targetExportName} PRIVATE "GP_EXECUTABLE_IS_GUI=$<BOOL:${__targetExecutableIsGUI}>")
  else()
    gpFatal("Internal error: Unsupported target type '${__targetType}' for target '${__targetName}'. This indicates a logic error in the build tool implementation.")
  endif()

  # Set target properties for symbol export, visibility, and other relevant settings
  set_target_properties(${__targetExportName} PROPERTIES
    OUTPUT_NAME "${__targetOutputName}"
    DEFINE_SYMBOL "GP_${__targetNameUpper}_API_EXPORTS"
    CXX_VISIBILITY_PRESET default
    VISIBILITY_INLINES_HIDDEN OFF
    POSITION_INDEPENDENT_CODE ON
    FOLDER "${__targetType}"
  )

  # On Windows, if we're building a shared library, we need to ensure that all symbols are exported correctly.
  if(WIN32 AND BUILD_SHARED_LIBS)
    set_target_properties(${__targetExportName} PROPERTIES
      WINDOWS_EXPORT_ALL_SYMBOLS ON
    )
  endif()

  # Set include directories with proper visibility (PUBLIC, PRIVATE, INTERFACE)
  target_include_directories(${__targetExportName}
    PUBLIC
      # Map all source-tree public paths to the Build Interface
      $<BUILD_INTERFACE:${__targetPublicIncludes}>
      # Define where headers will live after installation
      $<INSTALL_INTERFACE:include/${__targetName}>
    PRIVATE
      # Private and Internal remain local to this target
      ${__targetPrivateIncludes}
      ${__targetInternalIncludes}
  )

  # TODO: Add support for compile definitions with proper visibility (PUBLIC, PRIVATE, INTERNAL).

  # Add precompile headers if specified
  list(LENGTH __targetPCHs numPCHs)
  if(numPCHs GREATER 0)
    gpVerbose("Adding precompiled headers to target '${__targetName}': ${__targetPCHs}")
    target_precompile_headers(${__targetExportName} PRIVATE ${__targetPCHs})
  endif()

  # Apply public compilation flags to the target
  list(LENGTH __targetPublicCompileOptions numPublicCompileOptions)
  if(numPublicCompileOptions GREATER 0)
    gpVerbose("Applying public compile options to target '${__targetName}': ${__targetPublicCompileOptions}")
    target_compile_options(${__targetExportName} PUBLIC ${__targetPublicCompileOptions})
  endif()

  # Apply private compilation flags to the target
  list(LENGTH __targetPrivateCompileOptions numPrivateCompileOptions)
  if(numPrivateCompileOptions GREATER 0)
    gpVerbose("Applying private compile options to target '${__targetName}': ${__targetPrivateCompileOptions}")
    target_compile_options(${__targetExportName} PRIVATE ${__targetPrivateCompileOptions})
  endif()

  # Set the public dependencies for the target.
  _implGpGetCleanedDependencyList(__targetPublicDependencies publicDeps)
  list(LENGTH publicDeps numPublicDeps)
  if(numPublicDeps GREATER 0)
    gpVerbose("Linking public dependencies for target '${__targetName}': ${publicDeps}")
    target_link_libraries(${__targetExportName} PUBLIC ${publicDeps})
  endif()

  # Set the private dependencies for the target.
  _implGpGetCleanedDependencyList(__targetPrivateDependencies privateDeps)
  list(LENGTH privateDeps numPrivateDeps)
  if(numPrivateDeps GREATER 0)
    gpVerbose("Linking private dependencies for target '${__targetName}': ${privateDeps}")
    target_link_libraries(${__targetExportName} PRIVATE ${privateDeps})
  endif()

  # Set the internal dependencies for the target.
  _implGpGetCleanedDependencyList(__targetInternalDependencies internalDeps)
  list(LENGTH internalDeps numInternalDeps)
  if(numInternalDeps GREATER 0)
    gpVerbose("Linking internal dependencies for target '${__targetName}': ${internalDeps}")
    target_link_libraries(${__targetExportName} INTERFACE ${internalDeps})
  endif()

  # Ensure C++23 standard is used for all targets
  target_compile_features(${__targetExportName} PUBLIC cxx_std_23)

  # Setup installation rules for the target (optional, but common for libraries)
  install(TARGETS ${__targetExportName}
    EXPORT GPEngineTargets
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    INCLUDES DESTINATION include
  )

  # Install public headers to the appropriate location under include/ for this target.
  foreach(_dir IN LISTS __targetPublicIncludes)
    install(DIRECTORY ${_dir}/
      DESTINATION include/${__targetName}
      FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp" PATTERN "*.hh" PATTERN "*.hxx"
    )
  endforeach()

  # Strict warning handling (optional, but recommended for better code quality)
  if(__targetEnableStrictWarnings)
    if(MSVC)
      target_compile_options(${__targetExportName} PRIVATE /W4 /WX)
    else()
      target_compile_options(${__targetExportName} PRIVATE -Wall -Wextra -Werror)
    endif()
  endif()

  # Create the target for IPSC, if enabled
  if(__targetEnableISPC)
    _implGpDefineIPSCTarget()
  endif()

  # Create the target for tests, if enabled
  if(__targetEnableTests)
    _implGpDefineTestsTarget()
  endif()

  # Create the target for benchmarks, if enabled
  if(__targetEnableBenchmarks)
    _implGpDefineBenchmarksTarget()
  endif()

  # Create the target for examples, if enabled
  if(__targetEnableExamples)
    _implGpDefineExamplesTarget()
  endif()
endmacro()

# @brief Internal macro to define a tests target for the current target.
macro(_implGpDefineTestsTarget)
  # Check being in a target scope before trying to end it.
  gpFatalIfNotInNamedScope("target" "Internal error: _implGpDefineTestsTarget() called without a matching gpStartTarget(). This indicates a logic error in the build tool implementation.")

  # Get current phase
  _gpGetCurrentPhase(GPBT_CURRENT_PHASE)

  # Check the current phase to ensure this function is only called during the configuration phase
  if(NOT GPBT_CURRENT_PHASE STREQUAL "CONFIGURATION")
    gpFatal("Internal error: _implGpDefineTestsTarget() called during phase '${GPBT_CURRENT_PHASE}'. This function should only be called during the CONFIGURATION phase.")
  endif()

  # Check that the Catch2 testing framework is available before trying to create the tests target.
  if(NOT TARGET Catch2WithMain)
    gpFatal("Catch2 target 'Catch2WithMain' not found. Ensure Catch2 is properly added as a dependency for tests.")
  endif()

  # Log the creation of the tests target for better visibility in the build output
  gpLog("Defining tests target '${__targetExportName}_tests' for target '${__targetName}'")

  # Create the tests target as an executable that depends on the main target and Catch2.
  gpSetScoped(__targetTestsDir "${__targetLocation}/tests")
  if(NOT EXISTS "${__targetTestsDir}")
      gpWarning("No test sources found in '${__targetTestsDir}' for target '${__targetName}'. No tests will be generated.")
  else()
    file(GLOB_RECURSE __targetTestsSources "${__targetTestsDir}/*.cpp" "${__targetTestsDir}/*.cc")

    if (NOT __targetTestsSources)
      gpWarning("No test sources found in '${__targetTestsDir}' for target '${__targetName}'. No tests will be generated.")
    else()
      add_executable(${__targetExportName}_tests ${__targetTestsSources})
      target_link_libraries(${__targetExportName}_tests PRIVATE Catch2WithMain ${__targetExportName})
      set_target_properties(${__targetExportName}_tests PROPERTIES
        OUTPUT_NAME "${__targetOutputName}_tests"
        FOLDER "tests"
      )

      target_compile_features(${__targetExportName}_tests PUBLIC cxx_std_23)

      add_test(NAME ${__targetName}_tests COMMAND ${__targetExportName}_tests)
      set_tests_properties(${__targetName}_tests PROPERTIES
        LABELS "gp;${__targetName};tests"
      )
    endif()
  endif()
endmacro()

# @brief Internal macro to define an ISPC target for the current target.
macro(_implGpDefineIPSCTarget)
  # Check being in a target scope before trying to end it.
  gpFatalIfNotInNamedScope("target" "Internal error: _implGpDefineIPSCTarget() called without a matching gpStartTarget(). This indicates a logic error in the build tool implementation.")

  # Get current phase
  _gpGetCurrentPhase(GPBT_CURRENT_PHASE)

  # Check the current phase to ensure this function is only called during the configuration phase
  if(NOT GPBT_CURRENT_PHASE STREQUAL "CONFIGURATION")
    gpFatal("Internal error: _implGpDefineIPSCTarget() called during phase '${GPBT_CURRENT_PHASE}'. This function should only be called during the CONFIGURATION phase.")
  endif()

  gpWarning("ISPC target generation is not yet implemented for target '${__targetName}'.")
endmacro()

# @brief Internal macro to define a benchmarks target for the current target.
macro(_implGpDefineBenchmarksTarget)
  # Check being in a target scope before trying to end it.
  gpFatalIfNotInNamedScope("target" "Internal error: _implGpDefineBenchmarksTarget() called without a matching gpStartTarget(). This indicates a logic error in the build tool implementation.")

  # Get current phase
  _gpGetCurrentPhase(GPBT_CURRENT_PHASE)

  # Check the current phase to ensure this function is only called during the configuration phase
  if(NOT GPBT_CURRENT_PHASE STREQUAL "CONFIGURATION")
    gpFatal("Internal error: _implGpDefineBenchmarksTarget() called during phase '${GPBT_CURRENT_PHASE}'. This function should only be called during the CONFIGURATION phase.")
  endif()

  gpWarning("Benchmarks target generation is not yet implemented for target '${__targetName}'.")
endmacro()

# @brief Internal macro to define an examples target for the current target.
macro(_implGpDefineExamplesTarget)
  # Check being in a target scope before trying to end it.
  gpFatalIfNotInNamedScope("target" "Internal error: _implGpDefineExamplesTarget() called without a matching gpStartTarget(). This indicates a logic error in the build tool implementation.")

  # Get current phase
  _gpGetCurrentPhase(GPBT_CURRENT_PHASE)

  # Check the current phase to ensure this function is only called during the configuration phase
  if(NOT GPBT_CURRENT_PHASE STREQUAL "CONFIGURATION")
    gpFatal("Internal error: _implGpDefineExamplesTarget() called during phase '${GPBT_CURRENT_PHASE}'. This function should only be called during the CONFIGURATION phase.")
  endif()

  gpWarning("Examples target generation is not yet implemented for target '${__targetName}'.")
endmacro()
