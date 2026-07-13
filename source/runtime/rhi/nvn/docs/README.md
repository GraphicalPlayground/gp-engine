---
sidebar_position: 0
title: NVN
description: Nintendo Switch low-level graphics API backend. NDA-protected, work in progress.
tags:
  - rhi
  - switch
  - nintendo
  - nda
  - wip
---

# NVN

<p style={{ color: '#ffffffa6' }}> Nintendo Switch low-level graphics API backend. </p>

NVN is Nintendo's low-level graphics API for the Switch family of consoles, distributed as part of
the Nintendo SDK. Like [GNM](../gnm/README.md) on PlayStation 4 and [Direct3D 12](../d3d12/README.md)
on desktop, it favors explicit command buffer and resource management over driver-managed state.

:::danger NDA-protected platform
The Nintendo SDK, including the NVN headers, samples, and reference documentation, is covered by
Nintendo's non-disclosure agreement with licensed developers. Nothing from that material can be
reproduced, described, or linked to on this page, and the same restriction applies to every page
under this section of the documentation.
:::

## Current status

The `nvn` backend in `gp-engine` is internal work in progress. The module currently holds build
scaffolding and an empty CMake target; no Nintendo NDA material has been added, because the team
does not yet have Nintendo NDA clearance.

Once that clearance is in place, this page will be filled in for the contributors on the project
who are also licensed Nintendo developers. It will not be published outside that group, and public
builds of `gp-engine` will not ship with this backend enabled.

## Requesting access

Access to the Nintendo SDK, dev kits, and NVN documentation is managed entirely by Nintendo. If you
need to review or contribute to this backend, you or your studio have to register directly through
Nintendo's own program.

- [Nintendo Developer Portal](https://developer.nintendo.com/): the registration portal for the
  Nintendo developer program.

We have no ability to grant, share, or forward Nintendo NDA material on request. Approval is at
Nintendo's sole discretion.

## Curriculum

Graphical Playground plans to cover console-specific rendering paths, NVN included, as part of its
learning material. That curriculum sits behind the same NDA and will only be released to
contributors and learners who hold active Nintendo developer clearance, once it exists.
