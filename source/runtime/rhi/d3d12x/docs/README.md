---
sidebar_position: 0
title: DirectX 12 Xbox
description: Xbox low-level graphics API backend. NDA-protected, work in progress.
tags:
  - rhi
  - xbox
  - directx
  - nda
  - wip
---

# DirectX 12 Xbox

<p style={{ color: '#ffffffa6' }}> Xbox low-level graphics API backend. </p>

D3D12X is Microsoft's Xbox-specific extension of [Direct3D 12](../d3d12/README.md), distributed
through the Xbox Game Development Kit (GDK). It builds on the retail D3D12 API and adds
console-specific extensions and optimizations that are not exposed on desktop Windows.

:::danger NDA-protected platform
The Xbox GDK, including the D3D12X headers, samples, and reference documentation, is covered by
Microsoft's non-disclosure agreement with registered Xbox developers. Nothing from that material
can be reproduced, described, or linked to on this page, and the same restriction applies to every
page under this section of the documentation.
:::

## Current status

The `d3d12x` backend in `gp-engine` is internal work in progress. The module currently holds build
scaffolding and an empty CMake target; no Xbox NDA material has been added, because the team does
not yet have Xbox NDA clearance.

Once that clearance is in place, this page will be filled in for the contributors on the project
who are also registered Xbox developers. It will not be published outside that group, and public
builds of `gp-engine` will not ship with this backend enabled.

## Requesting access

Access to the Xbox GDK, dev kits, and D3D12X documentation is managed entirely by Microsoft. If you
need to review or contribute to this backend, you or your studio have to register directly through
Microsoft's own program.

- [ID@Xbox](https://www.xbox.com/en-us/developers/id): the registration portal for independent and
  studio developers seeking Xbox development access.

We have no ability to grant, share, or forward Xbox NDA material on request. Approval is at
Microsoft's sole discretion.

## Curriculum

Graphical Playground plans to cover console-specific rendering paths, D3D12X included, as part of
its learning material. That curriculum sits behind the same NDA and will only be released to
contributors and learners who hold active Xbox developer clearance, once it exists.
