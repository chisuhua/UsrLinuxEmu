# Change: three-sanitizer-infra

> **状态**: 📋 PROPOSED
> **优先级**: 🟡 P3
> **创建**: 2026-07-16
> **来源**: C-12 E.2.3 deferred（openspec/changes/archive/2026-07-16-2026-08-15-stage1-4-kfd-multi-file-integration/tasks.md §E.2.3）
> **依赖**: C-12 stage1-4-kfd-multi-file-integration（已归档 2026-07-16，commit `670e244`）
> **前置**: TSan infra 已就绪（`ENABLE_TSAN` opt-in，per C-12 B.1.10.7 + commit `75683ca` B.4.6 TSan hardening）
> **工作目录**: `openspec/changes/2026-07-16-three-sanitizer-infra/`

## Why

C-12 KFD multi-file integration 在 2026-07-16 归档时（81% 原子任务完成）留下了 E.2.3 三 sanitizer 的 deferred 工作：

- ✅ **TSan** (ThreadSanitizer) — 已有 CMake infra（`ENABLE_TSAN` option）+ 现有测试覆盖（`test_kfd_threading_standalone` 4 TEST_CASE concurrent producers + drain + atomic counters + `test_kfd_events_tsan_standalone`）
- ❌ **ASan** (AddressSanitizer) — 无 CMake infra；当前 default build 无 sanitizer active
- ❌ **UBSan** (UndefinedBehaviorSanitizer) — 无 CMake infra；同上

ASan 捕获堆/栈/全局缓冲区溢出、use-after-free、double-free。
UBSan 捕获未定义行为：有符号溢出、空指针解引用、类型混淆、对齐违规、移位越界。

KFD module (21 文件 drv/kfd/) + HAL bridge + sim layer (page_fault_handler.cpp + page_migration.cpp + sim_event.c) 是底层 C/C++ 代码，对内存安全和 UB 高度敏感。

## What Changes

### 1. 新增 ASan + UBSan CMake infra

在 `CMakeLists.txt` 顶层添加（与 `ENABLE_TSAN` 平级）：

```cmake
option(ENABLE_ASAN "Enable AddressSanitizer for memory error detection (GCC/Clang)" OFF)
if(ENABLE_ASAN)
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer -g -O1)
    add_link_options(-fsanitize=address)
    message(STATUS "[ASan] AddressSanitizer enabled globally (ENABLE_ASAN=ON)")
endif()

option(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer (GCC/Clang)" OFF)
if(ENABLE_UBSAN)
    add_compile_options(-fsanitize=undefined -fno-omit-frame-pointer -g -O1)
    add_link_options(-fsanitize=undefined)
    message(STATUS "[UBSan] UndefinedBehaviorSanitizer enabled globally (ENABLE_UBSAN=ON)")
endif()
```

类似 `tests/CMakeLists.txt` 中 `ENABLE_TSAN` 的实现。

### 2. 修复 ASan/UBSan 暴露的真 bug（预计）

经验上 ASan 首次跑 KFD + sim layer 会暴露：

- `kfd_process.c` 中 `kfd_process_exit()` 遍历链表 + free — 可能 double-free（list_del 后 entry 还持有指针）
- `kfd_sim_bridge.cpp` 中 `memcpy` 到 `apertures_ptr` 用户态指针 — 当前 stub `0` 指针触发 UBSan alignment（如果 buffer 8-byte 对齐）
- `sim_page_migration.cpp` 中 `sim_pm_migrate_to_system` — potential heap-buffer-overflow on size check
- `sim_event.c` 中 `atomic_fetch_add` race — TSan 已知不触发，ASan 可能暴露 use-after-free on cleanup

### 3. CI 集成建议

- Stage 3.1 多平台 CI matrix 可加 sanitizer build（每 sanitizer 一个 matrix entry）
- 每次 PR 自动跑 ASan + UBSan（轻量，~10-30s overhead）

### 4. 文档更新

- `docs/04-building/ci-cd.md` §Sanitizer builds 章节新增 ASan/UBSan 说明
- `docs/05-advanced/kfd-portability-boundary.md` v1.4 更新（标注 ASan/UBSan infra added）

## Acceptance

- [ ] `cmake -DENABLE_ASAN=ON` 构建 + `make -j4` 0 errors
- [ ] `cmake -DENABLE_UBSAN=ON` 构建 + `make -j4` 0 errors
- [ ] `cmake -DENABLE_TSAN=ON` 已有 infra 仍 PASS（无 regression）
- [ ] ASan 跑全 ctest 0 sanitizer-error（修复暴露的 bug 后）
- [ ] UBSan 跑全 ctest 0 sanitizer-error（修复暴露的 bug 后）
- [ ] 三 sanitizer 可组合使用（`ENABLE_ASAN=ON ENABLE_UBSAN=ON` 共 build）
- [ ] docs-audit 43/43 PASS（文档更新后）
- [ ] 104/104 ctest PASS（无 regression，default build）

## 测试方法

```bash
# Default build (no sanitizer)
cd build && cmake -DCMAKE_BUILD_TYPE=Debug .. && make -j4 && ctest -j4

# ASan build
cd build && cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_ASAN=ON .. \
  && make -j4 && ctest -j4 --output-on-failure
# Expect: 0 sanitizer errors

# UBSan build
cd build && cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_UBSAN=ON .. \
  && make -j4 && ctest -j4 --output-on-failure
# Expect: 0 sanitizer errors

# TSan build (existing infra)
cd build && cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_TSAN=ON .. \
  && make -j4 && ctest -j4 --output-on-failure
# Expect: 0 data race warnings (already passing)
```

## 关联 ADR

- ADR-035 (governance policy)
- ADR-020 (libgpu_core zero-modify constraint)
- ADR-023 (HAL interface contract)

## 关联 SSOT

- `openspec/changes/archive/2026-07-16-2026-08-15-stage1-4-kfd-multi-file-integration/tasks.md` §E.2.3
- `docs/04-building/ci-cd.md` (CI/CD workflow)