// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "hashing/HashRegistry.hpp"
#include "CoreMinimal.hpp"
#include "hashing/Crc32.hpp"
#include "hashing/Sha256.hpp"
#include "hashing/XxHash.hpp"
#include "platforms/base/system/Thread.hpp"
#include "threading/atomics/Atomic.hpp"
#include <gtest/gtest.h>

namespace gp::hashing::tests
{

namespace
{

struct RegistryWorkerContext
{
    BasicHashRegistry<256u, 8192u>* registry{ nullptr };
    gp::thread::Atomic<bool>* startGate{ nullptr };
    gp::UInt32 workerIndex{ 0u };
};

gp::UInt32 registryWorkerEntry(void* const userData)
{
    const RegistryWorkerContext* const ctx = static_cast<const RegistryWorkerContext*>(userData);
    if (ctx == nullptr || ctx->registry == nullptr || ctx->startGate == nullptr)
    {
        return 1u;
    }

    while (!ctx->startGate->load(gp::MemoryOrder::Acquire))
    {
        gp::platform::Thread::yieldExecution();
    }

    const gp::StringView names[4] = { "HeroMesh", "EnemyMesh", "AlbedoMap", "NormalMap" };
    for (gp::UInt32 iter = 0u; iter < 50u; ++iter)
    {
        const gp::StringView target = names[(ctx->workerIndex + iter) & 3u];
        const gp::UInt64 h = ctx->registry->registerString<XxHash64>(target);
        if (ctx->registry->lookup(h) != target)
        {
            return 2u;
        }
    }

    return 0u;
}

}   // namespace

TEST(HashRegistryTest, RegisterLookupCollisionAndCapacityHandling)
{
    BasicHashRegistry<16u, 128u> registry;
    EXPECT_EQ(registry.size(), 0u);
    EXPECT_EQ(registry.capacity(), 16u);
    EXPECT_EQ(registry.usedArenaBytes(), 0u);
    EXPECT_EQ(registry.arenaCapacity(), 128u);
    EXPECT_EQ(registry.collisionCount(), 0u);

    // Empty strings are rejected safely.
    EXPECT_FALSE(registry.registerEntry(123ULL, ""));
    EXPECT_EQ(registry.size(), 0u);

    // Registering strings across multiple hash widths.
    const gp::UInt64 fnvDigest = registry.registerString("Player_Root");
    const gp::UInt32 crcDigest = registry.registerString<Crc32>("Bone_Head");
    const Hash256 shaDigest = registry.registerString<Sha256>("Shader_PBR");

    EXPECT_EQ(registry.size(), 3u);
    EXPECT_TRUE(registry.contains(fnvDigest));
    EXPECT_EQ(registry.lookup(fnvDigest), "Player_Root");
    EXPECT_EQ(registry.lookupDigest(crcDigest), "Bone_Head");
    EXPECT_EQ(registry.lookupDigest(shaDigest), "Shader_PBR");
    EXPECT_TRUE(registry.lookup(0xDEADBEEF12345678ULL).isEmpty());

    // Duplicate registration of the same hash and text succeeds idempotently without growing arena.
    const gp::USize arenaBefore = registry.usedArenaBytes();
    EXPECT_TRUE(registry.registerEntry(fnvDigest, "Player_Root"));
    EXPECT_EQ(registry.size(), 3u);
    EXPECT_EQ(registry.usedArenaBytes(), arenaBefore);

    // Distinct text with the same hash records a collision and refuses to overwrite.
    EXPECT_FALSE(registry.registerEntry(fnvDigest, "DifferentString"));
    EXPECT_EQ(registry.collisionCount(), 1u);
    EXPECT_EQ(registry.lookup(fnvDigest), "Player_Root");

    // Exhausting the small inline arena fails cleanly without memory corruption.
    const gp::StringView longText("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA");
    EXPECT_TRUE(registry.registerEntry(1001ULL, longText));
    EXPECT_FALSE(registry.registerEntry(1002ULL, longText));

    registry.clear();
    EXPECT_EQ(registry.size(), 0u);
    EXPECT_EQ(registry.usedArenaBytes(), 0u);
    EXPECT_EQ(registry.collisionCount(), 0u);
    EXPECT_FALSE(registry.contains(fnvDigest));
}

TEST(HashRegistryTest, ThreadSafeConcurrentRegistrationAndGlobalInstance)
{
    BasicHashRegistry<256u, 8192u> sharedRegistry;
    gp::thread::Atomic<bool> startGate(false);

    constexpr gp::UInt32 kWorkers = 4u;
    RegistryWorkerContext contexts[kWorkers]{};
    gp::platform::Thread threads[kWorkers]{};

    for (gp::UInt32 i = 0u; i < kWorkers; ++i)
    {
        contexts[i].registry = &sharedRegistry;
        contexts[i].startGate = &startGate;
        contexts[i].workerIndex = i;

        gp::platform::ThreadDesc desc{};
        desc.name = "HashRegistryWorker";
        desc.entry = &registryWorkerEntry;
        desc.userData = &contexts[i];
        desc.stackSize = 64u * 1024u;
        ASSERT_EQ(threads[i].start(desc), gp::platform::ThreadResult::Success);
    }

    startGate.store(true, gp::MemoryOrder::Release);

    for (gp::UInt32 i = 0u; i < kWorkers; ++i)
    {
        EXPECT_EQ(threads[i].join(), gp::platform::ThreadResult::Success);
    }

    EXPECT_EQ(sharedRegistry.size(), 4u);
    EXPECT_EQ(sharedRegistry.collisionCount(), 0u);

    // Global singleton works cleanly.
    const gp::UInt64 h = globalHashRegistry().registerString("GlobalRegistrySmokeTest");
    EXPECT_EQ(globalHashRegistry().lookup(h), "GlobalRegistrySmokeTest");
}

}   // namespace gp::hashing::tests
