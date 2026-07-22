## ADDED Requirements

### Requirement: CMake project() VERSION is SSOT

The project SHALL define its version number in `CMakeLists.txt` via `project(user_kernel_emu VERSION <semver>)`.

#### Scenario: Version defined in CMake
- **WHEN** `cmake` is configured
- **THEN** `PROJECT_VERSION` / `CMAKE_PROJECT_VERSION` shall be available and equal to `1.0.0`

### Requirement: Version consistency

All version references in the project SHALL be consistent with the CMake `project()` VERSION.

#### Scenario: README badge matches CMake version
- **WHEN** any user reads README.md
- **THEN** the version badge (`[![Version]`) and the footer version text shall match `project()` VERSION

#### Scenario: docs-audit checks version consistency
- **WHEN** `tools/docs-audit.sh --strict` runs
- **THEN** it SHALL verify that README badge version equals the CMake `project()` VERSION