# stage4-l2-foundation-removal-hardware-puller-emu Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use skill_use("execute") to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remove L2 violation `#include "sim/hardware-puller-emu.h"` from drv/ files and replace direct sim calls with HAL fn-ptr wrappers. Per ADR-072 §Decision 4 revised + ADR-023 Decision 4 (opaque handle abstraction).

**Architecture:** drv/ side calls `hal_*` inline wrappers; HAL lambdas (`hal_user.cpp`) route to real sim/ implementations. `hal_mock.cpp` keeps no-op mocks.

**Tech Stack:** C99-compatible C (HAL interface), C++17 lambdas (sim impls), Catch2.

---

## File Structure

### Production Code (Modify)

| File | Responsibility |
|---|---|
| `plugins/gpu_driver/drv/gpgpu_device.cpp` | Remove `sim/hardware-puller-emu.h` include; replace sim calls with hal_* wrappers |
| `plugins/gpu_driver/drv/gpu_drm_driver.cpp` | (if applicable) Remove `sim/hardware-puller-emu.h` include; replace sim calls |
| `plugins/gpu_driver/hal/hal_user.cpp` | (if applicable) Realize hal_hardware-puller-emu_* lambdas |
| `plugins/gpu_driver/hal/hal_mock.cpp` | Verify no-op mocks unchanged |

### Tests (regression gate)

| File | Responsibility |
|---|---|
| `tests/test_*_standalone.cpp` | Run existing hardware-puller-emu tests; add regression constraint if applicable |

---

## Pre-Task: Audit

```bash
cd /workspace/project/UsrLinuxEmu
grep -n 'sim_hardware_puller_emu' plugins/gpu_driver/drv/gpgpu_device.cpp plugins/gpu_driver/drv/gpu_drm_driver.cpp
grep -n 'hal_hardware_puller_emu' plugins/gpu_driver/hal/hal_user.cpp plugins/gpu_driver/hal/hal_mock.cpp
```

---

## Source Tasks

Source: `openspec/changes/stage4-l2-foundation-removal-hardware-puller-emu/tasks.md`


## 1. Implementation

- [ ] 1.1 在 hardware_puller_emu 相关测试中增加 drv puller 经 `hal_puller_*` 路径访问 sim 的回归约束，并先运行目标测试确认该约束在迁移前能够暴露直接 `HardwarePullerEmu` class 路径或缺失的 HAL 路径证明
- [ ] 1.2 评估 `submitBatch` 语义覆盖：检查 foundation commit `11a0a2b` 中 3 个 puller fn-ptrs 签名是否覆盖 submitBatch；若未覆盖，在 scope 内新增 `hal_puller_submit_batch` fn-ptr + wrapper + hal_user/hal_mock 实现（append-only，46 → 47），或记录为独立 follow-up
- [ ] 1.3 在 `plugins/gpu_driver/drv/gpgpu_device.cpp` 移除 `#include "sim/hardware/hardware_puller_emu.h"`
- [ ] 1.4 在 `plugins/gpu_driver/drv/gpgpu_device.cpp` 将 `std::shared_ptr<HardwarePullerEmu>` 成员改为 `hal_puller_handle_t` opaque handle；不引入 `HardwarePullerEmu` class 的任何前向声明或 type alias
- [ ] 1.5 在 `plugins/gpu_driver/drv/gpgpu_device.cpp` 将所有 `HardwarePullerEmu` class 方法调用（submitBatch / registerQueue / unregisterQueue / setPuller 等）迁移到对应的 `hal_puller_*` inline wrappers
- [ ] 1.6 在 `plugins/gpu_driver/hal/hal_user.cpp` 为 `hal_user_context` 新增 puller 实例存储（`std::unordered_map<hal_puller_handle_t, std::shared_ptr<HardwarePullerEmu>>`）+ opaque handle 映射表
- [ ] 1.7 在 `plugins/gpu_driver/hal/hal_user.cpp` 将 `puller_create` lambda 从 stub 升级为**真实 HardwarePullerEmu 实例管理**（创建实例，传入 `struct gpu_hal_ops* hal`，返回 opaque handle 作为 key）
- [ ] 1.8 在 `plugins/gpu_driver/hal/hal_user.cpp` 将 `puller_set_puller` / `puller_register_queue` / `puller_unregister_queue` lambdas 改为从 handle 映射查找实例并调用对应方法
- [ ] 1.9 确认 `plugins/gpu_driver/hal/hal_mock.cpp` 中 puller mock 保持 no-op 行为不变
- [ ] 1.10 运行更新后的 puller 目标测试，确认 drv 使用 HAL 路径且现有 puller 测试仍 PASS

## 2. Verification

- [ ] 2.1 验证 `grep -rn '#include.*"sim/' plugins/gpu_driver/drv/` 恰好输出 1 行（仅 `sim/sim_event.h` in kfd_events.c，明确 Out of Scope）
- [ ] 2.2 验证 `plugins/gpu_driver/drv/gpgpu_device.cpp` 不再包含 `#include "sim/hardware/hardware_puller_emu.h"`
- [ ] 2.3 验证 drv 中不再出现 `HardwarePullerEmu` class 类型或 `shared_ptr<HardwarePullerEmu>`（全部改为 `hal_puller_handle_t`）
- [ ] 2.4 验证 `struct gpu_hal_ops` fn-ptr 总数：若未新增 submitBatch fn-ptr → 46；若新增 → 47
- [ ] 2.5 验证 `hal_user.cpp` 中 `hal_user_context` 已持有 HardwarePullerEmu 实例 + opaque handle 映射
- [ ] 2.6 验证 `puller_create` / `puller_set_puller` / `puller_register_queue` / `puller_unregister_queue` lambdas 已真实化（非 stub）
- [ ] 2.7 验证 `hal_mock.cpp` puller mock no-op 行为不变
- [ ] 2.8 验证 **Phase 2 5 个 removal 全部完成后** L2 违规 8 → 0（Phase 2 目标达成）
- [ ] 2.9 运行完整 ctest，确认 130/130 PASS（0 regression）
- [ ] 2.10 运行 docs-audit，确认 PASS

## 3. Process / Documentation

- [ ] 3.1 确认变更仅涉及 `plugins/gpu_driver/drv/gpgpu_device.cpp`、`plugins/gpu_driver/hal/hal_user.cpp`、`plugins/gpu_driver/hal/hal_mock.cpp` 及相关测试，未修改 `sim/hardware/hardware_puller_emu.h`、`sim/hardware/hardware_puller_emu.cpp` 或其他 sim layer
- [ ] 3.2 确认实现与验收记录引用 ADR-072 §Decision 4 revised、ADR-023 §Decision 4、ADR-023 §Decision 5、ADR-043 §D5（12 B-class violations 清除目标）及 foundation commit `11a0a2b`
- [ ] 3.3 确认与 `removal-gpu-queue-emu` 的顺序依赖：本 change 必须在 queue removal 已 ship 后执行（queue↔puller 相互引用需两侧 handle 都存在）
- [ ] 3.4 确认 `sim/sim_event.h`（kfd_events.c）显式标记为 Phase 2 范围外（L2 残留 1 处，需独立 proposal 新增 HAL fn-ptr）
- [ ] 3.5 使用单个 removal commit 涵盖所有 puller call site，并在 merge commit message 中引用 ADR-072 §D4 revised、ADR-023 §D4（opaque handle 抽象）、ADR-043 §D5 与 foundation commit `11a0a2b`
