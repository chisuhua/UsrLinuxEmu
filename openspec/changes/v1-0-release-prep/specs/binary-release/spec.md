## ADDED Requirements

### Requirement: Release workflow exists
The project SHALL have a GitHub Actions workflow triggered by version tags.

#### Scenario: Tag push triggers build
- **WHEN** a git tag matching `v*.*.*` is pushed
- **THEN** the release workflow shall start building binaries

### Requirement: Binary artifacts are uploaded
The release workflow SHALL upload build artifacts to the GitHub Release.

#### Scenario: Release contains binaries
- **WHEN** a release is published
- **THEN** the GitHub Release shall contain `cli`, `libkernel.so`, and `plugin_*.so` files

### Requirement: Build is Release mode
The release build SHALL use `-DCMAKE_BUILD_TYPE=Release`.

#### Scenario: Release build configured
- **WHEN** the release workflow runs cmake
- **THEN** `-DCMAKE_BUILD_TYPE=Release` shall be passed

### Requirement: Optional Dockerfile
The project MAY provide a `Dockerfile` for CI and quick-start environments.

#### Scenario: Dockerfile exists
- **WHEN** user checks project root
- **THEN** a `Dockerfile` may exist (optional, not required)