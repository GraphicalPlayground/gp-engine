// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Vector.hpp"      // IWYU pragma: keep
#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "platforms/base/system/PathTypes.hpp"
#include "platforms/generic/system/GenericPaths.hpp"

namespace gp::platform::unix
{

/// @brief Pure lexical naming helpers for POSIX and Linux shared object (`.so`) filenames.
/// @details Handles both unversioned (`libvulkan.so`) and ELF versioned sonames (`libvulkan.so.1`, `libGL.so.1.7.0`)
///          and expands bare library names with the standard POSIX `lib` prefix and `.so` suffix.
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
    /// @brief Checks whether a bare filename is a POSIX shared object (`.so` or versioned `.so.<digit>...`).
    /// @param[in] fileName Bare filename without directory components.
    /// @return True if `fileName` ends with `".so"` or contains `".so."` followed by a numeric version component.
    [[nodiscard]] static constexpr bool isSharedObjectFilename(const gp::StringView fileName) noexcept
    {
        constexpr gp::StringView kSoExt = ".so";
        constexpr gp::StringView kVersionedToken = ".so.";

        if (fileName.size() > kSoExt.size() && fileName.endsWith(kSoExt))
        {
            return true;
        }

        const gp::USize tokenPos = fileName.find(kVersionedToken);
        if (tokenPos != gp::StringView::npos && tokenPos > 0u)
        {
            const gp::USize versionStart = tokenPos + kVersionedToken.size();
            if (versionStart < fileName.size())
            {
                const char firstVersionChar = fileName[versionStart];
                return firstVersionChar >= '0' && firstVersionChar <= '9';
            }
        }

        return false;
    }

    /// @brief Checks whether the leaf filename of `path` is a POSIX shared object.
    /// @param[in] path Relative or absolute file path.
    /// @return True if the leaf component matches `.so` or `.so.<version>`.
    [[nodiscard]] static constexpr bool hasSharedObjectSuffix(const gp::StringView path) noexcept
    {
        return isSharedObjectFilename(gp::platform::generic::Paths::getCleanFilename(path));
    }

    /// @brief Appends `".so"` to `path` if it does not already carry a shared-object suffix or trailing dot.
    /// @param[in] path Input path or bare library name.
    /// @param[out] outWithExt Receives `path + ".so"` when an implicit suffix applies.
    /// @return True if `outWithExt` was populated; false if `path` already has a `.so` suffix or cannot fit.
    [[nodiscard]] static constexpr bool
        tryBuildWithDefaultExtension(const gp::StringView path, PathBuffer& outWithExt) noexcept
    {
        outWithExt.clear();
        if (path.isEmpty() || path.endsWith('.') || hasSharedObjectSuffix(path))
        {
            return false;
        }

        constexpr gp::StringView kSoExt = ".so";
        if ((path.size() + kSoExt.size()) > kMaxPathLength)
        {
            return false;
        }

        outWithExt.assign(path);
        outWithExt.append(kSoExt);
        return true;
    }

    /// @brief Expands a bare library name into ordered POSIX candidate filenames.
    /// @details Preserves the caller's exact name first, then appends `.so` and `lib`-prefixed variants as needed.
    ///          For example, `"vulkan"` expands to `["vulkan", "vulkan.so", "libvulkan.so"]`, while `"libvulkan.so.1"`
    ///          stays `["libvulkan.so.1"]`.
    /// @param[in] filename Bare library name without directory separators.
    /// @param[out] outCandidates Destination list populated with unique candidate filenames.
    static constexpr void
        buildCandidateFilenames(const gp::StringView filename, gp::InlineVector<PathBuffer, 4u>& outCandidates) noexcept
    {
        outCandidates.clear();
        if (filename.isEmpty() || filename.size() > kMaxPathLength)
        {
            return;
        }

        constexpr gp::StringView kLibPrefix = "lib";
        constexpr gp::StringView kSoExt = ".so";

        outCandidates.pushBack(PathBuffer(filename));

        const bool hasSo = isSharedObjectFilename(filename);
        const bool hasLibPrefix = filename.startsWith(kLibPrefix);
        const bool endsWithDot = filename.endsWith('.');

        if (!hasSo && !endsWithDot && (filename.size() + kSoExt.size()) <= kMaxPathLength)
        {
            PathBuffer withExt(filename);
            withExt.append(kSoExt);
            outCandidates.pushBack(withExt);
        }

        if (!hasLibPrefix)
        {
            if (hasSo && (kLibPrefix.size() + filename.size()) <= kMaxPathLength)
            {
                PathBuffer withPrefix(kLibPrefix);
                withPrefix.append(filename);
                outCandidates.pushBack(withPrefix);
            }
            else if (!hasSo && !endsWithDot && (kLibPrefix.size() + filename.size() + kSoExt.size()) <= kMaxPathLength)
            {
                PathBuffer withPrefixAndExt(kLibPrefix);
                withPrefixAndExt.append(filename);
                withPrefixAndExt.append(kSoExt);
                outCandidates.pushBack(withPrefixAndExt);
            }
        }
    }
};

}   // namespace gp::platform::unix
