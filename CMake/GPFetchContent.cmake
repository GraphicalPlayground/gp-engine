# Copyright (c) - Graphical Playground. All rights reserved.

# Include the command that downloads libraries
include(FetchContent)

function(gp_fetch_content)
    # Parse function arguments
    cmake_parse_arguments(
        ARG                                          # Prefix for parsed variables
        "QUIET"                                      # Boolean options
        "NAME;GIT_REPOSITORY;GIT_TAG;PACKAGE_NAME;REQUIRED_VERSION"  # Single-value arguments
        "COMPONENTS"                                 # Multi-value arguments
        ${ARGN}                                      # Arguments to parse
    )

    # Validate required arguments
    if(NOT ARG_NAME)
        message(FATAL_ERROR "gp_fetch_dependency: NAME argument is required")
    endif()

    if(NOT ARG_GIT_REPOSITORY)
        message(FATAL_ERROR "gp_fetch_dependency: GIT_REPOSITORY argument is required for ${ARG_NAME}")
    endif()

    if(NOT ARG_GIT_TAG)
        message(FATAL_ERROR "gp_fetch_dependency: GIT_TAG argument is required for ${ARG_NAME}")
    endif()

    # Default PACKAGE_NAME to NAME if not specified
    if(NOT ARG_PACKAGE_NAME)
        set(ARG_PACKAGE_NAME ${ARG_NAME})
    endif()

    message(STATUS "  [GP] Resolving dependency: ${ARG_NAME}")

    # Construct find_package arguments
    set(FIND_PACKAGE_ARGS ${ARG_PACKAGE_NAME})

    if(ARG_REQUIRED_VERSION)
        list(APPEND FIND_PACKAGE_ARGS ${ARG_REQUIRED_VERSION})
    endif()

    if(ARG_QUIET)
        list(APPEND FIND_PACKAGE_ARGS QUIET)
    else()
        list(APPEND FIND_PACKAGE_ARGS QUIET)  # Always use QUIET for initial check
    endif()

    if(ARG_COMPONENTS)
        list(APPEND FIND_PACKAGE_ARGS COMPONENTS ${ARG_COMPONENTS})
    endif()

    # Attempt to find the package locally
    message(STATUS "    [i] Checking for local installation of ${ARG_PACKAGE_NAME}...")
    find_package(${FIND_PACKAGE_ARGS})

    # Check if package was found (CMake sets <PackageName>_FOUND variable)
    if(${ARG_PACKAGE_NAME}_FOUND)
        # Local installation found - log details
        message(STATUS "    [+] Found local installation of ${ARG_PACKAGE_NAME}")

        if(${ARG_PACKAGE_NAME}_VERSION)
            message(STATUS "    [i] Version: ${${ARG_PACKAGE_NAME}_VERSION}")
        endif()

        if(${ARG_PACKAGE_NAME}_DIR)
            message(STATUS "    [i] Location: ${${ARG_PACKAGE_NAME}_DIR}")
        endif()

        message(STATUS "    [i] Using local installation")

    else()
        # Local installation not found - fetch from Git
        message(STATUS "    [i] Local installation not found")
        message(STATUS "    [i] Fetching from remote repository")
        message(STATUS "    [i] Repository: ${ARG_GIT_REPOSITORY}")
        message(STATUS "    [i] Tag/Branch: ${ARG_GIT_TAG}")

        # ========================================================================
        # Configure FetchContent
        # ========================================================================

        # Declare the external dependency
        FetchContent_Declare(
            ${ARG_NAME}
            GIT_REPOSITORY  ${ARG_GIT_REPOSITORY}
            GIT_TAG         ${ARG_GIT_TAG}
            GIT_SHALLOW     TRUE                    # Clone only the specified tag (faster)
            GIT_PROGRESS    TRUE                    # Show clone/fetch progress
        )

        # Log fetch operation start
        message(STATUS "    [i] Downloading and configuring ${ARG_NAME}...")

        # Fetch and make the dependency available
        # This will download, configure, and add the dependency to the build
        FetchContent_MakeAvailable(${ARG_NAME})

        # Log successful fetch
        message(STATUS "    [+] Successfully fetched and configured ${ARG_NAME}")

        # Get the source and binary directories for additional context
        FetchContent_GetProperties(${ARG_NAME} SOURCE_DIR SOURCE_DIR BINARY_DIR BINARY_DIR)

        if(SOURCE_DIR)
            message(STATUS "    [i] Source directory: ${SOURCE_DIR}")
        endif()

        if(BINARY_DIR)
            message(STATUS "    [i] Build directory: ${BINARY_DIR}")
        endif()

    endif()
endfunction()
