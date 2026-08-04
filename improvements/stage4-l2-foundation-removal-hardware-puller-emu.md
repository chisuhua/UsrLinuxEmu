# stage4-l2-foundation-removal-hardware-puller-emu

**优先级**: P1 | **来源**: ADR-072 §Decision 4 revised — Stage 4.7 B-class L2 Phase 2 第五刀（收尾）
**阶段**: stage-4.7 | **分类**: core-impl
**类型**: refactor (HAL 边界收尾 — 第二个 class 类型集成)

## 架构依据

[Stage 4.7.1 foundation](../archive/2026-08-04-2026-08-03-stage4-l2-foundation-phase2-hal/) (commit `11a0a2b`) 在 `struct gpu_hal_ops` 上追加了 3 个 hardware_puller_emu 相关 fn-ptrs，**用 opaque handle (`hal_puller_handle_t`) 抽象了 `HardwarePullerEmu` class 类型**：

- `puller_set_puller` / `puller_register_queue` / `puller_unregister_queue`

**这是 Phase 2 最后一个 removal**（5 个中收尾刀），完成后 **Phase 2 目标违规全部清除** (L2: 8 → 0)。

**与 gpu-queue-emu 相同的复杂度**：drv/ 使用 `std::shared_ptr<HardwarePullerEmu>` class 类型。此 change 需要与 `removal-gpu-queue-emu` 协同（queue↔puller 相互引用：`queue_register_puller` 传 puller handle，`puller_register_queue` 传 queue handle）。

**drv/ 当前用法**（per `gpgpu_device.cpp`）:
- `std::shared_ptr<HardwarePullerEmu>` 成员变量
- 调用 `submitBatch()` / `registerQueue()` / `setPuller()` 等 class 方法
- `HardwarePullerEmu` 构造函数接收 `struct gpu_hal_ops* hal`

**架构依据**:
- ADR-072 §Decision 4 revised — B-class 修复路径
- ADR-023 §Decision 4 — HAL 接口 append-only 扩展 + C 兼容约束
- ADR-023 §Decision 5 — ② 仅通过 HAL fn-ptrs 访问 ③

## 范围

- **In Scope**:
  - `plugins/gpu_driver/drv/gpgpu_device.cpp` — 移除 `#include "sim/hardware/hardware_puller_emu.h"`，将 `shared_ptr<HardwarePullerEmu>` 成员改为 `hal_puller_handle_t` opaque handle
  - drv/ 中所有 `HardwarePullerEmu` class 方法调用迁移到 HAL wrapper：
    - `submitBatch` → `hal_puller_set_puller` + 配套（submitBatch 语义需评估：foundation 阶段 stub 未覆盖此方法，可能需要新增 fn-ptr 或扩展现有 wrapper 语义）
    - `registerQueue` → `hal_puller_register_queue`
    - `unregisterQueue` → `hal_puller_unregister_queue`
    - `setPuller` → `hal_puller_set_puller`
  - `hal_user.cpp` 中 `puller_*` lambdas 从 stub 升级为真实 HardwarePullerEmu 实例管理（`hal_user_context` 持有实例 + opaque handle 映射）
  - `hal_mock.cpp` 中 mock 保持 no-op（不变）
  - **L2 违规计数最终验证**: 8 → 0（5 个 Phase 2 目标违规全部清除）
  - 完整 ctest 130/130 PASS
  - docs-audit PASS
- **Out Scope**:
  - `sim/sim_event.h` (kfd_events.c) — 明确不在 Phase 2 范围（L2 残留 1 处，需独立 proposal 新增 HAL fn-ptr）
  - 其他 4 个 removal change（本 change 依赖 `removal-gpu-queue-emu` 已 ship）
  - HardwarePullerEmu 的 `submitBatch` 语义真实化（如果发现 foundation 缺 fn-ptr → 需评估是否独立 change）

## 关键场景

- GIVEN drv/ 持有 `std::shared_ptr<HardwarePullerEmu>` WHEN 移除 `sim/hardware/hardware_puller_emu.h` include THEN 编译失败 → 改为 `hal_puller_handle_t` opaque handle
- GIVEN puller 创建 THEN 通过 HAL 返回 opaque handle，真实实例由 `hal_user_context` 管理
- GIVEN `submitBatch` 调用 THEN 通过 HAL wrapper（如果 foundation 已覆盖）或新增 fn-ptr
- WHEN 完成 THEN L2 违规计数: 2 → 1（移除 1 处 include）→ 5 个 removal 完成后 Phase 2 目标全部清除（仅 sim_event.h 残留）
- GIVEN queue↔puller 相互引用 THEN `queue_register_puller` / `puller_register_queue` 通过 opaque handles 互操作

## 技术约束

- MUST 在 `hal_user_context` 中新增 puller 实例存储（`std::vector<std::shared_ptr<HardwarePullerEmu>>` 或 map）
- MUST 保持 `hal_puller_handle_t` 为 uint64_t opaque
- MUST 将 `puller_*` lambdas 从 stub 升级为真实实例管理 + opaque handle 映射
- MUST 与 `removal-gpu-queue-emu` 保持顺序依赖（queue↔puller 相互引用需两侧 handle 都存在）
- MUST 处理 `submitBatch` 调用语义：如果 foundation 3 个 fn-ptrs 未覆盖，需在 scope 内新增 fn-ptr 或记录为独立 follow-up
- MUST 完整 ctest 130/130 PASS
- MUST 通过 HAL 边界检查：sim/hardware/hardware_puller_emu.h 不再出现在 drv/ grep 中
- MUST NOT 修改 `sim/hardware/hardware_puller_emu.h`、`sim/hardware/hardware_puller_emu.cpp`
- SHOULD 单 commit 涵盖所有 puller call site

## 验收标准

- [ ] `plugins/gpu_driver/drv/gpgpu_device.cpp` 不再包含 `#include "sim/hardware/hardware_puller_emu.h"`
- [ ] `grep -rn '#include.*"sim/' plugins/gpu_driver/drv/` 输出 **1 行**（仅 `sim/sim_event.h` in kfd_events.c）
- [ ] drv/ 中无 `shared_ptr<HardwarePullerEmu>` 类型依赖（全部改为 hal_puller_handle_t）
- [ ] `hal_user_context` 持有 puller 实例 + opaque handle 映射
- [ ] `puller_*` lambdas 已真实化（非 stub）
- [ ] **Phase 2 5 个 removal 全部完成后**: L2 违规 8 → 0（Phase 2 目标达成）
- [ ] 完整 ctest 130/130 PASS（0 regression）
- [ ] docs-audit PASS
- [ ] `struct gpu_hal_ops` fn-ptr 总数 = 46（不变，除非 submitBatch 需要新增）