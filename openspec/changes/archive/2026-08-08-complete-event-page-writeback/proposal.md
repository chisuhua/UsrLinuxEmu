# complete-event-page-writeback

## Why

[ADR-062](docs/00_adr/adr-062-hal-event-signal-extension.md) 定义 HAL event signal ops，要求：
- 真实 GPU：event signal 写入 user-mapped event page（amdgpu `kfd_event_page_set`）
- 用户空间通过 mmap event page 读取事件，无需陷入内核

`plugins/gpu_driver/sim/sim_event.c:11-23` 当前实现：

```c
int sim_signal_event(u32 pasid, u32 event_id, u64 events) {
  if (pasid > 0xFFFF) return -22;  /* -EINVAL */
  if (event_id > 1024) return -22;  /* -EINVAL */
  if (events == 0) return -22;     /* -EINVAL */
  atomic_fetch_add(&sim_signal_count_, 1);
  /* TODO Phase C/E: write to user-mapped event page (amdgpu_kfd_event_page_set) */
  return 0;
}
```

**当前后果**：
- `sim_signal_event` 仅增加内部计数器 `sim_signal_count_`，不写入 event page
- 用户空间无法通过 mmap 接收事件通知
- 任何 `kfd_ioctl_wait_events` 路径都会超时（因事件位永远不会被设置）

[stage4-gpu-cp-completion-gap-analysis.md §2.1](docs/architecture/stage4-gpu-cp-completion-gap-analysis.md) 隐含列为 HAL wiring 残留项。

## What Changes

**In Scope**:

- 在 sim 层实现 event page 管理（per-process）
- `sim_signal_event` 写入 user-mapped event page 对应 bit 位
- 添加 `sim_event_page_alloc(pid, *page_ptr)` + `sim_event_page_free(pid)` API
- 用户 mmap `/dev/kfd` 时映射 event page（mmap callback）
- 事件类型位分配（signal events vs process events per amdgpu KFD ABI）
- 配套测试 `test_sim_event_page_standalone.cpp`

### 关键场景

- GIVEN 用户 mmap `/dev/kfd` event page
  - WHEN mmap 返回
  - THEN user_ptr 指向共享内存页（含 event bitmask）
- GIVEN `sim_signal_event(pasid=1, event_id=10, events=0x1)` 被调用
  - WHEN 执行
  - THEN event page offset=10 的 bit 0 被设置，`sim_signal_count_` 增加
- GIVEN event page 已被设置
  - WHEN 用户 mmap 该 page 并读取
  - THEN 看到对应的 event bit，无需陷入
- GIVEN `sim_event_page_alloc` 被调用两次（同 pid）
  - WHEN 第二次调用
  - THEN 返回 -EEXIST（避免双分配）

**Out of Scope**:

- User-space event wait syscall 实现（属独立 task / TaskRunner）
- GPU hardware interrupt → event signal 路径（已在 `interrupt_raise_ex` 中处理）
- Cross-process event sharing（per ADR-011 Phase 1 deferred）

## Capabilities

- MUST 保持 `sim_signal_event` 签名不变（向后兼容）
- MUST NOT 引入新的全局单例（用 process lookup 模式）
- SHOULD 复用现有 `kfd_events.c` 的数据结构（如果存在）
- SHOULD 8-byte 对齐 event page（per KFD ABI）
- SHOULD 4KB page size（与 Linux page size 一致）

## Impact

- MUST 保持 `sim_signal_event` 签名不变（向后兼容）
- MUST NOT 引入新的全局单例（用 process lookup 模式）
- SHOULD 复用现有 `kfd_events.c` 的数据结构（如果存在）
- SHOULD 8-byte 对齐 event page（per KFD ABI）
- SHOULD 4KB page size（与 Linux page size 一致）

## Acceptance

- `sim_signal_event` 实际写入 event page bit 位（不仅 increment counter）
- 新增 `sim_event_page_alloc/free` API
- mmap `/dev/kfd` 返回 event page pointer
- 新增 `test_sim_event_page_standalone.cpp`，至少 5 个 test case 覆盖：
  - event page alloc/free
  - signal_event → page bit set
  - signal_event 同 event_id 多次（OR 累积）
  - 不同 event_id 互不干扰
  - 越界 event_id > 1024 返回 -EINVAL
- `make -j4` 编译通过，无 warning
- `ctest --output-on-failure` 全部 PASS
- 修改的代码行通过 `lsp_diagnostics` 检查

