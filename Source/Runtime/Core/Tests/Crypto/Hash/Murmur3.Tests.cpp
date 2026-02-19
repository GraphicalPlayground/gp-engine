// Copyright (c) - Graphical Playground. All rights reserved.

#include "Crypto/Hash/Murmur3.hpp"
#include <catch2/catch_test_macros.hpp>
#include <string>

TEST_CASE("Murmur3 Hashing", "[GP][Core][Crypto][Hash][Murmur3]")
{
    using namespace GP::Crypto;

    SECTION("Empty String")
    {
        REQUIRE(Murmur3::Hash32("", 0, 0) == 0);
        auto hash128 = Murmur3::Hash128("", 0, 0);
        REQUIRE(hash128.low == 0);
        REQUIRE(hash128.high == 0);
    }

    SECTION("Known Test Vectors 32-bit")
    {
        REQUIRE(Murmur3::Hash32("hello", 5, 0) == 0x248BFA47U);
        REQUIRE(Murmur3::Hash32("The quick brown fox jumps over the lazy dog", 43, 0) == 0x2E4FF723U);
    }

    SECTION("Different Seeds Produce Different Hashes")
    {
        const char* str = "seed test";
        auto hash1 = Murmur3::Hash32(str, std::char_traits<char>::length(str), 0);
        auto hash2 = Murmur3::Hash32(str, std::char_traits<char>::length(str), 123);
        REQUIRE(hash1 != hash2);

        auto hash128_1 = Murmur3::Hash128(str, std::char_traits<char>::length(str), 0);
        auto hash128_2 = Murmur3::Hash128(str, std::char_traits<char>::length(str), 123);
        REQUIRE(hash128_1.low != hash128_2.low);
    }

    SECTION("Different Overloads Consistency - Hash32")
    {
        const char* str = "consistency check";
        std::string_view sv(str);

        REQUIRE(Murmur3::Hash32(str) == Murmur3::Hash32(sv));
        REQUIRE(Murmur3::Hash32(str, std::char_traits<char>::length(str)) == Murmur3::Hash32(str));
        REQUIRE(
            Murmur3::Hash32(static_cast<const void*>(str), std::char_traits<char>::length(str)) == Murmur3::Hash32(str)
        );
    }

    SECTION("Hash128 Overloads Consistency")
    {
        const char* str = "128bit test";
        std::string_view sv(str);

        auto hash1 = Murmur3::Hash128(str);
        auto hash2 = Murmur3::Hash128(sv);
        REQUIRE(hash1 == hash2);

        auto hash3 = Murmur3::Hash128(str, std::char_traits<char>::length(str));
        REQUIRE(hash1 == hash3);

        auto hash4 = Murmur3::Hash128(static_cast<const void*>(str), std::char_traits<char>::length(str));
        REQUIRE(hash1 == hash4);
    }

    SECTION("Architecture-Sized Hash")
    {
        const char* str = "arch test";
        std::string_view sv(str);
        auto hash = Murmur3::Hash(str);

#if GP_ARCHITECTURE_64BIT
        REQUIRE(hash == Murmur3::Hash128(str).low);
#else
        REQUIRE(hash == Murmur3::Hash32(str));
#endif

        REQUIRE(Murmur3::Hash(str) == Murmur3::Hash(sv));
        REQUIRE(Murmur3::Hash(str, std::char_traits<char>::length(str)) == Murmur3::Hash(str));
        REQUIRE(
            Murmur3::Hash(static_cast<const void*>(str), std::char_traits<char>::length(str)) == Murmur3::Hash(str)
        );
    }

    SECTION("Constants Accessible")
    {
        REQUIRE(Murmur3::DefaultSeed32 == 0U);
        REQUIRE(Murmur3::DefaultSeed64 == 0ULL);
    }

    SECTION("Single Character")
    {
        REQUIRE(Murmur3::Hash32("a") != Murmur3::Hash32("b"));
        REQUIRE(Murmur3::Hash32("x") != Murmur3::Hash32("y"));
    }

    SECTION("Tail Processing 32-bit")
    {
        REQUIRE(Murmur3::Hash32("a") != 0);
        REQUIRE(Murmur3::Hash32("ab") != 0);
        REQUIRE(Murmur3::Hash32("abc") != 0);
        REQUIRE(Murmur3::Hash32("abcd") != 0);
        REQUIRE(Murmur3::Hash32("abcde") != 0);

        // All tail lengths must produce distinct results.
        REQUIRE(Murmur3::Hash32("a") != Murmur3::Hash32("ab"));
        REQUIRE(Murmur3::Hash32("ab") != Murmur3::Hash32("abc"));
        REQUIRE(Murmur3::Hash32("abc") != Murmur3::Hash32("abcd"));
    }

    SECTION("Tail Processing 128-bit")
    {
        for (int i = 1; i <= 15; ++i)
        {
            std::string str(i, 'x');
            auto hash = Murmur3::Hash128(str.data(), str.size());
            // clang-format off
            REQUIRE((hash.low != 0 || hash.high != 0));
            // clang-format on
        }
    }

    SECTION("Single Byte Sensitivity - 32-bit")
    {
        std::string data(20, 'M');
        auto base = Murmur3::Hash32(data.data(), data.size());
        for (GP::SizeT i = 0; i < data.size(); ++i)
        {
            std::string modified = data;
            modified[i] = 'N';
            REQUIRE(Murmur3::Hash32(modified.data(), modified.size()) != base);
        }
    }

    SECTION("Single Byte Sensitivity - 128-bit")
    {
        std::string data(20, 'M');
        auto base = Murmur3::Hash128(data.data(), data.size());
        for (GP::SizeT i = 0; i < data.size(); ++i)
        {
            std::string modified = data;
            modified[i] = 'N';
            REQUIRE(Murmur3::Hash128(modified.data(), modified.size()) != base);
        }
    }

    SECTION("Binary Data")
    {
        const unsigned char data[] = { 0x00, 0xFF, 0xAA, 0x55, 0x12, 0x34 };
        auto hash = Murmur3::Hash32(reinterpret_cast<const char*>(data), 6);
        REQUIRE(hash != 0);
    }

    SECTION("Long Strings")
    {
        std::string longStr(500, 'M');
        auto hash32 = Murmur3::Hash32(longStr.data(), longStr.size());
        REQUIRE(hash32 != 0);

        auto hash128 = Murmur3::Hash128(longStr.data(), longStr.size());
        // clang-format off
        REQUIRE((hash128.low != 0 || hash128.high != 0));
        // clang-format on
    }

    SECTION("Hash128 Equality Operator")
    {
        auto hash1 = Murmur3::Hash128("test");
        auto hash2 = Murmur3::Hash128("test");
        auto hash3 = Murmur3::Hash128("different");

        REQUIRE(hash1 == hash2);
        REQUIRE_FALSE(hash1 == hash3);
    }

    SECTION("Hash128 Seeds Produce Non-Trivial Distinct Results")
    {
        const char* str = "128 seed check";
        auto h0 = Murmur3::Hash128(str, std::char_traits<char>::length(str), 0);
        auto h1 = Murmur3::Hash128(str, std::char_traits<char>::length(str), 1);
        auto h2 = Murmur3::Hash128(str, std::char_traits<char>::length(str), 0xDEADBEEFULL);
        REQUIRE(h0 != h1);
        REQUIRE(h0 != h2);
        REQUIRE(h1 != h2);
    }

    SECTION("Block Boundary Testing")
    {
        std::string str16(16, 'B');
        std::string str32(32, 'B');
        std::string str33(33, 'B');

        auto hash1 = Murmur3::Hash128(str16.data(), str16.size());
        auto hash2 = Murmur3::Hash128(str32.data(), str32.size());
        auto hash3 = Murmur3::Hash128(str33.data(), str33.size());

        REQUIRE_FALSE(hash1 == hash2);
        REQUIRE_FALSE(hash2 == hash3);
    }

    SECTION("Compile-time Evaluation")
    {
        constexpr auto hash32 = Murmur3::Hash32("compile", 7, 0);
        REQUIRE(hash32 == Murmur3::Hash32("compile", 7, 0));

        constexpr auto hash128 = Murmur3::Hash128("time", 4, 0);
        REQUIRE(hash128 == Murmur3::Hash128("time", 4, 0));
    }

    SECTION("User-Defined Literal")
    {
        auto hash1 = "murmur"_murmur3;
        auto hash2 = Murmur3::Hash32("murmur");
        REQUIRE(hash1 == hash2);
    }

    SECTION("Deterministic Output")
    {
        const char* input = "deterministic";
        auto hash1 = Murmur3::Hash32(input);
        auto hash2 = Murmur3::Hash32(input);
        REQUIRE(hash1 == hash2);

        auto hash128_1 = Murmur3::Hash128(input);
        auto hash128_2 = Murmur3::Hash128(input);
        REQUIRE(hash128_1 == hash128_2);
    }
}

TEST_CASE("Murmur3 Edge Cases", "[GP][Core][Crypto][Hash][Murmur3]")
{
    using namespace GP::Crypto;

    SECTION("Empty String")
    {
        REQUIRE(Murmur3::Hash32("", 0, 0) == 0);
        auto hash128 = Murmur3::Hash128("", 0, 0);
        REQUIRE(hash128.low == 0);
        REQUIRE(hash128.high == 0);
    }

    SECTION("Known Test Vectors 32-bit")
    {
        REQUIRE(Murmur3::Hash32("hello", 5, 0) == 0x248BFA47U);
        REQUIRE(Murmur3::Hash32("The quick brown fox jumps over the lazy dog", 43, 0) == 0x2E4FF723U);
    }

    SECTION("Different Seeds Produce Different Hashes")
    {
        const char* str = "seed test";
        auto hash1 = Murmur3::Hash32(str, std::char_traits<char>::length(str), 0);
        auto hash2 = Murmur3::Hash32(str, std::char_traits<char>::length(str), 123);
        REQUIRE(hash1 != hash2);

        auto hash128_1 = Murmur3::Hash128(str, std::char_traits<char>::length(str), 0);
        auto hash128_2 = Murmur3::Hash128(str, std::char_traits<char>::length(str), 123);
        REQUIRE(hash128_1.low != hash128_2.low);
    }

    SECTION("Different Overloads Consistency")
    {
        const char* str = "consistency check";
        std::string_view sv(str);

        REQUIRE(Murmur3::Hash32(str) == Murmur3::Hash32(sv));
        REQUIRE(Murmur3::Hash32(str, std::char_traits<char>::length(str)) == Murmur3::Hash32(str));
        REQUIRE(
            Murmur3::Hash32(static_cast<const void*>(str), std::char_traits<char>::length(str)) == Murmur3::Hash32(str)
        );
    }

    SECTION("Hash128 Overloads Consistency")
    {
        const char* str = "128bit test";
        std::string_view sv(str);

        auto hash1 = Murmur3::Hash128(str);
        auto hash2 = Murmur3::Hash128(sv);
        REQUIRE(hash1 == hash2);

        auto hash3 = Murmur3::Hash128(str, std::char_traits<char>::length(str));
        REQUIRE(hash1 == hash3);
    }

    SECTION("Single Character")
    {
        REQUIRE(Murmur3::Hash32("a") != Murmur3::Hash32("b"));
        REQUIRE(Murmur3::Hash32("x") != Murmur3::Hash32("y"));
    }

    SECTION("Tail Processing 32-bit")
    {
        REQUIRE(Murmur3::Hash32("a") != 0);
        REQUIRE(Murmur3::Hash32("ab") != 0);
        REQUIRE(Murmur3::Hash32("abc") != 0);
        REQUIRE(Murmur3::Hash32("abcd") != 0);
        REQUIRE(Murmur3::Hash32("abcde") != 0);
    }

    SECTION("Tail Processing 128-bit")
    {
        for (int i = 1; i <= 15; ++i)
        {
            std::string str(i, 'x');
            auto hash = Murmur3::Hash128(str.data(), str.size());
            // clang-format off
            REQUIRE((hash.low != 0 || hash.high != 0));
            // clang-format on
        }
    }

    SECTION("Binary Data")
    {
        const unsigned char data[] = { 0x00, 0xFF, 0xAA, 0x55, 0x12, 0x34 };
        auto hash = Murmur3::Hash32(reinterpret_cast<const char*>(data), 6);
        REQUIRE(hash != 0);
    }

    SECTION("Long Strings")
    {
        std::string longStr(500, 'M');
        auto hash32 = Murmur3::Hash32(longStr.data(), longStr.size());
        REQUIRE(hash32 != 0);

        auto hash128 = Murmur3::Hash128(longStr.data(), longStr.size());
        // clang-format off
        REQUIRE((hash128.low != 0 || hash128.high != 0));
        // clang-format on
    }

    SECTION("Hash128 Equality Operator")
    {
        auto hash1 = Murmur3::Hash128("test");
        auto hash2 = Murmur3::Hash128("test");
        auto hash3 = Murmur3::Hash128("different");

        REQUIRE(hash1 == hash2);
        REQUIRE_FALSE(hash1 == hash3);
    }

    SECTION("Architecture-Sized Hash")
    {
        const char* str = "arch test";
        auto hash = Murmur3::Hash(str);

#if GP_ARCHITECTURE_64BIT
        REQUIRE(hash == Murmur3::Hash128(str).low);
#else
        REQUIRE(hash == Murmur3::Hash32(str));
#endif
    }

    SECTION("Compile-time Evaluation")
    {
        constexpr auto hash32 = Murmur3::Hash32("compile", 7, 0);
        REQUIRE(hash32 == Murmur3::Hash32("compile", 7, 0));

        constexpr auto hash128 = Murmur3::Hash128("time", 4, 0);
        REQUIRE(hash128 == Murmur3::Hash128("time", 4, 0));
    }

    SECTION("User-Defined Literal")
    {
        auto hash1 = "murmur"_murmur3;
        auto hash2 = Murmur3::Hash32("murmur");
        REQUIRE(hash1 == hash2);
    }

    SECTION("Deterministic Output")
    {
        const char* input = "deterministic";
        auto hash1 = Murmur3::Hash32(input);
        auto hash2 = Murmur3::Hash32(input);
        REQUIRE(hash1 == hash2);

        auto hash128_1 = Murmur3::Hash128(input);
        auto hash128_2 = Murmur3::Hash128(input);
        REQUIRE(hash128_1 == hash128_2);
    }

    SECTION("Block Boundary Testing")
    {
        std::string str16(16, 'B');
        std::string str32(32, 'B');
        std::string str33(33, 'B');

        auto hash1 = Murmur3::Hash128(str16.data(), str16.size());
        auto hash2 = Murmur3::Hash128(str32.data(), str32.size());
        auto hash3 = Murmur3::Hash128(str33.data(), str33.size());

        REQUIRE_FALSE(hash1 == hash2);
        REQUIRE_FALSE(hash2 == hash3);
    }
}
