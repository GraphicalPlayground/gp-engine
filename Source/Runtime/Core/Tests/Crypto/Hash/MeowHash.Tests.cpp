// Copyright (c) - Graphical Playground. All rights reserved.

#include "Crypto/Hash/MeowHash.hpp"
#include <catch2/catch_test_macros.hpp>

#if GP_MEOWHASH_AVAILABLE

#include <string>

TEST_CASE("MeowHash Hashing", "[GP][Core][Crypto][Hash][MeowHash]")
{
    using namespace GP::Crypto;

    SECTION("IsAvailable")
    {
        REQUIRE(MeowHash::IsAvailable());
    }

    SECTION("Hash64 Basic Consistency")
    {
        auto hash1 = MeowHash::Hash64("hello");
        auto hash2 = MeowHash::Hash64("hello");
        REQUIRE(hash1 == hash2);

        auto hash3 = MeowHash::Hash64("world");
        REQUIRE(hash1 != hash3);
    }

    SECTION("Hash128 Basic Consistency")
    {
        auto hash1 = MeowHash::Hash128("hello", 5);
        auto hash2 = MeowHash::Hash128("hello", 5);
        REQUIRE(hash1 == hash2);

        auto hash3 = MeowHash::Hash128("world", 5);
        REQUIRE(hash1 != hash3);
    }

    SECTION("Hash128 Non-Trivial Results")
    {
        auto hash = MeowHash::Hash128("hello world", 11);
        REQUIRE((hash.low != 0 || hash.high != 0));
    }

    SECTION("Hash64 Derived from Hash128")
    {
        const char* str = "derived test";
        auto hash64 = MeowHash::Hash64(str, std::char_traits<char>::length(str), 0);
        auto hash128 = MeowHash::Hash128(str, std::char_traits<char>::length(str), 0);
        REQUIRE(hash64 == hash128.low);
    }

    SECTION("Different Seeds Produce Different Hashes")
    {
        const char* str = "seed test";
        auto hash1 = MeowHash::Hash64(str, std::char_traits<char>::length(str), 0);
        auto hash2 = MeowHash::Hash64(str, std::char_traits<char>::length(str), 42);
        REQUIRE(hash1 != hash2);

        auto hash128_1 = MeowHash::Hash128(str, std::char_traits<char>::length(str), 0);
        auto hash128_2 = MeowHash::Hash128(str, std::char_traits<char>::length(str), 42);
        REQUIRE(hash128_1 != hash128_2);
    }

    SECTION("Different Overloads Consistency")
    {
        const char* str = "overload test";
        std::string_view sv(str);

        REQUIRE(MeowHash::Hash64(str) == MeowHash::Hash64(sv));
        REQUIRE(MeowHash::Hash(str) == MeowHash::Hash(sv));
        REQUIRE(
            MeowHash::Hash(str, std::char_traits<char>::length(str)) == MeowHash::Hash(str)
        );
    }

    SECTION("Empty String")
    {
        auto hash1 = MeowHash::Hash64("", static_cast<GP::SizeT>(0), 0);
        auto hash2 = MeowHash::Hash64("", static_cast<GP::SizeT>(0), 0);
        REQUIRE(hash1 == hash2);
    }

    SECTION("Long Strings")
    {
        std::string longStr(1000, 'z');
        auto hash64 = MeowHash::Hash64(longStr.data(), longStr.size(), 0);
        REQUIRE(hash64 != 0);

        auto hash128 = MeowHash::Hash128(longStr.data(), longStr.size(), 0);
        REQUIRE((hash128.low != 0 || hash128.high != 0));
    }

    SECTION("Deterministic Output")
    {
        const char* input = "deterministic";
        auto hash1 = MeowHash::Hash64(input);
        auto hash2 = MeowHash::Hash64(input);
        REQUIRE(hash1 == hash2);

        auto hash128_1 = MeowHash::Hash128(input, std::char_traits<char>::length(input));
        auto hash128_2 = MeowHash::Hash128(input, std::char_traits<char>::length(input));
        REQUIRE(hash128_1 == hash128_2);
    }
}

#else

TEST_CASE("MeowHash Unavailable", "[GP][Core][Crypto][Hash][MeowHash]")
{
    using namespace GP::Crypto;

    SECTION("IsAvailable Returns False")
    {
        REQUIRE_FALSE(MeowHash::IsAvailable());
    }
}

#endif   // GP_MEOWHASH_AVAILABLE
