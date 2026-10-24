// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "memory/MemoryOrder.hpp"   // IWYU pragma: export
#include "threading/atomics/Atomic.hpp"
#include "threading/atomics/AtomicFlag.hpp"
#include "threading/atomics/CacheAlignedAtomic.hpp"
#include "threading/Fences.hpp"

namespace gp
{

using thread::Atomic;
using thread::AtomicFlag;
using thread::AtomicValue;
using thread::CacheAlignedAtomic;

using thread::atomicSignalFence;
using thread::atomicThreadFence;
using thread::compilerBarrier;

}   // namespace gp
