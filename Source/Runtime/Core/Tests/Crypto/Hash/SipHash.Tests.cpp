// Copyright (c) - Graphical Playground. All rights reserved.

#include "Crypto/Hash/SipHash.hpp"
#include <catch2/catch_test_macros.hpp>
#include <string>

TEST_CASE("SipHash Hashing", "[GP][Core][Crypto][Hash][SipHash]")
{
    using namespace GP::Crypto;

    SECTION("Empty String")
    {
        auto hash = SipHash::Hash64("", 0);
        auto hash2 = SipHash::Hash64("", 0);
        REQUIRE(hash == hash2);
    }

    SECTION("Known SipHash-2-4 Test Vector")
    {
        // Official SipHash-2-4 test vector:
        // Key: k0 = 0x0706050403020100, k1 = 0x0F0E0D0C0B0A0908
        // Message: 0x00 0x01 0x02 ... 0x0E (15 bytes)
        // Expected: 0xA129CA6149BE45E5
        SipHashKey key = { 0x0706050403020100ULL, 0x0F0E0D0C0B0A0908ULL };
        const char message[] = { '\x00', '\x01', '\x02', '\x03', '\x04', '\x05', '\x06', '\x07',
                                 '\x08', '\x09', '\x0A', '\x0B', '\x0C', '\x0D', '\x0E' };

        auto hash = SipHash::Hash64(message, 15, key);
        REQUIRE(hash == 0xA129CA6149BE45E5ULL);
    }

    SECTION("Different Keys Produce Different Hashes")
    {
        const char* str = "key test";
        SipHashKey key1 = { 0x0123456789ABCDEFULL, 0xFEDCBA9876543210ULL };
        SipHashKey key2 = { 0x1111111111111111ULL, 0x2222222222222222ULL };

        auto hash1 = SipHash::Hash64(str, std::char_traits<char>::length(str), key1);
        auto hash2 = SipHash::Hash64(str, std::char_traits<char>::length(str), key2);
        REQUIRE(hash1 != hash2);
    }

    SECTION("SipHash-1-3 and SipHash-4-8 Differ from SipHash-2-4")
    {
        const char* str = "variant test";
        SipHashKey key = { 0x0706050403020100ULL, 0x0F0E0D0C0B0A0908ULL };

        auto hash24 = SipHash::Hash64(str, std::char_traits<char>::length(str), key);
        auto hash13 = SipHash::Hash64Fast(str, std::char_traits<char>::length(str), key);
        auto hash48 = SipHash::Hash64Strong(str, std::char_traits<char>::length(str), key);

        REQUIRE(hash24 != hash13);
        REQUIRE(hash24 != hash48);
        REQUIRE(hash13 != hash48);
    }

    SECTION("Different Overloads Consistency")
    {
        const char* str = "overload test";
        std::string_view sv(str);

        REQUIRE(SipHash::Hash64(str) == SipHash::Hash64(sv));
        REQUIRE(SipHash::Hash64(str, std::char_traits<char>::length(str)) == SipHash::Hash64(str));
        REQUIRE(
            SipHash::Hash64(static_cast<const void*>(str), std::char_traits<char>::length(str)) == SipHash::Hash64(str)
        );

        REQUIRE(SipHash::Hash(str) == SipHash::Hash(sv));
        REQUIRE(SipHash::Hash(str, std::char_traits<char>::length(str)) == SipHash::Hash(str));
        REQUIRE(
            SipHash::Hash(static_cast<const void*>(str), std::char_traits<char>::length(str)) == SipHash::Hash(str)
        );
    }

    SECTION("Hash64Fast Overloads Consistency")
    {
        const char* str = "fast overload";
        SipHashKey key = { 0ULL, 0ULL };

        auto hash1 = SipHash::Hash64Fast(str, std::char_traits<char>::length(str), key);
        auto hash2 = SipHash::Hash64Fast(static_cast<const void*>(str), std::char_traits<char>::length(str), key);
        REQUIRE(hash1 == hash2);
    }

    SECTION("Hash64Strong Overloads Consistency")
    {
        const char* str = "strong overload";
        SipHashKey key = { 0ULL, 0ULL };

        auto hash1 = SipHash::Hash64Strong(str, std::char_traits<char>::length(str), key);
        auto hash2 = SipHash::Hash64Strong(static_cast<const void*>(str), std::char_traits<char>::length(str), key);
        REQUIRE(hash1 == hash2);
    }

    SECTION("User-Defined Literal")
    {
        auto hash1 = "test"_siphash;
        auto hash2 = SipHash::Hash64("test");
        REQUIRE(hash1 == hash2);
    }

    SECTION("Compile-time Evaluation")
    {
        constexpr auto hash = SipHash::Hash64("constexpr", 9);
        REQUIRE(hash == SipHash::Hash64("constexpr"));
    }

    SECTION("Deterministic Output")
    {
        const char* input = "deterministic";
        auto hash1 = SipHash::Hash64(input);
        auto hash2 = SipHash::Hash64(input);
        REQUIRE(hash1 == hash2);
    }

    SECTION("Short Strings")
    {
        REQUIRE(SipHash::Hash64("a") != SipHash::Hash64("b"));
        REQUIRE(SipHash::Hash64("ab") != SipHash::Hash64("ba"));
    }

    SECTION("Long Strings")
    {
        std::string longStr(1000, 'z');
        auto hash = SipHash::Hash64(longStr.data(), longStr.size());
        REQUIRE(hash != 0);
    }

    SECTION("Hash64Fast Null-Terminated and StringView Overloads")
    {
        const char* str = "fast string";
        std::string_view sv(str);
        SipHashKey key = { 0x1111ULL, 0x2222ULL };

        auto h1 = SipHash::Hash64Fast(str, std::char_traits<char>::length(str), key);
        auto h2 = SipHash::Hash64Fast(sv, key);
        REQUIRE(h1 == h2);
        REQUIRE(h1 == SipHash::Hash64Fast(static_cast<const void*>(str), std::char_traits<char>::length(str), key));
    }

    SECTION("Hash64Strong Null-Terminated and StringView Overloads")
    {
        const char* str = "strong string";
        std::string_view sv(str);
        SipHashKey key = { 0xAAAAULL, 0xBBBBULL };

        auto h1 = SipHash::Hash64Strong(str, std::char_traits<char>::length(str), key);
        auto h2 = SipHash::Hash64Strong(sv, key);
        REQUIRE(h1 == h2);
        REQUIRE(h1 == SipHash::Hash64Strong(static_cast<const void*>(str), std::char_traits<char>::length(str), key));
    }

    SECTION("Multi-Block Input (> 8 bytes) Consistency")
    {
        // Exercises the full-block compression path in SipHashCore.
        const char* str = "sixteen bytes!!";
        SipHashKey key = { 0x0706050403020100ULL, 0x0F0E0D0C0B0A0908ULL };
        auto h1 = SipHash::Hash64(str, 16, key);
        auto h2 = SipHash::Hash64(str, 16, key);
        REQUIRE(h1 == h2);
        REQUIRE(h1 != SipHash::Hash64(str, 15, key));
        REQUIRE(h1 != SipHash::Hash64("seventeen bytes!!", 17, key));
    }

    SECTION("Default Key Overloads Use Zero Key")
    {
        const char* str = "default key";
        SipHashKey zeroKey = { 0ULL, 0ULL };
        REQUIRE(SipHash::Hash64(str) == SipHash::Hash64(str, std::char_traits<char>::length(str), zeroKey));
        REQUIRE(
            SipHash::Hash64Fast(str, std::char_traits<char>::length(str)) ==
            SipHash::Hash64Fast(str, std::char_traits<char>::length(str), zeroKey)
        );
        REQUIRE(
            SipHash::Hash64Strong(str, std::char_traits<char>::length(str)) ==
            SipHash::Hash64Strong(str, std::char_traits<char>::length(str), zeroKey)
        );
    }
}
