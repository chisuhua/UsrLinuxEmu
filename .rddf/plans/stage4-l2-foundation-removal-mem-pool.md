# stage4-l2-foundation-removal-mem-pool Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use skill_use("execute") to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remove L2 violation `#include "sim/mem-pool.h"` from drv/ files and replace direct sim calls with HAL fn-ptr wrappers. Per ADR-072 §Decision 4 revised + ADR-023 Decision 4 (opaque handle abstraction).

**Architecture:** drv/ side calls `hal_*` inline wrappers; HAL lambdas (`hal_user.cpp`) route to real sim/ implementations. `hal_mock.cpp` keeps no-op mocks.

**Tech Stack:** C99-compatible C (HAL interface), C++17 lambdas (sim impls), Catch2.

---

## File Structure

### Production Code (Modify)

| File | Responsibility |
|---|---|
| `plugins/gpu_driver/drv/gpgpu_device.cpp` | Remove `sim/mem-pool.h` include; replace sim calls with hal_* wrappers |
| `plugins/gpu_driver/drv/gpu_drm_driver.cpp` | (if applicable) Remove `sim/mem-pool.h` include; replace sim calls |
| `plugins/gpu_driver/hal/hal_user.cpp` | (if applicable) Realize hal_mem-pool_* lambdas |
| `plugins/gpu_driver/hal/hal_mock.cpp` | Verify no-op mocks unchanged |

### Tests (regression gate)

| File | Responsibility |
|---|---|
| `tests/test_*_standalone.cpp` | Run existing mem-pool tests; add regression constraint if applicable |

---

## Pre-Task: Audit

```bash
cd /workspace/project/UsrLinuxEmu
grep -n 'sim_mem_pool' plugins/gpu_driver/drv/gpgpu_device.cpp plugins/gpu_driver/drv/gpu_drm_driver.cpp
grep -n 'hal_mem_pool' plugins/gpu_driver/hal/hal_user.cpp plugins/gpu_driver/hal/hal_mock.cpp
```

---

## Source Tasks

Source: `openspec/changes/stage4-l2-foundation-removal-mem-pool/tasks.md`


## 1. Implementation

- [ ] 1.1 在 mem_pool 相关测试中增加 drv mem_pool 经 `hal_mem_pool_*` 路径访问 sim 的回归约束，并先运行目标测试确认该约束在迁移前能够暴露直接 `sim_mem_pool_*` 路径或缺失的 HAL 路径证明
- [ ] 1.2 在 `plugins/gpu_driver/drv/gpgpu_device.cpp` 移除 `#include "sim/mem_pool.h"`
- [ ] 1.3 在 `plugins/gpu_driver/drv/gpgpu_device.cpp` 将约 15 处 `sim_mem_pool_*` 调用 1:1 迁移为对应的 `hal_mem_pool_*` inline wrappers，不改变调用顺序、参数、错误处理或性能特性
- [ ] 1.4 处理 `mem_pool_set_attr` / `mem_pool_get_attr` 的签名差异：原 `uint64_t val` / `uint64_t* out` 改为 typed buffer + `sizeof(*buffer)`，确保编译期类型检查
- [ ] 1.5 在 `plugins/gpu_driver/drv/gpu_drm_driver.cpp` 移除 `#include "sim/mem_pool.h"`
- [ ] 1.6 在 `plugins/gpu_driver/drv/gpu_drm_driver.cpp` 将约 12 处 `sim_mem_pool_*` 调用 1:1 迁移为对应的 `hal_mem_pool_*` inline wrappers，不改变调用顺序、参数、错误处理或性能特性
- [ ] 1.7 处理 `mem_pool_set_attr` / `mem_pool_get_attr` 的签名差异（同 1.4 约束）
- [ ] 1.8 运行更新后的 mem_pool 目标测试，确认 drv 使用 HAL 路径且现有 mem_pool 测试仍 PASS

## 2. Verification

- [ ] 2.1 验证 `grep -rn '#include.*"sim/' plugins/gpu_driver/drv/` 恰好输出 5 行（L2 违规计数 7 → 5）
- [ ] 2.2 验证 `plugins/gpu_driver/drv/gpgpu_device.cpp` 与 `plugins/gpu_driver/drv/gpu_drm_driver.cpp` 均不再包含 `#include "sim/mem_pool.h"`
- [ ] 2.3 验证两个 in-scope drv 文件中不再存在 `sim_mem_pool_*` 直接调用（27 处 call site 全部迁移）
- [ ] 2.4 验证 `struct gpu_hal_ops` fn-ptr 总数仍为 46，且本 change 未新增、删除或重排 HAL fn-ptrs
- [ ] 2.5 验证 `mem_pool_set_attr` / `mem_pool_get_attr` 调用的 typed buffer + size 参数类型正确（编译通过 + mem_pool 行为测试通过）
- [ ] 2.6 运行完整 ctest，确认 130/130 PASS（0 regression）
- [ ] 2.7 运行 docs-audit，确认 PASS

## 3. Process / Documentation

- [ ] 3.1 确认变更仅涉及提案列出的两个 drv 文件，未修改 `sim/mem_pool.h`、`sim/mem_pool.cpp`、`hal_user.cpp`、`hal_mock.cpp` 或其他 removal scope
- [ ] 3.2 确认实现与验收记录引用 ADR-072 §Decision 4 revised、ADR-023 §Decision 4、ADR-023 §Decision 5 及 foundation commit `11a0a2b`
- [ ] 3.3 使用单个 removal commit 涵盖所有 27 处 call site（因 mem_pool 单一 include），并在 merge commit message 中引用 ADR-072 §D4 revised 与 foundation commit `11a0a2b`
