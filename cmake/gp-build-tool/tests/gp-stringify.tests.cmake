# Copyright (c) - Graphical Playground. All rights reserved.
# For more information, see https://graphical-playground/legal
# mailto:support AT graphical-playground DOT com

include (gp-build-tool/gp-stringify)
include (gp-build-tool/gp-tests)

_gpStartTestsSection("gp-stringify - UPPER_SNAKE_CASE Validation")
  set(TEST_STRING "MY_VARIABLE_NAME")
  gpIsUpperSnakeCase("${TEST_STRING}" result)
  _gpRunTest("Valid UPPER_SNAKE_CASE Test" ${result})

  set(TEST_STRING "My_Variable_Name")
  gpIsUpperSnakeCase("${TEST_STRING}" result)
  _gpRunTest("Invalid UPPER_SNAKE_CASE Test" NOT ${result})

  set(TEST_STRING "")
  gpIsUpperSnakeCase("${TEST_STRING}" result)
  _gpRunTest("Empty String UPPER_SNAKE_CASE Test" NOT ${result})
_gpEndTestsSection()

_gpStartTestsSection("gp-stringify - camelCase Validation")
  set(TEST_STRING "myVariableName")
  gpIsCamelCase("${TEST_STRING}" result)
  _gpRunTest("Valid camelCase Test" ${result})

  set(TEST_STRING "MyVariableName")
  gpIsCamelCase("${TEST_STRING}" result)
  _gpRunTest("Invalid camelCase Test (Starts with Capital)" NOT ${result})

  set(TEST_STRING "my_variableName")
  gpIsCamelCase("${TEST_STRING}" result)
  _gpRunTest("Invalid camelCase Test (Contains Underscore)" NOT ${result})
_gpEndTestsSection()

_gpStartTestsSection("gp-stringify - PascalCase Validation")
  set(TEST_STRING "MyVariableName")
  gpIsPascalCase("${TEST_STRING}" result)
  _gpRunTest("Valid PascalCase Test" ${result})

  set(TEST_STRING "myVariableName")
  gpIsPascalCase("${TEST_STRING}" result)
  _gpRunTest("Invalid PascalCase Test (Starts with Lowercase)" NOT ${result})
_gpEndTestsSection()

_gpStartTestsSection("gp-stringify - to UPPER_SNAKE_CASE")
  set(TEST_STRING "myCustomVariable")
  gpToUpperSnakeCase("${TEST_STRING}" result)
  _gpRunTest("camelCase to UPPER_SNAKE_CASE" "${result}" STREQUAL "MY_CUSTOM_VARIABLE")

  set(TEST_STRING "MyCustomVariable")
  gpToUpperSnakeCase("${TEST_STRING}" result)
  _gpRunTest("PascalCase to UPPER_SNAKE_CASE" "${result}" STREQUAL "MY_CUSTOM_VARIABLE")

  set(TEST_STRING "my_custom_variable")
  gpToUpperSnakeCase("${TEST_STRING}" result)
  _gpRunTest("lower_snake_case to UPPER_SNAKE_CASE" "${result}" STREQUAL "MY_CUSTOM_VARIABLE")
_gpEndTestsSection()

_gpStartTestsSection("gp-stringify - to camelCase")
  set(TEST_STRING "my_custom_variable")
  gpToCamelCase("${TEST_STRING}" result)
  _gpRunTest("lower_snake_case to camelCase" "${result}" STREQUAL "myCustomVariable")

  set(TEST_STRING "MY_CUSTOM_VARIABLE")
  gpToCamelCase("${TEST_STRING}" result)
  _gpRunTest("UPPER_SNAKE_CASE to camelCase" "${result}" STREQUAL "myCustomVariable")

  set(TEST_STRING "MyCustomVariable")
  gpToCamelCase("${TEST_STRING}" result)
  _gpRunTest("PascalCase to camelCase" "${result}" STREQUAL "myCustomVariable")
_gpEndTestsSection()

_gpStartTestsSection("gp-stringify - to PascalCase")
  set(TEST_STRING "my_custom_variable")
  gpToPascalCase("${TEST_STRING}" result)
  _gpRunTest("lower_snake_case to PascalCase" "${result}" STREQUAL "MyCustomVariable")

  set(TEST_STRING "myCustomVariable")
  gpToPascalCase("${TEST_STRING}" result)
  _gpRunTest("camelCase to PascalCase" "${result}" STREQUAL "MyCustomVariable")

  set(TEST_STRING "MY_CUSTOM_VARIABLE")
  gpToPascalCase("${TEST_STRING}" result)
  _gpRunTest("UPPER_SNAKE_CASE to PascalCase" "${result}" STREQUAL "MyCustomVariable")
_gpEndTestsSection()

_gpStartTestsSection("gp-stringify - Edge Cases")
  # Testing consecutive uppercase letters (Acronyms)
  set(TEST_STRING "XMLParser")
  gpToLowerSnakeCase("${TEST_STRING}" result)
  _gpRunTest("Acronym splitting (XMLParser -> xml_parser)" "${result}" STREQUAL "xml_parser")

  set(TEST_STRING "myHTTPResponse")
  gpToUpperSnakeCase("${TEST_STRING}" result)
  _gpRunTest("Embedded Acronym (myHTTPResponse -> MY_HTTP_RESPONSE)" "${result}" STREQUAL "MY_HTTP_RESPONSE")

  # Testing messy separators
  set(TEST_STRING "my__messy---variable_name")
  gpToCamelCase("${TEST_STRING}" result)
  _gpRunTest("Multiple mixed separators to camelCase" "${result}" STREQUAL "myMessyVariableName")

  set(TEST_STRING "__leading_and_trailing__")
  gpToPascalCase("${TEST_STRING}" result)
  _gpRunTest("Leading and trailing separators to PascalCase" "${result}" STREQUAL "LeadingAndTrailing")
_gpEndTestsSection()
