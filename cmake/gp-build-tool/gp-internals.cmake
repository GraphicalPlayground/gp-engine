# Copyright (c) - Graphical Playground. All rights reserved.

include(gp-build-tool/gp-utils)

macro(__gpDefineCMakeTarget)
  gpCheckInTarget()

  # Ensure that this macro is called inside the CONFIGURATION phase
  gpGetCurrentPhase(CURRENT_PHASE)
  if (NOT CURRENT_PHASE STREQUAL "CONFIGURATION")
    message(FATAL_ERROR "[GPBT] Internal error: __gpDefineCMakeTargets should only be called during the CONFIGURATION phase. Current phase: ${CURRENT_PHASE}")
  endif()

  # Define the actual CMake target based on the collected information
  set(__GP_TARGET_EXPORT_NAME "gp_${__GP_TARGET_NAME}")
  set(__GP_TARGET_ALIAS_NAME "gp::${__GP_TARGET_NAME}")
  gpVerbose("Defining CMake target '${__GP_TARGET_EXPORT_NAME}' of type '${__GP_TARGET_TYPE}' with alias '${__GP_TARGET_ALIAS_NAME}'")

  # If no sources were added, add a dummy source to ensure the target is created
  list(LENGTH __GP_TARGET_SOURCES NUM_SOURCES)
  if (NUM_SOURCES EQUAL 0)
    set(DUMMY_FILE "${CMAKE_CURRENT_BINARY_DIR}/GPBTDummy.cpp")
    if (NOT EXISTS "${DUMMY_FILE}")
      file(WRITE "${DUMMY_FILE}" "// Auto-generated keep file for link language detection\n")
    endif()
    list(APPEND __GP_TARGET_SOURCES "${DUMMY_FILE}")
    message(STATUS "[GPBT] No sources specified for target '${__GP_TARGET_NAME}'. Added dummy source to ensure target creation.")
  endif()

  # Gather the header files for IDE integration (optional, but improves IDE experience)
  set(__GP_TARGET_ALL_HEADERS)
  foreach(_dir ${__GP_TARGET_PUB_INCLUDES} ${__GP_TARGET_PRV_INCLUDES} ${__GP_TARGET_INT_INCLUDES})
    file(GLOB_RECURSE _headers "${_dir}/*.h" "${_dir}/*.hpp" "${_dir}/*.hh" "${_dir}/*.hxx")
    list(APPEND __GP_TARGET_ALL_HEADERS ${_headers})
  endforeach()

  if("${__GP_TARGET_TYPE}" STREQUAL "module")
    add_library(${__GP_TARGET_EXPORT_NAME} ${__GP_TARGET_SOURCES} ${__GP_TARGET_ALL_HEADERS})
    add_library(${__GP_TARGET_ALIAS_NAME} ALIAS ${__GP_TARGET_EXPORT_NAME})
  elseif ("${__GP_TARGET_TYPE}" STREQUAL "executable")
    add_executable(${__GP_TARGET_EXPORT_NAME} ${__GP_TARGET_SOURCES} ${__GP_TARGET_ALL_HEADERS} ${__GP_EXEC_RESOURCES})
    add_executable(${__GP_TARGET_ALIAS_NAME} ALIAS ${__GP_TARGET_EXPORT_NAME})

    if(WIN32)
      set_target_properties(${__GP_TARGET_EXPORT_NAME} PROPERTIES
        WIN32_EXECUTABLE $<NOT:$<BOOL:${__GP_EXEC_IS_TERMINAL}>>
      )
    endif()

    target_compile_definitions(${__GP_TARGET_EXPORT_NAME} PRIVATE "GP_TERMINAL_APP=$<BOOL:${__GP_EXEC_IS_TERMINAL}>")
  endif()

  # Set target properties for symbol export, visibility, and other relevant settings
  set_target_properties(${__GP_TARGET_EXPORT_NAME} PROPERTIES
    OUTPUT_NAME "${__GP_TARGET_OUTPUT_NAME}"
    DEFINE_SYMBOL "GP_${__GP_TARGET_NAME_UPPER}_API_EXPORTS"
    CXX_VISIBILITY_PRESET default
    VISIBILITY_INLINES_HIDDEN OFF
    POSITION_INDEPENDENT_CODE ON
    FOLDER "${__GP_TARGET_TYPE}"
  )

  if(WIN32 AND BUILD_SHARED_LIBS)
    set_target_properties(${__GP_TARGET_EXPORT_NAME} PROPERTIES
      WINDOWS_EXPORT_ALL_SYMBOLS ON
    )
  endif()

  # Set include directories with proper visibility (PUBLIC, PRIVATE, INTERFACE)
  target_include_directories(${__GP_TARGET_EXPORT_NAME}
    PUBLIC
      # Map all source-tree public paths to the Build Interface
      $<BUILD_INTERFACE:${__GP_TARGET_PUB_INCLUDES}>
      # Define where headers will live after installation
      $<INSTALL_INTERFACE:include/${__GP_TARGET_NAME}>
    PRIVATE
      # Private and Internal remain local to this target
      ${__GP_TARGET_PRV_INCLUDES}
      ${__GP_TARGET_INT_INCLUDES}
  )

  # Add precompiled headers if specified
  list(LENGTH __GP_TARGET_PCH_HEADERS NUM_PCH_HEADERS)
  if (NUM_PCH_HEADERS GREATER 0)
    target_precompile_headers(${__GP_TARGET_EXPORT_NAME} PRIVATE ${__GP_TARGET_PCH_HEADERS})
  endif()

  # Apply compile flags with proper visibility
  list(LENGTH __GP_TARGET_PUB_COMP_FLAGS NUM_PUB_FLAGS)
  if (NUM_PUB_FLAGS GREATER 0)
    target_compile_options(${__GP_TARGET_EXPORT_NAME} PUBLIC ${__GP_TARGET_PUB_COMP_FLAGS})
  endif()

  list(LENGTH __GP_TARGET_PRV_COMP_FLAGS NUM_PRV_FLAGS)
  if (NUM_PRV_FLAGS GREATER 0)
    target_compile_options(${__GP_TARGET_EXPORT_NAME} PRIVATE ${__GP_TARGET_PRV_COMP_FLAGS})
  endif()

  # Get the list of all registered targets to validate dependencies later
  get_property(_ALL_TARGETS GLOBAL PROPERTY GP_REGISTERED_TARGETS)

  # Set dependencies with proper visibility (PUBLIC, PRIVATE, INTERFACE)
  list(LENGTH __GP_TARGET_PUB_DEPS NUM_PUB_DEPS)
  if (NUM_PUB_DEPS GREATER 0)
    set(_FINAL_PUB_DEPS)
    foreach(_dep IN LISTS __GP_TARGET_PUB_DEPS)
      # If this is one of our internal GP modules, namespace it. Otherwise, leave it alone.
      if (_dep IN_LIST _ALL_TARGETS AND NOT _dep MATCHES "^gp::")
        list(APPEND _FINAL_PUB_DEPS "gp::${_dep}")
      else()
        list(APPEND _FINAL_PUB_DEPS "${_dep}")
      endif()
    endforeach()
    target_link_libraries(${__GP_TARGET_EXPORT_NAME} PUBLIC ${_FINAL_PUB_DEPS})
  endif()

  list(LENGTH __GP_TARGET_PRV_DEPS NUM_PRV_DEPS)
  if (NUM_PRV_DEPS GREATER 0)
    set(_FINAL_PRV_DEPS)
    foreach(_dep IN LISTS __GP_TARGET_PRV_DEPS)
      if (_dep IN_LIST _ALL_TARGETS AND NOT _dep MATCHES "^gp::")
        list(APPEND _FINAL_PRV_DEPS "gp::${_dep}")
      else()
        list(APPEND _FINAL_PRV_DEPS "${_dep}")
      endif()
    endforeach()
    target_link_libraries(${__GP_TARGET_EXPORT_NAME} PRIVATE ${_FINAL_PRV_DEPS})
  endif()

  list(LENGTH __GP_TARGET_INT_DEPS NUM_INT_DEPS)
  if (NUM_INT_DEPS GREATER 0)
    set(_FINAL_INT_DEPS)
    foreach(_dep IN LISTS __GP_TARGET_INT_DEPS)
      if (_dep IN_LIST _ALL_TARGETS AND NOT _dep MATCHES "^gp::")
        list(APPEND _FINAL_INT_DEPS "gp::${_dep}")
      else()
        list(APPEND _FINAL_INT_DEPS "${_dep}")
      endif()
    endforeach()
    target_link_libraries(${__GP_TARGET_EXPORT_NAME} PRIVATE ${_FINAL_INT_DEPS})
  endif()

  # Ensure C++23 standard is used for all targets
  target_compile_features(${__GP_TARGET_EXPORT_NAME} PUBLIC cxx_std_23)

  # Setup installation rules for the target (optional, but common for libraries)
  install(TARGETS ${__GP_TARGET_EXPORT_NAME}
    EXPORT GPEngineTargets
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    INCLUDES DESTINATION include
  )

  # Install public headers to the appropriate location under include/ for this target.
  foreach(_dir IN LISTS __GP_TARGET_PUB_INCLUDES)
    install(DIRECTORY ${_dir}/
      DESTINATION include/${__GP_TARGET_NAME}
      FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp" PATTERN "*.hh" PATTERN "*.hxx"
    )
  endforeach()

  # Strict warning handling (optional, but recommended for better code quality)
  if (__GP_TARGET_ENABLE_STRICT_WARNING)
    if (MSCV)
      target_compile_options(${__GP_TARGET_EXPORT_NAME} PRIVATE /W4 /WX)
    else()
      target_compile_options(${__GP_TARGET_EXPORT_NAME} PRIVATE -Wall -Wextra -Werror)
    endif()
  endif()

  # Create the target for IPSC, if enabled
  if(__GP_TARGET_ENABLE_IPSC)
    __gpDefineIPSCTarget()
  endif()

  # Create the target for tests, if enabled
  if (__GP_TARGET_ENABLE_TESTS)
    __gpDefineTestsTarget()
  endif()

  # Create the target for benchmarks, if enabled
  if(__GP_TARGET_ENABLE_BENCHMARKS)
    __gpDefineBenchmarksTarget()
  endif()

  # Create the target for examples, if enabled
  if(__GP_TARGET_ENABLE_EXAMPLES)
    __gpDefineExamplesTarget()
  endif()

  # Clean up internal variables that should not persist after target definition
  unset(__GP_TARGET_EXPORT_NAME)
  unset(__GP_TARGET_ALIAS_NAME)
  unset(__GP_TARGET_ALL_HEADERS)
endmacro()

macro(__gpDefineIPSCTarget)
  gpCheckInTarget()

  gpGetCurrentPhase(CURRENT_PHASE)
  if (NOT CURRENT_PHASE STREQUAL "CONFIGURATION")
    message(FATAL_ERROR "[GPBT] Internal error: __gpDefineIPSCTarget should only be called during the CONFIGURATION phase. Current phase: ${CURRENT_PHASE}")
  endif()

  message(WARNING "[GPBT] IPSC target generation is not yet implemented for target '${__GP_TARGET_NAME}'.")
endmacro()

macro(__gpDefineTestsTarget)
  gpCheckInTarget()

  gpGetCurrentPhase(CURRENT_PHASE)
  if (NOT CURRENT_PHASE STREQUAL "CONFIGURATION")
    message(FATAL_ERROR "[GPBT] Internal error: __gpDefineTestsTarget should only be called during the CONFIGURATION phase. Current phase: ${CURRENT_PHASE}")
  endif()

  if (NOT TARGET Catch2WithMain)
    message(FATAL_ERROR "[GPBT] Catch2 target 'Catch2WithMain' not found. Ensure Catch2 is properly added as a dependency for tests.")
  endif()

  set(__GP_TARGET_TESTS_DIR "${__GP_TARGET_LOCATION}/tests")
  if (NOT EXISTS "${__GP_TARGET_TESTS_DIR}")
    message(WARNING "[GPBT] Tests directory '${__GP_TARGET_TESTS_DIR}' does not exist for target '${__GP_TARGET_NAME}'. No tests will be generated.")
  else()
    file(GLOB_RECURSE __GP_TARGET_TEST_SOURCES "${__GP_TARGET_TESTS_DIR}/*.cpp" "${__GP_TARGET_TESTS_DIR}/*.cc")

    if (NOT __GP_TARGET_TEST_SOURCES)
      message(WARNING "[GPBT] No test sources found in '${__GP_TARGET_TESTS_DIR}' for target '${__GP_TARGET_NAME}'. No tests will be generated.")
    else()
      add_executable(${__GP_TARGET_EXPORT_NAME}_tests ${__GP_TARGET_TEST_SOURCES})
      target_link_libraries(${__GP_TARGET_EXPORT_NAME}_tests PRIVATE Catch2WithMain ${__GP_TARGET_EXPORT_NAME})
      set_target_properties(${__GP_TARGET_EXPORT_NAME}_tests PROPERTIES
        OUTPUT_NAME "${__GP_TARGET_OUTPUT_NAME}_tests"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/binaries/bin/tests"
        FOLDER "tests"
      )

      if (MSCV)
        set_target_properties(${__GP_TARGET_EXPORT_NAME}_tests PROPERTIES
          RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/binaries/bin/DEBUG/tests"
          RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/binaries/bin/RELEASE/tests"
          RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_SOURCE_DIR}/binaries/bin/RELWITHDEBINFO/tests"
          RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_SOURCE_DIR}/binaries/bin/MINSIZEREL/tests"
        )
      endif()

      target_compile_features(${__GP_TARGET_EXPORT_NAME}_tests PUBLIC cxx_std_23)

      add_test(NAME ${__GP_TARGET_NAME}_tests COMMAND ${__GP_TARGET_EXPORT_NAME}_tests)
      set_tests_properties(${__GP_TARGET_NAME}_tests PROPERTIES
        LABELS "gp;${__GP_TARGET_NAME};tests"
      )
    endif()
  endif()
endmacro()

macro(__gpDefineBenchmarksTarget)
  gpCheckInTarget()

  gpGetCurrentPhase(CURRENT_PHASE)
  if (NOT CURRENT_PHASE STREQUAL "CONFIGURATION")
    message(FATAL_ERROR "[GPBT] Internal error: __gpDefineBenchmarksTarget should only be called during the CONFIGURATION phase. Current phase: ${CURRENT_PHASE}")
  endif()

  message(WARNING "[GPBT] Benchmarks target generation is not yet implemented for target '${__GP_TARGET_NAME}'.")
endmacro()

macro(__gpDefineExamplesTarget)
  gpCheckInTarget()

  gpGetCurrentPhase(CURRENT_PHASE)
  if (NOT CURRENT_PHASE STREQUAL "CONFIGURATION")
    message(FATAL_ERROR "[GPBT] Internal error: __gpDefineExamplesTarget should only be called during the CONFIGURATION phase. Current phase: ${CURRENT_PHASE}")
  endif()

  message(WARNING "[GPBT] Examples target generation is not yet implemented for target '${__GP_TARGET_NAME}'.")
endmacro()
