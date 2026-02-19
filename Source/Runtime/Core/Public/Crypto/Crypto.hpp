// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

// Forward declarations and shared types
#include "CryptoForward.hpp"

// Shared internal utilities
#include "Hash/HashUtilities.hpp"

// Non-cryptographic hash algorithms
#include "Hash/CRC32.hpp"
#include "Hash/CityHash.hpp"
#include "Hash/FNV1a.hpp"
#include "Hash/FarmHash.hpp"
#include "Hash/HashCombine.hpp"
#include "Hash/MeowHash.hpp"
#include "Hash/Murmur3.hpp"
#include "Hash/NoiseHash.hpp"
#include "Hash/SipHash.hpp"
#include "Hash/SpookyHash.hpp"
#include "Hash/WyHash.hpp"
#include "Hash/xxHash.hpp"

// Cryptographic hash algorithms
#include "Hash/BLAKE2b.hpp"
#include "Hash/BLAKE3.hpp"
#include "Hash/MD5.hpp"
#include "Hash/SHA1.hpp"
#include "Hash/SHA256.hpp"
#include "Hash/SHA384.hpp"
#include "Hash/SHA512.hpp"
