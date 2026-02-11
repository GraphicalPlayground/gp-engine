# Copyright (c) - Graphical Playground. All rights reserved.

include(FetchContent)

message(STATUS "  [GP] Resolving dependency: ISPC Compiler")

# Determine the platform and set the appropriate suffix for the ISPC download
if(WIN32)
    set(ISPC_PLATFORM "windows")
    set(ISPC_SUFFIX "zip")
elseif(APPLE)
    set(ISPC_PLATFORM "macOS.universal")
    set(ISPC_SUFFIX "tar.gz")
elseif(UNIX)
    set(ISPC_PLATFORM "linux")
    set(ISPC_SUFFIX "tar.gz")
endif()

set(ISPC_VERSION "1.30.0")

# Fetch the ISPC compiler from the official GitHub releases
FetchContent_Declare(
    ispc
    URL "https://github.com/ispc/ispc/releases/download/v${ISPC_VERSION}/ispc-v${ISPC_VERSION}-${ISPC_PLATFORM}.${ISPC_SUFFIX}"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_MakeAvailable(ispc)

add_executable(ispc_compiler IMPORTED GLOBAL)
set_target_properties(ispc_compiler PROPERTIES
    IMPORTED_LOCATION "${ispc_SOURCE_DIR}/bin/ispc${CMAKE_EXECUTABLE_SUFFIX}"
)

message(STATUS "    [+] ISPC Compiler Resolved")
