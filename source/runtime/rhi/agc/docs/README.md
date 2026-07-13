---
sidebar_position: 0
title: AGC
description: PlayStation 5 low-level graphics API backend. NDA-protected, work in progress.
tags:
  - rhi
  - ps5
  - playstation
  - nda
  - wip
---

# AGC

<p style={{ color: '#ffffffa6' }}> PlayStation 5 low-level graphics API backend. </p>

AGC is Sony's low-level graphics library for the PlayStation 5. It is the successor to the
[GNM](../gnm/README.md) and [GNMX](../gnmx/README.md) libraries used on PlayStation 4, following
the same close-to-the-hardware approach:
the engine builds command buffers and manages GPU state directly, rather than going through a
higher-level abstraction layer.

:::danger NDA-protected platform
The PlayStation 5 SDK, including the AGC headers, samples, and reference documentation, is
covered by Sony's non-disclosure agreement with registered PlayStation Partners. Nothing from
that material can be reproduced, described, or linked to on this page, and the same restriction
applies to every page under this section of the documentation.
:::

## Current status

The `agc` backend in `gp-engine` is internal work in progress. The module currently holds build
scaffolding and an empty CMake target; no PlayStation NDA material has been added, because the
team does not yet have PlayStation NDA clearance.

Once that clearance is in place, this page will be filled in for the contributors on the project
who are also registered PlayStation Partners. It will not be published outside that group, and
public builds of `gp-engine` will not ship with this backend enabled.

## Requesting access

Access to the PS5 SDK, dev kits, and AGC documentation is managed entirely by Sony. If you need
to review or contribute to this backend, you or your studio have to register directly through
Sony's own program.

- [PlayStation Partners](https://partners.playstation.net/): the registration portal for the
  PlayStation developer program.

We have no ability to grant, share, or forward PlayStation NDA material on request. Approval is
at Sony's sole discretion.

## Curriculum

Graphical Playground plans to cover console-specific rendering paths, AGC included, as part of
its learning material. That curriculum sits behind the same NDA and will only be released to
contributors and learners who hold active PlayStation Partners clearance, once it exists.
