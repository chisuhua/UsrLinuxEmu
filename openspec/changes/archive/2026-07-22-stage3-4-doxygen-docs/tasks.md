## 1. Doxygen Configuration

- [x] 1.1 Create `docs/Doxyfile` with project info (name, version, output directory)
- [x] 1.2 Configure Doxygen input: `include/kernel/`, `include/linux_compat/`, `plugins/gpu_driver/drv/`, `plugins/gpu_driver/hal/`, `plugins/gpu_driver/shared/`, `src/kernel/`
- [x] 1.3 Set EXTRACT_PRIVATE = NO, GENERATE_LATEX = NO, WARN_AS_ERROR = NO
- [x] 1.4 Run Doxygen locally and verify HTML output at `docs/api/`

## 2. CMake Integration

- [x] 2.1 Add `find_package(Doxygen)` to root `CMakeLists.txt` (optional, QUIET)
- [x] 2.2 Add `add_custom_target(doxygen)` with COMMAND and WORKING_DIRECTORY
- [x] 2.3 Add `docs/api/` to `.gitignore`
- [x] 2.4 Verify `cmake .. && make doxygen` succeeds; verify `cmake ..` succeeds without Doxygen installed

## 3. docs-audit Integration

- [x] 3.1 Extend `tools/docs-audit.sh` to invoke Doxygen and check exit code
- [x] 3.2 Run `tools/docs-audit.sh --strict` and confirm PASS

## 4. Quickstart Guide Polish

- [x] 4.1 Review `docs/01-quickstart/installation.md` — verify package lists are current for Ubuntu 20.04+ / Debian 11+
- [x] 4.2 Review `docs/01-quickstart/building.md` — ensure instructions match current `build.sh` and CMake flow
- [x] 4.3 Review `docs/01-quickstart/first-example.md` — verify all code snippets compile and run against HEAD
- [x] 4.4 Time the end-to-end flow (clone → build → first test) and document actual duration

## 5. plan-handoff Update

- [x] 5.1 Update `.rddf/state/.plan-handoff.json` to reflect stage3-4-doxygen-docs as active change