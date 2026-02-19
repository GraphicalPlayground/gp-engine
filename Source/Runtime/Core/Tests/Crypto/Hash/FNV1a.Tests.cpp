// Copyright (c) - Graphical Playground. All rights reserved.

#include "Crypto/Hash/FNV1a.hpp"
#include <catch2/catch_test_macros.hpp>
#include <string>

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
        REQUIRE(
            FNV1a::Hash64(static_cast<const void*>(str), std::char_traits<char>::length(str)) == FNV1a::Hash64(str)
        );
    }

    SECTION("Architecture-Sized Hash Consistency")
    {
        const char* str = "arch test";
        std::string_view sv(str);
        auto hash = FNV1a::Hash(str);

#if GP_ARCHITECTURE_64BIT
        REQUIRE(hash == FNV1a::Hash64(str));
#else
        REQUIRE(hash == FNV1a::Hash32(str));
#endif

        REQUIRE(FNV1a::Hash(str) == FNV1a::Hash(sv));
        REQUIRE(FNV1a::Hash(static_cast<const void*>(str), std::char_traits<char>::length(str)) == FNV1a::Hash(str));
    }

    SECTION("Null Bytes")
    {
        const char data[] = { 'a', '\0', 'b', '\0' };
        auto hash32 = FNV1a::Hash32(data, 4);
        auto hash64 = FNV1a::Hash64(data, 4);
        REQUIRE(hash32 != FNV1a::Hash32("a", 1));
        REQUIRE(hash64 != FNV1a::Hash64("a", 1));
    }

    SECTION("Long Strings")
    {
        std::string longStr(1000, 'x');
        auto hash1 = FNV1a::Hash32(longStr.data(), longStr.size());
        longStr[999] = 'y';
        auto hash2 = FNV1a::Hash32(longStr.data(), longStr.size());
        REQUIRE(hash1 != hash2);

        std::string longStr64(1000, 'x');
        auto hash64_1 = FNV1a::Hash64(longStr64.data(), longStr64.size());
        longStr64[999] = 'y';
        auto hash64_2 = FNV1a::Hash64(longStr64.data(), longStr64.size());
        REQUIRE(hash64_1 != hash64_2);
    }

    SECTION("Constants Accessible")
    {
        REQUIRE(FNV1a::OffsetBasis32 == 2166136261U);
        REQUIRE(FNV1a::Prime32 == 16777619U);
        REQUIRE(FNV1a::OffsetBasis64 == 14695981039346656037ULL);
        REQUIRE(FNV1a::Prime64 == 1099511628211ULL);
    }

    SECTION("32-bit vs 64-bit Produce Different Width Results")
    {
        const char* str = "width test";
        auto hash32 = static_cast<GP::UInt64>(FNV1a::Hash32(str));
        auto hash64 = FNV1a::Hash64(str);
        // They use different offset bases and primes so results must differ.
        REQUIRE(hash32 != hash64);
    }

    SECTION("Single Byte Sensitivity")
    {
        std::string data(16, 'A');
        auto base32 = FNV1a::Hash32(data.data(), data.size());
        auto base64 = FNV1a::Hash64(data.data(), data.size());
        for (GP::SizeT i = 0; i < data.size(); ++i)
        {
            std::string modified = data;
            modified[i] = 'B';
            REQUIRE(FNV1a::Hash32(modified.data(), modified.size()) != base32);
            REQUIRE(FNV1a::Hash64(modified.data(), modified.size()) != base64);
        }
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

        auto hash64_1 = FNV1a::Hash64(input);
        auto hash64_2 = FNV1a::Hash64(input);
        REQUIRE(hash64_1 == hash64_2);
    }
}
