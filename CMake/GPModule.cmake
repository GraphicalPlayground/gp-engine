# Copyright (c) - Mallory SCOTTON. All rights reserved.

# Define valid module types
set(GP_VALID_MODULE_TYPES
    "Runtime"
    "Editor"
    "Programs"
    "Developer"
    "Launch"
)

function(gp_add_module)
    # Parse arguments
    cmake_parse_arguments(
        ARG
        ""
        "NAME;MODULE_TYPE"
        "DEPENDENCIES"
        ${ARGN}
    )

    # Validate required arguments
    if(NOT ARG_NAME)
        message(FATAL_ERROR "gp_add_module: NAME argument is required")
    endif()

    if(NOT ARG_MODULE_TYPE)
        message(FATAL_ERROR "gp_add_module: MODULE_TYPE argument is required. Valid types: Runtime, Editor, Developer, Programs, Launch")
    endif()

    if(NOT ARG_MODULE_TYPE IN_LIST GP_VALID_MODULE_TYPES)
        message(FATAL_ERROR "gp_add_module: Invalid MODULE_TYPE '${ARG_MODULE_TYPE}'. Must be one of: ${VALID_MODULE_TYPES}")
    endif()

    # Generate the export name with prefix: GP<ModuleType><Name>
    # Example: GPRuntimeCore, GPEditorViewport
    set(EXPORT_NAME "GP${ARG_MODULE_TYPE}${ARG_NAME}")

    # Generate the alias: GP::<ModuleType>::<Name>
    # Example: GP::Runtime::Core, GP::Editor::Viewport
    set(ALIAS_NAME "GP::${ARG_MODULE_TYPE}::${ARG_NAME}")

    # Gather source files
    file(GLOB_RECURSE MODULE_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/Private/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/Private/*.c"
    )

    file(GLOB_RECURSE MODULE_HEADERS
        "${CMAKE_CURRENT_SOURCE_DIR}/Public/*.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/Public/*.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/Public/*.inl"
    )

    # Gather ISPC source files (.ispc)
    file(GLOB_RECURSE MODULE_ISPC_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/Private/*.ispc"
    )

    # Save original C/C++ source count before adding ISPC objects
    list(LENGTH MODULE_SOURCES ORIGINAL_SOURCE_COUNT)

    # Gather ISPC header files (.isph)
    file(GLOB_RECURSE MODULE_ISPC_HEADERS
        "${CMAKE_CURRENT_SOURCE_DIR}/Public/*.isph"
        "${CMAKE_CURRENT_SOURCE_DIR}/Private/*.isph"
    )

    # Process ISPC files if any are found
    if(MODULE_ISPC_SOURCES)
        list(LENGTH MODULE_ISPC_SOURCES ISPC_SOURCE_COUNT)
        message(STATUS "    [i] Found ${ISPC_SOURCE_COUNT} ISPC source file(s)")

        # Check if ISPC compiler is available
        if(NOT TARGET ispc_compiler)
            message(FATAL_ERROR "gp_add_module: ISPC files found but ispc_compiler target not available. Please include ISPC setup before calling gp_add_module.")
        endif()

        # Get ISPC compiler path
        get_target_property(ISPC_EXECUTABLE ispc_compiler IMPORTED_LOCATION)

        # Create output directory for generated files
        set(ISPC_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/ispc_generated")
        file(MAKE_DIRECTORY ${ISPC_OUTPUT_DIR})

        # List to collect generated object files
        set(ISPC_GENERATED_OBJECTS)
        set(ISPC_GENERATED_HEADERS)

        # Process each ISPC source file
        foreach(ISPC_SOURCE ${MODULE_ISPC_SOURCES})
            # Get the source file name without extension
            get_filename_component(ISPC_SOURCE_NAME ${ISPC_SOURCE} NAME_WE)

            # Define output files
            set(ISPC_OUTPUT_OBJ "${ISPC_OUTPUT_DIR}/${ISPC_SOURCE_NAME}${CMAKE_C_OUTPUT_EXTENSION}")
            set(ISPC_OUTPUT_HEADER "${ISPC_OUTPUT_DIR}/${ISPC_SOURCE_NAME}_ispc.h")

            # Determine target architecture for ISPC
            if(CMAKE_SIZEOF_VOID_P EQUAL 8)
                set(ISPC_ARCH "x86-64")
            else()
                set(ISPC_ARCH "x86")
            endif()

            # Determine ISPC targets based on platform and configuration
            # Default to SSE4 and AVX2 for broad compatibility
            set(ISPC_TARGETS "sse4,avx2")

            # Optional: Detect CPU features for optimal targets
            # For production, you might want to add AVX-512 support
            # set(ISPC_TARGETS "sse4,avx2,avx512skx-i32x16")

            # Build include directories for ISPC compiler
            set(ISPC_INCLUDES)
            list(APPEND ISPC_INCLUDES "-I${CMAKE_CURRENT_SOURCE_DIR}/Public")
            list(APPEND ISPC_INCLUDES "-I${CMAKE_CURRENT_SOURCE_DIR}/Private")

            # Add any dependency include directories
            if(ARG_DEPENDENCIES)
                foreach(DEP ${ARG_DEPENDENCIES})
                    # Try to get include directories from the dependency
                    get_target_property(DEP_INCLUDES ${DEP} INTERFACE_INCLUDE_DIRECTORIES)
                    if(DEP_INCLUDES)
                        foreach(INC_DIR ${DEP_INCLUDES})
                            list(APPEND ISPC_INCLUDES "-I${INC_DIR}")
                        endforeach()
                    endif()
                endforeach()
            endif()

            # Add custom command to compile ISPC file
            add_custom_command(
                OUTPUT ${ISPC_OUTPUT_OBJ} ${ISPC_OUTPUT_HEADER}
                COMMAND ${ISPC_EXECUTABLE}
                    ${ISPC_SOURCE}
                    -o ${ISPC_OUTPUT_OBJ}
                    -h ${ISPC_OUTPUT_HEADER}
                    --arch=${ISPC_ARCH}
                    --target=${ISPC_TARGETS}
                    --pic  # Position Independent Code for shared libraries
                    ${ISPC_INCLUDES}
                DEPENDS ${ISPC_SOURCE} ${MODULE_ISPC_HEADERS} ispc_compiler
                COMMENT "Compiling ISPC: ${ISPC_SOURCE_NAME}.ispc"
                VERBATIM
            )

            # Add generated files to tracking lists
            list(APPEND ISPC_GENERATED_OBJECTS ${ISPC_OUTPUT_OBJ})
            list(APPEND ISPC_GENERATED_HEADERS ${ISPC_OUTPUT_HEADER})
        endforeach()

        # Add generated objects to module sources
        list(APPEND MODULE_SOURCES ${ISPC_GENERATED_OBJECTS})

        # Mark generated files as generated (so CMake doesn't complain they don't exist yet)
        set_source_files_properties(${ISPC_GENERATED_OBJECTS} ${ISPC_GENERATED_HEADERS}
            PROPERTIES GENERATED TRUE
        )

        message(STATUS "    [i] ISPC compilation configured with targets: ${ISPC_TARGETS}")
    endif()

    # If no C/C++ source files found, add a dummy file for CMake link language detection
    if(ORIGINAL_SOURCE_COUNT EQUAL 0)
        set(DUMMY_FILE "${CMAKE_CURRENT_BINARY_DIR}/Keep.cpp")
        if(NOT EXISTS ${DUMMY_FILE})
            file(WRITE ${DUMMY_FILE} "// Auto-generated keep file for link language detection\n")
        endif()
        list(APPEND MODULE_SOURCES ${DUMMY_FILE})
        message(STATUS "    [i] No C/C++ sources found, added dummy file for link language detection")
    endif()

    # Create the library target using the export name
    add_library(${EXPORT_NAME} ${MODULE_SOURCES} ${MODULE_HEADERS} ${MODULE_ISPC_HEADERS})

    # Create the alias for easier internal linking
    add_library(${ALIAS_NAME} ALIAS ${EXPORT_NAME})

    # Set target properties
    set_target_properties(${EXPORT_NAME} PROPERTIES
        # Output name (the actual .dll/.lib filename) - use the export name for consistency
        OUTPUT_NAME ${EXPORT_NAME}

        # Export symbol for DLL builds (e.g., GP_CORE_API_EXPORTS)
        DEFINE_SYMBOL "GP_${ARG_NAME}_API_EXPORTS"

        # Symbol visibility (use default so tests can link without export macros)
        CXX_VISIBILITY_PRESET default
        VISIBILITY_INLINES_HIDDEN OFF

        # Position Independent Code (for shared libraries on Linux)
        POSITION_INDEPENDENT_CODE ON

        # Folder organization in IDE
        FOLDER "${ARG_MODULE_TYPE}"
    )

    # Windows-specific: Export all symbols automatically for DLLs
    # This ensures .lib import libraries are generated properly
    if(WIN32 AND BUILD_SHARED_LIBS)
        set_target_properties(${EXPORT_NAME} PROPERTIES
            WINDOWS_EXPORT_ALL_SYMBOLS ON
        )
    endif()

    # Setup include directories
    target_include_directories(${EXPORT_NAME}
        PUBLIC
            # When building: use Public directory from source tree
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Public>
            # When installed: use include/${NAME} from install tree
            $<INSTALL_INTERFACE:include/${ARG_NAME}>
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/Private
            # Add ISPC generated headers directory if ISPC sources exist
            $<$<BOOL:${MODULE_ISPC_SOURCES}>:${ISPC_OUTPUT_DIR}>
    )

    # Link dependencies
    if(ARG_DEPENDENCIES)
        target_link_libraries(${EXPORT_NAME} PUBLIC ${ARG_DEPENDENCIES})
    endif()

    # Ensure C++20 standard
    target_compile_features(${EXPORT_NAME} PUBLIC cxx_std_20)

    # Installation rules
    install(TARGETS ${EXPORT_NAME}
        EXPORT GPEngineTargets
        ARCHIVE DESTINATION lib      # .lib files (import libraries for Windows DLLs)
        LIBRARY DESTINATION lib      # .so files (Linux shared libraries)
        RUNTIME DESTINATION bin      # .dll/.exe files (Windows DLLs/executables)
        INCLUDES DESTINATION include # Include directories for consumers
    )

    # Install public headers (preserving directory structure)
    if(MODULE_HEADERS)
        install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/Public/
            DESTINATION include/${ARG_NAME}
            FILES_MATCHING
                PATTERN "*.h"
                PATTERN "*.hpp"
                PATTERN "*.inl"
        )
    endif()

    # Install public ISPC headers if any exist
    if(MODULE_ISPC_HEADERS)
        # Filter only public ISPC headers
        set(PUBLIC_ISPC_HEADERS)
        foreach(ISPC_HEADER ${MODULE_ISPC_HEADERS})
            string(FIND ${ISPC_HEADER} "/Public/" PUBLIC_POS)
            if(NOT PUBLIC_POS EQUAL -1)
                list(APPEND PUBLIC_ISPC_HEADERS ${ISPC_HEADER})
            endif()
        endforeach()

        if(PUBLIC_ISPC_HEADERS)
            install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/Public/
                DESTINATION include/${ARG_NAME}
                FILES_MATCHING
                    PATTERN "*.isph"
            )
        endif()
    endif()

    # Debug output
    set(MODULE_INFO_MSG "  [+] Added module: ${ALIAS_NAME} (export: ${EXPORT_NAME}, output: ${EXPORT_NAME})")
    if(MODULE_ISPC_SOURCES)
        list(LENGTH MODULE_ISPC_SOURCES ISPC_COUNT)
        set(MODULE_INFO_MSG "${MODULE_INFO_MSG} [ISPC: ${ISPC_COUNT} file(s)]")
    endif()
    message(STATUS "${MODULE_INFO_MSG}")

    # Check if Tests directory exists
    set(TESTS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Tests")
    if(EXISTS ${TESTS_DIR} AND IS_DIRECTORY ${TESTS_DIR})
        # Gather test source files
        file(GLOB_RECURSE TEST_SOURCES
            "${TESTS_DIR}/*.cpp"
            "${TESTS_DIR}/*.c"
        )

        # Only create test executable if test sources are found
        if(TEST_SOURCES)
            # Count test files for reporting
            list(LENGTH TEST_SOURCES TEST_FILE_COUNT)

            # Generate test executable name: GP<ModuleType><Name>Tests
            set(TEST_EXPORT_NAME "${EXPORT_NAME}Tests")

            message(STATUS "    [i] Found ${TEST_FILE_COUNT} test source file(s) in Tests directory")
            message(STATUS "    [i] Creating test executable: ${TEST_EXPORT_NAME}")

            # Create test executable
            add_executable(${TEST_EXPORT_NAME} ${TEST_SOURCES})

            # Set test executable properties
            set_target_properties(${TEST_EXPORT_NAME} PROPERTIES
                # Output to Binaries/Bin directory (same as other executables)
                RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/Binaries/Bin"
                RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/Binaries/Bin/DEBUG"
                RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/Binaries/Bin/RELEASE"
                RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_SOURCE_DIR}/Binaries/Bin/RELWITHDEBINFO"
                RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_SOURCE_DIR}/Binaries/Bin/MINSIZEREL"

                # Organize in IDE under Tests folder hierarchy
                FOLDER "Tests/${ARG_MODULE_TYPE}"

                # Output name matches test export name
                OUTPUT_NAME ${TEST_EXPORT_NAME}
            )

            # Link against the module being tested
            target_link_libraries(${TEST_EXPORT_NAME}
                PRIVATE
                    ${ALIAS_NAME}  # Link to the module being tested
            )

            # Link against Catch2 if available
            if(TARGET Catch2::Catch2WithMain)
                target_link_libraries(${TEST_EXPORT_NAME}
                    PRIVATE
                        Catch2::Catch2WithMain  # Catch2 with main() provided
                )

                message(STATUS "    [i] Linked ${TEST_EXPORT_NAME} with Catch2::Catch2WithMain")

                # Include Catch2's CMake module for test discovery if available
                if(catch2_SOURCE_DIR)
                    list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
                    include(Catch OPTIONAL RESULT_VARIABLE CATCH_MODULE_FOUND)

                    if(CATCH_MODULE_FOUND)
                        # Automatically discover and register all test cases
                        # This creates individual CTest entries for each TEST_CASE
                        catch_discover_tests(${TEST_EXPORT_NAME}
                            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/Binaries/Bin"
                            PROPERTIES
                                LABELS "${ARG_MODULE_TYPE};${ARG_NAME}"
                        )

                        message(STATUS "    [i] Registered ${TEST_EXPORT_NAME} with CTest (auto-discovery enabled)")
                    else()
                        # Fallback: Manual test registration
                        add_test(NAME ${TEST_EXPORT_NAME} COMMAND ${TEST_EXPORT_NAME})
                        set_tests_properties(${TEST_EXPORT_NAME} PROPERTIES
                            LABELS "${ARG_MODULE_TYPE};${ARG_NAME}"
                        )

                        message(STATUS "    [i] Registered ${TEST_EXPORT_NAME} with CTest (manual registration)")
                    endif()
                endif()
            elseif(TARGET Catch2::Catch2)
                # If only Catch2::Catch2 is available (without main)
                target_link_libraries(${TEST_EXPORT_NAME}
                    PRIVATE
                        Catch2::Catch2
                )

                message(STATUS "    [i] Linked ${TEST_EXPORT_NAME} with Catch2::Catch2")
                message(STATUS "    [i] Note: Tests must provide their own main() function")

                # Manual test registration
                add_test(NAME ${TEST_EXPORT_NAME} COMMAND ${TEST_EXPORT_NAME})
                set_tests_properties(${TEST_EXPORT_NAME} PROPERTIES
                    LABELS "${ARG_MODULE_TYPE};${ARG_NAME}"
                )

                message(STATUS "    [i] Registered ${TEST_EXPORT_NAME} with CTest")
            else()
                message(STATUS "    [x] Catch2 not found - tests will be compiled but not linked to test framework")
                message(STATUS "    [x] To enable test framework, add Catch2 to ThirdParty dependencies")

                # Still register the test with CTest even without Catch2
                add_test(NAME ${TEST_EXPORT_NAME} COMMAND ${TEST_EXPORT_NAME})
                set_tests_properties(${TEST_EXPORT_NAME} PROPERTIES
                    LABELS "${ARG_MODULE_TYPE};${ARG_NAME}"
                )
            endif()

            # Ensure C++20 standard for tests
            target_compile_features(${TEST_EXPORT_NAME} PRIVATE cxx_std_20)
        else()
            message(STATUS "    [i] No test source files found in Tests directory for module ${ARG_NAME}")
        endif()
    else()
        message(STATUS "    [i] No Tests directory found for module ${ARG_NAME}")
    endif()
endfunction()
