// Copyright (c) - Graphical Playground. All rights reserved.

#include "Crypto/Hash/xxHash.hpp"
#include <catch2/catch_test_macros.hpp>
#include <string>

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

    SECTION("Different Overloads Consistency - Hash32")
    {
        const char* str = "overload test";
        std::string_view sv(str);

        REQUIRE(xxHash::Hash32(str) == xxHash::Hash32(sv));
        REQUIRE(xxHash::Hash32(str, std::char_traits<char>::length(str)) == xxHash::Hash32(str));
        REQUIRE(
            xxHash::Hash32(static_cast<const void*>(str), std::char_traits<char>::length(str)) == xxHash::Hash32(str)
        );
    }

    SECTION("Different Overloads Consistency - Hash64")
    {
        const char* str = "overload test";
        std::string_view sv(str);

        REQUIRE(xxHash::Hash64(str) == xxHash::Hash64(sv));
        REQUIRE(xxHash::Hash64(str, std::char_traits<char>::length(str)) == xxHash::Hash64(str));
        REQUIRE(
            xxHash::Hash64(static_cast<const void*>(str), std::char_traits<char>::length(str)) == xxHash::Hash64(str)
        );
    }

    SECTION("Different Overloads Consistency - Hash")
    {
        const char* str = "arch overload test";
        std::string_view sv(str);

        REQUIRE(xxHash::Hash(str) == xxHash::Hash(sv));
        REQUIRE(xxHash::Hash(str, std::char_traits<char>::length(str)) == xxHash::Hash(str));
        REQUIRE(xxHash::Hash(static_cast<const void*>(str), std::char_traits<char>::length(str)) == xxHash::Hash(str));
    }

    SECTION("Short Strings")
    {
        REQUIRE(xxHash::Hash32("a") != xxHash::Hash32("b"));
        REQUIRE(xxHash::Hash32("ab") != xxHash::Hash32("ba"));
        REQUIRE(xxHash::Hash64("x") != xxHash::Hash64("y"));
        REQUIRE(xxHash::Hash64("ab") != xxHash::Hash64("ba"));
    }

    SECTION("Block Boundary Testing - 32-bit")
    {
        // Hash32 main loop triggers at >= 16 bytes; test around that boundary.
        std::string str15(15, 'A');
        std::string str16(16, 'A');
        std::string str17(17, 'A');
        REQUIRE(xxHash::Hash32(str15.data(), str15.size()) != xxHash::Hash32(str16.data(), str16.size()));
        REQUIRE(xxHash::Hash32(str16.data(), str16.size()) != xxHash::Hash32(str17.data(), str17.size()));
    }

    SECTION("Block Boundary Testing - 64-bit")
    {
        // Hash64 main loop triggers at >= 32 bytes; test around that boundary.
        std::string str31(31, 'A');
        std::string str32(32, 'A');
        std::string str33(33, 'A');
        REQUIRE(xxHash::Hash64(str31.data(), str31.size()) != xxHash::Hash64(str32.data(), str32.size()));
        REQUIRE(xxHash::Hash64(str32.data(), str32.size()) != xxHash::Hash64(str33.data(), str33.size()));
    }

    SECTION("Single Byte Sensitivity - 32-bit")
    {
        std::string data(20, 'X');
        auto base = xxHash::Hash32(data.data(), data.size());
        for (GP::SizeT i = 0; i < data.size(); ++i)
        {
            std::string modified = data;
            modified[i] = 'Y';
            REQUIRE(xxHash::Hash32(modified.data(), modified.size()) != base);
        }
    }

    SECTION("Single Byte Sensitivity - 64-bit")
    {
        std::string data(40, 'X');
        auto base = xxHash::Hash64(data.data(), data.size());
        for (GP::SizeT i = 0; i < data.size(); ++i)
        {
            std::string modified = data;
            modified[i] = 'Y';
            REQUIRE(xxHash::Hash64(modified.data(), modified.size()) != base);
        }
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

    SECTION("Prime Constants Accessible")
    {
        REQUIRE(xxHash::Prime32_1 == 0x9E3779B1U);
        REQUIRE(xxHash::Prime32_2 == 0x85EBCA77U);
        REQUIRE(xxHash::Prime64_1 == 0x9E3779B185EBCA87ULL);
        REQUIRE(xxHash::Prime64_2 == 0xC2B2AE3D27D4EB4FULL);
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

TEST_CASE("xxHash Edge Cases", "[GP][Core][Crypto][Hash][xxHash]")
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
