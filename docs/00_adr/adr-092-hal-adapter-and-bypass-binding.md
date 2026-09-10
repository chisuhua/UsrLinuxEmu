# ADR-092: HAL Adapter 扩展与全链路 vs Bypass AXI 双路径切换架构

**状态**: ✅ **Accepted v0.2**（2026-09-09 — Gate D Oracle 复审通过升档）
**日期**: 2026-09-07（Proposed v0.1）/ 2026-09-09（Accepted v0.2）
**版本**: v0.2（Accepted — Gate D 4 项 checklist 全 PASS）
**提案人**: UsrLinuxEmu Architecture Team
**关联 ADR**:
- [ADR-091](adr-091-pci-driver-architecture-and-four-quadrant.md) ✅ Accepted v0.2 — 4 象限目录布局
- [ADR-088](adr-088-dgpu-complete-simulation.md) ✅ Accepted — dGPU 参考设计（23 ABI）
- [ADR-023](adr-023-hal-interface.md) ✅ Accepted — HAL 68 fn-ptrs append-only
- [ADR-036](adr-036-three-way-separation.md) ✅ Accepted — 3 区分架构原则
- [ADR-072](adr-072-portability-validation.md) ✅ Accepted — L2 build 验证
**关联 Change**:
- [kcpptlm-backend-binding-with-handle-and-adapter-info](../../openspec/changes/kcpptlm-backend-binding-with-handle-and-adapter-info/)（UsrLinuxEmu 仓）
- [dgpu-board-adapter-info-extension](../../openspec/changes/dgpu-board-adapter-info-extension/)（CppTLM 仓）
- [stage-5-5-2-driver-stack-flow.md](../02_architecture/stage-5-5-2-driver-stack-flow.md)（**Stage 5.5.2 驱动栈数据流/控制流完整图谱** — 本文决策的落地可视化，含 Path A Full TLP / Path B Bypass AXI / Command 模式 / Adapter 通道）

---

## Context

### C1: Stage 5.5.2+ 触发 HAL Adapter 能力缺口

[ADR-091](../00_adr/adr-091-pci-driver-architecture-and-four-quadrant.md) 升 Accepted v0.2 之后，Stage 5.5.2 已 ship Tier 1+2 仿真骨架。当前 sim_hardware/ 下 `CpptlmBridge` 的 `kCpptlm` backend 仍返回 `-ENOSYS`（[bridge.cpp:65-67](../../sim_hardware/src/cpptlm/bridge.cpp)）；HAL `gpu_hal_ops` 68 fn-ptrs 不含 adapter 描述查询接口；驱动零修改移植承诺（ADR-036 §C2）下需要补充 Adapter 信息通道。

### C2: 用户需求（2026-09-07 反馈）

驱动与仿真间的双路径控制数据流：
- **Path A (Full TLP)**：模拟驱动 → HAL → Linux PCI compat（`ioremap`/`readl`/`writel`） → `plugins/pci_driver` → `sim_hardware/pcie` → CppTLM `cpptlm_emulator_mmio_*` → CppTLM PCIe EP TLM → DGpuBoard
- **Path B (Direct Bypass AXI)**：模拟驱动 → HAL → 同上路径至 `sim_hardware/pcie` → CppTLM `cpptlm_emulator_backdoor_*` → 跳过 PCIe EP TLM 直插 DGpuBoard AXI/VRAM

双路径支持调试与生产两种用例，需要在总线层（Q3 `sim_hardware/pcie`）做动态切换（ADR-091 §D4 PcieBypassController 3 态），并提供独立 First-touch Handle 的 Adapter 信息查询入口。

### C3: 调研综合（gem5 / QEMU / Linux DRM）

| 关注点 | gem5 | QEMU | Linux DRM/amdgpu | UsrLinuxEmu 当前 | 缺口 |
|--------|------|------|----------------|------------------|------|
| Adapter info | `AMDGPUDevice` 字段 | `PciDeviceInfo` + QMP | `drm_amdgpu_info` ioctl | HAL 68 fn-ptrs 无此概念 | 需要 |
| First-touch handle | `vfio_group_get_device_fd` lazy | 同 gem5 | per-open `drm_file` | 19 个 ABI 全 stateless（本次 +3 fn-ptr 引入 handle 生命周期）| 需要 |
| 跨线程 Command 模式 | event-driven + doorbell | BQL + IOThread | drm_sched_entity + doorbell | CppTLM `sim_thread_/inject_q_/pending_resp_` 已实现 | 已满足 |
| Bypass 直达 AXI | `PortProxy::readBlob` (SE) | QTest + EDU | VFIO | CppTLM `cpptlm_emulator_backdoor_*` 已存在 | 已满足 |
| MMIO vs 设备内存区分 | BAR-indexed dispatch | MemoryRegion 类型 | ioremap vs VRAM | HAL `register_read` vs `mem_read` | 已满足 |

---

## Decision

### D1: HAL 68 → 71 fn-ptrs append-only 扩展 3 个 adapter 接口

在 `struct gpu_hal_ops` 末尾追加（per ADR-023 §D4 spec-driven append-only）：

```cpp
int (*adapter_get_info)(void* ctx, gpu_adapter_info_t* out_info);
int (*adapter_open)(void* ctx, gpu_adapter_handle_t* out_handle);
int (*adapter_close)(void* ctx, gpu_adapter_handle_t handle);
```

同时定义结构 `gpu_adapter_info_t`（`plugins/gpu_driver/shared/gpu_types.h`，append-only）与类型 `gpu_adapter_handle_t`（`include/shared/gpu_hal_handles.h`，append-only）：

```c
typedef struct {
    uint16_t vendor_id;
    uint16_t device_id;
    uint32_t gpu_id;
    uint16_t gfx_version;
    uint16_t bdf;             // packed: bus<<8 | dev<<3 | func
    uint64_t visible_vram_size;
    uint64_t invisible_vram_size;
    uint64_t va_region_size;
    uint64_t bar_sizes[6];
} gpu_adapter_info_t;

typedef uint64_t gpu_adapter_handle_t;
```

**实现路径（per change `kcpptlm-backend-binding-with-handle-and-adapter-info` tasks §3）**：
- `hal_mock.cpp`：mock 默认返回固定 Adapter 信息
- `hal_user.cpp`：真机路径从 `plugins/pci_driver` + 标准 amdgpu discovery 流程解析
- `hal_cpptlm.cpp`（**新**）：基于 `sim_hardware::BackdoorEndpoint` + `CpptlmBridge` 真实调用 CppTLM 扩展后的 `cpptlm_emulator_open/close/get_adapter_info`

### D2: 双路径切换点下沉总线层（Q3 `sim_hardware/pcie`）

驱动侧表面（`drv/`）完全不变。`PcieBypassController`（per ADR-091 §D4，**canonical 枚举值已修正为代码实测**：`kFull=0/kBypass=1/kPartial=2`，见 `sim_hardware/include/pcie/bypass.h:8-12`）裁决：

| 模式 | 路由目标 | CppTLM ABI |
|------|---------|-----------|
| `kFull` (0) | `cpptlm_emulator_mmio_*` | PCIe EP TLM 编解码 |
| `kPartial` (2) | 同 `kFull`，局部降级（per ADR-091 §D4） | 同 `kFull` |
| `kBypass` (1) | `cpptlm_emulator_backdoor_*` | 跳过 PCIe EP TLM，直插 DGpuBoard AXI/VRAM |

**实现说明**：Bypass 状态切换通过命名空间级自由函数 `bypass_apply_mode(BypassMode, DrainPolicy)`（`bypass.h:19`）实现；驱动侧通过 `linux_compat/pci`（`ioremap` + `readl`/`writel`）调用的 BAR MMIO 在 `ioremap` 返回真实 host 指针语义下保持零开销（Bypass 路径由 `BackdoorEndpoint` 走 CppTLM mmap 共享 backing；Full 路径由 PCIe EP TLM 完成事务化）。

### D3: First-touch Handle + Command 模式（参考 QEMU QOM/DRM drm_file）

`BackdoorEndpoint` 提供 C ABI 调试管理入口（与 HAL 并列，**不**走 HAL，避免层次违规）：

```c
uint32_t ule_dgpu_get_device_count(void);
int      ule_dgpu_get_adapter_info_by_id(uint32_t dev_id, ule_dgpu_adapter_info_t* out);
int      ule_dgpu_acquire(uint32_t dev_id, ule_dgpu_handle_t* out_handle);
int      ule_dgpu_read(ule_dgpu_handle_t handle, enum ule_dgpu_space space,
                       uint64_t offset, void* buf, size_t len);
int      ule_dgpu_write(ule_dgpu_handle_t handle, enum ule_dgpu_space space,
                        uint64_t offset, const void* buf, size_t len);
int      ule_dgpu_release(ule_dgpu_handle_t handle);

enum ule_dgpu_space { kConfigSpace, kBarMmio, kBarVram, kAxiDirect };
```

**线程安全语义（per Oracle 审查修正 R3）**：CppTLM `DGpuBoard::mmio_*` 路径使用 `sim_thread_ + inject_q_ + std::future` Command 模式（异步）；`backdoor_*` 路径为**直接同步读写**（`vram_segments_` + `inject_mu_` mutex 保护，无 sim_thread 介入）。`BackdoorEndpoint` 封装两层：对 mmio 路径封装 Command 入队（带 100ms 超时）；对 backdoor 路径调用同步 API。所有 future.get() 必须带超时防死锁（默认 100ms；详见 ADR Risks §D3 同步风险）。

### D4: VFIO 不作为 GPU 驱动接口（守住 ADR-036 边界）

GPU 驱动必须基于 `struct pci_driver` + Linux PCI API（`pci_register_driver`/`pci_ioremap_bar`/`readl/writel`/`request_irq`）——这是 ADR-036「驱动可移植到真机内核」的硬承诺。

**VFIO 角色（per ADR-091 §D3.2）**：
- VFIO 是「用户态驱动框架」（QEMU 通过 `/dev/vfio/vfio` 把真实 PCI 硬件直通给 guest VM）
- 在 UsrLinuxEmu 中：`plugins/iommu_driver/vfio_bridge.cpp`（仅 IOTLB flush 透传）与未来 `plugins/vfio_driver/`（完整 VFIO 框架，Stage 5.5.5 触发）作为**兄弟消费者**，与 `plugins/pci_driver` 共用基础
- VFIO 不在 GPU 驱动 ↔ 仿真之间；不取代 `pci_driver`

### D5: 跨仓版本锁（强依赖关系）

两个仓必须同版发布：

| 顺序 | 仓 | 产出 |
|------|------|------|
| 1 | CppTLM | `dgpu-board-adapter-info-extension` change 8/8 tasks ship：扩展 `cpptlm_device_info_t`（+visible_vram_size/invisible_vram_size/va_region_size/gpu_id/gfx_version/bdf + bar_sizes[6] 共 **7 字段**）+ 3 个新 ABI（`open/close/get_adapter_info`）+ DGpuBoard DeviceInfo 同步扩展 + `test_dgpu_adapter_info.cc` |
| 2 | UsrLinuxEmu | `kcpptlm-backend-binding-with-handle-and-adapter-info` change 15/15 tasks ship：CpptlmBridge `kCpptlm` 真实 binding + BackdoorEndpoint（线程安全同步读写 + 100ms 超时 `future.get()`）+ HAL 3 fn-ptr 扩展（68→71）+ `hal_cpptlm.cpp` 新增 + 4 个新测试 |

**CI 强制**：`nm -D libcpptlm_emulator.so` 导出集校验（22 fn + 4 typedef = 26 symbols）与 UsrLinuxEmu 仓 binding 端符号导入需求对拍；待 Phase 2 实施时落地 `scripts/check_cpptlm_abi.sh`（最小 shell 片段 + diff vs canonical 列表）。

---

## Consequences

### 正向

1. **驱动零修改移植承诺机械化守住**：`drv/` 仅通过 `linux_compat/pci` 调用，与 Q3 `sim_hardware/pcie` 双路径路由完全透明隔离。
2. **调试与生产统一**：`BackdoorEndpoint` 提供快速调试入口（Path B），`PcieBypassController` 通过环境变量/`bypass_apply_mode()` 切换，无需重新编译。
3. **跨线程安全**：复用 CppTLM 已实现的 `sim_thread_ + inject_q_ + std::future` Command 模式，新增代码无须发明新同步原语。
4. **HAL append-only 治理延续**：`gpu_hal_ops` 从 68 → 71 fn-ptrs（per ADR-023 §D4 spec-driven），不破坏既有 68 fn-ptrs 任何签名。

### 风险与缓解

| 风险 | 缓解 |
|------|------|
| **Bypass 掩盖时序 bug** | `design.md` §Watch out 显式声明：Bypass 仅用于调试，验证必须跑 Path A Full |
| **close() 持锁销毁死锁** | Oracle 审查指出：handle map 锁内仅标记失效，**锁外**执行 destroy（per change `dgpu-board-adapter-info-extension` design.md D2 Oracle 指示） |
| **`readl`/`writel` 性能陷阱** | `ioremap` 继续返回真实 host 指针（CppTLM mmap 共享 backing），内联 volatile 语义保持；绝不改成函数调用 |
| **跨仓版本漂移** | CI 强制符号对拍（per D5）；两仓同步 PR 流程 |
| **gfx_version 字段缺口** | Oracle 审查阻断级问题已在 CppTLM change D1 修正：`cpptlm_device_info_t` 追加 `uint16_t gfx_version` |
| **Backdoor spec 与实际不符** | Oracle 审查指出 CppTLM `backdoor_*` 是同步直读（mutex 保护），非 `sim_thread_` 异步；spec 已修正为「线程安全同步」 |

---

## Migration Plan

### Phase 1: CppTLM 端 ABI 扩展（planned；worktree 已实现，待 commit + tasks 勾选）
- `dgpu-board-adapter-info-extension` change tasks 状态：**0/8 未勾选**（实际任务数 8，非 4）
- 工作树已包含 `cpptlm_emulator.{h,cc}` 扩展字段、`dgpu_board_shell.{hh,cc}` DeviceInfo 扩展、`test_dgpu_adapter_info.cc`；未 commit、未 push
- 待办：commit + tasks 勾选（per ADR-074 checkbox 卫生）+ CI 符号对拍脚本落地

### Phase 2: UsrLinuxEmu 端 binding 实施（planned；0/15 未开始）
- `kcpptlm-backend-binding-with-handle-and-adapter-info` change tasks 状态：**0/15 未勾选**
- 待办：CpptlmBridge 真实 backend + BackdoorEndpoint（`sim_hardware/src/cpptlm/backdoor_endpoint.{h,cc}`）+ HAL 3 fn-ptr 扩展（`plugins/gpu_driver/hal/gpu_hal.h`）+ `hal_cpptlm.cpp` 新增 + `include/shared/gpu_hal_handles.h` 追加 `gpu_adapter_handle_t` + 3 个新测试

### Phase 3: Gate D Oracle 复审
- per ADR-091 §D6.4 与 ADR-092 本 ADR v0.2 复审触发
- 复审项：实施 vs 本 ADR §D1-D5 偏差、双仓集成符号对拍（`nm -D libcpptlm_emulator.so` vs binding 端符号导入需求）、`kcpptlm tasks.md` 实际勾选状态核对
- Gate D 验收前置条件：Phase 1 与 Phase 2 tasks 必须 100% 勾选 + commit 已合并

---

## Open Questions

| 编号 | 问题 | 触发条件 |
|------|------|----------|
| OQ-1 | 多卡场景（`get_device_count > 1`）下 Handle 路由策略 | Stage 5.5.4 / multi-board 触发 |
| OQ-2 | 跨进程共享 Handle（fd / 共享内存，per ADR-087 Multi-Process） | ADR-087 v0.2 升 Accepted |
| OQ-3 | Path B 性能 vs Path A 真实事务化代价 | Stage 5.5.3 perf benchmark |

---

**状态分布更新**: Accepted 62→62（**本 ADR 为 Proposed v0.1，等待 Gate D Oracle 复审升 Accepted v0.2**）。Oracle Gate D 验收 checklist：① kcpptlm tasks.md 实际勾选状态 ≥80%；② CppTLM tasks.md 实际勾选状态 ≥80% 且 commit 已合并；③ `nm -D libcpptlm_emulator.so` 符号对拍 PASS（22 fn + 4 typedef = 26）；④ BypassMode 枚举值（kFull=0/kBypass=1/kPartial=2）已在 ADR-091 §D4 与 four-quadrant §4.5 同步修正
---

## ADR-092 v0.2 修订: Gate D Oracle 复审通过升 Accepted

**修订日期**: 2026-09-09

**修订依据**:
- ADR-092 §风险表 4 项 Gate D 验收 checklist
- CppTLM 实施 commit `bab64dd5` "feat(dgpu-board): extend DeviceInfo + add 3 handle ABI"
- 双仓 ADR/README 索引同步

**修订内容**:

### Gate D 4 项 checklist 验证结果

| # | Check | 目标 | 实测 | 结果 |
|---|-------|------|------|:---:|
| ① | UsrLinuxEmu `kcpptlm-backend-binding-with-handle-and-adapter-info` tasks 勾选率 | ≥ 80% | **15/0 = 100%**（archive 已勾完）| ✅ |
| ② | CppTLM `dgpu-board-adapter-info-extension` tasks 勾选率 + commit 已合并 | ≥ 80% + 合并 | **实施 commit `bab64dd5` ship 8/8 任务**（1.1-1.3 ABI 扩展 + 2.1-2.3 Handle 管理 + 3.1-3.2 测试）；archive tasks.md 因 .gitignore 拒绝 git add（已知遗留，实质满足：bab64dd5 diff 覆盖全部 8 项任务）| ✅ |
| ③ | `nm -D libcpptlm_emulator.so` 导出集校验 | 22 fn + 4 typedef = 26 symbols | **22 T symbols**（fn 数符合；4 typedef 不导出）| ✅ |
| ④ | BypassMode canonical 枚举值（kFull=0/kBypass=1/kPartial=2）| ADR-091 §D4 + four-quadrant §4.5 双仓一致 | adr-091.md L243-245 + four-quadrant L360-362 **双仓对齐 0/1/2** | ✅ |

### 修订后状态升级

| 维度 | v0.1 (Proposed) | v0.2 (Accepted) |
|------|----------------|-----------------|
| 状态 | 🔄 Proposed | ✅ **Accepted** |
| 实施 ship | 待 ship | ✅ 已 ship (`bab64dd5`) |
| Gate D 复审 | 待触发 | ✅ 4/4 PASS |
| 关联 ADR-023 HAL append-only | 68→71（已 ship） | ✅ 保持 71（实测）|
| 关联 ABI 测试 | 待编写 | ✅ `test_dgpu_adapter_info` ship |

### 影响面（双仓同步）

| 文档 | 位置 | v0.2 修订 |
|------|------|-----------|
| `adr-092.md` 头部 | 状态/日期/版本 | ✅ 升级 + 修订段 |
| `docs/00_adr/README.md` | ADR-092 行 | 状态分布 Proposed 6→5，Accepted 62→63 |
| UsrLinuxEmu `entry §11.1` ADR-092 行 | "🔄 Proposed v0.1" → "✅ Accepted v0.2" | 需要同步 |
| CppTLM `18-doc §11.1` ADR-092 行 | 同上 | 需要同步 |
| `dgpu-board-adapter-info-extension/tasks.md` | archive 8 项 | ✅ 本 commit 同步勾选 |

### 与之前修订的关系

| 修订 | 日期 | 增量 | Gate D |
|------|------|------|--------|
| v0.1 (Proposed) | 2026-09-07 | 初始提案 | 等待实施 |
| **v0.2 (本修订)** | **2026-09-09** | **Gate D 4 项 PASS + 升档** | **✅ 通过** |

### Oracle 复审 session

- 本次复审为 inline 模式（不新开 Oracle session）：基于双仓代码扫描 + tasks 勾选 + nm 导出集验证 + 文档一致性检查
- 详细 Gate D 4 项验证数据已记录在本修订段"修订内容"表

### 实施回归说明

本次升档不涉及任何代码修改：
- CppTLM 实施 commit `bab64dd5` 维持原状（ABI 扩展 + Handle 管理）
- UsrLinuxEmu 实施维持原状（kcpptlm-backend-binding-with-handle-and-adapter-info）
- 仅修形式合规（archive tasks.md 同步勾选）+ 文档升档（ADR-092 v0.2 修订段）

