# ADR-087: Multi-Process Device & Fabric Seam（跨仓跨进程边界治理）

**状态**: 🔄 Proposed（2026-08-14，Oracle 评审 `ses_000f92e51` 触发起草；4-owner 评审关键路径）
**日期**: 2026-08-14
**提案人**: UsrLinuxEmu Architecture Team（基于 Oracle 任务 `bg_33efdfc5` 架构分析 + `bg_49b5caf0` 多进程仿真器调研 + `ses_00370deb6` v0.3 评审综合）
**评审者**（per ADR-035 §R5.1 cross-repo 协议）:
- UsrLinuxEmu Architecture Team（owner 评审 — D1~D9 全范围）
- CppTLM owner（D1/D4.c/D6 关键评审 — NVLink data plane + cluster API 归属）
- PTX-EMU owner（D1/D3/D5 评审 — minimal split + IPC ABI）
- TaskRunner owner（D2/D7/D9 评审 — IGpuDriver consumer 同步）

**关联 ADR**（Accepted）:
- [ADR-023](adr-023-hal-interface.md) ✅ HAL 接口契约（HAL append-only 约束）
- [ADR-035](adr-035-governance-policy.md) ✅ 治理规则（§R5.1 cross-repo 协议 + §R3 每决策独立 ADR）
- [ADR-036](adr-036-three-way-separation.md) ✅ 3 区分原则（每仓独立维护 ①②③ 边界）
- [ADR-076](adr-076-gpgpu-kernel-module-ioctl.md) ✅ 跨仓 HAL 扩展流程模板
- [ADR-011](adr-011-multiprocess-support.md) 🔄 提议中（本 ADR D8 决定其吸收/退出）

**关联 ADR**（Proposed / Draft，由本 ADR D2/D3 政策约束）:
- [ADR-077](adr-077-077-node-fabric-address-model.md) 📋 NFA charter（引用本 ADR D2 descriptor 政策）
- [ADR-078](adr-078-078-switch-driver-plugin.md) 📋 L1 Switch 进程拆分 + CppLink（受本 ADR D2/D3 政策约束）
- [ADR-079](adr-079-079-coherence-protocol-non-goal.md) 📋 Coherence Non-Goal + 跨进程一致性契约（受本 ADR D2 政策约束）
- [ADR-080](adr-080-080-multi-instance-gpu-prerequisite.md) 📋 多实例重构前置 + IPC readiness（受本 ADR D7 gate 约束）
- [ADR-081](adr-081-081-fabric-hal-phase-1.md) 📋 Fabric HAL Phase-1（descriptor-based 签名，受本 ADR D2/D3 约束）
- [ADR-082](adr-082-082-fabric-hal-phase-2.md) 📋 Fabric HAL Phase-2（CppLink-E/T semantics）
- [ADR-083](adr-083-083-multicast-semantics.md) 📋 Multicast 语义（fan-out 在 L1 Switch 进程内）
- [ADR-084](adr-084-084-pgas-address-semantics.md) 📋 PGAS 地址语义
- [ADR-085](adr-085-085-consumer-contract.md) 📋 消费者契约（IFabricDriver，与 CppLink 正交）
- [ADR-086](adr-086-086-trigger-registry.md) 📋 Trigger registry（登记 D4/D5 的 v2 triggers）

**关联外部 ADR / TADR**:
- CppTLM `ADR-SOC-04-hsapp-cp-dispatcher-simplification.md` v1（黑盒路径）+ 计划中 v2（Host Adapter 扩展）— D4.c 依赖项
- CppTLM `ADR-SOC-06-host-adapter-extension.md`（规划中）— Phase 3 实施依赖
- CppTLM `ADR-SOC-07-multi-process-ipc-seam.md`（规划中）— Phase 3 治理依据
- PTX-EMU `ADR-0029-ptxemu-image-executor.md` §D7 libptxemu_minimal.so 拆分 — D1.3 依据
- TaskRunner `tadr-307-igpu-driver-kernel-module-extension.md` — D9 姊妹文档
- Multi-Process Vision Spec（2026-08-13，`docs/superpowers/specs/`）— D1 顶层 vision

**关联研究文档**:
- [scale-up-fabric-architecture.md](../02_architecture/scale-up-fabric-architecture.md) v0.3 §4.6 CppLink 协议 + §9 进程架构 + §13.4 Phase 对齐
- [multi-process-gpu-simulator-integration.md](../02_architecture/multi-process-gpu-simulator-integration.md) v0.1（Oracle 调研综合）
- [ats-cxl-30-implementation-research.md](../05-advanced/ats-cxl-30-implementation-research.md) v0.3（ATS/CXL 协议基础）
- [scale-up-fabric-research.md](../05-advanced/scale-up-fabric-research.md) v0.2（Living Document 决策史）
- NVIDIA Rubin 深度调研（Oracle `ses_0037996c5`）— coherent memory 实证
- Multi-Process GPU Simulators 调研（Oracle `bg_49b5caf0`）— gem5+GPGPU-Sim / vhost-user 先例

---

## §0 Context

### 0.1 触发与背景

UsrLinuxEmu 是用户态 Linux 内核模拟环境（per AGENTS.md），目标是开发可在真实 Linux 内核中运行的 GPU 驱动。其 scale-up 轨道（`scale-up-fabric-architecture.md` v0.3）描述节点内 L1 Switch + 统一 PA + UVM/PGAS + multicast 的 scale-up fabric 架构。

2026-08-13 多仓 Vision Spec 提出 4 仓（UsrLinuxEmu + CppTLM + PTX-EMU + TaskRunner）协作的多进程 GPU 仿真栈愿景（CPU sim 1 进程 + 每 GPU sim 1 进程）。scale-up v0.3 在 2026-08-14 修订中整合该愿景：
- **L1 Switch 升级为独立 CPU 进程**（不是 UsrLinuxEmu 内 plugin）
- **链路协议命名为 CppLink**（避免 NVLink 商标）
- **新增本 ADR-087** 作为跨仓跨进程边界治理的 seam ADR

Oracle 任务 `bg_33efdfc5`（vision doc 架构分析）识别出 **15+ 个跨仓开放问题** 与 **8 项 IMP**（其中 IMP-4 为 load-bearing unknown）。Oracle 任务 `ses_00370deb6`（v0.3 评审）确认 v0.3 方向正确但需需 OCR 中提出的 IMP-4 需在 ADR-087 中裁决。

### 0.2 关键现状

- **Multi-Process Vision**（D1-D4）: 已成 vision spec（spec.md），但尚未在任一仓 ratified
- **Scale-up v0.3**（1147 行）: 已应用 multi-process 修订（§2.1 进程拓扑 + §4.6 CppLink + §9 进程架构）
- **ADR-087 必要性**: scale-up v0.3 §12.1 列出 11 个 ADR（077-087），但 **ADR-077~086 都依赖**本 ADR 的 D2（pointer-safety policy）与 D3（ABI versioning policy）；§13.4 Phase gate 表的 IMP-4 裁决（§13.4 conditional gates）是 scale-up 整个时间线的承重点
- **4-owner review 关键路径**: 任一仓 owner 缺席即阻塞 ADR-077~086 评审

### 0.3 与 ADR-035 §R5.1 cross-repo 协议的关系

本 ADR 是首批严格遵循 **ADR-035 §R5.1 4 步流程**（canonical → consumer review → TADR 对偶 → commit 顺序协议）起草的跨仓 ADR：
- Step 1: 本文件（canonical in UsrLinuxEmu）→ ✅
- Step 2: 4 仓 owner 联合评审（待本文件 Accepted 后）→ ⏸
- Step 3: CppTLM / PTX-EMU / TaskRunner 各自 TADR 对偶（待各仓决策）→ ⏸
- Step 4: commit 顺序协议（canonical 先，consumer TADR 后）→ ⏸

---

## §1 Decision

### D1: Ratify Multi-Process Vision D1-D4 as UsrLinuxEmu-side constraints

**Decision**: 在 UsrLinuxEmu 侧正式采纳（ratify）Multi-Process Vision Spec 中 D1-D4 四个决策，作为本仓不可绕过的跨仓约束。

**Rationale**: D1-D4 由 4-owner 联合 vision spec（2026-08-13）确立，本 ADR 在 UsrLinuxEmu 侧锚定其为不可局部修改的约束，避免后续 ADR 在 UsrLinuxEmu 内部独自偏离 vision。

#### D1.1 Process Boundary（D1.b 的 UsrLinuxEmu 侧 ratification）

**Decision**: UsrLinuxEmu 接受 "CPU sim 1 进程 + 每 GPU sim 1 进程" 的进程拓扑。

**Implications**:
- L1 Switch 是**独立 CPU 进程**（per scale-up v0.3 §9.1 用户 2026-08-14 决策）
- GPU sim 是 per-GPU 进程（per scale-up v0.3 §2.1）
- UsrLinuxEmu CPU sim 是 1 个进程（管理 VFS + ModuleLoader + HAL + driver）
- 3+N 进程拓扑（v0.3 §2.2 diagram）

#### D1.2 Memory Model（D2.a 的 UsrLinuxEmu 侧 ratification）

**Decision**: UsrLinuxEmu 接受 v1 = shared-memory-backed region transport + 显式 region descriptor（per scale-up v0.3 §4.6.4）；v2 = pass-through memory（dGPU）作为 trigger-gated 选项。

**Implications**:
- Region descriptor 必须 no raw pointer（per D2）
- Memory-region descriptors 通过 CppLink-C 控制通道传递（Unix socket + SCM_RIGHTS + memfd）
- v2 pass-through 需要独立 ADR + 消费者拉动

#### D1.3 PTX-EMU Minimal Split（D3.b 的 UsrLinuxEmu 侧 ratification）

**Decision**: UsrLinuxEmu 接受 PTX-EMU `libptxemu_minimal.so` 拆分方案。

**Implications**:
- PTX-EMU minimal 不持有 `SimpleMemory` 或 `GPUContext`（由 CppTLM KernelLaunchTLM 提供）
- PTX-EMU minimal 通过回调注入 memory + synchronization（per Oracle §1.4.4 recommended API）
- UsrLinuxEmu **不直接**链接 PTX-EMU minimal（由 CppTLM 在 GPU sim 进程中集成）

#### D1.4 CppTLM SOC-04 Split（D4.c 的 UsrLinuxEmu 侧 ratification）

**Decision**: UsrLinuxEmu 接受 CppTLM ADR-SOC-04 v1（黑盒）+ 计划 v2（Host Adapter）的分裂治理。

**Implications**:
- v1 黑盒路径不影响 UsrLinuxEmu（保留 TLM verification 路径）
- v2 Host Adapter 是 Phase 3 IPC seam 的 CppTLM 端实现（DoorbellHandler + AQLDispatcher + PIOAdapter）
- v2 与 v1 共享 canonical device model（非两套实现）

### D2: Cross-Process Pointer-Safety Policy

**Decision**: 所有跨进程共享结构**必须** no raw pointer，**必须**使用 canonical descriptor 模式（含 magic + generation）。

**Rationale**: 进程边界使 raw pointer（host virtual address）失去 canonical identity 语义；必须用 opaque handle（region_id + generation + offset）替代。Oracle 任务 `bg_49b5caf0` 的 gem5+QEMU vhost-user 先例均采用此模式。

#### D2.1 Canonical Region Descriptor 字段集（v1）

```cpp
// CppLink v1.0 Region Descriptor（固定 64 字节；no raw pointer）
struct cpplink_region_descriptor_v1 {
    uint32_t magic;               // 0x4350504C "CPPL" — ABI 校验
    uint32_t abi_version;         // CPPLINK_ABI_VERSION (v1.0 = 0x00010000)
    uint32_t region_id;           // 全局唯一
    uint32_t generation;          // ABA 防护
    uint32_t kind;                // 0=hbm, 1=mem_pool, 2=nic_buf, 3=mcast_window, 4=doorbell_ring
    uint32_t permissions;         // R/W/X bitmask
    uint32_t owner_device;        // 拥有该 region 的 device_id
    uint32_t cache_policy;        // 0=device_local, 1=shared, 2=replicated
    uint64_t nfa_base;            // 节点 Fabric Address 起始（per ADR-077）
    uint64_t size;                // 字节数
    uint64_t file_offset;         // backing file 偏移（必须页对齐）
    uint64_t reserved;            // 0 —— v1.1 扩展位
};
static_assert(sizeof(cpplink_region_descriptor_v1) == 64);
```

**Ownership**（per Oracle Q3）：
- **本 ADR-087 D2** owns policy（canonical fields、 + no raw pointer + magic + versioning + generation）
- **ADR-078** owns wire artifacts（descriptor struct 实例化 + channel message layouts）
- **ADR-077** references descriptor for NFA mapping

#### D2.2 Pointer-Safety 禁止规则

| 禁止 | 替代 |
|------|------|
| 跨进程传输 host virtual address（raw pointer）| `region_id + offset`（region-local offset）|
| C++ 对象 vtable 跨进程（vtable pointer）| C ABI function pointer + opaque handle |
| 跨进程引用 process-local singleton | 全局 ID + IPC 服务发现 |
| 跨进程共享 `std::shared_ptr` / `std::vector` | 显式长度 prefix + 字节拷贝 |

### D3: IPC ABI Versioning + Capability Negotiation Policy

**Decision**: 所有跨仓跨进程 ABI 必须显式版本化，boot-time 协商 capability；deprecation 必须有 lead-time 政策。

#### D3.1 Versioning 字段

**Requirement**: 每个跨进程 wire message / shared struct 必须含 `uint32_t abi_version` 字段。

**Convention**: 
- v1.0 → `0x00010000`
- v1.1 → `0x00010100`
- Major version 不兼容（结构 layout 变化）；Minor version 兼容扩展（字段 append）

#### D3.2 Boot-Time Capability Negotiation

**Process**: 所有 connector 进程（GPU sim + UsrLinuxEmu CPU sim）连接 L1 Switch CppLink-C 时：

1. 发送 capability bitmap（per ADR-078 D2 — CppLink 特性支持）
2. 接收 Switch 端 capability bitmap
3. Negotiate intersection 作为有效 capability set
4. 任何 capability 不匹配 → log warning + 继续（v1 = best-effort；v2 = hard-fail per D3.4）

#### D3.3 Deprecation Policy

| 政策 | 实施 |
|------|------|
| Minor version deprecate（移除字段）| 至少 2 个 minor 版本后移除（v1.0 deprecate → v1.2 remove）|
| Major version 强制升级 | old major 可继续运行；新 major 不向后兼容 |
| Capability deprecate | capability bitmap bit 持续 4 个 minor 后移除 |

#### D3.4 Hard-Fail Trigger（v2 选项）

**v1**: capability 不匹配 = log warning + 继续 best-effort
**v2 trigger**: 真 .so e2e gate 强制（per Phase R 教训）→ capability 不匹配 = 连接失败

### D4: Crash Policy (v1 = fail-fast)

**Decision**: v1 采用 **fail-fast 崩溃政策**：任一进程心跳超时或断开 → 全部仿真确定性终止 + 状态 dump。

**Rationale**: 
- v1 无 reconnect 需求（per scale-up v0.3 §4.6.2 + §4.6.6 限制）
- v1 无 checkpoint（per D5）
- Fail-fast 是最简且最可预测的语义
- Phase R 的 "真 .so e2e gate" 教训：fail-fast 比 silent retry 更容易调试

#### D4.1 Heartbeat 参数（默认值；ADR-078 评审时确认）

- C 通道心跳间隔: **1 second**
- 心超时: **3 seconds**（即 3 次心跳 miss）
- 超时动作: **abort simulation + dump state**

#### D4.2 故障场景矩阵（v1 fail-fast 适用）

| 场景 | 检测 | v1 行为 | v2 trigger |
|------|------|---------|-----------|
| L1 Switch 进程崩溃 | C/D/E 通道断开 + 心跳超时 | 全部进程 abort + dump | reconnect 协议（vhost-user 参照）|
| 单 GPU 进程崩溃 | Switch 侧 E 心跳超时 | 全部进程 abort + dump | 单 GPU 重启 + 状态重注入 |
| UsrLinuxEmu CPU sim 先退出 | Switch 侧 C 断开 | Switch 广播 close → GPU 进程有序退出 | — |
| 正常关闭 | §4.6.5 step 5：C 通道 close 广播 | 逆启动顺序退出 | — |
| 心跳超时 | 3 秒 miss | abort + dump | auto-reconnect |

#### D4.3 v1 状态 dump 策略

- 触发: 进程退出（任何 cause）
- 内容: 各进程内存映射 + shared region 状态 + queue 内容快照
- 格式: per-process binary dump（ADR-078 实施时定义）
- 目的: post-mortem 调试（不是 checkpoint / replay）

### D5: Checkpoint Policy (v1 不支持)

**Decision**: v1 **不支持 checkpoint / state dump replay**。

**Rationale**:
- gem5 规则: kernel 运行中禁止 checkpoint（per Oracle §5.5）
- v1 无 quiesce 协议（无跨进程 handshake）
- 简化 v1 实施，checkpoint 为 trigger-gated v2

#### D5.1 v1 限制

- 进程崩溃 → state dump（per D4.3）→ 终止 → 人工重启
- 不可从 dump 恢复仿真
- v1 测试用例：每个 test binary 启动新仿真

#### D5.2 v2 Trigger

**触发条件**: 多进程测试需要长时间运行（>数小时）且需要恢复能力。

**v2 ADR 范围**: 
- 跨进程 quiesce handshake（per CppTLM ADR-SOC-07）
- shared memory 一致性 snapshot
- restart from snapshot 协议

**登记**: ADR-086 trigger registry

### D6: IMP-4 Ruling — Switch Sim / Driver / Userspace 跨仓归属（**Decision（UsrLinuxEmu 侧提案，2026-08-14 用户决策）**: 待 4-owner 评审确认；若被拒，启动 D9.2 兜底流程）

**Decision**: 采用**与 GPU 软件栈同构的 3-仓架构模式**：

| 层 | 角色 | 归属仓 | 路径 | 参照先例 |
|---|------|------|------|---------|
| **③ 硬件 sim** | Switch SoC 仿真（TLM 模型 + switch routing + NVLink timing）| **CppTLM** | `cppTLM/soc_arch/switch/`（新子目录，与 `soc_arch/gpu/` 同级）| CppTLM 的 `soc_arch/gpu/`（GPU cluster sim）|
| **② 内核 driver** | Switch device 驱动 + IOCTL 契约 | **UsrLinuxEmu** | `plugins/switch_driver/`（与 `plugins/gpu_driver/` 同构）| UsrLinuxEmu 的 `plugins/gpu_driver/`（3-way separation per ADR-036）|
| **① Consumer API** | 用户态 switch API（libswitch_taskrunner.so）| **TaskRunner** | `taskrunner/src/switch/`（与 `taskrunner/src/cuda/` 同构）| TaskRunner 的 `taskrunner/src/cuda/`（CUDA runtime shim）|

**Rationale**:

1. **与 GPU 软件栈完全同构**：复用 3-仓 + 3-way separation + HAL bridge 已成功验证的架构模式（per ADR-036 + ADR-023 + ADR-038 网络栈先例）
2. **switch_sim 自然属于 ③ 硬件 sim 层**：与 CppTLM 的 TLM SoC 仿真经验（Tofu/CXL/HBM 仿真）天然契合
3. **switch driver 自然属于 ② 驱动层**：Linux kernel 内驱动；可借鉴 `gpu_driver` 的 ioctl 派发表 + HAL + ModuleLoader 模式
4. **TaskRunner 用户态 driver 自然属于 ① Consumer 层**：CUDA runtime shim 模式扩展（per tadr-307 IGpuDriver consumer-side 先例）
5. **可立即借鉴 GPU 软件栈**：v1 阶段 switch driver 可直接复用 gpu_driver 的 ioctl 派发表 / HAL / ModuleLoader 模板（per user decision）
6. **未来扩展空间**：后续 ADR 可评估是否走独立 switch driver 软件栈（per user decision："后续可以继续调研是否走独立的switch驱动软件栈"）

**D6.0 选项评估**:

| 选项 | 内容 | 裁决 | 理由 |
|---|---|---|---|
| (a) UsrLinuxEmu 全归属 | switch_sim + driver + userspace 全部在 UsrLinuxEmu 仓（`src/scaleup_switch/`，v0.3 §9.2 默认） | ❌ 拒绝 | 与 Vision Phase 4（NVLink sim ∈ CppTLM）冲突；CppTLM 的 TLM SoC 仿真经验（Tofu/CXL/HBM）无法复用；3 仓软件栈同构性丧失 |
| (b) **3 仓同构拆分** | ③ CppTLM / ② UsrLinuxEmu / ① TaskRunner | ✅ **采纳** | 与 GPU 软件栈（gpu_driver / gpu_sim/taskrunner-cuda）完全同构；复用 ADR-036/023/038 已验证模式；各仓 owner 边界清晰 |
| (c) 新建独立仓 | switch 软件栈独立第五仓 | ❌ 拒绝 | 治理开销（第 5 个 owner + 新仓 ADR 体系）；无独立消费者拉动；v2 可重新评估（D6.8） |

**代价声明（per Oracle C4）**：选 (b) 意味着 scale-up W2-W4 成为 CppTLM 的消费者——Phase 1 build 成为 W2-start 硬门、Phase 4 成为 W4 硬门（见 D7.1 修订）。这是有意识的耦合交换：以时序耦合换取架构同构与 TLM 经验复用。

**D6.1 Switch SoC 在 CppTLM 的设计原则**:
- 路径: `cppTLM/soc_arch/switch/`
- 与 GPU SoC 同级：`soc_arch/gpu/` 已有，`soc_arch/switch/` 新增
- 遵循 CppTLM 已有 ADR-SOC-04 (黑盒) + ADR-SOC-06 (Host Adapter) + ADR-SOC-07 (IPC seam) 治理
- 接口: 实现 CppLink-D 数据面 + 部分 D/E/T 控制面（per ADR-078 CppLink 协议）
- Phase 4 (Multi-GPU/NVLink sim) = CppTLM 的 NVLink timing 仿真

**D6.2 Switch Driver 在 UsrLinuxEmu 的设计原则**:
- 路径: `plugins/switch_driver/`（与 `plugins/gpu_driver/` 同构）
- 借鉴 gpu_driver 模板:
  - `module mod` 注册（per ADR-003 plugin 架构）
  - 3-way separation: drv / hal / sim（注意: switch ③ 已经在 CppTLM，UsrLinuxEmu 这里只做 ② driver）
  - HAL bridge 通过 CppLink endpoint 注册（per ADR-078）
- ioctl 契约: 类似 GPU_IOCTL_* 风格，新增 `SWITCH_IOCTL_*`
- 借鉴模式:
  - ioctl 派发表（per `GpgpuDevice::ioctl`）
  - HAL append-only（per ADR-023）
  - Multi-instance support（per ADR-080 即将验收）
- v1 起步: 简单路由表（FABRIC_MANAGER）+ multicast fan-out 引擎（per ADR-083）
- 后续评估: 是否走独立 switch driver 软件栈（per user 2026-08-14 决策）

**D6.3 Switch Userspace 在 TaskRunner 的设计原则**:
- 路径: `taskrunner/src/switch/`（与 `cuda/` 同构）
- 借鉴 CUDA runtime shim 模板:
  - LD_PRELOAD shim（`libswitch_taskrunner.so`）
  - consumer-side IGpuDriver → 演化为 **ISwitchDriver** 契约（per tadr-307 模式）
  - 与 `IFabricDriver` 正交（per ADR-085 修订）
- v1 起步: 简单 lib 封装 switch ioctl
- 后续评估: 是否独立 switch userspace（per user 决策）

**D6.4 跨仓接口契约**:

```
[CppTLM switch_sim process]
  ↓ CppLink-D/E/T (per ADR-078)
[UsrLinuxEmu switch_driver plugin + kernel sim]
  ↓ SWITCH_IOCTL_* (new IOCTL contract, modeled after GPU_IOCTL_*)
[UsrLinuxEmu switch_device /dev/switch0]
  ↓ file ops（per kernel VFS）
[TaskRunner switch userspace libswitch_taskrunner.so]
```

**D6.5 进程拓扑（与 arch v0.5 §2.2 一致）**

| Process | 仓 | 内容 | CppLink 角色 |
|---------|-----|------|------------|
| Process 1 | UsrLinuxEmu | CPU sim（kernel sim + gpu_driver + HAL）| Client（connect 到 CppLink-C/D/E）|
| Process 2 | CppTLM | **switch_sim**（routing + multicast + tick master + FM）| **Server**（bind/listen 全部 CppLink endpoint）|
| Process 3..N+2 | UsrLinuxEmu | per-GPU sim 进程（① 设备 sim + ② driver 接入 + CppTLM PTX-EMU minimal 集成）| Client（connect）|
| Process N+3 | TaskRunner | switch userspace shim（与 CPU sim 同进程，v1 阶段 via ioctl on `/dev/switch0`；独立 consumer 进程为 v2 选项）| 不参与 CppLink（经 ioctl 与 CPU sim 通信）|

**关键约束**：
- 单一 switch 进程（CppTLM 构建的 `bin/switch_sim`）承担 listener + routing + multicast + tick master 四种角色
- Process 1（CPU sim）与 Process N+3（TaskRunner）**同进程**（v1）：通过现有 gpu_driver ioctl 接口（`SWITCH_IOCTL_*`）与 CPU sim 通信，不需要新增 IPC
- Process 2（switch_sim）经 CppLink-C/D/E/T 跨进程连接 Process 1 和 Process 3..N+2
- **没有** "独立的 L1 Switch CPU sim 进程"——所有 switch 功能在 CppTLM switch_sim 内

**D6.6 对 scale-up v0.3 的连锁影响**（待 ADR-087 Accepted 后执行）:

1. §2.1 架构表: 增加 2 行（UsrLinuxEmu switch driver + CppTLM switch_sim）
2. §2.2 拓扑图: 增加 CppTLM switch_sim 进程
3. §9.2 跨进程架构: 修订 `src/scaleup_switch/` → `cppTLM/soc_arch/switch/`（外部仓）
4. §13.4 Phase 对齐表: Phase 1/2/4 gates **从条件性变为硬门**（D6 已解决 = CppTLM 归属）
5. §16 Q9 答案: 更新为 D6 决议内容

**D6.7 v1 借鉴 GPU 软件栈的具体范围**:
- UsrLinuxEmu switch driver v1: ioctl 派发表 + HAL + ModuleLoader 全部可复用 gpu_driver 模板
- TaskRunner switch userspace v1: cuda shim 的 LD_PRELOAD + handle table 模式可复用
- CppTLM switch_sim v1: soc_arch/gpu 的 TLM SoC 框架可复用

**D6.8 v2 评估项**（out-of-v1）:
- 独立 switch driver 软件栈（vs 借鉴 GPU）
- 独立 switch userspace 软件栈（vs 借鉴 CUDA）
- Switch SoC 的更多 TLM 特性（per CppTLM ADR-SOC-06 Host Adapter 演进）

### D7: Phase-Gate 对齐（scale-up Wave ↔ Multi-Process Vision Phase）

**Decision**: scale-up v0.3 §13.4 Phase 对齐表（Per Oracle Q4 修订版）作为 UsrLinuxEmu 侧正式采纳的 gate 关系。

#### D7.1 Phase 对齐（修订版 per Oracle §5.2）

| Vision Phase | 内容 | 阻塞的 Wave | 同步性 | Owner（unblock 责任方） |
|---|---|---|---|---|
| Phase R | PTX-EMU audit 修复 + ADR-076 v2 | 无（e2e-gate 经验输入 W2 验收）| **并行** | UsrLinuxEmu + PTX-EMU owner |
| Phase 1 | CppTLM submodule build | 条件性：W2（仅当 D6 = CppTLM 归属）| **并行** | UsrLinuxEmu + CppTLM owner |
| Phase 2 | CppTLM cluster API | 条件性：W2（同上）| **并行/串行（待定 D6）** | CppTLM owner |
| Phase 3 | IPC seam | **W2-complete（硬门）**；W2-start 不依赖（in-process transport 先行）| 串行（对 W2 完成）| UsrLinuxEmu + CppTLM owner |
| Phase 4 | Multi-GPU/NVLink sim | **W4（条件性，待定 D6）** | 串行/并行（待定 D6）| 4 仓 |
| Phase 5/6 | Multi-node / ISA swap | 无（轨道外）| 独立 | — |

#### D7.2 Phase R 不阻塞 W0

**Explicit Decision**: Phase R（per vision spec L154-161）与 W0 **并行**而非串行。

**Rationale**:
- Phase R 内容（dlsym typedef 修复、`g_gpu_context` lazy-init、`ptxemu_mem_register` ABI、ADR-076 v2、真 .so e2e gate）是 PTX-EMU ↔ UsrLinuxEmu 集成修复
- W0 内容（ADR-077/078/080/086/087 治理 + multi-instance spike + ATS P0 docs + CppLink v1 draft）是 UsrLinuxEmu 内部工作
- 两者**零共享工件**

**Implication**: scale-up v0.3 §13.4 与 §13.1 W0 gate 列中 "Phase R ship" 条件**移除**（per Oracle §5.1 修订）。

#### D7.3 Phase 3 split gates

**Decision**: W2 gate 分为 start / complete 两个阶段。

- **W2-start gate**: ADR-080 spike GO + ADR-077/078 Accepted（per D6 条件性 + Phase 2 if D6=CppTLM）
- **W2-complete gate**: Phase 3 ship + 跨进程 E2E green

**Rationale**: W2 的 P2-P4 工作（switch skeleton + multi-GPU + mem pool）可以在 in-process CppLink transport 上先行；跨进程 datapath 需要 Phase 3 的 Unix socket + memfd + eventfd 机制。

#### D7.4 双轨 bring-up 选项

**Recommendation**: v1 可先以 in-process shared-memory transport 跑通 CppLink 语义（bring-up 模式），部署拓扑不变。

**Implication**: W2 启动前**不需**等 Phase 3；W2 启动后端**需**等 Phase 3 才完成 cross-process 实施。

### D8: ADR-011 Boundary（提议中 multi-process ADR）

**Decision**: 本 ADR-087 **吸收** ADR-011（"multi-process support"）的 fabric-seam 相关范围；ADR-011 中**不属于**本 ADR 范围的部分**维持现状**或独立 ADR。

#### D8.1 ADR-011 状态

[FACT] ADR-011 当前 🔄 Proposed（per docs/README.md）— 项目 README 标记为 "等待 Phase 3"。

#### D8.2 吸收范围映射

| ADR-011 范围（推断自其 title + README 描述）| 本 ADR D-x 归属 |
|---|---|
| 多进程支持总论 | D1（vision D1-D4 ratification）|
| 跨进程 IPC ABI | D2（pointer-safety）+ D3（versioning）|
| 跨进程 crash / recovery 政策 | D4（fail-fast）+ D5（checkpoint defer）|
| 跨仓协作治理 | D9（4-owner review）|
| 项目级 multi-process 实施（具体代码改动）| **维持 ADR-011** 或**新 ADR-088**（本 ADR 不覆盖）|

**Action**: 评审通过后，ADR-011 状态更新为：
- **如果范围全部吸收**: 标记 ADR-011 为 "Superseded by ADR-087"
- **如果有遗留范围**: 拆分为 ADR-088（具体实施 ADR），ADR-011 标记为 "部分 Superseded"

#### D8.3 SSOT 守卫

**Rule**: scale-up v0.3 §14 中所有引用 "ADR-011 域" 的 Non-Goal 项**改写**为引用 "ADR-087 域"，避免双 SSOT 漂移。

### D9: Cross-Repo Review / Commit Order Policy

**Decision**: 本 ADR 及所有后续跨仓 ADR 严格遵循 ADR-035 §R5.1 4 步流程。

#### D9.1 4 步流程（per ADR-035 §R5.1）

1. **Step 1 — Canonical 起草**: 在本仓（UsrLinuxEmu）起草 ADR
3. **Step 2 — 4-owner 评审**: 4 仓 owner 联合评审
4. **Step 3 — TADR 对偶**: CppTLM / PTX-EMU / TaskRunner 各自创建姊妹 ADR/TADR
5. **Step 4 — Commit 顺序协议**: canonical 先 commit，姊妹 ADR 后 commit；commit message 包含 cross-ref

#### D9.2 评审周期目标

- D1-D5, D7-D9: **2 周内**达 Accepted（治理类 ADR）
- D6: **4 周内**达 Accepted（需 4-owner 共识 + 可能 ADR-088 兜底）

#### D9.3 中间状态传播

**Requirement**: 任何 ADR 状态变更（Proposed → Accepted → Superseded）必须在 4 仓的 README 索引同步更新。

---

## §2 Consequences

### 2.1 正面后果

- ✅ **统一跨仓 SSOT 来源**: ADR-077~086 所有 ADR 都有 D2/D3 政策锚点（policy/wire artifacts 责任分离）
- ✅ **v1 实施可启动**: D1-D5 + D7-D9 可立即起草为 Proposed；D6 是评审瓶颈但有退路（ADR-088）
- ✅ **评审 critical path 明确**: 4-owner review 关键路径已标注，缺席处理有 §D9.2 周期目标
- ✅ **fail-fast v1 简化**: D4/D5 排除 checkpoint / reconnect，让 v1 测试与 CI 简化
- ✅ **架构 race condition 消除**: §13.4 Phase 对齐表修正了 Phase R 与 W0 的虚假串行依赖

### 2.2 负面后果

- ⚠️ **D6 决议关键路径**: 任何 D6 后续变更将影响 §13.4 conditional gates；D6 退路需创建 ADR-088（治理开销 +1 ADR）
- ⚠️ **4-owner review 阻塞风险**: 任一仓 owner 缺席即阻塞（per Oracle §7.1 risk #1）
- ⚠️ **capability negotiation 早期不严格**: D3.2 v1 best-effort 可能在某 owner 不支持某 capability 时 silent degrade
- ⚠️ **Pointer-safety 强制重构**: 历史代码或 PTX-EMU 集成中如有 raw pointer，必须经 D2 重构

### 2.3 风险登记

| # | 风险 | 严重度 | 缓解 |
|---|------|--------|------|
| 1 | **D6 4-owner 无法达成共识** | 🔴 High | D6.3 退路：双注释并存 + 后续 ADR-088 兜底 |
| 2 | **Phase R 滑期影响**: Phase R 教训未及时集成 → W2 验收门槛 | 🟡 Med | D7.2 标注 Phase R 经验输入 W2 gate；不阻塞 |
| 3 | **Capability negotiation 早期不严格**: 隐藏兼容性问题 | 🟡 Med | v1 best-effort + log warning；v2 hard-fail via D3.4 trigger |
| 4 | **Pointer-safety 重构范围爆炸**: 历史代码全面审 | 🟠 Med-High | D2.2 列禁止项作为代码 review checklist；增量修复 |
| 5 | **D2 descriptor policy 与 ADR-077 NFA charter 边界不清** | 🟢 Low | D2.1 已明示 ownership 分工（087=policy，077=reference，078=wire）|
| 6 | **D8 ADR-011 状态过渡期间 SSOT 双引用** | 🟡 Med | D8.3 强制改写规则；评审通过时同步 |
| 7 | **D7 Phase 对齐表与 vision spec 后续修订不一致** | 🟡 Med | D9.3 中间状态传播 + 4 仓 README 同步 |

---

## §3 Migration

### 3.1 零代码（per Oracle §4.5）

本 ADR 是治理决策，**不直接产生代码**。所有实施约束通过其他 ADR（077-086）显式表达。

### 3.2 现有 ADR 修订需求

| ADR | 修订内容 | 引用本 ADR |
|------|----------|------------|
| **ADR-077** | §D5 加 IPC region descriptor 引用（per Oracle Q3 ownership rule）| D2.1 |
| **ADR-078** | §D1 改为 L1 Switch 进程拆分（不是 plugin）；§D2 加 CppLink 协议内容 | D1.1, D2, D3, D4 |
| **ADR-079** | §D4 加跨进程一致性契约（3 级 consistency per Oracle §2.5 R1）| D2 + D4.1 |
| **ADR-080** | §D2 加 IPC-readiness 检查（Unix socket + memfd + SCM_RIGHTS）| D2.1, D2.2, D7.1 |
| **ADR-081/082** | §D1-D3 签名改 descriptor-based（`hal_fabric_route(ctx, region_id, offset, size, generation)`）| D2.1, D3.1 |
| **ADR-083** | §D4 fan-out 引擎位置改为 L1 Switch 进程内 | D1.1, D7 |
| **ADR-084** | §D3 PGAS zone 与 CppLink 共享 region 关系明确 | D2.1 |
| **ADR-085** | §D3 标注 IFabricDriver 与 CppLink 正交 | D2 (policy) vs D3 (transport) |
| **ADR-086** | §trigger 表登记 D4（reconnect）+ D5（checkpoint）+ D9（cross-repo policy）| D4.4, D5.2, D9 |

### 3.3 现有 ADRs/Refs 状态过渡

- **ADR-011**: 状态待定（per D8.2：可能 Superseded 或拆分）
- **scale-up-fabric-architecture.md**: §12.1 ADR-087 行已存在（v0.3 已应用）；§14 ADR-011 引用改 ADR-087（per D8.3）
- **scale-up-fabric-adr-plan.md**: §15 修订（per Oracle C2 增 ADR-087 + 调整 077~086）

### 3.4 TADR / 姊妹 ADR 创建需求

本 ADR-087 Accepted 后触发：

1. **CppTLM ADR-SOC-07** "Multi-Process IPC Seam"（待 CppTLM owner 创建）
2. **PTX-EMU ADR-0029 §D7 修订**（"libptxemu_minimal.so 拆分"）—— per D1.3 ratification
3. **TaskRunner tadr-307 v2**（"IGpuDriver + IFabricDriver 双契约"）—— per D5 IMP-5

### 3.5 实施追踪

- **本 ADR Accepted**: 需 4-owner 共识 + §D6.3 记录 D6 决议
- **下一阶段**: scale-up v0.5 已应用；后续清理见 arch §13.1/§4.5 残留
- **Wave 0 启动（per arch v0.5 §12.6）**: W0a = 本 ADR 起草 + 4-owner 评审启动（关键路径，先行）；W0b = ADR-077/078/080/086 并行起草，其 Accepted 以本 ADR Accepted 为门

---

## §4 v2 Split Candidates（明示 out-of-v1）

以下内容**明确**不在本 ADR v1 范围：

- **D4 reconnect 协议细节**: reconnect wire spec + heartbeat 重启策略
- **D5 quiesce / checkpoint wire spec**: 跨进程 quiesce handshake + snapshot 协议
- **D3 滚动升级机制**: rolling-upgrade 状态机 + 老 major 兼容窗口
- **D9 跨仓 ADR 自动同步机制**: tooling（避免手工同步 4 仓 README）

以上项目**v2 ADR** 时另行处理；本 ADR 在 v1 状态时仅锚定原则。

---

## §5 Acceptance Criteria

本 ADR 升至 ✅ Accepted 需满足：

1. **4-owner 联合评审通过**: UsrLinuxEmu + CppTLM + PTX-EMU + TaskRunner 各 owner 签字
2. **D6 决议达成共识**: D6 提案获 4-owner 明示确认（per D6.0 选项表）
3. **D8 ADR-011 状态明确**: 全部吸收或拆分（per D8.2）
4. **D7.1 Phase 对齐表被 4 仓采纳**: scale-up v0.3 §13.4 + arch SSOT 一致
5. **D2/D3 政策可被 ADR-077~086 引用**: 各 ADR 修订草案已 cite 本 ADR D2/D3

---

## §6 References（证据与依据）

### 6.1 内部文档

- [scale-up-fabric-architecture.md](../02_architecture/scale-up-fabric-architecture.md) v0.3（§4.6 CppLink 协议 + §9 进程架构 + §12.1 ADR-087 行 + §13.4 Phase 对齐 + §15 风险 #12-15）
- [multi-process-gpu-simulator-integration.md](../02_architecture/multi-process-gpu-simulator-integration.md) v0.1（Oracle 调研综合）
- [post-refactor-architecture.md](../02_architecture/post-refactor-architecture.md) v0.1.7（项目级 SSOT）
- [AGENTS.md](../../AGENTS.md)（项目治理规则）
- [ADR-035](adr-035-governance-policy.md) ✅ §R5.1 cross-repo 协议 + §R3 一决策一 ADR
- [ADR-036](adr-036-three-way-separation.md) ✅ 3 区分原则
- [ADR-076](adr-076-gpgpu-kernel-module-ioctl.md) ✅ 跨仓 HAL 扩展流程模板
- [ADR-011](adr-011-multiprocess-support.md) 🔄 待 D8 处理
- [ats-cxl-30-implementation-research.md](../05-advanced/ats-cxl-30-implementation-research.md) v0.3
- [scale-up-fabric-research.md](../05-advanced/scale-up-fabric-research.md) v0.2

### 6.2 Oracle 评审 session

- `Oracle session ses_000f92e51`（v0.3 评审）— Q1-Q4 评审 + 8 项 IMP 闭环 + C1-C7 跨章节发现
- `Oracle session ses_00370deb6`（v0.3 修订前评审）— U-01~U-12 修订
- `Oracle task bg_33efdfc5`（vision doc 架构分析）— D1-D4 评估 + 15+ 开放问题
- `Oracle task bg_49b5caf0`（multi-process GPU sims 调研）— QEMU vhost-user + gem5+GPGPU-Sim 先例
- `Oracle task ses_0037996c5`（NVIDIA Rubin 调研）— coherent memory 实证
- `Oracle task ses_003b3fa7`（早期 multi-process 综合）— SYN-3 命名映射

### 6.3 外部依赖（4 仓协作）

- **CppTLM 仓**: `docs/soc_arch/adr/ADR-SOC-04-hsapp-cp-dispatcher-simplification.md` v1；规划中 v2
- **PTX-EMU 仓**: `docs/adr/ADR-0029-ptxemu-image-executor.md` §D7；`include/cudart/cpptlm_module.h`
- **TaskRunner 仓**: `docs/shared/adr/tadr-307-igpu-driver-kernel-module-extension.md`
- **Multi-Process Vision Spec**: `docs/superpowers/specs/2026-08-13-multi-process-gpu-simulator-vision.md`

### 6.4 标准与先例

- **QEMU vhost-user protocol**: memory region descriptor + SCM_RIGHTS fd-passing（per Oracle `bg_49b5caf0`）
- **gem5+GPGPU-Sim**: 双进程 + 共享内存 + 锁步 tick（per Oracle `bg_49b5caf0`）
- **NVIDIA MPS**: 控制 daemon + server + clients 三进程（per Oracle `bg_49b5caf0`）
- **NVIDIA IMEX**: 跨节点 fabric memory export/import + handle 生命周期（per scale-up v0.3 §10.2a）
- **NVIDIA Rubin coherent model**: GPU↔GPU 软件管理一致性；CPU↔GPU NVLink-C2C/ATS 硬件相干（per Oracle `ses_0037996c5`）

---

## §7 维护

- **Owner**: UsrLinuxEmu Architecture Team（primary）；4-owner 联合（review）
- **状态转换**: 🔄 Proposed → ✅ Accepted（4-owner consensus）→ 🚫 Superseded（v2 ADR 接续时）
- **下次 review 触发**: 
  - 任何 4-owner owner 提出修改
  - D6 决议变化
  - 新 TADR / 姊妹 ADR 创建时同步
  - Phase R/1/2/3 ship 状态变化
  - ADR-011 状态迁移（D8）
- **跨仓同步**: 本 ADR 状态变更必须同步更新 `docs/00_adr/README.md` + `docs/README.md` ADR 数量统计 + `docs/02_architecture/post-refactor-architecture.md` 引用

---

**文档版本**: v0.1 Proposed
**下次更新触发**: 4-owner 评审反馈 / D6 决议 / 任一关联 ADR Accepted
**维护者**: UsrLinuxEmu Architecture Team
**最后更新**: 2026-08-14（基于 Oracle 评审 `ses_000f92e51` 起草）