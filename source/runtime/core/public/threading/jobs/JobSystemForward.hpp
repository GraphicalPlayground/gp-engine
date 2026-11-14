// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"

namespace gp::thread
{

enum class JobSystemResult : gp::UInt8;
enum class JobSplitStrategy : gp::UInt8;
enum class JobFlags : gp::UInt16;

struct JobContext;
struct JobSystemDesc;
struct ParallelForDesc;
struct JobSystemStats;
struct JobGraphNode;

class JobGraph;
class ParallelFor;
class JobSystem;

}   // namespace gp::thread
