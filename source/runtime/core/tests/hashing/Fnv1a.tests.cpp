// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "hashing/Fnv1a.hpp"
#include "containers/strings/FixedString.hpp"
#include "containers/strings/String.hpp"     // IWYU pragma: keep
#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include <gtest/gtest.h>

namespace gp::hashing::tests
{

constexpr gp::UInt64 computeMemberStreamHash() noexcept
{
    Fnv1a64 hasher;
    hasher.update(nullptr, 16u);
    hasher.updateCString("ab");
    hasher.updateIgnoreCase(gp::StringView("CD"));
    hasher.updateCStringIgnoreCase("EF");
    hasher.update("GH", 2u, HashCase::IgnoreCase);
    hasher.update(gp::StringView("IJ"), HashCase::IgnoreCase);
    return hasher.finalize();
}

TEST(Fnv1aTest, ConstexprReferenceVectors32And64)
{
    static_assert(Fnv1a32::hash(nullptr, 0u) == 0x811C9DC5u);
    static_assert(Fnv1a32::hash("") == 0x811C9DC5u);
    static_assert(Fnv1a32::hash("a") == 0xE40C292Cu);
    static_assert(Fnv1a32::hash("abc") == 0x1A47E90Bu);
    static_assert(Fnv1a32::hash("123456789") == 0xBB86B11Cu);
    static_assert(Fnv1a32::hash("The quick brown fox jumps over the lazy dog") == 0x048FFF90u);
    static_assert(Fnv1a32::hash("GraphicalPlayground") == 0xB8230203u);

    static_assert(Fnv1a64::hash(nullptr, 0u) == 0xCBF29CE484222325ULL);
    static_assert(Fnv1a64::hash("") == 0xCBF29CE484222325ULL);
    static_assert(Fnv1a64::hash("a") == 0xAF63DC4C8601EC8CULL);
    static_assert(Fnv1a64::hash("abc") == 0xE71FA2190541574BULL);
    static_assert(Fnv1a64::hash("123456789") == 0x06D5573923C6CDFCULL);
    static_assert(Fnv1a64::hash("The quick brown fox jumps over the lazy dog") == 0xF3F9B7F5E7E47110ULL);
    static_assert(Fnv1a64::hash("GraphicalPlayground") == 0xD9B639267836E963ULL);

    EXPECT_EQ(FNV1a32::hash("abc", 3u), 0x1A47E90Bu);
    EXPECT_EQ(FNV1a64::hash("abc", 3u), 0xE71FA2190541574BULL);
}

TEST(Fnv1aTest, StreamingResetAndCheckpointInvariance)
{
    constexpr const char kText[] = "The quick brown fox jumps over the lazy dog";
    constexpr gp::USize kLen = sizeof(kText) - 1u;

    Fnv1a64 hasher;
    hasher.update(kText, 10u);
    const gp::UInt64 midDigest = hasher.finalize();
    EXPECT_EQ(midDigest, Fnv1a64::hash(kText, 10u));

    // Finalize must not mutate state, so appending the rest reaches the full digest.
    hasher.update(kText + 10u, kLen - 10u);
    EXPECT_EQ(hasher.finalize(), Fnv1a64::hash(kText, kLen));

    // Byte-by-byte streaming through Fnv1a32 must match one-shot.
    Fnv1a32 stream32;
    for (gp::USize i = 0u; i < kLen; ++i)
    {
        stream32.updateByte(static_cast<gp::UInt8>(kText[i]));
    }
    EXPECT_EQ(stream32.finalize(), Fnv1a32::hash(kText, kLen));

    stream32.reset(0x12345678u);
    stream32.update("abc", 3u);
    EXPECT_EQ(stream32.finalize(), Fnv1a32::hash("abc", 3u, 0x12345678u));
}

TEST(Fnv1aTest, InputOverloadsStringTypesAndCaseFolding)
{
    const gp::UInt8 rawBytes[3] = { 0x61u, 0x62u, 0x63u };
    const gp::Byte stdBytes[3] = { gp::Byte{ 0x61 }, gp::Byte{ 0x62 }, gp::Byte{ 0x63 } };
    const gp::StringView view("abc");
    const gp::String owned("abc");
    const gp::FixedString<16> fixed("abc");

    EXPECT_EQ(Fnv1a64::hash(rawBytes), 0xE71FA2190541574BULL);
    EXPECT_EQ(Fnv1a64::hash(stdBytes, 3u), 0xE71FA2190541574BULL);
    EXPECT_EQ(Fnv1a64::hash(static_cast<const void*>(rawBytes), 3u), 0xE71FA2190541574BULL);
    EXPECT_EQ(Fnv1a64::hash(view), 0xE71FA2190541574BULL);
    EXPECT_EQ(Fnv1a64::hash(owned), 0xE71FA2190541574BULL);
    EXPECT_EQ(Fnv1a64::hash(fixed), 0xE71FA2190541574BULL);
    EXPECT_EQ(Fnv1a64::hashCString("abc"), 0xE71FA2190541574BULL);
    EXPECT_EQ(Fnv1a64::hashCString(static_cast<const char*>(nullptr)), Fnv1a64::kOffsetBasis);

    // Case-insensitive variants fold ASCII letters only.
    static_assert(Fnv1a32::hashIgnoreCase("PlayerMesh") == Fnv1a32::hashIgnoreCase("PLAYERMESH"));
    static_assert(Fnv1a64::hashIgnoreCase("PlayerMesh") == Fnv1a64::hash("playermesh"));
    static_assert(Fnv1a64::hash("Mesh", 4u, HashCase::IgnoreCase) == Fnv1a64::hash("mesh"));
    static_assert(Fnv1a64::hash(L"ABC", 3u) == Fnv1a64::hash(L"ABC", 3u));
    static_assert(Fnv1a64::hashIgnoreCase(u"ShaderPass", 10u) == Fnv1a64::hash(u"shaderpass", 10u));

    EXPECT_EQ(Fnv1a64::hashIgnoreCase(gp::StringView("GraphicalPlayground")), Fnv1a64::hash("graphicalplayground"));
    EXPECT_EQ(Fnv1a64::hashCStringIgnoreCase("ABC"), Fnv1a64::hash("abc"));
    EXPECT_EQ(Fnv1a32::hash(gp::StringView("MESH"), HashCase::IgnoreCase), Fnv1a32::hash("mesh"));

    // Direct streaming overloads on StreamingHasherBase (member reset/update/appendAll variants).
    static_assert(computeMemberStreamHash() == Fnv1a64::hash("abcdefghij"));
    EXPECT_EQ(computeMemberStreamHash(), Fnv1a64::hash("abcdefghij"));

    Fnv1a64 multiAppendA;
    multiAppendA.appendAll(gp::UInt32{ 7u }, gp::StringView("Tag"), 3.5f);
    Fnv1a64 multiAppendB;
    multiAppendB.append(gp::UInt32{ 7u });
    multiAppendB.append(gp::StringView("Tag"));
    multiAppendB.append(3.5f);
    EXPECT_EQ(multiAppendA.finalize(), multiAppendB.finalize());

    Fnv1a64 wideRuntime;
    const wchar_t wideName[] = L"Mesh";
    wideRuntime.updateIgnoreCase(wideName, 4u);
    EXPECT_EQ(wideRuntime.finalize(), Fnv1a64::hash(L"mesh", 4u));
}

}   // namespace gp::hashing::tests
