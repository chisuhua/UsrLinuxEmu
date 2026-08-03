# add-abi-dispatch-consistency-test Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use skill_use("execute") to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 新增 CI 门禁 — ABI 头 (38 个 GPU_IOCTL_*) 与运行时派发表严格同步检测 (防 PR #20 模式漂移)

**Architecture:** 新增 Catch2 standalone test_ioctl_abi_dispatch_consistency，硬编码 `ABI_IOCTL_REQUESTS` 数组包含全部 38 个 `_IOWR(...)` 编码值，构造 GpgpuDevice（无需 HAL/plugin）取 `getIoctlTablePtr()` 静态表条目，排序后逐项 REQUIRE 等值。Section 用例：缺失值、多余值、重复检测。

**Tech Stack:** Catch2 · `std::array<uint32_t, 38>` · GpgpuDevice dispatch table · `_IOWR` 编码常量取自 `gpu_ioctl.h`

**Priority:** P2  **Wave:** 1

**Generated:** 2026-08-03T02:52:07+00:00 (by guide-ship Phase 1 plan-generation)

---

## File Structure

### Tests

| File | Responsibility |
|------|----------------|
| `tests/test_ioctl_abi_dispatch_consistency_standalone.cpp` | 新增 Catch2 standalone — ABI <-> dispatch table 双向一致性 CI 门禁 |
| `tests/CMakeLists.txt` | add_standalone_test 注册 |

---

## Tasks (3)

### Task 1: 硬编码 ABI 常量数组 + 测试骨架

- Create: `tests/test_ioctl_abi_dispatch_consistency_standalone.cpp`
- Modify: `tests/CMakeLists.txt`
- Test: `tests/test_ioctl_abi_dispatch_consistency_standalone.cpp`

- [x] **Step 1: Verify starting state**
```bash
cd build && make test_ioctl_abi_dispatch_consistency_standalone -j4
Expected: build FAIL (target not defined)
```

- [x] **Step 2: Implementation**
  - tests/test_ioctl_abi_dispatch_consistency_standalone.cpp:
  -   - `constexpr std::array<uint32_t, 38> kAbiIoctlRequests = {{ ... }};`
  -     注释行引用每个值对应的 ABI 名（如 `{0x01, "PUSHBUFFER_SUBMIT_BATCH"}`）
  -     数值取自 `plugins/gpu_driver/shared/gpu_ioctl.h` 中 `#define GPU_IOCTL_*` 的 `_IOWR(...)` 编码结果
  -   - 测试结构:
  -     构造 `GpgpuDevice` (无需 HAL/plugin, 直接静态调用 `getIoctlTablePtr()`)
  -     收集表中所有 `request` 值 → 排序
  -     与 `kAbiIoctlRequests` 排序后逐项 `REQUIRE((both == other))`
  -   - `SECTION("missing values reported")`: 缺失时输出具体值
  -   - `SECTION("extra values reported")`: 多余时输出具体值
  -   - `SECTION("duplicates detected")`: 检测重复 entry
  - Tests/CMakeLists.txt: `add_standalone_test(test_ioctl_abi_dispatch_consistency_standalone)`

- [x] **Step 3: Run tests to verify they pass**
```bash
cd build && make test_ioctl_abi_dispatch_consistency_standalone -j4 && ./bin/test_ioctl_abi_dispatch_consistency_standalone
Expected: 全部 4 SECTION 全 PASS（前提：kNumIoctls=38 + 派发表与 ABI 一致——见 wire-mfw 任务）
```

- [x] **Step 4: Commit**
```bash
git add tests/test_ioctl_abi_dispatch_consistency_standalone.cpp tests/CMakeLists.txt
git commit -m "test(gpu): add ABI dispatch consistency CI gate (PR #20 drift prevention)"
```

### Task 2: CRITICAL wire-mfw 依赖：派发表必须先修复


- [x] **Step 1: Verify starting state**
```bash
# 此 change 必须 ship 在 wire-mfw 合并后才有意义:
cd build && ctest -R test_ioctl_abi_dispatch_consistency_standalone
Expected: 当前 (kNumIoctls=36) 失败 — 派发表 36 ≠ ABI 38
```

- [x] **Step 2: Implementation**
  - **依赖关系**：本 change 是 wave 1，必须等 wire-mfu (wave 0) 合并后再运行
  - 若 wire-mfu 未合并，本测试必然失败（kNumIoctls 36 vs ABI 38 不等）
  - 执行顺序：wire-mwu PR 合并 → 本 change 可执行

- [x] **Step 3: Run tests to verify they pass**
```bash
# 在 wire-muw 合并后:
cd build && ctest -R test_ioctl_abi_dispatch_consistency_standalone --output-on-failure
Expected: 0 failed
```

- [x] **Step 4: Commit**
```bash
(
n
o
 
c
o
m
m
i
t
 
—
 
d
e
p
e
n
d
e
n
c
y
 
c
h
e
c
k
 
o
n
l
y
)
```

### Task 3: 全量 ctest 回归 + 文档更新


- [x] **Step 1: Verify starting state**
```bash
N/A
```

- [x] **Step 2: Implementation**
  - `cd build && ctest --output-on-failure` 验证 0 regression
  - 在 AGENTS.md 中记录本测试作为 CI gate 入口

- [x] **Step 3: Run tests to verify they pass**
```bash
cd build && ctest --output-on-failure
Expected: 0 failed
```

- [x] **Step 4: Commit**
```bash
(
n
o
 
c
o
m
m
i
t
 
—
 
c
o
v
e
r
e
d
 
b
y
 
T
a
s
k
 
1
)
```

---

## Self-Review Checklist

- [ ] Each task has explicit `Files:` lines (Create/Modify/Test)
- [ ] Each task has TDD 5-step structure (verify starting state, implement, run tests pass, commit)
- [ ] Each task includes a commit step with conventional message
- [ ] No 'TBD' / 'TODO' placeholders
- [ ] Type/method names consistent across tasks
- [ ] `cd build && ctest --output-on-failure` final regression — 0 failed

