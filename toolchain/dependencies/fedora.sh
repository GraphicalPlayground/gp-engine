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
  echo "Unsupported OS. This script requires Fedora."
  exit 1
fi

if [[ "$OS" != *"Fedora"* ]]; then
  echo "  Warning: This script is optimized for Fedora. Detected: $OS"
fi

echo "Detected OS: $OS $VER"

sudo dnf makecache

CORE_DEPS=(
  "@Development Tools"
  "cmake"
  "ninja-build"
  "git"
  "pkgconf"
  "gdb"
  "jemalloc-devel"
)

LLVM_DEPS=(
  "clang"
  "clang-tools-extra"
  "lld"
  "libcxx-devel"
  "libcxxabi-devel"
  "ccache"
)

TRACY_DEPS=(
  "capstone-devel"
  "dbus-devel"
  "freetype-devel"
  "glfw-devel"
)

VULKAN_DEPS=(
  "vulkan-loader-devel"
  "vulkan-headers"
)

SDL3_DEPS=(
  "alsa-lib-devel"
  "pulseaudio-libs-devel"
  "libX11-devel"
  "libXext-devel"
  "libXrandr-devel"
  "libXcursor-devel"
  "libXfixes-devel"
  "libXi-devel"
  "libXScrnSaver-devel"
  "libxkbcommon-devel"
  "libdrm-devel"
  "mesa-libgbm-devel"
  "mesa-libGL-devel"
  "mesa-libEGL-devel"
)

if awk "BEGIN {exit !($VER >= 34)}"; then
  echo "Detected Fedora 34+. Adding Wayland and Pipewire dependencies..."
  SDL3_DEPS+=(
    "pipewire-devel"
    "wayland-devel"
    "wayland-protocols-devel"
    "libdecor-devel"
    "liburing-devel"
  )
fi

ALL_DEPS=("${CORE_DEPS[@]}" "${LLVM_DEPS[@]}" "${VULKAN_DEPS[@]}" "${TRACY_DEPS[@]}" "${SDL3_DEPS[@]}")

echo "Installing dependencies..."
sudo dnf install -y "${ALL_DEPS[@]}"

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
