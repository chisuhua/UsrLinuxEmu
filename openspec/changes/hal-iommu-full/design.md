# hal-iommu-full: 技术设计

## Context

UsrLinuxEmu 使用 3 区分架构（ADR-036），HAL（硬件抽象层）是 ②（可移植驱动代码）与 ③（硬件模拟 / 真机硬件）之间的桥。ADR-061 已将 IOMMU 相关的 3 个 fn-ptrs（`iommu_map_memory`、`iommu_unmap_memory`、`iommu_invalidate_range`）添加到 `gpu_hal_iommu_ops`，并提交了 `hal_mock.cpp` 和 `hal_user.cpp` 中的 stub。当前 stub 全部返回 `-ENOSYS`。

## Goals / Non-Goals

**Goals:**
- 在 sim 和真机两条路径上实现完整的 IOMMU ops
- sim 路径基于 `UserEmuEnv` 的 VA 空间管理 IOMMU 映射
- 真机路径调用真实 Linux kernel IOMMU API（通过 `linux_compat` 层）
- 错误码与 Linux kernel IOMMU API 对齐（`-EINVAL`, `-ENOMEM`, `-EFAULT` 等）

**Non-Goals:**
- 不修改 `gpu_hal_iommu_ops` 的函数签名（接口已定型）
- 不修改 IOMMU domain 对象模型（`iommu_domain.h`）
- 不涉及 PCIe ATS full protocol 实现（属于 ADR-061 Phase 2）

## Decisions

### Decision 1: Sim IOMMU 使用 flat page table

**选择**: sim 路径使用简单的 flat page table（数组 + 线性查找），每页 4KB，基于 `UserEmuEnv.va_base` 偏移计算。

**理由**:
- sim 路径不需要真正的 page walk 模拟（那是 ③ 层的职责）
- flat page table 实现简单，验证成本低
- 真机 `hal_user.cpp` 已经使用真实 kernel IOMMU API

**替代方案**:
- Multi-level page table: 过度设计，test 场景用不到

### Decision 2: IOMMU 映射持久化在 sim 内存中

**选择**: sim 的 IOMMU 映射关系存储在 `UserEmuEnv.iommu_mappings` map 中（key=gpu_va, value=user_va+size+flags）。

**理由**:
- unmap 和 invalidate 需要查询已有映射
- 不需要模拟硬件 TLB 行为

## Risks / Trade-offs

| Risk | Mitigation |
|------|------------|
| Sim 和真机 IOMMU map/unmap 行为不一致 | 测试覆盖两条路径的相同用例 |
| IOMMU page table 内存开销 | sim 侧用 `std::unordered_map`，真机侧由 kernel 管理 |
| mmu_notifier 回调链未就绪 | Spec 中标注为 `hal-iommu-invalidate` 独立 capability |
