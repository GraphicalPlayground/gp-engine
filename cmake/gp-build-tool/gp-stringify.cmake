# Copyright (c) - Graphical Playground. All rights reserved.
# For more information, see https://graphical-playground/legal
# mailto:support AT graphical-playground DOT com

# @brief Checks if a string is in lower_snake_case.
# @param[in] input The string to check.
# @param[out] outVar The variable to store the result.
function(gpIsLowerSnakeCase input outVar)
  string(REGEX MATCH "^[a-z0-9]+(_[a-z0-9]+)*$" match "${input}")
  if(match AND NOT input STREQUAL "")
    set(${outVar} TRUE PARENT_SCOPE)
  else()
    set(${outVar} FALSE PARENT_SCOPE)
  endif()
endfunction()

# @brief Checks if a string is in UPPER_SNAKE_CASE.
# @param[in] input The string to check.
# @param[out] outVar The variable to store the result.
function(gpIsUpperSnakeCase input outVar)
  string(REGEX MATCH "^[A-Z0-9]+(_[A-Z0-9]+)*$" match "${input}")
  if(match AND NOT input STREQUAL "")
    set(${outVar} TRUE PARENT_SCOPE)
  else()
    set(${outVar} FALSE PARENT_SCOPE)
  endif()
endfunction()

# @brief Checks if a string is in camelCase.
# @param[in] input The string to check.
# @param[out] outVar The variable to store the result.
function(gpIsCamelCase input outVar)
  string(REGEX MATCH "^[a-z][a-z0-9]*([A-Z][a-z0-9]*)*$" match "${input}")
  if(match AND NOT input STREQUAL "")
    set(${outVar} TRUE PARENT_SCOPE)
  else()
    set(${outVar} FALSE PARENT_SCOPE)
  endif()
endfunction()

# @brief Checks if a string is in PascalCase.
# @param[in] input The string to check.
# @param[out] outVar The variable to store the result.
function(gpIsPascalCase input outVar)
  string(REGEX MATCH "^([A-Z][a-z0-9]*)+$" match "${input}")
  if(match AND NOT input STREQUAL "")
    set(${outVar} TRUE PARENT_SCOPE)
  else()
    set(${outVar} FALSE PARENT_SCOPE)
  endif()
endfunction()

# @brief Internal function to tokenize a string into words based on case transitions and separators.
# @param[in] input The string to tokenize.
# @param[out] outVar The variable to store the list of words.
function(_gpTokenizeToWords input outVar)
  # (e.g., "myCustomVar2" -> "my;Custom;Var2", "myHTTPResponse" -> "my;HTTPResponse")
  string(REGEX REPLACE "([a-z0-9])([A-Z])" "\\1;\\2" tokenized "${input}")

  # (e.g., "XMLParser" -> "XML;Parser", "my;HTTPResponse" -> "my;HTTP;Response")
  string(REGEX REPLACE "([A-Z]+)([A-Z][a-z])" "\\1;\\2" tokenized "${tokenized}")

  # (Using + naturally collapses multiple separators like "__" or "--")
  string(REGEX REPLACE "[-_]+" ";" tokenized "${tokenized}")

  string(TOLOWER "${tokenized}" tokenized)
  list(REMOVE_ITEM tokenized "")
  set(${outVar} "${tokenized}" PARENT_SCOPE)
endfunction()

# @brief Converts a string to lower_snake_case.
# @param[in] input The string to convert.
# @param[out] outVar The variable to store the result.
function(gpToLowerSnakeCase input outVar)
  _gpTokenizeToWords("${input}" words)
  string(REPLACE ";" "_" result "${words}")
  set(${outVar} "${result}" PARENT_SCOPE)
endfunction()

# @brief Converts a string to UPPER_SNAKE_CASE.
# @param[in] input The string to convert.
# @param[out] outVar The variable to store the result.
function(gpToUpperSnakeCase input outVar)
  _gpTokenizeToWords("${input}" words)
  string(REPLACE ";" "_" result "${words}")
  string(TOUPPER "${result}" result)
  set(${outVar} "${result}" PARENT_SCOPE)
endfunction()

# @brief Converts a string to PascalCase.
# @param[in] input The string to convert.
# @param[out] outVar The variable to store the result.
function(gpToPascalCase input outVar)
  _gpTokenizeToWords("${input}" words)
  set(result "")
  foreach(word IN LISTS words)
    string(SUBSTRING "${word}" 0 1 first_char)
    string(TOUPPER "${first_char}" first_char)
    string(SUBSTRING "${word}" 1 -1 rest)
    string(APPEND result "${first_char}${rest}")
  endforeach()
  set(${outVar} "${result}" PARENT_SCOPE)
endfunction()

# @brief Converts a string to camelCase.
# @param[in] input The string to convert.
# @param[out] outVar The variable to store the result.
function(gpToCamelCase input outVar)
  _gpTokenizeToWords("${input}" words)
  set(result "")
  list(LENGTH words len)
  if(len GREATER 0)
    list(GET words 0 first_word)
    set(result "${first_word}")
    list(REMOVE_AT words 0)

    foreach(word IN LISTS words)
      string(SUBSTRING "${word}" 0 1 first_char)
      string(TOUPPER "${first_char}" first_char)
      string(SUBSTRING "${word}" 1 -1 rest)
      string(APPEND result "${first_char}${rest}")
    endforeach()
  endif()
  set(${outVar} "${result}" PARENT_SCOPE)
endfunction()
