// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"

namespace GP::Events
{

class FDelegateHandle;
class FScopedDelegateHandle;

template <typename Signature>
class TDelegate;

template <typename Signature>
class TMulticastDelegate;

}   // namespace GP::Events

namespace GP
{

/// \brief Primary template for event traits. Specialize this template for each event type to define associated traits
/// such as the delegate signature, handle types, and other event-specific configurations.
/// \tparam ObjectType The type of the object that owns or is associated with the event. This is typically a class
/// type for member events, but can be void or any other type for free-function or static events.
template <typename ObjectType>
struct FEvents
{};

}   // namespace GP
