# Use the official Microsoft C++ DevContainer image (Ubuntu 22.04)
FROM mcr.microsoft.com/devcontainers/cpp:1-ubuntu-22.04

# Prevent interactive prompts during apt installations
ENV DEBIAN_FRONTEND=noninteractive

# Update system and install the dependencies
RUN apt-get update && apt-get install -y \
    clang \
    llvm \
    ninja-build \
    pkg-config \
    ccache \
    libasound2-dev \
    libpulse-dev \
    libaudio-dev \
    libjack-dev \
    libsndio-dev \
    libx11-dev \
    libxext-dev \
    libxrandr-dev \
    libxcursor-dev \
    libxfixes-dev \
    libxi-dev \
    libxss-dev \
    libxtst-dev \
    libxkbcommon-dev \
    libdrm-dev \
    libgbm-dev \
    libgl1-mesa-dev \
    libgles2-mesa-dev \
    libegl1-mesa-dev \
    libdbus-1-dev \
    libibus-1.0-dev \
    libudev-dev \
    libwayland-dev \
    libvulkan-dev \
    && apt-get clean && rm -rf /var/lib/apt/lists/*

# Add the dev container user to the video and render groups so it has permission to use hardware acceleration
RUN usermod -aG video,render vscode

# Set up Clang as the default compiler for CMake
ENV CC=clang
ENV CXX=clang++
