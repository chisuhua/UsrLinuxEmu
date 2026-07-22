## ADDED Requirements

### Requirement: CHANGELOG.md exists at project root
The project SHALL maintain a `CHANGELOG.md` following Keep a Changelog format.

#### Scenario: CHANGELOG is present
- **WHEN** user views project root
- **THEN** `CHANGELOG.md` shall exist with entries grouped by version

### Requirement: Entries grouped by Conventional Commits type
CHANGELOG entries SHALL be organized by Conventional Commits types (`feat`, `fix`, `docs`, `refactor`, `perf`, `test`, `build`, `ci`, `chore`).

#### Scenario: Entry classification
- **WHEN** a commit message starts with `feat(gpu):`
- **THEN** it shall appear under the "Added" section for the corresponding version

### Requirement: RELEASE_NOTES.md exists
The project SHALL have a `RELEASE_NOTES.md` summarizing each release.

#### Scenario: Release notes present
- **WHEN** a new version tag is created
- **THEN** `RELEASE_NOTES.md` shall be updated with the new version's summary