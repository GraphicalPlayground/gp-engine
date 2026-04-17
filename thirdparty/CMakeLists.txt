# Copyright (c) - Graphical Playground. All rights reserved.
# For more information, see https://graphical-playground/legal
# mailto:support AT graphical-playground DOT com

# Cross-platform dependencies
add_subdirectory(catch2)
add_subdirectory(sdl3)

if(WIN32) # Windows specific
elseif (UNIX AND NOT APPLE) # Linux specific
elseif(APPLE) # macOS specific
endif()
