# UsrLinuxEmu 架构演进路线图

> **项目目标**: **开发一个易移植到 Linux 内核的 GPU 驱动**。
>
> 本路线图描述**从当前 MVP 到实现此目标的演进路径**（4 阶段 + 蓝图）。

> **性质**: 架构层叙事，描述从当前 MVP 到终态蓝图的演进路径
> **不绑定**: 本路线图不引用具体 OpenSpec change 编号。后续 OpenSpec change 根据本路线图派生
> **同步关系**: 与 `docs/sync-plan.md` 互补（sync-plan 负责跨仓同步点，本路线图负责架构演进阶段）
> **最后更新**: 2026-07-21
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

**完整原则**: 见 [ADR-036](docs/00_adr/adr-036-three-way-separation.md) ✅ Accepted
**当前 SSOT 实现**: 见 [post-refactor-architecture.md §1.10](docs/02_architecture/post-refactor-architecture.md)

---

## 阶段总览

| 阶段 | 状态 | 目标 | 文件 |
|------|------|------|------|
| **阶段 0** | ✅ 已达成 | MVP，单一 GPGPU 设备可验证 | [docs/roadmap/stage-0-mvp.md](docs/roadmap/stage-0-mvp.md) |
| **阶段 1** | ✅ 已达成 (2026-07-16) | Linux 内核环境模拟（DRM + UVM + IOMMU + ATS + PCIe BAR/中断）；C-12 KFD 多文件集成 81% 完成 + L1↔L2 bridge skeleton | [docs/roadmap/stage-1-kernel-emu.md](docs/roadmap/stage-1-kernel-emu.md) |
| **阶段 2** | ✅ 已达成 (2026-07-05) | 多设备插件化（网络 + 存储）| [docs/roadmap/stage-2-multi-device.md](docs/roadmap/stage-2-multi-device.md) |
| **阶段 3** | 🔄 进行中 | v1.0 稳定（CUDA E2E ✅、sanitizer infra ✅、L1↔L2 bridge ✅、性能优化、文档完善）| [docs/roadmap/stage-3-v1.0.md](docs/roadmap/stage-3-v1.0.md) |
| **终态蓝图** | 📋 愿景 | 3 区分成熟形态，可移植驱动可在真实 Linux 内核中编译运行；Stage 4: 真实 BAR + ioremap 模拟 | [docs/roadmap/blueprint.md](docs/roadmap/blueprint.md) |

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

1. **本文**，先了解整体路线
2. **[docs/roadmap/stage-0-mvp.md](docs/roadmap/stage-0-mvp.md)**，查看当前 MVP 状态（已完成）
3. **[docs/roadmap/stage-1-kernel-emu.md](docs/roadmap/stage-1-kernel-emu.md)**，下一步核心工作
4. **[docs/roadmap/stage-2-multi-device.md](docs/roadmap/stage-2-multi-device.md)** + **[docs/roadmap/stage-3-v1.0.md](docs/roadmap/stage-3-v1.0.md)**，后续规划
5. **[docs/roadmap/blueprint.md](docs/roadmap/blueprint.md)**，终态愿景

---

## 跨引用

- [ADR-036](docs/00_adr/adr-036-three-way-separation.md), 3 区分架构原则
- [SSOT §1.10](docs/02_architecture/post-refactor-architecture.md), 3 区分的当前实现
- [ADR-035](docs/00_adr/adr-035-governance-policy.md), 治理规则（ADR/变更/SSOT 维护）
- [sync-plan.md](docs/sync-plan.md), 跨仓同步点（互补关系）

---

## 当前活跃 Changes（Stage 3）

> **来源**: [openspec/changes/INDEX.md](openspec/changes/INDEX.md)

| Change | 优先级 | 规模 | 当前进度 | 描述 |
|--------|--------|------|---------|------|
| `cuda-e2e-real-path` | 🔴 P1 | 64 tasks, ~15h | ✅ COMPLETED (2026-07-20) | CUDA 全链路: alloc→memcpy→launch→sync，双仓实施 |
| `three-sanitizer-infra` | 🟡 P3 | 34 tasks, 3-5d | ✅ COMPLETED (2026-07-17) | ASan/UBSan/TSan CMake infra + CI + 文档，34/34 tasks 已归档 |
| `kfd-l1-l2-bridge-e2e` | 🟡 P3 | 41 tasks, 1-2w | ✅ COMPLETED (2026-07-18) | 跨仓 KFD bridge E2E 验证，双仓已归档 (ADR-035 §Rule 5.1) |

### 近期里程碑（2026-07）

| 日期 | 事件 |
|------|------|
| 07-05 | Stage 2 达成 — 多设备插件化 76/76 ctest |
| 07-16 | C-12 KFD 多文件集成归档 — 81% 原子任务, 104/104 ctest |
| 07-16 | three-sanitizer-infra + kfd-l1-l2-bridge-e2e 创建 (C-12 follow-up) |
| 07-17 | ASan/UBSan/TSan 三 sanitizer 落地, CI matrix unified |
| **07-20** | **CUDA E2E real-path 全部 6 Phase ✅** — BO 真实内存 + Puller MEMCPY HAL + fence 异步 + E2E 测试 |
| 07-20 | ADR-064 内存模型分阶段策略 + ADR-023 HAL 边界强制执行规则 |
| 07-20 | 回归: docs-audit 43/43 + 104/104 + 14/14 ctest PASS |
| **07-21** | **three-sanitizer-infra ✅** — 34/34 tasks 归档确认；**kfd-l1-l2-bridge-e2e ✅** — 双仓归档确认 |
| 07-21 | Arch-handoff + roadmap 数据同步：移除已完成的 Stage 3 子任务 |

### 后续任务建议

```
1. 归档 cuda-e2e-real-path (openspec archive)
2. Stage 3.2 性能优化 (benchmark 基线 → hotpath)
3. Stage 3.3 错误处理审计 (全路径 Linux 错误码审计)
4. Stage 3.4 文档完善 (Doxygen API 参考 + docs-audit 43/43 PASS)
5. v1.0 发布清单 (Release notes + Migration guide + Binary release)
```

---

## 维护说明

本路线图文件由 UsrLinuxEmu Architecture Team 维护。任何阶段状态变更、原则修正、目标调整需走 ADR 流程（见 ADR-035）。

阶段文件 (`stage-*.md` / `blueprint.md`) 与 OpenSpec change 解耦：阶段描述"做什么"与"为什么"，OpenSpec change 描述"如何做"与"何时做"。两者通过 ADR 关联，不通过编号绑定。

---

**对应 ADR**: ADR-035 (governance) + ADR-036 (3-way principle)
**对应 SSOT 章节**: §1.10
**OpenSpec 状态**: 不绑定（无 change 编号）
