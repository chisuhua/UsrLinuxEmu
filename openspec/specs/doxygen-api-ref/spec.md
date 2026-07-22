# Doxygen API Reference

**Purpose**: Generate C/C++ API reference documentation from source code using Doxygen.

## Requirements

### Requirement: Doxygen configuration exists
The project SHALL provide a Doxyfile at `docs/Doxyfile` configured to generate HTML API reference.

#### Scenario: Doxyfile is present
- **WHEN** user runs `make doxygen` or `cmake --build . --target doxygen`
- **THEN** Doxygen shall process configured source directories and produce HTML output

### Requirement: CMake integration
The build system SHALL expose an optional `doxygen` target that does not block default build.

#### Scenario: Doxygen found
- **WHEN** Doxygen is installed and CMake is configured
- **THEN** `make doxygen` target shall be available

#### Scenario: Doxygen not found
- **WHEN** Doxygen is not installed
- **THEN** CMake configuration shall succeed with a status message, no error

### Requirement: Output is gitignored
Generated API documentation SHALL be excluded from version control.

#### Scenario: docs/api/ in .gitignore
- **WHEN** developer runs `git status` after `make doxygen`
- **THEN** `docs/api/` directory shall not appear as untracked

### Requirement: docs-audit validates Doxygen
The `tools/docs-audit.sh` script SHALL verify Doxygen generates without errors.

#### Scenario: docs-audit with Doxygen
- **WHEN** `tools/docs-audit.sh` is run
- **THEN** it shall invoke Doxygen and check exit code is zero
