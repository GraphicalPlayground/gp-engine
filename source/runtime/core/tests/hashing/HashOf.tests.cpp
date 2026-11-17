// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "hashing/HashOf.hpp"
#include "containers/arrays/Array.hpp"
#include "containers/strings/FixedString.hpp"
#include "containers/strings/Name.hpp"
#include "containers/strings/String.hpp"     // IWYU pragma: keep
#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "hashing/Crc32.hpp"
#include "hashing/Murmur3.hpp"
#include "hashing/Sha256.hpp"
#include "hashing/SipHash.hpp"
#include "hashing/Wyhash.hpp"
#include "hashing/XxHash.hpp"
#include "memory/pointers/RefCountedPtr.hpp"
#include "memory/pointers/UniquePtr.hpp"
#include "templates/Interval.hpp"
#include "templates/Optional.hpp"
#include "templates/Pair.hpp"
#include <gtest/gtest.h>

namespace gp::hashing::tests
{

namespace
{

enum class RenderStage : gp::UInt8
{
    DepthPrepass = 0u,
    GBuffer = 1u,
    Lighting = 2u
};

struct MemberAppendKey
{
    gp::UInt32 pipelineId{ 0u };
    gp::Float32 bias{ 0.0f };

    template <typename InHasher>
    constexpr void hashAppend(InHasher& hasher) const noexcept
    {
        hasher.append(pipelineId);
        hasher.append(bias);
    }
};

struct AdlAppendKey
{
    gp::Int32 x{ 0 };
    gp::Int32 y{ 0 };

    template <typename InHasher>
    friend constexpr void gpHashAppend(InHasher& hasher, const AdlAppendKey& value) noexcept
    {
        hasher.append(value.x);
        hasher.append(value.y);
    }
};

struct TraitSpecializedKey
{
    gp::UInt64 guid{ 0ULL };
    gp::UInt16 generation{ 0u };
};

struct MemberHashOnlyKey
{
    gp::UInt64 id{ 0ULL };

    [[nodiscard]] constexpr gp::UInt64 hash() const noexcept
    {
        return splitMix64(id ^ 0xA5A5A5A5A5A5A5A5ULL);
    }
};

struct TestRefObject final : public gp::IntrusiveRefCounted<TestRefObject>
{
    gp::Int32 payload{ 0 };

    explicit TestRefObject(const gp::Int32 value) noexcept
        : payload(value)
    {}
};

}   // namespace

}   // namespace gp::hashing::tests

namespace gp::hashing
{

template <>
struct HashTraits<tests::TraitSpecializedKey>
{
    [[nodiscard]] static constexpr gp::UInt64 hash(const tests::TraitSpecializedKey& key) noexcept
    {
        return hashCombine(hashInt64(key.guid), hashInt64(key.generation));
    }

    template <typename InHasher>
    static constexpr void append(InHasher& hasher, const tests::TraitSpecializedKey& key) noexcept
    {
        hasher.append(key.guid);
        hasher.append(key.generation);
    }
};

}   // namespace gp::hashing

namespace gp::hashing::tests
{

TEST(HashOfTest, PrimitivesStringsAndPointers)
{
    static_assert(hashOf(nullptr) == 0ULL);
    static_assert(hashOf(false) != hashOf(true));
    static_assert(hashOf(-0.0f) == hashOf(+0.0f));
    static_assert(hashOf(-0.0) == hashOf(+0.0));
    static_assert(hashOf(RenderStage::DepthPrepass) != hashOf(RenderStage::GBuffer));
    static_assert(hashOf("123456789") == Fnv1a64::hash("123456789"));
    static_assert(hashOf<Crc32>("123456789") == 0xCBF43926u);
    static_assert(hashOf<Crc32C>("123456789") == 0xE3069283u);
    static_assert(hashOf<XxHash64>("The quick brown fox jumps over the lazy dog") == 0x0B242D361FDA71BCULL);
    static_assert(hashOf<Murmur3_128>("abc") == Murmur3_128::hash("abc"));

    const char* const cstr = "AlbedoTexture";
    const gp::StringView view(cstr);
    const gp::String owned(cstr);
    const gp::FixedString<32> fixed(cstr);

    EXPECT_EQ(hashOf(cstr), Fnv1a64::hash(view));
    EXPECT_EQ(hashOf(view), view.hash());
    EXPECT_EQ(hashOf(owned), owned.hash());
    EXPECT_EQ(hashOf(fixed), fixed.hash());
    EXPECT_EQ(hashOf<Crc32>(owned), Crc32::hash("AlbedoTexture"));

    const gp::Int32 stackVar = 10;
    const void* const nullVoid = nullptr;
    EXPECT_EQ(hashOf(nullVoid), 0ULL);
    EXPECT_EQ(hashOf(static_cast<const void*>(&stackVar)), hashPointer(&stackVar));

    const gp::Int32 rawArr[3] = { 1, 2, 3 };
    const gp::Int32 rawArrRev[3] = { 3, 2, 1 };
    EXPECT_NE(hashOf(rawArr), hashOf(rawArrRev));
    EXPECT_NE(hashOf<Crc32>(rawArr), hashOf<Crc32>(rawArrRev));
}

TEST(HashOfTest, EngineTypesContainersAndSmartPointers)
{
    const gp::Name boneA("Spine_01_2");
    const gp::Name boneB("spine_01_2");
    const gp::Name boneC("Spine_01_3");
    EXPECT_EQ(hashOf(boneA), boneA.hash());
    EXPECT_EQ(hashOf(boneA), hashOf(boneB));
    EXPECT_NE(hashOf(boneA), hashOf(boneC));
    EXPECT_EQ(hashOf<Crc32>(boneA), hashOf<Crc32>(boneB));
    EXPECT_NE(hashOf<Crc32>(boneA), hashOf<Crc32>(boneC));

    const gp::Pair<gp::Int32, gp::StringView> pairA(7, "LOD0");
    const gp::Pair<gp::Int32, gp::StringView> pairB(7, "LOD1");
    EXPECT_NE(hashOf(pairA), hashOf(pairB));
    EXPECT_NE(hashOf<XxHash64>(pairA), hashOf<XxHash64>(pairB));

    const gp::Optional<gp::Int32> emptyOpt{};
    const gp::Optional<gp::Int32> zeroOpt(0);
    const gp::Optional<gp::Int32> oneOpt(1);
    EXPECT_EQ(hashOf(emptyOpt), 0ULL);
    EXPECT_NE(hashOf(emptyOpt), hashOf(zeroOpt));
    EXPECT_NE(hashOf(zeroOpt), hashOf(oneOpt));
    EXPECT_NE(hashOf<Crc32>(emptyOpt), hashOf<Crc32>(zeroOpt));

    const gp::Interval<gp::UInt32> ivA(0u, 64u);
    const gp::Interval<gp::UInt32> ivB(64u, 128u);
    EXPECT_NE(hashOf(ivA), hashOf(ivB));
    EXPECT_NE(hashOf<Fnv1a32>(ivA), hashOf<Fnv1a32>(ivB));

    const gp::Array<gp::UInt8, 4> byteArr{ 1u, 2u, 3u, 4u };
    EXPECT_EQ(hashOf(byteArr), Fnv1a64::hash(byteArr.data(), byteArr.size()));
    EXPECT_EQ(hashOf<Crc32>(byteArr), Crc32::hash(byteArr.data(), byteArr.size()));

    const gp::Array<gp::Int32, 3> intArr{ 10, 20, 30 };
    const gp::Array<gp::Int32, 3> intArrOther{ 10, 20, 31 };
    EXPECT_NE(hashOf(intArr), hashOf(intArrOther));
    EXPECT_NE(hashOf<XxHash32>(intArr), hashOf<XxHash32>(intArrOther));

    gp::UniquePtr<gp::Int32> nullUnique;
    gp::UniquePtr<gp::Int32> liveUnique = gp::makeUnique<gp::Int32>(99);
    EXPECT_EQ(hashOf(nullUnique), 0ULL);
    EXPECT_EQ(hashOf(liveUnique), hashPointer(liveUnique.get()));

    gp::RefCountedPtr<TestRefObject> refObj(new TestRefObject(42));
    EXPECT_EQ(hashOf(refObj), refObj.getHash());
    EXPECT_NE(hashOf<Crc32>(refObj), 0u);
}

TEST(HashOfTest, CustomHooksHashValuesAndUnifiedEntryPoints)
{
    constexpr MemberAppendKey kMemA{ 42u, -0.0f };
    constexpr MemberAppendKey kMemB{ 42u, +0.0f };
    static_assert(hashOf(kMemA) == hashOf(kMemB));
    static_assert(hashOf<Crc32>(kMemA) == hashOf<Crc32>(kMemB));
    static_assert(hashOf<XxHash64>(kMemA) == hashOf<XxHash64>(kMemB));

    constexpr AdlAppendKey kAdlA{ 3, 4 };
    constexpr AdlAppendKey kAdlB{ 4, 3 };
    static_assert(hashOf(kAdlA) != hashOf(kAdlB));
    static_assert(hashOf<Murmur3_32>(kAdlA) != hashOf<Murmur3_32>(kAdlB));

    constexpr TraitSpecializedKey kTrait{ 9001ULL, 2u };
    static_assert(hashOf(kTrait) == hashCombine(hashInt64(9001ULL), hashInt64(gp::UInt16{ 2u })));
    static_assert(hashOf<Murmur3_128>(kTrait).isValid());

    constexpr MemberHashOnlyKey kMemOnly{ 55ULL };
    static_assert(hashOf(kMemOnly) == kMemOnly.hash());

    // Variable-length strings inside hashValues stay length-framed to prevent concatenation collisions.
    EXPECT_NE(
        hashValues<XxHash64>(gp::StringView("ab"), gp::StringView("c")),
        hashValues<XxHash64>(gp::StringView("a"), gp::StringView("bc"))
    );
    EXPECT_NE(hashValues<Crc32>("ab", "c"), hashValues<Crc32>("a", "bc"));

    // Unified hashBytes, hashString, hashStringIgnoreCase, hashRange, and value hashCombine helpers.
    const gp::UInt8 bytes[3] = { 'a', 'b', 'c' };
    EXPECT_EQ(hashBytes<Crc32>(bytes, 3u), 0x352441C2u);
    EXPECT_EQ(hashBytes<XxHash32>(static_cast<const void*>(bytes), 3u), 0x32D153FFu);
    EXPECT_EQ(hashString<Crc32>(gp::StringView("123456789")), 0xCBF43926u);
    EXPECT_EQ(hashString<Crc32>("ABC", HashCase::IgnoreCase), Crc32::hash("abc"));
    EXPECT_EQ(hashStringIgnoreCase<Fnv1a64>(gp::StringView("MESH")), Fnv1a64::hash("mesh"));
    EXPECT_EQ(hashStringIgnoreCase<Fnv1a64>("MESH"), Fnv1a64::hash("mesh"));

    const gp::Int32 seq[4] = { 1, 2, 3, 4 };
    const gp::Int32 seqShort[3] = { 1, 2, 3 };
    EXPECT_NE(hashRange<XxHash64>(seq, seq + 4), hashRange<XxHash64>(seqShort, seqShort + 3));

    const gp::UInt64 combined = hashCombine(1ULL, 42u, gp::StringView("Draw"), 3.5f);
    const gp::UInt64 combinedFast = hashCombineFast(1ULL, 42u, gp::StringView("Draw"), 3.5f);
    EXPECT_NE(combined, 0ULL);
    EXPECT_NE(combinedFast, 0ULL);
    EXPECT_NE(combined, combinedFast);

    static_assert(hashCombine(10ULL, 20ULL) == hashCombine(gp::UInt64{ 10u }, gp::UInt64{ 20u }));
    static_assert(hashCombineFast(10ULL, 20ULL) == hashCombineFast(gp::UInt64{ 10u }, gp::USize{ 20u }));
    static_assert(hashCombine(1ULL, 42u) == hashCombine(1ULL, hashOf(gp::UInt32{ 42u })));
}

TEST(HashOfTest, UnifiedHashApiDomainSeparationAndUniquePodBytes)
{
    struct alignas(4) PackedPipelineKey
    {
        gp::UInt32 vertexLayoutId{ 0u };
        gp::UInt32 blendStateId{ 0u };
    };
    static_assert(concepts::IsUniqueRepresentation<PackedPipelineKey>);

    // 1. Unified single-value & multi-field `gp::hashing::hash(...)` overloads.
    static_assert(hash(42u) == hashOf(42u));
    static_assert(hash<XxHash64>("Shader") == XxHash64::hash("Shader"));
    static_assert(hash<HashAlgorithm::XxHash64>("Shader") == XxHash64::hash("Shader"));
    static_assert(hash<HashAlgorithm::Crc32>("123456789") == 0xCBF43926u);
    static_assert(hash<HashAlgorithm::Wyhash64>(10u, 20u) == hashValues<Wyhash64>(10u, 20u));
    static_assert(hash<HashAlgorithm::Sha256>("abc") == Sha256::hash("abc"));
    static_assert(hash<XxHash64>("Shader", HashCase::IgnoreCase) == XxHash64::hash("shader"));
    static_assert(hash<Fnv1a64>(gp::StringView("Shader"), HashCase::IgnoreCase) == Fnv1a64::hash("shader"));

    const gp::UInt8 raw[3] = { 'a', 'b', 'c' };
    const gp::Byte byteArr[3] = { gp::Byte{ 'a' }, gp::Byte{ 'b' }, gp::Byte{ 'c' } };
    EXPECT_EQ(hash<Sha256>(raw, 3u), Sha256::hash("abc"));
    EXPECT_EQ(hash<Wyhash64>(byteArr, 3u), Wyhash64::hash("abc"));
    EXPECT_EQ(hash<Crc32>(static_cast<const void*>(raw), 3u), 0x352441C2u);
    EXPECT_EQ(hash<HashAlgorithm::XxHash64>(raw, 3u), XxHash64::hash("abc"));

    // 2. Unified runtime `HashAlgorithm` dispatch.
    EXPECT_EQ(hash(HashAlgorithm::Crc32, raw, 3u).toUInt32(), 0x352441C2u);
    EXPECT_EQ(hash(HashAlgorithm::XxHash64, static_cast<const void*>(raw), 3u).toUInt64(), XxHash64::hash("abc"));
    EXPECT_EQ(hash(HashAlgorithm::Sha256, gp::StringView("abc")).toHash256(), Sha256::hash("abc"));
    EXPECT_TRUE(hash(HashAlgorithm::Wyhash64, PackedPipelineKey{ 1u, 2u }.vertexLayoutId).isValid());

    const gp::Pair<gp::Int32, gp::Int32> pairKey(7, 9);
    EXPECT_EQ(hash(HashAlgorithm::Fnv1a64, pairKey).toUInt64(), hashOf<Fnv1a64>(pairKey));

    // 3. Domain-separated hashing, seeded packs, and padding-free POD byte hashing.
    static_assert(
        hashDomain<XxHash64>("ShaderPSO", 10u, 20u) != hashDomain<XxHash64>("MaterialDef", 10u, 20u)
    );
    static_assert(hashValuesSeeded<Fnv1a64>(1ULL, 10u, 20u) != hashValuesSeeded<Fnv1a64>(2ULL, 10u, 20u));

    const PackedPipelineKey psoA{ 3u, 7u };
    const PackedPipelineKey psoB{ 3u, 8u };
    EXPECT_NE(hashUniqueBytes<XxHash64>(psoA), hashUniqueBytes<XxHash64>(psoB));
    EXPECT_EQ(hashUniqueBytes<Crc32>(psoA), Crc32::hash(static_cast<const void*>(&psoA), sizeof(psoA)));

    // 4. Hash256 HashTraits integration.
    constexpr Hash256 d256 = Sha256::hash("abc");
    static_assert(hash(d256) == d256.foldTo64());
    static_assert(hash<XxHash64>(d256) != 0ULL);

    // 5. Unified `makeHasher` factories.
    Wyhash64 concreteByType = makeHasher<Wyhash64>();
    Hasher<HashAlgorithm::Wyhash64> concreteByEnum = makeHasher<HashAlgorithm::Wyhash64>();
    DynamicHasher runtimeDyn = makeHasher(HashAlgorithm::Wyhash64);
    concreteByType.update("abc", 3u);
    concreteByEnum.update("abc", 3u);
    runtimeDyn.update("abc", 3u);
    EXPECT_EQ(concreteByType.finalize(), Wyhash64::hash("abc"));
    EXPECT_EQ(concreteByEnum.finalize(), Wyhash64::hash("abc"));
    EXPECT_EQ(runtimeDyn.finalize64(), Wyhash64::hash("abc"));
}

}   // namespace gp::hashing::tests
