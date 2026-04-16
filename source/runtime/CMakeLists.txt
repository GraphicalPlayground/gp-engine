# Copyright (c) - Graphical Playground. All rights reserved.

# Automatically find all subdirectories with a CMakeLists.txt and add them as modules
file(GLOB GP_RUNTIME_MODULES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} */CMakeLists.txt)
list(TRANSFORM GP_RUNTIME_MODULES REPLACE "/CMakeLists.txt$" "")

message(STATUS "[GP] Configuring Runtime modules...")

set(GP_MODULE_BUILD_PHASE OFF)

# First pass: include only for registration
foreach(Module IN LISTS GP_RUNTIME_MODULES)
    include(${Module}/CMakeLists.txt OPTIONAL)
endforeach()

# Topological sort
set(GP_SORTED_MODULES)
set(GP_TEMP_MARK)
set(GP_PERM_MARK)

function(gp_visit_module MODULE)
    if(MODULE IN_LIST GP_PERM_MARK)
        return()
    endif()

    if(MODULE IN_LIST GP_TEMP_MARK)
        message(FATAL_ERROR "Circular dependency detected at module ${MODULE}")
    endif()

    list(APPEND GP_TEMP_MARK ${MODULE})
    set(GP_TEMP_MARK ${GP_TEMP_MARK} PARENT_SCOPE)

    set(DEPS ${GP_MODULE_DEPS_${MODULE}})

    foreach(DEP ${DEPS})
        gp_visit_module(${DEP})
    endforeach()

    list(APPEND GP_PERM_MARK ${MODULE})
    list(APPEND GP_SORTED_MODULES ${MODULE})

    set(GP_PERM_MARK ${GP_PERM_MARK} PARENT_SCOPE)
    set(GP_SORTED_MODULES ${GP_SORTED_MODULES} PARENT_SCOPE)
endfunction()

foreach(Module IN LISTS GP_REGISTERED_MODULES)
    gp_visit_module(${Module})
endforeach()

message(STATUS "[GP] Runtime module build order: ${GP_SORTED_MODULES}")

set(GP_MODULE_BUILD_PHASE ON)

# Now actually add them in correct order
foreach(Module IN LISTS GP_SORTED_MODULES)
    add_subdirectory(${Module})
endforeach()
