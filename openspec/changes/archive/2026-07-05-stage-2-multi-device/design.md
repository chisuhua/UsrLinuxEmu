## Context

Stage 2 of UsrLinuxEmu's roadmap (after Stage 1.4 Tier-2 closeout at commit `b521f29`) extends the 3-way separation architecture (per ADR-036) from GPU-only to **network + storage device classes**. Additionally, Stage 2 absorbs two Tier-2 deferred items from kfd-portability-boundary.md §5.2 (vfio real IOTLB invalidation + mm_shim real process model) since both are necessary infrastructure for multi-device operation.

**Constraint background**:
- GPU 设备已验证 3 区分架构 (Stage 0-1)
- 4 项 Tier-2 deferred items 必须在 Stage 2+ 处理（否则 KFD 多文件集成评估无法启动）
- Roadmap §2 + §3 阶段 0 完全达成（commit `b521f29`，2026-07-05）

**Key stakeholder concerns**:
- 设备驱动开发：希望能够开发非 GPU 设备驱动，并在 UsrLinuxEmu 中验证
- KFD 项目路径：依赖 vfio + mm_shim 真实化作为下一阶段前置
- 3 区分原则验证：横向验证原则通用性

## Goals / Non-Goals

**Goals (G1-G6)**:

- **G1**: Network device plugin (net_driver.so) 加载后 `/dev/net0` 可见，响应 L2 send/recv
- **G2**: Storage device plugin (storage_driver.so) 加载后 `/dev/sda` 可见，read/write 持久化
- **G3**: ADR-038 网络栈 3 区分边界建立（net_device_ops + HAL bridge）
- **G4**: vfio 真实硬件 invalidation 集成（Tier-2 deferred 第 1 项吸收）
- **G5**: mm_shim 真实进程模型（Tier-2 deferred 第 2 项吸收）
- **G6**: G1-G4 (Stage 1.2 边界契约) 全部保持，0 regression

**Non-Goals (NG1-NG5)**:

- **NG1**: TCP/IP 协议栈完整实现（仅 L2 Ethernet）
- **NG2**: Block request queue 抽象（仅 read/write）
- **NG3**: Multi-process scheduling（mm_shim 仅 PID + VMA 单进程跟踪）
- **NG4**: 多文件 KFD 集成（Stage 3+ 独立子项目）
- **NG5**: ATS PRI/PRG response routing（Stage 2+ 条件性，PCIe 4.0+ 设备未到位）

## Decisions

### D1: 3 区分原则横向扩展（网络/存储）

**设计**：每个新设备类严格遵循 ① Linux 内核环境模拟（`src/kernel/{net,block}/`） + ② 可移植驱动代码（`plugins/{net,storage}_driver/drv/`） + ③ 硬件模拟（`plugins/{net,storage}_driver/sim/`）+ HAL 桥接。

**为什么不重写**：GpgpuDevice / GpuQueueEmu 等成熟组件已经验证 3 区分原则可工作；网络/存储仅需水平复制相同模式。

**拒绝的替代方案**：直接在 ② 层实现完整 L2/L3 协议栈——会与 ③ layer 重复实现，破坏 3 区分原则。

### D2: 网络设备 HAL 桥接模式（per ADR-038）

**设计**：`net_device_ops` 子集通过函数指针表暴露，net_driver/drv 调用 sim/NIC 实现：

```cpp
struct net_device_ops {
  int (*open)(struct net_device *);
  int (*stop)(struct net_device *);
  int (*start_xmit)(struct sk_buff *, struct net_device *);
  int (*set_mac_address)(struct net_device *, void *);
};
```

**为什么不重新设计 net_device**：参考 Linux 6.12 LTS 真实 struct，但仅暴露子集（sub-set 优先）。

### D3: 存储设备简化模型（read/write only）

**设计**：`storage_driver/drv/` 直接基于 `read/write` syscall 兼容层（`src/kernel/block/bio_compat.cpp`），不实现 request queue 抽象。

**为什么简化**：Stage 2 目标是验证 3 区分原则横向扩展，bio/request queue 是 Linux 5.x+ 内核的复杂抽象，但 UsrLinuxEmu 用户态模拟无 DMA 调度，bio 抽象收益不大。

### D4: vfio 用户态 opt-in 模式

**设计**：`src/kernel/iommu/vfio_bridge.cpp` 仅在用户显式调用 `vfio_bridge_enable()` 时尝试绑定 `/dev/vfio`。在普通笔记本上：
- 调用无效果 + 警告 log
- 不破坏 fallback 到纯用户态模拟

**为什么 opt-in**：vfio 绑定需要 root + 支持 IOMMU 的硬件；在大多数开发机器上无效。opt-in 允许在有 root 的 CI / bare-metal 机器上启用。

### D5: mm_shim 是 shim 不是完整 mm

**设计**：`src/kernel/uvm/mm_shim.cpp` 提供：
- `mm_shim_register(pid, mm)` — 注册 PID + mm
- `mm_shim_lookup_vma(mm, addr)` — 简化的 VMA 查找
- `mm_shim_track_page_fault(pid, addr)` — 记录 page fault

**为什么 shim**：完整 `mm/mmu_notifier.c` 用户态模拟 ~1000 行；Stage 2.1.2 目标是 Tier-2 触发的最小需求，完整替换推迟到 Stage 3。

### D6: 测试策略（runtime tests per Stage 2.2/2.3）

**设计**：
- `test_socket_skbuff_standalone` — 验证 ① compat 层 socket + sk_buff
- `test_net_driver_standalone` (6 cases / 34 assertions) — 验证 ② ③ 集成
- `test_storage_driver_standalone` (5 cases / 24 assertions) — 验证 存储插件

## Risks / Trade-offs

### R1: 3 区分原则可能在网络/存储场景不收敛

**风险**：网络栈深度抽象（TCP/IP）对 3 区分可能不友好。

**缓解**：Stage 2 仅 L2 Ethernet（最简单情况），验证原则本身。TCP/IP 子集作为 Stage 3+ 独立项目。

### R2: vfio opt-in 导致某些环境无法利用

**风险**：用户可能在没有 root 的 CI 上运行，vfio path 不可用。

**缓解**：保持纯用户态 fallback（Tier-2 现有实现）。Tier-2 path 总可工作，vfio 仅为可选增强。

### R3: mm_shim 不完整导致 page fault 跟踪不准

**风险**：简化版 mm_shim 不处理所有 `mm_struct` 关系（如多线程 VMA 共享）。

**缓解**：Stage 2.1.2 仅在 KFD Tier-2 path 调用 page fault 时有效；其他路径维持 Tier-2 实现。完整 mm 在 Stage 3。

### R4: 6 commits 在 main 上的并行 worktree 集成

**风险**：worktree 分支与 main 分叉可能导致 merge 冲突。

**缓解**：3-way merge 已无冲突（commit `fb75ed2` 验证）；后续 OpenSpec + archive 流程单一。

## Open Questions

1. **OQ1**: Phase 2.5 segfault 修复 (commit `6d090e6`) 是否需要回看 Stage 1.4 baseline？→ 否（Tier-2 报告已诚实记录）
2. **OQ2**: mm_shim 是否需要在 Stage 3 扩展为完整 mm？→ 是（Stage 3 子项目）
3. **OQ3**: net_driver 是否需要 TCP/IP 子集？→ 否（Stage 3+ 独立项目）
4. **OQ4**: storage_driver 是否需要 bio/request queue 抽象？→ 否（Stage 3+ 独立项目）
5. **OQ5**: TaskRunner Phase 3 跨仓协调请求如何响应？→ 评估 `2026-07-05-sim-stream-primitive-support` change；不在本 change 范围
