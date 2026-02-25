# Copilot Instructions for UsrLinuxEmu

## Project Overview

UsrLinuxEmu is a **user-space Linux kernel emulation environment** designed for device driver development and testing. It allows developers to write, test, and debug device drivers (especially GPGPU drivers) without root privileges or kernel compilation.

## Architecture

The project uses a layered architecture:

- **`include/kernel/`** – Kernel framework headers (VFS, plugin manager, device abstractions)
- **`include/linux_compat/`** – Linux kernel API compatibility layer for user-space (`compat.h` is the unified include)
- **`src/kernel/`** – Kernel framework implementation
- **`drivers/`** – Device driver implementations (GPU, serial, memory, PCIe)
- **`simulator/`** – Hardware simulators (GPU simulator)
- **`tests/`** – Unit and integration tests (Google Test)
- **`tools/cli/`** – CLI utilities
- **`plugins/`** – Loadable device plugins

### Key Design Notes

- The Linux `struct class` is named `struct device_class` in the compat layer because `class` is a reserved C++ keyword.
- The `swap` macro in the compat layer is only defined in C mode (not C++).
- `dev_t` and `loff_t` come from `sys/types.h`.

## Build System

```bash
# Build
mkdir -p build && cd build && cmake .. && make -j4

# Run tests
cd build && make test

# Run a specific test binary
./build/bin/<testname>_standalone
```

- Requires CMake ≥ 3.14, GCC/Clang with C++17 support.
- The `build/` directory is excluded via `.gitignore`.

## Coding Conventions

- **Language**: C++17
- **Style**: Follow Google C++ Style Guide
- **Naming**:
  - Classes: `PascalCase` (e.g., `GpgpuDevice`)
  - Functions & variables: `snake_case` (e.g., `allocate_memory`, `buffer_size`)
  - Private member variables: `snake_case_` with trailing underscore (e.g., `buffer_size_`)
  - Constants & macros: `UPPER_SNAKE_CASE` (e.g., `MAX_BUFFER_SIZE`, `GPGPU_ALLOC_MEM`)
- **Namespaces**: Use `usr_linux_emu` namespace
- **Headers**: Use `#pragma once` for header guards
- **Error handling**: Return Linux-style error codes (e.g., `-EINVAL`, `-ENOMEM`); use `0` for success
- **Resource management**: Prefer RAII, `std::unique_ptr`, and `std::shared_ptr` over raw pointers
- **Comments**: Doxygen-style (`/** @brief ... @param ... @return ... */`) for public APIs; `//` for inline explanations

## Testing

- Tests use **Google Test** framework.
- Each new feature should have unit tests with ≥ 80% coverage target.
- Bug fixes must include regression tests.
- Test binaries are built in `build/bin/`.

## Commit Messages

Follow [Conventional Commits](https://www.conventionalcommits.org/):

```
<type>(<scope>): <subject>
```

Types: `feat`, `fix`, `docs`, `style`, `refactor`, `perf`, `test`, `build`, `ci`, `chore`

Examples:
- `feat(gpu): add memory pool support`
- `fix(vfs): fix device lookup race condition`
- `docs(api): update device API documentation`
