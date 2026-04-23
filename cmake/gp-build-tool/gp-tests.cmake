# Copyright (c) - Graphical Playground. All rights reserved.
# For more information, see https://graphical-playground/legal
# mailto:support AT graphical-playground DOT com

macro(_gpStartTestsSection SECTION_NAME)
  message(STATUS "")
  message(STATUS " === Running ${SECTION_NAME} Tests ===")

  # Initialize counters and state using a safer __gp prefix
  set(__gp_TESTS_TOTAL 0)
  set(__gp_TESTS_PASSED 0)
  set(__gp_TESTS_FAILED 0)
  set(__gp_TEST_FAILURE_MESSAGES "")
endmacro()

macro(_gpRunTest TEST_NAME)
  # Increment total tests
  math(EXPR __gp_TESTS_TOTAL "${__gp_TESTS_TOTAL} + 1")

  # CRITICAL FIX: Evaluate the expression directly so STREQUAL, MATCHES, etc., work.
  if(${ARGN})
    math(EXPR __gp_TESTS_PASSED "${__gp_TESTS_PASSED} + 1")
    message(STATUS "  PASS: ${TEST_NAME}")
  else()
    math(EXPR __gp_TESTS_FAILED "${__gp_TESTS_FAILED} + 1")
    # Record the failure along with the evaluated arguments for easier debugging
    set(__gp_TEST_FAILURE_MESSAGES "${__gp_TEST_FAILURE_MESSAGES}  ❌ ${TEST_NAME} (Failed condition: ${ARGN})\n")
    message(WARNING "  FAIL: ${TEST_NAME}")
  endif()
endmacro()

macro(_gpEndTestsSection)
  message(STATUS "=== Test Summary ===")
  message(STATUS "  Total:  ${__gp_TESTS_TOTAL}")
  message(STATUS "  Passed: ${__gp_TESTS_PASSED}")
  message(STATUS "  Failed: ${__gp_TESTS_FAILED}")

  if(__gp_TESTS_FAILED GREATER 0)
    message(FATAL_ERROR "=== Some tests failed ===\n${__gp_TEST_FAILURE_MESSAGES}")
  else()
    message(STATUS "=== All tests passed successfully! ===")
  endif()

  message(STATUS "")

  # Cleanup internal state
  unset(__gp_TESTS_TOTAL)
  unset(__gp_TESTS_PASSED)
  unset(__gp_TESTS_FAILED)
  unset(__gp_TEST_FAILURE_MESSAGES)
endmacro()
