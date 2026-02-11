// Copyright (c) - Graphical Playground. All rights reserved.

#include "Crypto/Hash/FNV1a.hpp"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("FNV1a Hashing", "[GP][Core][Crypto][Hash][FNV1a]")
{
    using namespace GP::Crypto;

    SECTION("Empty String")
    {
        REQUIRE(FNV1a::Hash("", 0) == FNV1a::OffsetBasis);
        REQUIRE(FNV1a::Hash32("", 0) == FNV1a::OffsetBasis32);
        REQUIRE(FNV1a::Hash64("", 0) == FNV1a::OffsetBasis64);
    }

    SECTION("Known Test Vectors")
    {
        REQUIRE(FNV1a::Hash32("hello") == 0x4F9F2CABU);
        REQUIRE(FNV1a::Hash32("Hello World") == 0xB3902527U);
        REQUIRE(FNV1a::Hash32("test") == 0xAFD071E5U);
        REQUIRE(FNV1a::Hash32("The quick brown fox jumps over the lazy dog") == 0x048FFF90U);

        REQUIRE(FNV1a::Hash64("hello") == 0XA430D84680AABD0BULL);
        REQUIRE(FNV1a::Hash64("test") == 0xF9E6E6EF197C2B25ULL);
    }

    SECTION("Single Character")
    {
        REQUIRE(FNV1a::Hash32("a") != FNV1a::Hash32("b"));
        REQUIRE(FNV1a::Hash64("a") != FNV1a::Hash64("b"));
        REQUIRE(FNV1a::Hash32("A") != FNV1a::Hash32("a"));
    }

    SECTION("Different Overloads Consistency")
    {
        const char* str = "test string";
        std::string_view sv(str);

        REQUIRE(FNV1a::Hash32(str) == FNV1a::Hash32(sv));
        REQUIRE(FNV1a::Hash32(str, std::char_traits<char>::length(str)) == FNV1a::Hash32(str));
        REQUIRE(
            FNV1a::Hash32(static_cast<const void*>(str), std::char_traits<char>::length(str)) == FNV1a::Hash32(str)
        );

        REQUIRE(FNV1a::Hash64(str) == FNV1a::Hash64(sv));
        REQUIRE(FNV1a::Hash64(str, std::char_traits<char>::length(str)) == FNV1a::Hash64(str));
    }

    SECTION("Null Bytes")
    {
        const char data[] = { 'a', '\0', 'b', '\0' };
        auto hash1 = FNV1a::Hash32(data, 4);
        auto hash2 = FNV1a::Hash32("a", 1);
        REQUIRE(hash1 != hash2);
    }

    SECTION("Long Strings")
    {
        std::string longStr(1000, 'x');
        auto hash1 = FNV1a::Hash32(longStr.data(), longStr.size());
        longStr[999] = 'y';
        auto hash2 = FNV1a::Hash32(longStr.data(), longStr.size());
        REQUIRE(hash1 != hash2);
    }

    SECTION("Compile-time Evaluation")
    {
        constexpr auto hash = FNV1a::Hash32("constexpr", 9);
        REQUIRE(hash == FNV1a::Hash32("constexpr"));

        constexpr auto hash64 = FNV1a::Hash64("test", 4);
        REQUIRE(hash64 == FNV1a::Hash64("test"));
    }

    SECTION("User-Defined Literal")
    {
        auto hash1 = "hello"_fnv1a;
        auto hash2 = FNV1a::Hash("hello");
        REQUIRE(hash1 == hash2);
    }

    SECTION("Deterministic Output")
    {
        const char* input = "deterministic test";
        auto hash1 = FNV1a::Hash32(input);
        auto hash2 = FNV1a::Hash32(input);
        REQUIRE(hash1 == hash2);
    }
}
