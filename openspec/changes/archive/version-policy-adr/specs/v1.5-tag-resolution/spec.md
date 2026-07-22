## ADDED Requirements

### Requirement: v1.5 tag renamed to milestone tag

The existing `v1.5` git tag SHALL be renamed to `milestone-phase2.5-hotfix` to avoid collision with strict semver release tags.

#### Scenario: v1.5 tag renamed
- **WHEN** a developer runs `git tag -l`
- **THEN** `v1.5` SHALL NOT appear as a release-like tag
- **THEN** `milestone-phase2.5-hotfix` SHALL exist, annotated with historical context

### Requirement: v1.5 tag history documented

The purpose and history of the `v1.5` tag SHALL be documented in the version policy ADR.

#### Scenario: ADR records v1.5 history
- **WHEN** a developer reads the version policy ADR
- **THEN** they SHALL find a note explaining that `v1.5` was a pre-policy internal milestone tag (Phase 2.5 hotfix, commit 6d090e6) and has been renamed to `milestone-phase2.5-hotfix`

### Requirement: Release workflow excludes non-semver tags

The `v*.*.*` trigger pattern in `v1-0-release-prep`'s release workflow SHALL be replaced with a strict semver pattern `v[0-9]+.[0-9]+.[0-9]+` to prevent accidental triggers.

#### Scenario: Release workflow strict matching
- **WHEN** a tag `v1.5` is pushed
- **THEN** the release workflow SHALL NOT be triggered