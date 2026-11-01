// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "platforms/generic/system/GenericPaths.hpp"
#include "miscellaneous/CommandLine.hpp"
#include <gtest/gtest.h>

namespace gp::platform::generic::tests
{

static_assert(Paths::isSeparator('/'), "'/' must be recognized as a separator.");
static_assert(Paths::isSeparator('\\'), "'\\' must be recognized as a separator.");
static_assert(!Paths::isSeparator(':'), "':' must not be recognized as a separator.");

static_assert(
    Paths::getCleanFilename("C:/Engine/Source/Paths.cpp") == "Paths.cpp",
    "getCleanFilename must extract filename and extension."
);
static_assert(
    Paths::getCleanFilename("C:\\Engine\\Source\\") == "", "getCleanFilename on trailing slash must return empty view."
);
static_assert(
    Paths::getPathLeaf("C:\\Engine\\Source\\") == "Source",
    "getPathLeaf must strip trailing slashes before extracting leaf."
);
static_assert(
    Paths::getBaseFilename("C:/Engine/Source/Paths.tests.cpp", true) == "Paths.tests",
    "getBaseFilename must strip only the final extension."
);
static_assert(
    Paths::getBaseFilenameWithPath("C:/Engine/Source/Paths.cpp") == "C:/Engine/Source/Paths",
    "getBaseFilenameWithPath must preserve directory and strip extension."
);
static_assert(
    Paths::getPath("C:/Engine/Source/Paths.cpp") == "C:/Engine/Source", "getPath must extract parent directory."
);
static_assert(Paths::getPath("C:/file.txt") == "C:/", "getPath on root-level drive file must preserve 'C:/'.");
static_assert(Paths::getPath("/file.txt") == "/", "getPath on POSIX root-level file must preserve '/'.");
static_assert(Paths::getPath("C:/") == "", "getPath on bare drive root must return empty view.");
static_assert(Paths::getPath("/") == "", "getPath on bare POSIX root must return empty view.");

static_assert(Paths::getExtension("Hero.uasset") == "uasset", "getExtension default must omit leading dot.");
static_assert(
    Paths::getExtension("Hero.uasset", PathFlags::IncludeDot) == ".uasset",
    "getExtension with IncludeDot must include leading dot."
);
static_assert(
    Paths::getExtension("PathEnums.tests.cpp", PathFlags::AllowCompoundExtension) == "tests.cpp",
    "getExtension with AllowCompoundExtension must extract compound extension."
);
static_assert(
    Paths::getExtension("PathEnums.tests.cpp", PathFlags::IncludeDot | PathFlags::AllowCompoundExtension) ==
        ".tests.cpp",
    "getExtension with IncludeDot | AllowCompoundExtension must extract compound extension with dot."
);
static_assert(
    Paths::getExtension(".gitignore") == "", "Hidden dotfile without second dot must have an empty extension."
);
static_assert(Paths::getBaseFilename(".gitignore") == ".gitignore", "Hidden dotfile stem must be preserved in full.");

static_assert(Paths::isAbsolute("C:/Engine"), "Drive rooted path must be absolute.");
static_assert(Paths::isAbsolute("/usr/local/bin"), "POSIX rooted path must be absolute.");
static_assert(Paths::isAbsolute("\\\\server\\share\\file.txt"), "UNC path must be absolute.");
static_assert(Paths::isAbsolute("engine://Shaders/PBR.hlsl"), "VFS URI path must be absolute.");
static_assert(Paths::isRelative("Content/Meshes/Hero.uasset"), "Unrooted path must be relative.");
static_assert(Paths::isRelative("../Content/Meshes"), "Dot-dot path must be relative.");
static_assert(Paths::isRelative("C:relative_to_drive"), "Unrooted drive specifier must be relative.");
static_assert(Paths::isDriveSpecifierWithoutRoot("C:relative"), "C:relative must be detected as unrooted drive.");
static_assert(!Paths::isDriveSpecifierWithoutRoot("C:/rooted"), "C:/rooted must not be unrooted drive.");

static_assert(
    Paths::createStandardFilename("C:\\Engine\\\\Source\\Runtime\\..\\Shaders\\./PBR.hlsl").view() ==
        "C:/Engine/Source/Shaders/PBR.hlsl",
    "createStandardFilename must normalize slashes and collapse relative segments at compile time."
);

static_assert(
    Paths::equals("C:\\Engine\\Source\\Core\\", "c:/engine/source/runtime/../core"),
    "equals must match normalized collapsed paths case-insensitively by default."
);

static_assert(
    Paths::combine("C:/Engine", "Source\\Runtime", "/Core/", "Paths.hpp").view() ==
        "C:/Engine/Source/Runtime/Core/Paths.hpp",
    "Variadic combine must join and normalize path segments at compile time."
);

class GenericPathsTest : public ::testing::Test
{
protected:
    void TearDown() noexcept override
    {
        Paths::resetCachesForTesting();
        gp::CommandLine::shutdown();
    }
};

TEST_F(GenericPathsTest, VolumeSpecifierAndFirstComponentSplitting)
{
    gp::StringView volume{};
    gp::StringView remainder{};

    Paths::splitVolumeSpecifier("C:/Engine/Binaries", volume, remainder);
    EXPECT_EQ(volume, gp::StringView("C:"));
    EXPECT_EQ(remainder, gp::StringView("/Engine/Binaries"));

    Paths::splitVolumeSpecifier("engine://Shaders/Common.hlsl", volume, remainder);
    EXPECT_EQ(volume, gp::StringView("engine:"));
    EXPECT_EQ(remainder, gp::StringView("//Shaders/Common.hlsl"));

    Paths::splitVolumeSpecifier("//nas01/assets/hero.fbx", volume, remainder);
    EXPECT_EQ(volume, gp::StringView("//nas01"));
    EXPECT_EQ(remainder, gp::StringView("/assets/hero.fbx"));

    Paths::splitVolumeSpecifier("Relative/Path/File.txt", volume, remainder);
    EXPECT_TRUE(volume.isEmpty());
    EXPECT_EQ(remainder, gp::StringView("Relative/Path/File.txt"));

    gp::StringView firstComp{};
    gp::StringView restComp{};
    Paths::splitFirstComponent("/Engine/Source/Core/Paths.hpp", firstComp, restComp);
    EXPECT_EQ(firstComp, gp::StringView("Engine"));
    EXPECT_EQ(restComp, gp::StringView("Source/Core/Paths.hpp"));

    EXPECT_EQ(Paths::getPathPrefix("Engine/Source/Core/Paths.hpp", 2), gp::StringView("Engine/Source"));
    EXPECT_EQ(Paths::getPathPrefix("/Engine/Source/Core", 2), gp::StringView("/Engine/Source"));
    EXPECT_EQ(Paths::getPathPrefix("Engine/Source", 0), gp::StringView(""));
    EXPECT_EQ(Paths::getPathPrefix("Engine/Source", 5), gp::StringView("Engine/Source"));
}

TEST_F(GenericPathsTest, SplitAndExtensionManipulation)
{
    const PathSplitResult splitRes = Paths::split("C:/Project/Content/Characters/Hero.uasset");
    EXPECT_EQ(splitRes.directory, gp::StringView("C:/Project/Content/Characters"));
    EXPECT_EQ(splitRes.filename, gp::StringView("Hero"));
    EXPECT_EQ(splitRes.extension, gp::StringView("uasset"));

    const PathSplitResult dirOnlySplit = Paths::split("C:/Project/Content/");
    EXPECT_EQ(dirOnlySplit.directory, gp::StringView("C:/Project/Content"));
    EXPECT_TRUE(dirOnlySplit.filename.isEmpty());
    EXPECT_TRUE(dirOnlySplit.extension.isEmpty());

    // changeExtension only modifies paths that already have an extension
    EXPECT_EQ(Paths::changeExtension("Meshes/Hero.fbx", "uasset").view(), gp::StringView("Meshes/Hero.uasset"));
    EXPECT_EQ(Paths::changeExtension("Meshes/Hero.fbx", ".gltf").view(), gp::StringView("Meshes/Hero.gltf"));
    EXPECT_EQ(Paths::changeExtension("Meshes/Hero.fbx", "").view(), gp::StringView("Meshes/Hero"));
    EXPECT_EQ(
        Paths::changeExtension("Meshes/HeroWithoutExt", "uasset").view(), gp::StringView("Meshes/HeroWithoutExt")
    );

    // setExtension always sets or appends the extension, ignoring dots in parent directory names
    EXPECT_EQ(
        Paths::setExtension("Meshes/HeroWithoutExt", "uasset").view(), gp::StringView("Meshes/HeroWithoutExt.uasset")
    );
    EXPECT_EQ(Paths::setExtension("dir.v1/Hero", ".pak").view(), gp::StringView("dir.v1/Hero.pak"));
    EXPECT_EQ(Paths::setExtension("dir.v1/Hero.old", "").view(), gp::StringView("dir.v1/Hero"));
}

TEST_F(GenericPathsTest, CollapseRelativeDirectoriesNominalAndEdgeCases)
{
    PathBuffer p1("C:\\Engine\\Binaries\\Win64\\..\\..\\Content\\./Meshes\\Hero.uasset");
    EXPECT_TRUE(Paths::collapseRelativeDirectories(p1));
    EXPECT_EQ(p1.view(), gp::StringView("C:/Engine/Content/Meshes/Hero.uasset"));

    // Preserves trailing slash on directory paths
    PathBuffer p2("Engine/Source/Runtime/../Editor/");
    EXPECT_TRUE(Paths::collapseRelativeDirectories(p2));
    EXPECT_EQ(p2.view(), gp::StringView("Engine/Source/Editor/"));

    // Relative path with leading ".." preserves leading ".." when collapseAllPossible is true
    PathBuffer p3("../../Engine/Source/../Content/Level.umap");
    EXPECT_TRUE(Paths::collapseRelativeDirectories(p3, true));
    EXPECT_EQ(p3.view(), gp::StringView("../../Engine/Content/Level.umap"));

    // Relative path with leading ".." returns false when collapseAllPossible is false
    PathBuffer p4("../CannotCollapseLeading");
    EXPECT_FALSE(Paths::collapseRelativeDirectories(p4, false));

    // Rooted path attempting to climb above root clamps at root and returns false
    PathBuffer p5("C:/Engine/../../../Escaped/File.txt");
    EXPECT_FALSE(Paths::collapseRelativeDirectories(p5, true));
    EXPECT_EQ(p5.view(), gp::StringView("C:/Escaped/File.txt"));

    // VFS URI scheme root preservation and collapsing
    PathBuffer p6("engine://Shaders/Common/../PBR//Lighting.hlsl");
    EXPECT_TRUE(Paths::collapseRelativeDirectories(p6));
    EXPECT_EQ(p6.view(), gp::StringView("engine://Shaders/PBR/Lighting.hlsl"));
}

TEST_F(GenericPathsTest, TryMakeChildPathRelativeToAndHierarchyChecks)
{
    gp::StringView relView{};
    EXPECT_TRUE(
        Paths::tryMakeChildPathRelativeTo("C:/Engine/Source/Core/Paths.hpp", "C:/Engine/Source", relView, false)
    );
    EXPECT_EQ(relView, gp::StringView("Core/Paths.hpp"));

    // Prefix boundary trap: "C:/Engine/SourceExtra" is NOT a child of "C:/Engine/Source"
    EXPECT_FALSE(
        Paths::tryMakeChildPathRelativeTo("C:/Engine/SourceExtra/Paths.hpp", "C:/Engine/Source", relView, false)
    );

    EXPECT_TRUE(Paths::isUnderDirectory("C:\\Engine\\Source\\Runtime\\..\\Core\\Paths.hpp", "c:/engine/source/core"));
    EXPECT_FALSE(Paths::isUnderDirectory("C:/Engine/Content/Audio", "C:/Engine/Source"));
}

TEST_F(GenericPathsTest, MakePathRelativeToAndComputeRelativePath)
{
    const gp::Optional<PathBuffer> siblingRel =
        Paths::computeRelativePath("C:/Project/Content/Meshes/Hero.uasset", "C:/Project/Binaries/Win64/");
    ASSERT_TRUE(siblingRel.hasValue());
    EXPECT_EQ(siblingRel->view(), gp::StringView("../../Content/Meshes/Hero.uasset"));

    const gp::Optional<PathBuffer> childRel =
        Paths::computeRelativePath("C:/Project/Content/Meshes/Hero.uasset", "C:/Project/Content");
    ASSERT_TRUE(childRel.hasValue());
    EXPECT_EQ(childRel->view(), gp::StringView("Meshes/Hero.uasset"));

    const gp::Optional<PathBuffer> sameRel = Paths::computeRelativePath("C:/Project/Content/", "C:/Project/Content");
    ASSERT_TRUE(sameRel.hasValue());
    EXPECT_EQ(sameRel->view(), gp::StringView("."));

    // Different drives cannot be made relative to each other
    const gp::Optional<PathBuffer> crossDrive =
        Paths::computeRelativePath("D:/Assets/Hero.uasset", "C:/Project/Content");
    EXPECT_FALSE(crossDrive.hasValue());
}

TEST_F(GenericPathsTest, ValidatePathAndMakeValidFileName)
{
    PathValidationError err = PathValidationError::None;

    EXPECT_TRUE(Paths::validatePath("C:/Projects/gp-engine/source/Paths.hpp", &err));
    EXPECT_EQ(err, PathValidationError::None);

    EXPECT_TRUE(Paths::validatePath("engine://Shaders/Forward.hlsl", &err));
    EXPECT_EQ(err, PathValidationError::None);

    EXPECT_FALSE(Paths::validatePath("", &err));
    EXPECT_EQ(err, PathValidationError::EmptyPath);

    EXPECT_FALSE(Paths::validatePath("C:unrooted/file.txt", &err));
    EXPECT_EQ(err, PathValidationError::UnrootedDriveSpecifier);

    EXPECT_FALSE(Paths::validatePath("C:/Projects/bad*name?.txt", &err));
    EXPECT_EQ(err, PathValidationError::InvalidCharacter);

    EXPECT_FALSE(Paths::validatePath("C:/Projects/CON/file.txt", &err));
    EXPECT_EQ(err, PathValidationError::ReservedDeviceName);

    EXPECT_FALSE(Paths::validatePath("C:/Projects/com1.txt", &err));
    EXPECT_EQ(err, PathValidationError::ReservedDeviceName);

    EXPECT_FALSE(Paths::validatePath("C:/Projects/TrailingDot./file.txt", &err));
    EXPECT_EQ(err, PathValidationError::TrailingDotOrSpace);

    EXPECT_EQ(Paths::makeValidFileName("Hero:Weapon*01?.uasset", '_').view(), gp::StringView("Hero_Weapon_01_.uasset"));
    EXPECT_EQ(Paths::makeValidFileName("Hero:Weapon*01?.uasset", '\0').view(), gp::StringView("HeroWeapon01.uasset"));
    EXPECT_EQ(Paths::makeValidFileName("CON.txt", '_').view(), gp::StringView("_CON.txt"));
    EXPECT_EQ(Paths::makeValidFileName("TrailingSpace. . ", '_').view(), gp::StringView("TrailingSpace"));
}

TEST_F(GenericPathsTest, ComponentAndAncestorIteration)
{
    gp::StringView components[8]{};
    gp::UInt32 compCount = 0u;

    Paths::iterateComponents(
        "C:/Engine/Source/Core/Paths.hpp",
        [&components, &compCount](const gp::StringView comp) noexcept
    {
        if (compCount < 8u)
        {
            components[compCount++] = comp;
        }
    }
    );

    ASSERT_EQ(compCount, 5u);
    EXPECT_EQ(components[0], gp::StringView("C:"));
    EXPECT_EQ(components[1], gp::StringView("Engine"));
    EXPECT_EQ(components[2], gp::StringView("Source"));
    EXPECT_EQ(components[3], gp::StringView("Core"));
    EXPECT_EQ(components[4], gp::StringView("Paths.hpp"));

    gp::StringView ancestors[8]{};
    gp::UInt32 ancCount = 0u;
    const bool completed = Paths::iterateAncestors(
        "C:/Engine/Source/Core",
        [&ancestors, &ancCount](const gp::StringView anc) noexcept -> bool
    {
        if (ancCount < 8u)
        {
            ancestors[ancCount++] = anc;
        }
        return true;
    }
    );

    EXPECT_TRUE(completed);
    ASSERT_EQ(ancCount, 4u);
    EXPECT_EQ(ancestors[0], gp::StringView("C:/Engine/Source/Core"));
    EXPECT_EQ(ancestors[1], gp::StringView("C:/Engine/Source"));
    EXPECT_EQ(ancestors[2], gp::StringView("C:/Engine"));
    EXPECT_EQ(ancestors[3], gp::StringView("C:/"));
}

TEST_F(GenericPathsTest, TestingAndCommandLineOverrides)
{
    Paths::setExecutablePathForTesting("C:\\CustomBin\\gp-editor.exe");
    EXPECT_EQ(Paths::executablePath(), gp::StringView("C:/CustomBin/gp-editor.exe"));
    EXPECT_EQ(Paths::executableName(true), gp::StringView("gp-editor"));
    EXPECT_EQ(Paths::executableName(false), gp::StringView("gp-editor.exe"));
    EXPECT_EQ(Paths::executableDir(), gp::StringView("C:/CustomBin/"));
    EXPECT_EQ(Paths::baseDir(), gp::StringView("C:/CustomBin/"));

    const char* const argv[] = {
        "gp-editor.exe", "-basedir=D:\\OverrideBase", "-appdata=D:\\OverrideAppData", "-userdir=D:\\OverrideDocs"
    };
    gp::CommandLine::initialize(4, argv);

    EXPECT_EQ(Paths::baseDir(), gp::StringView("D:/OverrideBase/"));
    EXPECT_EQ(Paths::appDataDir(), gp::StringView("D:/OverrideAppData/"));
    EXPECT_EQ(Paths::documentsDir(), gp::StringView("D:/OverrideDocs/"));
    EXPECT_EQ(Paths::resolve("Shaders/PBR.hlsl").view(), gp::StringView("D:/OverrideBase/Shaders/PBR.hlsl"));
}

}   // namespace gp::platform::generic::tests
