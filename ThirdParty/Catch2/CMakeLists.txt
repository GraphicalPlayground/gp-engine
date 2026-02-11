# Copyright (c) - Mallory SCOTTON. All rights reserved.

include(GPFetchContent)

# Catch2 Build Configuration (Optional)
# Configure Catch2 build options before fetching

option(GP_CATCH2_BUILD_TESTING "Build Catch2 self-tests" OFF)
option(GP_CATCH2_BUILD_EXAMPLES "Build Catch2 examples" OFF)
option(GP_CATCH2_BUILD_EXTRA_TESTS "Build Catch2 extra tests" OFF)
option(GP_CATCH2_ENABLE_COVERAGE "Enable code coverage for Catch2" OFF)
option(GP_CATCH2_ENABLE_WERROR "Enable all warnings as errors for Catch2" OFF)
option(GP_CATCH2_INSTALL_DOCS "Install Catch2 documentation" OFF)
option(GP_CATCH2_INSTALL_EXTRAS "Install Catch2 extras (CMake scripts, etc.)" ON)

# Apply configuration to Catch2
set(CATCH_BUILD_TESTING ${GP_CATCH2_BUILD_TESTING} CACHE BOOL "" FORCE)
set(CATCH_BUILD_EXAMPLES ${GP_CATCH2_BUILD_EXAMPLES} CACHE BOOL "" FORCE)
set(CATCH_BUILD_EXTRA_TESTS ${GP_CATCH2_BUILD_EXTRA_TESTS} CACHE BOOL "" FORCE)
set(CATCH_ENABLE_COVERAGE ${GP_CATCH2_ENABLE_COVERAGE} CACHE BOOL "" FORCE)
set(CATCH_ENABLE_WERROR ${GP_CATCH2_ENABLE_WERROR} CACHE BOOL "" FORCE)
set(CATCH_INSTALL_DOCS ${GP_CATCH2_INSTALL_DOCS} CACHE BOOL "" FORCE)
set(CATCH_INSTALL_EXTRAS ${GP_CATCH2_INSTALL_EXTRAS} CACHE BOOL "" FORCE)

# Fetch Catch2
gp_fetch_content(
    NAME Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.12.0
    REQUIRED_VERSION 3.12.0
    PACKAGE_NAME Catch2
)

# Catch2 v3 provides these targets:
# - Catch2::Catch2 (the main library with REQUIRE/CHECK macros)
# - Catch2::Catch2WithMain (includes a default main() function)

# Helper function to set folder property on non-alias targets
function(set_target_folder_if_exists target_name folder_name)
    if(TARGET ${target_name})
        get_target_property(target_type ${target_name} TYPE)
        if(NOT target_type STREQUAL "INTERFACE_LIBRARY" AND NOT target_type STREQUAL "ALIAS_LIBRARY")
            set_target_properties(${target_name} PROPERTIES FOLDER "${folder_name}")
            message(STATUS "    Organized ${target_name} into folder: ${folder_name}")
        endif()
    endif()
endfunction()

if(TARGET Catch2::Catch2)
    message(STATUS "  [+] Catch2 targets are available")

    # Organize in IDE folders (only non-alias, non-interface targets)
    set_target_folder_if_exists(Catch2 "ThirdParty/Testing")
    set_target_folder_if_exists(Catch2WithMain "ThirdParty/Testing")

    # Make Catch2's CMake modules available for test registration
    # This allows using catch_discover_tests() in test CMakeLists
    list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
    set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} PARENT_SCOPE)

    message(STATUS "  [+] Catch2 CMake modules added to CMAKE_MODULE_PATH")
    message(STATUS "  [+] Use catch_discover_tests() to auto-discover test cases")
else()
    message(WARNING "  [-] Catch2::Catch2 target not found after fetch")
endif()
