# add-sim-singleton-init-idempotency-guards Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use skill_use("execute") to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** `GpuVramStore::init(size_t)` 与 `DmaCoherentPool::init()` 在第二次调用时立即返回，不再 double-mmap。VramStore 不同 size 的第二次调用返回 false + stderr WARN（不静默成功）。两类的现有 `bool initialized` 成员**直接使用**（不新增 `initialized_`）。

**Architecture:** 修改 `plugins/gpu_driver/sim/vram_store.cpp` 的 `GpuVramStore::init(size_t)`：在 mmap 之前加 `if (initialized) { if (vram_size == size_bytes) return true; else { WARN, return false; } }`。同样修改 `dma_coherent_pool.cpp` 的 `DmaCoherentPool::init()`（无 size 参数，简单 `if (initialized) return true;`）。两个 header 不动（成员已存在：`vram_store.h:21` / `dma_coherent_pool.h:16`）。

**Tech Stack:** C++17 · 现有 sim singleton 框架 · Catch2 测试

**Priority:** P2  **Wave:** 1  **Risk:** LOW（防御性变更；现有 ctest 不依赖 init 重复调用；F4 plugin guard 是前置保险）

**Generated:** 2026-09-06 (post a2701cd, Oracle deep-dive revisions applied)

**Refs:** Oracle audit `ses_f8b8c32e5ffeGgFiPdo4hE0jiu` · Oracle deep-dive `ses_f8f0f9467ffe3w1M4uCHZTcFmS` · openspec/changes/add-sim-singleton-init-idempotency-guards/

---

## File Structure

### Code

| File | Responsibility |
|------|----------------|
| `plugins/gpu_driver/sim/vram_store.cpp` | `GpuVramStore::init(size_t)` — 加 idempotency + size-mismatch 守卫 |
| `plugins/gpu_driver/sim/dma_coherent_pool.cpp` | `DmaCoherentPool::init()` — 加 idempotency 守卫 |

### Tests (new)

| File | Responsibility |
|------|----------------|
| `tests/test_sim_singleton_init_idempotent_standalone.cpp` | 新 Catch2 standalone — 同 size 二次 init 返回 true / 不同 size 返回 false / stderr WARN |

---

## Tasks (2)

### Task 1: Preflight + 读 sim 类当前 init 形状

- [ ] **Step 1: Verify starting state**
```bash
cd /workspace/project/UsrLinuxEmu && grep -n 'initialized' plugins/gpu_driver/sim/vram_store.h plugins/gpu_driver/sim/vram_store.cpp plugins/gpu_driver/sim/dma_coherent_pool.h plugins/gpu_driver/sim/dma_coherent_pool.cpp
# Expected: 每个文件至少一行 initialized 出现 — 确认现有成员存在
```

- [ ] **Step 2: Read source**
- 读 `vram_store.cpp` `init(size_t)` 函数（应有 mmap、`pool_backing`、`initialized = true`）
- 记录：
  - 参数名（`size_bytes` 或 `bytes`）
  - 现有 size 存储的成员名（`vram_size` 或 `size_`）
  - `initialized` 成员位置（line 数）
  - `initialized = true` 成功路径位置（line 数）
- 读 `dma_coherent_pool.cpp` `init()` 函数
- 记录：无 size 参数；`initialized = true` 在哪个 line

- [ ] **Step 3: Confirm no existing idempotency guard**
```bash
cd /workspace/project/UsrLinuxEmu && grep -B1 -A2 '::init' plugins/gpu_driver/sim/vram_store.cpp plugins/gpu_driver/sim/dma_coherent_pool.cpp | grep -A2 '::init'
# Expected: 当前 init 函数体前几行无 `if (initialized)` 守卫
```

### Task 2: 实现 idempotency 守卫 + regression test

- [ ] **Step 1: Verify starting state**
```bash
cd /workspace/project/UsrLinuxEmu/build && ctest --output-on-failure 2>&1 | tail -3
# Expected: 157/157 PASS（基线）
```

- [ ] **Step 2: Implement VramStore::init guard**
- 修改 `plugins/gpu_driver/sim/vram_store.cpp`：
  - 定位 `GpuVramStore::init(size_t)` 函数体（参数名按 preflight 实际确认）
  - 在**任何 mmap 之前**插入：

```cpp
  if (initialized) {
    if (vram_size == size_bytes) {
      return true;
    }
    std::cerr << "[GpuVramStore] WARN: init(" << size_bytes
              << ") ignored — existing init was " << vram_size << "\n";
    return false;
  }
```

  - `vram_size` 是 preflight 确认的 size 存储成员名
  - `size_bytes` 是 `init()` 参数名（如叫 `bytes` 则替换）

- [ ] **Step 3: Implement DmaCoherentPool::init guard**
- 修改 `plugins/gpu_driver/sim/dma_coherent_pool.cpp`：
  - 定位 `DmaCoherentPool::init()` 函数体
  - 在任何 mmap 之前插入：

```cpp
  if (initialized) return true;
```

  - 不需要 size 检查（init 无参数）

- [ ] **Step 4: 写 regression test**
- 创建 `tests/test_sim_singleton_init_idempotent_standalone.cpp`：

```cpp
#include <catch_amalgamated.hpp>
#include "sim/vram_store.h"
#include "sim/dma_coherent_pool.h"
#include <iostream>
#include <sstream>

TEST_CASE("GpuVramStore::init idempotent on same size", "[sim][singleton]") {
  // Use a per-test local instance — g_vram_store is process-wide singleton
  GpuVramStore store;
  REQUIRE(store.init(256) == true);
  REQUIRE(store.init(256) == true);  // second call MUST return true without double-mmap
}

TEST_CASE("GpuVramStore::init rejects different size with WARN", "[sim][singleton]") {
  GpuVramStore store;
  // Note: WARN goes to stderr — capture if needed via rdbuf redirect (see test_gpu_plugin_init_idempotent)
  REQUIRE(store.init(256) == true);
  REQUIRE(store.init(512) == false);  // different size MUST return false
}

TEST_CASE("DmaCoherentPool::init idempotent", "[sim][singleton]") {
  DmaCoherentPool pool;
  REQUIRE(pool.init() == true);
  REQUIRE(pool.init() == true);  // second call MUST return true without double-mmap
}
```

  - **若** `GpuVramStore` / `DmaCoherentPool` 构造不可调用（private ctor），改用 `g_vram_store` / `g_dma_pool` 单例 + 一个独立 Catch2 binary
  - **若** init 不返回 `bool`（void + 异常 / 状态成员），调整断言

- [ ] **Step 5: 注册 test 到 CMakeLists**
- 修改 `tests/CMakeLists.txt`：
  - 在 `CATCH2_TESTS` 列表（约 line 220）添加 `test_sim_singleton_init_idempotent_standalone`

- [ ] **Step 6: Run tests**
```bash
cd /workspace/project/UsrLinuxEmu/build && cmake --build . -j4 --target sim test_sim_singleton_init_idempotent_standalone 2>&1 | tail -10
# Expected: build clean
./bin/test_sim_singleton_init_idempotent_standalone
# Expected: 3 SECTION 全 PASS
ctest -R test_sim_singleton_init_idempotent --output-on-failure
# Expected: PASS
```

- [ ] **Step 7: 全量 blast radius**
```bash
cd /workspace/project/UsrLinuxEmu/build && ctest --output-on-failure 2>&1 | tail -3
# Expected: 158/158 PASS (新 test 1 个 + 原 157 个)
```

- [ ] **Step 8: Commit**
```bash
cd /workspace/project/UsrLinuxEmu && git add plugins/gpu_driver/sim/vram_store.cpp plugins/gpu_driver/sim/dma_coherent_pool.cpp tests/test_sim_singleton_init_idempotent_standalone.cpp tests/CMakeLists.txt
git commit -m "fix(sim): make VramStore::init and DmaCoherentPool::init idempotent

Defensive change. Both classes ALREADY own a \`bool initialized\` member
(vram_store.h:21, dma_coherent_pool.h:16); this fix adds the early-return
check at the top of each init() body.

GpuVramStore::init(size_t):
  - If initialized AND requested size matches stored size → return true (no mmap)
  - If initialized AND requested size differs → return false + stderr WARN
    (silent success would mask a caller bug)

DmaCoherentPool::init() (parameterless):
  - If initialized → return true (no mmap)

Refs:
  Oracle audit:    ses_f8b8c32e5ffeGgFiPdo4hE0jiu
  Oracle deep-dive: ses_f8f0f9467ffe3w1M4uCHZTcFmS
  gpu_driver plugin guard (precedent, defense in depth): commit 31dd5e1"
```

- [ ] **Step 9: Archive**
```bash
cd /workspace/project/UsrLinuxEmu
# tasks.md §2.1-2.8 / §3 / §4 标 [x]
openspec validate add-sim-singleton-init-idempotency-guards --strict
# Expected: Change 'add-sim-singleton-init-idempotency-guards' is valid
openspec archive add-sim-singleton-init-idempotency-guards --yes
git add openspec/ && git commit -m "chore(openspec): archive add-sim-singleton-init-idempotency-guards"
```

---

## Out of scope (handled elsewhere)

- vram_store.h / dma_coherent_pool.h — 成员已存在，不动
- 现有 ctest 不变（防御性变更；blast radius 通过全量 ctest 覆盖）
- Plugin 端 guard（g_plugin_initialized）保留