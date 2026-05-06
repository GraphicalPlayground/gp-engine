// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"

namespace gp::memory
{

/// @brief Memory counter regions used for tracking and categorizing memory usage in the engine.
enum class MemoryCounterRegion
{
    Invalid,                  //<! Not a valid region, used for error handling and debugging.
    Physical,                 //<! Main system memory (RAM) accessible by the CPU.
    GraphicsCard,             //<! Memory on the graphics card (VRAM) used for rendering and GPU computations.
    GraphicsCardSystem,       //<! System memory directly used by the graphics card.
    TexturePool,              //<! Presized texture pool for efficient texture allocations.
    StreamingPool,            //<! Amount of texture pool available for streaming textures.
    UsedStreamingPool,        //<! Amount of texture pool currently used for streaming textures.
    GraphicsCardDefragPool,   //<! Presized pool of memory than can be defragmented.
    PhysicalLLM,              //<! Physical memory tracked by Low-Level Memory Tracker (LLM).
    COUNT
};

/// @brief Memory allocator types used for selecting and configuring different memory allocation strategies.
enum class MemoryAllocatorType
{
    Ansi,       //<! Default C allocator.
    Stomp,      //<! Allocator that stomps on freed memory to help detect use-after-free errors.
    TBB,        //<! Intel Thread Building Blocks allocator for scalable multithreaded applications.
    JeMalloc,   //<! Linux/FreeBSD allocator known for good performance and low fragmentation.
    Binned,     //<! Binned allocator that categorizes allocations into bins based.
    Binned64,   //<! Latest version of the VM-based Binned allocator (64-bit only).
    Platform,   //<! Custom platform specific allocator optimized for the target platform.
    MiMalloc,   //<! Cross-platform allocator designed for high performance and low fragmentation.
    LibPAS,     //<! High-performance Parallel Allocator System (LibPAS) for multithreaded applications.
    COUNT
};

/// @brief Shared memory access types used for specifying the access permissions when creating or mapping shared
///        memory regions.
enum class SharedMemoryAccess : gp::UInt8
{
    Read = (1 << 1),
    Write = (1 << 2)
};

}   // namespace gp::memory
