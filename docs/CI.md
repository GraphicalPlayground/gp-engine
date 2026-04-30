---
sidebar_position: 3
title: Continuous Integration
description: Information about the CI pipeline for Graphical Playground.
tags:
  - ci
  - continuous integration
  - github actions
---

# Continuous Integration

<p style={{ color: '#ffffffa6' }}> Information about the CI pipeline for Graphical Playground. </p>

At Graphical Playground, we use [GitHub Actions](https://docs.github.com/en/actions) to automate
our build, validation, formatting, and release pipelines. All workflow files live under
`.github/workflows/` in the repository root.

Adhering to the practices described in this document is mandatory. A pull request that breaks CI
must not be merged until the failure is resolved.

## Workflow Overview

The table below lists every workflow file, its trigger, and its responsibility.

| File | Trigger | Purpose |
|---|---|---|
| `build.yml` | Push to `main`, Pull Request, `workflow_call` | Compile the engine across all target platforms |
| `formatting.yml` | Pull Request | Enforce `clang-format` on all C++ source files |
| `release.yml` | Push of a version tag (`v*.*.*`) | Build, package, and draft a GitHub release |
| `shader-ci.yml` | Pull Request (shader file changes) | Validate GLSL/HLSL shaders |
| `sync-docs.yml` | Push to `main` (documentation changes) | Mirror docs to the central `gp-docs` portal |
| `labeler.yml` | Pull Request | Automatically apply labels based on changed paths |
| `welcome.yml` | Pull Request opened | Post a welcome comment for first-time contributors |

---

## CI Build

The core of our pipeline is `build.yml`. It compiles the engine in `Release` mode across every
supported operating system. The job is designed to be both efficient and flexible through a
two-stage matrix approach.

### Triggers

The build workflow activates on three distinct events:

- **Push to `main`**: Runs automatically whenever code lands on the default branch. Supports
  commit-message tags to fine-tune which platforms are built (see [Selective Builds](#selective-builds)).
- **Pull Request targeting `main`**: Always builds on all platforms, with no tag-based filtering.
  This ensures complete coverage before code is merged.
- **`workflow_call`**: Allows other workflows (such as `release.yml`) to invoke the build job
  as a reusable step, with optional parameters for platform filtering and artifact packaging.

### Path Filtering

The build job does not run on every push to `main`. It is scoped to changes within the following
paths:

- `source/**`
- `thirdparty/**`
- `toolchain/**`
- `cmake/**`
- `CMakeLists.txt`

Changes that touch only documentation, CI workflow files, or other non-source assets will not
trigger a build. This keeps runner usage lean.

### Build Matrix

The `setup-matrix` job runs first. It dynamically computes which operating systems to build against
and passes the result to the `build` job via an output variable.

The three supported runners are:

- `ubuntu-latest`
- `windows-latest`
- `macos-latest`

The `build` job uses `fail-fast: false`. This means a failure on one platform will not cancel the
builds on the remaining platforms. We always want to see the full picture of breakage across all targets.

### Selective Builds

On a push to `main`, you may control the build matrix by embedding a tag in the commit message.
This is useful when a change is platform-specific and building everywhere would waste runner time.

- **`[skip ci]`**: Cancels all builds for that commit.
  - Note: GitHub natively honors `[skip ci]`, but we handle it explicitly as well.
- **`[run linux,windows]`**: Builds only on the listed platforms.
- **`[skip macos]`**: Builds everywhere except the listed platforms.

Valid platform keys for these tags are: `linux`, `ubuntu`, `windows`, `macos`.

:::tip
Selective build tags apply **only to pushes to `main`**. Pull requests always build on all
platforms: do not expect these tags to have any effect on PR-triggered runs.
:::

```yaml
# Example commit messages

# Build only on Linux and Windows
fix: resolve vertex buffer alignment [run linux,windows]

# Skip the macOS runner for this push
chore: update third-party headers [skip macos]

# Skip CI entirely
docs: fix typo in README [skip ci]
```

### Caching

We use two caching strategies to minimize build times:

- **`ccache`**: Compiler output is cached via `hendrikmuhs/ccache-action`. The cache key is
  scoped to the OS and build type (e.g., `ubuntu-latest-Release`).
- **CMake `FetchContent`**: Dependencies fetched by CMake are cached under `~/.cache/cmake_deps`.
  The cache key is derived from a hash of all `CMakeLists.txt` and `.cmake` files.

Do not disable or remove these caching steps without a documented justification. Cold builds are
significantly slower and consume unnecessary CI minutes.

### Configure and Build Steps

After caching, the workflow runs two CMake commands:

```yaml
# Configure
cmake -S . -B build \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER_LAUNCHER=ccache \
  -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
  -DFETCHCONTENT_BASE_DIR="$HOME/.cache/cmake_deps"

# Build
cmake --build build --config Release --parallel --verbose
```

- We always build with **Ninja** for consistent cross-platform performance.
- We always pass `--parallel` to utilize all available cores on the runner.
- We always pass `--verbose` to ensure full compiler output is visible in the CI log for
  diagnosis purposes.

### Artifact Packaging

Artifact packaging is **disabled by default**. It is only enabled when the workflow is called
with `package-artifacts: true`, which the `release.yml` workflow does automatically.

When enabled, the following additional steps run:

1. **Install**: CMake installs the build output into `install_staging/` using
   `cmake --install`.
2. **Version resolution**: The version string is resolved from the Git tag (if the workflow was
   triggered by one) or from the first line of the `VERSION` file.
3. **Packaging**: Archives are created per platform:
   - Linux / macOS: `.tar.gz` via `tar`
   - Windows: `.zip` via PowerShell's `Compress-Archive`
4. **Upload**: Artifacts are uploaded under the name `release-{os}` with a 14-day retention
   period. The step fails if no archive file is found (`if-no-files-found: error`).

---

## Code Formatting

The `formatting.yml` workflow enforces code style on every pull request. It uses
`jidicula/clang-format-action` at version `22` and checks all files under the `source/` directory.

A pull request that fails the formatting check must not be merged. Fix the formatting locally
before pushing.

:::tip
Run `clang-format` on your changes before opening a pull request. Most editors and IDEs support
format-on-save when pointed at the project's `.clang-format` configuration file. Use this to
avoid formatting failures entirely.
:::

The following additional checks are planned for future integration:

- `clang-tidy`: static analysis
- `cmake-format` / `cmake-lint`: CMake file consistency
- `markdownlint`: documentation formatting

---

## Release Pipeline

The `release.yml` workflow is triggered automatically when a tag matching the pattern `v*.*.*` is
pushed to the repository. It is responsible for producing a complete, packaged draft release on
GitHub.

### Steps

1. **Compile artifacts**: Invokes `build.yml` via `workflow_call` with `package-artifacts: true`.
   This compiles and archives the engine on all three platforms.
2. **Download artifacts**: Downloads all uploaded archives from the build job into a local
   `artifacts/` directory.
3. **Create draft release**: Uses `softprops/action-gh-release` to create a **draft** release
   with auto-generated release notes and all platform archives attached.

The release is always created as a **draft**. A maintainer must manually review and publish it.
Do not publish a release without verifying the attached artifacts.

### Versioning

The resolved version string follows this priority:

- If triggered by a Git tag (e.g., `v1.2.0`), the tag name is used directly.
- Otherwise, the first line of the `VERSION` file in the repository root is used.

Always ensure the `VERSION` file is updated and committed before pushing a release tag.

:::tip
The canonical way to cut a release is to push a version tag:
```
git tag v1.2.0
git push origin v1.2.0
```
The pipeline handles everything from that point forward.
:::

---

## Shader Validation

The `shader-ci.yml` workflow runs on pull requests that modify files in `source/shaders/`. It
targets the following file extensions:

- `.hlsl`
- `.glsl`
- `.vert`
- `.frag`
- `.geom`

:::tip
This workflow is currently a **stub**. The Vulkan SDK installation and the
`glslangValidator` invocation steps are planned but not yet implemented. Shader validation is
not yet enforced at the CI level.
:::

---

## Documentation Sync

The `sync-docs.yml` workflow runs on pushes to `main` when any file matching `**/docs/**` is
modified. It automatically mirrors documentation into the central
[`gp-docs`](https://github.com/GraphicalPlayground/gp-docs) portal repository.

### How It Works

1. Both `gp-engine` and `gp-docs` are checked out in parallel.
2. All files matching `*/docs/*` within `gp-engine` are discovered via `find`.
3. Each file is copied to the corresponding path under `gp-docs/docs/gp-engine/`, preserving the
   directory structure but stripping the `docs/` path segment.
4. If any changes are detected in `gp-docs`, they are committed and pushed by the
   `github-actions[bot]` user.

The sync requires a Personal Access Token with write access to `gp-docs`, stored as the
`DOCS_SYNC_PAT` repository secret. Do not rotate or delete this secret without updating the
workflow.

:::tip
Do not manually edit files under `gp-docs/docs/gp-engine/`. That directory is fully managed by
this sync workflow and any manual changes will be overwritten on the next push to `gp-engine`.
:::

---

## Automated Labeling and Contributor Onboarding

### Pull Request Labeler

The `labeler.yml` workflow uses `actions/labeler@v6` to automatically apply labels to pull
requests based on the files changed. Label rules are defined in `.github/labeler.yml`.

Keep the labeler configuration up to date whenever new top-level directories or source modules
are introduced.

### Welcome Message

The `welcome.yml` workflow posts an automated comment on a contributor's very first pull request.
It fires on `pull_request_target` when the PR is opened and is suppressed for bot accounts.

The welcome message directs contributors to:

- The formatting guidelines.
- The `CONTRIBUTING.md` file.

Do not remove or disable this workflow. First impressions matter, and the message provides
immediate, actionable guidance to new contributors.

---

## Concurrency Control

The CI build workflow uses a concurrency group keyed on the workflow name and the Git ref:

```yaml
concurrency:
  group: ${{ github.workflow }}-${{ github.ref }}
  cancel-in-progress: true
```

When a new push arrives on the same branch while a build is already running, the in-progress run
is cancelled automatically. This prevents queuing multiple redundant builds during rapid iteration.

---

## Permissions

We follow the principle of least privilege for all workflow permissions.

| Workflow | Permission | Reason |
|---|---|---|
| `build.yml` | `contents: read` | Source checkout only |
| `release.yml` | `contents: write` | Required to create and upload the GitHub release |
| `labeler.yml` | `contents: read`, `pull-requests: write` | Read files, apply labels |
| `welcome.yml` | `pull-requests: write` | Post the welcome comment |

Do not elevate workflow permissions beyond what is listed here without a code review and a
documented justification in the workflow file itself.