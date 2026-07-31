## ADDED Requirements

### Requirement: ASan + UBSan Build is Green

The system SHALL pass all Catch2 tests under AddressSanitizer + UndefinedBehaviorSanitizer configuration.

#### Scenario: ASan+UBSan build succeeds
- **WHEN** `SANITIZER=asan-ubsan ./build.sh` is run (creates `build-asan/`)
- **THEN** the build succeeds

#### Scenario: All tests green under ASan+UBSan
- **WHEN** `cd build-asan && ctest --output-on-failure` is run
- **THEN** 0 test failures AND 0 sanitizer reports (memory leaks, use-after-free, UB)

#### Scenario: Preemption test green under ASan+UBSan
- **WHEN** `./build-asan/bin/test_preemption_standalone` is run
- **THEN** the test passes with no sanitizer reports

#### Scenario: Timeline semaphore test green under ASan+UBSan
- **WHEN** `./build-asan/bin/test_timeline_semaphore_standalone` is run
- **THEN** the test passes with no sanitizer reports

### Requirement: TSan Build is Green

The system SHALL pass all Catch2 tests under ThreadSanitizer configuration (requires Clang).

#### Scenario: TSan build succeeds
- **WHEN** `SANITIZER=tsan ./build.sh` is run (creates `build-tsan/`, requires Clang per AGENTS.md)
- **THEN** the build succeeds

#### Scenario: All tests green under TSan
- **WHEN** `cd build-tsan && ctest --output-on-failure` is run
- **THEN** 0 test failures AND 0 data race reports

#### Scenario: Preemption test green under TSan
- **WHEN** `./build-tsan/bin/test_preemption_standalone` is run
- **THEN** the test passes with no data race reports

#### Scenario: Concurrent preempt test green under TSan
- **WHEN** `./build-tsan/bin/test_concurrent_preempt` is run
- **THEN** the test passes with no data race reports (critical validation of concurrent spec)

#### Scenario: Timeline semaphore test green under TSan
- **WHEN** `./build-tsan/bin/test_timeline_semaphore_standalone` is run
- **THEN** the test passes with no data race reports

### Requirement: Baseline Regression

The system SHALL pass all tests in default (no sanitizer) configuration to establish a non-sanitizer-clean baseline.

#### Scenario: Default build tests green
- **WHEN** `cd build && ctest --output-on-failure` is run
- **THEN** 0 test failures (no regression vs pre-sanitizer baseline)

### Requirement: Sanitizer CI Integration

The CI pipeline SHALL run sanitizer builds to prevent regressions.

#### Scenario: ASan+UBSan job in CI
- **WHEN** `.github/workflows/cmake-multi-platform.yml` is inspected
- **THEN** a job exists that runs `SANITIZER=asan-ubsan ./build.sh test`

#### Scenario: TSan job in CI
- **WHEN** `.github/workflows/cmake-multi-platform.yml` is inspected
- **THEN** a job exists that runs `SANITIZER=tsan ./build.sh test`

#### Scenario: CI sanitizer job passes
- **WHEN** a PR is opened
- **THEN** all sanitizer jobs complete with exit code 0

### Requirement: Sanitizer Status Documentation

The system SHALL document the sanitizer-clean baseline state.

#### Scenario: Sanitizer status file exists
- **WHEN** `docs/05-advanced/sanitizer-status.md` is inspected
- **THEN** it documents the latest sanitizer-green commit SHA + last verified date

### Requirement: No Sanitizer Suppression Policy

The system SHALL NOT use sanitizer suppression files (`.tsan_suppressions`, `.ubsan_suppressions`, `.asan_suppressions`) to silence reports. Every sanitizer report SHALL be either fixed, or explicitly documented as a known false positive with justification.

> **Rationale**: Suppression files are a maintenance burden and can mask real bugs long after they are introduced. The project priority is correctness; false positives are cheaper than masked bugs.

#### Scenario: No suppression file checked in
- **WHEN** the repository is searched for `*.suppressions` files at the project root
- **THEN** no such file exists (or, if present, it is empty and the corresponding sanitizer is configured to ignore empty suppressions)

#### Scenario: New sanitizer report triggers fix
- **WHEN** a CI run reports a new sanitizer finding (not previously documented)
- **THEN** the finding is treated as a bug
- **AND** either:
  1. The code is fixed and the fix is committed in a separate PR, OR
  2. The finding is documented as a known false positive in `docs/05-advanced/sanitizer-status.md` with:
    - Specific function/module name
    - Reason for false positive classification
    - Link to upstream issue (if applicable)
    - Reviewer sign-off date

#### Scenario: Sanitizer status file lists known false positives
- **WHEN** `docs/05-advanced/sanitizer-status.md` is inspected
- **THEN** it contains a "Known False Positives" section (or empty section with explicit "none" note)
- **AND** each entry has the fields specified in the previous scenario