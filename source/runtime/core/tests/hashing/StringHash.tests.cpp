// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "hashing/StringHash.hpp"
#include "containers/strings/String.hpp"     // IWYU pragma: keep
#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "hashing/HashOf.hpp"
#include <gtest/gtest.h>

namespace gp::hashing::tests
{

using namespace gp::literals;

TEST(StringHashTest, CompileTimeLiteralsAndSwitchDispatch)
{
    static_assert(sizeof(StringHash32) == 4u);
    static_assert(sizeof(StringHash64) == 8u);

    static_assert(StringHash64{}.isEmpty());
    static_assert(!StringHash64{}.isValid());
    static_assert(!static_cast<bool>(StringHash64{}));
    static_assert(StringHash64("").isEmpty());

    static_assert("Position"_fnv32 == StringHash32("Position"));
    static_assert("Position"_fnv32 != "position"_fnv32);
    static_assert("Position"_fnv32i == "POSITION"_fnv32i);

    static_assert("Roughness"_fnv64 == StringHash64("Roughness"));
    static_assert("Roughness"_fnv64 != "roughness"_fnv64);
    static_assert("Roughness"_fnv64i == "ROUGHNESS"_fnv64i);

    static_assert("123456789"_crc32 == 0xCBF43926u);
    static_assert("123456789"_crc32c == 0xE3069283u);
    static_assert("Shader"_crc32i == "SHADER"_crc32i);
    static_assert("Entity"_murmur32 == Murmur3_32::hash("Entity"));
    static_assert("Packet"_xxh32 == XxHash32::hash("Packet"));
    static_assert("ShaderCache"_xxh64 == XxHash64::hash("ShaderCache"));

    const gp::String runtimeStr("Roughness");
    const StringHash64 runtimeHash(runtimeStr);
    EXPECT_TRUE(runtimeHash.isValid());
    EXPECT_TRUE(static_cast<bool>(runtimeHash));
    EXPECT_EQ(hashOf(runtimeHash), runtimeHash.value());

    gp::Int32 matched = 0;
    switch (runtimeHash)
    {
    case "Albedo"_fnv64:
        matched = 1;
        break;
    case "Roughness"_fnv64:
        matched = 2;
        break;
    default:
        matched = -1;
        break;
    }
    EXPECT_EQ(matched, 2);

    const StringHash64 fromRaw = StringHash64::fromRaw(runtimeHash.value());
    EXPECT_EQ(fromRaw, runtimeHash);
    EXPECT_EQ(fromRaw <=> runtimeHash, std::strong_ordering::equal);
}

TEST(StringHashTest, HashedStringViewCachesHashAndChecksCollisions)
{
    constexpr HashedStringView64 kStatic = "AlbedoMap"_hs;
    constexpr HashedStringView64IgnoreCase kStaticInsensitive = "AlbedoMap"_hsi;

    static_assert(!kStatic.isEmpty());
    static_assert(kStatic.size() == 9u);
    static_assert(kStatic.hash() == Fnv1a64::hash("AlbedoMap"));
    static_assert(kStatic == HashedStringView64("AlbedoMap"));
    static_assert(kStatic != HashedStringView64("albedomap"));
    static_assert(kStaticInsensitive == HashedStringView64IgnoreCase("ALBEDOMAP"));

    EXPECT_EQ(kStatic.view(), "AlbedoMap");
    EXPECT_STREQ(kStatic.data(), "AlbedoMap");
    EXPECT_EQ(kStatic.stringHash(), "AlbedoMap"_fnv64);
    EXPECT_LT(HashedStringView64("Alpha"), HashedStringView64("Beta"));
    EXPECT_EQ(
        HashedStringView64IgnoreCase("alpha") <=> HashedStringView64IgnoreCase("ALPHA"), std::strong_ordering::equal
    );

    // Simulated hash collision: same precomputed hash, different text must compare unequal!
    const HashedStringView64 fakeA(0x123456789ABCDEF0ULL, "MeshA");
    const HashedStringView64 fakeB(0x123456789ABCDEF0ULL, "MeshB");
    EXPECT_NE(fakeA, fakeB);

    const HashedStringView64 emptyView{};
    EXPECT_TRUE(emptyView.isEmpty());
    EXPECT_EQ(emptyView.hash(), 0ULL);
    EXPECT_NE(hashOf<Crc32>(kStatic), hashOf<Crc32>(emptyView));
}

}   // namespace gp::hashing::tests
