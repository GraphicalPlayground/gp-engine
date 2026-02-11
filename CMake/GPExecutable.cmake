# Copyright (c) - Graphical Playground. All rights reserved.

function(gp_add_executable)
    cmake_parse_arguments(
        ARG
        "TERMINAL"
        "NAME;MODULE_TYPE;OUTPUT_NAME;ICON"
        "DEPENDENCIES"
        ${ARGN}
    )

    if(NOT ARG_NAME)
        message(FATAL_ERROR "gp_add_executable: NAME argument is required")
    endif()

    if(NOT ARG_MODULE_TYPE)
        set(ARG_MODULE_TYPE "Launch")
    endif()

    # Generate names
    set(EXPORT_NAME "GP${ARG_MODULE_TYPE}${ARG_NAME}")
    set(ALIAS_NAME "GP::${ARG_MODULE_TYPE}::${ARG_NAME}")

    # Set default output name if not provided
    if(NOT ARG_OUTPUT_NAME)
        set(ARG_OUTPUT_NAME ${EXPORT_NAME})
    endif()

    # Gather source files
    file(GLOB_RECURSE EXEC_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/Private/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/Private/*.c"
    )

    file(GLOB_RECURSE EXEC_HEADERS
        "${CMAKE_CURRENT_SOURCE_DIR}/Public/*.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/Public/*.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/Public/*.inl"
    )

    # Set resources (if any)
    set(EXEC_RESOURCES "")

    # Add icon resource for Windows
    if(WIN32 AND ARG_ICON)
        list(APPEND EXEC_RESOURCES ${ARG_ICON})
    endif()

    # Create executable
    add_executable(${EXPORT_NAME} ${EXEC_SOURCES} ${EXEC_HEADERS} ${EXEC_RESOURCES})
    add_executable(${ALIAS_NAME} ALIAS ${EXPORT_NAME})

    # Set properties
    set_target_properties(${EXPORT_NAME} PROPERTIES
        OUTPUT_NAME ${ARG_OUTPUT_NAME}
        FOLDER "${ARG_MODULE_TYPE}"
    )

    # Set terminal property
    if(ARG_TERMINAL AND WIN32)
        set_target_properties(${EXPORT_NAME} PROPERTIES
            WIN32_EXECUTABLE OFF
        )
    elseif(WIN32)
        set_target_properties(${EXPORT_NAME} PROPERTIES
            WIN32_EXECUTABLE ON
        )
    endif()

    # Add information to the app that is a terminal app (if applicable)
    if(ARG_TERMINAL)
        target_compile_definitions(${EXPORT_NAME} PRIVATE GP_TERMINAL_APP=1)
    else()
        target_compile_definitions(${EXPORT_NAME} PRIVATE GP_TERMINAL_APP=0)
    endif()

    # Link dependencies
    if(ARG_DEPENDENCIES)
        target_link_libraries(${EXPORT_NAME} PRIVATE ${ARG_DEPENDENCIES})
    endif()

    # Ensure C++20 standard
    target_compile_features(${EXPORT_NAME} PRIVATE cxx_std_20)

    # Installation
    install(TARGETS ${EXPORT_NAME}
        RUNTIME DESTINATION bin
    )

    message(STATUS "  [+] Added executable: ${ALIAS_NAME} (export: ${ARG_OUTPUT_NAME})")
endfunction()
