# Graphical Playground Governance

This document establishes the governance model for the **Graphical Playground** ecosystem, covering `gp-engine`, `gp-docs`, and associated tooling. The goal is an open, transparent community that welcomes contributions while maintaining technical and quality standards.

The Project accepts contributions from independent developers, students, and researchers. A **Steering Committee (SC)** oversees development to ensure the engine remains useful to the broader community and stays true to one principle it will not trade away: code should be readable and teachable, not merely optimized. The SC determines where work belongs within the ecosystem, whether in the core engine, a community add-on, or a contributor's own project.

This document refers to the project collectively as "the Project."

> Graphical Playground intends to maintain high technical and quality standards. It is ultimately up to each user to verify that the software meets any requirements specific to their context.

## Applicability

This document applies to all repositories under the Graphical Playground organization, including `gp-engine`, `gp-docs`, and official tooling. It also applies to community repositories directly managed by the Project. Third-party forks and add-ons are governed by their creators.

## Roles

The community is organized into roles with distinct responsibilities and privileges.

| Role              | Available to                   | Description                                                                                              |
|-------------------|--------------------------------|----------------------------------------------------------------------------------------------------------|
| User              | Anyone                         | Anyone who downloads, deploys, or learns from the engine.                                                |
| Contributor       | Anyone                         | Anyone who provides input to the Project: code, issues, documentation, tutorials, etc.                   |
| SC Member         | Active, sustained contributors | Member of the Steering Committee (SC), responsible for guiding Project development.                      |
| Maintainer        | Active SC members              | Holds binding review and merge authority for specific repositories. Selected by the SC.                  |
| Community Manager | Active SC members              | Responsible for executing SC decisions. Also a member of the SC.                                         |

Each role carries a set of privileges:

| Role              | Read/Clone | Propose pull request | Comment in tickets/discussions | Review | Binding review | Merge | Project decisions | Release |
|-------------------|------------|----------------------|--------------------------------|--------|----------------|-------|-------------------|---------|
| User              | ✅         | ❌                   | ❌                             | ❌     | ❌             | ❌    | ❌                | ❌      |
| Contributor       | ✅         | ✅                   | ✅                             | ✅     | ❌             | ❌    | ❌                | ❌      |
| SC Member         | ✅         | ✅                   | ✅                             | ✅     | ❌             | ❌    | ✅                | ❌      |
| Maintainer        | ✅         | ✅                   | ✅                             | ✅     | ✅             | ✅    | ❌                | ✅      |
| Community Manager | ✅         | ✅                   | ✅                             | ✅     | ✅             | ✅    | ✅                | ✅      |

### User

Anyone who downloads or uses Graphical Playground is a User. Users may read and clone any Project repository. A User who opens an issue or posts to the community becomes a Contributor.

### Contributor

Contributors are anyone who provides input to the Project. This includes code, issues, documentation, tutorials, and design work. The easiest way to get started is to join the Discussions or submit an Issue.

Contributors must follow the [Code of Conduct](CODE_OF_CONDUCT.md) and the [Contributing Guide](CONTRIBUTING.md).

### SC Member

Steering Committee members guide the direction of the Project. As a group, they decide how proposed ideas fit within the ecosystem and whether contributions belong in the core engine, a community add-on, or a contributor's own project. SC members gather feedback through Discussions and Issues, and open Calls for Comments when wider input is needed before a decision is finalized.

SC membership is open to active, sustained contributors and is determined by the existing SC.

### Maintainer

Maintainers hold binding review authority for specific repositories and are authorized to merge and release changes within their scope. They work alongside Community Managers to carry out SC decisions for their assigned repository. Maintainers are selected by the SC.

### Community Manager

Community Managers carry out SC decisions. This includes communicating outcomes, merging pull requests, and running Calls for Comments. Community Managers are also SC members and participate in all Project decisions.

Pull requests are merged by Community Managers after receiving a binding review from at least one SC Member.

## The decision-making process

The Project must serve a broad community of learners and practitioners while staying true to its educational mission. The SC guides these decisions.

Ideas are submitted as Issues and placed in "SC Review" on submission. Contributors should avoid developing substantial changes without SC approval, as unapproved work may be reworked or rejected.

The SC evaluates each idea to answer: how does this fit within the Graphical Playground ecosystem? For small changes like bug reports, this is usually a fast decision. For larger or more disruptive proposals, the SC may produce one of the following outcomes:

1. A Call for Comments
2. A recommendation for modifications
3. Acceptance into the core engine
4. Acceptance as a community add-on
5. A recommendation to develop the work within the contributor's own project

Contributors can check the status of an item on the SC Resolution Board.

### Call for Comments

When the SC lacks sufficient context, a proposal is incomplete, or a change could affect a large portion of the community, the SC will open a Call for Comments in the Project's Discussions. Community input is gathered before the SC re-evaluates the proposal.

### A recommendation for modifications

The SC may respond with suggested changes to help an idea fit better within the engine's scope and standards. Receiving these recommendations usually means the contribution is wanted and the SC is helping it meet the bar for inclusion.

### Acceptance into the core engine

The SC has determined that the contribution belongs in the core Graphical Playground engine and development may begin. When evaluating acceptance, the SC applies the Project's foundational principle: the preferred implementation is the one that is easier to explain and teach, not simply the most compact or optimized.

### Acceptance as a community add-on

When an idea has merit but limited applicability to the broader community, the SC may recommend development as a community add-on. These add-ons are available to all users and released independently from the core engine. Should a community add-on demonstrate sufficient merit and code quality, it may be adopted into the official organization.

### A recommendation to develop within a contributor's project

Some ideas fit better within a contributor's own codebase. These may be changes with narrow community impact, broad breaking changes, or contributions that require looser standards than the Project allows. In these cases, the SC routes the idea back to the contributor.

## Bypassing the process

Since Graphical Playground is open source, nothing prevents a Contributor from submitting a pull request without SC approval. Community Managers will make a reasonable effort to evaluate these contributions and seek SC review. This effort is not guaranteed, and unsolicited contributions may be closed without feedback. The process above is always the recommended path.

## Conflict resolution

Technical disagreements are expected and healthy in graphics engineering. The Project resolves them by:

1. Focusing on the code, not the contributor.
2. Applying the pedagogical principle: when two technically valid approaches exist, the one that is easier to explain and teach takes precedence.
3. Escalating to the SC: if Maintainers cannot reach consensus, the SC reviews the arguments and makes a final, binding decision.

All participants must follow the [Code of Conduct](CODE_OF_CONDUCT.md). Harassment, gatekeeping, and elitism have no place here.

## Communication channels

GitHub is the primary communication medium for the Project. Issues and Discussions are the main venues for community conversation. The "Reviews" feature communicates feedback on specific contributions. The SC comments on Issues and pull requests to document decisions. Community Managers use the "Reviews" feature to provide the required binding pre-merge reviews.

Announcements communicate Project-wide news to the entire community. Calls for Comments gather input on specific SC proposals.

To report security concerns, follow the [Security Policy](SECURITY.md).

## Changing this document

This governance model is a living document. Any community member may propose a change via a pull request. Changes to this document require approval by a majority of the SC.

## Acknowledgements

The structure of this document was modeled after the [NASA F Prime Project Governance](https://github.com/nasa/fprime/blob/devel/GOVERNANCE.md).
