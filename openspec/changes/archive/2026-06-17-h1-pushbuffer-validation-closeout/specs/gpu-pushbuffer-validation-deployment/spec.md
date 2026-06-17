# Capability: gpu-pushbuffer-validation-deployment

## ADDED Requirements

### Requirement: TaskRunner client can plumb va_space_handle

The `external/TaskRunner/include/gpu_driver_client.h` `GpuDriverClient` class MUST provide a way for callers to specify a VA Space handle that will be populated into `args.va_space_handle` when submitting `GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH`. The plumbing MUST be opt-in: existing call sites that do not use the new API MUST continue to compile and behave identically (zero-initialized `va_space_handle`).

#### Scenario: New API passes handle through

- **WHEN** a caller invokes the new VA Space setter (e.g. `setCurrentVASpace(42)`) and then calls `submitPushbuffer(...)`
- **THEN** the ioctl receives `args.va_space_handle == 42`
- **AND** the H-1 validation runs against VA Space 42

#### Scenario: Legacy callers unaffected

- **WHEN** a caller does not invoke the new VA Space setter (legacy code path)
- **THEN** `args.va_space_handle` is `0` at ioctl time
- **AND** the H-1 validation is skipped (per the D1 backward-compatibility design from H-1)
- **AND** the legacy caller compiles and runs without modification

### Requirement: TaskRunner submodule cross-repo commit

The `external/TaskRunner/` submodule MUST receive a commit (either directly pushed if user has push access, or referenced in a pointer-only commit in the main repo if not) that updates the `GpuDriverClient` per the previous requirement. The submodule pointer in the main repo MUST be updated to reference this new commit.

#### Scenario: Direct commit path

- **WHEN** user has push access to the TaskRunner upstream repository
- **THEN** user commits the client changes inside `external/TaskRunner/`
- **AND** pushes the commit to the TaskRunner remote
- **AND** updates the main repo's submodule pointer to reference the new commit

#### Scenario: Pointer-only path

- **WHEN** user does not have push access to TaskRunner upstream
- **THEN** user creates the TaskRunner change as a separate PR
- **AND** in the main repo, makes a commit whose message references "TaskRunner PR #XX" without changing the submodule pointer
- **AND** the pointer is updated in a follow-up commit once the PR is merged upstream

### Requirement: TaskRunner plan documentation updated

The `external/TaskRunner/plans/sync-plan.md` (or equivalent) MUST be updated to mark the S3.1 (PUSHBUFFER_SUBMIT_BATCH va_space_handle) task as completed once the client change is merged.

#### Scenario: Sync plan reflects completion

- **WHEN** the TaskRunner client change lands
- **THEN** the sync-plan entry for S3.1 shows "✅ 已加 va_space_handle 透传"
- **AND** any cross-reference to GPU ABI compatibility notes the new field

### Requirement: H-1 archive is git-tracked

The directory `openspec/changes/archive/2026-06-17-fix-gpu-pushbuffer-va-space-validation/` MUST be tracked by git, so that `git log` and `git clone` operations preserve the historical change artifacts (proposal.md, design.md, tasks.md, specs/, README.md, .openspec.yaml).

#### Scenario: Files visible in git

- **WHEN** user runs `git ls-tree HEAD openspec/changes/archive/2026-06-17-fix-gpu-pushbuffer-va-space-validation/`
- **THEN** the output includes all 5 historical files (README.md, design.md, proposal.md, tasks.md, specs/gpu-pushbuffer-validation/spec.md) plus .openspec.yaml

#### Scenario: Git history preserved

- **WHEN** a future contributor runs `git log --all -- openspec/changes/archive/2026-06-17-fix-gpu-pushbuffer-va-space-validation/`
- **THEN** the original H-1 commit `0272970` and subsequent archive-related commits are visible

#### Scenario: File content unchanged

- **WHEN** the archive is brought under git tracking
- **THEN** the file contents on filesystem are byte-identical to what was committed during H-1 archive
- **AND** no openspec re-processing or re-templating is triggered

### Requirement: SSOT v0.1.4 changelog entry

The `docs/02_architecture/post-refactor-architecture.md` "变更记录" table MUST include a new v0.1.4 row that references the `h1-pushbuffer-validation-closeout` change, summarizing the two closure items (TaskRunner sync + archive git tracking).

#### Scenario: Changelog entry present

- **WHEN** the closeout change is complete
- **THEN** the 变更记录 table contains a row with version "v0.1.4", date "2026-06-17", and a description that names both the TaskRunner sync and the archive git tracking recovery

#### Scenario: v0.1.3 entry untouched

- **WHEN** the v0.1.4 entry is added
- **THEN** the v0.1.3 entry (added in commit `61b67db`) is byte-identical to its post-`61b67db` state

### Requirement: All gates remain green

After applying the closeout change, the following gates MUST pass with no regression: `make -j4` (100%), `ctest` (34/34), and `bash tools/docs-audit.sh --strict` (36/36). If TaskRunner's own CI is configured in the submodule, it MUST also pass.

#### Scenario: Main repo gates green

- **WHEN** the closeout commit is applied
- **THEN** `make -j4` reports "Built target gpu_driver_plugin" with no errors
- **AND** `ctest` reports 34/34 tests passed
- **AND** `tools/docs-audit.sh --strict` reports 36/36 checks passed

#### Scenario: TaskRunner build green

- **WHEN** the TaskRunner submodule changes are applied
- **THEN** building TaskRunner standalone (e.g., via its own build system) succeeds
- **AND** any existing TaskRunner tests still pass

### Requirement: Closeout change is itself archived

The `h1-pushbuffer-validation-closeout` change, once all its requirements are met, MUST be archived via `openspec archive h1-pushbuffer-validation-closeout`, producing `openspec/changes/archive/2026-06-17-h1-pushbuffer-validation-closeout/`.

#### Scenario: Successful archive

- **WHEN** user runs `openspec archive h1-pushbuffer-validation-closeout` after all requirements are met
- **THEN** the change directory is moved to `openspec/changes/archive/2026-06-17-h1-pushbuffer-validation-closeout/`
- **AND** `openspec list` reports "No active changes"
- **AND** the archive directory is git-tracked (per the same standard as Requirement "H-1 archive is git-tracked")
