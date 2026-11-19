// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"

namespace gp::vfs
{

enum class VfsError : gp::UInt8;
enum class MountPriority : gp::UInt16;
enum class MountAccess : gp::UInt8;

struct ParsedUri;
struct MountPointDesc;
struct VfsDirectoryEntry;

class IMountBackend;
class PhysicalMountBackend;
class MemoryMountBackend;
class VirtualFileSystem;

}   // namespace gp::vfs
