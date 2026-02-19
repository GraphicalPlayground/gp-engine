// Copyright (c) - Graphical Playground. All rights reserved.

#include "Crypto/Hash/WyHash.hpp"
#include <catch2/catch_test_macros.hpp>
#include <string>

TEST_CASE("WyHash Hashing", "[GP][Core][Crypto][Hash][WyHash]")
{
    using namespace GP::Crypto;

    SECTION("Empty String")
    {
        auto hash = WyHash::Hash64("", 0);
        REQUIRE(hash != 0);

        auto hash2 = WyHash::Hash64("", 0);
        REQUIRE(hash == hash2);
    }

    SECTION("Known Consistency Checks")
    {
        auto h1 = WyHash::Hash64("hello");
        auto h2 = WyHash::Hash64("hello");
        REQUIRE(h1 == h2);

        auto h3 = WyHash::Hash64("hello world");
        REQUIRE(h1 != h3);

        auto h4 = WyHash::Hash64("a");
        auto h5 = WyHash::Hash64("b");
        REQUIRE(h4 != h5);
    }

    SECTION("Different Seeds Produce Different Hashes")
    {
        const char* str = "seed test";
        auto hash1 = WyHash::Hash64(str, std::char_traits<char>::length(str), 0);
        auto hash2 = WyHash::Hash64(str, std::char_traits<char>::length(str), 42);
        REQUIRE(hash1 != hash2);

        auto hash3 = WyHash::Hash64(str, std::char_traits<char>::length(str), 0xDEADBEEFULL);
        REQUIRE(hash1 != hash3);
        REQUIRE(hash2 != hash3);
    }

    SECTION("Different Overloads Consistency")
    {
        const char* str = "overload test";
        std::string_view sv(str);

        REQUIRE(WyHash::Hash64(str) == WyHash::Hash64(sv));
        REQUIRE(WyHash::Hash64(str, std::char_traits<char>::length(str)) == WyHash::Hash64(str));
        REQUIRE(
            WyHash::Hash64(static_cast<const void*>(str), std::char_traits<char>::length(str)) == WyHash::Hash64(str)
        );

        REQUIRE(WyHash::Hash(str) == WyHash::Hash(sv));
        REQUIRE(WyHash::Hash(str, std::char_traits<char>::length(str)) == WyHash::Hash(str));
        REQUIRE(WyHash::Hash(static_cast<const void*>(str), std::char_traits<char>::length(str)) == WyHash::Hash(str));
    }

    SECTION("Short Strings")
    {
        REQUIRE(WyHash::Hash64("a") != WyHash::Hash64("b"));
        REQUIRE(WyHash::Hash64("ab") != WyHash::Hash64("ba"));
    }

    SECTION("Medium Strings")
    {
        std::string medium(50, 'x');
        auto hash1 = WyHash::Hash64(medium.data(), medium.size());
        medium[49] = 'y';
        auto hash2 = WyHash::Hash64(medium.data(), medium.size());
        REQUIRE(hash1 != hash2);
    }

    SECTION("Long Strings")
    {
        std::string longStr(1000, 'z');
        auto hash = WyHash::Hash64(longStr.data(), longStr.size());
        REQUIRE(hash != 0);
    }

    SECTION("User-Defined Literal")
    {
        auto hash1 = "test"_wyhash;
        auto hash2 = WyHash::Hash64("test");
        REQUIRE(hash1 == hash2);
    }

    SECTION("Consistency Check")
    {
        auto hash = WyHash::Hash64("constexpr", 9);
        REQUIRE(hash == WyHash::Hash64("constexpr"));
    }

    SECTION("Deterministic Output")
    {
        const char* input = "deterministic";
        auto hash1 = WyHash::Hash64(input);
        auto hash2 = WyHash::Hash64(input);
        REQUIRE(hash1 == hash2);

        auto hash3 = WyHash::Hash(input);
        auto hash4 = WyHash::Hash(input);
        REQUIRE(hash3 == hash4);
    }

    SECTION("Binary Data with Nulls")
    {
        const char data[] = { 'a', '\0', 'b', '\0', 'c' };
        auto hash = WyHash::Hash64(data, 5);
        REQUIRE(hash != WyHash::Hash64("abc"));
    }

    SECTION("Large Input - Three-Lane Path (> 48 bytes)")
    {
        // Exercises the do-while loop in Hash64 that uses see1/see2 alongside seed.
        std::string large(200, 'W');
        auto h1 = WyHash::Hash64(large.data(), large.size());
        auto h2 = WyHash::Hash64(large.data(), large.size());
        REQUIRE(h1 == h2);
        large[100] = 'X';
        auto h3 = WyHash::Hash64(large.data(), large.size());
        REQUIRE(h1 != h3);
    }

    SECTION("Large Input - Single Byte Sensitivity In > 48 Byte Block")
    {
        std::string data(96, 'P');
        auto base = WyHash::Hash64(data.data(), data.size());
        for (GP::SizeT i = 0; i < data.size(); ++i)
        {
            std::string mod = data;
            mod[i] = 'Q';
            REQUIRE(WyHash::Hash64(mod.data(), mod.size()) != base);
        }
    }

    SECTION("Block Boundary - 17-48 byte range")
    {
        // Exercises the length <= 48 branch that uses WyMix with optional second block.
        for (GP::SizeT len = 17; len <= 48; ++len)
        {
            std::string a(len, 'R');
            std::string b(len, 'R');
            b.back() = 'S';
            REQUIRE(WyHash::Hash64(a.data(), a.size()) != WyHash::Hash64(b.data(), b.size()));
        }
    }

    SECTION("Null-Terminated C-String Overload for Hash")
    {
        const char* str = "null terminated";
        REQUIRE(WyHash::Hash(str) == WyHash::Hash64(str));
        REQUIRE(WyHash::Hash(str) == WyHash::Hash(std::string_view(str)));
    }
}
