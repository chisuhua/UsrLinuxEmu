## ADDED Requirements

### Requirement: Git tags follow strict semver

All release tags SHALL follow the pattern `v<major>.<minor>.<patch>`, matching the regex `^v[0-9]+\.[0-9]+\.[0-9]+$`.

#### Scenario: Release tag created
- **WHEN** a maintainer creates a git tag for release
- **THEN** the tag name SHALL match `^v[0-9]+\.[0-9]+\.[0-9]+$` (e.g., `v1.0.0`, `v1.0.1`, `v2.0.0`)

#### Scenario: Non-compliant tag rejected
- **WHEN** a tag name does not match strict semver (e.g., `v1.5`, `v1.0.0-beta`, `release-1`)
- **THEN** the release workflow SHALL NOT be triggered

### Requirement: Tag policy documented in ADR

The tag naming policy SHALL be documented in a new ADR under `docs/00_adr/`.

#### Scenario: ADR exists
- **WHEN** a developer reads `docs/00_adr/`
- **THEN** they SHALL find an ADR file documenting the tag naming policy

### Requirement: CI validates tag format

The CI workflow SHALL validate tag format on tag-push events.

#### Scenario: Tag format validation
- **WHEN** a workflow is triggered by a tag push
- **THEN** the first job SHALL validate the tag matches `^v[0-9]+\.[0-9]+\.[0-9]+$` and fail early if not