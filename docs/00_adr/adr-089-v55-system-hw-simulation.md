# ADR-089: v5.5+ sim_hardware/ 仿真范围扩展（VFIO / IOMMUFD / Live Migration / vDPA）

**状态**: ✅ **Accepted**（2026-08-16，Oracle v0.6 复审 PASS（per Stage 5.5.1 实施后 Gate D），4 项 Minor 修订 + 2 项 OQ 裁决合入 v0.5）
**日期**: 2026-08-16
**版本**: v0.5（✅ Accepted — 4 Minor + 2 OQ 合入）
**提案人**: UsrLinuxEmu Architecture Team
**评审者**: Oracle（session `ses_*` 2026-08-16 v0.4 复审 PASS）+ Architecture Team（v0.5 Approved 2026-08-16）
**关联 ADR**:
- [ADR-088](../00_adr/adr-088-dgpu-complete-simulation.md) ✅ Accepted — dGPU 参考设计（明确提出 `sim_hardware/` 概念 + §Open Questions 把 VFIO / IOMMUFD 列为 v5.5+ 评估）
- [ADR-023](adr-023-hal-interface.md) ✅ — HAL 接口契约（68 fn-ptrs append-only 治理）
- [ADR-036](adr-036-three-way-separation.md) ✅ — 3 区分架构原则
- [ADR-061](adr-061-hal-iommu-extension.md) ✅ — HAL IOMMU ops 扩展（hal_iommu_map/unmap）
- [ADR-060](adr-060-message-notification-threading.md) ✅ — kernel_workqueue 线程架构
- [ADR-027](adr-027-linux-compat-strategy.md) ✅ — Linux 兼容层扩展策略（spec-driven）
- [ADR-035](adr-035-governance-policy.md) ✅ — 治理规则
**关联调研**:
- [docs/05-advanced/system-hw-survey-2026-08-16.md](../05-advanced/system-hw-survey-2026-08-16.md) — **主调研报告（可信度 ⭐⭐⭐⭐ 极高）**
- [docs/05-advanced/vfio-live-migration-research.md](../05-advanced/vfio-live-migration-research.md) — Live Migration 独立深度报告（425 行 18KB）
- 4 个 librarian 调研 session（VFIO / IOMMUFD / Live Migration / vDPA，2026-08-16 完成）
**关联 Change**: （待创建）`openspec/changes/2026-MM-DD-system-hw-v55/`

---

## Context

### C1: 现状盘点

**ADR-088 [✅ Accepted]** 提出了 `sim_hardware/` 概念（**§C2 拓扑对齐**），但**仅设计 v1.0 范围**：

```
sim_hardware/
├── iommu/        # 5 函数（domain_alloc / attach_dev / map / unmap / iova_to_phys）
└── cxl_memdev/   # 4 函数（memdev_read / memdev_write / pmem_init / flush）
```

**ADR-088 §Open Questions** 明确把以下 4 项列为 **v5.5+ 评估**：

| # | 子系统 | ADR-088 原文 | 本 ADR 处置 |
|---|--------|------------|------------|
| 1 | **VFIO / IOMMUFD** | "VFIO 主要是 user-mode 访问（KVM/QEMU），与 dGPU driver 关系较弱；v5.5+ 评估" | **v5.5.1-v5.5.2 核心目标** |
| 3 | **嵌套 IOMMU domain** | "复杂度高；v5.5+ 评估" | **v5.5.2 P2**（IOMMUFD viommu/vdevice 范围内） |
| 4 | **CXL.cache** | "CXL 1.1+ 特性；v5.5+ 扩展" | **P3 延后**（per ADR-088 §D8，不纳入 v5.5+ 实施） |
| 7 | **dGPU → CXL.mem fabric 路径** | "仅仿真 CPU/driver 侧访问；dGPU 经 CXL/PCIe fabric 直接访问 CXL.mem 的路径**不仿真**；如需则 v5.5+ 评估" | **P3 延后**（per ADR-088 §D8，不纳入 v5.5+ 实施） |
| (—) | **CXL 2.0+ HDM** | ADR-088 §D8 表格（非 Open Questions）；"Host-managed Memory；v5.5+ 评估" | **P3 延后**（per ADR-088 §D8） |

> **Oracle 评审修正（v0.2 → v0.3）**：v0.1 草案曾错误引用 ADR-088 OQ #5（"23 ABI 维护"）作为 CXL 2.0+ HDM 来源；实际 ADLineR-088 OQ #5 是 ABI 维护，CXL 2.0+ HDM 来自 §D8 表格"范围外（不含）"。本 ADR 必须明确区分 OQ（带"v5.5+ 评估"标记的实际问题）与 §D8 范围外清单。

**项目已有基础**（实地探索确认）：
- `src/kernel/iommu/` 1262 行传统 IOMMU 仿真（Stage 1.1 已交付）
  - `iommu_domain.cpp` / `iommu_group.cpp` / `dma_remap.cpp` / `ats_protocol.cpp` / `ioasid.cpp` / `invalidate.cpp` / `pcie_integration.cpp` / `iommu_emu_state.cpp`
  - **`vfio_bridge.cpp`**（67 行）— **仅桥接真实 Linux `/dev/vfio/vfio`**（harness 仿真，不是 VFIO 仿真）
- `include/linux_compat/iommu/` 4 个头文件（iommu.h / iommu_domain.h / iommu_group.h / ioasid.h）
- `include/linux_compat/pci/` 3 个头文件（ats.h / msi.h / pci.h）
- `kernel_workqueue` + `kernel_thread_base`（per ADR-060）— async dispatch 基础
- HAL 68 fn-ptrs（per ADR-023 append-only）— 治理稳定

### C2: 4 个 Linux 子系统的最新演进（librarian 调研确认）

**调研依托 4 个 background task**（model: `minimax-coding-plan/MiniMax-M3`，2026-08-16 完成）：
- `bg_58ae2170` — VFIO 演进（Linux 6.14 文档 + GitHub 源码，3m26s）
- `bg_b17d8e06` — IOMMUFD 框架（v6.2-v6.14，4m36s）
- `bg_177fd25d` — Live Migration（v5.0-v6.9，5m27s）— 生成了独立深度报告 [vfio-live-migration-research.md](../05-advanced/vfio-live-migration-research.md)
- `bg_dd7459af` — vDPA 框架（v5.7-v6.x，3m16s）

**6 个关键事实 V1-V6 全部验证完成**（见调研报告 §0.2）— 可信度 ⭐⭐⭐⭐ 极高。

### C3: 仿真价值分析

| Use case | 用户价值 | 优先级 |
|----------|---------|--------|
| **GPGPU 驱动 + VFIO mdev** | amdgpu mdev / NVIDIA vGPU 开发 | 高 |
| **IOMMUFD + SVA/SVM** | GPU 共享 CPU 虚拟地址（per ADR-088 §C4）| 高 |
| **Live Migration（VM 迁移）** | GPGPU 实例跨主机迁移 | 中 |
| **vDPA 数据路径加速** | SR-IOV 网络 / 块设备直通 | 中 |

### C4: 关键设计约束

1. **ADR-036 3 区分原则**：`sim_hardware/` 属 ③ 硬件模拟层
2. **ADR-023 append-only**：HAL 68 fn-ptrs 不修改；v5.5+ 仿真若需 HAL 桥接，新 fn-ptr 走 ADR 流程
3. **ADR-088 §C2 拓扑对齐**：仿真拓扑与真硬件一致（VFIO 设备 = dGPU 板卡，系统 IOMMU 在 system_hw）
4. **ADR-027 spec-driven**：`linux_compat/` 增量补齐（不强求同步）
5. **ADR-035 治理规则**：本 ADR 自身走此规则
6. **范围约束**：v5.5+ **不仿真 KVM / 真实硬件 IOMMU**（per ADR-088 §Open Questions）

---

## Decision

### D1: v5.5+ 仿真范围（4 大子系统）

按核心度划分 P0/P1/P2：

| 子系统 | P0（必须） | P1（重要）| P2（可选）|
|--------|------------|----------|----------|
| **VFIO** | 核心 `vfio_device` + cdev + PCI region + MSI cap | IOMMUFD 集成 + vfio-user 协议 | mdev |
| **IOMMUFD** | `iommufd_ctx` + IOAS + HWPT + device + **P0+P1 uapi 命令全集**（v6.2 核心 ~10 + v6.3+ 扩展 ~8 = **~18-20 个**；详见 `IOMMUFD_CMD_*` 完整清单） | PASID attach/detach | v6.5+ viommu/vdevice |
| **vDPA** | `vdpa_sim_net` 移植 + vhost-vdpa ioctl | netlink 控制接口 | vdpa_sim_blk |
| **Live Migration** | V2 状态机 + data_fd | dirty tracking | PRE_COPY |

### D2: 4 阶段实施路线图（v5.5.1-v5.5.4）

#### **阶段 v5.5.1：VFIO 核心仿真（6-8 周，UsrLinuxEmu 团队主导）**

**目标**: 实现 `/dev/vfio/vfio` 字符设备 + VFIO PCI 子设备 + 完整 `vfio_device_ops` 派发

**Owner 论证**（per ADR-088 §C2 范围切分）：
- ADR-088 §C2 明确：`sim_hardware/` 属 UsrLinuxEmu 维护，CppTLM 仅仿真 dGPU 板卡（23 ABI）
- VFIO 核心仿真位于 `sim_hardware/vfio/`，属系统级硬件 → **UsrLinuxEmu 主导**
- ADR-088 §Open Questions #6 已明确 CppTLM 容量是风险（5-7 周尚需确认承诺），不应再加 6-8 周 VFIO 负担
- CppTLM 团队范围限定为 dGPU 板卡仿真（v5.5.1 期间 CppTLM 可提供 ABI 稳定咨询，但实施主体在 UsrLinuxEmu）

**新增模块**（`sim_hardware/vfio/`）：
- `vfio_device.cpp` — `vfio_device` 抽象 + `vfio_device_ops` 完整回调表
- `vfio_ops.cpp` — ioctl 派发（基于 UsrLinuxEmu 现有 `Device::fops` 派发机制）
- `vfio_char.cpp` — `/dev/vfio/vfio` 字符设备（open/ioctl/read/write）
- `vfio_pci.cpp` — 9 个 PCI region（BAR0-5 + ROM + CONFIG + VGA）+ MSI capability 虚拟化
- `vfio_user.cpp` — vfio-user 协议（**P2 范围**；v5.5.1 仅预留接口）

**新增 linux_compat**：
- `include/linux_compat/vfio.h` — VFIO API 表面（`vfio_device_info` / `vfio_region_info` / `vfio_irq_info` struct）
- `include/linux_compat/vfio_pci.h` — VFIO PCI region 索引（pci.h 已经存在）

**关键 ioctl 仿真**（P0）：
```
VFIO_GET_API_VERSION / VFIO_CHECK_EXTENSION / VFIO_SET_IOMMU
VFIO_DEVICE_GET_INFO / VFIO_DEVICE_GET_REGION_INFO / VFIO_DEVICE_GET_IRQ_INFO
VFIO_DEVICE_SET_IRQS / VFIO_DEVICE_RESET
```

**验收 Gate 1**：
- 真实 VFIO 驱动代码（amdgpu 子模块）能编译运行
- 实测 `lspci -v` 在 UsrLinuxEmu 内可见 VFIO 设备
- 真实 `vfio-pci` driver 可 attach 到 UsrLinuxEmu 仿真的 dGPU

#### **阶段 v5.5.2：IOMMUFD 仿真（6-8 周）**

**目标**: 实现 v6.2+ IOMMUFD 完整 ioctl + 对象管理 + 与现有 `src/kernel/iommu/` 集成

**新增模块**（`sim_hardware/iommufd/`）：
- `iommufd_core.cpp` — `iommufd_ctx` 单例 + 对象 xarray 管理
- `iommufd_ioas.cpp` — IOAS 仿真（IOVA 映射）
- `iommufd_hwpt.cpp` — HWPT 仿真（关联 `iommu_domain`）
- `iommufd_device.cpp` — device 绑定
- `ioctl_handlers.cpp` — **P0+P1 共 ~18-20 个** uapi 命令处理（v6.2 核心 + v6.3+ 扩展；完整清单见调研报告 §3）
- `pasid_manager.cpp` — PASID 分配（v6.3+）

**P0 ioctl 仿真**（v6.2 核心）：
```
IOMMUFD_CMD_DESTROY (0x80) / IOAS_ALLOC (0x81) / IOAS_MAP (0x85) / IOAS_UNMAP (0x86)
IOMMUFD_CMD_HWPT_ALLOC (0x89) / DEVICE_ATTACH / DEVICE_DETACH
```

**P1 ioctl 仿真**（v6.3+）：
```
IOMMUFD_CMD_IOAS_ALLOW_IOVAS (0x82) / DEVICE_PASID_ATTACH
IOMMUFD_CMD_IOAS_MAP_FILE (0x8f) / IOMMUFD_CMD_VDEVICE_TSM_OP (0x95)
```

**与现有 1262 行 IOMMU 代码集成**（**双栈并存**，per 调研报告 §6 设计）：
- `iommu_domain` 作为 `iommufd_ioas` 的 backing store
- 现有 `iommu_map/unmap` API 保留（向后兼容）
- IOMMUFD 是**新路径**，不替换传统 API

**验收 Gate 2**：
- 真实 iommufd 库测试（`tools/testing/iommufd/`）能跑过
- 实测 amdgpu KFD 通过 IOMMUFD attach
- 1M 次 IOAS map/unmap 延迟 < 10 ms

#### **阶段 v5.5.3：vDPA 仿真（4-6 周）**

**目标**: 实现 `vdpa_sim_net` 移植 + vhost-vdpa 字符设备 + vDPA 设备生命周期

**新增模块**（`sim_hardware/vdpa/`）：
- `vdpa_device.cpp` — `vdpa_device` 抽象 + `vdpa_config_ops` 完整实现
- `vdpa_sim_net.cpp` — **直接移植 `drivers/vdpa/vdpa_sim/vdpa_sim_net.c` 源码**（调研报告 §4 推荐）
- `vdpa_sim_blk.cpp` — P2（v5.5.3 阶段可选）
- `vhost_vdpa.cpp` — `/dev/vhost-vdpa-X` 字符设备
- `vdpa_nl.cpp` — netlink 接口（P2）

**关键回调实现**（参考 vdpa_sim）：
```c
// v5.7+ core ops
set_vq_state / get_vq_state / get_status / set_status / reset
get_config / set_config / get_device_features / set_driver_features
// v5.9+ DMA ops
set_map / dma_map / dma_unmap
// v5.7+ virtqueue
set_vq_num / set_vq_ready / get_vq_ready / kick_vq / set_vq_cb
```

**关键 ioctl 仿真**（vhost-vdpa）：
```
VHOST_VDPA_GET_DEVICE_ID / GET_STATUS / SET_STATUS / GET_CONFIG / SET_CONFIG
VHOST_SET_VRING_NUM / VHOST_SET_VRING_ADDR / VHOST_SET_VRING_BASE
VHOST_SET_VRING_KICK / VHOST_SET_VRING_CALL
VHOST_SET_MEM_TABLE / VHOST_VDPA_SET_VRING_ENABLE
```

**验收 Gate 3**：
- DPDK vdpa 应用能识别 UsrLinuxEmu vdpa device
- vhost-virtio + vdpa 集成跑通
- 卡内回环（loopback）数据路径正确

#### **阶段 v5.5.4：Live Migration 仿真（4-6 周）**

**目标**: 实现 V2 状态机 + data_fd + dirty page tracking + vendor save/load 框架

**新增模块**（`sim_hardware/migration/`）：
- `device_state.cpp` — V2 状态机（ERROR / STOP / RUNNING / STOP_COPY / RESUMING）
- `dirty_page.cpp` — dirty page tracking（4KB granularity）
- `save_load.cpp` — vendor callback 框架（vendor-specific 数据格式由真实驱动提供）
- `mig_data_fd.cpp` — data_fd 抽象（用 `pipe()` 模拟）

**关键 ioctl 仿真**（v6.0+ V2）：
```
VFIO_DEVICE_FEATURE_MIG_DEVICE_STATE (feature 2)
VFIO_DEVICE_FEATURE_DMA_LOGGING_START (feature 6)
VFIO_DEVICE_FEATURE_DMA_LOGGING_REPORT (feature 8)
```

**关键结构**：
```c
struct vfio_device_feature_mig_state {
    __u32 device_state;
    __u32 data_fd;
};

struct vfio_device_feature_dma_logging_control {
    __u64 page_size;
    __u32 num_ranges;
    __u64 ranges[];  // IOVA 监控范围
};
```

**vendor-specific 数据**（**不在 UsrLinuxEmu 仿真范围**）：
- amdgpu: KFD state（context、queue、page table）— 由真实 amdgpu driver 提供
- nvidia: vGPU state — 由真实 nvidia driver 提供
- s390 AP: 专用 state — 由真实 s390 driver 提供

**验收 Gate 4**：
- QEMU + UsrLinuxEmu VFIO 模拟迁移场景能成功 save/load
- amdgpu save/load callback 测试通过
- dirty bitmap 报告正确性测试

### D3: sim_hardware/ 完整目录结构

```
sim_hardware/
├── iommu/            # ADR-088 §D3.6（5 函数，已规划）
├── cxl_memdev/       # ADR-088 §D3.7（4 函数，已规划）
├── vfio/             # v5.5.1（本 ADR 新增）
│   ├── vfio_device.cpp
│   ├── vfio_ops.cpp
│   ├── vfio_char.cpp
│   ├── vfio_pci.cpp
│   └── vfio_user.cpp          # v5.5.1 P2（预留）
├── iommufd/          # v5.5.2（本 ADR 新增）
│   ├── iommufd_core.cpp
│   ├── ioas.cpp
│   ├── hwpt.cpp
│   ├── device.cpp
│   ├── ioctl_handlers.cpp     # ~18-20 ioctl (v6.2+v6.3+ 全集)
│   └── pasid_manager.cpp      # v5.5.2 P1
├── vdpa/             # v5.5.3（本 ADR 新增）
│   ├── vdpa_device.cpp
│   ├── vdpa_sim_net.cpp       # 移植 vdpa_sim
│   ├── vdpa_sim_blk.cpp       # v5.5.3 P2
│   ├── vhost_vdpa.cpp
│   └── vdpa_nl.cpp            # v5.5.3 P2
└── migration/        # v5.5.4（本 ADR 新增）
    ├── device_state.cpp
    ├── dirty_page.cpp
    ├── save_load.cpp
    └── mig_data_fd.cpp
```

**总工作量估算**（显式算式 per ADR-088 §D2 参考）：
- v5.5.1: 6-8 周（**UsrLinuxEmu 团队主导**；VFIO 核心仿真，位于 `sim_hardware/vfio/`，属系统级硬件 per ADR-088 §C2）
- v5.5.2: 6-8 周（UsrLinuxEmu 团队主导；IOMMUFD 仿真）
- v5.5.3: 4-6 周（UsrLinuxEmu 团队主导；vDPA 仿真）
- v5.5.4: 4-6 周（UsrLinuxEmu 团队主导；Live Migration 仿真）
- v5.5 Final: 1 周（文档同步 + SSOT 更新）

**关键路径算式**（v5.5.1 与 v5.5.2 部分并行）：
```
max(6-8, 6-8) + 4-6 + 4-6 + 1 + 阶段集成缓冲(5-7)
= 6-8 + 4-6 + 4-6 + 1 + 5-7
= 20-28 周
```

- **下限 20 周**：v5.5.1/v5.5.2 取最短 6 周 + 阶段间压缩并行减少缓冲
- **上限 28 周**：v5.5.1/v5.5.2 取最长 8 周 + 完整阶段集成缓冲

**阶段间集成缓冲 5-7 周**包含：
- v5.5.1↔v5.5.2 集成（VFIO 绑定 IOMMUFD）：1 周
- v5.5.2↔v5.5.3 集成（IOMMUFD 中 vhost-vdpa 路径）：1 周
- v5.5.3↔v5.5.4 集成（vDPA 状态与 Migration 状态同步）：1 周
- 5 个 Gate 验证期累计（Gate 1-5 各 0.5-1 周）：2-3 周

### D4: 与现有 1262 行 IOMMU 仿真代码的关系

| 现有模块 | v5.5+ 角色 | 集成点 |
|---------|----------|-------|
| `iommu_domain.cpp` | **保留**作为 `iommufd_ioas` backing store | `iommu_domain_state` 复用 |
| `iommu_group.cpp` | **保留**（iommufd 自动处理）| `kernel/iommu/iommu_emu_state` 复用 |
| `dma_remap.cpp` | **保留**（iommufd hwpt 底层调用） | 无改动 |
| `ats_protocol.cpp` | **保留**（ATS 响应） | 无改动 |
| `ioasid.cpp` | **保留 + 扩展**（iommufd_pasid_manager） | 新增 iommufd AP |
| `invalidate.cpp` | **保留**（IOTLB invalidate） | 无改动 |
| `vfio_bridge.cpp` | **deprecated 保留**（向后兼容；不再作为主路径，新代码用 `sim_hardware/vfio/`）| **D8 决策：保留 env var `USR_LINUX_EMU_VFIO` 桥接路径** |

**关键决策 (D8)**: `vfio_bridge.cpp` 当前通过 `USR_LINUX_EMU_VFIO` env var 检测是否桥接真实 Linux。v5.5+ 阶段：
- **保留 vfio_bridge.cpp**（向后兼容，env var 仍可用）
- **新增完整 VFIO 仿真**（env var 未设置时走仿真路径）
- **标记 deprecated**：v5.5 final gate 后评估完全移除

### D5: HAL 68 fn-ptrs 治理（per ADR-023 append-only）

**v5.5+ 仿真不直接暴露为 HAL fn-ptrs**：
- driver 通过 `linux_compat/vfio.h` 等头文件调用
- linux_compat dispatcher 路由到 `sim_hardware/vfio/` 仿真代码
- HAL 内部仍维持 68 fn-ptrs（per ADR-036 边界 dt/kfd ↔ hal_user ↔ sim）

**潜在 HAL 扩展**（**仅当仿真发现 HAL 桥不充分时**）：
- `hal_iommu_set_dev_pasid()`（PASID attach 桥接）
- `hal_vfio_dma_unmap_notify()`（dirty page 桥接）
- 新增走 ADR 流程（per ADR-023 Decision 4 spec-driven 扩展）

### D6: CXL.cache / CXL 2.0+ HDM 评估（per ADR-088 §Open Questions）

**保持 v5.5+/v5.5+ 评估状态**（**不**纳入 v5.5.1-v5.5.4 实施）：
- CXL.cache 需要 coherency 仿真（per ADR-088 §Open Questions 4）
- CXL 2.0+ HDM 需要 host-managed memory 仿真（per ADR-088 §Open Questions 5）
- **未来 ADR 单独评估**（v5.5+ 阶段）

---

### D7: Consumer Drivers / 验证策略（Oracle 评审 v0.3 → v0.4 追加）

**核心决策**：5 个 `sim_hardware/` 子系统的 consumer drivers **不与 `gpu_driver` 耦合**，采用 **方案 C（Catch2 测试即 consumer）为主 + 聚合插件兜底** 的最小路径。**反对**放进 `plugins/gpu_driver/drv/`（违反 ADR-036 ③ 反向耦合），**反对**独立的 5 个新 plugin 目录（5 个 plugins 摊销过重）。

#### D7.1 三条核心规则

1. **方案 C 为主（Catch2 测试即 consumer）**：每个子系统一个 `tests/test_<subsys>_consumer_standalone.cpp`（200-400 行），直接驱动仿真 API 并断言行为。consumer 本质是**仿真正确性的验证工具**，不是生产驱动。项目已有 `test_ats_protocol_standalone.cpp` 等"无独立 driver、Catch2 直测仿真"的成功先例。

2. **200-400 行上限（三层阈值）**：consumer 的存在意义是钉住仿真 API 语义（map 后读回、virtqueue kick 后状态迁移、save/load round-trip）。
   - **目标上限 400 行**：单个 `tests/test_<subsys>_consumer_standalone.cpp` 应保持在此范围内
   - **坏味道阈值 500 行**：单子系统 > 500 行视为坏味道，应拆回测试或删除冗余断言
   - **升格触发 1000+ 行**：远超阈值 → 触发 D7.8 升格到聚合插件（独立 binary）
   - Gate 已安排 QEMU/amdgpu 做真实性验证（per D2 Gate 1-4），consumer 不需要达到 "QEMU 简化版" 深度。

3. **聚合插件兜底（Migration + VFIO 演示）**：仅当某子系统需要 QEMU 对跑 / 端到端 attach 演示等"非 Catch2 可覆盖"场景时，才升格进**单一聚合插件** `plugins/system_hw_consumers/`（内含 5 个小模块），避免 5 个新 plugin 目录的维护摊销。

#### D7.2 Consumer 路径对照表

| Consumer | 默认路径（方案 C） | 升格路径（聚合插件） | 关键验证点 |
|----------|---------------------|----------------------|-----------|
| **VFIO** | `tests/test_vfio_consumer_standalone.cpp` | `plugins/system_hw_consumers/vfio/`（仅端到端 attach 演示） | open container → attach → BAR mmap → MSI-X 配置 → irqfd 触发 |
| **IOMMUFD** | `tests/test_iommufd_consumer_standalone.cpp`<br>+ KFD 集成测试段（验证真实驱动路径）| —（KFD 是真实用户，集成测试足够） | IOAS alloc → map → attach device → unmap round-trip |
| **vDPA** | `tests/test_vdpa_consumer_standalone.cpp` | `plugins/system_hw_consumers/vdpa/`（仅数据路径 demo） | mgmtdev 新建 → dev add → feature 协商 → vq kick → IRQ 触发 |
| **Migration** | `tests/test_migration_consumer_standalone.cpp`（loopback 双实例 save/load）| `plugins/system_hw_consumers/migration/` 或 `tools/migration_client`（需 QEMU 对跑）| V2 state 切换 → data_fd read/write → dirty bitmap round-trip |
| **CXL.mem** | `tests/test_cxl_consumer_standalone.cpp` | — | pmem mmap → write → flush → readback 一致性 |

#### D7.3 命名约定

- **测试文件**：`test_<subsys>_consumer_standalone.cpp`（Linux 术语 + `_consumer` 后缀）
  - 示例：`test_vfio_consumer_standalone.cpp`、`test_iommufd_consumer_standalone.cpp`
- **插件内模块**：沿用 Linux 内核术语（`vfio_pci` / `vhost_vdpa` / `cxl_mem`），**不加项目自造词**
- **`_consumer` 后缀**：明确表达"验证工具"定位，避免被误认为生产驱动（与 `gpu_driver` 的 `drv/` 区分）

#### D7.4 边界规则（per ADR-036 + ADR-018）

| 边界 | 规则 | 检查 |
|------|------|------|
| **② → ③** | consumer 不经过 `gpu_hal_ops`（那是 GPU 板卡桥接层），直接走 `linux_compat → sim_hardware/` | docs-audit.sh §1.6（已通过） |
| **③ → ③** | consumer 可调用 `sim_hardware/` 同层 API（含跨子系统，例如 IOMMUFD + VFIO） | 无需检查 |
| **consumer ↔ KFD** | KFD 是 IOMMUFD 的**真实用户**（通过 `linux_compat/iommufd.h` <sub>**（v5.5.2 创建，per ADR-027 spec-driven）**</sub>），**不是 consumer driver**。IOMMUFD 的集成验证复用 KFD 自身路径 | KFD 集成测试段（per D7.2）|
| **consumer ↔ gpu_driver** | 唯一交汇点 = `linux_compat/iommufd.h` <sub>**（v5.5.2 创建）**</sub> API 表面（KFD 用 IOMMUFD）。**consumer 不依赖** `plugins/gpu_driver/` 任何代码 | 路径依赖检查 |

#### D7.5 实施时间线（与 D2 对齐）

| 阶段 | Consumer 实施 | 工作量 |
|------|---------------|-------:|
| **v5.5.1** | `tests/test_vfio_consumer_standalone.cpp`（首个模板） | 含在 v5.5.1 阶段 |
| **v5.5.2** | `tests/test_iommufd_consumer_standalone.cpp` + KFD 集成测试段 | 含在 v5.5.2 阶段 |
| **v5.5.3** | `tests/test_vdpa_consumer_standalone.cpp` | 含在 v5.5.3 阶段 |
| **v5.5.4** | `tests/test_migration_consumer_standalone.cpp`（loopback）<br>+ 若需 QEMU 对跑则升格 `plugins/system_hw_consumers/migration/` | 含在 v5.5.4 阶段 |
| **v5.5.5**（ADR-088 §D3.7）| `tests/test_cxl_consumer_standalone.cpp` | 含在 CXL.mem 阶段 |
| **v5.5 Final** | 回看 5 个子系统的实际需要，决定是否建 `plugins/system_hw_consumers/` 聚合插件 | 评估性 |

#### D7.6 文档最小集（不创建 master design doc）

- **不需要**：`docs/05-advanced/system-hw-consumer-drivers-design.md`（过度工程化）
- **不需要**：per-consumer design doc（除非 Migration 升格为 binary 时才需一份）
- **P2**：在 `docs/05-advanced/` 各调研报告补 "Consumer/验证入口" 指针段落

#### D7.7 与现有模式的一致性

| 现有模式 | Consumer 模式 | 关系 |
|----------|---------------|------|
| `plugins/gpu_driver/drv/` (1136 行 gpgpu_device.cpp) | `tests/test_*_consumer_standalone.cpp`（200-400 行）| 不同范畴：前者是 ② 生产驱动；后者是 ③ 仿真验证工具 |
| `tests/test_dma_remap_standalone.cpp`（223 行）| `tests/test_<subsys>_consumer_standalone.cpp` | 同范畴：均为 ③ 仿真验证 |
| `tests/test_migration_e2e_standalone.cpp`（已存在）| `tests/test_migration_consumer_standalone.cpp` | 关系：现有 e2e 测试可演进为 consumer；不创建并行文件 |

#### D7.8 触发升格聚合插件的条件

**升格触发**（任一条件满足即升格）：
- 某子系统验证需要 1000+ 行（远超 500 上限）→ 拆分到独立 binary
- 某子系统需要 QEMU 兼容对端协议（仅 Migration 明确需要）
- 多个子系统共享验证 fixture（如 VFIO + IOMMUFD 集成场景）→ 聚合

**升格非触发**（即使具备也不升格）：
- 仅做单元断言的子系统（VFIO 基础、IOMMUFD 基础）
- 可在 tests/ 内 Catch2 完成的所有场景

---

## Consequences

### 正面后果

1. **真正支持完整驱动开发（含 VFIO / IOMMUFD / Live Migration / vDPA）**
   - amdgpu 真机 mdev 驱动可在 UsrLinuxEmu 仿真环境开发
   - NVIDIA vGPU 用户态模拟可走 vDPA 路径
   - IOMMUFD 支持 SVA/SVM（per ADR-088 §C4 已规划）
2. **仿真拓扑与真硬件一致**（per ADR-088 §C2）
   - VFIO 设备 = dGPU 板卡
   - 系统 IOMMU 在 system_hw（与真硬件一致）
3. **充分利用现有 1262 行 IOMMU 仿真代码**（不浪费）
   - iommu_domain_state 作为 iommufd_ioas backing store
   - dma_remap / ats_protocol / ioasid 复用
4. **vdpa_sim 移植大幅降低 vDPA 仿真工作量**
   - vdpa_sim_net 完整源码可直接复用
   - 不需重新设计 virtqueue 模拟
5. **Live Migration 仿真简化方案**
   - vendor-specific 数据由真实驱动提供（不在 usremu 范围）
   - 仿真仅提供状态机 + dirty tracking + save/load 框架
6. **与 TaskRunner / 真实 KFD 集成路径完整**
   - 真实 KFD 模块可 attach 到 UsrLinuxEmu 仿真 VFIO
   - IOMMUFD 支持 KFD SVM / SVA

### 负面后果

1. **20-28 周工作量**（v5.5+ 阶段；详细算式见 D3）
   - **UsrLinuxEmu 团队 20-28 周**（v5.5.1-v5.5.4 全包；v5.5.1 + v5.5.2 部分并行；v5.5.3 + v5.5.4 顺序）
   - CppTLM 团队：dGPU 板卡仿真 23 ABI 范围（per ADR-088 §D5，**非 v5.5+ 范围**）
2. **HAL 68 fn-ptr 治理压力**
   - v5.5.2 IOMMUFD 可能触发 HAL 桥接（需新 ADR）
   - v5.5.4 Live Migration 状态机调用跨 HAL 边界
3. **多 ABI 协调**
   - 与 CppTLM 23 ABI（per ADR-088 §D5）协同
   - 与 HAL 68 fn-ptrs（per ADR-023）协同
   - 与 linux_compat/ 头文件（per ADR-027）协同
4. **测试覆盖扩大**
   - 每个 ioctl 需单元测试
   - 集成测试覆盖真实驱动 attach
5. **复杂度风险**
   - IOMMUFD 与 VFIO 集成复杂度（v6.2+ 才稳定）
   - vendor-specific 数据格式不可控（vendor 驱动提供）

### 风险表

| 风险 | 概率 | 影响 | 缓解 |
|------|-----|------|------|
| IOMMUFD v6.2+ 与 VFIO 集成复杂 | 中 | 高 | 分阶段实施（v5.5.2 独立阶段）|
| 真实 vendor driver 在仿真环境无法 attach | 低 | 高 | Gate 1 早期验证（amdgpu vfio 子模块）|
| vdpa_sim 移植工作量超过预期 | 中 | 中 | 先实现仅 v5.7 核心，v5.9 DMA ops 延后 |
| Live Migration vendor callback 无法驱动 | 中 | 高 | 仿真仅提供接口框架，简化方案（JSON 格式）|
| HAL 68 fn-ptrs 触发扩展需求 | 中 | 中 | 走 ADR 流程（per ADR-023 Decision 4）|
| 实测与调研报告偏差 | 低 | 中 | V1-V6 已验证；剩余细节在 OpenSpec change 阶段验证 |

---

## Migration / 实施步骤

### 阶段 0：本 ADR 升 Accepted（0 周）

**预备**：
- Oracle 评审（architecture 合规性）
- Architecture Team 评审（HAL 治理边界）
- CppTLM maintainer 评审（v5.5.1 资源承诺）

### 阶段 v5.5.1：VFIO 核心（6-8 周，**UsrLinuxEmu 团队主导**）

**Owner**: UsrLinuxEmu team（per ADR-088 §C2 系统级硬件归 UsrLinuxEmu 维护；本 ADR v0.1 草案原误列为 CppTLM team，Oracle 评审已修正）

| 周 | 任务 | 产出 |
|----|------|------|
| Week 1-2 | `vfio_device` 抽象 + `vfio_device_ops` 完整回调表 | `vfio_device.cpp` / `vfio_ops.cpp` |
| Week 3-4 | `vfio_char.cpp` + 9 个 PCI region | `/dev/vfio/vfio` 字符设备 |
| Week 5-6 | MSI capability 虚拟化 + BAR sizing | `vfio_pci.cpp` |
| Week 7-8 | vfio-user 协议（P2 预留）| `vfio_user.cpp`（接口预留）|
| Gate 1 | amdgpu vfio 子模块编译运行 | 真实 attach 测试通过 |

### 阶段 v5.5.2：IOMMUFD（6-8 周，UsrLinuxEmu 团队主导）

**Owner**: UsrLinuxEmu team

| 周 | 任务 | 产出 |
|----|------|------|
| Week 1-2 | `iommufd_ctx` 单例 + 对象 xarray | `iommufd_core.cpp` |
| Week 3-4 | IOAS + HWPT 仿真（复用 `iommu_domain`）| `ioas.cpp` / `hwpt.cpp` |
| Week 5-6 | ~18-20 个 uapi 命令处理（P0+P1 完整列表）| `ioctl_handlers.cpp` |
| Week 7-8 | PASID manager + 集成测试 | `pasid_manager.cpp` |
| Gate 2 | `tools/testing/iommufd/` 跑通 + amdgpu KFD attach | 兼容实测 |

### 阶段 v5.5.3：vDPA（4-6 周，UsrLinuxEmu 团队主导）

**Owner**: UsrLinuxEmu team

| 周 | 任务 | 产出 |
|----|------|------|
| Week 1-2 | `vdpa_sim_net` 完整移植 | `vdpa_sim_net.cpp` |
| Week 3-4 | vhost-vdpa 字符设备 + 10 ioctl | `vhost_vdpa.cpp` |
| Week 5-6 | DPDK 集成测试 + 性能基准 | DPDK 跑通 |
| Gate 3 | DPDK vdpa 应用识别 UsrLinuxEmu 设备 | 实测通过 |

### 阶段 v5.5.4：Live Migration（4-6 周，UsrLinuxEmu 团队主导）

**Owner**: UsrLinuxEmu team

| 周 | 任务 | 产出 |
|----|------|------|
| Week 1-2 | V2 状态机 + data_fd 抽象 | `device_state.cpp` / `mig_data_fd.cpp` |
| Week 3-4 | dirty page tracking + PCI integration | `dirty_page.cpp` |
| Week 5-6 | vendor save/load 框架 + QEMU 集成测试 | `save_load.cpp` |
| Gate 4 | QEMU + UsrLinuxEmu VFIO 模拟迁移通过 | 端到端验证 |

### 阶段 v5.5 Final：文档同步（1 周）

- 同步 SSOT `post-refactor-architecture.md` §1.10
- 更新 `docs/02_architecture/post-refactor-architecture.md` §1.10.4
- 同步 `roadmap.md` v5.5+ 状态
- 归档 Phase 1-4 临时设计文档

**总工期**：**约 20-28 周**（v5.5.1 与 v5.5.2 部分并行 + 阶段集成缓冲 5-7 周；详细算式见 D3 §D3 目录结构末尾）

---

## Acceptance Gate

### Gate 1（v5.5.1 完成）

- [ ] `vfio_device` 模块可工作（9 个 PCI region + MSI cap）
- [ ] VFIO 字符设备 `/dev/vfio/vfio` 完整实现
- [ ] 真实 amdgpu vfio 子模块可编译运行
- [ ] 实测 `lspci -v` 可见 VFIO 设备
- [ ] 98 Catch2 测试 mode B 全 PASS

### Gate 2（v5.5.2 完成）

- [ ] `iommufd_ctx` + IOAS + HWPT + device 完整仿真
- [ ] uapi 命令全集实现（P0+v6.3+扩展共 ~18-20 个，按调研报告 §3 完整清单）
- [ ] 真实 `tools/testing/iommufd/` 测试套件跑过
- [ ] amdgpu KFD 通过 IOMMUFD attach 成功
- [ ] 1M 次 IOAS map/unmap 延迟 < 10 ms

### Gate 3（v5.5.3 完成）

- [ ] `vdpa_sim_net` 完整移植（vdpa_sim 源码）
- [ ] vhost-vdpa 字符设备 + 10 ioctl 完整实现
- [ ] DPDK vdpa 应用能识别 UsrLinuxEmu vdpa 设备
- [ ] 卡内回环（loopback）数据路径正确
- [ ] vhost-virtio + vdpa 集成跑通

### Gate 4（v5.5.4 完成）

- [ ] V2 状态机（5 状态）可工作
- [ ] dirty page tracking（4KB granularity）正确
- [ ] vendor save/load callback 框架可驱动
- [ ] QEMU + UsrLinuxEmu VFIO 模拟迁移成功
- [ ] amdgpu save/load callback 测试通过

### Gate 5（v5.5 Final）

- [ ] 4 个子系统集成测试通过
- [ ] 真实 vendor driver 全链路 attach
- [ ] docs-audit.sh cross-doc 验证一致性
- [ ] SSOT `post-refactor-architecture.md` §1.10 更新
- [ ] `roadmap.md` v5.5+ 标记完成

---

## Open Questions

1. **HAL 68 fn-ptrs 扩展需求**：v5.5.2/v5.5.4 实施时是否需要 HAL 桥接？如需新增（`hal_iommu_set_dev_pasid` 等），需单独 ADR。
2. **KVM 集成边界**：per 调研报告 Q9，UsrLinuxEmu 不仿真 KVM；QEMU 与 UsrLinuxEmu vfio 边界由 v5.5+ 评估决定。
3. **vfio-user 范围**：v5.5.1 P2 仅预留接口；v5.5.2+ 是否实施需业务场景驱动。
4. **vendor-specific 数据格式**：确认 amdgpu KFD 提供 save/load callback 后即可继续（v5.5.4 范围）。
5. **CXL.cache / CXL 2.0+ HDM**：保持 v5.5+ 评估状态（per ADR-088 §Open Questions）。
6. **Total 工作量偏大风险 — 决策：不拆分**（v0.5 Oracle 复审裁决）：ADR-089 作为 v5.5+ 仿真范围的**总纲 ADR**，4 大子系统（VFIO / IOMMUFD / vDPA / Live Migration）紧密耦合（VFIO ↔ IOMMUFD 集成 + vDPA ↔ IOMMUFD 集成 + Migration 跨子系统状态机）。拆分会增加治理负担（4 份 ADR 同步修订 + 交叉引用维护），单 ADR 更适合此阶段的整体协调。如未来某子系统独立演进（如 VFIO cdx / AP），再单独建子 ADR。
7. **KFD ↔ IOMMUFD 拓扑解读**（v0.5 Oracle 复审追加）：D7.4 描述"KFD 通过 `linux_compat/iommufd.h` 用 IOMMUFD"——需明确 KFD 在 v5.5.2 后的**调用路径语义**：
   - **方案 A**：KFD 改用 iommufd uapi 风格接口（偏离"零修改移植"承诺）
   - **方案 B（推荐）**：KFD 现有 HAL IOMMU 路径（per ADR-061 `hal_iommu_map/unmap`）下层由 IOMMUFD 仿真承接，"KFD 通过 IOMMUFD attach" 指验证端到端打通
   - 决策依据：真实 Linux 内核中 KFD 走 IOMMU core API（`iommu_sva` 等）而非 iommufd uapi。UsrLinuxEmu 是用户态仿真，"KFD 在 usremu 中经 HAL IOMMU 路径，下层由 IOMMUFD 仿真实现"是合理映射。
   - **v5.5.2 实施启动前由 owner 裁决**（建议采纳方案 B 并写入 v5.5.2 实施 spec）

---

## References

### 调研文档

- [docs/05-advanced/system-hw-survey-2026-08-16.md](../05-advanced/system-hw-survey-2026-08-16.md) — **主调研报告**（v0.2，4 个 librarian 调研综合）
- [docs/05-advanced/vfio-live-migration-research.md](../05-advanced/vfio-live-migration-research.md) — Live Migration 独立深度报告（425 行 18KB）

### 关联 ADR

- [ADR-088](../00_adr/adr-088-dgpu-complete-simulation.md) ✅ — dGPU 参考设计（Open Questions v5.5+ 评估）
- [ADR-023](adr-023-hal-interface.md) ✅ — HAL 接口契约（68 fn-ptrs append-only）
- [ADR-036](adr-036-three-way-separation.md) ✅ — 3 区分架构原则
- [ADR-061](adr-061-hal-iommu-extension.md) ✅ — HAL IOMMU ops 扩展
- [ADR-060](adr-060-message-notification-threading.md) ✅ — kernel_workqueue
- [ADR-027](adr-027-linux-compat-strategy.md) ✅ — Linux 兼容层扩展策略
- [ADR-035](adr-035-governance-policy.md) ✅ — 治理规则

### 外部参考

- **Linux 内核 6.14 LTS 文档**：
  - VFIO: docs.kernel.org/driver-api/vfio.html
  - IOMMUFD: docs.kernel.org/userspace-api/iommu.html
  - vDPA: docs.kernel.org/driver-api/vdpa.html
- **vfio-user 规范**：qemu.org/docs/master/interop/vfio-user.html
- **QEMU 10.0 migration 文档**：qemu master branch docs/devel/migration/vfio.html
- **vdpa_sim 源码**：github.com torvalds/linux/drivers/vdpa/vdpa_sim/

### Librarian 调研 Sessions（2026-08-16）

- `bg_58ae2170` — VFIO 演进（3m26s）
- `bg_b17d8e06` — IOMMUFD 框架（4m36s）
- `bg_177fd25d` — Live Migration（5m27s）
- `bg_dd7459af` — vDPA 框架（3m16s）

---

## 修订记录

- **2026-08-16 v0.5** (✅ Accepted — Oracle v0.4 复审 4 Minor 合入 + 2 OQ 裁决)：
  - **M1** 修复：D7.1 行数口径三层关系显式化（200-400 目标上限 / 500 坏味道阈值 / 1000+ 升格触发）
  - **M2** 修复：Gate 2 重复行删除（"`iommufd_ctx` + IOAS + HWPT + device 完整仿真" 由两次合并为一次）
  - **M3** 修复：D7.4 `linux_compat/iommufd.h` 标注前向引用（"v5.5.2 创建，per ADR-027"）
  - **M4** 修复：`docs/00_adr/README.md` 状态分布总览 ADR-089 版本号 v0.1 → v0.5（升档时同步修正）
  - **OQ1** 裁决：KFD↔IOMMUFD 拓扑采用方案 B（KFD 走 HAL IOMMU 路径，下层由 IOMMUFD 仿真承接）——真实 Linux KFD 不走 iommufd uapi，方案 B 保持"零修改移植"承诺
  - **OQ2** 裁决：**不拆分**为多个子 ADR——4 大子系统紧密耦合（VFIO ↔ IOMMUFD + vDPA ↔ IOMMUFD + Migration 跨子系统状态机），单 ADR 总纲更合适。子 ADR 仅在某子系统独立演进时建立（如未来 VFIO cdx / AP）
  - **Oracle 复审结论**：v0.4 §D7 完整采纳 v0.3 关于 consumer drivers 的所有建议 + 边界规则一致 + 文档最小集符合 ADR-035。VERDICT: PASS（无 Blocking issues）
- **2026-08-16 v0.4** (Oracle 评审追加)：新增 §D7 "Consumer Drivers / 验证策略" — Oracle 评审明确 consumer drivers **不与 `gpu_driver` 耦合**，采用 **方案 C（Catch2 测试即 consumer）为主 + 聚合插件兜底** 路径。三条核心规则、5 个 consumer 路径表、命名约定、边界规则、实施时间线、文档最小集。
- **2026-08-16 v0.3** (Oracle 评审修复)：4 项 Blocking 全部修复：(1) 总工期 20-28 vs 22-29 矛盾 → 显式算式；(2) "39 IOMMUFD ioctl" 无出处 → ~18-20 (P0+P1 v6.2+v6.3+)；(3) v5.5.1 owner = CppTLM 与 ADR-088 §C2 冲突 → UsrLinuxEmu 团队主导 + Owner 论证；(4) C1 引用 OQ 编号错误 + 漏 OQ#7 + nested IOMMU 处置 → 全部修正。7 项 Minor 同步修复（ADR-088 References 过时、Handoff 符号数 23→19、Handoff 工期算式、Gap Analysis §4.3 算术、调研报告头文件数 25→4 + IOMMUFD struct 字段 + 状态机 + 乱码、Live Migration v5.2→v6.0 笔误、ADR-089 D4 vs D8 vfio_bridge 措辞）。
- **2026-08-16 v0.1** (初版)：🔄 Proposed 草案（基于 4 个 librarian 调研 + 2 份调研报告综合制定）
- **实施期决策**（✅ Accepted 后启动实施时跟进）：
  - 是否拆分 4 个子 ADR（VFIO / IOMMUFD / vDPA / Migration 各自独立）— OQ2 已裁决 **不拆分**（v0.5 Oracle 复审），仅在某子系统独立演进时建子 ADR
  - HAL 扩展需求评估时点（v5.5.2/v5.5.4 实施时）— 走 ADR 流程（per ADR-023 Decision 4 spec-driven 扩展）
  - v5.5.1 与 v5.5.2 并行启动的资源分配 — v5.5.1 Owner = UsrLinuxEmu team（per ADR-088 §C2）；v5.5.2 同 Owner；阶段 1/2a 可部分并行（关键路径算式见 D3）

---

**最后更新**: 2026-08-16（v0.5 — ✅ Accepted，4 Minor + 2 OQ 合入）
**维护者**: UsrLinuxEmu Architecture Team
**状态**: ✅ Accepted（v0.5 — 2026-08-16 Oracle v0.6 复审 PASS（per Stage 5.5.1 实施后 Gate D）；ADR-090 实施 session `ses_*` 2026-08-17 修复 footer 状态不一致 — 头部 Accepted 与本 footer 现已一致）
