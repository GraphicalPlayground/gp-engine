# Copyright (c) - Graphical Playground. All rights reserved.
# For more information, see https://graphical-playground/legal
# mailto:support AT graphical-playground DOT com

# Options for the GP Build Tool
option(GP_BUILD_TOOL_VERBOSE "Enable verbose output for the GP Build Tool" OFF)

# Version control
set(GP_BUILD_TOOL_VERSION_MAJOR 0)
set(GP_BUILD_TOOL_VERSION_MINOR 2)
set(GP_BUILD_TOOL_VERSION_PATCH 0)
set(GP_BUILD_TOOL_VERSION "${GP_BUILD_TOOL_VERSION_MAJOR}.${GP_BUILD_TOOL_VERSION_MINOR}.${GP_BUILD_TOOL_VERSION_PATCH}")

# Include all the dependencies
include(gp-build-tool/internals/gp-logger.internal)
include(gp-build-tool/internals/gp-scope.internal)
include(gp-build-tool/internals/gp-stringify.internal)
include(gp-build-tool/internals/gp-targets.internal)
include(gp-build-tool/internals/gp-utils.internal)
include(gp-build-tool/internals/gp-api.internal)
include(gp-build-tool/internals/gp-scan.internal)
include(gp-build-tool/gp-thirdparty)

# Startup banner, only printed once per CMake invocation
gpExecuteOnce("GPBT_STARTUP_BANNER" _shouldPrintBanner)
if(_shouldPrintBanner)
  gpLog("${GP_MAGENTA}=======================================${GP_RESET}")
  gpLog("${GP_MAGENTA}    Graphical Playground Build Tool     ${GP_RESET}")
  gpLog("${GP_MAGENTA}                v${GP_BUILD_TOOL_VERSION}${GP_RESET}")
  gpLog("${GP_MAGENTA}=======================================${GP_RESET}")
  gpLog("Initializing build environment...")
endif()

# TODO: Add comments for every function of the gpbt API

# @brief Starts the definition of a new build target.
#        This macro initializes the target's scope and sets up necessary properties for the target.
#        It also performs validation to ensure that the target name is unique and that we are not
#        already in another target definition.
# @param[in] targetName The name of the target to create (e.g., "my_module")
# @param[in] targetType The type of the target (e.g., "module" or "executable")
macro(gpStartTarget targetName targetType)
  _implGpStartTarget("${targetName}" "${targetType}")
endmacro()

macro(gpStartModule targetName)
  gpStartTarget("${targetName}" "module")
endmacro()

macro(gpStartExecutable targetName)
  gpStartTarget("${targetName}" "executable")
endmacro()

macro(gpEndTarget)
  _implGpEndTarget()
endmacro()

macro(gpEndModule)
  _implGpEndTarget()
endmacro()

macro(gpEndExecutable)
  _implGpEndTarget()
endmacro()

macro(gpAddPrivateDefinitions definitions)
  _implGpAddDefinitions("private" "${definitions}")
endmacro()

macro(gpAddPublicDefinitions definitions)
  _implGpAddDefinitions("public" "${definitions}")
endmacro()

macro(gpAddInternalDefinitions definitions)
  _implGpAddDefinitions("internal" "${definitions}")
endmacro()

macro(gpAddPrivateDependency dependency)
  _implGpAddDependency("private" "${dependency}")
endmacro()

macro(gpAddPublicDependency dependency)
  _implGpAddDependency("public" "${dependency}")
endmacro()

macro(gpAddInternalDependency dependency)
  _implGpAddDependency("internal" "${dependency}")
endmacro()

macro(gpAddDynamicDependency dependency)
  _implGpAddDependency("dynamic" "${dependency}")
endmacro()

macro(gpAddPrivateCompileFlag flag)
  _implGpAddCompileFlag("private" "${flag}")
endmacro()

macro(gpAddPublicCompileFlag flag)
  _implGpAddCompileFlag("public" "${flag}")
endmacro()

macro(gpTargetReplaceCompileFlag newFlag conflicRegex visibility)
  _implGpTargetReplaceCompileFlag("${newFlag}" "${conflicRegex}" "${visibility}")
endmacro()

macro(gpAddPrivateIncludesPath includePath)
  _implGpAddIncludesPath("private" "${includePath}")
endmacro()

macro(gpAddPublicIncludesPath includePath)
  _implGpAddIncludesPath("public" "${includePath}")
endmacro()

macro(gpAddInternalIncludesPath includePath)
  _implGpAddIncludesPath("internal" "${includePath}")
endmacro()

macro(gpTargetSetTestsEnabled value)
  _implGpSetBooleanTargetValue("gpTargetSetTestsEnabled" __targetEnableTests "${value}")
endmacro()

macro(gpTargetSetBenchmarksEnabled value)
  _implGpSetBooleanTargetValue("gpTargetSetBenchmarksEnabled" __targetEnableBenchmarks "${value}")
endmacro()

macro(gpTargetSetExamplesEnabled value)
  _implGpSetBooleanTargetValue("gpTargetSetExamplesEnabled" __targetEnableExamples "${value}")
endmacro()

macro(gpTargetSetISPCEnabled value)
  _implGpSetBooleanTargetValue("gpTargetSetISPCEnabled" __targetEnableISPC "${value}")
endmacro()

macro(gpTargetSetStrictWarningEnabled value)
  _implGpSetBooleanTargetValue("gpTargetSetStrictWarningEnabled" __targetEnableStrictWarnings "${value}")
endmacro()

macro(gpTargetSetIsHeaderOnly value)
  _implGpSetBooleanTargetValue("gpTargetSetIsHeaderOnly" __targetIsHeaderOnly "${value}")
endmacro()

macro(gpTargetAddSources)
  gpFatalIfNotInNamedScope("target" "gpTargetAddSources can only be called within a target scope.")
  list(APPEND __targetSources ${ARGN})
  gpVerbose("Added sources '${ARGN}' to target '${__targetName}'")
endmacro()

macro(gpTargetExcludeSourcesRegex regexPattern)
  gpFatalIfNotInNamedScope("target" "gpTargetExcludeSourcesRegex can only be called within a target scope.")
  list(FILTER __targetSources EXCLUDE REGEX "${regexPattern}")
  gpVerbose("Excluded sources matching regex '${regexPattern}' from target '${__targetName}'")
endmacro()

macro(gpTargetExcludeDirectory directoryName)
  gpTargetExcludeSourcesRegex("/${directoryName}/")
endmacro()

macro(gpTargetExcludeFile fileName)
  gpTargetExcludeSourcesRegex("/${fileName}$")
endmacro()

macro(gpTargetAddPCHHeaders headerPath)
  gpFatalIfNotInNamedScope("target" "gpTargetAddPCHHeaders can only be called within a target scope.")
  list(APPEND __targetPCHs "${headerPath}")
  gpVerbose("Added PCH header '${headerPath}' to target '${__targetName}'")
endmacro()

macro(gpTargetAddSourcesFromDirectory directoryPath)
  gpFatalIfNotInNamedScope("target" "gpTargetAddSourcesFromDirectory can only be called within a target scope.")
  file(GLOB_RECURSE sourcesFromDir "${directoryPath}/*.cpp" "${directoryPath}/*.c" "${directoryPath}/*.cc")
  list(APPEND __targetSources ${sourcesFromDir})
  gpVerbose("Added sources from directory '${directoryPath}' to target '${__targetName}'")
  unset(sourcesFromDir)
endmacro()

macro(gpSetTargetOutputName outputName)
  gpFatalIfNotInNamedScope("target" "gpSetTargetOutputName can only be called within a target scope.")
  set(__targetOutputName "${outputName}")
  gpVerbose("Set output name for target '${__targetName}' to '${outputName}'")
endmacro()

macro(gpExecutableAddResource resourcePath)
  gpFatalIfNotInNamedScope("target" "gpExecutableAddResource can only be called within a target scope.")
  list(APPEND __targetExecutableResourceFiles "${resourcePath}")
  gpVerbose("Added resource file '${resourcePath}' to target '${__targetName}'")
endmacro()

macro(gpExecutableSetEntryPoint entryPoint)
  gpFatalIfNotInNamedScope("target" "gpExecutableSetEntryPoint can only be called within a target scope.")
  set(__targetExecutableEntryPoint "${entryPoint}")
  gpVerbose("Set entry point for executable target '${__targetName}' to '${entryPoint}'")
endmacro()

macro(gpExecutableSetIsGUI isGui)
  if (NOT "${__targetType}" STREQUAL "executable")
    gpFatal("gpExecutableSetIsGUI can only be used within an executable target. Current target '${__targetName}' is of type '${__targetType}'.")
  endif()
  _implGpSetBooleanTargetValue("gpExecutableSetIsGUI" __targetExecutableIsGUI "${isGui}")
endmacro()

macro(gpScanForTargets)
  _implGpScanForTargets()
endmacro()
