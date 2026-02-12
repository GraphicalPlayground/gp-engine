// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Container/BasicStringView.hpp"
#include "Container/ContainerForward.hpp"
#include "ShadersBuild.hpp"
#include <compare>

namespace GP::Shaders
{

/// \brief 128-bit hash uniquely identifying shader source + defines + target. Used as the primary key for all shader
/// caches and variant lookups. Why 128-bit: Collision probability < 1e-18 even with millions of shaders, while being
/// small enough to use as inline map keys.
struct FShaderHash
{
public:
    UInt64 low = 0;    //<! Lower 64 bits of the hash
    UInt64 high = 0;   //<! Upper 64 bits of the hash

public:
    /// \brief Equality operator for FShaderHash to allow direct comparison of hash values.
    /// \param other Another FShaderHash to compare with.
    /// \return True if both low and high parts are equal, false otherwise.
    GP_NODISCARD bool operator==(const FShaderHash&) const noexcept = default;

    /// \brief Three-way comparison operator for FShaderHash to enable ordering and comparisons.
    /// \param other Another FShaderHash to compare with.
    /// \return std::strong_ordering indicating the relative order of the two hashes.
    GP_NODISCARD auto operator<=>(const FShaderHash&) const noexcept = default;

public:
    /// \brief Check if the shader hash is valid (non-zero).
    /// \return True if either low or high part of the hash is non-zero, false if both are zero.
    GP_NODISCARD constexpr bool IsValid() const noexcept { return low != 0 || high != 0; }

public:
    /// \brief Static factory method to create a shader hash from raw data.
    /// \param data Pointer to the input data bytes.
    /// \param length Number of bytes to process for hashing.
    /// \return A new FShaderHash computed from the input data.
    GP_NODISCARD static FShaderHash FromData(const void* data, SizeT length);

    /// \brief Static factory method to create a shader hash from a string view.
    /// \param str String view containing the data to hash.
    /// \return A new FShaderHash computed from the input string view.
    GP_NODISCARD static FShaderHash FromString(FStringView str);

    /// \brief Combine two shader hashes into a single hash, useful for creating a hash for a shader variant based on
    /// its source and defines.
    /// \param a The first FShaderHash to combine.
    /// \param b The second FShaderHash to combine.
    /// \return A new FShaderHash that is the combination of the two input hashes.
    GP_NODISCARD static FShaderHash Combine(const FShaderHash& a, const FShaderHash& b);
};

}   // namespace GP::Shaders

/// \brief Specialization of std::hash for FShaderHash to allow usage in hash-based containers like std::unordered_map.
template <>
struct std::hash<GP::Shaders::FShaderHash>
{
    /// \brief Compute a hash value for an FShaderHash instance by combining its low and high parts.
    /// This allows FShaderHash to be used as a key in unordered containers.
    /// \param shaderHash The FShaderHash instance to compute the hash for.
    /// \return A size_t hash value derived from the low and high parts of the FShaderHash.
    size_t operator()(const GP::Shaders::FShaderHash& shaderHash) const noexcept
    {
        return static_cast<size_t>(shaderHash.low ^ (shaderHash.high * 0x9e3779b97f4a7c15ULL));
    }
};
