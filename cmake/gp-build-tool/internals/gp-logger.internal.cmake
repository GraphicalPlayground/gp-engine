# Copyright (c) - Graphical Playground. All rights reserved.
# For more information, see https://graphical-playground/legal
# mailto:support AT graphical-playground DOT com

# Generate the ASCII escape character
string(ASCII 27 gp_ESC)

# Text Formatting
set(GP_RESET "${gp_ESC}[0m")
set(GP_BOLD  "${gp_ESC}[1m")

# Foreground Colors
set(GP_RED     "${gp_ESC}[31m")
set(GP_GREEN   "${gp_ESC}[32m")
set(GP_YELLOW  "${gp_ESC}[33m")
set(GP_BLUE    "${gp_ESC}[34m")
set(GP_MAGENTA "${gp_ESC}[35m")
set(GP_CYAN    "${gp_ESC}[36m")
set(GP_WHITE   "${gp_ESC}[37m")

# Bold Foreground Colors
set(GP_BOLD_RED    "${gp_ESC}[1;31m")
set(GP_BOLD_GREEN  "${gp_ESC}[1;32m")
set(GP_BOLD_YELLOW "${gp_ESC}[1;33m")

# Default state for the prefix
set(__GP_LOG_PREFIX_ENABLED TRUE CACHE INTERNAL "Enable or disable the [GPBT] log prefix")
set(__GP_PREVIOUS_LOG_PREFIX_ENABLED TRUE CACHE INTERNAL "Store the previous state of the log prefix for scope management")

# @brief Enables or disables the [GPBT] prefix on all gpLog/gpWarning/gpFatal calls.
# @param[in] value TRUE to enable the prefix, FALSE to disable it.
function(gpSetLogPrefixEnabled value)
  set(__GP_PREVIOUS_LOG_PREFIX_ENABLED ${__GP_LOG_PREFIX_ENABLED} CACHE INTERNAL "Store the previous state of the log prefix for scope management")
  set(__GP_LOG_PREFIX_ENABLED ${value} CACHE INTERNAL "Enable or disable the [GPBT] log prefix")
endfunction()

# @brief Restores the previous state of the log prefix enabled setting. Useful for ensuring that changes to the log prefix state are properly scoped.
function(gpRestorePreviousLogPrefixState)
  set(__GP_LOG_PREFIX_ENABLED ${__GP_PREVIOUS_LOG_PREFIX_ENABLED} CACHE INTERNAL "Restore the previous state of the log prefix enabled setting")
endfunction()

# @brief Internal function to format log messages with optional severity and prefix.
# @param[out] outVar The variable to store the formatted message in.
# @param[in] severityColor The color code for the severity level (e.g. ${GP_RED} for errors).
# @param[in] severityName The name of the severity level (e.g. "ERROR", "WARNING"). Can be empty for no severity.
macro(_gpFormatLogMessage outVar severityColor severityName)
  set(_prefix "")
  if(__GP_LOG_PREFIX_ENABLED)
    set(_prefix "[GPBT] ")
  endif()
  
  # Concatenate all passed arguments into a single string 
  # (so users can do gpLog("Value is: " ${MY_VAR}))
  set(_msg_list ${ARGN})
  string(REPLACE ";" "" _msg "${_msg_list}")
  
  if("${severityName}" STREQUAL "")
    set(${outVar} "${_prefix}${_msg}")
  else()
    set(${outVar} "${severityColor}${_prefix}${severityName}: ${_msg}${GP_RESET}")
  endif()
endmacro()

# @brief Prints a standard log message.
function(gpLog)
  _gpFormatLogMessage(_formatted_msg "" "" ${ARGN})
  message(STATUS "${_formatted_msg}")
endfunction()

# @brief Prints a success in green.
function(gpSuccess)
  _gpFormatLogMessage(_formatted_msg "${GP_BOLD_GREEN}" "SUCCESS" ${ARGN})
  message(STATUS "${_formatted_msg}")
endfunction()

# @brief Prints a warning in yellow WITHOUT a CMake stack trace.
function(gpWarning)
  _gpFormatLogMessage(_formatted_msg "${GP_BOLD_YELLOW}" "WARNING" ${ARGN})
  # Using STATUS instead of WARNING prevents CMake from dumping the stack trace!
  message(STATUS "${_formatted_msg}")
endfunction()

# @brief Prints a fatal error in red WITHOUT a stack trace on the actual message, then halts.
function(gpFatal)
  _gpFormatLogMessage(_formatted_msg "${GP_BOLD_RED}" "FATAL ERROR" ${ARGN})
  # Print the colored error nicely without a stack trace
  message(STATUS "${_formatted_msg}")
  
  # We still MUST call FATAL_ERROR to stop CMake execution, but we pass a generic 
  # short message so the user focuses on your beautiful custom colored log above it.
  message(FATAL_ERROR "Build halted. See the error above.")
endfunction()

# @brief Prints a verbose message only if GP_BUILD_TOOL_VERBOSE is enabled.
function(gpVerbose)
  if(GP_BUILD_TOOL_VERBOSE)
    _gpFormatLogMessage(_formatted_msg "${GP_CYAN}" "VERBOSE" ${ARGN})
    message(STATUS "${_formatted_msg}")
  endif()
endfunction()

# @brief Prints a blank line for better readability in logs.
function(gpNewLine)
  message(STATUS "")
endfunction(gpNewLine)
