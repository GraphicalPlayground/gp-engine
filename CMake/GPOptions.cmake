# Copyright (c) - Graphical Playground. All rights reserved.

function(gp_display_options)
    cmake_parse_arguments(
        ARG
        ""
        "NAME;VARIABLE"
        ""
        ${ARGN}
    )

    if(NOT ARG_NAME)
        message(FATAL_ERROR "gp_display_options: NAME argument is required")
    endif()

    if(NOT ARG_VARIABLE)
        message(FATAL_ERROR "gp_display_options: VARIABLE argument is required")
    endif()

    if(${ARG_VARIABLE})
        message(STATUS "  [+] ${ARG_NAME} enabled")
    else()
        message(STATUS "  [-] ${ARG_NAME} disabled")
    endif()
endfunction()

function(gp_apply_options)
    option(GP_ENABLE_BENCHMARKS "Enable benchmarks" ON)
    option(GP_ENABLE_TESTS "Enable tests" ON)

    message(STATUS "[GP] Options:")
    gp_display_options(NAME "Benchmarks" VARIABLE GP_ENABLE_BENCHMARKS)
    gp_display_options(NAME "Tests" VARIABLE GP_ENABLE_TESTS)
endfunction()
