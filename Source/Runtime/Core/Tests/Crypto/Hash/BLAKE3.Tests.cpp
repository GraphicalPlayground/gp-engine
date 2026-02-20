// Copyright (c) - Graphical Playground. All rights reserved.

#include "Crypto/Hash/BLAKE3.hpp"
#include <catch2/catch_test_macros.hpp>
#include <string>
#include <string_view>

TEST_CASE("BLAKE3 Hashing", "[GP][Core][Crypto][Hash][BLAKE3]")
{
    using namespace GP;
    using namespace GP::Crypto;

    SECTION("Size Constants")
    {
        REQUIRE(BLAKE3::BLOCK_SIZE == 64);
        REQUIRE(BLAKE3::CHUNK_SIZE == 1024);
        REQUIRE(BLAKE3::DIGEST_SIZE == 32);
        REQUIRE(BLAKE3::KEY_SIZE == 32);
        REQUIRE(sizeof(BLAKE3Digest::bytes) == 32);
        REQUIRE(BLAKE3Digest::DIGEST_SIZE == 32);
    }

    SECTION("Known Empty String Vector")
    {
        // BLAKE3("") = af1349b9f5f9a1a6a0404dea36dcc9499bcb25c9adc112b7cc9d5af5c97b1017
        constexpr auto d = BLAKE3::Hash("", 0);
        constexpr UInt8 kExpected[32] = { 0xAF, 0x13, 0x49, 0xB9, 0xF5, 0xF9, 0xA1, 0xA6, 0xA0, 0x40, 0x4D,
                                          0xEA, 0x36, 0xDC, 0xC9, 0x49, 0x9B, 0xCB, 0x25, 0xC9, 0xAD, 0xC1,
                                          0x12, 0xB7, 0xCC, 0x9D, 0x5A, 0xF5, 0xC9, 0x7B, 0x10, 0x17 };
        for (SizeT i = 0; i < 32; ++i)
        {
            INFO("Byte " << i);
            REQUIRE(static_cast<UInt8>(d.bytes[i]) == kExpected[i]);
        }
    }

    SECTION("Known 'abc' Vector")
    {
        // BLAKE3("abc") = 6437b3ac38465133ffb63b75273a8db548c558465d79db03fd359c6cd5bd9d85
        constexpr auto d = BLAKE3::Hash("abc", 3);
        constexpr UInt8 kExpected[32] = { 0x64, 0x37, 0xB3, 0xAC, 0x38, 0x46, 0x51, 0x33, 0xFF, 0xB6, 0x3B,
                                          0x75, 0x27, 0x3A, 0x8D, 0xB5, 0x48, 0xC5, 0x58, 0x46, 0x5D, 0x79,
                                          0xDB, 0x03, 0xFD, 0x35, 0x9C, 0x6C, 0xD5, 0xBD, 0x9D, 0x85 };
        for (SizeT i = 0; i < 32; ++i)
        {
            INFO("Byte " << i);
            REQUIRE(static_cast<UInt8>(d.bytes[i]) == kExpected[i]);
        }
    }

    SECTION("Void Pointer Overload Matches char* Overload")
    {
        const char* str = "hello";
        auto d1 = BLAKE3::Hash(str, 5);
        auto d2 = BLAKE3::Hash(static_cast<const void*>(str), 5);
        REQUIRE(d1 == d2);
    }

    SECTION("Null-Terminated C-String Overload Matches Length Overload")
    {
        constexpr auto d1 = BLAKE3::Hash("abc", 3);
        constexpr auto d2 = BLAKE3::Hash("abc");
        REQUIRE(d1 == d2);
    }

    SECTION("String View Overload Matches Length Overload")
    {
        constexpr auto d1 = BLAKE3::Hash("abc", 3);
        std::string_view sv("abc", 3);
        auto d2 = BLAKE3::Hash(sv);
        REQUIRE(d1 == d2);
    }

    SECTION("Single-Byte Sensitivity")
    {
        constexpr auto d0 = BLAKE3::Hash("aaaaaaaaaa", 10);
        constexpr auto d1 = BLAKE3::Hash("aaaaaaaaab", 10);
        REQUIRE(d0 != d1);
    }

    SECTION("Determinism")
    {
        auto d1 = BLAKE3::Hash("determinism", 11);
        auto d2 = BLAKE3::Hash("determinism", 11);
        REQUIRE(d1 == d2);
    }

    SECTION("Non-Empty Differs from Empty")
    {
        constexpr auto dEmpty = BLAKE3::Hash("", 0);
        constexpr auto dAbc = BLAKE3::Hash("abc", 3);
        REQUIRE(dEmpty != dAbc);
    }

    SECTION("Verify Matching Digest Returns True")
    {
        constexpr auto d = BLAKE3::Hash("verify test", 11);
        REQUIRE(BLAKE3::Verify("verify test", 11, d));
    }

    SECTION("Verify Mismatched Input Returns False")
    {
        constexpr auto d = BLAKE3::Hash("verify test", 11);
        REQUIRE_FALSE(BLAKE3::Verify("verify teSt", 11, d));
    }

    SECTION("Single-Chunk Boundary (1023 vs 1024 vs 1025 bytes)")
    {
        // BLAKE3 splits input into 1024-byte chunks. Hashing at the exact chunk
        // boundary exercises the chunk-merging code path.
        std::string s1023(1023, 'x');
        std::string s1024(1024, 'x');
        std::string s1025(1025, 'x');

        auto d1023 = BLAKE3::Hash(s1023.c_str(), 1023);
        auto d1024 = BLAKE3::Hash(s1024.c_str(), 1024);
        auto d1025 = BLAKE3::Hash(s1025.c_str(), 1025);

        // All three must be distinct.
        REQUIRE(d1023 != d1024);
        REQUIRE(d1024 != d1025);
        REQUIRE(d1023 != d1025);

        // Each must be deterministic.
        REQUIRE(d1024 == BLAKE3::Hash(s1024.c_str(), 1024));
        REQUIRE(d1025 == BLAKE3::Hash(s1025.c_str(), 1025));
    }

    SECTION("Multi-Chunk Input (> 2 * CHUNK_SIZE)")
    {
        // Exercise the Merkle-tree merging of multiple chunks.
        std::string large(3000, 'y');
        auto d1 = BLAKE3::Hash(large.c_str(), 3000);
        auto d2 = BLAKE3::Hash(large.c_str(), 3000);
        REQUIRE(d1 == d2);
        // Appending one byte must change the digest.
        large += 'z';
        auto d3 = BLAKE3::Hash(large.c_str(), 3001);
        REQUIRE(d1 != d3);
    }

    SECTION("Compile-Time Evaluation")
    {
        // BLAKE3("abc") first byte = 0x64
        constexpr auto d = BLAKE3::Hash("abc", 3);
        static_assert(static_cast<UInt8>(d.bytes[0]) == 0x64, "BLAKE3 compile-time evaluation failed");
        REQUIRE(static_cast<UInt8>(d.bytes[0]) == 0x64);
        REQUIRE(static_cast<UInt8>(d.bytes[31]) == 0x85);
    }
}
