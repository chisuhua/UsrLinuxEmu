# Quickstart User Guide

**Purpose**: Provide a first-time developer experience that enables building and running the first GPU example within 15 minutes.

## Requirements

### Requirement: Quickstart is completable in 15 minutes
A first-time developer SHALL be able to build and run the first GPU example within 15 minutes (excluding `git clone` time).

#### Scenario: Developer follows quickstart
- **WHEN** a developer follows `docs/01-quickstart/` instructions from build to running `test_gpu_ioctl_standalone`
- **THEN** the process shall complete in under 15 minutes

### Requirement: First example is runnable
The `docs/01-quickstart/first-example.md` SHALL contain a complete, copy-pasteable example that runs against the current codebase.

#### Scenario: First example compiles and runs
- **WHEN** developer copies the example code from `first-example.md` and compiles it
- **THEN** the binary shall link against `libkernel.so` and run without modification

### Requirement: Prerequisites are documented
The quickstart SHALL list all required system packages and tools.

#### Scenario: Prerequisites listed
- **WHEN** developer reads `docs/01-quickstart/installation.md`
- **THEN** all required packages for Ubuntu 20.04+ and Debian 11+ shall be listed