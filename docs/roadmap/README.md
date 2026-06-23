# UsrLinuxEmu 架构演进路线图

> **性质**: 架构层叙事，描述从当前 MVP 到终态蓝图的演进路径
> **不绑定**: 本路线图不引用具体 OpenSpec change 编号。后续 OpenSpec change 根据本路线图派生
> **同步关系**: 与 `docs/sync-plan.md` 互补（sync-plan 负责跨仓同步点，本路线图负责架构演进阶段）
> **最后更新**: 2026-06-23
> **维护者**: UsrLinuxEmu Architecture Team

---

## 架构原则：3 区分 + HAL 桥

UsrLinuxEmu 的所有工作围绕三个清晰分离的层面 + 一个桥接适配器：

| 编号 | 层次 | 路径 | 职责 |
|------|------|------|------|
| ① | Linux 内核环境模拟 | `src/kernel/`, `include/kernel/`, `include/linux_compat/` | 提供 Linux 内核 API（VFS, 调度, IOMMU, mmu_notifier, DRM, PCIe, 中断）|
| ② | 可移植的驱动代码实现 | `plugins/gpu_driver/drv/` | GPGPU 驱动逻辑（KFD 风格），用真实 Linux 内核 API 写，可编译进真实内核模块 |
| ③ | 硬件模拟 | `plugins/gpu_driver/sim/` | 模拟真实 GPU 硬件（pushbuffer, 调度器, 寄存器, fence, 中断）|
| HAL | **桥（bridge）** | `plugins/gpu_driver/hal/` | 11 个函数指针表，② 与 ③ 之间的依赖反向注入点 |

**HAL 不是第 4 层**，HAL 是 ② 调 ③ 的桥接适配器。UsrLinuxEmu 通过 `hal_mock.cpp` 注入 sim，真机通过 `hal_user.cpp` 注入真实硬件。驱动代码本身零修改即可切换环境。

**完整原则**: 见 [ADR-036](../00_adr/adr-036-three-way-separation.md) ✅ Accepted
**当前 SSOT 实现**: 见 [post-refactor-architecture.md §1.10](../02_architecture/post-refactor-architecture.md)

---

## 阶段总览

| 阶段 | 状态 | 目标 | 文件 |
|------|------|------|------|
| **阶段 0** | ✅ 已达成 | MVP，单一 GPGPU 设备可验证 | [stage-0-mvp.md](stage-0-mvp.md) |
| **阶段 1** | 🔄 计划中 | Linux 内核环境模拟（DRM + UVM + IOMMU + ATS + PCIe BAR/中断）| [stage-1-kernel-emu.md](stage-1-kernel-emu.md) |
| **阶段 2** | 📋 规划中 | 多设备插件化（网络 + 存储）| [stage-2-multi-device.md](stage-2-multi-device.md) |
| **阶段 3** | 📋 规划中 | v1.0 稳定（CI 全平台、文档完善、性能优化）| [stage-3-v1.0.md](stage-3-v1.0.md) |
| **终态蓝图** | 📋 愿景 | 3 区分成熟形态，可移植驱动可在真实 Linux 内核中编译运行 | [blueprint.md](blueprint.md) |

---

## 阶段关系图

```
阶段 0 (MVP, 已达成)
   ↓
阶段 1 (Linux 内核环境模拟，5 子阶段)
   ├── 1.0 PCIe 设备模拟
   ├── 1.1 IOMMU + ATS
   ├── 1.2 DRM 子集（drm_device 风格重构）
   ├── 1.3 UVM/HMM
   └── 1.4 集成验证（KFD 编译跑通）
   ↓
阶段 2 (多设备插件化)
   ↓
阶段 3 (v1.0 稳定)
   ↓
终态蓝图（3 区分成熟形态）
```

---

## 阅读顺序

1. **本文**（README.md），先了解整体路线
2. **[stage-0-mvp.md](stage-0-mvp.md)**，查看当前 MVP 状态（已完成）
3. **[stage-1-kernel-emu.md](stage-1-kernel-emu.md)**，下一步核心工作
4. **[stage-2-multi-device.md](stage-2-multi-device.md)** + **[stage-3-v1.0.md](stage-3-v1.0.md)**，后续规划
5. **[blueprint.md](blueprint.md)**，终态愿景

---

## 跨引用

- [ADR-036](../00_adr/adr-036-three-way-separation.md), 3 区分架构原则
- [SSOT §1.10](../02_architecture/post-refactor-architecture.md), 3 区分的当前实现
- [ADR-035](../00_adr/adr-035-governance-policy.md), 治理规则（ADR/变更/SSOT 维护）
- [sync-plan.md](../sync-plan.md), 跨仓同步点（互补关系）

## 维护说明

本路线图文件由 UsrLinuxEmu Architecture Team 维护。任何阶段状态变更、原则修正、目标调整需走 ADR 流程（见 ADR-035）。

阶段文件 (`stage-*.md` / `blueprint.md`) 与 OpenSpec change 解耦：阶段描述"做什么"与"为什么"，OpenSpec change 描述"如何做"与"何时做"。两者通过 ADR 关联，不通过编号绑定。

---

**对应 ADR**: ADR-035 (governance) + ADR-036 (3-way principle)
**对应 SSOT 章节**: §1.10
**OpenSpec 状态**: 不绑定（无 change 编号）
