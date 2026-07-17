# sanitizer-infrastructure Specification

## Purpose
TBD - created by archiving change 2026-07-16-three-sanitizer-infra. Update Purpose after archive.
## Requirements
### Requirement: Root-level sanitizer configuration

根 `CMakeLists.txt` MUST 在相关 `add_subdirectory()` 调用之前提供 `ENABLE_ASAN`、`ENABLE_UBSAN` 和 `ENABLE_TSAN` 选项，并将对应编译和链接选项传播到所有下游目标。ASan 与 UBSan MAY 同时启用；TSan MUST 与 ASan/UBSan 互斥。ASan/UBSan MUST 要求 C 与 C++ 编译器属于 GCC/Clang 家族，TSan MUST 要求两者均为 Clang 家族；不满足时 CMake 配置 MUST 以非零状态失败。

#### Scenario: Configure an ASan build

- **WHEN** 用户执行 `cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DENABLE_ASAN=ON`
- **THEN** CMake 配置成功
- **AND** 后续 kernel、plugin、driver、test、tool 和 `libgpu_core` 目标获得 ASan 编译与链接选项
- **AND** 配置输出明确表示 ASan 已启用

#### Scenario: Configure an ASan plus UBSan build

- **WHEN** 用户同时设置 `-DENABLE_ASAN=ON -DENABLE_UBSAN=ON`
- **THEN** CMake 配置与构建成功
- **AND** ASan 与 UBSan runtime 均被链接
- **AND** UBSan 使用不可恢复模式
- **NOTE**: 本 change 仅要求 ASan+UBSan 构建成功；组合模式完整 staging + ctest 验收不属于本 change scope。

#### Scenario: Reject incompatible TSan combinations

- **WHEN** 用户同时设置 `-DENABLE_TSAN=ON` 与 `-DENABLE_ASAN=ON` 或 `-DENABLE_UBSAN=ON`
- **THEN** CMake 配置以非零状态失败
- **AND** 输出包含 TSan 与 ASan/UBSan 互斥说明

#### Scenario: Reject unsupported sanitizer compilers

- **WHEN** ASan/UBSan 使用非 GCC/Clang 家族编译器，或 TSan 的 C/C++ 编译器不是 Clang 家族
- **THEN** CMake 配置以非零状态失败
- **AND** 输出同时标明 C 与 C++ 编译器 ID

### Requirement: Isolated sanitizer build and plugin staging

每种 sanitizer 配置 MUST 使用独立的 out-of-source build directory：`build-default`、`build-asan`、`build-ubsan`、`build-asan-ubsan` 和 `build-tsan`。由于当前 plugin loader 从源码目录的 `plugins/plugin_gpu_driver.so` 加载插件，系统 MUST 在每次 ctest 前从对应 build directory 重新构建 `gpu_driver_plugin`，并通过 `scripts/stage-plugin.sh` 验证共享插件包含匹配的 sanitizer 符号。不同 sanitizer 构建的 ctest MUST NOT 并发执行。

#### Scenario: Stage an ASan plugin before tests

- **WHEN** 用户从项目根目录执行 `scripts/stage-plugin.sh build-asan`
- **THEN** 脚本重新构建 `build-asan` 的 `gpu_driver_plugin`
- **AND** 共享路径 `plugins/plugin_gpu_driver.so` 被替换为该构建的插件
- **AND** 脚本确认插件包含 ASan runtime 符号，否则以非零状态失败
- **AND** default build staging 后 MUST 确认插件三类 sanitizer 符号（`__asan_init`、`__ubsan_handle`、`__tsan_init`）均不存在

#### Scenario: Prevent cross-configuration plugin use

- **WHEN** 用户准备运行另一个 sanitizer build 的 ctest
- **THEN** 必须先执行对应 build directory 的 staging 命令
- **AND** 不得与另一个 sanitizer build 的 staging 或 ctest 并发
- **AND** default 回归前必须重新 staging `build-default` 的 plugin

### Requirement: Fatal sanitizer test verification and bounded triage

ASan 测试 MUST 使用 `detect_leaks=0`、`halt_on_error=1`、`abort_on_error=1` 和 `print_stacktrace=1`。本 change 不启用 LeakSanitizer。UBSan MUST 使用编译期 `-fno-sanitize-recover=all`，运行时保留 stack trace。每个 sanitizer ctest 运行 MUST 保存日志并检查 sanitizer 错误模式。sanitizer 暴露的修复只有在局部、单个缺陷源码改动不超过约 50 行、附带 Catch2 回归测试且不改变跨模块契约时才可留在本 change；其他问题 MUST 记录为 follow-up。

TSan 的 zero-race gate MUST 排除 `test_hal_thread_safety_standalone`（`hal_mock` 预期 data race per ADR-060）；该测试使用 `ctest -E test_hal_thread_safety_standalone` 从 zero-race 检查中排除后，其余全量测试 MUST 无 `WARNING: ThreadSanitizer: data race`。

#### Scenario: ASan test run succeeds without reports

- **WHEN** 用户 staging `build-asan` 后运行全量 `ctest --test-dir build-asan --output-on-failure`
- **THEN** ctest 退出码为 0
- **AND** 保存的日志不包含 `ERROR: AddressSanitizer` 或 `runtime error:`
- **AND** 测试数量不低于实施前基线

#### Scenario: UBSan report is not recoverable

- **WHEN** UBSan 测试触发 undefined behavior
- **THEN** 测试进程以非零状态终止
- **AND** 日志包含可定位的 stack trace
- **AND** 该问题按照局部修复预算决定留在本 change 或转 follow-up

### Requirement: Required CI and documentation coverage

CI MUST 提供 GCC Debug ASan、GCC Debug UBSan 和 Clang Debug TSan 三个独立 job，限制构建/测试并发资源，并在失败时上传构建和 sanitizer 日志。三个 job 名称 MUST 被配置为分支保护 required status checks。构建和调试文档 MUST 使用新的 CMake options、runtime defaults、互斥规则和 plugin staging 流程；`build.sh` MUST 支持 sanitizer 选择，同时保持未设置 sanitizer 时的既有行为。

#### Scenario: Required sanitizer CI jobs run on a pull request

- **WHEN** pull request 触发 `.github/workflows/cmake-multi-platform.yml`
- **THEN** `sanitizer-asan`、`sanitizer-ubsan` 和 `sanitizer-tsan` 均被调度
- **AND** 各 job 在独立 build directory 中构建并运行 ctest
- **AND** 失败时上传对应日志 artifact
- **AND** 任一 job 失败都会阻止 required check 通过
- **NOTE**: CI runner 假定 Linux (ubuntu-latest/ubuntu-22.04)；`nm` / `strip` / Clang toolchain 需 CI 显式安装；`ctest --test-dir` 要求 CMake ≥3.14（项目最低版本承诺），实际 CI 环境需验证该命令可用

#### Scenario: Existing build command remains compatible

- **WHEN** 用户未设置 `SANITIZER` 并执行既有 `./build.sh` 或 `./build.sh test`
- **THEN** 脚本继续使用既有 `build/` 目录和默认 sanitizer-off 配置
- **AND** 设置 `SANITIZER=asan`、`ubsan`、`tsan` 或 `asan-ubsan` 时切换到对应独立 build directory 和 CMake option

