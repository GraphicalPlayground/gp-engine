// Copyright (c) - Graphical Playground. All rights reserved.

#include "Crypto/Hash/SHA256.hpp"
#include <catch2/catch_test_macros.hpp>
#include <string>
#include <string_view>

TEST_CASE("SHA256 Hashing", "[GP][Core][Crypto][Hash][SHA256]")
{
    using namespace GP;
    using namespace GP::Crypto;

    SECTION("DIGEST_SIZE and BLOCK_SIZE Constants")
    {
        REQUIRE(SHA256::DIGEST_SIZE == 32);
        REQUIRE(SHA256::BLOCK_SIZE == 64);
        REQUIRE(sizeof(SHA256Digest::bytes) == 32);
    }

    SECTION("FIPS 180-4 Empty String Vector")
    {
        // SHA256("") = e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855
        constexpr auto d = SHA256::Hash("", 0);
        constexpr UInt8 kExpected[32] = { 0xE3, 0xB0, 0xC4, 0x42, 0x98, 0xFC, 0x1C, 0x14, 0x9A, 0xFB, 0xF4,
                                          0xC8, 0x99, 0x6F, 0xB9, 0x24, 0x27, 0xAE, 0x41, 0xE4, 0x64, 0x9B,
                                          0x93, 0x4C, 0xA4, 0x95, 0x99, 0x1B, 0x78, 0x52, 0xB8, 0x55 };
        for (SizeT i = 0; i < 32; ++i)
        {
            INFO("Byte " << i);
            REQUIRE(static_cast<UInt8>(d.bytes[i]) == kExpected[i]);
        }
    }

    SECTION("FIPS 180-4 'abc' Vector")
    {
        // SHA256("abc") = ba7816bf8f01cfea414140de5dae2ec73b00361bbef0469348423f656b6f3b8a
        constexpr auto d = SHA256::Hash("abc", 3);
        constexpr UInt8 kExpected[32] = { 0xBA, 0x78, 0x16, 0xBF, 0x8F, 0x01, 0xCF, 0xEA, 0x41, 0x41, 0x40,
                                          0xDE, 0x5D, 0xAE, 0x2E, 0xC7, 0x3B, 0x00, 0x36, 0x1B, 0xBE, 0xF0,
                                          0x46, 0x93, 0x48, 0x42, 0x3F, 0x65, 0x6B, 0x6F, 0x3B, 0x8A };
        for (SizeT i = 0; i < 32; ++i)
        {
            INFO("Byte " << i);
            REQUIRE(static_cast<UInt8>(d.bytes[i]) == kExpected[i]);
        }
    }

    SECTION("FIPS 180-4 56-Byte Vector (Two-Block Padding Path)")
    {
        // "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq" (56 bytes)
        // SHA256 = 248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1
        // 56 bytes of remaining data forces the two-block (128-byte) padding path.
        constexpr auto d = SHA256::Hash("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 56);
        constexpr UInt8 kExpected[32] = { 0x24, 0x8D, 0x6A, 0x61, 0xD2, 0x06, 0x38, 0xB8, 0xE5, 0xC0, 0x26,
                                          0x93, 0x0C, 0x3E, 0x60, 0x39, 0xA3, 0x3C, 0xE4, 0x59, 0x64, 0xFF,
                                          0x21, 0x67, 0xF6, 0xEC, 0xED, 0xD4, 0x19, 0xDB, 0x06, 0xC1 };
        for (SizeT i = 0; i < 32; ++i)
        {
            INFO("Byte " << i);
            REQUIRE(static_cast<UInt8>(d.bytes[i]) == kExpected[i]);
        }
    }

    SECTION("Void Pointer Overload Matches char* Overload")
    {
        const char* str = "hello world";
        auto d1 = SHA256::Hash(str, 11);
        auto d2 = SHA256::Hash(static_cast<const void*>(str), 11);
        REQUIRE(d1 == d2);
    }

    SECTION("Null-Terminated C-String Overload Matches Length Overload")
    {
        constexpr auto d1 = SHA256::Hash("abc", 3);
        constexpr auto d2 = SHA256::Hash("abc");
        REQUIRE(d1 == d2);
    }

    SECTION("String View Overload Matches Length Overload")
    {
        constexpr auto d1 = SHA256::Hash("abc", 3);
        std::string_view sv("abc", 3);
        auto d2 = SHA256::Hash(sv);
        REQUIRE(d1 == d2);
    }

    SECTION("Single-Byte Sensitivity")
    {
        constexpr auto d0 = SHA256::Hash("aaaaaaaaaa", 10);
        constexpr auto d1 = SHA256::Hash("aaaaaaaaab", 10);
        REQUIRE(d0 != d1);
    }

    SECTION("Determinism")
    {
        auto d1 = SHA256::Hash("determinism", 11);
        auto d2 = SHA256::Hash("determinism", 11);
        REQUIRE(d1 == d2);
    }

    SECTION("Verify Matching Digest Returns True")
    {
        constexpr auto d = SHA256::Hash("verify test", 11);
        REQUIRE(SHA256::Verify("verify test", 11, d));
    }

    SECTION("Verify Mismatched Input Returns False")
    {
        constexpr auto d = SHA256::Hash("verify test", 11);
        REQUIRE_FALSE(SHA256::Verify("verify teSt", 11, d));
    }

    SECTION("Verify Void Pointer Overload")
    {
        const char* str = "verify";
        constexpr auto d = SHA256::Hash("verify", 6);
        REQUIRE(SHA256::Verify(static_cast<const void*>(str), 6, d));
    }

    SECTION("Streaming Context Matches One-Shot - Simple")
    {
        SHA256::Context ctx;
        ctx.Update("a", 1);
        ctx.Update("bc", 2);
        auto streamResult = ctx.Finalize();

        constexpr auto oneShotResult = SHA256::Hash("abc", 3);
        REQUIRE(streamResult == oneShotResult);
    }

    SECTION("Streaming Context Matches One-Shot - Across Block Boundary")
    {
        // Feed 128 bytes (two SHA-256 blocks) split into non-aligned chunks.
        std::string input(128, 'q');
        SHA256::Context ctx;
        ctx.Update(input.c_str(), 50);
        ctx.Update(input.c_str() + 50, 50);
        ctx.Update(input.c_str() + 100, 28);
        auto streamResult = ctx.Finalize();
        auto oneShotResult = SHA256::Hash(input.c_str(), 128);
        REQUIRE(streamResult == oneShotResult);
    }

    SECTION("Streaming Context Matches One-Shot - 56-Byte Two-Block Padding")
    {
        // Verify streaming produces the same result as one-shot for the two-block padding case.
        const char* str56 = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
        SHA256::Context ctx;
        ctx.Update(str56, 28);
        ctx.Update(str56 + 28, 28);
        auto streamResult = ctx.Finalize();
        auto oneShotResult = SHA256::Hash(str56, 56);
        REQUIRE(streamResult == oneShotResult);
    }

    SECTION("Compile-Time Evaluation")
    {
        // SHA256("abc") = ba7816bf...6b6f3b8a
        constexpr auto d = SHA256::Hash("abc", 3);
        static_assert(static_cast<UInt8>(d.bytes[0]) == 0xBA, "SHA256 compile-time evaluation failed");
        REQUIRE(static_cast<UInt8>(d.bytes[0]) == 0xBA);
        REQUIRE(static_cast<UInt8>(d.bytes[31]) == 0x8A);
    }
}
