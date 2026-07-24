# iommu-mmu-notifier: 技术设计

## Context

`include/linux_compat/iommu/iommu_domain.h:96` 标记 `TODO(stage-1.3): full mmu_notifier callback`。当前 `mmu_notifier` 接口已定义回调类型（`mmu_notifier_ops`），但缺少注册（`mmu_notifier_register`）和通知（`mmu_notifier_invalidate_range`）的实现。Stage 1.4 Tier-2 的 `mmu_notifier` callback body 已部分完整化（`iommu_invalidate_register_notifier_internal` 桥接到 framework），但回调链的注册和管理逻辑仍在 `TODO` 阶段。

## Goals / Non-Goals

**Goals:**
- 在 `src/kernel/iommu/` 中实现 `mmu_notifier_register` / `mmu_notifier_unregister` / `mmu_notifier_invalidate_range`
- 在 `iommu_invalidate_range` 完成后自动遍历注册列表触发回调
- 回调按注册顺序串行执行（先注册先通知）
- 添加 Catch2 测试验证注册、通知、注销全链路

**Non-Goals:**
- 不修改 `gpu_hal_ops` 或 HAL 层（HAL 通过 `iommu_invalidate` 间接触发通知）
- 不实现 Linux kernel `mmu_notifier_ops` 的全部回调（仅 `invalidate_range`）
- 不涉及 CPU-side page table walk 模拟

## Decisions

### Decision 1: 使用 `std::list` 管理回调链

**选择**: 回调注册表使用 `std::list<mmu_notifier_entry>`，entry 包含 `void *owner` + `mmu_notifier_ops *ops`。

**理由**:
- 需要支持注册/注销，`std::vector` 的 O(n) 删除不够好
- `std::list` 迭代中删除安全（注销后继续遍历其他回调）
- 注册频率低（设备 init 时），查找性能不关键

**替代方案**:
- `std::vector`: 注册 O(1)，但注销 O(n)，回调数少（<10）时可接受
- `std::unordered_map<owner, ops>`: 过度设计，owner 可能复用

### Decision 2: 回调在 `iommu_invalidate_range` 内部同步触发

**选择**: 回调在 `iommu_invalidate_range` 函数体内同步调用，不异步调度。

**理由**:
- 用户态模拟不需要异步（无硬件 TLB async flush）
- 同步调用保证内存一致性语义（回调执行完前映射不恢复）
- 与 Linux kernel `mmu_notifier_invalidate_range_start` 同步调用语义一致

**替代方案**:
- 异步工作队列: 用户态模拟中无意义，引入不必要的复杂性

## Risks / Trade-offs

| Risk | Mitigation |
|------|------------|
| 回调中注销自己导致 `std::list` 迭代器失效 | 使用 erase 返回值推进迭代器（`it = list.erase(it)`） |
| 回调中注册新回调导致无限循环 | 仅遍历调用前的快照（invalidation 范围已确定） |
| 阻塞回调延迟 IOMMU 操作 | 用户态模拟场景，回调不在热路径 |