// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "utils/VulkanUtil.hpp"
#include "containers/arrays/Array.hpp"
#include "containers/arrays/Vector.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "hashing/Crc32.hpp"
#include "hashing/HashOf.hpp"
#include <gtest/gtest.h>
#include <utility>

namespace gp::rhi::vulkan::tests
{

namespace
{

struct SamplePayload final
{
    gp::UInt32 a{ 0u };
    gp::UInt32 b{ 0u };
};

class CheckedKey final : public DataKey<CheckedKey, false>
{};

class AlwaysCompareKey final : public DataKey<AlwaysCompareKey, true>
{};

class HashOnlyKey final : public DataKeyBase<0>
{
public:
    void populate(const gp::UInt8 byteValue) noexcept
    {
        this->setData(&byteValue, 1u);
        this->getData(
            [byteValue, this](gp::Vector<gp::UInt8>& buf) noexcept -> void
        {
            buf.pushBack(byteValue);
            gp::hashing::Crc32 hasher;
            hasher.update(buf.data(), buf.size());
            this->m_hash = hasher.finalize();
        }
        );
    }

    [[nodiscard]] gp::UInt32 getHash() const noexcept
    {
        return this->m_hash;
    }

    [[nodiscard]] HashOnlyKey copyDeep() const noexcept
    {
        HashOnlyKey out;
        out.m_hash = this->m_hash;
        this->copyDataDeep(out);
        return out;
    }

    [[nodiscard]] HashOnlyKey copyShallow() const noexcept
    {
        HashOnlyKey out;
        out.m_hash = this->m_hash;
        this->copyDataShallow(out);
        return out;
    }

    [[nodiscard]] bool equals(const HashOnlyKey& rhs) const noexcept
    {
        return (this->m_hash == rhs.m_hash) && this->isDataEquals(rhs);
    }
};

}   // namespace

TEST(VulkanUtilTest, CopyAndReturnNotEqualDetectsChanges)
{
    gp::UInt32 value = 10u;
    EXPECT_FALSE(copyAndReturnNotEqual(value, 10u));
    EXPECT_EQ(value, 10u);

    EXPECT_TRUE(copyAndReturnNotEqual(value, 25u));
    EXPECT_EQ(value, 25u);

    EXPECT_FALSE(copyAndReturnNotEqual(value, 25u));

    gp::Int32 a = 1;
    gp::Int32 b = 2;
    gp::Int32* ptr = &a;
    EXPECT_FALSE(copyAndReturnNotEqual(ptr, &a));
    EXPECT_TRUE(copyAndReturnNotEqual(ptr, &b));
    EXPECT_EQ(ptr, &b);
}

TEST(VulkanUtilTest, DataKeyGenerationDeepShallowCopyMovesAndHashing)
{
    // 1. Known IEEE 802.3 CRC-32 test vector ("123456789" -> 0xCBF43926u) & gp::hashing::hashOf integration.
    {
        static constexpr char kStandardVector[9] = { '1', '2', '3', '4', '5', '6', '7', '8', '9' };
        AlwaysCompareKey crcKey;
        crcKey.generateFromData(kStandardVector, 9u);
        EXPECT_EQ(crcKey.getHash(), 0xCBF43926u);
        EXPECT_EQ(gp::hashing::hashOf(crcKey), static_cast<gp::UInt64>(crcKey.getHash()));
    }

    // 2. AlwaysCompareData = true (DataKeyBase<2>): generateFromObject, deep/shallow copy, regeneration, empty, move.
    {
        const SamplePayload p1{ 0x11223344u, 0x55667788u };
        const SamplePayload p2{ 0x11223344u, 0x55667789u };

        AlwaysCompareKey keyA;
        keyA.generateFromObject(p1);
        AlwaysCompareKey keyB;
        keyB.generateFromObject(p1);
        AlwaysCompareKey keyC;
        keyC.generateFromObject(p2);

        EXPECT_NE(keyA.getHash(), 0u);
        EXPECT_EQ(keyA.getHash(), keyB.getHash());
        EXPECT_NE(keyA.getHash(), keyC.getHash());
        EXPECT_TRUE(keyA == keyB);
        EXPECT_FALSE(keyA != keyB);
        EXPECT_FALSE(keyA == keyC);
        EXPECT_TRUE(keyA != keyC);
        EXPECT_EQ(keyA.getDataRef().size(), sizeof(SamplePayload));

        // Deep copy owns independent byte storage.
        AlwaysCompareKey deep = keyA.copyDeep();
        EXPECT_TRUE(deep == keyA);
        EXPECT_NE(deep.getDataRef().data(), keyA.getDataRef().data());

        // Shallow copy aliases source storage; regenerating it allocates independent storage without mutating source.
        AlwaysCompareKey shallow = keyA.copyShallow();
        EXPECT_TRUE(shallow == keyA);
        EXPECT_EQ(shallow.getDataRef().data(), keyA.getDataRef().data());

        shallow.generateFromObject(p2);
        EXPECT_NE(shallow.getDataRef().data(), keyA.getDataRef().data());
        EXPECT_TRUE(shallow == keyC);
        EXPECT_TRUE(keyA == keyB);

        // In-place regeneration with shorter payload clears stale trailing bytes.
        const gp::Array<gp::UInt8, 2u> shortBytes{ 0xAAu, 0xBBu };
        shallow.generateFromData(shortBytes.data(), 2u);
        EXPECT_EQ(shallow.getDataRef().size(), 2u);
        EXPECT_EQ(shallow.getDataRef()[0], 0xAAu);
        EXPECT_EQ(shallow.getDataRef()[1], 0xBBu);

        // Mutating deep copy payload while keeping same hash causes DataKeyBase<2> equality to return false.
        deep.getDataRef()[0] ^= 0xFFu;
        EXPECT_EQ(deep.getHash(), keyA.getHash());
        EXPECT_FALSE(deep == keyA);

        // Different lengths with identical hash also return false.
        deep.getDataRef().pushBack(0u);
        EXPECT_FALSE(deep == keyA);

        // Empty payload comparison on DataKeyBase<2>.
        AlwaysCompareKey empty1;
        empty1.generateFromData(nullptr, 0u);
        AlwaysCompareKey empty2;
        empty2.generateFromWriter(
            [](gp::Vector<gp::UInt8>&) noexcept -> void
        {
        }
        );
        EXPECT_TRUE(empty1 == empty2);

        // Move constructor and move assignment preserve hash/payload and reset source hash.
        AlwaysCompareKey movedCtor(std::move(keyB));
        EXPECT_EQ(keyB.getHash(), 0u);
        EXPECT_TRUE(movedCtor == keyA);

        AlwaysCompareKey movedAssign;
        movedAssign = std::move(movedCtor);
        EXPECT_EQ(movedCtor.getHash(), 0u);
        EXPECT_TRUE(movedAssign == keyA);
    }

    // 3. CheckedKey (AlwaysCompareData = false): generateFromObject, generateFromWriter, copyDeep, copyShallow.
    {
        const SamplePayload p1{ 1u, 2u };
        const SamplePayload p2{ 3u, 4u };
        CheckedKey objA;
        objA.generateFromObject(p1);
        CheckedKey objB;
        objB.generateFromObject(p2);
        EXPECT_TRUE(objA != objB);

        const CheckedKey objDeep = objA.copyDeep();
        const CheckedKey objShallow = objA.copyShallow();
        EXPECT_TRUE(objDeep == objA);
        EXPECT_TRUE(objShallow == objA);

        CheckedKey writerKey;
        writerKey.generateFromWriter(
            [](gp::Vector<gp::UInt8>& out) noexcept -> void
        {
            out.pushBack(1u);
            out.pushBack(2u);
            out.pushBack(3u);
            out.pushBack(4u);
        },
            16u
        );

        const gp::Array<gp::UInt8, 4u> rawBytes{ 1u, 2u, 3u, 4u };
        CheckedKey dataKey;
        dataKey.generateFromData(rawBytes.data(), static_cast<gp::UInt32>(rawBytes.size()));

        EXPECT_EQ(writerKey.getHash(), dataKey.getHash());
        EXPECT_TRUE(writerKey == dataKey);

        CheckedKey emptyA;
        emptyA.generateFromData(nullptr, 0u);
        CheckedKey emptyB;
        emptyB.generateFromWriter(
            [](gp::Vector<gp::UInt8>&) noexcept -> void
        {
        }
        );
        EXPECT_EQ(emptyA.getHash(), emptyB.getHash());
        EXPECT_TRUE(emptyA == emptyB);
    }

    // 4. Hash-only specialization (DataKeyBase<0>) computes hash without retaining backing bytes.
    {
        HashOnlyKey k1;
        k1.populate(0x5Au);
        HashOnlyKey k2 = k1.copyDeep();
        HashOnlyKey k3 = k1.copyShallow();
        HashOnlyKey kDiff;
        kDiff.populate(0xA5u);

        EXPECT_EQ(k1.getHash(), k2.getHash());
        EXPECT_EQ(k1.getHash(), k3.getHash());
        EXPECT_TRUE(k1.equals(k2));
        EXPECT_TRUE(k1.equals(k3));
        EXPECT_FALSE(k1.equals(kDiff));
    }
}

}   // namespace gp::rhi::vulkan::tests
