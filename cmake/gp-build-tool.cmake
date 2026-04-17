option(GP_BUILD_TOOL_VERBOSE "Enable verbose output for the GP Build Tool" OFF)

include(gp-build-tool/gp-target)
include(gp-build-tool/gp-dependencies)
include(gp-build-tool/gp-includes)
include(gp-build-tool/gp-definitions)
include(gp-build-tool/gp-thirdparty)
include(gp-build-tool/gp-flags)

# TODO:
#  - Add support for custom build steps (e.g., code generation, asset processing)
#  - Add different type of dependencies: static, shared, interface
#  - Add support for custom build configurations (e.g., Debug, Release, etc.)
#  - Add support for custom build targets (e.g., tests, benchmarks, etc.)
#  - Add support for custom build tools (e.g., code formatters, linters, etc.)
#  - Write documentation for the GP Build Tool, including usage examples and best practices
