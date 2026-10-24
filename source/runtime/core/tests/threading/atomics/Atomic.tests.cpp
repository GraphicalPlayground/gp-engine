// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#include "threading/atomics/Atomic.hpp"
#include "concepts/Concepts.hpp"
#include "memory/MemoryOrder.hpp"
#include <gtest/gtest.h>
#include <thread>
#include <vector>

namespace gp::thread::tests
{

enum class TestFlags : gp::UInt32
{
    None = 0x00000000u,
    Read = 0x00000001u,
    Write = 0x00000002u,
    Exec = 0x00000004u,
    All = Read | Write | Exec
};

struct PackedPod32
{
    gp::UInt16 index{ 0u };
    gp::UInt16 generation{ 0u };

    [[nodiscard]] constexpr bool operator==(const PackedPod32& other) const noexcept
    {
        return index == other.index && generation == other.generation;
    }
};

struct PackedPod64
{
    gp::UInt32 id{ 0u };
    gp::Float32 weight{ 0.0f };

    [[nodiscard]] constexpr bool operator==(const PackedPod64& other) const noexcept
    {
        return id == other.id && weight == other.weight;
    }
};

struct InvalidSizePod24Bit
{
    gp::UInt8 bytes[3]{};
};

struct InvalidSizePod128Bit
{
    gp::UInt64 low{ 0ULL };
    gp::UInt64 high{ 0ULL };
};

struct NonTrivialPod
{
    gp::UInt32 value{ 0u };

    NonTrivialPod() noexcept = default;

    NonTrivialPod(const NonTrivialPod& other) noexcept
        : value(other.value)
    {}

    ~NonTrivialPod() noexcept
    {}
};

constexpr bool testConstexprAtomic()
{
    Atomic<gp::Int32> counter(10);
    if (counter.load(MemoryOrder::Relaxed) != 10)
    {
        return false;
    }

    counter.store(25, MemoryOrder::Release);
    if (counter.load(MemoryOrder::Acquire) != 25)
    {
        return false;
    }

    if (counter.exchange(50, MemoryOrder::AcqRel) != 25 || counter.load() != 50)
    {
        return false;
    }

    gp::Int32 expected = 50;
    if (!counter.compareExchangeStrong(expected, 100) || counter.load() != 100)
    {
        return false;
    }

    expected = 999;
    if (counter.compareExchangeWeak(expected, 200) || expected != 100)
    {
        return false;
    }

    if (counter.fetchAdd(10) != 100 || counter.load() != 110)
    {
        return false;
    }

    if (counter.fetchSub(30) != 110 || counter.load() != 80)
    {
        return false;
    }

    Atomic<gp::UInt32> bits(0x0Fu);
    if (bits.fetchOr(0xF0u) != 0x0Fu || bits.load() != 0xFFu)
    {
        return false;
    }
    if (bits.fetchAnd(0xAAu) != 0xFFu || bits.load() != 0xAAu)
    {
        return false;
    }
    if (bits.fetchXor(0xFFu) != 0xAAu || bits.load() != 0x55u)
    {
        return false;
    }

    Atomic<gp::UInt32> bitMask(0u);
    if (bitMask.fetchBitSet(3u) || bitMask.load() != (1u << 3u))
    {
        return false;
    }
    if (!bitMask.fetchBitSet(3u))
    {
        return false;
    }
    if (!bitMask.fetchBitClear(3u) || bitMask.load() != 0u)
    {
        return false;
    }

    gp::Int32 values[4] = { 10, 20, 30, 40 };
    Atomic<gp::Int32*> ptrAtomic(&values[0]);
    if (ptrAtomic.fetchAdd(2) != &values[0] || ptrAtomic.load() != &values[2])
    {
        return false;
    }
    if (ptrAtomic.fetchSub(1) != &values[2] || ptrAtomic.load() != &values[1])
    {
        return false;
    }

    return true;
}

static_assert(testConstexprAtomic(), "Constexpr Atomic evaluation failed.");

TEST(AtomicConceptTest, ValidAndInvalidAtomicValueTypes)
{
    EXPECT_TRUE(AtomicValue<bool>);
    EXPECT_TRUE(AtomicValue<gp::Int8>);
    EXPECT_TRUE(AtomicValue<gp::UInt8>);
    EXPECT_TRUE(AtomicValue<gp::Int16>);
    EXPECT_TRUE(AtomicValue<gp::UInt16>);
    EXPECT_TRUE(AtomicValue<gp::Int32>);
    EXPECT_TRUE(AtomicValue<gp::UInt32>);
    EXPECT_TRUE(AtomicValue<gp::Int64>);
    EXPECT_TRUE(AtomicValue<gp::UInt64>);

    EXPECT_TRUE(AtomicValue<gp::Float32>);
    EXPECT_TRUE(AtomicValue<gp::Float64>);

    EXPECT_TRUE(AtomicValue<int*>);
    EXPECT_TRUE(AtomicValue<void*>);
    EXPECT_TRUE(AtomicValue<TestFlags>);
    EXPECT_TRUE(AtomicValue<PackedPod32>);
    EXPECT_TRUE(AtomicValue<PackedPod64>);

    EXPECT_FALSE(AtomicValue<const gp::Int32>);
    EXPECT_FALSE(AtomicValue<volatile gp::Int32>);
    EXPECT_FALSE(AtomicValue<const volatile gp::Int32>);

    EXPECT_FALSE(AtomicValue<void>);
    EXPECT_FALSE(AtomicValue<InvalidSizePod24Bit>);
    EXPECT_FALSE(AtomicValue<InvalidSizePod128Bit>);
    EXPECT_FALSE(AtomicValue<NonTrivialPod>);
}

TEST(AtomicConceptTest, NonCopyableAndNonMovableInvariants)
{
    EXPECT_FALSE(gp::concepts::IsCopyConstructible<Atomic<gp::Int32>>);
    EXPECT_FALSE(gp::concepts::IsCopyAssignable<Atomic<gp::Int32>>);
    EXPECT_FALSE(gp::concepts::IsMoveConstructible<Atomic<gp::Int32>>);
    EXPECT_FALSE(gp::concepts::IsMoveAssignable<Atomic<gp::Int32>>);
}

template <typename T>
class AtomicIntegralTest : public ::testing::Test
{};

using IntegralTypes =
    ::testing::Types<gp::Int8, gp::UInt8, gp::Int16, gp::UInt16, gp::Int32, gp::UInt32, gp::Int64, gp::UInt64>;

TYPED_TEST_SUITE(AtomicIntegralTest, IntegralTypes);

TYPED_TEST(AtomicIntegralTest, ConstructionAndLockFreeGuarantee)
{
    Atomic<TypeParam> defaultAtomic;
    EXPECT_EQ(defaultAtomic.load(), TypeParam{ 0 });
    EXPECT_TRUE(defaultAtomic.isLockFree());
    EXPECT_TRUE(Atomic<TypeParam>::kIsAlwaysLockFree);
    EXPECT_EQ(sizeof(Atomic<TypeParam>), sizeof(TypeParam));
    EXPECT_EQ(alignof(Atomic<TypeParam>), sizeof(TypeParam));

    Atomic<TypeParam> initializedAtomic(TypeParam{ 42 });
    EXPECT_EQ(initializedAtomic.load(), TypeParam{ 42 });
}

TYPED_TEST(AtomicIntegralTest, LoadAndStoreAllMemoryOrders)
{
    Atomic<TypeParam> value(TypeParam{ 0 });

    value.store(TypeParam{ 11 }, MemoryOrder::Relaxed);
    EXPECT_EQ(value.load(MemoryOrder::Relaxed), TypeParam{ 11 });

    value.store(TypeParam{ 22 }, MemoryOrder::Release);
    EXPECT_EQ(value.load(MemoryOrder::Acquire), TypeParam{ 22 });
    EXPECT_EQ(value.load(MemoryOrder::Consume), TypeParam{ 22 });

    value.store(TypeParam{ 33 }, MemoryOrder::SeqCst);
    EXPECT_EQ(value.load(MemoryOrder::SeqCst), TypeParam{ 33 });
}

TYPED_TEST(AtomicIntegralTest, Exchange)
{
    Atomic<TypeParam> value(TypeParam{ 10 });

    TypeParam prev = value.exchange(TypeParam{ 20 }, MemoryOrder::Relaxed);
    EXPECT_EQ(prev, TypeParam{ 10 });
    EXPECT_EQ(value.load(), TypeParam{ 20 });

    prev = value.exchange(TypeParam{ 30 }, MemoryOrder::AcqRel);
    EXPECT_EQ(prev, TypeParam{ 20 });
    EXPECT_EQ(value.load(), TypeParam{ 30 });

    prev = value.exchange(TypeParam{ 40 }, MemoryOrder::SeqCst);
    EXPECT_EQ(prev, TypeParam{ 30 });
    EXPECT_EQ(value.load(), TypeParam{ 40 });
}

TYPED_TEST(AtomicIntegralTest, CompareExchangeStrongAndWeak)
{
    Atomic<TypeParam> value(TypeParam{ 15 });

    TypeParam expected = TypeParam{ 15 };
    EXPECT_TRUE(value.compareExchangeStrong(expected, TypeParam{ 25 }, MemoryOrder::AcqRel, MemoryOrder::Acquire));
    EXPECT_EQ(expected, TypeParam{ 15 });
    EXPECT_EQ(value.load(), TypeParam{ 25 });

    expected = TypeParam{ 15 };
    EXPECT_FALSE(value.compareExchangeStrong(expected, TypeParam{ 99 }, MemoryOrder::SeqCst, MemoryOrder::Release));
    EXPECT_EQ(expected, TypeParam{ 25 });
    EXPECT_EQ(value.load(), TypeParam{ 25 });

    expected = value.load(MemoryOrder::Relaxed);
    while (!value.compareExchangeWeak(expected, TypeParam{ 50 }, MemoryOrder::Release, MemoryOrder::Relaxed))
    {
    }
    EXPECT_EQ(value.load(), TypeParam{ 50 });

    expected = TypeParam{ 10 };
    EXPECT_FALSE(value.compareExchangeWeak(expected, TypeParam{ 75 }));
    EXPECT_EQ(expected, TypeParam{ 50 });
    EXPECT_EQ(value.load(), TypeParam{ 50 });
}

TYPED_TEST(AtomicIntegralTest, FetchAddAndSub)
{
    Atomic<TypeParam> value(TypeParam{ 20 });

    EXPECT_EQ(value.fetchAdd(TypeParam{ 5 }, MemoryOrder::Relaxed), TypeParam{ 20 });
    EXPECT_EQ(value.load(), TypeParam{ 25 });

    EXPECT_EQ(value.fetchSub(TypeParam{ 10 }, MemoryOrder::AcqRel), TypeParam{ 25 });
    EXPECT_EQ(value.load(), TypeParam{ 15 });
}

TYPED_TEST(AtomicIntegralTest, BitwiseFetchOperations)
{
    Atomic<TypeParam> value(TypeParam{ 0x0F });

    EXPECT_EQ(value.fetchOr(TypeParam{ 0x30 }, MemoryOrder::Relaxed), TypeParam{ 0x0F });
    EXPECT_EQ(value.load(), TypeParam{ 0x3F });

    EXPECT_EQ(value.fetchAnd(TypeParam{ 0x33 }, MemoryOrder::AcqRel), TypeParam{ 0x3F });
    EXPECT_EQ(value.load(), TypeParam{ 0x33 });

    EXPECT_EQ(value.fetchXor(TypeParam{ 0x11 }, MemoryOrder::SeqCst), TypeParam{ 0x33 });
    EXPECT_EQ(value.load(), TypeParam{ 0x22 });
}

TYPED_TEST(AtomicIntegralTest, FetchBitSetAndClear)
{
    Atomic<TypeParam> value(TypeParam{ 0 });

    EXPECT_FALSE(value.fetchBitSet(2u, MemoryOrder::AcqRel));
    EXPECT_EQ(value.load(), static_cast<TypeParam>(1u << 2u));

    EXPECT_TRUE(value.fetchBitSet(2u, MemoryOrder::AcqRel));
    EXPECT_EQ(value.load(), static_cast<TypeParam>(1u << 2u));

    EXPECT_TRUE(value.fetchBitClear(2u, MemoryOrder::AcqRel));
    EXPECT_EQ(value.load(), TypeParam{ 0 });

    EXPECT_FALSE(value.fetchBitClear(2u, MemoryOrder::AcqRel));
}

TYPED_TEST(AtomicIntegralTest, ErgonomicOperators)
{
    Atomic<TypeParam> value;

    value = TypeParam{ 10 };
    TypeParam implicitLoad = value;
    EXPECT_EQ(implicitLoad, TypeParam{ 10 });

    EXPECT_EQ(++value, TypeParam{ 11 });
    EXPECT_EQ(value++, TypeParam{ 11 });
    EXPECT_EQ(value.load(), TypeParam{ 12 });

    EXPECT_EQ(--value, TypeParam{ 11 });
    EXPECT_EQ(value--, TypeParam{ 11 });
    EXPECT_EQ(value.load(), TypeParam{ 10 });

    EXPECT_EQ(value += TypeParam{ 5 }, TypeParam{ 15 });
    EXPECT_EQ(value -= TypeParam{ 3 }, TypeParam{ 12 });

    value = TypeParam{ 0x06 };
    EXPECT_EQ(value |= TypeParam{ 0x01 }, TypeParam{ 0x07 });
    EXPECT_EQ(value &= TypeParam{ 0x03 }, TypeParam{ 0x03 });
    EXPECT_EQ(value ^= TypeParam{ 0x01 }, TypeParam{ 0x02 });
}

template <typename T>
class AtomicFloatTest : public ::testing::Test
{};

using FloatingPointTypes = ::testing::Types<gp::Float32, gp::Float64>;
TYPED_TEST_SUITE(AtomicFloatTest, FloatingPointTypes);

TYPED_TEST(AtomicFloatTest, LoadStoreExchangeAndCAS)
{
    Atomic<TypeParam> value(TypeParam{ 1.5 });
    EXPECT_EQ(value.load(), TypeParam{ 1.5 });

    value.store(TypeParam{ 3.25 }, MemoryOrder::Release);
    EXPECT_EQ(value.load(MemoryOrder::Acquire), TypeParam{ 3.25 });

    EXPECT_EQ(value.exchange(TypeParam{ -7.5 }), TypeParam{ 3.25 });
    EXPECT_EQ(value.load(), TypeParam{ -7.5 });

    TypeParam expected = TypeParam{ -7.5 };
    EXPECT_TRUE(value.compareExchangeStrong(expected, TypeParam{ 10.0 }));
    EXPECT_EQ(value.load(), TypeParam{ 10.0 });
}

TYPED_TEST(AtomicFloatTest, BitwiseSignedZeroCompareExchange)
{
    Atomic<TypeParam> value(TypeParam{ +0.0 });
    TypeParam negativeZero = TypeParam{ -0.0 };

    EXPECT_FALSE(value.compareExchangeStrong(negativeZero, TypeParam{ 1.0 }));
    EXPECT_EQ(value.load(), TypeParam{ +0.0 });
}

TYPED_TEST(AtomicFloatTest, FetchAddAndSub)
{
    Atomic<TypeParam> value(TypeParam{ 2.5 });

    EXPECT_NEAR(value.fetchAdd(TypeParam{ 1.25 }), TypeParam{ 2.5 }, TypeParam{ 1e-6 });
    EXPECT_NEAR(value.load(), TypeParam{ 3.75 }, TypeParam{ 1e-6 });

    EXPECT_NEAR(value.fetchSub(TypeParam{ 0.75 }), TypeParam{ 3.75 }, TypeParam{ 1e-6 });
    EXPECT_NEAR(value.load(), TypeParam{ 3.0 }, TypeParam{ 1e-6 });

    EXPECT_NEAR(value += TypeParam{ 2.0 }, TypeParam{ 5.0 }, TypeParam{ 1e-6 });
    EXPECT_NEAR(value -= TypeParam{ 1.5 }, TypeParam{ 3.5 }, TypeParam{ 1e-6 });
}

TEST(AtomicSpecializationTest, BoolOperations)
{
    Atomic<bool> flag(false);
    EXPECT_FALSE(flag.load());

    flag.store(true, MemoryOrder::Release);
    EXPECT_TRUE(flag.load(MemoryOrder::Acquire));

    EXPECT_TRUE(flag.exchange(false));
    EXPECT_FALSE(flag.load());

    bool expected = false;
    EXPECT_TRUE(flag.compareExchangeStrong(expected, true));
    EXPECT_TRUE(flag.load());
}

TEST(AtomicSpecializationTest, PointerArithmeticScaling)
{
    gp::UInt64 array[8] = { 10, 20, 30, 40, 50, 60, 70, 80 };
    Atomic<gp::UInt64*> ptr(&array[0]);

    EXPECT_EQ(ptr.load(), &array[0]);

    EXPECT_EQ(ptr.fetchAdd(3), &array[0]);
    EXPECT_EQ(ptr.load(), &array[3]);
    EXPECT_EQ(*ptr.load(), 40ULL);

    EXPECT_EQ(ptr.fetchSub(2), &array[3]);
    EXPECT_EQ(ptr.load(), &array[1]);
    EXPECT_EQ(*ptr.load(), 20ULL);

    EXPECT_EQ(++ptr, &array[2]);
    EXPECT_EQ(ptr++, &array[2]);
    EXPECT_EQ(ptr.load(), &array[3]);

    EXPECT_EQ(--ptr, &array[2]);
    EXPECT_EQ(ptr--, &array[2]);
    EXPECT_EQ(ptr.load(), &array[1]);

    EXPECT_EQ(ptr += 4, &array[5]);
    EXPECT_EQ(ptr -= 3, &array[2]);
}

TEST(AtomicSpecializationTest, EnumBitmaskOperations)
{
    Atomic<TestFlags> flags(TestFlags::None);
    EXPECT_EQ(flags.load(), TestFlags::None);

    EXPECT_EQ(flags.fetchOr(TestFlags::Read), TestFlags::None);
    EXPECT_EQ(flags.load(), TestFlags::Read);

    flags |= TestFlags::Write;
    EXPECT_EQ(flags.load(), static_cast<TestFlags>(0x3u));

    flags ^= TestFlags::Read;
    EXPECT_EQ(flags.load(), TestFlags::Write);

    flags &= TestFlags::Exec;
    EXPECT_EQ(flags.load(), TestFlags::None);
}

TEST(AtomicSpecializationTest, TriviallyCopyablePackedPods)
{
    // clang-format off
    Atomic<PackedPod32> handle32({ 1u, 100u });
    EXPECT_EQ(handle32.load(), (PackedPod32{ 1u, 100u }));

    PackedPod32 expected32{ 1u, 100u };
    EXPECT_TRUE(handle32.compareExchangeStrong(expected32, PackedPod32{ 2u, 101u }));
    EXPECT_EQ(handle32.load(), (PackedPod32{ 2u, 101u }));

    Atomic<PackedPod64> payload64({ 42u, 0.75f });
    PackedPod64 prev64 = payload64.exchange(PackedPod64{ 99u, 1.0f });
    EXPECT_EQ(prev64, (PackedPod64{ 42u, 0.75f }));
    EXPECT_EQ(payload64.load(), (PackedPod64{ 99u, 1.0f }));
    // clang-format on
}

TEST(AtomicConcurrencyTest, MultiThreadedCompareExchangeWeakLoop)
{
    constexpr gp::UInt32 kThreadCount = 8u;
    constexpr gp::UInt32 kIterationsPerThread = 10000u;

    Atomic<PackedPod64> sharedState({ 0u, 0.0f });

    std::vector<std::thread> workers;
    workers.reserve(kThreadCount);

    for (gp::UInt32 t = 0u; t < kThreadCount; ++t)
    {
        workers.emplace_back([&sharedState]()
        {
            for (gp::UInt32 i = 0u; i < kIterationsPerThread; ++i)
            {
                PackedPod64 expected = sharedState.load(MemoryOrder::Relaxed);
                PackedPod64 desired{};
                do
                {
                    desired.id = expected.id + 1u;
                    desired.weight = expected.weight + 0.5f;
                }
                while (!sharedState.compareExchangeWeak(expected, desired, MemoryOrder::AcqRel, MemoryOrder::Relaxed));
            }
        });
    }

    for (auto& worker: workers)
    {
        worker.join();
    }

    const PackedPod64 finalState = sharedState.load(MemoryOrder::Acquire);
    const gp::UInt32 totalOps = kThreadCount * kIterationsPerThread;

    EXPECT_EQ(finalState.id, totalOps);
    EXPECT_FLOAT_EQ(finalState.weight, static_cast<gp::Float32>(totalOps) * 0.5f);
}

}   // namespace gp::thread::tests
