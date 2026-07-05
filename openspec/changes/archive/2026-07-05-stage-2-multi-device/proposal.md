# Change: stage-2-multi-device

> **状态**: 🔄 APPLIED（已完成实施 + 已合入 main）→ 待 archive
> **创建**: 2026-07-05
> **来源**: Stage 1.4 Tier-2 完成（commit `b521f29`，2026-07-05）+ roadmap §2 启动
> **关联 SSOT**:
>   - [docs/roadmap/stage-2-multi-device.md](../../docs/roadmap/stage-2-multi-device.md)
>   - [docs/02_architecture/post-refactor-architecture.md §1.10](../../docs/02_architecture/post-refactor-architecture.md)
>   - [ADR-036 3 区分架构](../../docs/00_adr/adr-036-three-way-separation.md)
>   - [ADR-038 network stack 3 区分边界](../../docs/00_adr/adr-038-network-stack-three-way-separation.md)
> **关联 plan**:
>   - [docs/superpowers/plans/2026-07-05-stage-2-multi-device.md](../../docs/superpowers/plans/) (Oracle-revised v2)
>   - [docs/superpowers/plans/2026-07-05-stage-2-spike.md](../../docs/superpowers/plans/) (GO/NO-GO spike)

## Why

### 阶段 1 完成 → 阶段 2 启动条件

Stage 1（DRM + UVM/HMM + IOMMU + ATS + PCIe BAR/中断）已全部交付；Stage 1.4 Tier-2（KFD runtime penetration）已于 2026-07-05 完成。Roadmap §2 多设备插件化的启动条件全部满足。

### Tier-2 deferred items absorption

Stage 1.4 Tier-2 (commit `b521f29`) 显式延后到 Stage 2 的 4 项（per kfd-portability-boundary.md §5.2）：

1. **IOMMU 真实硬件 invalidation**（需 vfio）→ Stage 2.1.1 ✅
2. **mmu_notifier 真实进程模型**（需真实 mm struct）→ Stage 2.1.2 ✅
3. **ATS PRI/PRG response routing**（需 PCIe 4.0+ 设备）→ Stage 2+ 条件性（仍未触碰）
4. **MAP_QUEUE_RING happy path segfault** (Phase 2.5) → Stage 2.0.1 ✅

### 3 区分原则横向验证

Stage 0-1 仅在 GPU 设备上验证 3 区分架构原则（per ADR-036）。Stage 2 横向扩展到网络 + 存储设备，验证该原则的**通用性**。

## What Changes

### 新增 capability `3-way-separation-network-storage`

- 网络设备（net_driver）+ 存储设备（storage_driver）插件，遵循 3 区分
- 网络子任务（§2.1）和存储子任务（§2.2）并行实施

### 新增 capability `tier2-vfio-absorption`

- vfio_bridge.cpp/h 实现 `/dev/vfio` 用户态绑定
- 升级 `iommu_flush_iotlb` 为调用 `ioctl(VFIO_IOMMU_UNMAP_DMA)` 触发 host kernel invalidation

### 新增 capability `tier2-mm-shim`

- mm_shim.cpp/h 提供真实 `mm_struct` 跟踪（PID + VMA list）
- 替换 `SimPageFaultHandler` 匿名 struct

### 新增 capability `hotfix-map-queue-ring`

- 修复 `GpgpuDevice::handleMapQueueRing` Phase 2.5 segfault
- Tier-2 报告中标注的限制解除

### 新增 ADR `ADR-038` (network stack 3 区分)

- Network stack 3 区分边界定义
- 明确 net_device_ops 子集 + HAL 桥接方式

### 新增 CI hooks (Phase I CI-2)

- docs-audit hook for Stage 2 paths
- 防止文档回归

## Impact

### Code 规模

| 类别 | 新增 | commit 数 |
|------|------|----------|
| ADR | docs/00_adr/adr-038 | 1 |
| Spec/Plan/Spike | docs/superpowers/plans/ + docs/05-advanced/ | 4 |
| Kernel compat (①) | include/kernel/net + src/kernel/net, include/kernel/block + src/kernel/block, include/kernel/uvm/mm_shim + src/kernel/uvm/mm_shim, src/kernel/iommu/vfio_bridge | 5+ |
| Drivers (②) | plugins/net_driver/drv/, plugins/storage_driver/drv/ | 2 |
| Sim (③) | plugins/net_driver/sim/, plugins/storage_driver/sim/ | 2 |
| Tests | tests/test_net_driver_standalone.cpp, tests/test_storage_driver_standalone.cpp, tests/test_socket_skbuff_standalone.cpp | 3 |
| CI | pre-commit hook updates | 1 |
| **Total** | | **~20 commits** |

### 测试影响

- baseline: 73/73 (Stage 1.4 Tier-2 完成)
- after Stage 2: **76/76 PASS**
- 新增: test_net_driver_standalone (6 cases / 34 assertions), test_storage_driver_standalone (5 cases / 24 assertions), test_socket_skbuff_standalone

### ctest 演进

| 阶段 | tests |
|------|------|
| Stage 1.4 Tier-2 (commit b521f29) | 73 |
| + Stage 2.1.1 (vfio) | (no new test) |
| + Stage 2.1.2 (mm_shim) | (no new test) |
| + Stage 2.2.1-2.2.5 (net) | +2 (test_socket_skbuff + test_net_driver) |
| + Stage 2.3.1-2.3.4 (storage) | +1 (test_storage_driver) |
| **Stage 2 total** | **76/76 PASS** |

## Launch Conditions

- [x] **LC1**: Stage 1 完成（Tier-1 + Tier-2 commit `b521f29`）
- [x] **LC2**: 3 区分架构原则定义（ADR-036）
- [x] **LC3**: Roadmap §2 定义完整（stage-2-multi-device.md）
- [x] **LC4**: Worktrees 创建（`stage-2-0-adr-037` + `stage-2-1-tier2-absorption` + `stage-2-2-network`）
- [x] **LC5**: 集成 plan 评审（docs/superpowers/plans/Oracle-revised v2）
- [x] **LC6**: GO/NO-GO spike (`9d90cfe` Stage 2.0 Tier-2 feasibility spike)
- [x] **LC7**: Tier-2 baseline regression 73/73 PASS（commit `b521f29`）

## Out of Scope (explicitly excluded)

按 kfd-portability-boundary.md §5.2 + Stage 2 不做项：

| 排除项 | 原因 | 推荐延后阶段 |
|--------|------|-------------|
| 多文件 KFD 集成（kfd_module.c / kfd_device.c / kfd_process.c / kfd_doorbell.c）| 53+ amdgpu headers 阻塞，~50K 行 amdgpu driver 需移植 | Stage 3+ 独立子项目 |
| 完整 kfd_queue.c queue 生命周期 | 上游原文件后段函数需 amdgpu_* 依赖 | 随多文件集成延后 |
| ATS PRI/PRG response routing | 需 PCIe 4.0+ 设备；当前模拟设备不依赖 ATS | Stage 2+ 条件性（仍未触碰）|
| 真实网络协议栈（TCP/IP）| Stage 2 仅 L2 Ethernet；TCP/IP 子集优先级低 | 独立子项目 |
| 请求队列（request queue）| Stage 2 存储设备简化版仅 read/write，不实现 request queue | Stage 3+ |
| mmu_notifier 真实进程模型替换 | Stage 2.1.2 实现 mm_shim，但完全替换 SimPageFaultHandler 留待 Stage 3 | Stage 3 |

## Caveats / Known Limitations

1. **vfio 需要 root + IOMMU group**: Stage 2.1.1 实现 `/dev/vfio` 绑定但不能在普通笔记本上运行，需要 root 权限 + 支持 IOMMU 的硬件。在用户态 opt-in 模式。
2. **mm_shim 是 shim 不是完整 mm**: Stage 2.1.2 提供 PID + VMA list 跟踪，但仍是简化版。完整 `mm/mmu_notifier.c` 用户态模拟需要 ~1000 行。
3. **net_driver 仅 L2 Ethernet**: 不实现 TCP/IP，用户态无法直接使用 `socket()`/`bind()`/`connect()` 系统调用，仅 `/dev/net0` 设备节点 API。
4. **storage_driver 仅 read/write**: 不实现 bio/request queue 抽象，仅最小化块 I/O。
5. **G1-G4 边界契约未破坏**: Stage 2 实施未触及 Stage 1.2 锁定的 drm_device 生命周期 / BO 引用计数 / prime 释放顺序 / fence 触发时机。`test_uvm_drm_lifecycle` 仍 PASS。
