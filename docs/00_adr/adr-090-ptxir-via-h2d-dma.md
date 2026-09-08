# ADR-090: PTXIR Image Loading via CppTLM H2D DMA（Supersedes ADR-076 v2）

**状态**: 🔄 Proposed（2026-08-17 初版；**Gate 进度** #1 HAL append-only ✅ / #5 UsrLinuxEmu Architecture Team review ✅（2026-08-17 user ack）/ #6 Oracle 复审 ✅ PASS（session `ses_ff1f38c57ffevzUfohqp45av02` + `ses_ff1ecf07cffe5D3lwDqOmgFdyx`）；待 #3 PTX-EMU owner ack + #4 TaskRunner owner ack + #2 CppTLM maintainer ack（三方跨仓 ack 由本 ADR §Migration M5 跨仓协调 annex 启动）
**日期**: 2026-08-17
**提案人**: Sisyphus（基于 ADR-088 v0.5 实施路径 + Oracle 评审 session `ses_ff2106f84ffeM2oItBEa9iu4hL`）
**评审者**:
- UsrLinuxEmu Architecture Team（owner 评审 — HAL append-only + 层次修复合规性）
- CppTLM maintainer（评审 — SM executor 职责归属 + ABI 扩展）
- PTX-EMU owner（评审 — 集成点迁移 + 子模块化承诺）
- TaskRunner owner（评审 — tadr-308 consumer-side 迁移可行性）

**关联 ADR**:
- [ADR-076](adr-076-gpgpu-kernel-module-ioctl.md) 🚫 **Superseded v2**（**本 ADR 是其修订替代**；ADR-076 已 ship 产物的退役路径见 §Migration）
- [ADR-088](adr-088-dgpu-complete-simulation.md) ✅ CppTLM dGPU 板卡仿真（**§C2 修订注记 + §D6.2 扩展 SM executor +2~3 ABI**，本 ADR 修订后）
- [ADR-036](adr-036-three-way-separation.md) ✅ 3 区分架构原则（**本 ADR 恢复严格遵守**）
- [ADR-023](adr-023-hal-interface.md) ✅ HAL append-only 治理（fn-ptrs 3→1，#67/#68 deprecated stub）
- [ADR-035](adr-035-governance-policy.md) ✅ 治理规则（本 ADR 走 §R5.1 cross-repo 流程）
- [ADR-061](adr-061-hal-iommu-extension.md) ✅ HAL IOMMU ops 扩展（**模式借鉴** — append-only + hal_mock + hal_user stub）

**关联外部 ADR / TADR**:
- [PTX-EMU ADR-0029 §D8](../external/PTX-EMU/docs/adr/ADR-0029-ptxemu-image-executor.md#d8-cp-端集成约定--hal-扩展方案usrlinuxemu--ptx-emu-跨仓契约) 📋 **待 §D8 amendment**：集成点从 UsrLinuxEmu HAL → CppTLM 子模块
- [TaskRunner tadr-307](../external/TaskRunner/docs/shared/adr/tadr-307-igpu-driver-kernel-module-extension.md) 📋 **待 Supersede → 新 tadr-308**

**关联 Source Oracle Session**:
- [ses_ff2106f84ffeM2oItBEa9iu4hL](https://oracle-sessions/...) — 2026-08-17 架构评审 PASS-WITH-CONDITIONS（条件全部已合入本 ADR）

**修订记录**:
- 2026-08-17 v1：初版草案（基于 Oracle 评审 session `ses_ff2106f84ffeM2oItBEa9iu4hL` 输出）

---

## Context

### C1: ADR-076 v1 层次违规 — Oracle 评审识别

[ADR-076 v1](adr-076-gpgpu-kernel-module-ioctl.md) 已在 2026-08-13 升 ✅ Accepted 并 ship 实施（3 个 ioctl 0x27/0x28/0x29 + 3 个 HAL fn-ptr #66/#67/#68 + `hal_user.cpp` dlopen `libptxemu_device.so` + 144 ctest）。但 Oracle session `ses_ff2106f84ffeM2oItBEa9iu4hL`（2026-08-17）识别一个**架构层次违规**：

**问题**：`sim/hardware/hardware_puller_emu.cpp` 是 PM4/AQL packet 调度器（FSM: IDLE→FETCH→DECODE→SCHEDULE→DISPATCH→COMPLETE，analogous to AMD MES / NVIDIA GSP-RM Puller），**不是** SM 内部 kernel 调度器。sim/ 内**不存在** SM 指令执行代码（DISPATCH 状态对 `GPU_OP_LAUNCH_KERNEL` 仅调用 `scheduler_->translateLaunch()` 回调，line 273-276）。同时 `hal_user.cpp`（line 797-846）让 HAL 桥直接 dlopen 外部硬件行为库 `libptxemu_device.so` 并同步执行 kernel。

**结果**：HAL 从"②③ 之间的桥"变成了"硬件行为提供者"——`struct gpu_hal_ops` 不仅承载 fn-ptr 派发，还承载 PTX-EMU 进程的 dlsym 生命周期管理、handle 分配、错误码映射。这违反 [ADR-036](adr-036-three-way-separation.md) 严格遵守的"HAL 是桥不是第 4 层"判定。

**层次判断矩阵**（Oracle 评审输出版本）：

| 层级 | 仿真组件 | 模拟内容 | 真实硬件对标 |
|------|---------|---------|------------|
| L1 PCIe 板卡 | `libcpptlm_emulator.so` (per ADR-088) | BAR MMIO + PCIe Config Space + MSI-X + DMA translate | dGPU 板卡 |
| L2 PM4 packet | `sim/hardware/hardware_puller_emu` | PM4/AQL packet 调度（FETCH→DECODE→SCHEDULE→DISPATCH→COMPLETE） | AMD MES / NVIDIA GSP Puller |
| L3 SM/CU 调度 | **无独立仿真**（由 PTX-EMU 隐含执行） | 由 PTX-EMU 接管 | SM scheduler (Wave Dispatcher) |
| L4 SM ISA 执行 | `libptxemu_device.so` (per ADR-076 → 修订后属 CppTLM submodule) | PTX → 硬件 ISA 翻译 + ISA 执行 | SASS/GCN/RDNA ISA 物理执行 |

**关键洞察**：PTX-EMU 仿真的是 **L3 (SM wave dispatch) + L4 (ISA 执行)**，**不是 dGPU 板卡**。它既不是 PCIe 板卡仿真（CppTLM），也不是 Linux kernel env sim（UsrLinuxEmu），而是 **SM/CU 内部的仿真**，按层次归属应在 dGPU 板卡仿真范畴（= CppTLM 内部 submodule）。

### C2: 真实 CUDA 栈的层次验证

真实 GPU driver 中 kernel module 处理流程（AMD/NVIDIA 通用）：
1. **UMD 解析**（CUDA Runtime / HIP Runtime / OpenCL ICD，用户态）：
   - `cuModuleLoadData(image)` 解析 PTX/CUBIN 头，提取 kernel 名与 entry offset
   - `cuModuleGetFunction()` 返回 function handle（含 entry offset）
2. **驱动加载**（kernel-mode driver，Linux 内核）：
   - 通过 ioctl 把 image bytes 拷贝到 device VRAM（H2D DMA）
   - **不解析** image header；只搬运 bytes
3. **launch**（驱动 + 硬件）：
   - 驱动写 DISPATCH_KERNEL packet 到 ring buffer（GPFIFO/UMQ），payload 含 code gpu_va + entry offset + grid/block/args
   - 硬件 Puller 解析 packet → 触发 SM/CU 调度 → SM 执行 PTX-ISA → interrupt/DMA 完成通知

**结论**：**PTXIR header 解析是 UMD 职责，不是 driver 职责**。当前 ADR-076 ioctl `gpu_load_kernel_module_args` 含 `kernel_name[256]` 字段是层次错位——kernel name 应在 UMD 侧解析（TaskRunner / cu_module.cpp），不通过 driver ioctl 上传。

### C3: ADR-088 Phase 1 路径 + Mode A 回退

[ADR-088](adr-088-dgpu-complete-simulation.md) v0.5 已 Accepted，定义 CppTLM 23 ABI（含 PCIe + CP + H2D/D2H DMA）。理论上层次修复**严格依赖** ADR-088 Phase 1 完成（打通 H2D DMA path），但 Oracle 识别出 **Mode A 回退路径**：

- 现有 `sim/hardware/hardware_puller_emu.cpp` 的 Puller MEMCPY 分支已使用 `hal_mem_write`（line 260-261）
- 新 `kernel_module_load` 语义（写 VRAM + icache flush）可以**先对 sim/ HAL heap 实现**
- PTX-EMU 可以先挂到 sim/ 的 `translateLaunch` 回调后面作为 **mode A** 执行
- CppTLM 就绪后平移（**mode B**）

**结论**：**层次修复（UsrLinuxEmu 侧）与 CppTLM 接管（板卡侧）是可分离的两件事**。用户原始"等 ADR-088 Phase 1 完成再开工"的判断过于保守——Mode A 解耦可节省 4-6 周串行时间。

**Mode A 当前状态澄清**（2026-08-17 Oracle 评审识别）：本次实施后 Mode A 的**实际状态是 kernel 执行路径完全不存在**（ioctl 0x28 返回 `-ENOSYS`，`translateLaunch` 回调后面未挂任何 PTX-EMU 实现）。Mode A 作为"可执行的 interim 路径"在当前代码里**并未落地**——落地的是"加载路径修复 + 执行路径故意留空（等 DISPATCH_KERNEL packet 路径 wire-up）"。

**因此 §C3 中 "Mode A 节省 4-6 周串行时间"的论证仅对加载路径成立**——执行路径的端到端打通仍需后续工作（PTX-EMU 挂到 `translateLaunch` 回调 + `DISPATCH_KERNEL` packet payload 含 vram_addr）。Mode A 的 PTX-EMU 挂接为**后续任务**，不在本 ADR 实施范围。

### C4: 跨仓契约修订需求

本 ADR 触发 3 个外部仓的契约修订：
1. **PTX-EMU ADR-0029 §D8**：当前指向 UsrLinuxEmu HAL 扩展方案 → 修订为指向 CppTLM 内部 API
2. **TaskRunner tadr-307**：当前 mirror 3 个 HAL fn-ptrs → 修订为 tadr-308（仅 1 个 fn-ptr）
3. **CppTLM**：新增 SM executor 章节 + 23 ABI 扩展（**+2~3 ABI**：image install / dispatch / completion callback），需走 ADR-088 §D6.2 BREAKING 流程

### C5: ADR-088 §C2 必须修订

[ADR-088 §C2 line 63](adr-088-dgpu-complete-simulation.md#L63) 当前明确：

> "与 ADR-076（PTX-EMU HAL Backend）的关系：两者**共存不冲突**——`hal_user.cpp` 可同时配置 PTX-EMU backend（`libptxemu_device.so`）与 CppTLM backend（`libcpptlm_emulator.so`），由不同 env var 触发，互不干扰；**Kernel Module 路径仍走 PTX-EMU（不被本 ADR 取代）**。"

本 ADR **直接推翻此条款**——Kernel Module 路径不再走 UsrLinuxEmu HAL PTX-EMU，而是通过 CppTLM H2D DMA + DISPATCH_KERNEL packet 完成。需修订 ADR-088 §C2 + 同步 ADR-076 §演进路线图 (a)(b) 决策（"维持共存/不新增 fn-ptr"被推翻）。

---

## Decision

### D1: HAL fn-ptrs 3→1（#66 保留，#67/#68 deprecated stub）

**保留 #66 (`kernel_module_load`)**：

```c
/* ADR-090 修订后：#66 保留，重定义为 VRAM 写入 + icache invalidate */
int (*kernel_module_load)(void *ctx,
                          const uint8_t* image_bytes, size_t image_size,
                          uint64_t* out_vram_addr);  /* 改：返回 VRAM GPU VA */
```

- **保留理由**：代码 BO 在真实 GPU 上有特殊处理——装载后必须 **icache invalidate** 才能 dispatch。一个专用 fn-ptr 把"VRAM 写入 + icache flush"封装为原子语义，比让 drv 组合"ALLOC_BO + H2D + 裸 flush"更贴近真实硬件
- **签名变更**：`out_module_handle + kernel_name` → `out_vram_addr`（UMD 侧解析 kernel name + entry offset，不通过 driver ioctl）

**删除 #67 (`kernel_module_execute`)**：

- 功能**完全**走 `GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH` (0x07) 的 `GPU_OP_DISPATCH_KERNEL` packet
- struct 槽位**保留**（避免破坏 struct layout 与 §D6 未来兼容性），实现返回 `-ENOSYS`，标注 `/* deprecated by ADR-090 — use PUSHBUFFER_SUBMIT_BATCH with DISPATCH_KERNEL opcode */`
- 编号永久不复用（per ABI 卫生规则）

**删除 #68 (`kernel_module_unload`)**：

- 功能走现有 `GPU_IOCTL_FREE_BO` (driver 侧组合 BO free 路径，无新硬件行为)
- struct 槽位保留，实现 `-ENOSYS`，标 deprecated

### D2: ioctl 0x27/0x28/0x29 语义重定义

| ioctl | 修订前 | 修订后 | 编号保留 |
|-------|--------|--------|---------|
| **0x27 `GPU_IOCTL_LOAD_KERNEL_MODULE`** | 返回 opaque handle + `kernel_name[256]` | 返回 `vram_addr`（code BO 的 GPU VA）+ `image_size` | ✅ 永久保留 |
| **0x28 `GPU_IOCTL_LAUNCH_KERNEL_MODULE`** | 调 PTX-EMU execute | **删除功能**；handler 返回 `-ENOSYS`（明确信号迫 consumer 迁移），一个 release 后改 `-ENOTTY` | ✅ 永久保留（防复用） |
| **0x29 `GPU_IOCTL_UNLOAD_KERNEL_MODULE`** | 调 PTX-EMU unload | 语义改：释放 code BO（基于 `vram_addr`），driver 走 `FREE_BO` 路径 | ✅ 永久保留 |

**`gpu_load_kernel_module_args` 结构体重定义**：

```c
struct gpu_load_kernel_module_args {
    uint64_t image_ptr;            // 用户态 image buffer (PTXIR or PTXIR-Embedded CUBIN)
    uint64_t image_size;
    uint64_t out_vram_addr;        // 改：code BO 的 GPU VA（替代 opaque handle）
    /* kernel_name[256] 字段删除 — 由 TaskRunner UMD 侧解析 */
};
```

### D3: PTX-EMU 作为 CppTLM Submodule（不是 UsrLinuxEmu HAL Backend）

- **Mode A（interim）**：PTX-EMU 移出 `hal_user.cpp`，挂到 `sim/` 的 `translateLaunch` 回调后面（与现有 sim/ hardware 路径一致）
- **Mode B（终态）**：PTX-EMU 作为 CppTLM submodule（per ADR-088 Phase 4 集成窗口 Week 12-21）
- Mode A 与 Mode B **不冲突**——同一测试集在两种模式下应全 PASS（Oracle 推荐双模式回退，per Q6 风险矩阵）

**UsrLinuxEmu 进程不再链接 `libptxemu_device.so`**：
- `hal_user.cpp` 删除 `g_ptxemu_abi` 全局状态 + `dlopen` 逻辑 + 5 个 ABI 函数声明
- `hal_user.cpp` #66 实现改为：**H2D DMA 写入 VRAM**（Mode A 走 sim/ HAL heap，Mode B 走 CppTLM ABI）

### D4: Cross-Repo 契约修订

| 仓 | 动作 | Owner |
|----|------|-------|
| **PTX-EMU** | ADR-0029 §D8 amendment：集成点从 UsrLinuxEmu HAL → CppTLM submodule；5 个 ABI 函数**原样保留**（image_load/kernel_name/execute/unload/module_version），仅换调用方 | PTX-EMU owner |
| **TaskRunner** | tadr-307 Superseded → 新 tadr-308：删除 3 纯虚方法（#48-50）；`cuModuleLoadData` 改 UMD 侧组合（解析 header + alloc VRAM + 写 bytes）；`cuLaunchKernel` 改发 DISPATCH_KERNEL pushbuffer entry；**UMD 侧新增 PTX 解析能力**（或直链 libptxemu 作 UMD 辅助库） | TaskRunner owner |
| **CppTLM** | handoff spec v5.0：新增 SM executor + PTX-EMU 子模块章节；23 ABI 扩展 **+2~3 ABI**（image install / dispatch / completion callback）；走 ADR-088 §D6.2 BREAKING 流程 + minor version 升级 + ADR-088 §C2 + §D6.2 修订注记 | CppTLM maintainer |

### D5: ADR-088 §C2 + §D6.2 修订注记

- **ADR-088 §C2 line 63 修订**：移除"Kernel Module 路径仍走 PTX-EMU（不被本 ADR 取代）"条款，改为"PTXIR image 加载由 UsrLinuxEmu 走 H2D DMA → CppTLM VRAM；kernel 执行由 CppTLM SM executor（PTX-EMU submodule）承担"
- **ADR-088 §D6.2 修订注记**：23 ABI 冻结声明扩展为"+23+2~3 ABI"（SM executor 子系统）；明确走 BREAKING 流程（minor version 升级 + `tools/docs-audit.sh` BREAKING 检测）

---

## Consequences

### 正面后果

- ✅ **ADR-036 严格遵守**：HAL 恢复"桥"职责（②③ 之间 fn-ptr 派发），不承担硬件行为提供者角色
- ✅ **UsrLinuxEmu 职责清晰**：只仿真 Linux kernel env + 提供可移植 driver 代码；不感知 SM/ISA 概念
- ✅ **CppTLM 拥有完整 dGPU 仿真栈**：板卡 + CP + SM executor（PTX-EMU submodule），符合真实 GPU 硬件拓扑
- ✅ **HAL contract 大幅简化**：3 fn-ptrs → 1（#67/#68 deprecated stub 仅为 ABI 卫生保留槽位）
- ✅ **真实硬件路径对齐**：driver 把 image 写 VRAM + DISPATCH_KERNEL packet → SM 执行（与真硬件完全等价）
- ✅ **跨仓契约明确**：PTX-EMU 单一 caller（CppTLM），不再与 UsrLinuxEmu 直接 ABI 耦合
- ✅ **mode A/mode B 解耦**：层次修复可与 CppTLM 交付并行（节省 4-6 周串行时间）

### 负面后果

- ⚠️ **ADR-076 已 ship 实施需要退役**：144 ctest 拆分（ioctl 契约保留 + PTX-EMU 行为测试迁出本仓 + LAUNCH 功能测试删除）
- ⚠️ **23 ABI 冻结声明被打破**：+2~3 ABI（SM executor）需走 ADR-088 §D6.2 BREAKING 流程；CppTLM maintainer 主笔
- ⚠️ **跨仓协调成本**：PTX-EMU + TaskRunner 各需修订 ADR/TADR；commit 顺序必须按 ADR-035 §R5.1 严格执行
- ⚠️ **TaskRunner UMD 职责新增**：PTXIR header 解析下沉到 TaskRunner；TaskRunner owner 需评估新增 PTX 解析能力或直链 libptxemu
- ⚠️ **ioctl 0x28 现有 consumer 需迁移**：返回 `-ENOSYS` 给出明确信号，但 TaskRunner 必须在同 release ship tadr-308
- ⚠️ **mode A/mode B 双执行路径行为分歧风险**：需通过 ADR-088 Gate 4.7 模式（同一测试集双模式 PASS）验证
- ⚠️ **0x27 `_IOWR` encoded request value 因 struct 缩小而变化**：删除 `kernel_name[256]` 使 `gpu_load_kernel_module_args` 从 ~280B 缩到 24B，**`_IOWR(G, 0x27, ...)` 编码后的完整 request 值随之改变**。nr 编号（0x27/0x28/0x29）永久保留，但旧二进制若直接调旧 header 编译产物会落到派发表外（`-ENOTTY`）。仓内已确认 TaskRunner 代码无引用这些 struct（tadr-307 仅有文档无代码），共享头文件 symlink 机制保证同步重编译，**实际无受害者**；此处显式承认以保文本精确。

### 风险矩阵

| 风险 | 概率 | 影响 | 缓解 |
|------|:--:|:--:|------|
| ADR-088 Phase 1 H2D DMA 出问题 | 中 | 中 | Mode A 回退：新 load 语义先对 sim/ HAL heap 实现（已有 `hal_mem_write` 路径证明可行），ADR-090 实施不被阻塞 |
| CppTLM 集成 PTX-EMU 延期 | 中 | 中 | 同上——层次修复与 CppTLM 接管解耦；mode A 长期可用，mode B 是增强而非必需 |
| tadr-307 现有 consumer (`cuModuleLoadData` 已接通) 断裂 | 高（必然） | 中 | 0x28 stub 返回 `-ENOSYS` 给出明确信号；tadr-308 与 ADR-090 同 release 窗口 ship；`PTXEMU_ROOT` 直链路径（UMD 侧）作过渡逃生舱 |
| 向后兼容（已依赖 0x28 同步语义的用户） | 低 | 低 | 该 ABI 仅 TaskRunner 一个 consumer （共享头文件机制），无第三方；0x27/0x28/0x29 编号永久保留防复用 |
| 23 ABI 冻结被打破引发治理争议 | 中 | 低 | ADR-090 中显式声明"+2~3 ABI 走 ADR-088 §D6.2 BREAKING 流程"，前置沟通 CppTLM maintainer |
| mode A/mode B 双执行路径行为分歧 | 低 | 中 | ADR-088 Gate 4.7 模式：同一测试集在 mode A（sim+PTX-EMU）与 mode B（CppTLM+PTX-EMU）下全 PASS + 切换 100 次无回归 |
| kernel_name 字段删除破坏 TaskRunner 现有实现 | 中 | 中 | tadr-308 同步 ship；TaskRunner UMD 侧新增 PTX 解析能力（Oracle 开放问题 1）|

---

## Migration / 实施步骤

### Phase M1: 文档 SSOT 同步（本周，纯文档零代码风险）

1. 创建本 ADR-090（canonical 源）
2. ADR-076 添加 🚫 Superseded 头注（指向 ADR-090）
4. ADR-088 §C2 line 63 + §D6.2 修订注记
5. `docs/00_adr/README.md` 索引更新（ADR-090 entry + ADR-076 状态）

### Phase M2: 共享头文件 ABI 修订（本月）

1. `plugins/gpu_driver/shared/gpu_ioctl.h`：
   - `gpu_load_kernel_module_args`：删除 `kernel_name[256]`，新增 `out_vram_addr`
   - `GPU_IOCTL_LAUNCH_KERNEL_MODULE`：标 `/* deprecated by ADR-090 */`
2. `plugins/gpu_driver/hal/gpu_hal.h`：
   - #66 fn-ptr 注释修正（返回 `out_vram_addr`，描述 VRAM + icache invalidate 语义）
   - #67/#68 标 `/* deprecated by ADR-090 — return -ENOSYS */`

### Phase M3: 实现层修订（本月）

1. `plugins/gpu_driver/hal/hal_user.cpp`：
   - 删除 `g_ptxemu_abi` 全局状态（5 个 fn-ptr + dlopen handle）
   - 删除 `ensure_ptxemu_abi_loaded()` 函数
   - 删除 `hal_user_kernel_module_load/execute/unload` 的 dlsym 实现
   - 重写 `kernel_module_load` 实现为 **H2D DMA 写入 VRAM**（Mode A 走 sim/ HAL heap 路径，Mode B 走 CppTLM ABI）
   - `kernel_module_execute/unload` 实现改为 `-ENOSYS` + 标 deprecated
2. `plugins/gpu_driver/hal/hal_mock.cpp`：
   - 保留 `kernel_module_load` VRAM-write mock
   - `kernel_module_execute/unload` 改为 `-ENOSYS` mock
   - 标 `hal_mock_inject_ptxemu_error/clear_ptxemu_errors/get_ptxemu_unload_call_count/reset_ptxemu_unload_counter` 为 deprecated
3. `plugins/gpu_driver/drv/gpgpu_device.cpp`：
   - `handle_load_kernel_module`：移除 kernel_name 处理，调用 `hal_kernel_module_load` 写入 VRAM
   - `handle_launch_kernel_module`：返回 `-ENOSYS`，填充 `launch_status = -ENOSYS`
   - `handle_unload_kernel_module`：通过现有 BO free 路径释放 code BO

### Phase M4: 测试拆分（本月）

1. **ioctl 契约测试**（UsrLinuxEmu 仓）：改写保留
   - `test_gpu_ioctl_kernel_module_load_vram.cpp`（新）：验证 0x27 返回 vram_addr
   - `test_gpu_ioctl_kernel_module_launch_stub.cpp`（新）：验证 0x28 返回 -ENOSYS
   - `test_gpu_ioctl_kernel_module_unload.cpp`（新）：验证 0x29 释放 code BO
2. **PTX-EMU 行为测试**（迁移到 PTX-EMU/CppTLM 仓）：删除
   - `test_hal_ptxemu_load_failure_*`（8 个 cudaError_t 测试）
   - `test_hal_ptxemu_unload_busy.cpp`
   - `test_hal_ptxemu_version_mismatch.cpp`
   - `test_hal_ptxemu_dlsym_missing.cpp`
3. **LAUNCH 功能测试**（删除）：替换为 pushbuffer DISPATCH_KERNEL 端到端测试

### Phase M5: 跨仓协调（本月 - 下月）

1. PTX-EMU ADR-0029 §D8 amendment 草案
2. TaskRunner tadr-308 草案（cuModuleLoadData UMD-side 解析 + DISPATCH_KERNEL pushbuffer entry）
3. CppTLM handoff spec v5.0（SM executor 章节 + ABI 扩展）

### Phase M6: 构建验证（本月）

1. cmake + make（确认所有目标编译通过）
2. ctest（确认 ioctl 契约测试 + 现有 98 测试不受影响）

### Phase M7: Oracle 复审 + 迭代（本月）

1. Oracle 评审 session（PASS / NEEDS-FIX）
2. NEEDS-FIX 时按 Phase 1-6 顺序修复
3. PASS 时 ADR-090 升 ✅ Accepted

### Phase M8: Mode B 切换（ADR-088 Phase 4 窗口，Week 12-21）

1. CppTLM 完成 SM executor submodule 集成
2. 23+2~3 ABI ship（走 ADR-088 §D6.2 BREAKING 流程）
3. Mode A 路径保留为回退（Gate 4.7 双模式）

### Phase M9: 清理 deprecated hal_mock injection helpers（后续任务）

ADR-090 实施保留 4 个 `hal_mock_inject_ptxemu_*` 注入 helper（无 caller 的 dead code）以缓和现有测试迁移。本实施范围**仅标 deprecated**，正式清理留给后续 release（预计 M8 完成后一 release cycle）。清理范围：
- `hal_mock_inject_ptxemu_error(handle, cuda_error)`
- `hal_mock_clear_ptxemu_errors()`
- `hal_mock_get_ptxemu_unload_call_count(handle)`
- `hal_mock_reset_ptxemu_unload_counter(handle)`

跟踪方式：建议建 issue `#adr-090-cleanup-hal-mock-injection-helpers` 跟踪。

---

## Acceptance Gate

本 ADR 由 Proposed → Accepted 必须满足：

1. ✅ **HAL append-only 合规**：3 fn-ptrs → 1 + 2 deprecated stub（#67/#68 struct 槽位保留）
2. ⏳ **CppTLM maintainer ack**：SM executor 职责归属确认 + ABI 扩展 (+2~3) 走 BREAKING 流程 ack
3. ⏳ **PTX-EMU owner ack**：集成点迁移 UsrLinuxEmu HAL → CppTLM submodule 确认 + ADR-0029 §D8 amendment 起草
4. ⏳ **TaskRunner owner ack**：tadr-308 计划可行 + UMD 侧新增 PTX 解析能力评估
5. ✅ **UsrLinuxEmu Architecture Team review**：本 ADR 内容 + Mode A 实施路径 review PASS（2026-08-17 user ack）
6. ✅ **Oracle 最终复审 PASS**：Phase M3-M6 实施产出 + 测试拆分后 ctest 全 PASS（session `ses_ff1f38c57ffevzUfohqp45av02` + `ses_ff1ecf07cffe5D3lwDqOmgFdyx`）

通过全部 6 项 gate 后状态升 ✅ Accepted。**当前进度 3/6（#1/#5/#6 ✅, #2/#3/#4 ⏳ 待跨仓 ack）**。

---

## Open Questions

1. **TaskRunner UMD 侧 PTX 解析能力**：TaskRunner owner 接受新增 UMD 职责吗？是否直链 libptxemu 作 UMD 辅助库（避免重复造 PTX parser）？— **待 TaskRunner owner 评估**
2. **Mode A 路径保留期限**：Mode B（CppTLM）验证通过后，sim/ 的 PTX-EMU 挂载点是永久保留（Gate 4.7 双模式）还是计划性拆除？— **待 CppTLM maintainer 评估**
3. **SM executor ABI 具体形状**（image install / dispatch / completion callback）：需与 CppTLM 的 CP/SDMA FSM side-effect 模型对齐——**由 CppTLM maintainer 在 handoff spec v5.0 中主笔**，UsrLinuxEmu 只做 consumer-lens 评审

---

## References

- [ADR-036 — 3-Way Separation](adr-036-three-way-separation.md)
- [ADR-023 — HAL Append-Only Governance](adr-023-hal-interface.md)
- [ADR-061 — HAL IOMMU Extension](adr-061-hal-iommu-extension.md)（append-only + hal_mock + hal_user stub 模式参考）
- [ADR-076 — GPGPU Kernel Module IOCTL (🚫 Superseded v2)](adr-076-gpgpu-kernel-module-ioctl.md)
- [ADR-088 — CppTLM dGPU Complete Simulation](adr-088-dgpu-complete-simulation.md)
- [PTX-EMU ADR-0029 §D8](../external/PTX-EMU/docs/adr/ADR-0029-ptxemu-image-executor.md#d8-cp-端集成约定--hal-扩展方案usrlinuxemu--ptx-emu-跨仓契约)（待 amendment）
- [TaskRunner tadr-307](../external/TaskRunner/docs/shared/adr/tadr-307-igpu-driver-kernel-module-extension.md)（待 Supersede → tadr-308）
- Oracle Session `ses_ff2106f84ffeM2oItBEa9iu4hL` — 2026-08-17 评审输出
- [core-architecture.md](../02_architecture/core-architecture.md) — 核心架构 SSOT

---

**维护者**: UsrLinuxEmu Architecture Team（canonical source）
**最后更新**: 2026-08-17（v1 草案）
**关联 Change**: 待创建 `openspec/changes/2026-08-17-adr-090-ptxir-via-h2d/`（实施 spec）
**下一步**: CppTLM/PTX-EMU/TaskRunner 三方 ack → 实施 Mode A → Oracle 复审 → Accepted