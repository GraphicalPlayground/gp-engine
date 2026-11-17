// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"

namespace gp::thread
{

enum class NamedThread : gp::UInt8;
enum class ThreadRole : gp::UInt8;
enum class ThreadRoleResult : gp::UInt8;

struct NamedThreadTask;
struct NamedThreadDispatcherDesc;

class ThreadRoleRegistry;
class ScopedThreadRole;
class ScopedNamedThread;
class NamedThreadDispatcher;

}   // namespace gp::thread
