// Copyright (c) - Graphical Playground. All rights reserved.

#include "Crypto/Hash/MD5.hpp"
#include <catch2/catch_test_macros.hpp>
#include <string>
#include <string_view>

TEST_CASE("MD5 Hashing", "[GP][Core][Crypto][Hash][MD5]")
{
    using namespace GP;
    using namespace GP::Crypto;

    SECTION("DIGEST_SIZE and BLOCK_SIZE Constants")
    {
        REQUIRE(MD5::DIGEST_SIZE == 16);
        REQUIRE(MD5::BLOCK_SIZE == 64);
        REQUIRE(sizeof(MD5Digest::bytes) == 16);
    }

    SECTION("RFC 1321 Empty String Vector")
    {
        // MD5("") = d41d8cd98f00b204e9800998ecf8427e
        constexpr auto d = MD5::Hash("", 0);
        constexpr UInt8 kExpected[16] = { 0xD4, 0x1D, 0x8C, 0xD9, 0x8F, 0x00, 0xB2, 0x04,
                                          0xE9, 0x80, 0x09, 0x98, 0xEC, 0xF8, 0x42, 0x7E };
        for (SizeT i = 0; i < 16; ++i)
        {
            INFO("Byte " << i);
            REQUIRE(static_cast<UInt8>(d.bytes[i]) == kExpected[i]);
        }
    }

    SECTION("RFC 1321 'a' Vector")
    {
        // MD5("a") = 0cc175b9c0f1b6a831c399e269772661
        constexpr auto d = MD5::Hash("a", 1);
        constexpr UInt8 kExpected[16] = { 0x0C, 0xC1, 0x75, 0xB9, 0xC0, 0xF1, 0xB6, 0xA8,
                                          0x31, 0xC3, 0x99, 0xE2, 0x69, 0x77, 0x26, 0x61 };
        for (SizeT i = 0; i < 16; ++i)
        {
            INFO("Byte " << i);
            REQUIRE(static_cast<UInt8>(d.bytes[i]) == kExpected[i]);
        }
    }

    SECTION("RFC 1321 'abc' Vector")
    {
        // MD5("abc") = 900150983cd24fb0d6963f7d28e17f72
        constexpr auto d = MD5::Hash("abc", 3);
        constexpr UInt8 kExpected[16] = { 0x90, 0x01, 0x50, 0x98, 0x3C, 0xD2, 0x4F, 0xB0,
                                          0xD6, 0x96, 0x3F, 0x7D, 0x28, 0xE1, 0x7F, 0x72 };
        for (SizeT i = 0; i < 16; ++i)
        {
            INFO("Byte " << i);
            REQUIRE(static_cast<UInt8>(d.bytes[i]) == kExpected[i]);
        }
    }

    SECTION("RFC 1321 'message digest' Vector")
    {
        // MD5("message digest") = f96b697d7cb7938d525a2f31aaf161d0
        constexpr auto d = MD5::Hash("message digest", 14);
        constexpr UInt8 kExpected[16] = { 0xF9, 0x6B, 0x69, 0x7D, 0x7C, 0xB7, 0x93, 0x8D,
                                          0x52, 0x5A, 0x2F, 0x31, 0xAA, 0xF1, 0x61, 0xD0 };
        for (SizeT i = 0; i < 16; ++i)
        {
            INFO("Byte " << i);
            REQUIRE(static_cast<UInt8>(d.bytes[i]) == kExpected[i]);
        }
    }

    SECTION("Void Pointer Overload Matches char* Overload")
    {
        const char* str = "test input";
        const SizeT len = 10;
        auto d1 = MD5::Hash(str, len);
        auto d2 = MD5::Hash(static_cast<const void*>(str), len);
        REQUIRE(d1 == d2);
    }

    SECTION("Null-Terminated C-String Overload Matches Length Overload")
    {
        constexpr auto d1 = MD5::Hash("abc", 3);
        constexpr auto d2 = MD5::Hash("abc");
        REQUIRE(d1 == d2);
    }

    SECTION("String View Overload Matches Length Overload")
    {
        constexpr auto d1 = MD5::Hash("abc", 3);
        std::string_view sv("abc", 3);
        auto d2 = MD5::Hash(sv);
        REQUIRE(d1 == d2);
    }

    SECTION("Single-Byte Sensitivity")
    {
        // Flipping one byte must produce a different digest.
        constexpr auto d0 = MD5::Hash("aaaaaaaaa", 9);
        constexpr auto d1 = MD5::Hash("aaaaaaaab", 9);
        REQUIRE(d0 != d1);
    }

    SECTION("Determinism")
    {
        auto d1 = MD5::Hash("determinism", 11);
        auto d2 = MD5::Hash("determinism", 11);
        REQUIRE(d1 == d2);
    }

    SECTION("Verify Matching Digest Returns True")
    {
        constexpr auto d = MD5::Hash("verify test", 11);
        REQUIRE(MD5::Verify("verify test", 11, d));
    }

    SECTION("Verify Mismatched Input Returns False")
    {
        constexpr auto d = MD5::Hash("verify test", 11);
        REQUIRE_FALSE(MD5::Verify("verify teSt", 11, d));
    }

    SECTION("Verify Void Pointer Overload")
    {
        const char* str = "verify";
        constexpr auto d = MD5::Hash("verify", 6);
        REQUIRE(MD5::Verify(static_cast<const void*>(str), 6, d));
    }

    SECTION("Streaming Context Matches One-Shot - Simple")
    {
        // Split "abc" arbitrarily.
        MD5::Context ctx;
        ctx.Update("a", 1);
        ctx.Update("bc", 2);
        auto streamResult = ctx.Finalize();

        constexpr auto oneShotResult = MD5::Hash("abc", 3);
        REQUIRE(streamResult == oneShotResult);
    }

    SECTION("Streaming Context Matches One-Shot - Across Block Boundary")
    {
        // Feed 64 bytes (one full MD5 block) split at the boundary.
        std::string input(64, 'x');
        MD5::Context ctx;
        ctx.Update(input.c_str(), 32);
        ctx.Update(input.c_str() + 32, 32);
        auto streamResult = ctx.Finalize();
        auto oneShotResult = MD5::Hash(input.c_str(), 64);
        REQUIRE(streamResult == oneShotResult);
    }

    SECTION("Two-Block Padding Path (remaining >= 56)")
    {
        // A message with 56-byte remainder triggers the two-block padding path.
        // A 56-byte message has 0 full blocks and 56 remaining bytes.
        const char* str56 = "12345678901234567890123456789012345678901234567890123456";
        auto d55 = MD5::Hash(str56, 55);
        auto d56 = MD5::Hash(str56, 56);
        // Both must be deterministic and distinct.
        REQUIRE(d55 != d56);
        REQUIRE(d55 == MD5::Hash(str56, 55));
        REQUIRE(d56 == MD5::Hash(str56, 56));
    }

    SECTION("Compile-Time Evaluation")
    {
        // MD5("abc") = 900150983cd24fb0d6963f7d28e17f72
        constexpr auto d = MD5::Hash("abc", 3);
        static_assert(static_cast<UInt8>(d.bytes[0]) == 0x90, "MD5 compile-time evaluation failed");
        REQUIRE(static_cast<UInt8>(d.bytes[0]) == 0x90);
        REQUIRE(static_cast<UInt8>(d.bytes[15]) == 0x72);
    }
}
