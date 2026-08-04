# stage4-l2-foundation-removal-gpu-queue-emu

**优先级**: P1 | **来源**: ADR-072 §Decision 4 revised — Stage 4.7 B-class L2 Phase 2 第四刀
**阶段**: stage-4.7 | **分类**: core-impl
**类型**: refactor (HAL 边界收尾 — 首个 class 类型集成)

## 架构依据

[Stage 4.7.1 foundation](../archive/2026-08-04-2026-08-03-stage4-l2-foundation-phase2-hal/) (commit `11a0a2b`) 在 `struct gpu_hal_ops` 上追加了 5 个 gpu_queue_emu 相关 fn-ptrs，**并用 opaque handle (`hal_queue_handle_t`) 抽象了 `GpuQueueEmu` class 类型**：

- `queue_create` / `queue_attach_shmem` / `queue_submit` / `queue_destroy` / `queue_register_puller`

**这是 Phase 2 中最复杂的 removal**，原因：
- drv/ 使用 `shared_ptr<GpuQueueEmu>` **class 类型**（不只是函数调用）
- foundation 阶段用 `hal_queue_handle_t`（uint64_t）避免 C++ class 泄漏到 HAL C 接口（per ADR-023 §D4）
- drv/ 当前持有 `std::shared_ptr<GpuQueueEmu>` 成员，移除 `sim/gpu_queue_emu.h` include 后**类型不可见**

**drv/ 当前用法**（per `gpgpu_device.cpp`）:
- `std::shared_ptr<GpuQueueEmu>` 成员变量
- 调用 `attachSharedMemory()` / `submit()` / `setPuller()` 等 class 方法
- `GpuQueueEmu` 通过构造函数 `(queue_id, queue_type, priority, ring_size)` 创建

**架构依据**:
- ADR-072 §Decision 4 revised — B-class 修复路径
- ADR-023 §Decision 4 — HAL 接口 append-only 扩展 + C 兼容约束
- ADR-023 §Decision 5 — ② 仅通过 HAL fn-ptrs 访问 ③

## 范围

- **In Scope**:
  - `plugins/gpu_driver/drv/gpgpu_device.cpp` — 移除 `#include "sim/gpu_queue_emu.h"`，将 `shared_ptr<GpuQueueEmu>` 成员改为 `hal_queue_handle_t` opaque handle
  - drv/ 中所有 `GpuQueueEmu` class 方法调用（attachSharedMemory / submit / setPuller / queueId 等）迁移到对应 HAL wrapper：
    - `attachSharedMemory` → `hal_queue_attach_shmem`
    - `submit` → `hal_queue_submit`
    - `setPuller` → `hal_queue_register_puller`
    - 构造 → `hal_queue_create`
  - `hal_user.cpp` 中 `queue_create` lambda 从 stub 升级为**真实 GpuQueueEmu 实例管理**（在 `hal_user_context` 中持有实例 + opaque handle 映射表）
  - `hal_mock.cpp` 中 mock 保持单调 handle（不变）
  - 完整 ctest 130/130 PASS
  - docs-audit PASS
- **Out Scope**:
  - `HardwarePullerEmu` 集成（`hal_queue_register_puller` 传 `hal_puller_handle_t` — 是 `removal-hardware-puller-emu` change 的范畴）
  - 其他 3 个 removal change
  - kfd_events.c 中 sim_event.h 违规

## 关键场景

- GIVEN drv/ 持有 `std::shared_ptr<GpuQueueEmu>` WHEN 移除 `sim/gpu_queue_emu.h` include THEN 编译失败（类型不可见）→ 需改为 `hal_queue_handle_t` opaque handle
- GIVEN queue 创建 THEN 通过 `hal_queue_create` 返回 opaque handle，真实实例由 `hal_user_context` 管理
- GIVEN drv 需要 queue 方法调用 THEN 通过 `hal_queue_*` wrappers，传递 opaque handle
- WHEN 完成 THEN L2 违规计数: 3 → 2（移除 1 处 include，sim/gpu_queue_emu.h 消失）
- GIVEN `queue_attach_shmem` / `queue_submit` 在 foundation 阶段是 stub WHEN 此 change 真实化 THEN 行为与直接调用 GpuQueueEmu class 方法等价

## 技术约束

- MUST 在 `hal_user_context` 中新增 queue 实例存储（如 `std::vector<std::shared_ptr<GpuQueueEmu>>` 或 `std::unordered_map<hal_queue_handle_t, std::shared_ptr<GpuQueueEmu>>`）
- MUST 保持 `hal_queue_handle_t` 为 uint64_t opaque（不泄漏 C++ 类型到 HAL 头文件）
- MUST 将 `queue_create` lambda 从 stub 升级为真实实例创建 + opaque handle 映射
- MUST 将 `queue_attach_shmem` / `queue_submit` lambda 从 stub 升级为真实委托（reinterpret_cast opaque → GpuQueueEmu*）
- MUST 处理 drv/ 中所有 `shared_ptr<GpuQueueEmu>` 成员变量的生命周期（opaque handle 不管理所有权 → drv 所有权语义变化需文档化）
- MUST 完整 ctest 130/130 PASS
- MUST 通过 HAL 边界检查：sim/gpu_queue_emu.h 不再出现在 drv/ grep 中
- MUST NOT 修改 `sim/gpu_queue_emu.h`、`sim/gpu_queue_emu.cpp`
- SHOULD 与 `removal-hardware-puller-emu` 保持耦合认知（queue↔puller 相互引用）

## 验收标准

- [ ] `plugins/gpu_driver/drv/gpgpu_device.cpp` 不再包含 `#include "sim/gpu_queue_emu.h"`
- [ ] `grep -rn '#include.*"sim/' plugins/gpu_driver/drv/` 输出 2 行（3 - 1 = 2）
- [ ] drv/ 中无 `shared_ptr<GpuQueueEmu>` 类型依赖（全部改为 hal_queue_handle_t）
- [ ] `hal_user_context` 持有 queue 实例 + opaque handle 映射
- [ ] `queue_create` / `queue_attach_shmem` / `queue_submit` lambda 已真实化（非 stub）
- [ ] 完整 ctest 130/130 PASS（0 regression）
- [ ] docs-audit PASS
- [ ] `struct gpu_hal_ops` fn-ptr 总数 = 46（不变）