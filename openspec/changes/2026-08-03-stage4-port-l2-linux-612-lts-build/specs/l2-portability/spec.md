# L2 Linux 6.12 LTS Portability — Stage 4 Acceptance Gate

## ADDED Requirements

### Requirement: L2 build harness script

A bash script `tools/ci/l2-portability/build-drv-against-linux-6.12.sh` SHALL provide reproducible Linux 6.12 LTS build of `plugins/gpu_driver/drv/` as a standalone kernel module.

#### Scenario: L2 build succeeds

- **GIVEN** Linux 6.12 LTS kernel source is fetched (or cached at `${HOME}/.cache/linux-6.12.tar.xz`)
- **AND** `plugins/gpu_driver/drv/` exists in the repo
- **WHEN** `./tools/ci/l2-portability/build-drv-against-linux-6.12.sh` runs
- **THEN** the script SHALL build the drv/ contents in the kernel tree as a module
- **THEN** the script SHALL output `errors=0` for a clean baseline (Stage 4 close)
- **AND** the script SHALL exit with status 0

#### Scenario: L2 build fails with recoverable errors

- **GIVEN** a PR introduces an incompatible header reference in drv/
- **WHEN** the L2 build runs
- **THEN** the script SHALL output non-zero error count
- **THEN** the script SHALL exit with non-zero status (block PR merge)
- **THEN** the build log SHALL be uploaded as CI artifact for debugging

### Requirement: CI gating workflow

A GitHub Actions workflow `.github/workflows/l2-portability.yml` SHALL trigger L2 build on PRs that touch drv/ or include/ paths.

#### Scenario: PR triggers L2 CI job

- **GIVEN** PR changes `plugins/gpu_driver/drv/**/*.cpp` or `include/**/*.h`
- **WHEN** PR is opened or synchronized
- **THEN** GitHub Actions SHALL schedule the L2 job
- **AND** the L2 job SHALL run on ubuntu-22.04 + ubuntu-20.04 matrix
- **AND** the L2 job SHALL test kernel versions 6.6 and 6.12 LTS (matrix)

#### Scenario: L2 CI blocking merge

- **GIVEN** the L2 build script exits with non-zero status
- **WHEN** PR CI runs complete
- **THEN** the required status check SHALL fail
- **AND** GitHub SHALL block PR merge (per repo branch protection rules)

### Requirement: L2 baseline documentation

A baseline document `docs/04-building/portability-l2-baseline-2026-08.md` SHALL record the first successful L2 baseline run.

#### Scenario: First baseline recorded

- **GIVEN** L2 build runs successfully on the initial repo state
- **WHEN** the baseline document is created
- **THEN** it SHALL record: kernel version, host OS, `errors=0`, `warnings=N`, date
- **AND** it SHALL categorize warnings (if N>0) by fixable class (include path vs linux_compat extension)

### Requirement: ADR-072 L2 mapping

The L2 implementation SHALL align with ADR-072 §L2 rules (zero-modify constraint for `plugins/gpu_driver/drv/`).

#### Scenario: L2 zero-modify validation

- **GIVEN** a PR changes drv/ source code
- **WHEN** L2 build runs
- **THEN** only the changed drv/ files SHALL be rebuilt
- **THEN** fixable warnings via include path adjustment SHALL be allowed (per ADR-035 §Rule 5.3)
- **THEN** semantic drv/ changes SHALL require linux_compat/ extension as a sibling change

### Requirement: Stage 4 整体验收 §② gate

Upon L2 PASS, the Stage 4 acceptance gate §② (`[] ② 驱动代码使用 ioremap/readl/writel 后，仅 #include 调整即可在 Linux 6.12 LTS 编译（可移植性）`) SHALL be flip-checked.

#### Scenario: L2 PASS flips Stage 4 gate §②

- **GIVEN** L2 build runs successfully (errors=0) on Linux 6.12 LTS
- **WHEN** the closeout review accepts the L2 baseline
- **THEN** `docs/roadmap/stage-4-bar-ioremap.md` §"Stage 4 整体验收" §② checkbox SHALL be flipped to [x]
- **AND** `openspec/changes/INDEX.md` SHALL record the L2 milestone

#### Scenario: L2 isolated from drv/ semantic changes

- **GIVEN** drv/ code intentionally changes semantics (e.g., new ioctl handler logic)
- **WHEN** L2 build runs
- **THEN** the L2 build artifact SHALL rebuild affected object files
- **THEN** include-path-only fixes SHALL NOT count as drv/ semantic changes
- **AND** changes that require kernel API extensions SHALL be tracked as separate `linux_compat/` extension proposals
