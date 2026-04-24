# Copyright (c) - Graphical Playground. All rights reserved.
# For more information, see https://graphical-playground/legal
# mailto:support AT graphical-playground DOT com

include(gp-build-tool/internals/gp-logger.internal)
include(FetchContent)

# @brief A helper function to fetch and manage third-party dependencies. It first attempts to find the package locally, and if it is not found, it
#        will fetch it from a specified Git repository and tag. This function also allows for custom build options to be injected into the CMake cache before configuration.
# @param[in] NAME The name of the dependency. This is used for logging and as the identifier for the FetchContent module.
# @param[in] GIT_REPOSITORY The URL of the Git repository to fetch the dependency from if it is not found locally.
# @param[in] GIT_TAG The specific Git tag or commit to fetch from the repository.
# @param[in] PACKAGE_NAME (Optional) The name of the package to find with find_package. If not specified, it defaults to the value of NAME.
# @param[in] REQUIRED_VERSION (Optional) The version requirement to pass to find_package when checking for a local installation of the package.
# @param[in] COMPONENTS (Optional) A list of components to pass to find_package when checking for a local installation of the package.
# @param[in] OPTIONS (Optional) A list of custom build options to set in the CMake cache before fetching the dependency. Each option should be in the format "KEY=VALUE".
function(gpFetchContent)
  # Parse function arguments
  cmake_parse_arguments(
    ARG
    "QUIET"
    "NAME;GIT_REPOSITORY;GIT_TAG;PACKAGE_NAME;REQUIRED_VERSION"
    "COMPONENTS;OPTIONS"
    ${ARGN}
  )

  # Validate required arguments
  if(NOT ARG_NAME)
    gpFatal("gpFetchContent: 'NAME' argument is required.")
  endif()

  if(NOT ARG_GIT_REPOSITORY)
    gpFatal("gpFetchContent: 'GIT_REPOSITORY' argument is required for '${ARG_NAME}'.")
  endif()

  if(NOT ARG_GIT_TAG)
    gpFatal("gpFetchContent: 'GIT_TAG' argument is required for '${ARG_NAME}'.")
  endif()

  # Default PACKAGE_NAME to NAME if not specified
  if(NOT ARG_PACKAGE_NAME)
    set(ARG_PACKAGE_NAME ${ARG_NAME})
  endif()

  gpLog("Resolving third-party dependency: '${ARG_NAME}'")

  # Construct find_package arguments
  set(FIND_PACKAGE_ARGS ${ARG_PACKAGE_NAME})

  if(ARG_REQUIRED_VERSION)
    list(APPEND FIND_PACKAGE_ARGS ${ARG_REQUIRED_VERSION})
  endif()

  # Always use QUIET for the internal check to keep the console output clean
  list(APPEND FIND_PACKAGE_ARGS QUIET)

  if(ARG_COMPONENTS)
    list(APPEND FIND_PACKAGE_ARGS COMPONENTS ${ARG_COMPONENTS})
  endif()

  # Attempt to find the package locally
  gpVerbose("Checking for local system installation of '${ARG_PACKAGE_NAME}'...")
  find_package(${FIND_PACKAGE_ARGS})

  # Check if package was found
  if(${ARG_PACKAGE_NAME}_FOUND)
    # Local installation found - log details
    gpLog("Found local system installation of '${ARG_PACKAGE_NAME}'")

    if(${ARG_PACKAGE_NAME}_VERSION)
      gpVerbose("Version: ${${ARG_PACKAGE_NAME}_VERSION}")
    endif()

    if(${ARG_PACKAGE_NAME}_DIR)
      gpVerbose("Location: ${${ARG_PACKAGE_NAME}_DIR}")
    endif()

  else()
    # Local installation not found - fetch from Git
    gpVerbose("Local installation not found. Fetching from remote repository...")
    gpVerbose("Repository: ${ARG_GIT_REPOSITORY} | Tag: ${ARG_GIT_TAG}")

    # Declare the external dependency
    FetchContent_Declare(
      ${ARG_NAME}
      GIT_REPOSITORY  ${ARG_GIT_REPOSITORY}
      GIT_TAG         ${ARG_GIT_TAG}
      GIT_SHALLOW     TRUE                     # Clone only the specified tag (faster)
      GIT_PROGRESS    ${GP_BUILD_TOOL_VERBOSE} # Show clone/fetch progress
      SYSTEM          TRUE                     # Treat as system dependency (suppress warnings)
    )

    # Inject custom build options into the CMake cache before configuration
    if(ARG_OPTIONS)
      gpVerbose("Applying custom build options for '${ARG_NAME}':")
      foreach(_OPTION IN LISTS ARG_OPTIONS)
        # Use regex to split "KEY=VALUE" into CMAKE_MATCH_1 (Key) and CMAKE_MATCH_2 (Value)
        if(_OPTION MATCHES "^([^=]+)=(.*)$")
          set(${CMAKE_MATCH_1} "${CMAKE_MATCH_2}" CACHE STRING "GPBT Forced Dependency Option" FORCE)
          gpVerbose("  -> ${CMAKE_MATCH_1} = ${CMAKE_MATCH_2}")
        else()
          gpFatal("Invalid option format '${_OPTION}' passed to gpFetchContent. Expected format is 'KEY=VALUE'.")
        endif()
      endforeach()
    endif()

    # Log fetch operation start
    gpLog("Downloading and configuring '${ARG_NAME}'...")

    # Fetch and make the dependency available
    FetchContent_MakeAvailable(${ARG_NAME})

    # Log successful fetch
    gpLog("Successfully fetched and configured '${ARG_NAME}'")

    # Get the source and binary directories for additional context (visible in verbose mode)
    FetchContent_GetProperties(${ARG_NAME} SOURCE_DIR SOURCE_DIR BINARY_DIR BINARY_DIR)

    if(SOURCE_DIR)
      gpVerbose("Source directory: ${SOURCE_DIR}")
    endif()

    if(BINARY_DIR)
      gpVerbose("Build directory: ${BINARY_DIR}")
    endif()

  endif()
endfunction()

# @brief A helper function to set the FOLDER property of a target, which organizes the target into a folder in IDEs that support it (like Visual Studio or Xcode).
#        This is purely for improving the organization and readability of the project structure within the IDE and does not affect the build process.
# @param[in] TARGET_NAME The name of the target to organize into a folder.
# @param[in] FOLDER_NAME The name of the folder to place the target in within the IDE.
function(gpSetTargetFolder TARGET_NAME FOLDER_NAME)
  if(TARGET ${TARGET_NAME})
    get_target_property(_TARGET_TYPE ${TARGET_NAME} TYPE)

    # We cannot set the FOLDER property on Interface or Alias libraries
    if(NOT _TARGET_TYPE STREQUAL "INTERFACE_LIBRARY" AND NOT _TARGET_TYPE STREQUAL "ALIAS_LIBRARY")
      set_target_properties(${TARGET_NAME} PROPERTIES FOLDER "${FOLDER_NAME}")
      gpVerbose("Organized target '${TARGET_NAME}' into IDE folder '${FOLDER_NAME}'")
    endif()
  endif()
endfunction()
