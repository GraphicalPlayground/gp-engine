// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#include "threading/Fences.hpp"
#include "memory/MemoryOrder.hpp"
#include <gtest/gtest.h>

namespace gp::thread::tests
{

constexpr bool testConstexprFences()
{
    compilerBarrier();
    atomicSignalFence(MemoryOrder::SeqCst);
    atomicThreadFence(MemoryOrder::AcqRel);
    return true;
}

static_assert(testConstexprFences(), "Constexpr fences evaluation failed.");

TEST(FencesTest, HardwareAndCompilerFences)
{
    compilerBarrier();

    atomicSignalFence(MemoryOrder::Relaxed);
    atomicSignalFence(MemoryOrder::Acquire);
    atomicSignalFence(MemoryOrder::Release);
    atomicSignalFence(MemoryOrder::AcqRel);
    atomicSignalFence(MemoryOrder::SeqCst);

    atomicThreadFence(MemoryOrder::Relaxed);
    atomicThreadFence(MemoryOrder::Consume);
    atomicThreadFence(MemoryOrder::Acquire);
    atomicThreadFence(MemoryOrder::Release);
    atomicThreadFence(MemoryOrder::AcqRel);
    atomicThreadFence(MemoryOrder::SeqCst);
}

}   // namespace gp::thread::tests
