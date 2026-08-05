#!/bin/bash
# Copyright (c) - Graphical Playground. All rights reserved.
# For more information, see https://graphical-playground/legal
# mailto:support AT graphical-playground DOT com

if [ -z "$SKIP_OPTIONAL" ]; then
  SKIP_OPTIONAL=1
fi

# Compares two version numbers. Returns 0 (true) if $1 >= $2
version_ge() {
  # Uses sort -V (version sort) which is standard in GNU coreutils
  printf '%s\n%s\n' "$2" "$1" | sort -C -V
}

# Extracts the version number from a command's output
get_version() {
  local cmd="$1"
  # Runs the command, captures stderr and stdout, and extracts the first semantic version string
  "$cmd" --version 2>&1 | grep -oE '[0-9]+\.[0-9]+(\.[0-9]+)?' | head -n 1
}

# Checks if a command exists and optionally checks its version
check_command() {
  local cmd="$1"
  local min_version="$2"
  local is_optional="$3"

  printf "Checking for %-20s " "$cmd"

  # Check if command exists
  if ! command -v "$cmd" >/dev/null 2>&1; then
    if [ "$is_optional" == "true" ] && [ "$SKIP_OPTIONAL" -eq 1 ]; then
      printf "\033[33mMISSING (Skipped)\033[0m\n"
      return 0
    else
      printf "\033[31mMISSING\033[0m\n"
      return 1
    fi
  fi

  # Check version if a minimum is specified
  if [ -n "$min_version" ]; then
    local current_version
    current_version=$(get_version "$cmd")

    if [ -z "$current_version" ]; then
      printf "\033[33mFOUND (could not determine version)\033[0m\n"
      return 0
    fi

    if version_ge "$current_version" "$min_version"; then
      printf "\033[32mFOUND (v%s >= v%s)\033[0m\n" "$current_version" "$min_version"
    else
      printf "\033[31mOUTDATED (v%s < v%s)\033[0m\n" "$current_version" "$min_version"
      # Allow failure to pass if it's optional and the flag is set
      if [ "$is_optional" == "true" ] && [ "$SKIP_OPTIONAL" -eq 1 ]; then
        return 0
      fi
      return 1
    fi
  else
    printf "\033[32mFOUND\033[0m\n"
  fi
  return 0
}

# Checks if a C++ library is installed by attempting to compile a test file
check_cxx_lib() {
  local lib_desc="$1"
  local compiler="$2"
  local flags="$3"
  local is_optional="$4"

  printf "Checking for %-20s " "$lib_desc"

  if ! command -v "$compiler" >/dev/null 2>&1; then
    printf "\033[31mCOMPILER NOT FOUND\033[0m\n"
    return 1
  fi

  # Create a temporary C++ file
  local tmpfile
  tmpfile=$(mktemp --suffix=.cpp)
  echo "#include <iostream>" > "$tmpfile"
  echo "int main() { std::cout << \"test\"; return 0; }" >> "$tmpfile"

    # Attempt to compile with the specific library flags
    if $compiler $flags "$tmpfile" -o /dev/null >/dev/null 2>&1; then
        printf "\033[32mFOUND & USABLE\033[0m\n"
        rm -f "$tmpfile"
        return 0
    else
        if [ "$is_optional" == "true" ] && [ "$SKIP_OPTIONAL" -eq 1 ]; then
            printf "\033[33mMISSING (Skipped)\033[0m\n"
            rm -f "$tmpfile"
            return 0
        else
            printf "\033[31mMISSING (Failed to compile)\033[0m\n"
            rm -f "$tmpfile"
            return 1
        fi
    fi
}

# Checks if a system library is installed via pkg-config
check_pkg() {
  local pkg="$1"
  local is_optional="$2"

  printf "Checking for %-20s " "pkg:$pkg"

  if ! command -v pkg-config >/dev/null 2>&1; then
    printf "\033[31mpkg-config NOT FOUND\033[0m\n"
    return 1
  fi

  if pkg-config --exists "$pkg"; then
    local version
    version=$(pkg-config --modversion "$pkg")
    printf "\033[32mFOUND (v%s)\033[0m\n" "$version"
    return 0
  else
    if [ "$is_optional" == "true" ] && [ "$SKIP_OPTIONAL" -eq 1 ]; then
      printf "\033[33mMISSING (Skipped)\033[0m\n"
      return 0
    else
      printf "\033[31mMISSING\033[0m\n"
      return 1
    fi
  fi
}
