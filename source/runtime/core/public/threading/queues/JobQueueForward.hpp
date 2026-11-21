// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Concepts.hpp"
#include "CoreMinimal.hpp"

namespace gp::thread
{

/// @brief Concept constraining element types eligible for lock-free work-stealing and MPMC job queues ([ADR-0051]).
/// @details Requires trivial copyability and absence of `const` qualification so speculative thief reads in
///          `WorkStealingDeque::tryStealTop()` and slot copies in `MPMCRingQueue` never invoke non-trivial copy
///          constructors or mutate `const` storage.
template <typename InElementType>
concept JobQueueElement = gp::concepts::IsTriviallyCopyable<InElementType> && !gp::concepts::IsConst<InElementType>;

enum class JobQueueResult : gp::UInt8;
enum class JobPriority : gp::UInt8;
enum class JobQueueItemKind : gp::UInt8;

struct JobDecl;
struct JobQueueItem;

template <JobQueueElement InElementType>
class WorkStealingDeque;

template <JobQueueElement InElementType>
class MPMCRingQueue;

}   // namespace gp::thread
