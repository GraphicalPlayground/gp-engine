// Copyright (c) - Mallory SCOTTON. All rights reserved.

#include "Crypto/Hash/HashCombine.hpp"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("HashCombine Utilities", "[GP][Core][Crypto][Hash][HashCombine]")
{
    using namespace GP::Crypto;

    SECTION("Combine Two Values")
    {
        auto hash1 = HashCombine::Combine(0, 123);
        auto hash2 = HashCombine::Combine(0, 456);
        REQUIRE(hash1 != hash2);
        REQUIRE(hash1 != 0);
    }

    SECTION("Combine Order Matters")
    {
        auto hash1 = HashCombine::Combine(100, 200);
        auto hash2 = HashCombine::Combine(200, 100);
        REQUIRE(hash1 != hash2);
    }

    SECTION("Combine32 Specific")
    {
        GP::UInt32 seed = 42U;
        auto hash1 = HashCombine::Combine32(seed, 100U);
        auto hash2 = HashCombine::Combine32(seed, 200U);
        REQUIRE(hash1 != hash2);
    }

    SECTION("Combine64 Specific")
    {
        GP::UInt64 seed = 42ULL;
        auto hash1 = HashCombine::Combine64(seed, 100ULL);
        auto hash2 = HashCombine::Combine64(seed, 200ULL);
        REQUIRE(hash1 != hash2);
    }

    SECTION("Multiple Values")
    {
        auto hash = HashCombine::Multiple(0, 1, 2, 3, 4, 5);
        REQUIRE(hash != 0);

        auto single = HashCombine::Combine(0, 1);
        single = HashCombine::Combine(single, 2);
        single = HashCombine::Combine(single, 3);
        single = HashCombine::Combine(single, 4);
        single = HashCombine::Combine(single, 5);

        REQUIRE(hash == single);
    }

    SECTION("Multiple with Different Seeds")
    {
        auto hash1 = HashCombine::Multiple(0, 1, 2, 3);
        auto hash2 = HashCombine::Multiple(100, 1, 2, 3);
        REQUIRE(hash1 != hash2);
    }

    SECTION("Empty Multiple")
    {
        HashCombine::HashType seed = 42;
        auto result = HashCombine::Multiple(seed);
        REQUIRE(result == seed);
    }

    SECTION("Iterative Combining")
    {
        HashCombine::HashType hash = 0;
        hash = HashCombine::Combine(hash, 10);
        hash = HashCombine::Combine(hash, 20);
        hash = HashCombine::Combine(hash, 30);

        auto directHash = HashCombine::Multiple(0, 10, 20, 30);
        REQUIRE(hash == directHash);
    }

    SECTION("Architecture-Sized Hash")
    {
        auto hash = HashCombine::Combine(0, 999);
#if GP_ARCHITECTURE_64BIT
        REQUIRE(sizeof(hash) == 8);
#else
        REQUIRE(sizeof(hash) == 4);
#endif
    }

    SECTION("Same Value Different Seeds")
    {
        auto hash1 = HashCombine::Combine(1, 100);
        auto hash2 = HashCombine::Combine(2, 100);
        auto hash3 = HashCombine::Combine(3, 100);

        REQUIRE(hash1 != hash2);
        REQUIRE(hash2 != hash3);
        REQUIRE(hash1 != hash3);
    }

    SECTION("Variadic Type Casting")
    {
        auto hash1 = HashCombine::Multiple(0, 1U, 2ULL, 3);
        auto hash2 = HashCombine::Multiple(
            0,
            static_cast<HashCombine::HashType>(1),
            static_cast<HashCombine::HashType>(2),
            static_cast<HashCombine::HashType>(3)
        );
        REQUIRE(hash1 == hash2);
    }

    SECTION("Compile-time Evaluation")
    {
        constexpr auto hash1 = HashCombine::Combine(100, 200);
        constexpr auto hash2 = HashCombine::Multiple(0, 1, 2, 3);
        REQUIRE(hash1 != 0);
        REQUIRE(hash2 != 0);
    }

    SECTION("Deterministic Output")
    {
        auto hash1 = HashCombine::Combine(42, 777);
        auto hash2 = HashCombine::Combine(42, 777);
        REQUIRE(hash1 == hash2);

        auto multi1 = HashCombine::Multiple(0, 1, 2, 3, 4);
        auto multi2 = HashCombine::Multiple(0, 1, 2, 3, 4);
        REQUIRE(multi1 == multi2);
    }

    SECTION("Large Number of Values")
    {
        auto hash = HashCombine::Multiple(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
        REQUIRE(hash != 0);
    }
}
