# Copyright (c) - Graphical Playground. All rights reserved.
# For more information, see https://graphical-playground/legal
# mailto:support AT graphical-playground DOT com

include (gp-build-tool/internals/gp-scope.internal)
include (gp-build-tool/gp-tests)

_gpStartTestsSection("gp-utils - Scope System")
  # Ensure we start from a clean slate for testing
  set(__GP_SCOPE_DEPTH 0)

  # Test 1: Basic Scoping (Creation and Deletion)
  gpPushScope()
  gpSetScoped(TEST_SCOPE_VAR_1 "HelloWorld")
  _gpRunTest("Scoped variable creation" DEFINED TEST_SCOPE_VAR_1 AND "${TEST_SCOPE_VAR_1}" STREQUAL "HelloWorld")
  gpPopScope()
  _gpRunTest("Scoped variable deletion after pop" NOT DEFINED TEST_SCOPE_VAR_1)

  # Test 2: Variable Masking (Backup and Restore)
  set(TEST_SCOPE_VAR_2 "OriginalValue")
  gpPushScope()
  gpSetScoped(TEST_SCOPE_VAR_2 "ScopedValue")
  _gpRunTest("Scoped variable successfully masks original" "${TEST_SCOPE_VAR_2}" STREQUAL "ScopedValue")
  gpPopScope()
  _gpRunTest("Original variable correctly restored after pop" "${TEST_SCOPE_VAR_2}" STREQUAL "OriginalValue")
  unset(TEST_SCOPE_VAR_2)

  # Test 3: Nested Scopes
  set(TEST_SCOPE_VAR_3 "Level0")
  
  gpPushScope()
  gpSetScoped(TEST_SCOPE_VAR_3 "Level1")
  
  gpPushScope()
  gpSetScoped(TEST_SCOPE_VAR_3 "Level2")
  _gpRunTest("Nested scope reads Level 2 value" "${TEST_SCOPE_VAR_3}" STREQUAL "Level2")
  gpPopScope()

  _gpRunTest("Nested scope correctly falls back to Level 1" "${TEST_SCOPE_VAR_3}" STREQUAL "Level1")
  gpPopScope()

  _gpRunTest("Global scope correctly falls back to Level 0" "${TEST_SCOPE_VAR_3}" STREQUAL "Level0")
  unset(TEST_SCOPE_VAR_3)

  # Test 4: Mutation Tracking (Using standard CMake commands)
  set(TEST_SCOPE_LIST "OriginalList")
  gpPushScope()
  gpSetScoped(TEST_SCOPE_LIST "A")
  
  # Mutate heavily using standard CMake commands
  list(APPEND TEST_SCOPE_LIST "B" "C")
  string(REPLACE ";" "_" TEST_SCOPE_LIST_STR "${TEST_SCOPE_LIST}")
  
  _gpRunTest("Scoped list mutation successful" "${TEST_SCOPE_LIST_STR}" STREQUAL "A_B_C")
  gpPopScope()
  
  _gpRunTest("Mutated scoped list restored to original after pop" "${TEST_SCOPE_LIST}" STREQUAL "OriginalList")
  unset(TEST_SCOPE_LIST)

  # Test 5: Multiple Arguments Initialization
  gpPushScope()
  gpSetScoped(TEST_SCOPE_MULTI "One" "Two" "Three")
  set(LIST_MATCHES FALSE)
  if("${TEST_SCOPE_MULTI}" STREQUAL "One;Two;Three")
    set(LIST_MATCHES TRUE)
  endif()
  _gpRunTest("Scoped initialization handles multiple arguments (lists)" ${LIST_MATCHES})
  gpPopScope()
  _gpRunTest("Multiple arguments cleaned up after pop" NOT DEFINED TEST_SCOPE_MULTI)

  # Ensure the scope stack is perfectly balanced after all tests
  _gpRunTest("Scope depth returned to 0" __GP_SCOPE_DEPTH EQUAL 0)
_gpEndTestsSection()
