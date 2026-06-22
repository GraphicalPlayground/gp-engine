#!/bin/bash

set -e

echo "=========================================="
echo "  Setting up Graphical Playground Env     "
echo "=========================================="

if [ -f /etc/os-release ]; then
  . /etc/os-release
  OS=$NAME
  VER=$VERSION_ID
else
  echo "Unsupported OS. This script requires Ubuntu/Debian."
  exit 1
fi

if [[ "$OS" != *"Ubuntu"* && "$OS" != *"Debian"* ]]; then
  echo "  Warning: This script is optimized for Ubuntu. Detected: $OS"
fi

echo "Detected OS: $OS $VER"

sudo apt-get update

CORE_DEPS=(
  "build-essential"
  "cmake"
  "lld"
  "libc++-dev"
  "libc++abi-dev"
  "ninja-build"
  "git"
  "pkg-config"
  "gdb"
  "clang-format-22"
  "clangd-22"
  "clang-tidy-22"
  "ccache"
  "libjemalloc-dev"
)

TRACY_DEPS=(
  "libcapstone-dev"
  "libdbus-1-dev"
  "libfreetype6-dev"
  "libglfw3-dev"
)

VULKAN_DEPS=(
  "libvulkan-dev"
)

SDL3_DEPS=(
  "libasound2-dev"
  "libpulse-dev"
  "libaudio-dev"
  "libx11-dev"
  "libxext-dev"
  "libxrandr-dev"
  "libxcursor-dev"
  "libxfixes-dev"
  "libxi-dev"
  "libxss-dev"
  "libxkbcommon-dev"
  "libdrm-dev"
  "libgbm-dev"
  "libgl1-mesa-dev"
)

if awk "BEGIN {exit !($VER >= 22.04)}"; then
  echo "Detected Ubuntu 22.04+. Adding Wayland and Pipewire dependencies..."
  SDL3_DEPS+=(
    "libpipewire-0.3-dev"
    "libwayland-dev"
    "libdecor-0-dev"
    "liburing-dev"
  )
fi

ALL_DEPS=("${CORE_DEPS[@]}" "${VULKAN_DEPS[@]}" "${TRACY_DEPS[@]}" "${SDL3_DEPS[@]}")

echo "Installing dependencies..."
sudo apt-get install -y "${ALL_DEPS[@]}"

echo "=========================================="
echo "       Checking environment setup...      "
echo "=========================================="

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source ${SCRIPT_DIR}/common.sh

echo -e "\n--- Required Dependencies ---"
check_command "ninja" "1.10" "false" || FAILURES=$((FAILURES+1))
check_command "ld.lld" "" "false" || FAILURES=$((FAILURES+1))
check_command "clang" "20" "false" || FAILURES=$((FAILURES+1))
check_command "clang++" "20" "false" || FAILURES=$((FAILURES+1))
check_command "ccache" "" "false" || FAILURES=$((FAILURES+1))

echo -e "\n--- Libraries ---"
check_cxx_lib "libc++(abi)" "clang++" "-stdlib=libc++" "false" || FAILURES=$((FAILURES+1))

echo -e "\n--- Optional Dependencies ---"
check_command "clang-format" "20" "true" || FAILURES=$((FAILURES+1))
check_command "clangd" "20" "true" || FAILURES=$((FAILURES+1))
check_command "clang-tidy" "20" "true" || FAILURES=$((FAILURES+1))
check_command "doxygen" "" "true" || FAILURES=$((FAILURES+1))
check_command "graphviz" "" "true" || FAILURES=$((FAILURES+1))

echo "=========================================="
echo "      Environment setup complete!         "
echo "=========================================="
