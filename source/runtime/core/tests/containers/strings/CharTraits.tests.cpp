// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "containers/strings/CharTraits.hpp"
#include <gtest/gtest.h>

namespace gp::tests
{

using Traits = gp::container::CharTraits<char>;
using WTraits = gp::container::CharTraits<wchar_t>;
using U8Traits = gp::container::CharTraits<char8_t>;
using U16Traits = gp::container::CharTraits<char16_t>;
using U32Traits = gp::container::CharTraits<char32_t>;

TEST(CharTraitsTests, AsciiClassificationAndCaseFolding)
{
    static_assert(Traits::isUpper('A') && Traits::isUpper('Z') && !Traits::isUpper('a'));
    static_assert(Traits::isLower('a') && Traits::isLower('z') && !Traits::isLower('A'));
    static_assert(Traits::isAlpha('G') && Traits::isAlpha('g') && !Traits::isAlpha('5'));
    static_assert(Traits::isDigit('0') && Traits::isDigit('9') && !Traits::isDigit('a'));
    static_assert(
        Traits::isHexDigit('0') && Traits::isHexDigit('f') && Traits::isHexDigit('F') && !Traits::isHexDigit('g')
    );
    static_assert(Traits::isAlnum('A') && Traits::isAlnum('9') && !Traits::isAlnum('_'));
    static_assert(
        Traits::isWhitespace(' ') && Traits::isWhitespace('\t') && Traits::isWhitespace('\n') &&
        Traits::isWhitespace('\r') && !Traits::isWhitespace('x')
    );

    static_assert(Traits::toLower('A') == 'a');
    static_assert(Traits::toLower('Z') == 'z');
    static_assert(Traits::toLower('a') == 'a');
    static_assert(Traits::toLower('5') == '5');

    static_assert(Traits::toUpper('a') == 'A');
    static_assert(Traits::toUpper('z') == 'Z');
    static_assert(Traits::toUpper('A') == 'A');
    static_assert(Traits::toUpper('_') == '_');

    EXPECT_EQ(WTraits::toLower(L'M'), L'm');
    EXPECT_EQ(U8Traits::toUpper(u8'k'), u8'K');
    EXPECT_EQ(U16Traits::toLower(u'Q'), u'q');
    EXPECT_EQ(U32Traits::toUpper(U'w'), U'W');
}

TEST(CharTraitsTests, LengthAndElementComparison)
{
    static_assert(Traits::length(nullptr) == 0u);
    static_assert(Traits::length("") == 0u);
    static_assert(Traits::length("Frostbite") == 9u);
    static_assert(WTraits::length(L"Decima") == 6u);
    static_assert(U16Traits::length(u"Anvil") == 5u);
    static_assert(U32Traits::length(U"idTech") == 6u);

    EXPECT_TRUE(Traits::eq('a', 'a'));
    EXPECT_FALSE(Traits::eq('a', 'A'));
    EXPECT_TRUE(Traits::eqIgnoreCase('a', 'A'));
    EXPECT_TRUE(Traits::eq('a', 'A', SearchCase::IgnoreCase));
    EXPECT_TRUE(Traits::lt('a', 'b'));
    EXPECT_TRUE(Traits::ltIgnoreCase('A', 'b'));
}

TEST(CharTraitsTests, BufferCompareAndEquals)
{
    EXPECT_EQ(Traits::compare("abc", "abc", 3u), 0);
    EXPECT_LT(Traits::compare("abc", "abd", 3u), 0);
    EXPECT_GT(Traits::compare("abd", "abc", 3u), 0);

    EXPECT_EQ(Traits::compareIgnoreCase("AbC", "aBc", 3u), 0);
    EXPECT_LT(Traits::compareIgnoreCase("AbC", "aBd", 3u), 0);

    EXPECT_TRUE(Traits::equals("Engine", "Engine", 6u, SearchCase::CaseSensitive));
    EXPECT_FALSE(Traits::equals("Engine", "engine", 6u, SearchCase::CaseSensitive));
    EXPECT_TRUE(Traits::equals("Engine", "engine", 6u, SearchCase::IgnoreCase));
}

TEST(CharTraitsTests, FindAndRFind)
{
    const char* const sample = "Mesh_LOD0_Mesh";
    const gp::USize len = Traits::length(sample);

    const char* const firstL = Traits::find(sample, len, 'l', SearchCase::IgnoreCase);
    ASSERT_NE(firstL, nullptr);
    EXPECT_EQ(firstL - sample, 5);

    const char* const lastM = Traits::rfind(sample, len, 'm', SearchCase::IgnoreCase);
    ASSERT_NE(lastM, nullptr);
    EXPECT_EQ(lastM - sample, 10);

    EXPECT_EQ(Traits::findSubrange(sample, len, "mesh", 4u, SearchCase::CaseSensitive), Traits::npos);
    EXPECT_EQ(Traits::findSubrange(sample, len, "mesh", 4u, SearchCase::IgnoreCase), 0u);
    EXPECT_EQ(Traits::rfindSubrange(sample, len, "MESH", 4u, SearchCase::IgnoreCase), 10u);
}

TEST(CharTraitsTests, CopyMoveAndAssign)
{
    char buffer[16]{};
    Traits::assign(buffer, 5u, 'x');
    EXPECT_STREQ(buffer, "xxxxx");

    Traits::copy(buffer, "hello_world", 11u);
    buffer[11] = '\0';
    EXPECT_STREQ(buffer, "hello_world");

    // Overlapping move forward (dest > src)
    Traits::move(buffer + 2, buffer, 5u);
    buffer[11] = '\0';
    EXPECT_STREQ(buffer, "hehelloorld");

    // Overlapping move backward (dest < src)
    Traits::move(buffer, buffer + 2, 5u);
    buffer[5] = '\0';
    EXPECT_STREQ(buffer, "hello");
}

TEST(CharTraitsTests, ConstexprFnv1aHashing)
{
    constexpr gp::UInt64 h1 = Traits::hash("PlayerCameraManager", 19u);
    constexpr gp::UInt64 h2 = Traits::hashIgnoreCase("playercameramanager", 19u);
    constexpr gp::UInt64 h3 = Traits::hashIgnoreCase("PLAYERCAMERAMANAGER", 19u);

    static_assert(h1 != h2);
    static_assert(h2 == h3);

    EXPECT_EQ(Traits::hash("Mesh", 4u, SearchCase::IgnoreCase), Traits::hash("MESH", 4u, SearchCase::IgnoreCase));
}

}   // namespace gp::tests
