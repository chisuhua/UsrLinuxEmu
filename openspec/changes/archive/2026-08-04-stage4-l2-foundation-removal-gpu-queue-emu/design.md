## Context

Stage 4.7.1 foundation change（commit `11a0a2b`）已在 `struct gpu_hal_ops` 上按 append-only 规则追加 5 个 gpu_queue_emu fn-ptrs，并通过 opaque handle `hal_queue_handle_t`（uint64_t）抽象了 `GpuQueueEmu` C++ class 类型（per ADR-023 §Decision 4 — HAL 接口 C 兼容约束）：

- `queue_create`
- `queue_attach_shmem`
- `queue_submit`
- `queue_destroy`
- `queue_register_puller`

`gpu_hal.h` 同时提供对应的零开销 inline wrappers `hal_queue_*`。

当前 ② drv 层在 `plugins/gpu_driver/drv/gpgpu_device.cpp` 中：
- 持有 `std::shared_ptr<GpuQueueEmu>` **class 类型**成员变量（不只是函数调用）
- 调用 `attachSharedMemory()` / `submit()` / `setPuller()` 等 class 方法
- 通过构造函数 `(queue_id, queue_type, priority, ring_size)` 创建实例

直接 `#include "sim/gpu_queue_emu.h"` 是 Phase 2 中最复杂的 removal 之一，原因：移除 include 后 `GpuQueueEmu` 类型对 drv 不可见，`shared_ptr<GpuQueueEmu>` 成员无法编译。

本 change 的迁移映射：

- `attachSharedMemory()` → `hal_queue_attach_shmem(handle, ...)`
- `submit()` → `hal_queue_submit(handle, ...)`
- `setPuller()` → `hal_queue_register_puller(handle, ...)`
- 构造 → `hal_queue_create(...)` 返回 opaque handle

drv/ 成员从 `shared_ptr<GpuQueueEmu>` 改为 `hal_queue_handle_t` opaque handle；`hal_user.cpp` 中 `queue_create` lambda 从 stub 升级为**真实 GpuQueueEmu 实例管理**（`hal_user_context` 持有实例 + opaque handle 映射）；`hal_mock.cpp` 中 mock 保持单调 handle。

本 change 是 Stage 4.7.2 Phase 2 B-class 的第四个 removal。完成后 L2 违规计数从 3 降至 2。明确 Out of Scope：`HardwarePullerEmu` 集成（`hal_queue_register_puller` 传 `hal_puller_handle_t` — 是 `removal-hardware-puller-emu` change 的范畴，queue↔puller 相互引用需要两侧 handle 都存在后再统一处理）。

**架构依据：**

- **ADR-072 §Decision 4 revised** — B-class 使用 1 个 foundation + N 个 removals 的修复路径
- **ADR-023 §Decision 4** — `struct gpu_hal_ops` 只能 append-only 扩展；opaque handle 抽象是 C 兼容约束的体现
- **ADR-023 §Decision 5** — ② 驱动代码仅通过 HAL fn-ptrs 访问 ③ sim

## Goals / Non-Goals

**Goals:**

- 从 `gpgpu_device.cpp` 移除 `#include "sim/gpu_queue_emu.h"`
- 将 `std::shared_ptr<GpuQueueEmu>` 成员改为 `hal_queue_handle_t` opaque handle
- 迁移所有 `GpuQueueEmu` class 方法调用（attachSharedMemory / submit / setPuller / queueId 等）到对应 HAL wrapper
- 升级 `hal_user.cpp` 中 `queue_create` lambda 从 stub 升级为**真实 GpuQueueEmu 实例管理**（`hal_user_context` 持有实例 + opaque handle 映射表）
- 保持 `hal_mock.cpp` 中 mock 单调 handle 行为不变
- 完整 ctest 130/130 PASS，docs-audit PASS

**Non-Goals:**

- 不修改 `sim/gpu_queue_emu.h`、`sim/gpu_queue_emu.cpp` 或其他 sim 层 source
- 不处理 `HardwarePullerEmu` 集成（明确在 `removal-hardware-puller-emu` 范畴）
- 不新增、删除或重排 `struct gpu_hal_ops` fn-ptrs
- 不处理 graph、mem_pool、stream_capture、hardware_puller_emu 的其他 removal
- 不处理 `kfd_events.c` 对 `sim_event.h` 的独立违规

## Approach

### Step 1: 先建立 HAL 路径回归约束

在相关 gpu_queue_emu 测试中增加或调整覆盖，验证 drv queue 操作通过 HAL queue 接口执行；保留现有 sim queue 独立行为测试。先运行目标测试确认新增约束在迁移前能够暴露直接 `GpuQueueEmu` class 路径或缺失的 HAL 路径证明。

### Step 2: 修改 drv 成员类型

在 `plugins/gpu_driver/drv/gpgpu_device.cpp`：
- 移除 `#include "sim/gpu_queue_emu.h"`
- 将 `std::shared_ptr<GpuQueueEmu>` 成员改为 `hal_queue_handle_t`（uint64_t opaque handle）
- 不引入 `GpuQueueEmu` class 的任何前向声明或 type alias（HAL 边界要求 drv 完全不感知 sim class）

### Step 3: 迁移 drv 中 GpuQueueEmu class 方法调用

将以下 class 方法调用替换为对应 HAL inline wrapper：

- `q->attachSharedMemory(...)` → `hal_queue_attach_shmem(hal_, h, ...)`
- `q->submit(...)` → `hal_queue_submit(hal_, h, ...)`
- `q->setPuller(...)` → `hal_queue_register_puller(hal_, h, puller_h, ...)`（puller handle 暂以 opaque 类型传入，wire-up 在 hardware-puller-emu change 完成）
- `q->queueId()` 等只读访问改为通过 HAL 提供的查询 wrapper（若有），或作为 follow-up 评估是否需要新增 fn-ptr

### Step 4: 升级 hal_user.cpp queue_create lambda

- `hal_user_context` 新增 `std::unordered_map<hal_queue_handle_t, std::shared_ptr<GpuQueueEmu>>` 或 `std::vector<std::shared_ptr<GpuQueueEmu>>` 实例存储
- `queue_create` lambda 创建真实 GpuQueueEmu 实例，返回 opaque handle 作为 key
- `queue_attach_shmem` / `queue_submit` / `queue_destroy` 等 lambda 从 handle 映射查找实例并调用对应方法
- `queue_register_puller` lambda 暂以 stub 行为存在（hardware-puller-emu change 真实化）

### Step 5: 静态边界与 ABI 验证

- 确认 `drv/` 中不再直接包含 `sim/gpu_queue_emu.h`，也不再出现 `GpuQueueEmu` class 类型或 `shared_ptr<GpuQueueEmu>`
- 确认 `grep -rn '#include.*"sim/' plugins/gpu_driver/drv/` 恰好输出 2 行（3 - 1）
- 确认 `struct gpu_hal_ops` fn-ptr 总数仍为 46

### Step 6: 回归与文档门禁

- 运行 queue 相关测试，验证 sim queue 行为保持不变
- 运行完整 ctest，要求 130/130 PASS
- 运行 docs-audit，要求 PASS

## Risks

| Risk | Likelihood | Mitigation |
|------|------------|------------|
| drv 中仍有未迁移的 `GpuQueueEmu` class 方法调用（如只读访问） | Medium | 编译期强制要求移除 include；所有 class 方法调用必须通过 HAL；只读访问若缺 wrapper 评估是否新增 fn-ptr 或作为 follow-up |
| `hal_user_context` 的实例存储与 handle 映射在并发场景下产生 race | Medium | hal_user 实例由单线程 drv 持有；若需并发则后续独立 change 处理 |
| `queue_register_puller` 传 `hal_puller_handle_t` 在 hardware-puller-emu change 之前不能完整 wire-up | High | 明确 Out of Scope；本 change 中 lambda 保留 stub 行为，wire-up 留到下一个 change |
| `setPuller` 与 `queue_register_puller` 语义差异（foundation 中两者签名需核对） | Medium | 对照 foundation commit `11a0a2b` 中 `queue_register_puller` 的精确签名迁移 |
| HAL 间接调用改变错误传播或调用顺序 | Low | 严格 1:1 替换，不重构周边控制流；完整 ctest 验证等价行为 |
| L2 计数受其他并行变更影响 | Low | 以本 change 基线 3 → 2 为验收口径；静态命令必须恰好输出 2 行 |
