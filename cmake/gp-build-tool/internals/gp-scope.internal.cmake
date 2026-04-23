# Copyright (c) - Graphical Playground. All rights reserved.
# For more information, see https://graphical-playground/legal
# mailto:support AT graphical-playground DOT com

# Verify that scope depth variable is initialized
if(NOT DEFINED __GP_SCOPE_DEPTH)
  set(__GP_SCOPE_DEPTH 0)
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
    message(FATAL_ERROR "[GPBT] gpPopScope called without a matching gpPushScope().")
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

# @brief Sets a variable in the current scope, tracking it so that it can be reverted by gpPopScope().
#        Must be called after gpPushScope() to ensure the variable is tracked properly.
# @param[in] varName The name of the variable to set.
# @param[in] ARGN The value(s) to set the variable to. Supports lists and multiple values.
macro(gpSetScoped varName)
  # Ensure we are inside a scope
  if(__GP_SCOPE_DEPTH EQUAL 0)
    message(FATAL_ERROR "[GPBT] gpSetScoped called outside of any scope. Call gpPushScope() first.")
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
