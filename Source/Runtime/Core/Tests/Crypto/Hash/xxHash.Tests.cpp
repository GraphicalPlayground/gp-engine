// Copyright (c) - Graphical Playground. All rights reserved.

#include "Crypto/Hash/xxHash.hpp"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("xxHash Hashing", "[GP][Core][Crypto][Hash][xxHash]")
{
    using namespace GP::Crypto;

    SECTION("Empty String")
    {
        REQUIRE(xxHash::Hash32("", 0) == 0x02CC5D05U);
        REQUIRE(xxHash::Hash64("", 0) == 0xEF46DB3751D8E999ULL);
    }

    SECTION("Known Test Vectors 32-bit")
    {
        REQUIRE(xxHash::Hash32("hello", 5, 0) == 0XFB0077F9U);
        REQUIRE(xxHash::Hash32("a", 1, 0) == 0x550D7456U);
    }

    SECTION("Known Test Vectors 64-bit")
    {
        REQUIRE(xxHash::Hash64("hello", 5, 0) == 0X26C7827D889F6DA3ULL);
        REQUIRE(xxHash::Hash64("a", 1, 0) == 0xD24EC4F1A98C6E5BULL);
    }

    SECTION("Different Seeds Produce Different Hashes")
    {
        const char* str = "seed test";
        auto hash1 = xxHash::Hash32(str, std::char_traits<char>::length(str), 0);
        auto hash2 = xxHash::Hash32(str, std::char_traits<char>::length(str), 42);
        REQUIRE(hash1 != hash2);

        auto hash64_1 = xxHash::Hash64(str, std::char_traits<char>::length(str), 0);
        auto hash64_2 = xxHash::Hash64(str, std::char_traits<char>::length(str), 42);
        REQUIRE(hash64_1 != hash64_2);
    }

    SECTION("Different Overloads Consistency")
    {
        const char* str = "overload test";
        std::string_view sv(str);

        REQUIRE(xxHash::Hash32(str) == xxHash::Hash32(sv));
        REQUIRE(xxHash::Hash32(str, std::char_traits<char>::length(str)) == xxHash::Hash32(str));
        REQUIRE(
            xxHash::Hash32(static_cast<const void*>(str), std::char_traits<char>::length(str)) == xxHash::Hash32(str)
        );

        REQUIRE(xxHash::Hash64(str) == xxHash::Hash64(sv));
        REQUIRE(xxHash::Hash64(str, std::char_traits<char>::length(str)) == xxHash::Hash64(str));
    }

    SECTION("Short Strings")
    {
        REQUIRE(xxHash::Hash32("a") != xxHash::Hash32("b"));
        REQUIRE(xxHash::Hash32("ab") != xxHash::Hash32("ba"));
        REQUIRE(xxHash::Hash64("x") != xxHash::Hash64("y"));
    }

    SECTION("Medium Strings")
    {
        std::string medium(50, 'x');
        auto hash1 = xxHash::Hash32(medium.data(), medium.size());
        medium[49] = 'y';
        auto hash2 = xxHash::Hash32(medium.data(), medium.size());
        REQUIRE(hash1 != hash2);
    }

    SECTION("Long Strings")
    {
        std::string longStr(1000, 'z');
        auto hash32 = xxHash::Hash32(longStr.data(), longStr.size());
        auto hash64 = xxHash::Hash64(longStr.data(), longStr.size());
        REQUIRE(hash32 != 0);
        REQUIRE(hash64 != 0);
    }

    SECTION("Binary Data with Nulls")
    {
        const char data[] = { 'a', '\0', 'b', '\0', 'c' };
        auto hash = xxHash::Hash32(data, 5);
        REQUIRE(hash != xxHash::Hash32("abc"));
    }

    SECTION("Architecture-Sized Hash")
    {
        const char* str = "architecture test";
        auto hash = xxHash::Hash(str);

#if GP_ARCHITECTURE_64BIT
        REQUIRE(hash == xxHash::Hash64(str));
#else
        REQUIRE(hash == xxHash::Hash32(str));
#endif
    }

    SECTION("Compile-time Evaluation")
    {
        constexpr auto hash32 = xxHash::Hash32("compile", 7, 0);
        REQUIRE(hash32 == xxHash::Hash32("compile", 7, 0));

        constexpr auto hash64 = xxHash::Hash64("time", 4, 0);
        REQUIRE(hash64 == xxHash::Hash64("time", 4, 0));
    }

    SECTION("User-Defined Literal")
    {
        auto hash1 = "literal"_xxhash;
        auto hash2 = xxHash::Hash("literal");
        REQUIRE(hash1 == hash2);
    }

    SECTION("Deterministic Output")
    {
        const char* input = "deterministic";
        auto hash1 = xxHash::Hash32(input);
        auto hash2 = xxHash::Hash32(input);
        REQUIRE(hash1 == hash2);

        auto hash64_1 = xxHash::Hash64(input);
        auto hash64_2 = xxHash::Hash64(input);
        REQUIRE(hash64_1 == hash64_2);
    }

    SECTION("Large Block Processing")
    {
        std::string large(200, 'L');
        auto hash1 = xxHash::Hash32(large.data(), large.size());
        REQUIRE(hash1 != 0);

        auto hash2 = xxHash::Hash64(large.data(), large.size());
        REQUIRE(hash2 != 0);
    }
}
