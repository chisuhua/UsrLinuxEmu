# 终态蓝图 — 3 区分架构的成熟形态

> **性质**: 终态愿景描述（what "done" looks like）
> **对比**: 与 [SSOT §1.10](../02_architecture/post-refactor-architecture.md) 互补 — SSOT 描述当前实现，本文描述成熟形态
> **关联原则**: [ADR-036](../00_adr/adr-036-three-way-separation.md) (✅ Accepted)
> **最后更新**: 2026-06-23

---

## 蓝图总览

成熟态的 UsrLinuxEmu 应满足：

1. **驱动可移植性达成**: 在 UsrLinuxEmu 开发的驱动 .c 文件能直接拷贝到 `drivers/gpu/xxx/` 下编译通过（除 #include 路径调整）
   **限定**：移植性限于 C-12 [kfd-abi-comparison-report.md §2](../05-advanced/kfd-abi-comparison-report.md) 字段白名单范围内（~38 必需字段）。完整 amdgpu KFD ABI 对齐归入蓝图后 work（per ADR-059 D4 scope boundary）。
2. **真实内核驱动可运行**: 编译并运行 KFD/amdgpu/NV 子集，至少 5 个核心 ioctl 跑通
3. **多设备类型支持**: 至少 3 种设备类型（GPU + 网络 + 存储）能加载并响应 syscall
4. **v1.0 稳定**: CI 全平台绿、性能基准达标、文档审计 36/36 PASS

---

## 3 区分成熟形态

### ① Linux 内核环境模拟（成熟态）

**能力清单**:
- 完整 VFS + 调度 + IOMMU + mmu_notifier + DRM + PCIe + 中断子系统
- Linux 内核 API 覆盖率 ≥ 80%（常用 API）
- DRM 后端支持 KFD 所需的所有 ioctl（≥ 30 个）
- IOMMU 支持 ATS + Device-TLB
- UVM/HMM 支持 SVM（Shared Virtual Memory）
- mmu_notifier 支持 page fault 注入

**验收**:
- KFD 编译 errors = 0
- 5 个核心 KFD ioctl 跑通
- lspci 在 UsrLinuxEmu 内可见模拟设备

### ② 可移植的驱动代码实现（成熟态）

**能力清单**:
- 驱动代码用 `struct drm_device`, `struct file`, `drm_ioctl_desc[]` 等真实 Linux 内核 API 写
- 所有硬件访问通过 HAL（`hal_*` 函数指针），不直接调 sim
- HAL 覆盖率 ≥ 14 个 ops（已有的）+ 蓝图所需的额外 ops
- 多设备驱动示例（GPU + 网络 + 存储）

**验收**:
- 驱动的 .c 文件能直接拷贝到 `drivers/gpu/xxx/` 编译
- 仅 #include 路径需要调整
- 驱动代码无任何 `#ifdef CONFIG_USRLINUXEMU`

### ③ 硬件模拟（成熟态）

**能力清单**:
- 模拟主流 GPU 行为（pushbuffer 执行、ring buffer、interrupt、reset、power）
- 模拟网络设备（NIC packet buffer、interrupt on arrival）
- 模拟块设备（基于 host 文件的 disk emulator）
- 性能可参考真实硬件（在合理误差范围内）
- **Stage 4: BO 内存路径通过 ioremap/BAR 模拟**（独立 VRAM backing store + `readl`/`writel` MMIO + `dma_alloc_coherent`；见 [ADR-064](../00_adr/adr-064-memory-model-staging.md) Decision 3）
- **GPU 命令处理器完整实现**（Phase 4-7 per [ADR-040~057](../00_adr/README.md)）：图启动真实化、方法编解码、HyperQueue 调度、抢占/上下文切换、跨引擎同步、AQL/PM4、Green Context/PDL（详见 [stage-4-bar-ioremap.md](stage-4-bar-ioremap.md) §4.2-4.6）

**验收**:
- 模拟设备能运行真实工作负载（非 trivial benchmark）
- 性能回归测试纳入 CI

### HAL 桥接层（成熟态）

**能力清单**:
- `struct gpu_hal_ops` 扩展到覆盖 KFD 所有硬件交互
- `hal_user.cpp` 真机部署路径稳定
- `hal_mock.cpp` 用户态模拟路径稳定
- HAL 接口变更走 ADR 流程

**验收**:
- HAL 接口变更 100% 有 ADR 记录
- 真机部署路径测试覆盖

---

## 与 SSOT §1.10 的关系

| 维度 | SSOT §1.10 | 本蓝图 |
|------|-----------|--------|
| 时间视角 | **当前**实现状态 | **终态**愿景 |
| 内容性质 | 事实（已实现的）| 目标（将要达成的）|
| 维护规则 | 每次 change 归档后必须更新（ADR-035 Rule 4.2）| 阶段完成时更新（Stage 0/1/2/3）|
| 责任方 | 架构师记录现状 | 路线图持有者描述未来 |

**两者职责分离，互不重复**。

---

## 蓝图验收（阶段 3 完成时）

- [ ] 借鉴 KFD 风格、`drv/kfd/` 子目录下用 Linux kernel idioms 编写的 .c 文件零修改可编译进真实内核模块（per ADR-036）
      **限定**：字段白名单范围见 [kfd-abi-comparison-report.md §2](../../05-advanced/kfd-abi-comparison-report.md)。超出白名单的字段需走扩展流程（per ADR-059 D4 scope boundary）。
- [ ] KFD 5 个核心 ioctl 在 UsrLinuxEmu 内跑通
- [ ] 多设备插件（GPU + 网络 + 存储）全部能加载
- [ ] CI 全平台绿（Linux x86_64 + aarch64）
- [ ] docs-audit 36/36 PASS
- [ ] v1.0 release 完成
- [ ] 用户 quickstart ≤ 15 分钟

---

## 非可达愿景（明确不在蓝图内）

为保持蓝图诚实，以下愿景**明确不在范围内**：

- ❌ **完整模拟真实 GPU 指令集执行**: sim 仅模拟行为，不实现 shader 执行
- ❌ **零性能开销**: 用户态模拟有不可避免的开销，文档明示
- ❌ **100% Linux API 兼容**: 仅覆盖 ≥ 80% 常用 API
- ❌ **macOS/Windows 原生部署**: 部署目标仅 Linux
- ❌ **完整 amdgpu 驱动移植**: 仅移植 KFD 子集（最高 ROI）

---

## 跨引用

- [ADR-036](../00_adr/adr-036-three-way-separation.md) — 3 区分架构原则
- [SSOT §1.10](../02_architecture/post-refactor-architecture.md) — 3 区分的当前实现
- [stage-0-mvp.md](stage-0-mvp.md) — 当前 MVP 状态
- [stage-1-kernel-emu.md](stage-1-kernel-emu.md) — 阶段 1（Linux 内核环境）
- [stage-2-multi-device.md](stage-2-multi-device.md) — 阶段 2（多设备）
- [stage-3-v1.0.md](stage-3-v1.0.md) — 阶段 3（v1.0 稳定）

---

**蓝图状态**: 📋 愿景描述
**预计达成**: 阶段 3 完成时（时间待定）
