# Copyright (c) - Graphical Playground. All rights reserved.
# For more information, see https://graphical-playground/legal
# mailto:support AT graphical-playground DOT com

include(gp-build-tool/internals/gp-logger.internal)

# Verify that scope depth variable is initialized
if(NOT DEFINED __GP_SCOPE_DEPTH)
  set(__GP_SCOPE_DEPTH 0)
endif()

# Track active named scopes to prevent duplicates
if(NOT DEFINED __GP_ACTIVE_NAMED_SCOPES)
  set(__GP_ACTIVE_NAMED_SCOPES "")
endif()

# @brief Pushes a new scope onto the stack. Variables set after this call will be tracked and can
#        be reverted by a corresponding call to gpPopScope().
macro(gpPushScope)
  math(EXPR __GP_SCOPE_DEPTH "${__GP_SCOPE_DEPTH} + 1")
  set(__gp_scope_vars_${__GP_SCOPE_DEPTH} "")
endmacro()

# @brief Pops the current scope from the stack, reverting any variables that were set since the
#        last call to gpPushScope().
macro(gpPopScope)
  if(__GP_SCOPE_DEPTH EQUAL 0)
    gpFatal("gpPopScope called without a matching gpPushScope().")
  endif()

  foreach(_var IN LISTS __gp_scope_vars_${__GP_SCOPE_DEPTH})
    if(__gp_scope_had_${__GP_SCOPE_DEPTH}_${_var})
      set(${_var} "${__gp_scope_backup_${__GP_SCOPE_DEPTH}_${_var}}")
      unset(__gp_scope_backup_${__GP_SCOPE_DEPTH}_${_var})
    else()
      unset(${_var})
    endif()

    unset(__gp_scope_had_${__GP_SCOPE_DEPTH}_${_var})
  endforeach()

  unset(__gp_scope_vars_${__GP_SCOPE_DEPTH})
  math(EXPR __GP_SCOPE_DEPTH "${__GP_SCOPE_DEPTH} - 1")
endmacro()

# @brief Pushes a new named scope. Wraps gpPushScope and tracks the name.
#        Prevents opening a duplicate named scope if it is already active.
# @param[in] scopeName The unique name of the scope to open.
macro(gpPushNamedScope scopeName)
  # Check if the scope is already active (using list(FIND) avoids CMP0057 IN_LIST requirements)
  list(FIND __GP_ACTIVE_NAMED_SCOPES "${scopeName}" _gp_name_idx)
  if(NOT _gp_name_idx EQUAL -1)
    gpFatal("Cannot open named scope '${scopeName}': a scope with this name is already active.")
  endif()

  # Call your existing base scope logic
  gpPushScope()

  # Track the new named scope globally and link it to the current depth
  list(APPEND __GP_ACTIVE_NAMED_SCOPES "${scopeName}")
  set(__gp_scope_name_${__GP_SCOPE_DEPTH} "${scopeName}")
endmacro()

# @brief Optional but recommended: A strict pop that enforces matching names.
# @param[in] scopeName The name of the scope you expect to be popping.
macro(gpPopNamedScope scopeName)
  if(NOT DEFINED __gp_scope_name_${__GP_SCOPE_DEPTH} OR NOT "${__gp_scope_name_${__GP_SCOPE_DEPTH}}" STREQUAL "${scopeName}")
    gpFatal("gpPopNamedScope called for '${scopeName}', but the current top scope is '${__gp_scope_name_${__GP_SCOPE_DEPTH}}'.")
  endif()

  # Delegate to gpPopScope, which will handle the actual cleanup
  gpPopScope()
endmacro()

# @brief Sets a variable in the current scope, tracking it so that it can be reverted by gpPopScope().
#        Must be called after gpPushScope() to ensure the variable is tracked properly.
# @param[in] varName The name of the variable to set.
# @param[in] ARGN The value(s) to set the variable to. Supports lists and multiple values.
macro(gpSetScoped varName)
  # Ensure we are inside a scope
  if(__GP_SCOPE_DEPTH EQUAL 0)
    gpFatal("gpSetScoped called outside of any scope. Call gpPushScope() first.")
  endif()

  # If this is the first time we are modifying this variable in the current scope, back it up
  if(NOT "${varName}" IN_LIST __gp_scope_vars_${__GP_SCOPE_DEPTH})
    list(APPEND __gp_scope_vars_${__GP_SCOPE_DEPTH} "${varName}")

    if(DEFINED ${varName})
      # Variable already exists, back it up
      set(__gp_scope_backup_${__GP_SCOPE_DEPTH}_${varName} "${${varName}}")
      set(__gp_scope_had_${__GP_SCOPE_DEPTH}_${varName} TRUE)
    else()
      # Variable didn't exist, remember that so we completely delete it later
      set(__gp_scope_had_${__GP_SCOPE_DEPTH}_${varName} FALSE)
    endif()
  endif()

  # Actually set the variable (using ARGN to support lists/multiple values)
  set(${varName} ${ARGN})
endmacro()

# @brief Bulk sets multiple variables in the current scope using Key-Value pairs.
#        Must be called after gpPushScope().
# @param[in] ARGN Key-Value pairs. e.g., gpSetScopedMultiple(KEY1 "Val1" KEY2 "Val2")
macro(gpSetScopedMultiple)
  # Capture all arguments as a list
  set(_gp_bulk_args "${ARGN}")
  list(LENGTH _gp_bulk_args _gp_bulk_count)
  math(EXPR _gp_bulk_remainder "${_gp_bulk_count} % 2")

  # Ensure we have perfect pairs
  if(NOT _gp_bulk_remainder EQUAL 0)
    message(FATAL_ERROR "[GPBT] gpSetScopedMultiple requires an even number of arguments (Key-Value pairs).")
  endif()

  # Process pairs
  math(EXPR _gp_bulk_max "${_gp_bulk_count} - 1")
  if(_gp_bulk_count GREATER 0)
    foreach(_idx RANGE 0 ${_gp_bulk_max} 2)
      math(EXPR _val_idx "${_idx} + 1")

      # Extract key and value
      list(GET _gp_bulk_args ${_idx} _gp_key)
      list(GET _gp_bulk_args ${_val_idx} _gp_value)

      # Delegate to your existing single-setter macro
      gpSetScoped("${_gp_key}" "${_gp_value}")
    endforeach()
  endif()

  # Clean up temporary macro variables so we don't pollute the caller's scope
  unset(_gp_bulk_args)
  unset(_gp_bulk_count)
  unset(_gp_bulk_remainder)
  unset(_gp_bulk_max)
  unset(_idx)
  unset(_val_idx)
  unset(_gp_key)
  unset(_gp_value)
endmacro()

# @brief Registers existing variables into the current scope tracker so they are
#        reverted/deleted on gpPopScope(), without modifying their current values.
# @param[in] ARGN A list of variable names. e.g., gpTrackScopedMultiple(varA varB varC)
macro(gpTrackScopedMultiple)
  if(__GP_SCOPE_DEPTH EQUAL 0)
    message(FATAL_ERROR "[GPBT] gpTrackScopedMultiple called outside of any scope.")
  endif()

  foreach(_varName IN ITEMS ${ARGN})
    if(NOT "${_varName}" IN_LIST __gp_scope_vars_${__GP_SCOPE_DEPTH})
      list(APPEND __gp_scope_vars_${__GP_SCOPE_DEPTH} "${_varName}")

      if(DEFINED ${_varName})
        set(__gp_scope_backup_${__GP_SCOPE_DEPTH}_${_varName} "${${_varName}}")
        set(__gp_scope_had_${__GP_SCOPE_DEPTH}_${_varName} TRUE)
      else()
        set(__gp_scope_had_${__GP_SCOPE_DEPTH}_${_varName} FALSE)
      endif()
    endif()
  endforeach()
endmacro()

# @brief Guard to ensure a block of code executes only once per CMake configuration run.
# @param[in] identifier A unique name for the code block (e.g., "STARTUP_BANNER").
# @param[out] outVar Evaluates to TRUE on the first call, and FALSE on all subsequent calls.
macro(gpExecuteOnce identifier outVar)
  get_property(_has_run GLOBAL PROPERTY "__GP_ONCE_${identifier}")

  if(NOT _has_run)
    # Mark it as run so future checks return false
    set_property(GLOBAL PROPERTY "__GP_ONCE_${identifier}" TRUE)
    set(${outVar} TRUE)
  else()
    set(${outVar} FALSE)
  endif()
endmacro()

# @brief Checks whether a specific named scope is currently active anywhere in the stack.
# @param[in] scopeName The name of the scope to check.
# @param[out] outVar Evaluates to TRUE if inside the scope, FALSE otherwise.
macro(gpIsInNamedScope scopeName outVar)
  list(FIND __GP_ACTIVE_NAMED_SCOPES "${scopeName}" _gp_name_idx)
  if(NOT _gp_name_idx EQUAL -1)
    set(${outVar} TRUE)
  else()
    set(${outVar} FALSE)
  endif()
endmacro()
