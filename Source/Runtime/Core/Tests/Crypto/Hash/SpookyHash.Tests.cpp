// Copyright (c) - Graphical Playground. All rights reserved.

#include "Crypto/Hash/SpookyHash.hpp"
#include <catch2/catch_test_macros.hpp>
#include <string>

TEST_CASE("SpookyHash Hashing", "[GP][Core][Crypto][Hash][SpookyHash]")
{
    using namespace GP::Crypto;

    SECTION("Empty String")
    {
        auto hash64 = SpookyHash::Hash64("", 0);
        auto hash64_2 = SpookyHash::Hash64("", 0);
        REQUIRE(hash64 == hash64_2);

        auto hash128 = SpookyHash::Hash128("", 0);
        auto hash128_2 = SpookyHash::Hash128("", 0);
        REQUIRE(hash128 == hash128_2);
    }

    SECTION("Known Consistency")
    {
        auto h1 = SpookyHash::Hash64("hello");
        auto h2 = SpookyHash::Hash64("hello");
        REQUIRE(h1 == h2);

        auto h3 = SpookyHash::Hash64("hello world");
        REQUIRE(h1 != h3);

        auto h4 = SpookyHash::Hash64("a");
        auto h5 = SpookyHash::Hash64("b");
        REQUIRE(h4 != h5);
    }

    SECTION("Different Seeds Produce Different Results")
    {
        const char* str = "seed test";
        auto hash1 = SpookyHash::Hash64(str, std::char_traits<char>::length(str), 0);
        auto hash2 = SpookyHash::Hash64(str, std::char_traits<char>::length(str), 42);
        REQUIRE(hash1 != hash2);

        auto hash3 = SpookyHash::Hash64(str, std::char_traits<char>::length(str), 0xDEADBEEFULL);
        REQUIRE(hash1 != hash3);
        REQUIRE(hash2 != hash3);

        auto hash128_1 = SpookyHash::Hash128(str, std::char_traits<char>::length(str), 0, 0);
        auto hash128_2 = SpookyHash::Hash128(str, std::char_traits<char>::length(str), 42, 42);
        REQUIRE(hash128_1 != hash128_2);
    }

    SECTION("Hash128 Non-Trivial Results")
    {
        auto hash1 = SpookyHash::Hash128("hello");
        REQUIRE(hash1.low != 0 || hash1.high != 0);

        auto hash2 = SpookyHash::Hash128("world");
        REQUIRE(hash1 != hash2);

        auto hash3 = SpookyHash::Hash128("hello");
        REQUIRE(hash1 == hash3);
    }

    SECTION("Hash128 Overloads Consistency")
    {
        const char* str = "overload test";
        std::string_view sv(str);

        REQUIRE(SpookyHash::Hash128(str) == SpookyHash::Hash128(sv));
        REQUIRE(SpookyHash::Hash128(str, std::char_traits<char>::length(str)) == SpookyHash::Hash128(str));
        REQUIRE(
            SpookyHash::Hash128(static_cast<const void*>(str), std::char_traits<char>::length(str)) ==
            SpookyHash::Hash128(str)
        );
    }

    SECTION("Hash64 Overloads Consistency")
    {
        const char* str = "overload test";
        std::string_view sv(str);

        REQUIRE(SpookyHash::Hash64(str) == SpookyHash::Hash64(sv));
        REQUIRE(SpookyHash::Hash64(str, std::char_traits<char>::length(str)) == SpookyHash::Hash64(str));
        REQUIRE(
            SpookyHash::Hash64(static_cast<const void*>(str), std::char_traits<char>::length(str)) ==
            SpookyHash::Hash64(str)
        );

        REQUIRE(SpookyHash::Hash(str) == SpookyHash::Hash(sv));
        REQUIRE(SpookyHash::Hash(str, std::char_traits<char>::length(str)) == SpookyHash::Hash(str));
        REQUIRE(
            SpookyHash::Hash(static_cast<const void*>(str), std::char_traits<char>::length(str)) ==
            SpookyHash::Hash(str)
        );
    }

    SECTION("Hash64 Derived from Hash128")
    {
        const char* str = "derived test";
        auto hash64 = SpookyHash::Hash64(str);
        auto hash128 = SpookyHash::Hash128(str);
        REQUIRE(hash64 == hash128.low);
    }

    SECTION("User-Defined Literal")
    {
        auto hash1 = "test"_spooky;
        auto hash2 = SpookyHash::Hash64("test");
        REQUIRE(hash1 == hash2);
    }

    SECTION("Compile-time Evaluation")
    {
        constexpr auto hash = SpookyHash::Hash64("constexpr", 9);
        REQUIRE(hash == SpookyHash::Hash64("constexpr"));
    }

    SECTION("Deterministic Output")
    {
        const char* input = "deterministic";
        auto hash64_1 = SpookyHash::Hash64(input);
        auto hash64_2 = SpookyHash::Hash64(input);
        REQUIRE(hash64_1 == hash64_2);

        auto hash128_1 = SpookyHash::Hash128(input);
        auto hash128_2 = SpookyHash::Hash128(input);
        REQUIRE(hash128_1 == hash128_2);
    }

    SECTION("Long Strings")
    {
        std::string longStr(1000, 'z');
        auto hash64 = SpookyHash::Hash64(longStr.data(), longStr.size());
        REQUIRE(hash64 != 0);

        auto hash128 = SpookyHash::Hash128(longStr.data(), longStr.size());
        // clang-format off
        REQUIRE((hash128.low != 0 || hash128.high != 0));
        // clang-format on
    }

    SECTION("ShortHash Branch - 16 to 31 Bytes")
    {
        // Exercises the nRounds > 0 path inside ShortHash.
        for (GP::SizeT len = 16; len < 32; ++len)
        {
            std::string a(len, 'Q');
            std::string b(len, 'Q');
            b.back() = 'R';
            REQUIRE(SpookyHash::Hash64(a.data(), a.size()) != SpookyHash::Hash64(b.data(), b.size()));
        }
    }

    SECTION("ShortHash Branch - 32 to 191 Bytes")
    {
        // Exercises multiple full 32-byte rounds in ShortHash.
        std::string base(128, 'Z');
        auto h1 = SpookyHash::Hash64(base.data(), base.size());
        base[64] = 'A';
        auto h2 = SpookyHash::Hash64(base.data(), base.size());
        REQUIRE(h1 != h2);
    }

    SECTION("Binary Data with Nulls")
    {
        const char data[] = { 'a', '\0', 'b', '\0', 'c' };
        auto hash64 = SpookyHash::Hash64(data, 5);
        auto hash128 = SpookyHash::Hash128(data, 5);
        REQUIRE(hash64 != SpookyHash::Hash64("abc"));
        REQUIRE(hash128 != SpookyHash::Hash128("abc", 3));
    }

    SECTION("Hash128 Different Seed Pairs")
    {
        const char* str = "seed pairs";
        auto h1 = SpookyHash::Hash128(str, std::char_traits<char>::length(str), 1, 2);
        auto h2 = SpookyHash::Hash128(str, std::char_traits<char>::length(str), 2, 1);
        auto h3 = SpookyHash::Hash128(str, std::char_traits<char>::length(str), 0, 0);
        REQUIRE(h1 != h2);
        REQUIRE(h1 != h3);
        REQUIRE(h2 != h3);
    }

    SECTION("Hash128 Void-Pointer Overload")
    {
        const char* str = "void ptr 128";
        auto h1 = SpookyHash::Hash128(str, std::char_traits<char>::length(str));
        auto h2 = SpookyHash::Hash128(static_cast<const void*>(str), std::char_traits<char>::length(str));
        REQUIRE(h1 == h2);
    }
}
