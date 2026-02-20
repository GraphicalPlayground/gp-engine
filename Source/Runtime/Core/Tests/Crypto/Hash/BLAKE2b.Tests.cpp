// Copyright (c) - Graphical Playground. All rights reserved.

#include "Crypto/Hash/BLAKE2b.hpp"
#include <catch2/catch_test_macros.hpp>
#include <string>
#include <string_view>

TEST_CASE("BLAKE2b Hashing", "[GP][Core][Crypto][Hash][BLAKE2b]")
{
    using namespace GP;
    using namespace GP::Crypto;

    SECTION("Size Constants")
    {
        REQUIRE(BLAKE2b::BLOCK_SIZE == 128);
        REQUIRE(BLAKE2b::MAX_DIGEST_SIZE == 64);
        REQUIRE(BLAKE2b::MAX_KEY_SIZE == 64);
        REQUIRE(BLAKE2b::DEFAULT_DIGEST_SIZE == 32);
        REQUIRE(sizeof(BLAKE2bDigest::bytes) == 64);
        REQUIRE(BLAKE2bDigest::MAX_DIGEST_SIZE == 64);
    }

    SECTION("BLAKE2b-512 Empty String Vector (RFC 7693)")
    {
        // BLAKE2b("", 64) =
        //   786a02f742015903c6c6fd852552d272912f4740e15847618a86e217f71f5419
        //   d25e1031afee585313896444934eb04b903a685b1448b755d56f701afe9be2ce
        auto d = BLAKE2b::Hash("", 0, 64);
        constexpr UInt8 kExpected[64] = { 0x78, 0x6A, 0x02, 0xF7, 0x42, 0x01, 0x59, 0x03, 0xC6, 0xC6, 0xFD, 0x85, 0x25,
                                         0x52, 0xD2, 0x72, 0x91, 0x2F, 0x47, 0x40, 0xE1, 0x58, 0x47, 0x61, 0x8A, 0x86,
                                         0xE2, 0x17, 0xF7, 0x1F, 0x54, 0x19, 0xD2, 0x5E, 0x10, 0x31, 0xAF, 0xEE, 0x58,
                                         0x53, 0x13, 0x89, 0x64, 0x44, 0x93, 0x4E, 0xB0, 0x4B, 0x90, 0x3A, 0x68, 0x5B,
                                         0x14, 0x48, 0xB7, 0x55, 0xD5, 0x6F, 0x70, 0x1A, 0xFE, 0x9B, 0xE2, 0xCE };
        REQUIRE(d.length == 64);
        for (SizeT i = 0; i < 64; ++i)
        {
            INFO("Byte " << i);
            REQUIRE(static_cast<UInt8>(d.bytes[i]) == kExpected[i]);
        }
    }

    SECTION("BLAKE2b-256 (Default Output Length) Empty String Vector")
    {
        // BLAKE2b("", 32) = 0e5751c026e543b2e8ab2eb06099daa1d1e5df47778f7787faab45cdf12fe3a8
        auto d = BLAKE2b::Hash("", 0);   // DEFAULT_DIGEST_SIZE = 32
        constexpr UInt8 kExpected[32] = { 0x0E, 0x57, 0x51, 0xC0, 0x26, 0xE5, 0x43, 0xB2, 0xE8, 0xAB, 0x2E,
                                         0xB0, 0x60, 0x99, 0xDA, 0xA1, 0xD1, 0xE5, 0xDF, 0x47, 0x77, 0x8F,
                                         0x77, 0x87, 0xFA, 0xAB, 0x45, 0xCD, 0xF1, 0x2F, 0xE3, 0xA8 };
        REQUIRE(d.length == 32);
        for (SizeT i = 0; i < 32; ++i)
        {
            INFO("Byte " << i);
            REQUIRE(static_cast<UInt8>(d.bytes[i]) == kExpected[i]);
        }
    }

    SECTION("Variable Output Length Changes First Byte")
    {
        // Different output lengths must yield completely different results
        // (not just truncation — BLAKE2b encodes output length in its IV parameter block).
        auto d32 = BLAKE2b::Hash("hello", 5, 32);
        auto d48 = BLAKE2b::Hash("hello", 5, 48);
        auto d64 = BLAKE2b::Hash("hello", 5, 64);
        // They cannot all share the same first 32 bytes.
        REQUIRE(d32.length == 32);
        REQUIRE(d48.length == 48);
        REQUIRE(d64.length == 64);
        bool all_equal = true;
        for (SizeT i = 0; i < 32; ++i)
        {
            if (d32.bytes[i] != d64.bytes[i])
            {
                all_equal = false;
                break;
            }
        }
        REQUIRE_FALSE(all_equal);
    }

    SECTION("Void Pointer Overload Matches char* Overload")
    {
        const char* str = "hello";
        auto d1 = BLAKE2b::Hash(str, 5);
        auto d2 = BLAKE2b::Hash(static_cast<const void*>(str), 5);
        REQUIRE(d1 == d2);
    }

    SECTION("Null-Terminated C-String Overload Matches Length Overload")
    {
        auto d1 = BLAKE2b::Hash("abc", 3);
        auto d2 = BLAKE2b::Hash("abc");
        REQUIRE(d1 == d2);
    }

    SECTION("String View Overload Matches Length Overload")
    {
        auto d1 = BLAKE2b::Hash("abc", 3);
        std::string_view sv("abc", 3);
        auto d2 = BLAKE2b::Hash(sv);
        REQUIRE(d1 == d2);
    }

    SECTION("Single-Byte Sensitivity")
    {
        auto d0 = BLAKE2b::Hash("aaaaaaaaaa", 10);
        auto d1 = BLAKE2b::Hash("aaaaaaaaab", 10);
        REQUIRE(d0 != d1);
    }

    SECTION("Determinism")
    {
        auto d1 = BLAKE2b::Hash("determinism", 11);
        auto d2 = BLAKE2b::Hash("determinism", 11);
        REQUIRE(d1 == d2);
    }

    SECTION("Verify Matching Digest Returns True")
    {
        auto d = BLAKE2b::Hash("verify test", 11);
        REQUIRE(BLAKE2b::Verify("verify test", 11, d));
    }

    SECTION("Verify Mismatched Input Returns False")
    {
        auto d = BLAKE2b::Hash("verify test", 11);
        REQUIRE_FALSE(BLAKE2b::Verify("verify teSt", 11, d));
    }

    SECTION("Keyed Hash Differs from Unkeyed Hash")
    {
        // Providing a key must produce a distinct digest.
        const char* key = "secretkey1234567";
        auto unkeyed = BLAKE2b::Hash("hello", 5);
        auto keyed = BLAKE2b::HashKeyed("hello", 5, key, 16);
        REQUIRE(keyed != unkeyed);
    }

    SECTION("Keyed Hash is Deterministic")
    {
        const char* key = "mykey";
        auto d1 = BLAKE2b::HashKeyed("data", 4, key, 5);
        auto d2 = BLAKE2b::HashKeyed("data", 4, key, 5);
        REQUIRE(d1 == d2);
    }

    SECTION("Different Keys Produce Different Keyed Hashes")
    {
        const char* key1 = "keyA";
        const char* key2 = "keyB";
        auto d1 = BLAKE2b::HashKeyed("data", 4, key1, 4);
        auto d2 = BLAKE2b::HashKeyed("data", 4, key2, 4);
        REQUIRE(d1 != d2);
    }

    SECTION("Keyed Empty Input Returns Valid Digest")
    {
        // Keyed BLAKE2b with zero-length message has a special code path.
        const char* key = "k";
        auto d = BLAKE2b::HashKeyed("", 0, key, 1);
        REQUIRE(d.length == 32);
        // Must differ from unkeyed empty hash.
        auto unkeyed = BLAKE2b::Hash("", 0);
        REQUIRE(d != unkeyed);
    }

    SECTION("Streaming Context Matches One-Shot - Simple")
    {
        BLAKE2b::Context ctx;
        ctx.Init();   // default: 32-byte output, no key
        ctx.Update("a", 1);
        ctx.Update("bc", 2);
        auto streamResult = ctx.Finalize();

        auto oneShotResult = BLAKE2b::Hash("abc", 3);
        REQUIRE(streamResult == oneShotResult);
    }

    SECTION("Streaming Context Matches One-Shot - Across Block Boundary")
    {
        // Feed 256 bytes (two BLAKE2b blocks of 128 bytes) in misaligned chunks.
        std::string input(256, 'b');
        BLAKE2b::Context ctx;
        ctx.Init();
        ctx.Update(input.c_str(), 100);
        ctx.Update(input.c_str() + 100, 100);
        ctx.Update(input.c_str() + 200, 56);
        auto streamResult = ctx.Finalize();
        auto oneShotResult = BLAKE2b::Hash(input.c_str(), 256);
        REQUIRE(streamResult == oneShotResult);
    }

    SECTION("Streaming Context with Custom Output Length")
    {
        BLAKE2b::Context ctx;
        ctx.Init(48);   // 48-byte output
        ctx.Update("abc", 3);
        auto streamResult = ctx.Finalize();

        auto oneShotResult = BLAKE2b::Hash("abc", 3, 48);
        REQUIRE(streamResult == oneShotResult);
    }

    SECTION("Streaming Context with Key Matches HashKeyed")
    {
        const char* key = "mykey";
        BLAKE2b::Context ctx;
        ctx.Init(32, key, 5);
        ctx.Update("abc", 3);
        auto streamResult = ctx.Finalize();

        auto oneShotResult = BLAKE2b::HashKeyed("abc", 3, key, 5, 32);
        REQUIRE(streamResult == oneShotResult);
    }

    SECTION("BLAKE2bDigest operator== Considers Length")
    {
        // Two digests with the same bytes but different lengths must not compare equal.
        BLAKE2bDigest d32 = BLAKE2b::Hash("abc", 3, 32);
        // Manually create a digest with the same first 32 bytes but length=64.
        BLAKE2bDigest d64_fake{};
        d64_fake.length = 64;
        for (SizeT i = 0; i < 32; ++i) { d64_fake.bytes[i] = d32.bytes[i]; }
        // A digest of length 64 with only partial data must not equal d32.
        REQUIRE(d32 != d64_fake);
    }
}
