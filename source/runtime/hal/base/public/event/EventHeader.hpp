// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "event/EventCategory.hpp"

namespace gp::hal::event
{

/// @brief Represents the header of an event, containing its category and payload size.
struct EventHeader
{
    EventCategory category;
    gp::UInt16 payloadSize;
};

}   // namespace gp::hal::event
