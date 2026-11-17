// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep

namespace gp::hashing
{

/// @section Enumerations

enum class HashAlgorithm : gp::UInt8;
enum class HashCase : gp::UInt8;
enum class HashWidth : gp::UInt8;

/// @section Digest & Result Types

struct Hash128;
struct HashDigest;

/// @section Algorithm Traits & Unified Accumulator Base

template <HashAlgorithm InAlgorithm>
struct HashAlgorithmTraits;

template <typename InDerived, typename InDigestType, InDigestType InDefaultSeed, HashAlgorithm InAlgorithm>
class StreamingHasherBase;

/// @section Concrete Hash Algorithms

template <typename InDigestType, InDigestType InOffsetBasis, InDigestType InPrime>
class BasicFnv1a;

using Fnv1a32 = BasicFnv1a<gp::UInt32, 0x811C9DC5u, 0x01000193u>;
using Fnv1a64 = BasicFnv1a<gp::UInt64, 0xCBF29CE484222325ULL, 0x00000100000001B3ULL>;
using FNV1a32 = Fnv1a32;
using FNV1a64 = Fnv1a64;

template <gp::UInt32 InReflectedPolynomial>
class BasicCrc32;

using Crc32 = BasicCrc32<0xEDB88320u>;
using Crc32C = BasicCrc32<0x82F63B78u>;
using CRC32 = Crc32;
using CRC32C = Crc32C;

class Murmur3_32;
class Murmur3_128;

class XxHash32;
class XxHash64;

class DynamicHasher;

/// @section Type Hashing Customization Point

template <typename InT, typename InEnable = void>
struct HashTraits;

}   // namespace gp::hashing
