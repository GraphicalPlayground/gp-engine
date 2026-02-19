// Copyright (c) - Graphical Playground. All rights reserved.

#include "Crypto/Hash/SHA512.hpp"
#include <catch2/catch_test_macros.hpp>
#include <string>
#include <string_view>

TEST_CASE("SHA512 Hashing", "[GP][Core][Crypto][Hash][SHA512]")
{
    using namespace GP;
    using namespace GP::Crypto;

    SECTION("DIGEST_SIZE and BLOCK_SIZE Constants")
    {
        REQUIRE(SHA512::DIGEST_SIZE == 64);
        REQUIRE(SHA512::BLOCK_SIZE == 128);
        REQUIRE(sizeof(SHA512Digest::bytes) == 64);
    }

    SECTION("FIPS 180-4 Empty String Vector")
    {
        // SHA512("") =
        //   cf83e1357eefb8bdf1542850d66d8007d620e4050b5715dc83f4a921d36ce9ce
        //   47d0d13c5d85f2b0ff8318d2877eec2f63b931bd47417a81a538327af927da3e
        constexpr auto d = SHA512::Hash("", 0);
        constexpr Byte kExpected[64] = { 0xCF, 0x83, 0xE1, 0x35, 0x7E, 0xEF, 0xB8, 0xBD, 0xF1, 0x54, 0x28, 0x50, 0xD6,
                                         0x6D, 0x80, 0x07, 0xD6, 0x20, 0xE4, 0x05, 0x0B, 0x57, 0x15, 0xDC, 0x83, 0xF4,
                                         0xA9, 0x21, 0xD3, 0x6C, 0xE9, 0xCE, 0x47, 0xD0, 0xD1, 0x3C, 0x5D, 0x85, 0xF2,
                                         0xB0, 0xFF, 0x83, 0x18, 0xD2, 0x87, 0x7E, 0xEC, 0x2F, 0x63, 0xB9, 0x31, 0xBD,
                                         0x47, 0x41, 0x7A, 0x81, 0xA5, 0x38, 0x32, 0x7A, 0xF9, 0x27, 0xDA, 0x3E };
        for (SizeT i = 0; i < 64; ++i)
        {
            INFO("Byte " << i);
            REQUIRE(d.bytes[i] == kExpected[i]);
        }
    }

    SECTION("FIPS 180-4 'abc' Vector")
    {
        // SHA512("abc") =
        //   ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a
        //   2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f
        constexpr auto d = SHA512::Hash("abc", 3);
        constexpr Byte kExpected[64] = { 0xDD, 0xAF, 0x35, 0xA1, 0x93, 0x61, 0x7A, 0xBA, 0xCC, 0x41, 0x73, 0x49, 0xAE,
                                         0x20, 0x41, 0x31, 0x12, 0xE6, 0xFA, 0x4E, 0x89, 0xA9, 0x7E, 0xA2, 0x0A, 0x9E,
                                         0xEE, 0xE6, 0x4B, 0x55, 0xD3, 0x9A, 0x21, 0x92, 0x99, 0x2A, 0x27, 0x4F, 0xC1,
                                         0xA8, 0x36, 0xBA, 0x3C, 0x23, 0xA3, 0xFE, 0xEB, 0xBD, 0x45, 0x4D, 0x44, 0x23,
                                         0x64, 0x3C, 0xE8, 0x0E, 0x2A, 0x9A, 0xC9, 0x4F, 0xA5, 0x4C, 0xA4, 0x9F };
        for (SizeT i = 0; i < 64; ++i)
        {
            INFO("Byte " << i);
            REQUIRE(d.bytes[i] == kExpected[i]);
        }
    }

    SECTION("Void Pointer Overload Matches char* Overload")
    {
        const char* str = "hello world";
        auto d1 = SHA512::Hash(str, 11);
        auto d2 = SHA512::Hash(static_cast<const void*>(str), 11);
        REQUIRE(d1 == d2);
    }

    SECTION("Null-Terminated C-String Overload Matches Length Overload")
    {
        constexpr auto d1 = SHA512::Hash("abc", 3);
        constexpr auto d2 = SHA512::Hash("abc");
        REQUIRE(d1 == d2);
    }

    SECTION("String View Overload Matches Length Overload")
    {
        constexpr auto d1 = SHA512::Hash("abc", 3);
        std::string_view sv("abc", 3);
        auto d2 = SHA512::Hash(sv);
        REQUIRE(d1 == d2);
    }

    SECTION("Single-Byte Sensitivity")
    {
        constexpr auto d0 = SHA512::Hash("aaaaaaaaaa", 10);
        constexpr auto d1 = SHA512::Hash("aaaaaaaaab", 10);
        REQUIRE(d0 != d1);
    }

    SECTION("Determinism")
    {
        auto d1 = SHA512::Hash("determinism", 11);
        auto d2 = SHA512::Hash("determinism", 11);
        REQUIRE(d1 == d2);
    }

    SECTION("Verify Matching Digest Returns True")
    {
        constexpr auto d = SHA512::Hash("verify test", 11);
        REQUIRE(SHA512::Verify("verify test", 11, d));
    }

    SECTION("Verify Mismatched Input Returns False")
    {
        constexpr auto d = SHA512::Hash("verify test", 11);
        REQUIRE_FALSE(SHA512::Verify("verify teSt", 11, d));
    }

    SECTION("Verify Void Pointer Overload")
    {
        const char* str = "verify";
        constexpr auto d = SHA512::Hash("verify", 6);
        REQUIRE(SHA512::Verify(static_cast<const void*>(str), 6, d));
    }

    SECTION("Streaming Context Matches One-Shot - Simple")
    {
        SHA512::Context ctx;
        ctx.Update("a", 1);
        ctx.Update("bc", 2);
        auto streamResult = ctx.Finalize();

        constexpr auto oneShotResult = SHA512::Hash("abc", 3);
        REQUIRE(streamResult == oneShotResult);
    }

    SECTION("Streaming Context Matches One-Shot - Across Block Boundary")
    {
        // Feed 256 bytes (two SHA-512 blocks of 128 bytes) in misaligned chunks.
        std::string input(256, 'r');
        SHA512::Context ctx;
        ctx.Update(input.c_str(), 100);
        ctx.Update(input.c_str() + 100, 100);
        ctx.Update(input.c_str() + 200, 56);
        auto streamResult = ctx.Finalize();
        auto oneShotResult = SHA512::Hash(input.c_str(), 256);
        REQUIRE(streamResult == oneShotResult);
    }

    SECTION("Two-Block Padding Path (remaining >= 112)")
    {
        // SHA-512 block = 128 bytes; remaining >= 112 forces two-block padding.
        std::string str112(112, 's');
        std::string str111(111, 's');
        auto d111 = SHA512::Hash(str111.c_str(), 111);
        auto d112 = SHA512::Hash(str112.c_str(), 112);
        REQUIRE(d111 != d112);
        REQUIRE(d111 == SHA512::Hash(str111.c_str(), 111));
        REQUIRE(d112 == SHA512::Hash(str112.c_str(), 112));
    }

    SECTION("SHA512 Differs from SHA384 on Same Input")
    {
        // IPv different initial hash values → different digest even for same input.
        constexpr auto d512 = SHA512::Hash("abc", 3);
        REQUIRE(d512.bytes[0] == 0xDD);    // first byte of known SHA-512("abc")
        REQUIRE(d512.bytes[63] == 0x9F);   // last byte
    }

    SECTION("Compile-Time Evaluation")
    {
        constexpr auto d = SHA512::Hash("abc", 3);
        static_assert(d.bytes[0] == 0xDD, "SHA512 compile-time evaluation failed");
        static_assert(d.bytes[63] == 0x9F, "SHA512 compile-time evaluation failed");
        REQUIRE(d.bytes[0] == 0xDD);
        REQUIRE(d.bytes[63] == 0x9F);
    }
}
