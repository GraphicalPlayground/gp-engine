// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Vector.hpp"      // IWYU pragma: keep
#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "platforms/base/system/PathTypes.hpp"
#include "platforms/generic/system/GenericPaths.hpp"

namespace gp::platform::apple
{

/// @brief Pure lexical naming and bundle-path helpers for Apple dynamic libraries (`.dylib`) and `.framework` bundles.
/// @details Resolves `.dylib` extensions, `.framework/<Name>` inner binaries, and macOS `.app` bundle
///          `Contents/Frameworks/` and `Contents/PlugIns/` directories without touching the filesystem.
struct SharedLibraryNaming final
{
public:
    SharedLibraryNaming() = delete;
    ~SharedLibraryNaming() = delete;
    SharedLibraryNaming(const SharedLibraryNaming&) = delete;
    SharedLibraryNaming& operator=(const SharedLibraryNaming&) = delete;
    SharedLibraryNaming(SharedLibraryNaming&&) = delete;
    SharedLibraryNaming& operator=(SharedLibraryNaming&&) = delete;

public:
    /// @brief Checks whether a bare filename ends with `".dylib"` (case-insensitively).
    /// @param[in] fileName Bare filename without directory components.
    /// @return True if `fileName` has a non-empty stem followed by `".dylib"`.
    [[nodiscard]] static constexpr bool isDynamicLibraryFilename(const gp::StringView fileName) noexcept
    {
        constexpr gp::StringView kDylibExt = ".dylib";
        if (fileName.size() <= kDylibExt.size())
        {
            return false;
        }

        const gp::StringView tail = fileName.substr(fileName.size() - kDylibExt.size());
        return gp::platform::generic::Paths::compareCaseInsensitive(tail, kDylibExt) == 0;
    }

    /// @brief Checks whether `path` refers to an Apple `.framework` bundle directory (ignoring any trailing `'/'`).
    /// @param[in] path Relative or absolute path or bare framework name.
    /// @return True if the final path component ends with `".framework"` and has a non-empty stem.
    [[nodiscard]] static constexpr bool isFrameworkPath(gp::StringView path) noexcept
    {
        while (!path.isEmpty() && gp::platform::generic::Paths::isSeparator(path[path.size() - 1u]))
        {
            path.removeSuffix(1u);
        }

        const gp::StringView leaf = gp::platform::generic::Paths::getCleanFilename(path);
        constexpr gp::StringView kFrameworkExt = ".framework";
        if (leaf.size() <= kFrameworkExt.size())
        {
            return false;
        }

        const gp::StringView tail = leaf.substr(leaf.size() - kFrameworkExt.size());
        return gp::platform::generic::Paths::compareCaseInsensitive(tail, kFrameworkExt) == 0;
    }

    /// @brief Maps a `.framework` bundle path to its inner Mach-O binary (`Foo.framework` -> `Foo.framework/Foo`).
    /// @param[in] frameworkPath Path ending in `<Name>.framework` (optional trailing slash).
    /// @return Resolved inner binary path, or an empty `PathBuffer` if `frameworkPath` is not a framework path.
    [[nodiscard]] static constexpr PathBuffer resolveFrameworkBinaryPath(gp::StringView frameworkPath) noexcept
    {
        if (!isFrameworkPath(frameworkPath))
        {
            return {};
        }

        while (!frameworkPath.isEmpty() &&
               gp::platform::generic::Paths::isSeparator(frameworkPath[frameworkPath.size() - 1u]))
        {
            frameworkPath.removeSuffix(1u);
        }

        const gp::StringView leaf = gp::platform::generic::Paths::getCleanFilename(frameworkPath);
        constexpr gp::USize kFrameworkExtLen = 10u;   // ".framework"
        const gp::StringView stem = leaf.substr(0u, leaf.size() - kFrameworkExtLen);

        if ((frameworkPath.size() + 1u + stem.size()) > kMaxPathLength)
        {
            return {};
        }

        PathBuffer resolved(frameworkPath);
        resolved.pushBack('/');
        resolved.append(stem);
        return resolved;
    }

    /// @brief Resolves a sibling directory inside a macOS `.app/Contents/` bundle from the executable directory.
    /// @details When `executableDirectory` ends with `".app/Contents/MacOS"` (with or without trailing slash),
    ///          replaces `"MacOS"` with `subdirName` and appends a trailing `'/'`. Returns an empty `PathBuffer` when
    ///          the executable is not running inside a `.app` bundle.
    /// @param[in] executableDirectory Directory containing the running binary.
    /// @param[in] subdirName Target `Contents/` subdirectory name (for example `"Frameworks"` or `"PlugIns"`).
    /// @return Normalized bundle subdirectory path ending with `'/'`, or empty if not inside a `.app` bundle.
    [[nodiscard]] static constexpr PathBuffer
        resolveBundleSubdir(const gp::StringView executableDirectory, const gp::StringView subdirName) noexcept
    {
        if (executableDirectory.isEmpty() || subdirName.isEmpty())
        {
            return {};
        }

        const PathBuffer stdDir = gp::platform::generic::Paths::createStandardFilename(executableDirectory);
        const gp::StringView view = stdDir.view();
        constexpr gp::StringView kMacOsSuffix = ".app/Contents/MacOS";

        if (view.size() <= kMacOsSuffix.size())
        {
            return {};
        }

        const gp::StringView tail = view.substr(view.size() - kMacOsSuffix.size());
        if (gp::platform::generic::Paths::compareCaseInsensitive(tail, kMacOsSuffix) != 0)
        {
            return {};
        }

        constexpr gp::USize kMacOsTokenLen = 5u;   // "MacOS"
        const gp::StringView contentsPrefix = view.substr(0u, view.size() - kMacOsTokenLen);
        if ((contentsPrefix.size() + subdirName.size() + 1u) > kMaxPathLength)
        {
            return {};
        }

        PathBuffer result(contentsPrefix);
        result.append(subdirName);
        result.pushBack('/');
        return result;
    }

    /// @brief Resolves `<App>.app/Contents/Frameworks/` when running inside a macOS `.app` bundle.
    /// @param[in] executableDirectory Directory containing the running binary.
    /// @return Frameworks bundle directory ending with `'/'`, or empty if not inside a `.app` bundle.
    [[nodiscard]] static constexpr PathBuffer
        resolveBundleFrameworksDir(const gp::StringView executableDirectory) noexcept
    {
        return resolveBundleSubdir(executableDirectory, "Frameworks");
    }

    /// @brief Resolves `<App>.app/Contents/PlugIns/` when running inside a macOS `.app` bundle.
    /// @param[in] executableDirectory Directory containing the running binary.
    /// @return PlugIns bundle directory ending with `'/'`, or empty if not inside a `.app` bundle.
    [[nodiscard]] static constexpr PathBuffer resolveBundlePlugInsDir(const gp::StringView executableDirectory) noexcept
    {
        return resolveBundleSubdir(executableDirectory, "PlugIns");
    }

    /// @brief Appends `".dylib"` to `path` if it does not already carry a `.dylib` or `.framework` suffix.
    /// @param[in] path Input path or bare library name.
    /// @param[out] outWithExt Receives `path + ".dylib"` when an implicit suffix applies.
    /// @return True if `outWithExt` was populated; false otherwise.
    [[nodiscard]] static constexpr bool
        tryBuildWithDefaultExtension(const gp::StringView path, PathBuffer& outWithExt) noexcept
    {
        outWithExt.clear();
        if (path.isEmpty() || path.endsWith('.') || isFrameworkPath(path))
        {
            return false;
        }

        const gp::StringView leaf = gp::platform::generic::Paths::getCleanFilename(path);
        if (isDynamicLibraryFilename(leaf))
        {
            return false;
        }

        constexpr gp::StringView kDylibExt = ".dylib";
        if ((path.size() + kDylibExt.size()) > kMaxPathLength)
        {
            return false;
        }

        outWithExt.assign(path);
        outWithExt.append(kDylibExt);
        return true;
    }

    /// @brief Expands a bare library or framework name into ordered Apple candidate filenames.
    /// @details For a `.framework` name such as `"Metal.framework"`, emits `["Metal.framework/Metal",
    /// "Metal.framework"]`.
    ///          For a bare name such as `"MoltenVK"`, emits `["MoltenVK", "MoltenVK.dylib", "libMoltenVK.dylib",
    ///          "MoltenVK.framework/MoltenVK"]`.
    /// @param[in] filename Bare library or framework name without directory separators.
    /// @param[out] outCandidates Destination list populated with unique candidate names.
    static constexpr void
        buildCandidateFilenames(const gp::StringView filename, gp::InlineVector<PathBuffer, 6u>& outCandidates) noexcept
    {
        outCandidates.clear();
        if (filename.isEmpty() || filename.size() > kMaxPathLength)
        {
            return;
        }

        if (isFrameworkPath(filename))
        {
            const PathBuffer innerBinary = resolveFrameworkBinaryPath(filename);
            if (!innerBinary.isEmpty())
            {
                outCandidates.pushBack(innerBinary);
            }
            outCandidates.pushBack(PathBuffer(filename));
            return;
        }

        constexpr gp::StringView kLibPrefix = "lib";
        constexpr gp::StringView kDylibExt = ".dylib";
        constexpr gp::StringView kFrameworkMid = ".framework/";

        outCandidates.pushBack(PathBuffer(filename));

        const bool hasDylib = isDynamicLibraryFilename(filename);
        const bool hasLibPrefix = filename.startsWith(kLibPrefix);
        const bool endsWithDot = filename.endsWith('.');

        if (!hasDylib && !endsWithDot && (filename.size() + kDylibExt.size()) <= kMaxPathLength)
        {
            PathBuffer withExt(filename);
            withExt.append(kDylibExt);
            outCandidates.pushBack(withExt);
        }

        if (!hasLibPrefix)
        {
            if (hasDylib && (kLibPrefix.size() + filename.size()) <= kMaxPathLength)
            {
                PathBuffer withPrefix(kLibPrefix);
                withPrefix.append(filename);
                outCandidates.pushBack(withPrefix);
            }
            else if (
                !hasDylib && !endsWithDot && (kLibPrefix.size() + filename.size() + kDylibExt.size()) <= kMaxPathLength
            )
            {
                PathBuffer withPrefixAndExt(kLibPrefix);
                withPrefixAndExt.append(filename);
                withPrefixAndExt.append(kDylibExt);
                outCandidates.pushBack(withPrefixAndExt);
            }
        }

        // Bare stem without dots or "lib" prefix may refer to a .framework bundle (for example "Metal").
        if (!hasDylib && !hasLibPrefix && filename.find('.') == gp::StringView::npos)
        {
            const gp::USize frameworkBinaryLen = filename.size() + kFrameworkMid.size() + filename.size();
            if (frameworkBinaryLen <= kMaxPathLength)
            {
                PathBuffer frameworkBinary(filename);
                frameworkBinary.append(kFrameworkMid);
                frameworkBinary.append(filename);
                outCandidates.pushBack(frameworkBinary);
            }
        }
    }
};

}   // namespace gp::platform::apple
