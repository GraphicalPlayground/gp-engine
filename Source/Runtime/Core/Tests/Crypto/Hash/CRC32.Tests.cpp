// Copyright (c) - Graphical Playground. All rights reserved.

#include "Crypto/Hash/CRC32.hpp"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("CRC32 Hashing", "[GP][Core][Crypto][Hash][CRC32]")
{
    using namespace GP::Crypto;

    SECTION("Empty String") { REQUIRE(CRC32::Hash("", 0) == 0); }

    SECTION("Known Test Vectors")
    {
        REQUIRE(CRC32::Hash("123456789") == 0xCBF43926U);
        REQUIRE(CRC32::Hash("hello") == 0x3610A686U);
        REQUIRE(CRC32::Hash("The quick brown fox jumps over the lazy dog") == 0x414FA339U);
        REQUIRE(CRC32::Hash("a") == 0xE8B7BE43U);
    }

    SECTION("Different Overloads Consistency")
    {
        const char* str = "test data";
        std::string_view sv(str);

        REQUIRE(CRC32::Hash(str) == CRC32::Hash(sv));
        REQUIRE(CRC32::Hash(str, std::char_traits<char>::length(str)) == CRC32::Hash(str));
        REQUIRE(CRC32::Hash(static_cast<const void*>(str), std::char_traits<char>::length(str)) == CRC32::Hash(str));
    }

    SECTION("Binary Data")
    {
        const unsigned char data[] = { 0x00, 0xFF, 0xAA, 0x55 };
        auto hash = CRC32::Hash(reinterpret_cast<const char*>(data), 4);
        REQUIRE(hash != 0);
    }

    SECTION("Append Functionality")
    {
        const char* part1 = "hello";
        const char* part2 = " world";
        const char* full = "hello world";

        auto hash1 = CRC32::Hash(part1);
        auto hash2 = CRC32::Append(hash1, part2, std::char_traits<char>::length(part2));
        auto hash3 = CRC32::Hash(full);

        REQUIRE(hash2 == hash3);
    }

    SECTION("Append with Void Pointer")
    {
        const char* part1 = "test";
        const char* part2 = "123";

        auto hash1 = CRC32::Hash(part1);
        auto hash2 = CRC32::Append(hash1, static_cast<const void*>(part2), 3);
        auto hash3 = CRC32::Hash("test123");

        REQUIRE(hash2 == hash3);
    }

    SECTION("Verify Functionality")
    {
        const char* data = "verify me";
        auto hash = CRC32::Hash(data);

        REQUIRE(CRC32::Verify(data, std::char_traits<char>::length(data), hash));
        REQUIRE_FALSE(CRC32::Verify(data, std::char_traits<char>::length(data), hash + 1));
    }

    SECTION("Verify with Void Pointer")
    {
        const char* data = "check";
        auto hash = CRC32::Hash(data);

        REQUIRE(CRC32::Verify(static_cast<const void*>(data), 5, hash));
        REQUIRE_FALSE(CRC32::Verify(static_cast<const void*>(data), 5, hash ^ 0xFFFFFFFFU));
    }

    SECTION("Consistency Check")
    {
        auto hash1 = CRC32::Hash("constexpr", 9);
        auto hash2 = CRC32::Hash("constexpr", 9);
        REQUIRE(hash1 == hash2);
    }

    SECTION("User-Defined Literal")
    {
        auto hash1 = "test"_crc32;
        auto hash2 = CRC32::Hash("test");
        REQUIRE(hash1 == hash2);
    }

    SECTION("Streaming CRC")
    {
        const char* chunk1 = "chunk1";
        const char* chunk2 = "chunk2";
        const char* chunk3 = "chunk3";

        auto crc = CRC32::Hash(chunk1);
        crc = CRC32::Append(crc, chunk2, 6);
        crc = CRC32::Append(crc, chunk3, 6);

        auto fullHash = CRC32::Hash("chunk1chunk2chunk3");
        REQUIRE(crc == fullHash);
    }

    SECTION("Deterministic Output")
    {
        const char* input = "deterministic";
        auto hash1 = CRC32::Hash(input);
        auto hash2 = CRC32::Hash(input);
        REQUIRE(hash1 == hash2);
    }

    SECTION("Constants Accessible")
    {
        REQUIRE(CRC32::Polynomial == 0xEDB88320U);
        REQUIRE(CRC32::InitialValue == 0xFFFFFFFFU);
    }

    SECTION("String View Overload")
    {
        std::string_view sv("hello world");
        REQUIRE(CRC32::Hash(sv) == CRC32::Hash("hello world"));
        REQUIRE(CRC32::Hash(sv) == CRC32::Hash("hello world", 11));
    }

    SECTION("Single Byte Sensitivity")
    {
        // Flipping one byte anywhere must change the CRC.
        std::string data(20, 'A');
        auto base = CRC32::Hash(data.data(), data.size());
        for (GP::SizeT i = 0; i < data.size(); ++i)
        {
            std::string modified = data;
            modified[i] = 'B';
            REQUIRE(CRC32::Hash(modified.data(), modified.size()) != base);
        }
    }

    SECTION("Append Idempotence with Zero-Length Chunk")
    {
        const char* str = "base string";
        auto hash = CRC32::Hash(str);
        auto appended = CRC32::Append(hash, "", 0);
        REQUIRE(appended == hash);
    }

    SECTION("Compile-time Evaluation")
    {
        constexpr auto hash = CRC32::Hash("constexpr", 9);
        REQUIRE(hash == CRC32::Hash("constexpr"));
        REQUIRE(hash != 0);
    }

    SECTION("Block Boundary Sensitivity")
    {
        // Lengths that straddle the table-lookup loop boundaries.
        for (GP::SizeT len = 1; len <= 32; ++len)
        {
            std::string a(len, 'Q');
            std::string b(len, 'Q');
            b.back() = 'R';
            REQUIRE(CRC32::Hash(a.data(), a.size()) != CRC32::Hash(b.data(), b.size()));
        }
    }

    SECTION("Long String Sensitivity")
    {
        std::string longStr(10000, 'X');
        auto hash1 = CRC32::Hash(longStr.data(), longStr.size());
        longStr[9999] = 'Y';
        auto hash2 = CRC32::Hash(longStr.data(), longStr.size());
        REQUIRE(hash1 != hash2);
    }

    SECTION("Verify - String View Overload")
    {
        std::string_view sv("view verify");
        auto hash = CRC32::Hash(sv);
        REQUIRE(CRC32::Verify(sv.data(), sv.size(), hash));
        REQUIRE_FALSE(CRC32::Verify(sv.data(), sv.size(), hash ^ 1U));
    }
}
