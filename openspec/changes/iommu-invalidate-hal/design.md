# iommu-invalidate-hal: 技术设计

## Context

hal-iommu-full 已实现 3 个 IOMMU 回调（`iommu_map_memory`、`iommu_unmap_memory`、`iommu_invalidate_range`），但 `iommu_invalidate_range` 是按 range 范围失效，不满足精细控制需求。本 change 新增独立的 `iommu_invalidate` fn-ptr，提供与 Linux kernel `iommu_ops.flush_iotlb_all` 语义对齐的全量/选择性 IOMMU TLB 失效能力。

## Goals / Non-Goals

**Goals:**
- 在 `gpu_hal_ops` 中新增第 15 个 fn-ptr `iommu_invalidate`（不修改现有 14 个签名）
- Sim 路径基于 `UserEmuEnv.iommu_mappings` 实现批量失效
- 真机路径调用 `iommu_flush_iotlb_all`（Linux kernel IOMMU API）
- 错误码与 Linux kernel IOMMU API 对齐（`-EINVAL`, `-ENOENT`, `0` 成功）

**Non-Goals:**
- 不修改现有 14 个 fn-ptrs 的签名
- 不改变现有 `iommu_map_memory` / `iommu_unmap_memory` 行为
- 不涉及 PCIe ATS invalidation protocol

## Decisions

### Decision 1: 新增独立 fn-ptr，不重载现有回调

**选择**: 在 `gpu_hal_ops` 结构体末尾追加 `int (*iommu_invalidate)(gpu_va_t va, size_t size)`，保持现有 14 个 fn-ptr 不变。

**理由**:
- API 向前兼容：hal-iommu-full 的 3 个回调已经在用，修改签名会破坏所有消费者
- 语义清晰：`iommu_invalidate_range` 是粗粒度的 (per-range)，`iommu_invalidate` 是细粒度的 (per-mapping)
- Linux kernel 先例：Linux `iommu_ops` 同时提供 `flush_iotlb_all` 和 `iotlb_range_add`，两者不互斥

**替代方案**:
- 重载 `iommu_invalidate_range`: 语义分裂，range vs single-va 互不清
- 放在其他结构体: 不属于 `gpu_hal_ops` 的职责范围

### Decision 2: Sim 端失效策略 — 直接清空映射表

**选择**: Sim 端 `iommu_invalidate` 直接从 `UserEmuEnv.iommu_mappings` 中移除目标映射条目。

**理由**:
- Sim 不模拟硬件 TLB buffering，flush 即 remove
- 与 hal-iommu-full Decision 2 的映射存储方案一致
- 实现最简单，验证成本低

## Risks / Trade-offs

| Risk | Mitigation |
|------|------------|
| Sim/真机 invalidate 语义不一致（sim 直接 remove，真机依赖硬件 TLB） | 测试覆盖两条路径的 invalidate + re-map 场景 |
| `va=0` 的调用可能合法 | 不将 `va=0` 作为非法值，依赖返回值 `-ENOENT` 区分 |
| 真机端 `iommu_flush_iotlb_all` 可能过于激进 | 未来可扩展 per-domain flush，当前全量 flush 满足需求 |