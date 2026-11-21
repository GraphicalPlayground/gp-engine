// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"

namespace gp::thread
{

enum class JobSyncResult : gp::UInt8;
enum class JobWaitMode : gp::UInt8;
enum class JobWaitNodeState : gp::UInt8;

struct JobWaitNode;
struct JobFiberParker;
class ScopedJobFiberParker;

class JobCounter;
struct CacheAlignedJobCounter;
class JobFence;

}   // namespace gp::thread
