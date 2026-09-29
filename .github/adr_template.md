---
id: ADR-XXXX
title: Short, descriptive title of the decision
date: YYYY-MM-DD
status: proposed # proposed | accepted | rejected | deprecated | superseded
authors:
  - Author Name
deciders:
  - Decider Name
---

<!-- markdownlint-disable MD025 -->
# ADR-XXXX: [Short, descriptive title of the decision]

![Proposed][badge-proposed]
<!-- proposed | accepted | rejected | deprecated | superseded -->

*(If superseded, link to the new ADR: "Superseded by [ADR-YYYY](path/to/adr-yyyy.md)")*

## Context

Describe the problem space, the current technical constraints, and the forces at play.

- What is the issue we are trying to solve?
- Why does it need to be solved now?
- Are there performance, architectural, or organizational constraints?

## Decision

State the clear, actionable architectural decision.

- What are we choosing to do?
- Be precise and concrete. If this introduces a new pattern, tool, or folder structure, describe it
  exactly.

## Alternatives Considered

Briefly document other approaches that were evaluated and why they were ultimately rejected.

### Option A: [Short description of the alternative]

Explain the alternative.

**Why rejected:** Describe the reasons this option was not chosen. Focus on trade-offs, not just
personal preference.

### Option B: [Short description of the alternative]

Explain the alternative.

**Why rejected:** Describe the reasons this option was not chosen. Focus on trade-offs, not just
personal preference.

## Consequences

List the direct outcomes of applying this decision. Focus on trade-offs.

### Positive

- (e.g., Reduces cross-module coupling)
- (e.g., Improves rendering thread performance by 15%)

### Negative

- (e.g., Increases initial boilerplate for creating new modules)
- (e.g., Requires migrating X legacy systems to the new API)

## Rationale and Cross-References

- [ADR-YYYY](./ADR-YYYY.md): Link to related ADRs that influenced this decision.

**Canonical references:**

- Other projects, patterns, or literature that influenced this decision.

<!-- markdownlint-disable MD053 -->
[badge-proposed]: https://img.shields.io/badge/Status-Proposed-yellow.svg
[badge-accepted]: https://img.shields.io/badge/Status-Accepted-brightgreen.svg
[badge-rejected]: https://img.shields.io/badge/Status-Rejected-red.svg
[badge-deprecated]: https://img.shields.io/badge/Status-Deprecated-lightgrey.svg
[badge-superseded]: https://img.shields.io/badge/Status-Superseded-lightgrey.svg
