## Context

Stage 4.7.1 foundation change（commit `11a0a2b`）已在 `struct gpu_hal_ops` 上按 append-only 规则追加 3 个 hardware_puller_emu fn-ptrs，并通过 opaque handle `hal_puller_handle_t`（uint64_t）抽象了 `HardwarePullerEmu` C++ class 类型（per ADR-023 §Decision 4 — HAL 接口 C 兼容约束）：

- `puller_set_puller`
- `puller_register_queue`
- `puller_unregister_queue`

`gpu_hal.h` 同时提供对应的零开销 inline wrappers `hal_puller_*`。

当前 ② drv 层在 `plugins/gpu_driver/drv/gpgpu_device.cpp` 中：
- 持有 `std::shared_ptr<HardwarePullerEmu>` **class 类型**成员变量
- 调用 `submitBatch()` / `registerQueue()` / `setPuller()` 等 class 方法
- `HardwarePullerEmu` 构造函数接收 `struct gpu_hal_ops* hal`

直接 `#include "sim/hardware/hardware_puller_emu.h"` 是 Phase 2 最后一个 removal（5 个中收尾刀），完成后 **Phase 2 目标违规全部清除**（L2: 8 → 0）。与 `removal-gpu-queue-emu` 相同的 class 类型复杂度：drv 使用 `std::shared_ptr<HardwarePullerEmu>` class 类型；需要与 queue removal 协同（queue↔puller 相互引用：`queue_register_puller` 传 puller handle，`puller_register_queue` 传 queue handle）。

Open item（来自 proposal）：foundation 3 个 fn-ptrs 可能不覆盖 `submitBatch` 语义；本 change 内评估是否需要新增 fn-ptr 或扩展 wrapper 语义，否则记录为独立 follow-up。

迁移映射：

- `submitBatch()` → `hal_puller_submit_batch(hal_, h, ...)` 或合并到 `puller_set_puller`（评估）
- `registerQueue()` → `hal_puller_register_queue(hal_, h, queue_h)`
- `unregisterQueue()` → `hal_puller_unregister_queue(hal_, h, queue_h)`
- `setPuller()` → `hal_puller_set_puller(hal_, h, sim_puller_handle)`
- 构造 → `hal_puller_create(...)` 返回 opaque handle

drv/ 成员从 `shared_ptr<HardwarePullerEmu>` 改为 `hal_puller_handle_t` opaque handle；`hal_user.cpp` 中 `puller_*` lambdas 从 stub 升级为真实 HardwarePullerEmu 实例管理；`hal_mock.cpp` 中 mock 保持 no-op。

**架构依据：**

- **ADR-072 §Decision 4 revised** — B-class 使用 1 个 foundation + N 个 removals 的修复路径
- **ADR-023 §Decision 4** — `struct gpu_hal_ops` 只能 append-only 扩展；opaque handle 抽象是 C 兼容约束的体现
- **ADR-023 §Decision 5** — ② 驱动代码仅通过 HAL fn-ptrs 访问 ③ sim

## Goals / Non-Goals

**Goals:**

- 从 `gpgpu_device.cpp` 移除 `#include "sim/hardware/hardware_puller_emu.h"`
- 将 `std::shared_ptr<HardwarePullerEmu>` 成员改为 `hal_puller_handle_t` opaque handle
- 迁移所有 `HardwarePullerEmu` class 方法调用（submitBatch / registerQueue / unregisterQueue / setPuller 等）到对应 HAL wrapper
- 升级 `hal_user.cpp` 中 `puller_*` lambdas 从 stub 升级为真实 HardwarePullerEmu 实例管理（`hal_user_context` 持有实例 + opaque handle 映射表）
- 保持 `hal_mock.cpp` 中 mock no-op 行为不变
- **L2 违规计数最终验证**：8 → 0（5 个 Phase 2 目标违规全部清除）
- 完整 ctest 130/130 PASS，docs-audit PASS

**Non-Goals:**

- 不修改 `sim/hardware/hardware_puller_emu.h`、`sim/hardware/hardware_puller_emu.cpp` 或其他 sim layer source
- 不处理 `sim/sim_event.h`（kfd_events.c）—— 明确不在 Phase 2 范围（L2 残留 1 处，需独立 proposal 新增 HAL fn-ptr）
- 不实现 `HardwarePullerEmu::submitBatch` 的真实 sim 行为（若 foundation 缺 fn-ptr，本 change 内评估新增或记录为 follow-up）
- 不新增、删除或重排 `struct gpu_hal_ops` 中已有的 fn-ptrs（除非 submitBatch 需要新增）
- 不处理 graph、mem_pool、stream_capture、gpu_queue_emu 的其他 removal

## Approach

### Step 1: 先建立 HAL 路径回归约束

在相关 hardware_puller_emu 测试中增加或调整覆盖，验证 drv puller 操作通过 HAL puller 接口执行；保留现有 sim puller 独立行为测试。先运行目标测试确认新增约束在迁移前能够暴露直接 `HardwarePullerEmu` class 路径或缺失的 HAL 路径证明。

### Step 2: 评估 submitBatch 语义覆盖

- 检查 foundation commit `11a0a2b` 中 3 个 puller fn-ptrs 的精确签名，确认是否覆盖 `submitBatch`
- 若未覆盖：在本 change scope 内新增 `hal_puller_submit_batch` fn-ptr + wrapper + hal_user/hal_mock 实现；或将 submitBatch 调用整合到 `puller_set_puller` 语义
- 若决定新增 fn-ptr：保持 append-only 规则，fn-ptr 总数 46 → 47

### Step 3: 修改 drv 成员类型

在 `plugins/gpu_driver/drv/gpgpu_device.cpp`：
- 移除 `#include "sim/hardware/hardware_puller_emu.h"`
- 将 `std::shared_ptr<HardwarePullerEmu>` 成员改为 `hal_puller_handle_t`（uint64_t opaque handle）
- 不引入 `HardwarePullerEmu` class 的任何前向声明或 type alias

### Step 4: 迁移 drv 中 HardwarePullerEmu class 方法调用

将以下 class 方法调用替换为对应 HAL inline wrapper：

- `p->submitBatch(...)` → `hal_puller_submit_batch` 或合并到现有 wrapper（视 Step 2 决策）
- `p->registerQueue(...)` → `hal_puller_register_queue(hal_, h, queue_h)`
- `p->unregisterQueue(...)` → `hal_puller_unregister_queue(hal_, h, queue_h)`
- `p->setPuller(...)` → `hal_puller_set_puller(hal_, h, sim_puller_handle)`

### Step 5: 升级 hal_user.cpp puller_* lambdas

- `hal_user_context` 新增 `std::unordered_map<hal_puller_handle_t, std::shared_ptr<HardwarePullerEmu>>` 实例存储
- `puller_create` lambda 创建真实 HardwarePullerEmu 实例，传入 `struct gpu_hal_ops* hal`，返回 opaque handle
- `puller_set_puller` / `puller_register_queue` / `puller_unregister_queue` / `puller_submit_batch`（若新增）lambda 从 handle 映射查找实例并调用对应方法

### Step 6: 静态边界与 ABI 验证

- 确认 `drv/` 中不再直接包含 `sim/hardware/hardware_puller_emu.h`，也不再出现 `HardwarePullerEmu` class 类型或 `shared_ptr<HardwarePullerEmu>`
- 确认 `grep -rn '#include.*"sim/' plugins/gpu_driver/drv/` 恰好输出 1 行（仅 `sim/sim_event.h` in kfd_events.c，Out of Scope）
- 确认 `struct gpu_hal_ops` fn-ptr 总数：若 submitBatch 不新增 fn-ptr → 46；若新增 → 47

### Step 7: 回归与文档门禁

- 运行 puller 相关测试，验证 sim puller 行为保持不变
- 运行完整 ctest，要求 130/130 PASS
- 运行 docs-audit，要求 PASS

## Risks

| Risk | Likelihood | Mitigation |
|------|------------|------------|
| foundation 3 个 fn-ptrs 不覆盖 `submitBatch` | High | Step 2 在实现前评估；若缺，在 scope 内新增 fn-ptr 或记录为 follow-up |
| drv 中仍有未迁移的 `HardwarePullerEmu` class 方法调用 | Medium | 编译期强制要求移除 include；所有 class 方法调用必须通过 HAL |
| `hal_user_context` 实例存储与 queue 侧的 `hal_queue_handle_t` 映射互操作 | Medium | 与 `removal-gpu-queue-emu` 协同：本 change 在该 change 已 ship 后执行，queue 侧 handle 类型已存在 |
| queue↔puller 相互引用在两侧 handle 都存在后才能完整 wire-up | High | `removal-gpu-queue-emu` 先 ship；本 change 完成 puller 侧实例存储后，wire-up 验证可在 ctest 中端到端验证 |
| L2 计数受其他并行变更影响 | Low | 以本 change 基线 2 → 1 → 0（5 个 removal 完成后 Phase 2 目标达成）为验收口径 |
| HAL 间接调用改变错误传播或调用顺序 | Low | 严格 1:1 替换，不重构周边控制流；完整 ctest 验证等价行为 |
