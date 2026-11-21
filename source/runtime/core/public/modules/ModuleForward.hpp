// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"

namespace gp::modules
{

enum class ModuleLoadResult : gp::UInt8;
enum class ModuleUnloadResult : gp::UInt8;
enum class ModuleState : gp::UInt8;
enum class ModuleFlags : gp::UInt8;
enum class ModuleChangeReason : gp::UInt8;
enum class ModuleShutdownPhase : gp::UInt8;

struct ModuleInitContext;
struct ModuleInitHandshake;
struct ModuleDesc;
struct ModuleStatusView;
struct StaticModuleRegistrantNode;

class IModule;
class DefaultModuleImpl;
class ModuleManager;

}   // namespace gp::modules

namespace gp
{

using modules::DefaultModuleImpl;
using modules::IModule;
using modules::ModuleChangeReason;
using modules::ModuleDesc;
using modules::ModuleFlags;
using modules::ModuleInitContext;
using modules::ModuleInitHandshake;
using modules::ModuleLoadResult;
using modules::ModuleManager;
using modules::ModuleShutdownPhase;
using modules::ModuleState;
using modules::ModuleStatusView;
using modules::ModuleUnloadResult;

}   // namespace gp
