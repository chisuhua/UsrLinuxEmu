# ci-docs-audit-hook Specification

## Purpose
TBD - created by archiving change stage-2-multi-device. Update Purpose after archive.
## Requirements
### Requirement: docs-audit strict mode for Stage 2 paths

When pre-commit runs `tools/docs-audit.sh --strict`, it MUST verify Stage 2-related docs contain proper cross-references.

#### Scenario: committed doc requires new Stage 2 cross-ref

- **WHEN** developer commits `docs/roadmap/stage-*.md`
- **THEN** `docs-audit.sh --strict` verifies cross-ref to ADR-036 (3-way separation)
- **AND** verifies cross-ref to ADR-038 (network stack)
- **AND** fails if either reference missing

### Requirement: hook scope limited to Stage 2 paths

The hook MUST NOT regress existing pre-commit behavior for Stage 1.4 docs.

#### Scenario: backward compat

- **WHEN** existing Tier-1 docs are committed
- **THEN** Stage 2 hooks do not introduce false positives
- **AND** existing checks continue to pass

