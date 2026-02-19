// Copyright (c) - Graphical Playground. All rights reserved.

#include "Crypto/Hash/HashUtilities.hpp"
#include <catch2/catch_test_macros.hpp>

// HashUtilities lives in GP::Crypto::Detail - it is an internal header tested
// here to verify the correctness of bit-manipulation primitives that underpin
// every cryptographic hash implementation.

TEST_CASE("HashUtilities - Rotation Primitives", "[GP][Core][Crypto][Detail][HashUtilities]")
{
    using namespace GP;
    using Util = GP::Crypto::Detail::HashUtilities;

    SECTION("Rotl32 - Single Bit Left Rotation")
    {
        REQUIRE(Util::Rotl32(0x00000001U, 1) == 0x00000002U);
        REQUIRE(Util::Rotl32(0x00000001U, 31) == 0x80000000U);
        REQUIRE(Util::Rotl32(0x80000000U, 1) == 0x00000001U);   // wraps around
    }

    SECTION("Rotl32 - Identity at 0 and 32")
    {
        REQUIRE(Util::Rotl32(0x12345678U, 0) == 0x12345678U);
        REQUIRE(Util::Rotl32(0xDEADBEEFU, 32) == 0xDEADBEEFU);
    }

    SECTION("Rotl32 - Pattern Rotation")
    {
        // 0xF0000000 rotated left by 4 → 0x0000000F
        REQUIRE(Util::Rotl32(0xF0000000U, 4) == 0x0000000FU);
    }

    SECTION("Rotr32 - Single Bit Right Rotation")
    {
        REQUIRE(Util::Rotr32(0x00000002U, 1) == 0x00000001U);
        REQUIRE(Util::Rotr32(0x00000001U, 1) == 0x80000000U);   // wraps around
        REQUIRE(Util::Rotr32(0x80000000U, 31) == 0x00000001U);
    }

    SECTION("Rotr32 - Inverse of Rotl32")
    {
        constexpr UInt32 val = 0xABCD1234U;
        for (Int32 i = 0; i <= 31; ++i)
        {
            INFO("Rotation amount: " << i);
            REQUIRE(Util::Rotr32(Util::Rotl32(val, i), i) == val);
        }
    }

    SECTION("Rotl64 - Single Bit Left Rotation")
    {
        REQUIRE(Util::Rotl64(0x0000000000000001ULL, 1) == 0x0000000000000002ULL);
        REQUIRE(Util::Rotl64(0x8000000000000000ULL, 1) == 0x0000000000000001ULL);
        REQUIRE(Util::Rotl64(0x0000000000000001ULL, 63) == 0x8000000000000000ULL);
    }

    SECTION("Rotr64 - Inverse of Rotl64")
    {
        constexpr UInt64 val = 0xFEDCBA9876543210ULL;
        for (Int32 i = 0; i <= 63; ++i)
        {
            INFO("Rotation amount: " << i);
            REQUIRE(Util::Rotr64(Util::Rotl64(val, i), i) == val);
        }
    }

    SECTION("Rotr64 - Pattern Rotation")
    {
        // 0xFF00000000000000 rotr by 8 → 0x00FF000000000000
        REQUIRE(Util::Rotr64(0xFF00000000000000ULL, 8) == 0x00FF000000000000ULL);
    }
}

TEST_CASE("HashUtilities - Read Primitives", "[GP][Core][Crypto][Detail][HashUtilities]")
{
    using namespace GP;
    using Util = GP::Crypto::Detail::HashUtilities;

    // bytes: 01 02 03 04 05 06 07 08
    static constexpr char kBytes[8] = { '\x01', '\x02', '\x03', '\x04', '\x05', '\x06', '\x07', '\x08' };

    SECTION("ReadU32LE - Little-Endian Word at Offset 0")
    {
        // bytes 01 02 03 04 → LE u32 = 0x04030201
        REQUIRE(Util::ReadU32LE(kBytes, 0) == 0x04030201U);
    }

    SECTION("ReadU32LE - Little-Endian Word at Offset 4")
    {
        // bytes 05 06 07 08 → LE u32 = 0x08070605
        REQUIRE(Util::ReadU32LE(kBytes, 4) == 0x08070605U);
    }

    SECTION("ReadU32BE - Big-Endian Word at Offset 0")
    {
        // bytes 01 02 03 04 → BE u32 = 0x01020304
        REQUIRE(Util::ReadU32BE(kBytes, 0) == 0x01020304U);
    }

    SECTION("ReadU32BE - Big-Endian Word at Offset 4")
    {
        // bytes 05 06 07 08 → BE u32 = 0x05060708
        REQUIRE(Util::ReadU32BE(kBytes, 4) == 0x05060708U);
    }

    SECTION("ReadU64LE - Little-Endian QWord at Offset 0")
    {
        // bytes 01 02 03 04 05 06 07 08 → LE u64 = 0x0807060504030201
        REQUIRE(Util::ReadU64LE(kBytes, 0) == 0x0807060504030201ULL);
    }

    SECTION("ReadU64BE - Big-Endian QWord at Offset 0")
    {
        // bytes 01 02 03 04 05 06 07 08 → BE u64 = 0x0102030405060708
        REQUIRE(Util::ReadU64BE(kBytes, 0) == 0x0102030405060708ULL);
    }

    SECTION("ReadU16LE - Little-Endian Half-Word")
    {
        // bytes 01 02 → LE u16 = 0x0201
        REQUIRE(Util::ReadU16LE(kBytes, 0) == 0x0201U);
    }
}

TEST_CASE("HashUtilities - Write Primitives", "[GP][Core][Crypto][Detail][HashUtilities]")
{
    using namespace GP;
    using Util = GP::Crypto::Detail::HashUtilities;

    SECTION("WriteU32BE then ReadU32BE Round-Trip")
    {
        char buf[4] = {};
        Util::WriteU32BE(buf, 0, 0xDEADBEEFU);
        REQUIRE(Util::ReadU32BE(buf, 0) == 0xDEADBEEFU);
        // Verify byte layout: DE AD BE EF
        REQUIRE(static_cast<UInt8>(buf[0]) == 0xDE);
        REQUIRE(static_cast<UInt8>(buf[1]) == 0xAD);
        REQUIRE(static_cast<UInt8>(buf[2]) == 0xBE);
        REQUIRE(static_cast<UInt8>(buf[3]) == 0xEF);
    }

    SECTION("WriteU32LE then ReadU32LE Round-Trip")
    {
        char buf[4] = {};
        Util::WriteU32LE(buf, 0, 0x12345678U);
        REQUIRE(Util::ReadU32LE(buf, 0) == 0x12345678U);
        // Verify byte layout: 78 56 34 12
        REQUIRE(static_cast<UInt8>(buf[0]) == 0x78);
        REQUIRE(static_cast<UInt8>(buf[1]) == 0x56);
        REQUIRE(static_cast<UInt8>(buf[2]) == 0x34);
        REQUIRE(static_cast<UInt8>(buf[3]) == 0x12);
    }

    SECTION("WriteU64BE then ReadU64BE Round-Trip")
    {
        char buf[8] = {};
        constexpr UInt64 kVal = 0x0102030405060708ULL;
        Util::WriteU64BE(buf, 0, kVal);
        REQUIRE(Util::ReadU64BE(buf, 0) == kVal);
        // Verify first and last byte.
        REQUIRE(static_cast<UInt8>(buf[0]) == 0x01);
        REQUIRE(static_cast<UInt8>(buf[7]) == 0x08);
    }

    SECTION("WriteU64LE then ReadU64LE Round-Trip")
    {
        char buf[8] = {};
        constexpr UInt64 kVal = 0xFEDCBA9876543210ULL;
        Util::WriteU64LE(buf, 0, kVal);
        REQUIRE(Util::ReadU64LE(buf, 0) == kVal);
        // Verify first and last byte.
        REQUIRE(static_cast<UInt8>(buf[0]) == 0x10);
        REQUIRE(static_cast<UInt8>(buf[7]) == 0xFE);
    }

    SECTION("Write at Non-Zero Offset")
    {
        char buf[8] = {};
        Util::WriteU32BE(buf, 4, 0x01020304U);
        // First 4 bytes must remain zero.
        REQUIRE(static_cast<UInt8>(buf[0]) == 0x00);
        REQUIRE(static_cast<UInt8>(buf[4]) == 0x01);
        REQUIRE(static_cast<UInt8>(buf[7]) == 0x04);
    }
}

TEST_CASE("HashUtilities - Byte Swap", "[GP][Core][Crypto][Detail][HashUtilities]")
{
    using namespace GP;
    using Util = GP::Crypto::Detail::HashUtilities;

    SECTION("ByteSwap32 - Known Value") { REQUIRE(Util::ByteSwap32(0x12345678U) == 0x78563412U); }

    SECTION("ByteSwap32 - Idempotent Double-Swap")
    {
        REQUIRE(Util::ByteSwap32(Util::ByteSwap32(0xABCD1234U)) == 0xABCD1234U);
    }

    SECTION("ByteSwap32 - All Bytes Distinct")
    {
        // 0x01020304 → 0x04030201
        REQUIRE(Util::ByteSwap32(0x01020304U) == 0x04030201U);
    }

    SECTION("ByteSwap64 - Known Value") { REQUIRE(Util::ByteSwap64(0x0102030405060708ULL) == 0x0807060504030201ULL); }

    SECTION("ByteSwap64 - Idempotent Double-Swap")
    {
        REQUIRE(Util::ByteSwap64(Util::ByteSwap64(0xFEDCBA9876543210ULL)) == 0xFEDCBA9876543210ULL);
    }

    SECTION("ByteSwap64 - All Bytes Distinct")
    {
        REQUIRE(Util::ByteSwap64(0xFFEEDDCCBBAA9988ULL) == 0x8899AABBCCDDEEFFULL);
    }

    SECTION("ByteSwap32 Consistent with ReadU32BE / WriteU32LE")
    {
        // Writing a value as LE then reading via ByteSwap32 should give the BE view.
        char buf[4] = {};
        Util::WriteU32LE(buf, 0, 0x12345678U);
        // ReadU32LE gives back 0x12345678; ByteSwap32 of that gives the BE view.
        auto le = Util::ReadU32LE(buf, 0);
        REQUIRE(Util::ByteSwap32(le) == Util::ReadU32BE(buf, 0));
    }
}

TEST_CASE("HashUtilities - Multiply128", "[GP][Core][Crypto][Detail][HashUtilities]")
{
    using namespace GP;
    using Util = GP::Crypto::Detail::HashUtilities;

    SECTION("1 * 1 = 1 (no overflow)")
    {
        UInt64 lo = 0, hi = 0;
        Util::Multiply128(1ULL, 1ULL, lo, hi);
        REQUIRE(lo == 1ULL);
        REQUIRE(hi == 0ULL);
    }

    SECTION("0 * anything = 0")
    {
        UInt64 lo = 1, hi = 1;
        Util::Multiply128(0ULL, 0xFFFFFFFFFFFFFFFFULL, lo, hi);
        REQUIRE(lo == 0ULL);
        REQUIRE(hi == 0ULL);
    }

    SECTION("Large * 2 - Overflow into High Word")
    {
        // 0xFFFFFFFFFFFFFFFF * 2 = 0x1_FFFFFFFFFFFFFFFE
        // hi = 1, lo = 0xFFFFFFFFFFFFFFFE
        UInt64 lo = 0, hi = 0;
        Util::Multiply128(0xFFFFFFFFFFFFFFFFULL, 2ULL, lo, hi);
        REQUIRE(lo == 0xFFFFFFFFFFFFFFFEULL);
        REQUIRE(hi == 1ULL);
    }

    SECTION("MAX * MAX - Maximum Overflow")
    {
        // (2^64-1)^2 = 2^128 - 2^65 + 1
        // hi = 2^64 - 2 = 0xFFFFFFFFFFFFFFFE, lo = 1
        UInt64 lo = 0, hi = 0;
        Util::Multiply128(0xFFFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFFULL, lo, hi);
        REQUIRE(lo == 1ULL);
        REQUIRE(hi == 0xFFFFFFFFFFFFFFFEULL);
    }

    SECTION("2^32 * 2^32 = 2^64")
    {
        // hi = 1, lo = 0
        UInt64 lo = 0, hi = 0;
        Util::Multiply128(0x100000000ULL, 0x100000000ULL, lo, hi);
        REQUIRE(lo == 0ULL);
        REQUIRE(hi == 1ULL);
    }

    SECTION("Commutativity: a*b == b*a")
    {
        UInt64 lo1 = 0, hi1 = 0;
        UInt64 lo2 = 0, hi2 = 0;
        Util::Multiply128(0x123456789ABCDEF0ULL, 0x0FEDCBA987654321ULL, lo1, hi1);
        Util::Multiply128(0x0FEDCBA987654321ULL, 0x123456789ABCDEF0ULL, lo2, hi2);
        REQUIRE(lo1 == lo2);
        REQUIRE(hi1 == hi2);
    }
}
