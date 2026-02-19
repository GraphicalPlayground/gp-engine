// Copyright (c) - Graphical Playground. All rights reserved.

#include "Crypto/Hash/FarmHash.hpp"
#include <catch2/catch_test_macros.hpp>
#include <string>

TEST_CASE("FarmHash Hashing", "[GP][Core][Crypto][Hash][FarmHash]")
{
    using namespace GP::Crypto;

    SECTION("Empty String")
    {
        auto hash32 = FarmHash::Hash32("", 0);
        auto hash64 = FarmHash::Hash64("", 0);

        auto hash32_2 = FarmHash::Hash32("", 0);
        auto hash64_2 = FarmHash::Hash64("", 0);
        REQUIRE(hash32 == hash32_2);
        REQUIRE(hash64 == hash64_2);
    }

    SECTION("Different Overloads Consistency - Hash32")
    {
        const char* str = "overload test";
        std::string_view sv(str);

        REQUIRE(FarmHash::Hash32(str) == FarmHash::Hash32(sv));
        REQUIRE(FarmHash::Hash32(str, std::char_traits<char>::length(str)) == FarmHash::Hash32(str));
        REQUIRE(
            FarmHash::Hash32(static_cast<const void*>(str), std::char_traits<char>::length(str)) ==
            FarmHash::Hash32(str)
        );
    }

    SECTION("Different Overloads Consistency - Hash64")
    {
        const char* str = "overload test";
        std::string_view sv(str);

        REQUIRE(FarmHash::Hash64(str) == FarmHash::Hash64(sv));
        REQUIRE(FarmHash::Hash64(str, std::char_traits<char>::length(str)) == FarmHash::Hash64(str));
        REQUIRE(
            FarmHash::Hash64(static_cast<const void*>(str), std::char_traits<char>::length(str)) ==
            FarmHash::Hash64(str)
        );
    }

    SECTION("Different Overloads Consistency - Hash")
    {
        const char* str = "overload test";
        std::string_view sv(str);

        REQUIRE(FarmHash::Hash(str) == FarmHash::Hash(sv));
        REQUIRE(FarmHash::Hash(str, std::char_traits<char>::length(str)) == FarmHash::Hash(str));
        REQUIRE(
            FarmHash::Hash(static_cast<const void*>(str), std::char_traits<char>::length(str)) == FarmHash::Hash(str)
        );
    }

    SECTION("Hash32 and Hash64 Produce Different Values for Same Input")
    {
        const char* str = "width test";
        auto hash32 = static_cast<GP::UInt64>(FarmHash::Hash32(str));
        auto hash64 = FarmHash::Hash64(str);
        REQUIRE(hash32 != hash64);
    }

    SECTION("Architecture-Sized Hash")
    {
        const char* str = "architecture test";
        auto hash = FarmHash::Hash(str);

#if GP_ARCHITECTURE_64BIT
        REQUIRE(hash == FarmHash::Hash64(str));
#else
        REQUIRE(hash == FarmHash::Hash32(str));
#endif
    }

    SECTION("Short Strings")
    {
        REQUIRE(FarmHash::Hash32("a") != FarmHash::Hash32("b"));
        REQUIRE(FarmHash::Hash64("a") != FarmHash::Hash64("b"));
        REQUIRE(FarmHash::Hash32("ab") != FarmHash::Hash32("ba"));
    }

    SECTION("Medium Strings")
    {
        std::string medium(50, 'x');
        auto hash1 = FarmHash::Hash64(medium.data(), medium.size());
        medium[49] = 'y';
        auto hash2 = FarmHash::Hash64(medium.data(), medium.size());
        REQUIRE(hash1 != hash2);
    }

    SECTION("Long Strings")
    {
        std::string longStr(1000, 'z');
        auto hash32 = FarmHash::Hash32(longStr.data(), longStr.size());
        auto hash64 = FarmHash::Hash64(longStr.data(), longStr.size());
        REQUIRE(hash32 != 0);
        REQUIRE(hash64 != 0);
    }

    SECTION("User-Defined Literal")
    {
        auto hash1 = "test"_farmhash;
        auto hash2 = FarmHash::Hash("test");
        REQUIRE(hash1 == hash2);
    }

    SECTION("Compile-time Evaluation")
    {
        constexpr auto hash32 = FarmHash::Hash32("constexpr", 9);
        REQUIRE(hash32 == FarmHash::Hash32("constexpr"));

        constexpr auto hash64 = FarmHash::Hash64("compile", 7);
        REQUIRE(hash64 == FarmHash::Hash64("compile"));
    }

    SECTION("Deterministic Output")
    {
        const char* input = "deterministic";
        auto hash32_1 = FarmHash::Hash32(input);
        auto hash32_2 = FarmHash::Hash32(input);
        REQUIRE(hash32_1 == hash32_2);

        auto hash64_1 = FarmHash::Hash64(input);
        auto hash64_2 = FarmHash::Hash64(input);
        REQUIRE(hash64_1 == hash64_2);
    }

    SECTION("Binary Data with Nulls")
    {
        const char data[] = { 'a', '\0', 'b', '\0', 'c' };
        auto hash = FarmHash::Hash32(data, 5);
        REQUIRE(hash != FarmHash::Hash32("abc"));
    }

    SECTION("Hash32 Length Boundary Coverage")
    {
        // Exercises the 0-4, 5-12, 13-24, and > 24 internal branches.
        const char* str = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
        for (GP::SizeT len = 0; len <= 26; ++len)
        {
            std::string a(len, 'Z');
            std::string b(len, 'Z');
            if (len > 0) { b.back() = 'Y'; }
            if (len > 0) { REQUIRE(FarmHash::Hash32(a.data(), a.size()) != FarmHash::Hash32(b.data(), b.size())); }
            // Deterministic at each length.
            REQUIRE(FarmHash::Hash32(a.data(), a.size()) == FarmHash::Hash32(a.data(), a.size()));
        }
    }

    SECTION("Hash64 Length Boundary Coverage")
    {
        // Exercises <= 8, 8-16, 17-32, 33-64, and > 64 branches.
        for (GP::SizeT len: { GP::SizeT(0),
                              GP::SizeT(4),
                              GP::SizeT(8),
                              GP::SizeT(16),
                              GP::SizeT(17),
                              GP::SizeT(32),
                              GP::SizeT(33),
                              GP::SizeT(64),
                              GP::SizeT(65),
                              GP::SizeT(128) })
        {
            std::string a(len, 'F');
            REQUIRE(FarmHash::Hash64(a.data(), a.size()) == FarmHash::Hash64(a.data(), a.size()));
        }
    }

    SECTION("Single Byte Sensitivity - 32-bit")
    {
        std::string data(30, 'K');
        auto base = FarmHash::Hash32(data.data(), data.size());
        for (GP::SizeT i = 0; i < data.size(); ++i)
        {
            std::string modified = data;
            modified[i] = 'L';
            REQUIRE(FarmHash::Hash32(modified.data(), modified.size()) != base);
        }
    }

    SECTION("Single Byte Sensitivity - 64-bit")
    {
        std::string data(80, 'K');
        auto base = FarmHash::Hash64(data.data(), data.size());
        for (GP::SizeT i = 0; i < data.size(); ++i)
        {
            std::string modified = data;
            modified[i] = 'L';
            REQUIRE(FarmHash::Hash64(modified.data(), modified.size()) != base);
        }
    }
}
