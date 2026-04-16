# Copyright (c) - Graphical Playground. All rights reserved.

# TODO: Add automatic discovery of modules
add_subdirectory(hal)
add_subdirectory(core)

# Remove this and replace with automatic discovery of modules
gpSortTargets(MY_ORDERED_MODULES)
message(STATUS "[GPBT] Correct compilation order: ${MY_ORDERED_MODULES}")
