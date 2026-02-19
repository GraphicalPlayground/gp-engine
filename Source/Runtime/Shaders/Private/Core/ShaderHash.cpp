// Copyright (c) - Graphical Playground. All rights reserved.

#include "Core/ShaderHash.hpp"

namespace GP::Shaders
{

GP_NODISCARD FShaderHash FShaderHash::FromData(const void* data, SizeT length)
{
    // TODO: Implement a proper hash function (e.g., FNV-1a, MurmurHash3, or CityHash) to compute the low and high parts
    // of the hash from the input data. For now, this is a placeholder that simply returns an empty hash.
    GP_UNUSED(data);
    GP_UNUSED(length);
    return FShaderHash{};
}

GP_NODISCARD FShaderHash FShaderHash::FromData(Container::TSpan<Byte> data)
{
    return FromData(data.Data(), data.Size());
}

GP_NODISCARD FShaderHash FShaderHash::FromString(FStringView str) { return FromData(str.Data(), str.Size()); }

GP_NODISCARD FShaderHash FShaderHash::Combine(const FShaderHash& a, const FShaderHash& b)
{
    // TODO: Implement a proper hash combination function (e.g., using XOR and bit shifts) to combine the low and high
    // parts of both hashes. For now, this is a placeholder that simply returns an empty hash.
    GP_UNUSED(a);
    GP_UNUSED(b);
    return FShaderHash{};
}

}   // namespace GP::Shaders
