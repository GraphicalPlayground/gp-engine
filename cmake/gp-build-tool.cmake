option(GP_BUILD_TOOL_VERBOSE "Enable verbose output for the GP Build Tool" OFF)

include(gp-build-tool/gp-target)
include(gp-build-tool/gp-dependencies)
include(gp-build-tool/gp-includes)
include(gp-build-tool/gp-definitions)

# TODO:
#  - Add support for custom build steps (e.g., code generation, asset processing)
#  - Add support for adding definitons, includes, and dependencies to specific targets
#  - Add different type of dependencies: static, shared, interface
#  - Add support for custom build configurations (e.g., Debug, Release, etc.)
#  - Add support for custom build options (e.g., enabling/disabling features, setting compiler flags, etc.)
#  - Add support for custom build targets (e.g., tests, benchmarks, etc.)
#  - Add support for custom build tools (e.g., code formatters, linters, etc.)
#  - Enhance module ordering to ensure that dependencies are built before the targets that depend on them
#  - Add the custom register/configure systems for targets
#  - Write documentation for the GP Build Tool, including usage examples and best practices