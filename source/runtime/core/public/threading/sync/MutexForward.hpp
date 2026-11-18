// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"

namespace gp::thread
{

enum class OnceState : gp::UInt32;

struct DeferLockT;
struct TryToLockT;
struct AdoptLockT;

class Mutex;
struct CacheAlignedMutex;

class RecursiveMutex;
using CriticalSection = RecursiveMutex;
struct CacheAlignedRecursiveMutex;
using CacheAlignedCriticalSection = CacheAlignedRecursiveMutex;

class SharedMutex;
struct CacheAlignedSharedMutex;

template <typename InMutexType = Mutex>
class UniqueLock;

template <typename InMutexType = Mutex>
using ScopedLock = UniqueLock<InMutexType>;

template <typename InSharedMutexType = SharedMutex>
class SharedLock;

class OnceFlag;

}   // namespace gp::thread

namespace gp
{

using thread::AdoptLockT;
using thread::CacheAlignedCriticalSection;
using thread::CacheAlignedMutex;
using thread::CacheAlignedRecursiveMutex;
using thread::CacheAlignedSharedMutex;
using thread::CriticalSection;
using thread::DeferLockT;
using thread::Mutex;
using thread::OnceFlag;
using thread::OnceState;
using thread::RecursiveMutex;
using thread::ScopedLock;
using thread::SharedLock;
using thread::SharedMutex;
using thread::TryToLockT;
using thread::UniqueLock;

}   // namespace gp
