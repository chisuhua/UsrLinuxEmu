# cleanup-orphan-spec-purpose Specification

## Purpose

Cleanup TBD Purpose placeholders in archived OpenSpec specs (A3 #3 from v0.1.6 SSOT audit, extended from 3 to 5 specs). Establishes recursive-prevention practice for future archives. The capability ensures every archived capability spec has a real Purpose describing the capability, not a TBD template placeholder.

## Requirements

### Requirement: Non-TBD Purpose in all 5 affected archived specs

The `cleanup-orphan-spec-purpose` capability MUST ensure that the following 5 archived spec.md files have a non-`TBD` `## Purpose` section, with each Purpose extracted (1-3 sentences) from the corresponding archived `proposal.md` "Why" section:
- `openspec/specs/adr-placeholder-cleanup/spec.md`
- `openspec/specs/gpu-pushbuffer-validation/spec.md`
- `openspec/specs/gpu-pushbuffer-validation-deployment/spec.md`
- `openspec/specs/ssot-deep-audit/spec.md`
- `openspec/specs/ssot-v0-1-7-comprehensive-fix/spec.md`

#### Scenario: Each Purpose describes the underlying capability
- **WHEN** any of the 5 affected spec.md files is read
- **THEN** its `## Purpose` section MUST contain real capability description text (not the OpenSpec `TBD - created by archiving change ...` template placeholder)
- **AND** the Purpose MUST be derivable from the corresponding archived `proposal.md` "Why" section

#### Scenario: TBD template removed from Purpose sections
- **WHEN** the change is applied
- **THEN** `grep -l "TBD - created by archiving" openspec/specs/*/spec.md` returns no files in the Purpose section
- **AND** any remaining `TBD` substring matches are limited to legitimate references inside `## Requirements` body text describing the audit rule itself

### Requirement: Recursive prevention for future archives

The `cleanup-orphan-spec-purpose` capability MUST prevent the recursive re-introduction of TBD Purpose placeholders when this change is itself archived. The `openspec/specs/cleanup-orphan-spec-purpose/spec.md` MUST be authored with a real Purpose before `openspec archive` runs, so that the archive flow either skips spec generation (`--skip-specs`) or preserves the pre-written file.

#### Scenario: This spec's own Purpose is non-TBD before archive
- **WHEN** the maintainer is about to run `openspec archive cleanup-orphan-spec-purpose --yes`
- **THEN** `openspec/specs/cleanup-orphan-spec-purpose/spec.md` MUST already exist on filesystem
- **AND** its `## Purpose` section MUST NOT contain any `TBD - created by archiving change ...` placeholder
- **AND** if `openspec archive` re-creates the spec from a template, the maintainer MUST overwrite it post-archive with the same non-TBD content

#### Scenario: No new orphan TBD specs introduced
- **WHEN** this change is archived
- **THEN** the total count of spec.md files containing `TBD - created by archiving` in their `## Purpose` section MUST be 0
- **AND** the count MUST remain 0 for all future archives that follow this capability's pattern

### Requirement: Single-commit + archive workflow

The `cleanup-orphan-spec-purpose` capability MUST be applied as a single atomic git commit followed by `openspec archive cleanup-orphan-spec-purpose --yes`. No intermediate commits, no force-pushes, no skips.

#### Scenario: One commit covering all 5 Purpose replacements
- **WHEN** the change is committed
- **THEN** `git log -1 --format=%s` shows a `docs(specs):` Conventional Commits subject line
- **AND** the commit message MUST name all 5 affected specs
- **AND** the commit MUST close A3 #3 from v0.1.6 audit and reference the scope extension to 5 specs after v0.1.7

#### Scenario: Successful archive produces no TBD
- **WHEN** `openspec archive cleanup-orphan-spec-purpose --yes` completes
- **THEN** the change directory is moved under `openspec/changes/archive/2026-06-17-cleanup-orphan-spec-purpose/`
- **AND** `openspec list` no longer shows `cleanup-orphan-spec-purpose` as active
- **AND** `openspec/specs/cleanup-orphan-spec-purpose/spec.md` exists with a non-TBD Purpose (per the recursive-prevention requirement)
