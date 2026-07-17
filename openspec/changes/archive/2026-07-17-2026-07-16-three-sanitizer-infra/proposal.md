# Change: three-sanitizer-infra

> **状态**: 🔄 IN-PROGRESS
> **优先级**: 🟡 P3
> **创建**: 2026-07-16
> **来源**: C-12 E.2.3 deferred（openspec/changes/archive/2026-07-16-2026-08-15-stage1-4-kfd-multi-file-integration/tasks.md §E.2.3）
> **依赖**: C-12 stage1-4-kfd-multi-file-integration（已归档 2026-07-16，commit `670e244`）
> **前置**: TSan infra 已就绪（`ENABLE_TSAN` opt-in，per C-12 B.1.10.7 + commit `75683ca` B.4.6 TSan hardening）
> **工作目录**: `openspec/changes/2026-07-16-three-sanitizer-infra/`

## Why

C-12 KFD multi-file integration 在 2026-07-16 归档时（81% 原子任务完成）留下了 E.2.3 三 sanitizer 的 deferred 工作：

- ✅ **TSan** (ThreadSanitizer) — 已有 CMake infra（`ENABLE_TSAN` option），但实际 `add_compile_options` 在 `tests/CMakeLists.txt:327`，**仅作用于 tests/ 子目录中定义于此点之后的目标**；kernel/plugins/drivers 等目标未受 TSan instrument，coverage 不完整。现有覆盖：`test_kfd_threading_standalone`、`test_kfd_events_tsan_standalone`、`test_hal_thread_safety_standalone`。
- ❌ **ASan** (AddressSanitizer) — 无 CMake infra；当前 default build 无 sanitizer active。
- ❌ **UBSan** (UndefinedBehaviorSanitizer) — 无 CMake infra；同上。

ASan 捕获堆/栈/全局缓冲区溢出、use-after-free、double-free、stack-buffer-overflow。UBSan 捕获未定义行为：有符号整型溢出、空指针解引用、类型混淆、对齐违规、移位越界、可空对象解引用等。

KFD module（`plugins/gpu_driver/drv/kfd/` 21 个 C 文件）+ HAL bridge（`plugins/gpu_driver/hal/`）+ sim layer（`plugins/gpu_driver/sim/page_fault_handler.cpp`、`page_migration.cpp`、`sim_event.c`）是底层 C/C++ 代码，对内存安全与 UB 高度敏感。当前 default build 不暴露这类 bug，必须借助 sanitizer 才能在 CI 上系统性拦截。

Metis 审稿发现现有提案存在如下阻塞项：

1. sanitizer CMake 设置放置过晚 —— `add_compile_options/add_link_options` 写在 `tests/CMakeLists.txt`，无法传播到 kernel/plugins/drivers/libgpu_core；现行 TSan infra 也受此限制。
2. 不应再重复 `tests/CMakeLists.txt` 的 TSan 模式（"类似 tests/CMakeLists.txt 中 ENABLE_TSAN 的实现"）—— 该方向会再次漏掉非测试目标。
3. sanitizer 构建目录未隔离 —— `plugins/gpu_driver/CMakeLists.txt:43-48` 的 POST_BUILD `copy` 把 `gpu_driver_plugin.so` 复制到**共享**路径 `${PROJECT_SOURCE_DIR}/plugins/plugin_gpu_driver.so`；任何 sanitizer 构建都会覆盖前一次的插件，再跑 ctest 就会加载失配的插件 lib。
4. UBSan 默认非致命（继续执行并打印）—— 失败可能被掩盖；需要明确致命策略。
5. ASan 泄漏检测策略未定义 —— LSan 与 ASan 默认联动在某些 libc/release 模式下会放大测试失败噪声；需要明确的默认关闭 + 显式 out-of-scope 声明。
6. TSan 与 ASan/UBSan 兼容性未定义 —— 这三类 sanitizer 的 runtime 互斥，必须显式拒绝组合，否则会发生链接冲突。
7. bug 修复范围无界 —— 现提案罗列若干"预计 bug"且无回归测试/分裂约束。
8. CI 仅"可选建议" —— 未作为 required deliverable。
9. acceptance 命令不可观测 —— "0 sanitizer-error" 缺少明确的 log/退出码扫描步骤。
10. 文档任务范围过窄 —— 漏掉 `docs/03-development/debugging.md`、`docs/05-advanced/kfd-portability-boundary.md`，未涉及 `build.sh`。

## What Changes

本 change 仅交付 sanitizer **基础设施 + 受限 bug 分流 + 文档 + CI**，不在本 change 中进行架构性修复。

### 1. ASan + UBSan CMake Infra（根 `CMakeLists.txt`）

**放置规则（替代旧"类似 tests/CMakeLists.txt"表述）**: 在根 `CMakeLists.txt` 中、**所有 `add_subdirectory()` 调用之前**（即先于 `add_subdirectory(src)`、`add_subdirectory(drivers)`、`add_subdirectory(plugins)`、`add_subdirectory(tests)`、`add_subdirectory(tools/cli)`、`add_subdirectory(libgpu_core)` 六处）声明 sanitizer 选项并施加 directory-level compile/link 设置。`add_compile_options()`/`add_link_options()` 以 directory scope 向下传播，从而覆盖 kernel（必须保持 SHARED，per Issue #11）、所有 plugin、drivers、tests、tools、libgpu_core。

**编译器支持 + 配置期拦截**:

```cmake
# 位于 add_subdirectory() 调用之前
option(ENABLE_ASAN  "Enable AddressSanitizer (GCC/Clang)" OFF)
option(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer (GCC/Clang)" OFF)
option(ENABLE_TSAN  "Enable ThreadSanitizer (Clang only)" OFF)

# 编译器支持：ASan/UBSan 要求 C 与 C++ 均为 GCC 或 Clang 家族；TSan 要求 C 与 C++ 均为 Clang 家族
if(ENABLE_ASAN AND NOT (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang" AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang"))
    message(FATAL_ERROR "ENABLE_ASAN requires GCC or Clang for both C and C++; got C=${CMAKE_C_COMPILER_ID} CXX=${CMAKE_CXX_COMPILER_ID}")
endif()
if(ENABLE_UBSAN AND NOT (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang" AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang"))
    message(FATAL_ERROR "ENABLE_UBSAN requires GCC or Clang for both C and C++; got C=${CMAKE_C_COMPILER_ID} CXX=${CMAKE_CXX_COMPILER_ID}")
endif()
if(ENABLE_TSAN AND NOT (CMAKE_C_COMPILER_ID MATCHES "Clang" AND CMAKE_CXX_COMPILER_ID MATCHES "Clang"))
    message(FATAL_ERROR "ENABLE_TSAN requires Clang family for both C and C++; got C=${CMAKE_C_COMPILER_ID} CXX=${CMAKE_CXX_COMPILER_ID}")
endif()

# 互斥：TSan runtime 与 ASan/UBSan runtime 不能共载（链接冲突）
if(ENABLE_TSAN AND (ENABLE_ASAN OR ENABLE_UBSAN))
    message(FATAL_ERROR "TSan is mutually exclusive with ASan/UBSan: pick exactly one family.")
endif()

if(ENABLE_ASAN)
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=address)
    message(STATUS "[ASan]  AddressSanitizer enabled globally (ENABLE_ASAN=ON)")
endif()
if(ENABLE_UBSAN)
    add_compile_options(-fsanitize=undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=undefined)
    message(STATUS "[UBSan] UndefinedBehaviorSanitizer enabled globally, fatal mode (ENABLE_UBSAN=ON)")
endif()
if(ENABLE_TSAN)
    add_compile_options(-fsanitize=thread -fno-omit-frame-pointer -g -O1)
    add_link_options(-fsanitize=thread)
    message(STATUS "[TSan]  ThreadSanitizer enabled globally (ENABLE_TSAN=ON)")
endif()
```

**关键决策**:

- **ASan/UBSan 不强制全局 `-O1`** —— 与项目现行 CMake 风格一致（根 CMakeLists.txt 不设 `-O` flag，依赖 `-DCMAKE_BUILD_TYPE=Debug` 默认 `-O0 -g`）。本 change 的 ASan/UBSan 构建均使用 `CMAKE_BUILD_TYPE=Debug`。**TSan 保留现有 tests/CMakeLists.txt 中的 `-O1` 基线**，迁移到根目录时不得降低其竞态检测优化级别。若 ASan/UBSan 因 `-O0` 触发额外噪声或漏报，可在后续 follow-up change 引入条件 `-O1`，但本 change 不引入。
- **保留 `-fno-omit-frame-pointer`** —— 保证 sanitizer stack trace 可见。
- **TSan 行为变更** —— 现有 `tests/CMakeLists.txt:327-331` 的 `if(ENABLE_TSAN)` 块在那点之上定义的目标不受 TSan instrument；移入根 CMakeLists.txt 后会扩展覆盖到 kernel/plugins/drivers/libgpu_core。这是受控的"补全 coverage" 变更，必须在 acceptance 中显式验证现有 TSan tests 仍 PASS。
- **`tests/CMakeLists.txt` 中残留的 `if(ENABLE_TSAN)` 块** —— 删除（避免双重 `add_compile_options(-fsanitize=thread ...)`），保留 `test_kfd_queue_standalone` 的 `if(ENABLE_TSAN) target_link_libraries(... -fsanitize=thread)` 链接兜底（因该测试直接拼 .c 文件）。

### 2. Plugin Artifact Isolation（最小范围方案）

**问题**: `plugins/gpu_driver/CMakeLists.txt:43-48` 的 POST_BUILD `cmake -E copy` 把每次 `gpu_driver_plugin` 构建产物复制到共享路径 `${PROJECT_SOURCE_DIR}/plugins/plugin_gpu_driver.so`。任意 sanitizer 构建均覆盖前一次的插件，且 `tests/CMakeLists.txt` 的 `set_tests_properties(... WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})` + `ModuleLoader::load_plugins("plugins")`（相对路径）使所有测试加载同一 `plugins/plugin_gpu_driver.so`，因此跨 build 的 ctest 会加载失配的插件 lib，产生假阳性/假阴性。

**本 change 不改 POST_BUILD copy 本身**（最小范围）。改为**在每个 sanitizer build 的 ctest 运行前**重新触发该 build dir 的 `gpu_driver_plugin` 目标，从而把对应 build 的插件放回共享路径，并在 ctest 前用 ELF 符号扫描断言"即将被加载的插件"与"构建类型"匹配。**严禁并发运行多个 sanitizer 构建目录的 ctest**（共享插件路径，必然交叉覆盖）。

**具体序列**（每个 sanitizer build q）:

```bash
# 在项目根目录运行；构建目录 build-<config>
CM_CONFIG=asan   # 或 ubsan / tsan / asan-ubsan / default
# (1) 构建 + 强制重新生成该 build 的插件（POST_BUILD 自动 copy 到共享路径）
cmake --build "build-${CM_CONFIG}" --target gpu_driver_plugin
# (2) 验证共享路径下的插件携带对应 sanitizer 符号
#     ASan:   nm  plugins/plugin_gpu_driver.so | grep -c '__asan_init'  须为 >=1
#     UBSan:  nm  plugins/plugin_gpu_driver.so | grep -c '__ubsan_handle' 须为 >=1
#     TSan:   nm  plugins/plugin_gpu_driver.so | grep -c '__tsan_init'   须为 >=1
#     default:nm  plugins/plugin_gpu_driver.so | grep -c '__asan_init'    须为 0
# (3) 跑 ctest  ——  必须从项目根目录（tests/CMakeLists.txt 已设 WORKING_DIRECTORY=PROJECT_SOURCE_DIR）
ctest --test-dir "build-${CM_CONFIG}" --output-on-failure
```

`scripts/stage-plugin.sh` 是本 change 的 staging helper：它接收 build 目录，重新构建对应的 `gpu_driver_plugin`，校验共享插件路径中的 sanitizer 符号与 build 配置相符，并在失配时以非零退出。它不改变 `ModuleLoader` 的加载路径契约。

### 3. Bug Triage（受控分流）

**调查候选**（仅作风险面盘点，**不预设存在 bug**；实际以 sanitizer 运行为准）:

- `plugins/gpu_driver/drv/kfd/kfd_process.c` — 链表遍历 + 释放模式（list_del/cleanup 路径），ASan/UBSan 候查区。
- `plugins/gpu_driver/drv/kfd_sim_bridge.cpp` — 用户态指针拷贝路径（`apertures_ptr`、`memcpy`）。UBSan 对齐与 ASan buffer 边界候查区。
- `plugins/gpu_driver/sim/page_migration.cpp` — `sim_pm_migrate_to_system` 等，尺寸/指针边界候查区。
- `plugins/gpu_driver/sim/sim_event.c` — `atomic_fetch_add` 顺序、cleanup 释放后访问的候查区。
- `plugins/gpu_driver/sim/mem_pool.cpp`、`page_fault_handler.cpp` — 内存池越界候查区。

**修复纳入本 change 的条件**（逐条判定，登录 tasks.md C.2 / D.2）:

- (a) 修复**最小**且**局部** —— 不跨模块边界、不变更 HAL/IOCTL/VFS 等契约、不动 ADR-020 不动 libgpu_core、不动 ADR-023 HAL 接口。
- (b) 修复涉及源码改动 **≤50 行**（每个独立 bug）。
- (c) 提供 Catch2 回归测试锁定缺陷（按现有 `tests/CMakeLists.txt` 模式：`add_standalone_test` / `add_sim_test` / `add_catch_sim_test`）。
- (d) 不在 archive/、不在 external/TaskRunner/、不在已 deprecated 路径下。

超出任一条件即记录入 `openspec/changes/sanitizer-triage-YYYY-MM-DD/proposal.md`（**本 change 不创建该 follow-up，仅记录 follow-up 名称、文件列表与简要原因作为本阶段交付摘要**；YYYY-MM-DD 为实施当天的日期）。**严禁** 在本 change 内进行跨模块重构。

**停止条件**: sanitizer 报告告警一旦超出局部修复预算，对应项立即转为 follow-up，不在本 change 反复迭代。

### 4. CI Integration（required deliverables，不可选）

向 `.github/workflows/cmake-multi-platform.yml` 新增三个 sanitizer jobs（与既有 `build` / `docs-audit` 平级）；job 名称作为 required status checks 写入仓库分支保护规则，PR 不通过则阻塞 merge:

| Job | 编译器 | Build Type | 开关 | 说明 |
|-----|--------|-----------|------|------|
| `sanitizer-asan` | GCC | Debug | `ENABLE_ASAN=ON` | 全量 ctest |
| `sanitizer-ubsan` | GCC | Debug | `ENABLE_UBSAN=ON` | 全量 ctest |
| `sanitizer-tsan` | Clang | Debug | `ENABLE_TSAN=ON` | 全量 ctest（新增 TSan CI 覆盖） |

约束:

- 每个 sanitizer job 使用独立 build 目录（`build-asan` / `build-ubsan` / `build-tsan`），互不并发。
- 失败时使用 `actions/upload-artifact@v4` 上传构建日志与 `ctest --output-on-failure` 输出（含 sanitizer stack trace）。
- **资源受控**: 单 job 内 `make -j2` + `ctest -j2`（或串行）；不在 matrix 内并发同一 build 目录。
- 排除 perf 测试（`--exclude-regex "perf"`）—— `USR_LINUX_EMU_PERF_TESTS=ON` 仅夜测，本 change 不影响该约定。
- 三个 sanitizer job 与 `docs-audit` job 同为 required（PR 必检）。

### 5. Documentation + build.sh

- **`docs/04-building/ci-cd.md`** —— 替换现有 §"启用 AddressSanitizer"（行 205-213 用 `-DCMAKE_CXX_FLAGS` 的示例）与 §"示例：内存安全检查"（行 382-422）为基于 `ENABLE_ASAN` / `ENABLE_UBSAN` / `ENABLE_TSAN` 的 canonical 用法 + 互斥/插件隔离说明 + 三个 CI job 描述。
- **`docs/03-development/debugging.md`** —— §"AddressSanitizer (ASan)"（行 222-259）+ §"ThreadSanitizer (TSan)"（行 362-377）替换示例为 `-DENABLE_ASAN=ON` / `-DENABLE_UBSAN=ON` / `-DENABLE_TSAN=ON` 选项用法 + runtime options 默认值表 + 互斥规则。
- **`docs/05-advanced/kfd-portability-boundary.md`** —— 现状 v1.3；在 §"3 Tier-2 PoC 实际超界" 旁追加 v1.4 标注: "ASan/UBSan infra added (change 2026-07-16-three-sanitizer-infra); TSan coverage 扩展至 kernel/plugins/drivers 全目标；本 change 不引入 KFD 边界契约变更"。
- **`build.sh`** —— 现有 `build()` 函数（行 36-61）仅转发 `CMAKE_BUILD_TYPE=Debug`；新增 sanitizer 选项转发机制，使 `SANITIZER=asan ./build.sh` 等价于 `cmake -DENABLE_ASAN=ON -DCMAKE_BUILD_TYPE=Debug -B build-asan`，同理 `ubsan` / `tsan` / `asan-ubsan` / `default`。无 `SANITIZER` 时行为不变。

### 6. Out of Scope

- **LeakSanitizer (LSan) enablement** —— ASan 默认 `detect_leaks=0`；LSan 启用作为独立 follow-up。
- **全局 `-O1`** —— 本 change 用 `-O0`（Debug）；`-O1` 调优作为独立 follow-up（仅在观察到噪声显著时）。
- **per-build 插件目录隔离**（修改 `ModuleLoader` 加载路径契约）—— 本 change 最小范围保持单点共享路径 + 序列约束。
- **跨模块/契约/架构 bug 修复** —— 一律转为 follow-up OpenSpec change。
- **不修改** `src/`、`plugins/gpu_driver/drv|hal|sim/`、`libgpu_core/`、`drivers/`、`include/`、`external/TaskRunner/` 等业务源码中的契约 —— 仅允许在 C.x / D.x task 下做最小局部缺陷修复（带回归测试）。

## Acceptance

每个验收步骤均为**可观测命令**，从项目根目录 `/workspace/project/UsrLinuxEmu` 运行；**禁止并发运行不同 sanitizer 构建目录的 ctest**。基线声明: **当前实施前实测值以 `ctest --test-dir build-default -N` 为准（104 个 CTest tests，`Total Tests: 104`）** + **docs-audit 43/43 PASS**。实现阶段允许新增测试，但不允许 regression；下方命令中的 `<baseline>` 指实施前实测的 CTest 数量（104）。

- [ ] **A.x（CMake Infra）**
  - [ ] `cmake -DENABLE_ASAN=ON -DCMAKE_BUILD_TYPE=Debug -B build-asan -S . && cmake --build build-asan -j4` → 退出码 0
  - [ ] `cmake -DENABLE_UBSAN=ON -DCMAKE_BUILD_TYPE=Debug -B build-ubsan -S . && cmake --build build-ubsan -j4` → 退出码 0
  - [ ] `cmake -DENABLE_ASAN=ON -DENABLE_UBSAN=ON -DCMAKE_BUILD_TYPE=Debug -B build-asan-ubsan -S . && cmake --build build-asan-ubsan -j4` → 退出码 0
  - [ ] `cmake -DENABLE_TSAN=ON -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Debug -B build-tsan -S . && cmake --build build-tsan -j4` → 退出码 0
  - [ ] `cmake -DENABLE_TSAN=ON -DENABLE_ASAN=ON -B build-bad-S -S . 2>&1 | grep -q 'mutually exclusive'` → grep 退出码 0，且 CMake 配置命令本身必须非零（FATAL_ERROR 触发）
   - [ ] `cmake -DENABLE_TSAN=ON -DENABLE_UBSAN=ON -B build-bad-S2 -S . 2>&1 | grep -q 'mutually exclusive'` → grep 退出码 0，且 CMake 配置命令本身必须非零（FATAL_ERROR 触发）
  - [ ] 配置日志含 `[ASan] AddressSanitizer enabled globally` / `[UBSan] ... fatal mode` / `[TSan] ... enabled globally` 行（按对应 build）

- [ ] **B.x（Plugin Isolation）每个 sanitizer build 在 ctest 之前完成**
  - [ ] 对 `build-asan`: `cmake --build build-asan --target gpu_driver_plugin` 后 `nm plugins/plugin_gpu_driver.so | grep -c '__asan_init'` 须 ≥1
  - [ ] 对 `build-ubsan`: 上述命令后 `nm plugins/plugin_gpu_driver.so | grep -c '__ubsan_handle'` 须 ≥1
  - [ ] 对 `build-tsan`: 上述命令后 `nm plugins/plugin_gpu_driver.so | grep -c '__tsan_init'` 须 ≥1
  - [ ] 对 `build-default`: 上述命令后 `nm plugins/plugin_gpu_driver.so | grep -cE '__asan_init|__ubsan_handle|__tsan_init'` 须 =0（default 插件三类 sanitizer 符号均不存在）

- [ ] **C.x（ASan Run）**
  - [ ] `bash -o pipefail -c 'scripts/stage-plugin.sh build-asan && ASAN_OPTIONS=detect_leaks=0:halt_on_error=1:abort_on_error=1:print_stacktrace=1 ctest --test-dir build-asan --output-on-failure 2>&1 | tee build-asan/asan-ctest.log'` 从根目录运行 → 退出码 0
  - [ ] `grep -ciE 'ERROR: AddressSanitizer|runtime error:' build-asan/asan-ctest.log` 须为 0
  - [ ] ctest 用例数 ≥ `<baseline>` 且全部 PASS

- [ ] **D.x（UBSan Run）**
  - [ ] `bash -o pipefail -c 'scripts/stage-plugin.sh build-ubsan && UBSAN_OPTIONS=print_stacktrace=1 ctest --test-dir build-ubsan --output-on-failure 2>&1 | tee build-ubsan/ubsan-ctest.log'` → 退出码 0
  - [ ] `grep -ciE 'runtime error:' build-ubsan/ubsan-ctest.log` 须为 0
  - [ ] ctest 用例数 ≥ `<baseline>` 且全部 PASS

- [ ] **E.x（TSan Regression）** —— 现有 TSan tests 仍 PASS；`test_hal_thread_safety_standalone` 的 `hal_mock` SECTION（预期 race per ADR-060）从本项 zero-race 检查中排除
  - [ ] `bash -o pipefail -c 'scripts/stage-plugin.sh build-tsan && TSAN_OPTIONS=report_signal_unsafe=0 ctest --test-dir build-tsan --output-on-failure 2>&1 | tee build-tsan/tsan-ctest.log'` → 退出码 0
  - [ ] `grep -ciE 'WARNING: ThreadSanitizer: data race' build-tsan/tsan-ctest.log` 须为 0（注：若 `test_hal_thread_safety_standalone` 产生 race，该项会失败 → 应在运行前通过 `ctest --test-dir build-tsan -E test_hal_thread_safety_standalone` 排除该测试后再执行 TSan gate 检查，或将 hal_mock race 从「zero-race」判定中排除）
  - [ ] ctest 用例数 ≥ `<baseline>`（排除 hal_thread_safety 时为 `<baseline>-1`）且全部 PASS

- [ ] **F.x（CI Integration）** —— 三个 sanitizer job 在 `.github/workflows/cmake-multi-platform.yml` 中存在，命名 `sanitizer-asan` / `sanitizer-ubsan` / `sanitizer-tsan`；`on.pull_request` / `on.push` 触发；失败 `actions/upload-artifact@v4` 上传 `build-*` 与 ctest 日志。本地校验: `yq '.jobs | keys' .github/workflows/cmake-multi-platform.yml` 输出含上述三名（或用 grep 等价校验）。

- [ ] **G.x（Documentation + build.sh）**
  - [ ] `docs/04-building/ci-cd.md` 含 `ENABLE_ASAN` / `ENABLE_UBSAN` / `ENABLE_TSAN`（互斥 + plugin isolation 序列）
  - [ ] `docs/03-development/debugging.md` §ASan/§TSan 引用上述新 options
  - [ ] `docs/05-advanced/kfd-portability-boundary.md` 含 "v1.4 ASan/UBSan infra added"
  - [ ] `SANITIZER=asan ./build.sh test` 等价于 `cmake -DENABLE_ASAN=ON ...`（手测或 docs 描述命令复制即可执行）

- [ ] **H.x（Docs Audit + Final Default Regression）**
  - [ ] `bash tools/docs-audit.sh --strict` → 退出码 0，输出 `Passed: ≥43`（不得少于当前 43 项）
  - [ ] **最终 default 回归**（在跑过所有 sanitizer build 后）: `cmake --build build-default --target gpu_driver_plugin && nm plugins/plugin_gpu_driver.so | grep -cE '__asan_init|__ubsan_handle|__tsan_init'` 须 =0；再跑 `ctest --test-dir build-default --output-on-failure` → 退出码 0，`ctest --test-dir build-default -N | grep "Total Tests"` 数量 ≥ `<baseline>` 且全部 PASS
  - [ ] `openspec validate 2026-07-16-three-sanitizer-infra`（若当前 CLI 版本不接受日期前缀，则使用 `openspec list --json` 确认 change 后人工逐项对照本提案；不得使用不存在的缩短名称）

## Key Design Decisions

| 决策 | 选定方案 | 备注 |
|------|---------|------|
| 选项放置位置 | 根 CMakeLists.txt，所有 `add_subdirectory()` 之前 | 覆盖 kernel/plugins/drivers/tests/tools/libgpu_core |
| 编译器支持 | GCC/Clang（ASan/UBSan）；TSan 仅 Clang | 配置期 `FATAL_ERROR` |
| 互斥规则 | TSan ⨯ (ASan ∨ UBSan) 配置期 FATAL_ERROR | runtime 不能共存 |
| ASan+UBSan | 可共存（`ENABLE_ASAN=ON ENABLE_UBSAN=ON`） | 两 runtime 可链接 |
| ASan runtime default | `detect_leaks=0:halt_on_error=1:abort_on_error=1:print_stacktrace=1` | LSan 启用 out-of-scope |
| UBSan 致命策略 | 编译期 `-fno-sanitize-recover=all` + runtime `UBSAN_OPTIONS=print_stacktrace=1` | 选用编译期 fatal 为主，runtime 仅加 stack trace |
| `-fno-omit-frame-pointer` | 全部 sanitizer 启用 | stack trace 可读 |
| `-O1` | ASan/UBSan **不引入**；统一 `CMAKE_BUILD_TYPE=Debug`；TSan 保留既有 `-O1` | 避免回归现有 TSan 检测基线 |
| 构建目录 | 独立 `build-default`/`build-asan`/`build-ubsan`/`build-asan-ubsan`/`build-tsan` | 禁跨 config 复用 |
| Plugin 隔离方案 | 共享路径不变 + 每 ctest 前 `cmake --build build-q --target gpu_driver_plugin` + ELF 符号扫描 + 禁并发 | 不改 ModuleLoader 契约 |
| bug 修复范围 | 局部、≤50 行、附回归测试；否则转 follow-up | tasks C.2 / D.2 / C.3 / D.3 |
| CI sanitizer jobs | **required**，三个，串行/限并发，失败上传 logs | 不可选 |
| docs 范围 | ci-cd.md / debugging.md / kfd-portability-boundary.md + build.sh | 见上 §5 |

## 关联 ADR / SSOT

- ADR-035（governance policy）—— change 边界纪律
- ADR-020（libgpu_core zero-modify constraint）—— 本 change 不动 libgpu_core 源码；sanitizer 仅作 instrument
- ADR-023（HAL interface contract）—— 本 change 不动 HAL 契约
- ADR-036（3 区分架构）—— sanitizer infra 同时覆盖 ① 模拟 / ② 驱动 / ③ 仿真 三区
- Issue #11（kernel SHARED）—— 保持 `add_library(kernel SHARED ...)`，sanitizer 不改 linkage

## 关联 SSOT

- `openspec/changes/archive/2026-07-16-2026-08-15-stage1-4-kfd-multi-file-integration/tasks.md` §E.2.3
- `docs/04-building/ci-cd.md`（CI/CD workflow）
- `docs/03-development/debugging.md`（sanitizer 调试章节）
- `docs/05-advanced/kfd-portability-boundary.md`（KFD 边界 SSOT）
- `.github/workflows/cmake-multi-platform.yml`（CI 工作流）
- `CMakeLists.txt`（根构建配置）
- `plugins/gpu_driver/CMakeLists.txt:43-48`（POST_BUILD copy 约束点）
- `tests/CMakeLists.txt:325-337`（既有 TSan `if(ENABLE_TSAN)` 块，本 change 删除）
- `build.sh`（构建脚本，本 change 增 sanitizer 转发）