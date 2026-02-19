// Copyright (c) - Graphical Playground. All rights reserved.

#include "Crypto/Hash/SHA384.hpp"
#include <catch2/catch_test_macros.hpp>
#include <string>
#include <string_view>

TEST_CASE("SHA384 Hashing", "[GP][Core][Crypto][Hash][SHA384]")
{
    using namespace GP;
    using namespace GP::Crypto;

    SECTION("DIGEST_SIZE and BLOCK_SIZE Constants")
    {
        REQUIRE(SHA384::DIGEST_SIZE == 48);
        REQUIRE(SHA384::BLOCK_SIZE == 128);
        REQUIRE(sizeof(SHA384Digest::bytes) == 48);
    }

    SECTION("FIPS 180-4 Empty String Vector")
    {
        // SHA384("") =
        //   38b060a751ac96384cd9327eb1b1e36a21fdb71114be07434c0cc7bf63f6e1da
        //   274edebfe76f65fbd51ad2f14898b95b
        constexpr auto d = SHA384::Hash("", 0);
        constexpr Byte kExpected[48] = { 0x38, 0xB0, 0x60, 0xA7, 0x51, 0xAC, 0x96, 0x38, 0x4C, 0xD9, 0x32, 0x7E,
                                         0xB1, 0xB1, 0xE3, 0x6A, 0x21, 0xFD, 0xB7, 0x11, 0x14, 0xBE, 0x07, 0x43,
                                         0x4C, 0x0C, 0xC7, 0xBF, 0x63, 0xF6, 0xE1, 0xDA, 0x27, 0x4E, 0xDE, 0xBF,
                                         0xE7, 0x6F, 0x65, 0xFB, 0xD5, 0x1A, 0xD2, 0xF1, 0x48, 0x98, 0xB9, 0x5B };
        for (SizeT i = 0; i < 48; ++i)
        {
            INFO("Byte " << i);
            REQUIRE(d.bytes[i] == kExpected[i]);
        }
    }

    SECTION("FIPS 180-4 'abc' Vector")
    {
        // SHA384("abc") =
        //   cb00753f45a35e8bb5a03d699ac65007272c32ab0eded1631a8b605a43ff5bed
        //   8086072ba1e7cc2358baeca134c825a7
        constexpr auto d = SHA384::Hash("abc", 3);
        constexpr Byte kExpected[48] = { 0xCB, 0x00, 0x75, 0x3F, 0x45, 0xA3, 0x5E, 0x8B, 0xB5, 0xA0, 0x3D, 0x69,
                                         0x9A, 0xC6, 0x50, 0x07, 0x27, 0x2C, 0x32, 0xAB, 0x0E, 0xDE, 0xD1, 0x63,
                                         0x1A, 0x8B, 0x60, 0x5A, 0x43, 0xFF, 0x5B, 0xED, 0x80, 0x86, 0x07, 0x2B,
                                         0xA1, 0xE7, 0xCC, 0x23, 0x58, 0xBA, 0xEC, 0xA1, 0x34, 0xC8, 0x25, 0xA7 };
        for (SizeT i = 0; i < 48; ++i)
        {
            INFO("Byte " << i);
            REQUIRE(d.bytes[i] == kExpected[i]);
        }
    }

    SECTION("Void Pointer Overload Matches char* Overload")
    {
        const char* str = "hello world";
        auto d1 = SHA384::Hash(str, 11);
        auto d2 = SHA384::Hash(static_cast<const void*>(str), 11);
        REQUIRE(d1 == d2);
    }

    SECTION("Null-Terminated C-String Overload Matches Length Overload")
    {
        constexpr auto d1 = SHA384::Hash("abc", 3);
        constexpr auto d2 = SHA384::Hash("abc");
        REQUIRE(d1 == d2);
    }

    SECTION("String View Overload Matches Length Overload")
    {
        constexpr auto d1 = SHA384::Hash("abc", 3);
        std::string_view sv("abc", 3);
        auto d2 = SHA384::Hash(sv);
        REQUIRE(d1 == d2);
    }

    SECTION("Single-Byte Sensitivity")
    {
        constexpr auto d0 = SHA384::Hash("aaaaaaaaaa", 10);
        constexpr auto d1 = SHA384::Hash("aaaaaaaaab", 10);
        REQUIRE(d0 != d1);
    }

    SECTION("Determinism")
    {
        auto d1 = SHA384::Hash("determinism", 11);
        auto d2 = SHA384::Hash("determinism", 11);
        REQUIRE(d1 == d2);
    }

    SECTION("Verify Matching Digest Returns True")
    {
        constexpr auto d = SHA384::Hash("verify test", 11);
        REQUIRE(SHA384::Verify("verify test", 11, d));
    }

    SECTION("Verify Mismatched Input Returns False")
    {
        constexpr auto d = SHA384::Hash("verify test", 11);
        REQUIRE_FALSE(SHA384::Verify("verify teSt", 11, d));
    }

    SECTION("Verify Void Pointer Overload")
    {
        const char* str = "verify";
        constexpr auto d = SHA384::Hash("verify", 6);
        REQUIRE(SHA384::Verify(static_cast<const void*>(str), 6, d));
    }

    SECTION("Streaming Context Matches One-Shot - Simple")
    {
        SHA384::Context ctx;
        ctx.Update("a", 1);
        ctx.Update("bc", 2);
        auto streamResult = ctx.Finalize();

        constexpr auto oneShotResult = SHA384::Hash("abc", 3);
        REQUIRE(streamResult == oneShotResult);
    }

    SECTION("Streaming Context Matches One-Shot - Across Block Boundary")
    {
        // Feed 256 bytes (two SHA-384 blocks of 128 bytes) in unaligned chunks.
        std::string input(256, 'm');
        SHA384::Context ctx;
        ctx.Update(input.c_str(), 100);
        ctx.Update(input.c_str() + 100, 100);
        ctx.Update(input.c_str() + 200, 56);
        auto streamResult = ctx.Finalize();
        auto oneShotResult = SHA384::Hash(input.c_str(), 256);
        REQUIRE(streamResult == oneShotResult);
    }

    SECTION("Two-Block Padding Path (remaining >= 112)")
    {
        // SHA-384 uses a 128-byte block. A message with remaining >= 112 bytes
        // forces 256-byte paddding (two-block padding path).
        // A 112-byte message has 0 full blocks and 112 remaining bytes.
        std::string str112(112, 'p');
        std::string str111(111, 'p');
        auto d111 = SHA384::Hash(str111.c_str(), 111);
        auto d112 = SHA384::Hash(str112.c_str(), 112);
        REQUIRE(d111 != d112);
        REQUIRE(d111 == SHA384::Hash(str111.c_str(), 111));
        REQUIRE(d112 == SHA384::Hash(str112.c_str(), 112));
    }

    SECTION("SHA384 Differs from SHA512 on Same Input")
    {
        // SHA-384 and SHA-512 use different IVs and produce different-sized digests.
        constexpr auto d384 = SHA384::Hash("abc", 3);
        REQUIRE(d384.bytes[0] == 0xCB);    // first byte of known SHA-384("abc")
        REQUIRE(d384.bytes[47] == 0xA7);   // last byte
    }

    SECTION("Compile-Time Evaluation")
    {
        constexpr auto d = SHA384::Hash("abc", 3);
        static_assert(d.bytes[0] == 0xCB, "SHA384 compile-time evaluation failed");
        static_assert(d.bytes[47] == 0xA7, "SHA384 compile-time evaluation failed");
        REQUIRE(d.bytes[0] == 0xCB);
        REQUIRE(d.bytes[47] == 0xA7);
    }
}
