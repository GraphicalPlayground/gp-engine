// Copyright (c) - Graphical Playground. All rights reserved.

// SHA-1 is marked deprecated in the engine. Suppress the warning for testing purposes.
#if defined(__GNUC__) || defined(__clang__)
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#elif defined(_MSC_VER)
    #pragma warning(push)
    #pragma warning(disable : 4996)
#endif

#include "Crypto/Hash/SHA1.hpp"
#include <catch2/catch_test_macros.hpp>
#include <string>
#include <string_view>

TEST_CASE("SHA1 Hashing", "[GP][Core][Crypto][Hash][SHA1]")
{
    using namespace GP;
    using namespace GP::Crypto;

    SECTION("DIGEST_SIZE and BLOCK_SIZE Constants")
    {
        REQUIRE(SHA1::DIGEST_SIZE == 20);
        REQUIRE(SHA1::BLOCK_SIZE == 64);
        REQUIRE(sizeof(SHA1Digest::bytes) == 20);
    }

    SECTION("FIPS 180-4 Empty String Vector")
    {
        // SHA1("") = da39a3ee5e6b4b0d3255bfef95601890afd80709
        constexpr auto d = SHA1::Hash("", 0);
        constexpr Byte kExpected[20] = { 0xDA, 0x39, 0xA3, 0xEE, 0x5E, 0x6B, 0x4B, 0x0D, 0x32, 0x55,
                                         0xBF, 0xEF, 0x95, 0x60, 0x18, 0x90, 0xAF, 0xD8, 0x07, 0x09 };
        for (SizeT i = 0; i < 20; ++i)
        {
            INFO("Byte " << i);
            REQUIRE(d.bytes[i] == kExpected[i]);
        }
    }

    SECTION("FIPS 180-4 'abc' Vector")
    {
        // SHA1("abc") = a9993e364706816aba3e25717850c26c9cd0d89d
        constexpr auto d = SHA1::Hash("abc", 3);
        constexpr Byte kExpected[20] = { 0xA9, 0x99, 0x3E, 0x36, 0x47, 0x06, 0x81, 0x6A, 0xBA, 0x3E,
                                         0x25, 0x71, 0x78, 0x50, 0xC2, 0x6C, 0x9C, 0xD0, 0xD8, 0x9D };
        for (SizeT i = 0; i < 20; ++i)
        {
            INFO("Byte " << i);
            REQUIRE(d.bytes[i] == kExpected[i]);
        }
    }

    SECTION("FIPS 180-4 448-bit Message Vector (Two-Block Padding)")
    {
        // "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq" (56 bytes)
        // SHA1 = 84983e441c3bd26ebaae4aa1f95129e5e54670f1
        constexpr auto d = SHA1::Hash("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 56);
        constexpr Byte kExpected[20] = { 0x84, 0x98, 0x3E, 0x44, 0x1C, 0x3B, 0xD2, 0x6E, 0xBA, 0xAE,
                                         0x4A, 0xA1, 0xF9, 0x51, 0x29, 0xE5, 0xE5, 0x46, 0x70, 0xF1 };
        for (SizeT i = 0; i < 20; ++i)
        {
            INFO("Byte " << i);
            REQUIRE(d.bytes[i] == kExpected[i]);
        }
    }

    SECTION("Void Pointer Overload Matches char* Overload")
    {
        const char* str = "hello";
        auto d1 = SHA1::Hash(str, 5);
        auto d2 = SHA1::Hash(static_cast<const void*>(str), 5);
        REQUIRE(d1 == d2);
    }

    SECTION("Null-Terminated C-String Overload Matches Length Overload")
    {
        constexpr auto d1 = SHA1::Hash("abc", 3);
        constexpr auto d2 = SHA1::Hash("abc");
        REQUIRE(d1 == d2);
    }

    SECTION("String View Overload Matches Length Overload")
    {
        constexpr auto d1 = SHA1::Hash("abc", 3);
        std::string_view sv("abc", 3);
        auto d2 = SHA1::Hash(sv);
        REQUIRE(d1 == d2);
    }

    SECTION("Single-Byte Sensitivity")
    {
        constexpr auto d0 = SHA1::Hash("aaaaaaaaa", 9);
        constexpr auto d1 = SHA1::Hash("aaaaaaaab", 9);
        REQUIRE(d0 != d1);
    }

    SECTION("Determinism")
    {
        auto d1 = SHA1::Hash("determinism", 11);
        auto d2 = SHA1::Hash("determinism", 11);
        REQUIRE(d1 == d2);
    }

    SECTION("Verify Matching Digest Returns True")
    {
        constexpr auto d = SHA1::Hash("verify test", 11);
        REQUIRE(SHA1::Verify("verify test", 11, d));
    }

    SECTION("Verify Mismatched Input Returns False")
    {
        constexpr auto d = SHA1::Hash("verify test", 11);
        REQUIRE_FALSE(SHA1::Verify("verify teSt", 11, d));
    }

    SECTION("Verify Void Pointer Overload")
    {
        const char* str = "verify";
        constexpr auto d = SHA1::Hash("verify", 6);
        REQUIRE(SHA1::Verify(static_cast<const void*>(str), 6, d));
    }

    SECTION("Streaming Context Matches One-Shot - Simple")
    {
        SHA1::Context ctx;
        ctx.Update("a", 1);
        ctx.Update("bc", 2);
        auto streamResult = ctx.Finalize();

        constexpr auto oneShotResult = SHA1::Hash("abc", 3);
        REQUIRE(streamResult == oneShotResult);
    }

    SECTION("Streaming Context Matches One-Shot - Across Block Boundary")
    {
        // Feed 128 bytes (two full SHA-1 blocks) split at various points.
        std::string input(128, 'z');
        SHA1::Context ctx;
        ctx.Update(input.c_str(), 64);
        ctx.Update(input.c_str() + 64, 64);
        auto streamResult = ctx.Finalize();
        auto oneShotResult = SHA1::Hash(input.c_str(), 128);
        REQUIRE(streamResult == oneShotResult);
    }

    SECTION("Streaming Context Matches One-Shot - Partial Fill")
    {
        // Feed data in chunks smaller than the buffer to exercise the partial-fill paths.
        SHA1::Context ctx;
        ctx.Update("abc", 3);
        ctx.Update("defg", 4);
        ctx.Update("hi", 2);
        auto streamResult = ctx.Finalize();
        auto oneShotResult = SHA1::Hash("abcdefghi", 9);
        REQUIRE(streamResult == oneShotResult);
    }

    SECTION("Compile-Time Evaluation")
    {
        // SHA1("abc") = a9993e364706816aba3e25717850c26c9cd0d89d
        constexpr auto d = SHA1::Hash("abc", 3);
        static_assert(d.bytes[0] == 0xA9, "SHA1 compile-time evaluation failed");
        static_assert(d.bytes[19] == 0x9D, "SHA1 compile-time evaluation failed");
        REQUIRE(d.bytes[0] == 0xA9);
        REQUIRE(d.bytes[19] == 0x9D);
    }
}

#if defined(__GNUC__) || defined(__clang__)
    #pragma GCC diagnostic pop
#elif defined(_MSC_VER)
    #pragma warning(pop)
#endif
