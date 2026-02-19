// Copyright (c) - Graphical Playground. All rights reserved.

#include "Crypto/Hash/CityHash.hpp"
#include <catch2/catch_test_macros.hpp>
#include <string>

TEST_CASE("CityHash Hashing", "[GP][Core][Crypto][Hash][CityHash]")
{
    using namespace GP::Crypto;

    SECTION("Empty String")
    {
        auto hash64 = CityHash::Hash64("", 0);
        REQUIRE(hash64 != 0);

        auto hash128 = CityHash::Hash128("", 0);
        auto hash128_2 = CityHash::Hash128("", 0);
        REQUIRE(hash128 == hash128_2);
    }

    SECTION("Short Strings")
    {
        REQUIRE(CityHash::Hash64("a") != CityHash::Hash64("b"));
        REQUIRE(CityHash::Hash64("ab") != CityHash::Hash64("ba"));
        REQUIRE(CityHash::Hash64("hello") != CityHash::Hash64("world"));
    }

    SECTION("Medium Strings")
    {
        std::string medium(50, 'x');
        auto hash1 = CityHash::Hash64(medium.data(), medium.size());
        medium[49] = 'y';
        auto hash2 = CityHash::Hash64(medium.data(), medium.size());
        REQUIRE(hash1 != hash2);
    }

    SECTION("Long Strings")
    {
        std::string longStr(1000, 'z');
        auto hash64 = CityHash::Hash64(longStr.data(), longStr.size());
        REQUIRE(hash64 != 0);

        auto hash128 = CityHash::Hash128(longStr.data(), longStr.size());
        REQUIRE(hash128.low != 0 || hash128.high != 0);
    }

    SECTION("Different Overloads Consistency")
    {
        const char* str = "overload test";
        std::string_view sv(str);

        REQUIRE(CityHash::Hash64(str) == CityHash::Hash64(sv));
        REQUIRE(CityHash::Hash64(str, std::char_traits<char>::length(str)) == CityHash::Hash64(str));
        REQUIRE(
            CityHash::Hash64(static_cast<const void*>(str), std::char_traits<char>::length(str)) ==
            CityHash::Hash64(str)
        );

        REQUIRE(CityHash::Hash128(str) == CityHash::Hash128(sv));
        REQUIRE(CityHash::Hash128(str, std::char_traits<char>::length(str)) == CityHash::Hash128(str));
        REQUIRE(
            CityHash::Hash128(static_cast<const void*>(str), std::char_traits<char>::length(str)) ==
            CityHash::Hash128(str)
        );

        REQUIRE(CityHash::Hash(str) == CityHash::Hash(sv));
        REQUIRE(CityHash::Hash(str, std::char_traits<char>::length(str)) == CityHash::Hash(str));
        REQUIRE(
            CityHash::Hash(static_cast<const void*>(str), std::char_traits<char>::length(str)) == CityHash::Hash(str)
        );
    }

    SECTION("Hash128 Produces Non-Trivial Results")
    {
        auto hash1 = CityHash::Hash128("hello");
        REQUIRE(hash1.low != 0);
        REQUIRE(hash1.high != 0);

        auto hash2 = CityHash::Hash128("world");
        REQUIRE(hash1 != hash2);

        auto hash3 = CityHash::Hash128("hello");
        REQUIRE(hash1 == hash3);
    }

    SECTION("HashWithSeed64 Differs from Hash64")
    {
        const char* str = "seed comparison";
        auto hashNoSeed = CityHash::Hash64(str);
        auto hashWithSeed = CityHash::HashWithSeed64(str, std::char_traits<char>::length(str), 42);
        REQUIRE(hashNoSeed != hashWithSeed);

        auto hashWithSeed2 = CityHash::HashWithSeed64(str, std::char_traits<char>::length(str), 99);
        REQUIRE(hashWithSeed != hashWithSeed2);
    }

    SECTION("User-Defined Literal")
    {
        auto hash1 = "test"_cityhash;
        auto hash2 = CityHash::Hash64("test");
        REQUIRE(hash1 == hash2);
    }

    SECTION("Compile-time Evaluation")
    {
        constexpr auto hash = CityHash::Hash64("constexpr", 9);
        REQUIRE(hash == CityHash::Hash64("constexpr"));
    }

    SECTION("Deterministic Output")
    {
        const char* input = "deterministic";
        auto hash1 = CityHash::Hash64(input);
        auto hash2 = CityHash::Hash64(input);
        REQUIRE(hash1 == hash2);

        auto hash128_1 = CityHash::Hash128(input);
        auto hash128_2 = CityHash::Hash128(input);
        REQUIRE(hash128_1 == hash128_2);
    }

    SECTION("Binary Data with Nulls")
    {
        const char data[] = { 'a', '\0', 'b', '\0', 'c' };
        auto hash = CityHash::Hash64(data, 5);
        REQUIRE(hash != CityHash::Hash64("abc"));
    }

    SECTION("HashWithSeed64 Void-Pointer Overload")
    {
        const char* str = "seeded void";
        auto h1 = CityHash::HashWithSeed64(str, std::char_traits<char>::length(str), 77ULL);
        REQUIRE(h1 != CityHash::Hash64(str));
        REQUIRE(h1 == CityHash::HashWithSeed64(str, std::char_traits<char>::length(str), 77ULL));
    }

    SECTION("Hash64 Length Boundary Coverage")
    {
        // Exercises <= 16, 17-32, 33-64, and > 64 branches explicitly.
        for (GP::SizeT len: { GP::SizeT(0),
                              GP::SizeT(8),
                              GP::SizeT(16),
                              GP::SizeT(17),
                              GP::SizeT(32),
                              GP::SizeT(33),
                              GP::SizeT(64),
                              GP::SizeT(65),
                              GP::SizeT(128) })
        {
            std::string a(len, 'C');
            REQUIRE(CityHash::Hash64(a.data(), a.size()) == CityHash::Hash64(a.data(), a.size()));
        }
    }

    SECTION("Hash128 Length Boundary Coverage")
    {
        // Exercises CityMurmur (< 128) and full Hash128WithSeed (>= 128) paths.
        for (GP::SizeT len:
             { GP::SizeT(0), GP::SizeT(16), GP::SizeT(64), GP::SizeT(127), GP::SizeT(128), GP::SizeT(256) })
        {
            std::string a(len, 'H');
            auto h1 = CityHash::Hash128(a.data(), a.size());
            auto h2 = CityHash::Hash128(a.data(), a.size());
            REQUIRE(h1 == h2);
        }
    }

    SECTION("Single Byte Sensitivity - Hash64")
    {
        std::string data(80, 'T');
        auto base = CityHash::Hash64(data.data(), data.size());
        for (GP::SizeT i = 0; i < data.size(); ++i)
        {
            std::string modified = data;
            modified[i] = 'U';
            REQUIRE(CityHash::Hash64(modified.data(), modified.size()) != base);
        }
    }

    SECTION("Single Byte Sensitivity - Hash128")
    {
        std::string data(80, 'T');
        auto base = CityHash::Hash128(data.data(), data.size());
        for (GP::SizeT i = 0; i < data.size(); ++i)
        {
            std::string modified = data;
            modified[i] = 'U';
            REQUIRE(CityHash::Hash128(modified.data(), modified.size()) != base);
        }
    }
}
