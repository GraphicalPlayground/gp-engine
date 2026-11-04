// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "containers/strings/Name.hpp"
#include "threading/atomics/Atomic.hpp"
#include <format>
#include <gtest/gtest.h>
#include <sstream>
#include <thread>
#include <vector>

namespace gp::tests
{

using gp::container::NameFindMode;
using namespace gp::literals;

TEST(NameTests, StaticMemoryLayoutAndRelocatability)
{
    static_assert(sizeof(gp::Name) == 8u);
    static_assert(alignof(gp::Name) == 4u);
    static_assert(sizeof(gp::container::NameEntryHeader) == 16u);
    static_assert(gp::concepts::IsRelocatable<gp::Name>);
}

TEST(NameTests, DefaultAndNoneInvariants)
{
    const gp::Name defaultName;
    const gp::Name emptyName("");
    const gp::Name explicitNone("None");
    const gp::Name lowerNone("none");
    const gp::Name upperNone("NONE");

    EXPECT_TRUE(defaultName.isNone());
    EXPECT_FALSE(defaultName.isValid());
    EXPECT_FALSE(static_cast<bool>(defaultName));
    EXPECT_EQ(defaultName.comparisonId(), gp::container::kNameNoneId);
    EXPECT_EQ(defaultName.rawNumber(), gp::container::kNameNoNumber);
    EXPECT_FALSE(defaultName.hasNumber());
    EXPECT_EQ(defaultName.baseView(), "None");
    EXPECT_STREQ(defaultName.baseCString(), "None");
    EXPECT_EQ(defaultName.toString(), "None");

    EXPECT_EQ(defaultName, emptyName);
    EXPECT_EQ(defaultName, explicitNone);
    EXPECT_EQ(defaultName, lowerNone);
    EXPECT_EQ(defaultName, upperNone);

    // Numbered "None_0" is not empty/None because it carries an explicit instance number
    const gp::Name numberedNone("None_0");
    EXPECT_FALSE(numberedNone.isNone());
    EXPECT_TRUE(numberedNone.isValid());
    EXPECT_EQ(numberedNone.comparisonId(), gp::container::kNameNoneId);
    EXPECT_TRUE(numberedNone.hasNumber());
    EXPECT_EQ(numberedNone.getNumber(), 0u);
    EXPECT_EQ(numberedNone.baseView(), "None");
    EXPECT_EQ(numberedNone.toString(), "None_0");
}

TEST(NameTests, CaseInsensitiveDeduplicationAndDisplayCasingPreservation)
{
    const gp::Name first("Character_Pelvis_Bone");
    const gp::Name lower("character_pelvis_bone");
    const gp::Name upper("CHARACTER_PELVIS_BONE");

    EXPECT_FALSE(first.isNone());
    EXPECT_EQ(first.comparisonId(), lower.comparisonId());
    EXPECT_EQ(first.comparisonId(), upper.comparisonId());
    EXPECT_EQ(first, lower);
    EXPECT_EQ(first, upper);
    EXPECT_EQ(first.hash(), lower.hash());

    // The pool preserves the display casing of the first registration
    EXPECT_EQ(first.baseView(), "Character_Pelvis_Bone");
    EXPECT_EQ(lower.baseView(), "Character_Pelvis_Bone");
    EXPECT_EQ(upper.baseView(), "Character_Pelvis_Bone");
    EXPECT_STREQ(lower.baseCString(), "Character_Pelvis_Bone");
}

TEST(NameTests, FindModeDoesNotInternUnregisteredNames)
{
    const gp::StringView probeToken = "Unregistered_Probe_Token_For_FindMode_Test";

    const gp::Name beforeAdd(probeToken, NameFindMode::Find);
    EXPECT_TRUE(beforeAdd.isNone());

    // Confirm a second Find still returns None (pool was not mutated)
    const gp::Name secondProbe(probeToken, NameFindMode::Find);
    EXPECT_TRUE(secondProbe.isNone());

    // Now intern the token
    const gp::Name added(probeToken, NameFindMode::Add);
    EXPECT_FALSE(added.isNone());
    EXPECT_EQ(added.baseView(), probeToken);

    // Subsequent Find must locate the interned entry
    const gp::Name afterAdd("unregistered_probe_token_for_findmode_test", NameFindMode::Find);
    EXPECT_FALSE(afterAdd.isNone());
    EXPECT_EQ(afterAdd, added);

    // Numbered Find on an interned base also succeeds without mutating the pool
    const gp::Name numberedFind("unregistered_probe_token_for_findmode_test_17", NameFindMode::Find);
    EXPECT_FALSE(numberedFind.isNone());
    EXPECT_EQ(numberedFind.comparisonId(), added.comparisonId());
    EXPECT_TRUE(numberedFind.hasNumber());
    EXPECT_EQ(numberedFind.getNumber(), 17u);
}

TEST(NameTests, SplitNumericInstanceSuffixesAndLeadingZeroRules)
{
    const gp::Name baseEnemy("EnemySpawner");
    const gp::Name enemy0("EnemySpawner_0");
    const gp::Name enemy42("enemyspawner_42");

    EXPECT_FALSE(baseEnemy.hasNumber());
    EXPECT_EQ(baseEnemy.rawNumber(), 0u);

    EXPECT_TRUE(enemy0.hasNumber());
    EXPECT_EQ(enemy0.getNumber(), 0u);
    EXPECT_EQ(enemy0.rawNumber(), 1u);
    EXPECT_EQ(enemy0.baseView(), "EnemySpawner");
    EXPECT_EQ(enemy0.toString(), "EnemySpawner_0");

    EXPECT_TRUE(enemy42.hasNumber());
    EXPECT_EQ(enemy42.getNumber(), 42u);
    EXPECT_EQ(enemy42.rawNumber(), 43u);
    EXPECT_EQ(enemy42.baseView(), "EnemySpawner");
    EXPECT_EQ(enemy42.toString(), "EnemySpawner_42");

    // All three share the exact same base entry ID in the NamePool
    EXPECT_EQ(baseEnemy.comparisonId(), enemy0.comparisonId());
    EXPECT_EQ(baseEnemy.comparisonId(), enemy42.comparisonId());
    EXPECT_NE(baseEnemy, enemy0);
    EXPECT_NE(enemy0, enemy42);

    // Leading zeros ("_007", "_01") must NOT be split so round-trip formatting is lossless
    const gp::Name agent007("Agent_007");
    EXPECT_FALSE(agent007.hasNumber());
    EXPECT_EQ(agent007.baseView(), "Agent_007");
    EXPECT_EQ(agent007.toString(), "Agent_007");

    // Leading underscore with empty base ("_42") or trailing underscore ("Weapon_") must not split
    const gp::Name leadingUnderscore("_42");
    EXPECT_FALSE(leadingUnderscore.hasNumber());
    EXPECT_EQ(leadingUnderscore.baseView(), "_42");

    const gp::Name trailingUnderscore("Weapon_");
    EXPECT_FALSE(trailingUnderscore.hasNumber());
    EXPECT_EQ(trailingUnderscore.baseView(), "Weapon_");
}

TEST(NameTests, ExplicitNumberConstructionAndModifiers)
{
    // Explicit number constructor does not split "_01" from the base string
    gp::Name socket("HandSocket_01", 3u);
    EXPECT_EQ(socket.baseView(), "HandSocket_01");
    EXPECT_TRUE(socket.hasNumber());
    EXPECT_EQ(socket.getNumber(), 3u);
    EXPECT_EQ(socket.toString(), "HandSocket_01_3");

    const gp::Name renumbered = socket.withNumber(99u);
    EXPECT_EQ(renumbered.comparisonId(), socket.comparisonId());
    EXPECT_EQ(renumbered.getNumber(), 99u);
    EXPECT_EQ(renumbered.toString(), "HandSocket_01_99");

    const gp::Name unnumbered = socket.withoutNumber();
    EXPECT_EQ(unnumbered.comparisonId(), socket.comparisonId());
    EXPECT_FALSE(unnumbered.hasNumber());
    EXPECT_EQ(unnumbered.toString(), "HandSocket_01");

    socket.setNumber(0u);
    EXPECT_TRUE(socket.hasNumber());
    EXPECT_EQ(socket.getNumber(), 0u);
    EXPECT_EQ(socket.toString(), "HandSocket_01_0");

    socket.clearNumber();
    EXPECT_FALSE(socket.hasNumber());
    EXPECT_EQ(socket.toString(), "HandSocket_01");
}

TEST(NameTests, ConstexprNameHashAndSwitchDispatch)
{
    static_assert("Spine_01"_nh == "spine_01"_nh);
    static_assert("Spine_01"_nh != "Spine_02"_nh);
    static_assert("Spine_01"_nhs != "spine_01"_nhs);
    static_assert("Spine_01"_nhs == gp::NameHash("Spine_01", SearchCase::CaseSensitive));

    const gp::Name bone("spine_01_5");
    EXPECT_EQ(bone.baseNameHash(), "SPINE_01"_nh);

    gp::Int32 matchedBranch = 0;
    switch (bone.baseNameHash().value())
    {
    case "root"_nh.value():
        matchedBranch = 1;
        break;
    case "spine_01"_nh.value():
        matchedBranch = 2;
        break;
    default:
        matchedBranch = -1;
        break;
    }
    EXPECT_EQ(matchedBranch, 2);
}

TEST(NameTests, LexicographicalComparisonFormattingAndStringEquality)
{
    const gp::Name alpha1("Alpha_1");
    const gp::Name alpha2("Alpha_2");
    const gp::Name beta("Beta");

    EXPECT_LT(alpha1.compareLexicographical(alpha2), 0);
    EXPECT_GT(alpha2.compareLexicographical(alpha1), 0);
    EXPECT_LT(alpha2.compareLexicographical(beta), 0);
    EXPECT_EQ(alpha1.compareLexicographical(gp::Name("alpha_1")), 0);

    EXPECT_TRUE(alpha1.equalsString("alpha_1", SearchCase::IgnoreCase));
    EXPECT_TRUE(alpha1.equalsString("Alpha_1", SearchCase::CaseSensitive));
    EXPECT_FALSE(alpha1.equalsString("alpha_1", SearchCase::CaseSensitive));
    EXPECT_FALSE(alpha1.equalsString("Alpha_2", SearchCase::IgnoreCase));

    const gp::FixedString<64> fixed = alpha2.toFixedString<64>();
    EXPECT_EQ(fixed.view(), "Alpha_2");

    std::ostringstream oss;
    oss << alpha1;
    EXPECT_EQ(oss.str(), "Alpha_1");

    const std::string formatted = std::format("{}", beta);
    EXPECT_EQ(formatted, "Beta");
}

TEST(NameTests, MultiThreadedConcurrentInterningAndLookup)
{
    constexpr gp::UInt32 kThreadCount = 8u;
    constexpr gp::UInt32 kNamesPerThread = 250u;

    gp::thread::Atomic<bool> startFlag(false);
    std::vector<std::thread> workers;
    workers.reserve(kThreadCount);

    for (gp::UInt32 t = 0u; t < kThreadCount; ++t)
    {
        workers.emplace_back([t, &startFlag]()
        {
            while (!startFlag.load(gp::memory::MemoryOrder::Acquire))
            {
                std::this_thread::yield();
            }

            for (gp::UInt32 i = 0u; i < kNamesPerThread; ++i)
            {
                // Shared contended name across all threads
                const gp::String sharedStr = gp::String::format("SharedContendedAsset_{}_{}", i, i % 10u);
                const gp::Name sharedName(sharedStr.view());
                EXPECT_FALSE(sharedName.isNone());
                EXPECT_TRUE(sharedName.hasNumber());
                EXPECT_EQ(sharedName.getNumber(), static_cast<gp::Int32>(i % 10u));

                // Thread-specific unique name
                const gp::String uniqueStr = gp::String::format("ThreadUnique_{}_Entry_{}", t, i);
                const gp::Name uniqueName(uniqueStr.view());
                EXPECT_FALSE(uniqueName.isNone());
                EXPECT_TRUE(uniqueName.hasNumber());
                EXPECT_EQ(uniqueName.getNumber(), static_cast<gp::Int32>(i));
            }
        });
    }

    startFlag.store(true, gp::memory::MemoryOrder::Release);

    for (std::thread& worker: workers)
    {
        worker.join();
    }

    // Verify all shared and unique names resolve deterministically after contention
    for (gp::UInt32 i = 0u; i < kNamesPerThread; ++i)
    {
        const gp::String numberedQuery = gp::String::format("sharedcontendedasset_{}_{}", i, i % 10u);
        const gp::Name found(numberedQuery.view(), NameFindMode::Find);
        EXPECT_FALSE(found.isNone());
        EXPECT_TRUE(found.hasNumber());
        EXPECT_EQ(found.getNumber(), static_cast<gp::Int32>(i % 10u));
    }

    EXPECT_GT(gp::container::NamePool::instance().entryCount(), kNamesPerThread);
    EXPECT_GT(gp::container::NamePool::instance().slabCount(), 0u);
    EXPECT_GT(gp::container::NamePool::instance().allocatedSlabBytes(), 0u);
}

}   // namespace gp::tests
