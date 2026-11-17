// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "hashing/PathHash.hpp"
#include "containers/strings/String.hpp"
#include "containers/views/StringView.hpp"
#include "CoreMinimal.hpp"
#include "hashing/HashOf.hpp"
#include <gtest/gtest.h>

namespace gp::hashing::tests
{

using namespace gp::literals;

TEST(PathHashTest, LexicalNormalizationAndCompileTimeLiterals)
{
    static_assert(sizeof(PathHash32) == 4u);
    static_assert(sizeof(PathHash64) == 8u);
    static_assert(sizeof(PathHash128) == 16u);
    static_assert(sizeof(PathDigest256) == 32u);

    static_assert(PathHash64{}.isEmpty());
    static_assert(!PathHash64{}.isValid());
    static_assert(!static_cast<bool>(PathHash64{}));
    static_assert(PathHash64("").isEmpty());

    // Separators, redundant slashes, ".", "..", trailing slashes, and ASCII case normalize identically.
    static_assert("Textures\\Hero/../Hero/./Body.dds/"_path64 == "textures/hero/body.dds"_path64);
    static_assert("Meshes//Props\\\\Crate.mesh"_path128 == "meshes/props/crate.mesh"_path128);
    static_assert(hashPath<Fnv1a64>("a/b/c") == Fnv1a64::hash("a/b/c"));
    static_assert(hashPath<XxHash64>("./a/b/..") == XxHash64::hash("a"));
    static_assert(hashPath<XxHash64>("/root/dir/../sub/") == XxHash64::hash("/root/sub"));
    static_assert(hashPath<Fnv1a64>("../up/file.txt") == Fnv1a64::hash("../up/file.txt"));
    static_assert(hashPath<Fnv1a64>("/") == Fnv1a64::hash("/"));

    // Case-sensitive mode preserves letter casing while still normalizing separators.
    EXPECT_NE(
        hashPath<Fnv1a64>("Textures/Hero.dds", HashCase::CaseSensitive),
        hashPath<Fnv1a64>("textures/hero.dds", HashCase::CaseSensitive)
    );
    EXPECT_EQ(
        hashPath<Fnv1a64>("Textures\\Hero.dds", HashCase::CaseSensitive),
        hashPath<Fnv1a64>("Textures/Hero.dds", HashCase::CaseSensitive)
    );

    const gp::String runtimePath("Textures\\Characters\\..\\Characters\\Hero.dds");
    const PathHash64 h64(runtimePath);
    EXPECT_TRUE(h64.isValid());
    EXPECT_TRUE(static_cast<bool>(h64));
    EXPECT_EQ(h64, "textures/characters/hero.dds"_path64);
    EXPECT_EQ(hashOf(h64), h64.value());
    EXPECT_EQ(PathHash64::fromRaw(h64.value()), h64);
    EXPECT_EQ(h64 <=> "textures/characters/hero.dds"_path64, std::strong_ordering::equal);

    // Switch dispatch on integer-backed PathHash64.
    gp::Int32 matched = 0;
    switch (h64)
    {
    case "textures/characters/hero.dds"_path64:
        matched = 1;
        break;
    default:
        matched = -1;
        break;
    }
    EXPECT_EQ(matched, 1);

    // Multi-width PathHash variants and runtime HashAlgorithm dispatch.
    const PathHash32 p32("Shaders/PBR.hlsl");
    const PathHash128 p128("Shaders\\PBR.hlsl");
    const PathWyHash64 pWy("Shaders/./PBR.hlsl");
    const PathDigest256 pSha("SHADERS/PBR.HLSL");
    EXPECT_NE(p32.hash(), 0ULL);
    EXPECT_NE(p128.hash(), 0ULL);
    EXPECT_NE(pWy.hash(), 0ULL);
    EXPECT_NE(pSha.hash(), 0ULL);
    EXPECT_EQ(hashPath(HashAlgorithm::Sha256, "Shaders\\PBR.hlsl").toHash256(), pSha.value());
    EXPECT_EQ(hashPath(HashAlgorithm::Murmur3_128, "Shaders\\PBR.hlsl").toHash128(), p128.value());
    EXPECT_TRUE(hashPath(HashAlgorithm::XxHash64, "").isZero());

    // Streamable via hashValues / hashAppend.
    EXPECT_NE(hashValues<XxHash64>(h64, 1u), hashValues<XxHash64>(h64, 2u));

    // DynamicHasher normalizeAndStreamPath overload.
    DynamicHasher dyn(HashAlgorithm::XxHash64);
    normalizeAndStreamPath(dyn, "Audio\\SFX/../Music/Theme.ogg", HashCase::IgnoreCase);
    EXPECT_EQ(dyn.finalize64(), XxHash64::hash("audio/music/theme.ogg"));
}

}   // namespace gp::hashing::tests
