# Copyright (c) - Graphical Playground. All rights reserved.
# For more information, see https://graphical-playground/legal
# mailto:support AT graphical-playground DOT com

include(gp-build-tool/internals/gp-logger.internal)

# @brief A simple testing framework for CMake to validate conditions during the build process.
# It provides macros to define test sections, run individual tests, and summarize results.
# @param[in] sectionName The name of the test section being defined.
macro(_gpStartTestsSection sectionName)
  gpSetLogPrefixEnabled(FALSE)
  gpNewLine()
  gpLog(" === Running ${sectionName} Tests ===")

  # Initialize counters and state using a safer __GP prefix
  set(__GP_TESTS_TOTAL 0)
  set(__GP_TESTS_PASSED 0)
  set(__GP_TESTS_FAILED 0)
  set(__GP_TEST_FAILURE_MESSAGES "")
endmacro()

# @brief Runs a test and updates the test counters based on the result.
# @param[in] testName A descriptive name for the test being run.
# @param[in] ARGN The condition to evaluate for the test. If it evaluates to true, the test is considered passed; otherwise, it is failed.
macro(_gpRunTest testName)
  # Increment total tests
  math(EXPR __GP_TESTS_TOTAL "${__GP_TESTS_TOTAL} + 1")

  # CRITICAL FIX: Evaluate the expression directly so STREQUAL, MATCHES, etc., work.
  if(${ARGN})
    math(EXPR __GP_TESTS_PASSED "${__GP_TESTS_PASSED} + 1")
    gpSuccess("Test passed: ${testName}")
  else()
    math(EXPR __GP_TESTS_FAILED "${__GP_TESTS_FAILED} + 1")
    # Record the failure along with the evaluated arguments for easier debugging
    set(__GP_TEST_FAILURE_MESSAGES "${__GP_TEST_FAILURE_MESSAGES}  ${testName} (Failed condition: ${ARGN})\n")
    gpWarning("Test failed: ${testName}")
  endif()
endmacro()

# @brief Summarizes the test results and reports any failures. If there are failed tests, it will stop the build process with an error.
macro(_gpEndTestsSection)
  gpLog(" === Test Summary ===")
  gpLog("  Total:  ${__GP_TESTS_TOTAL}")
  gpLog("  Passed: ${GP_GREEN}${__GP_TESTS_PASSED}${GP_RESET}")
  gpLog("  Failed: ${GP_RED}${__GP_TESTS_FAILED}${GP_RESET}")

  if(__GP_TESTS_FAILED GREATER 0)
    gpFatal(" === Some tests failed ===\n${__GP_TEST_FAILURE_MESSAGES}")
  else()
    gpLog(" === All tests passed successfully! ===")
  endif()

  gpNewLine()

  # Cleanup internal state
  unset(__GP_TESTS_TOTAL)
  unset(__GP_TESTS_PASSED)
  unset(__GP_TESTS_FAILED)
  unset(__GP_TEST_FAILURE_MESSAGES)
  gpRestorePreviousLogPrefixState()
endmacro()
