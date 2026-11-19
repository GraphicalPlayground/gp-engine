// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "vfs/VirtualFileSystem.hpp"
#include "platforms/base/system/FileSystem.hpp"
#include "platforms/base/system/Paths.hpp"
#include "threading/atomics/Atomic.hpp"
#include "vfs/MemoryMountBackend.hpp"
#include <gtest/gtest.h>

namespace gp::vfs::tests
{

class VirtualFileSystemTest : public ::testing::Test
{
protected:
    gp::PathBuffer m_sandboxRoot{};
    gp::PathBuffer m_baseGameDir{};
    gp::PathBuffer m_patchDir{};
    gp::PathBuffer m_modDir{};
    gp::PathBuffer m_userSaveDir{};

protected:
    void SetUp() noexcept override
    {
        VirtualFileSystem::shutdown();

        static gp::thread::Atomic<gp::UInt32> s_vfsSandboxId(1u);
        const gp::UInt32 id = s_vfsSandboxId.fetchAdd(1u, gp::memory::MemoryOrder::Relaxed);

        gp::FixedString<64> folderName("gp_vfs_test_sandbox_");
        char digits[16]{};
        gp::UInt32 tempId = id;
        gp::USize digitLen = 0u;
        do
        {
            digits[digitLen++] = static_cast<char>('0' + (tempId % 10u));
            tempId /= 10u;
        }
        while (tempId > 0u);
        for (gp::USize i = digitLen; i > 0u; --i)
        {
            folderName.pushBack(digits[i - 1u]);
        }

        m_sandboxRoot = gp::platform::Paths::combine(gp::platform::Paths::tempDir(), folderName.view());
        if (gp::platform::FileSystem::exists(m_sandboxRoot.view()))
        {
            (void)gp::platform::FileSystem::removeDirectoryTree(m_sandboxRoot.view());
        }

        m_baseGameDir = gp::platform::Paths::combine(m_sandboxRoot.view(), "BaseGame");
        m_patchDir = gp::platform::Paths::combine(m_sandboxRoot.view(), "Patch_1_01");
        m_modDir = gp::platform::Paths::combine(m_sandboxRoot.view(), "Mod_HD");
        m_userSaveDir = gp::platform::Paths::combine(m_sandboxRoot.view(), "UserSaves");

        ASSERT_TRUE(gp::platform::FileSystem::createDirectoryTree(m_baseGameDir.view()).hasValue());
        ASSERT_TRUE(gp::platform::FileSystem::createDirectoryTree(m_patchDir.view()).hasValue());
        ASSERT_TRUE(gp::platform::FileSystem::createDirectoryTree(m_modDir.view()).hasValue());
        ASSERT_TRUE(gp::platform::FileSystem::createDirectoryTree(m_userSaveDir.view()).hasValue());
    }

    void TearDown() noexcept override
    {
        VirtualFileSystem::shutdown();
        if (!m_sandboxRoot.isEmpty() && gp::platform::FileSystem::exists(m_sandboxRoot.view()))
        {
            (void)gp::platform::FileSystem::removeDirectoryTree(m_sandboxRoot.view());
        }
    }
};

TEST_F(VirtualFileSystemTest, UriParsingAndSandboxJailRejectDirectoryTraversal)
{
    // Valid nested relative collapse inside the sandbox
    const gp::Expected<ParsedUri, VfsError> validUri =
        VirtualFileSystem::parseUri("GAME://textures/characters/../weapons/sword.ktx2");
    ASSERT_TRUE(validUri.hasValue());
    EXPECT_EQ(validUri.value().scheme.view(), gp::StringView("game://"));
    EXPECT_EQ(validUri.value().relativePath.view(), gp::StringView("textures/weapons/sword.ktx2"));

    // Root scheme URI
    const gp::Expected<ParsedUri, VfsError> rootUri = VirtualFileSystem::parseUri("engine://");
    ASSERT_TRUE(rootUri.hasValue());
    EXPECT_EQ(rootUri.value().scheme.view(), gp::StringView("engine://"));
    EXPECT_TRUE(rootUri.value().isSchemeRoot());

    // Direct escape attempts via leading ".." or net-negative ".." must fail with SandboxEscapeViolation (ADR-0044)
    EXPECT_EQ(VirtualFileSystem::parseUri("game://../secret.ini").error(), VfsError::SandboxEscapeViolation);
    EXPECT_EQ(
        VirtualFileSystem::parseUri("game://textures/../../outside.txt").error(), VfsError::SandboxEscapeViolation
    );
    EXPECT_EQ(VirtualFileSystem::parseUri("user://C:/Windows/System32").error(), VfsError::SandboxEscapeViolation);

    // Malformed URIs without scheme
    EXPECT_EQ(VirtualFileSystem::parseUri("textures/hero.ktx2").error(), VfsError::InvalidUri);
    EXPECT_EQ(VirtualFileSystem::parseUri("://missing_scheme").error(), VfsError::InvalidUri);
    EXPECT_EQ(VirtualFileSystem::parseUri("").error(), VfsError::InvalidUri);
}

TEST_F(VirtualFileSystemTest, PriorityLayeredOverlayReadsAndReadOnlyVsReadWriteEnforcement)
{
    // Populate Base, Patch, and Mod directories with overlapping files
    ASSERT_TRUE(
        gp::platform::FileSystem::writeAllText(
            gp::platform::Paths::combine(m_baseGameDir.view(), "balance.cfg").view(), "version=base"
        )
            .hasValue()
    );
    ASSERT_TRUE(
        gp::platform::FileSystem::writeAllText(
            gp::platform::Paths::combine(m_baseGameDir.view(), "base_only.txt").view(), "from_base"
        )
            .hasValue()
    );
    ASSERT_TRUE(
        gp::platform::FileSystem::writeAllText(
            gp::platform::Paths::combine(m_patchDir.view(), "balance.cfg").view(), "version=patch_1_01"
        )
            .hasValue()
    );
    ASSERT_TRUE(
        gp::platform::FileSystem::writeAllText(
            gp::platform::Paths::combine(m_modDir.view(), "balance.cfg").view(), "version=mod_hd"
        )
            .hasValue()
    );

    // Mount Base and Mod out-of-order to verify priority sorting (Mod > Patch > Base)
    ASSERT_TRUE(
        VirtualFileSystem::mount("game://", m_baseGameDir.view(), MountPriority::Base, MountAccess::ReadOnly).hasValue()
    );
    ASSERT_TRUE(
        VirtualFileSystem::mount("game://", m_modDir.view(), MountPriority::ModOverride, MountAccess::ReadOnly)
            .hasValue()
    );
    ASSERT_TRUE(
        VirtualFileSystem::mount("game://", m_patchDir.view(), MountPriority::Patch, MountAccess::ReadOnly).hasValue()
    );
    ASSERT_TRUE(
        VirtualFileSystem::mount("user://", m_userSaveDir.view(), MountPriority::Base, MountAccess::ReadWrite)
            .hasValue()
    );

    EXPECT_TRUE(VirtualFileSystem::isSchemeMounted("game://"));
    EXPECT_EQ(VirtualFileSystem::getMountCount("game://"), 3u);

    // Reading "game://balance.cfg" must return the highest-priority ModOverride version
    gp::String content{};
    ASSERT_TRUE(VirtualFileSystem::readAllText("game://balance.cfg", content).hasValue());
    EXPECT_EQ(content.view(), gp::StringView("version=mod_hd"));

    // Unmounting ModOverride falls back transparently to Patch
    EXPECT_TRUE(VirtualFileSystem::unmount("game://", m_modDir.view()));
    ASSERT_TRUE(VirtualFileSystem::readAllText("game://balance.cfg", content).hasValue());
    EXPECT_EQ(content.view(), gp::StringView("version=patch_1_01"));

    // Base-only file is still accessible through the fallback layer
    ASSERT_TRUE(VirtualFileSystem::readAllText("game://base_only.txt", content).hasValue());
    EXPECT_EQ(content.view(), gp::StringView("from_base"));

    // Attempting to write, mkdir, or delete on ReadOnly "game://" must fail with ReadOnlyMount (ADR-0044)
    EXPECT_EQ(VirtualFileSystem::writeAllText("game://hack.txt", "illegal").error(), VfsError::ReadOnlyMount);
    EXPECT_EQ(VirtualFileSystem::writeAtomicText("game://hack.txt", "illegal").error(), VfsError::ReadOnlyMount);
    EXPECT_EQ(VirtualFileSystem::createDirectory("game://new_dir").error(), VfsError::ReadOnlyMount);
    EXPECT_EQ(VirtualFileSystem::removeFile("game://base_only.txt").error(), VfsError::ReadOnlyMount);

    // Writing and atomic writing to ReadWrite "user://" must succeed
    ASSERT_TRUE(VirtualFileSystem::createDirectoryTree("user://profiles/slot1").hasValue());
    ASSERT_TRUE(VirtualFileSystem::writeAtomicText("user://profiles/slot1/save.dat", "SaveData_V1").hasValue());
    ASSERT_TRUE(VirtualFileSystem::readAllText("user://profiles/slot1/save.dat", content).hasValue());
    EXPECT_EQ(content.view(), gp::StringView("SaveData_V1"));

    // Cross-scheme copy from ReadOnly "game://" to ReadWrite "user://"
    ASSERT_TRUE(VirtualFileSystem::copyFile("game://base_only.txt", "user://copied_from_game.txt", true).hasValue());
    ASSERT_TRUE(VirtualFileSystem::readAllText("user://copied_from_game.txt", content).hasValue());
    EXPECT_EQ(content.view(), gp::StringView("from_base"));
}

TEST_F(VirtualFileSystemTest, MergedDirectoryEnumerationDeduplicatesShadowedFilesAcrossLayers)
{
    ASSERT_TRUE(
        gp::platform::FileSystem::writeAllText(
            gp::platform::Paths::combine(m_baseGameDir.view(), "shared.txt").view(), "base_shared"
        )
            .hasValue()
    );
    ASSERT_TRUE(
        gp::platform::FileSystem::writeAllText(
            gp::platform::Paths::combine(m_baseGameDir.view(), "only_base.txt").view(), "base_unique"
        )
            .hasValue()
    );
    ASSERT_TRUE(
        gp::platform::FileSystem::writeAllText(
            gp::platform::Paths::combine(m_modDir.view(), "shared.txt").view(), "mod_shared_override"
        )
            .hasValue()
    );
    ASSERT_TRUE(
        gp::platform::FileSystem::writeAllText(
            gp::platform::Paths::combine(m_modDir.view(), "only_mod.txt").view(), "mod_unique"
        )
            .hasValue()
    );

    ASSERT_TRUE(
        VirtualFileSystem::mount("game://", m_baseGameDir.view(), MountPriority::Base, MountAccess::ReadOnly).hasValue()
    );
    ASSERT_TRUE(
        VirtualFileSystem::mount("game://", m_modDir.view(), MountPriority::ModOverride, MountAccess::ReadOnly)
            .hasValue()
    );

    struct MergedWalkResult
    {
        gp::UInt32 totalVisited{ 0u };
        gp::UInt32 sharedVisitedCount{ 0u };
        MountPriority sharedWinningPriority{ MountPriority::Base };
        gp::UInt64 sharedWinningSize{ 0ULL };
    };

    MergedWalkResult walk{};
    const VfsDirectoryVisitorFn visitor = [](const VfsDirectoryEntry& entry,
                                             void* const rawCtx) -> gp::platform::DirectoryVisitControl
    {
        MergedWalkResult* const res = static_cast<MergedWalkResult*>(rawCtx);
        ++res->totalVisited;
        if (entry.name == "shared.txt")
        {
            ++res->sharedVisitedCount;
            res->sharedWinningPriority = entry.sourcePriority;
            res->sharedWinningSize = entry.stats.size;
        }
        return gp::platform::DirectoryVisitControl::Continue;
    };

    ASSERT_TRUE(VirtualFileSystem::iterateDirectory("game://", visitor, &walk).hasValue());
    EXPECT_EQ(walk.totalVisited, 3u);           // shared.txt + only_base.txt + only_mod.txt
    EXPECT_EQ(walk.sharedVisitedCount, 1u);     // shared.txt must be emitted once, not twice!
    EXPECT_EQ(walk.sharedWinningPriority, MountPriority::ModOverride);
    EXPECT_EQ(walk.sharedWinningSize, 19ULL);   // strlen("mod_shared_override")
}

TEST_F(VirtualFileSystemTest, InMemoryMountBackendSupportsFullZeroDiskVfsOperations)
{
    MemoryMountBackend memoryDisk("[memory:unit_test]");
    ASSERT_TRUE(
        VirtualFileSystem::mountBackend("cache://", &memoryDisk, MountPriority::Base, MountAccess::ReadWrite).hasValue()
    );

    ASSERT_TRUE(VirtualFileSystem::createDirectoryTree("cache://shaders/dxil/sm6_6").hasValue());
    EXPECT_TRUE(VirtualFileSystem::isDirectory("cache://shaders/dxil/sm6_6"));

    ASSERT_TRUE(VirtualFileSystem::writeAllText("cache://shaders/dxil/sm6_6/pbr.bin", "DXIL_BYTECODE_V1").hasValue());
    EXPECT_TRUE(VirtualFileSystem::exists("cache://shaders/dxil/sm6_6/pbr.bin"));
    EXPECT_TRUE(VirtualFileSystem::isFile("cache://shaders/dxil/sm6_6/pbr.bin"));
    EXPECT_EQ(VirtualFileSystem::fileSize("cache://shaders/dxil/sm6_6/pbr.bin").value(), 16ULL);

    // Move & Copy inside MemoryMountBackend
    ASSERT_TRUE(
        VirtualFileSystem::copyFile(
            "cache://shaders/dxil/sm6_6/pbr.bin", "cache://shaders/dxil/sm6_6/pbr_copy.bin", false
        )
            .hasValue()
    );
    ASSERT_TRUE(
        VirtualFileSystem::move(
            "cache://shaders/dxil/sm6_6/pbr_copy.bin", "cache://shaders/dxil/sm6_6/pbr_moved.bin", false
        )
            .hasValue()
    );
    EXPECT_FALSE(VirtualFileSystem::exists("cache://shaders/dxil/sm6_6/pbr_copy.bin"));
    EXPECT_TRUE(VirtualFileSystem::exists("cache://shaders/dxil/sm6_6/pbr_moved.bin"));

    gp::String readBack{};
    ASSERT_TRUE(VirtualFileSystem::readAllText("cache://shaders/dxil/sm6_6/pbr_moved.bin", readBack).hasValue());
    EXPECT_EQ(readBack.view(), gp::StringView("DXIL_BYTECODE_V1"));

    gp::String readBackBytes{};
    ASSERT_TRUE(VirtualFileSystem::readAllBytes("cache://shaders/dxil/sm6_6/pbr_moved.bin", readBackBytes).hasValue());
    EXPECT_EQ(readBackBytes.view(), gp::StringView("DXIL_BYTECODE_V1"));

    // Recursive removal inside MemoryMountBackend
    ASSERT_TRUE(VirtualFileSystem::removeDirectoryTree("cache://shaders").hasValue());
    EXPECT_FALSE(VirtualFileSystem::exists("cache://shaders/dxil/sm6_6/pbr.bin"));
    EXPECT_FALSE(VirtualFileSystem::exists("cache://shaders"));

    EXPECT_TRUE(VirtualFileSystem::unmountBackend("cache://", &memoryDisk));
}

TEST_F(VirtualFileSystemTest, InitializeDefaultMountsRegistersAllSixCanonicalSchemes)
{
    ASSERT_TRUE(VirtualFileSystem::initializeDefaultMounts("GraphicalPlaygroundTest").hasValue());

    EXPECT_TRUE(VirtualFileSystem::isSchemeMounted(kEngineScheme));
    EXPECT_TRUE(VirtualFileSystem::isSchemeMounted(kGameScheme));
    EXPECT_TRUE(VirtualFileSystem::isSchemeMounted(kUserScheme));
    EXPECT_TRUE(VirtualFileSystem::isSchemeMounted(kAppDataScheme));
    EXPECT_TRUE(VirtualFileSystem::isSchemeMounted(kCacheScheme));
    EXPECT_TRUE(VirtualFileSystem::isSchemeMounted(kTempScheme));

    // engine:// and game:// are ReadOnly by default; user://, appdata://, cache://, temp:// are ReadWrite
    EXPECT_EQ(VirtualFileSystem::resolveWritablePhysicalPath("engine://test.txt").error(), VfsError::ReadOnlyMount);
    EXPECT_EQ(VirtualFileSystem::resolveWritablePhysicalPath("game://test.txt").error(), VfsError::ReadOnlyMount);
    EXPECT_TRUE(VirtualFileSystem::resolveWritablePhysicalPath("user://saves/01.sav").hasValue());
    EXPECT_TRUE(VirtualFileSystem::resolveWritablePhysicalPath("appdata://settings.json").hasValue());
    EXPECT_TRUE(VirtualFileSystem::resolveWritablePhysicalPath("cache://pso.bin").hasValue());
    EXPECT_TRUE(VirtualFileSystem::resolveWritablePhysicalPath("temp://crash.dmp").hasValue());
}

TEST_F(VirtualFileSystemTest, RecursiveMergedWalkShadowingFileOverDirAndCrossMountEdgeCases)
{
    // Base has directory "levels/chapter1" with two files + "shadowed_dir/a.txt"
    const gp::PathBuffer baseLvl = gp::platform::Paths::combine(m_baseGameDir.view(), "levels/chapter1");
    const gp::PathBuffer baseShadowDir = gp::platform::Paths::combine(m_baseGameDir.view(), "shadowed_dir");
    const gp::PathBuffer modLvl = gp::platform::Paths::combine(m_modDir.view(), "levels/chapter1");
    ASSERT_TRUE(gp::platform::FileSystem::createDirectoryTree(baseLvl.view()).hasValue());
    ASSERT_TRUE(gp::platform::FileSystem::createDirectoryTree(baseShadowDir.view()).hasValue());
    ASSERT_TRUE(gp::platform::FileSystem::createDirectoryTree(modLvl.view()).hasValue());

    ASSERT_TRUE(
        gp::platform::FileSystem::writeAllText(gp::platform::Paths::combine(baseLvl.view(), "map.bin").view(), "base")
            .hasValue()
    );
    ASSERT_TRUE(
        gp::platform::FileSystem::writeAllText(
            gp::platform::Paths::combine(baseLvl.view(), "ambient.ogg").view(), "sound"
        )
            .hasValue()
    );
    ASSERT_TRUE(
        gp::platform::FileSystem::writeAllText(gp::platform::Paths::combine(baseShadowDir.view(), "a.txt").view(), "a")
            .hasValue()
    );
    // Mod overrides "levels/chapter1/map.bin" AND shadows "shadowed_dir" with a regular file!
    ASSERT_TRUE(
        gp::platform::FileSystem::writeAllText(
            gp::platform::Paths::combine(modLvl.view(), "map.bin").view(), "mod_override"
        )
            .hasValue()
    );
    ASSERT_TRUE(
        gp::platform::FileSystem::writeAllText(
            gp::platform::Paths::combine(m_modDir.view(), "shadowed_dir").view(), "not_a_dir"
        )
            .hasValue()
    );

    ASSERT_TRUE(
        VirtualFileSystem::mount("game://", m_baseGameDir.view(), MountPriority::Base, MountAccess::ReadOnly).hasValue()
    );
    ASSERT_TRUE(
        VirtualFileSystem::mount("game://", m_modDir.view(), MountPriority::ModOverride, MountAccess::ReadOnly)
            .hasValue()
    );
    ASSERT_TRUE(
        VirtualFileSystem::mount("user://", m_userSaveDir.view(), MountPriority::Base, MountAccess::ReadWrite)
            .hasValue()
    );

    // Iterating a URI shadowed by a higher-priority regular file must fail with NotDirectory (no base dir leak!)
    EXPECT_EQ(
        VirtualFileSystem::iterateDirectory(
            "game://shadowed_dir",
            [](const VfsDirectoryEntry&) -> gp::platform::DirectoryVisitControl
    {
        return gp::platform::DirectoryVisitControl::Continue;
    }
        ).error(),
        VfsError::NotDirectory
    );

    // Recursive merged walk over "game://levels" using lambda overload
    gp::UInt32 dirVisits = 0u;
    gp::UInt32 fileVisits = 0u;
    gp::UInt32 maxDepthSeen = 0u;
    MountPriority mapPriority = MountPriority::Base;

    ASSERT_TRUE(
        VirtualFileSystem::iterateDirectoryRecursive(
            "game://levels",
            [&dirVisits, &fileVisits, &maxDepthSeen, &mapPriority](const VfsDirectoryEntry& entry)
                -> gp::platform::DirectoryVisitControl
    {
        if (entry.depth > maxDepthSeen)
        {
            maxDepthSeen = entry.depth;
        }
        if (entry.isDirectory())
        {
            ++dirVisits;
        }
        else if (entry.isFile())
        {
            ++fileVisits;
            if (entry.name == "map.bin")
            {
                mapPriority = entry.sourcePriority;
            }
        }
        return gp::platform::DirectoryVisitControl::Continue;
    }
        ).hasValue()
    );
    EXPECT_EQ(dirVisits, 1u);    // chapter1 (deduplicated across Base + Mod!)
    EXPECT_EQ(fileVisits, 2u);   // map.bin (deduplicated) + ambient.ogg
    EXPECT_EQ(maxDepthSeen, 1u);
    EXPECT_EQ(mapPriority, MountPriority::ModOverride);

    // Unmounted scheme errors
    EXPECT_EQ(VirtualFileSystem::getStats("unknown://file.txt").error(), VfsError::SchemeNotMounted);
    EXPECT_EQ(VirtualFileSystem::openRead("unknown://file.txt").error(), VfsError::SchemeNotMounted);
    EXPECT_EQ(VirtualFileSystem::writeAllText("unknown://file.txt", "x").error(), VfsError::SchemeNotMounted);

    // Cross-mount move between two different ReadWrite schemes must return CrossMountOperation
    MemoryMountBackend tempMem("[memory:cross_move]");
    ASSERT_TRUE(
        VirtualFileSystem::mountBackend("temp://", &tempMem, MountPriority::Base, MountAccess::ReadWrite).hasValue()
    );
    ASSERT_TRUE(VirtualFileSystem::writeAllText("user://src.txt", "hello").hasValue());
    EXPECT_EQ(VirtualFileSystem::move("user://src.txt", "temp://dst.txt", true).error(), VfsError::CrossMountOperation);

    // Multi-chunk cross-mount copy (> 16 KiB) between PhysicalMountBackend and MemoryMountBackend
    gp::String largePayload{};
    largePayload.reserve(40000u);
    for (gp::USize i = 0u; i < 40000u; ++i)
    {
        largePayload.pushBack(static_cast<char>('A' + (i % 26u)));
    }
    ASSERT_TRUE(VirtualFileSystem::writeAllText("user://large.bin", largePayload.view()).hasValue());
    ASSERT_TRUE(VirtualFileSystem::copyFile("user://large.bin", "temp://large_mem.bin", false).hasValue());
    ASSERT_TRUE(VirtualFileSystem::copyFile("temp://large_mem.bin", "user://large_roundtrip.bin", false).hasValue());

    gp::String roundTrip{};
    ASSERT_TRUE(VirtualFileSystem::readAllText("user://large_roundtrip.bin", roundTrip).hasValue());
    EXPECT_EQ(roundTrip.view(), largePayload.view());

    // MemoryMountBackend directory move reparents descendants cleanly and rejects moving into itself
    ASSERT_TRUE(VirtualFileSystem::createDirectoryTree("temp://dirA/nested").hasValue());
    ASSERT_TRUE(VirtualFileSystem::writeAllText("temp://dirA/nested/item.txt", "nested_payload").hasValue());
    EXPECT_EQ(
        VirtualFileSystem::move("temp://dirA", "temp://dirA/nested/sub", true).error(), VfsError::InvalidArgument
    );
    ASSERT_TRUE(VirtualFileSystem::move("temp://dirA", "temp://dirB", false).hasValue());
    EXPECT_FALSE(VirtualFileSystem::exists("temp://dirA/nested/item.txt"));
    EXPECT_TRUE(VirtualFileSystem::exists("temp://dirB/nested/item.txt"));
    gp::String movedNested{};
    ASSERT_TRUE(VirtualFileSystem::readAllText("temp://dirB/nested/item.txt", movedNested).hasValue());
    EXPECT_EQ(movedNested.view(), gp::StringView("nested_payload"));

    // VfsError::toString constexpr check
    static_assert(toString(VfsError::SandboxEscapeViolation) == "SandboxEscapeViolation");
    static_assert(toString(VfsError::ReadOnlyMount) == "ReadOnlyMount");
    EXPECT_EQ(toString(VfsError::CrossMountOperation), gp::StringView("CrossMountOperation"));

    // Verify unmountScheme clears all mounts for a scheme
    EXPECT_EQ(VirtualFileSystem::unmountScheme("game://"), 2u);
    EXPECT_FALSE(VirtualFileSystem::isSchemeMounted("game://"));
    EXPECT_TRUE(VirtualFileSystem::unmountBackend("temp://", &tempMem));
}

}   // namespace gp::vfs::tests
