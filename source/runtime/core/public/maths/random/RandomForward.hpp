// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep

namespace gp::math
{

/// @section Enumerations

enum class RandomAlgorithm : gp::UInt8;

/// @section Serializable Engine State & Counter Descriptors

struct SplitMix64State;
struct Pcg32State;
struct Xoshiro128State;
struct Xoshiro256State;
struct Sfc64State;
struct Philox4x32Key;
struct Philox4x32Counter;

/// @section Algorithm Traits & Unified Engine Base

template <RandomAlgorithm InAlgorithm>
struct RandomAlgorithmTraits;

template <typename InDerived, typename InWordType, typename InStateType, RandomAlgorithm InAlgorithm>
class RandomEngineBase;

/// @section Concrete PRNG Engines

class SplitMix64;
class Pcg32;
class Xoshiro128PlusPlus;
class Xoshiro256PlusPlus;
class Sfc64;
class Philox4x32;

/// @section Ergonomic Random Streams

template <typename InDerived>
class RandomStreamOperations;

template <typename InEngine>
class BasicRandomStream;

using RandomStream = BasicRandomStream<Xoshiro256PlusPlus>;
using RandomStream32 = BasicRandomStream<Pcg32>;
using CompactRandomStream = BasicRandomStream<Xoshiro128PlusPlus>;
using FastRandomStream = BasicRandomStream<Sfc64>;

class DynamicRandomStream;

}   // namespace gp::math
