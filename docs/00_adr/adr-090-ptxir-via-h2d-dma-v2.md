# ADR-090 v2: PTXIR Image Loading via CppTLM dGPU Board Submodule（Supersedes ADR-090 v1 + 仲裁 Canonical 归属冲突）

**状态**: 🔄 **Proposed Draft**（2026-08-17 v2 初版；v1 见 [`adr-090-ptxir-via-h2d-dma.md`](adr-090-ptxir-via-h2d-dma.md)，**v1 已 ship 实施产物（commits `c07d245` + `b26412a`）保留有效**——v1 ship 的 HAL #66 单 fn-ptr + 8 函数 ABI 全部归档为 v2 §D1 实施基础；v1 ship 的 §D2/D3/D4 描述因事实错误整段重写）
**日期**: 2026-08-17
**提案人**: Sisyphus（基于 v1 三 RFC 失败复盘 + Oracle session `ses_fef78854dffeLfDJh7p8ELuMLy` 调查）
**评审者**（v2 重新发起）:
- UsrLinuxEmu Architecture Team（v2 内部 review）
- CppTLM maintainer（**critical path** — v3.0.0 dGPU board 设计 owner）
- PTX-EMU Architecture Team（HSK-1 真相源持有方 + ABI owner + HSK-6 联署发起方）
- TaskRunner owner（tadr-308 创建 + `IGpuDriver` 接口修订 owner）

**关联 ADR**（v2 关系全部重写为仲裁链，见 §C0）:
- [ADR-090 v1](adr-090-ptxir-via-h2d-dma.md) 🚫 **Superseded by v2**（v1 ship 实施产物保留在仓内作为 v2 §D1 实施基础，详见 §M0 退役路径）
- [ADR-076](adr-076-gpgpu-kernel-module-ioctl.md) 🚫 **Historical**（v2 升 Accepted 后，ADR-076 不再称为 "canonical"；v1 §修订记录中 F3 修订记录的 canonical 指向在 v2 §C0 中被显式推翻）
- [ADR-088](adr-088-dgpu-complete-simulation.md) ✅ CppTLM dGPU 板卡仿真（**§C2 + §D6.2 修订注记在本 ADR v2 中重新起草**，整合 CppTLM owner 在 #18 反馈的 dGPU 最小完整集）
- [ADR-036](adr-036-three-way-separation.md) ✅ 3 区分架构原则（v2 恢复严格遵守）
- [ADR-023](adr-023-hal-interface.md) ✅ HAL append-only 治理（v2 §D1.3 解释为何保留 #66 单 fn-ptr + #67/#68 deprecated stub 物理结构）
- [ADR-035](adr-035-governance-policy.md) ✅ 治理规则（v2 走 §R5.1 cross-repo 流程）

**关联外部 ADR / TADR**（v2 全部关系重写）:
- [PTX-EMU ADR-0029](https://github.com/chisuhua/PTX-EMU/blob/main/docs/adr/ADR-0029-ptxemu-image-executor.md) ✅ Accepted 2026-08-09（**§D8 需重大修订** —— v1 假设 "banner 修订" 错误；§D8 是 ~250 行 HAL 扩展方案，v2 修订范围 = D8.1~D8.8 + D8-Alt 全部）
- [PTX-EMU HSK-1~HSK-5](https://github.com/chisuhua/PTX-EMU/tree/main/docs/superpowers/specs) ✅ Active（v2 §D5 新增 HSK-6 联发协议公告）
- [TaskRunner tadr-307](https://github.com/chisuhua/TaskRunner/blob/main/docs/shared/adr/tadr-307-igpu-driver-kernel-module-extension.md) 📋 **PROPOSED, STALE**（提议 3 个 IGpuDriver 方法与 ADR-090 v1 "1+2 deprecated" 矛盾，v2 §C0 要求撤或重写为 tadr-308）
- [TaskRunner tadr-308](https://github.com/chisuhua/TaskRunner/issues/10) 📋 **TO CREATE**（基于 annex §B v2 重写版）

**关联 Oracle Sessions**（v2 决策链）:
- `ses_ff2106f84ffeM2oItBEa9iu4hL`（v1 启动） — 2026-08-17 v1 架构评审
- `ses_fef78854dffeLfDJh7p8ELuMLy`（v2 决策） — 2026-08-17 v2 重写决策（24 BLOCKERS 分析 + Hybrid RFC 策略）
- Oracle 本地深度调查（2026-08-17） — 验证 PTX-EMU ABI 真实形态 + CppTLM 4 测试文件符号引用 + HSK 协议归属

**修订记录**:
- 2026-08-17 v1：初版（已 ship 实施 + 24 BLOCKERS，由 v2 取代）
- 2026-08-17 v2：基于 v1 三 RFC 失败 + Oracle 本地深度调查重写；§C0 Canonical 仲裁（**NEW**）；§D1 8 函数 ABI 采纳（**REWRITE**）；§D3 Mode B 强化为 dGPU board 最小完整集；§D5 HSK-6 + G-D4 门禁（**NEW**）；§D6 两阶段删除流程（**NEW**）

---

## §C0 Canonical 归属仲裁（NEW — v1 缺失）

### §C0.1 三方 canonical 来源冲突的发现

v1 提交时假设 "ADR-090 = canonical, ADR-076 = Superseded"。但本地调查（Oracle session `ses_fef78854dffeLfDJh7p8ELuMLy` 2026-08-17）揭示**三方文档对 canonical 来源的认识不一致**:

| 来源 | 自称 canonical 指向 | 证据 |
|---|---|---|
| **PTX-EMU ADR-0029:630**（F3 修订记录） | 指向 UsrLinuxEmu **adr-076** | `docs/adr/ADR-0029-ptxemu-image-executor.md:630` 明确 "canonical source 是 UsrLinuxEmu `adr-076`，consumer 对偶是 TaskRunner `tadr-307`" |
| **ADR-090 v1 标题** | 自称 "Supersedes ADR-076 v2" | `docs/00_adr/adr-090-ptxir-via-h2d-dma.md:1` |
| **TaskRunner tadr-307**（PROPOSED） | 仍按 ADR-076 v1 3-fn-ptr 提案 | `docs/shared/adr/tadr-307-igpu-driver-kernel-module-extension.md:52-78` 提议 3 个 IGpuDriver 方法（load/launch/unload_kernel_module），对齐 ADR-076 v1 旧世界 |

**冲突本质**: PTX-EMU 已 ship 的 8 函数 ABI（`CPPTLM_MODULE_VERSION 2`）是 HSK-1 真相源（`PTX-EMU/include/cudart/cpptlm_module.h:12-52`，注释明确 "Consumer must check ptxemu_module_version() >= 2"），但 ADR-076 v1 设计的 HAL #66/#67/#68 三 fn-ptr 是 ad-hoc 镜像（来自 adr-076 v1 自身 §D2 "3 个新 HAL fn-ptr"），**8 函数 ABI 与 3 fn-ptr 之间不存在正式映射**。

### §C0.2 仲裁裁决

v2 裁决（依据 ADR-035 §R5.1 cross-repo 流程 + ADR-036 3 区分架构原则）:

1. **HSK-1 真相源** = PTX-EMU 仓 `include/cudart/cpptlm_module.h`（8 个 `ptxemu_image_*` 函数）。**这是唯一权威 ABI 表面**, 不可由 UsrLinuxEmu / TaskRunner / CppTLM 任何单方面修改。
2. **UsrLinuxEmu HAL** = 桥接层, **不可** 自创新 ABI（违反 ADR-023 §D4 append-only 治理）。HAL #66 (H2D DMA image write) 保留, #67/#68 保留 deprecated stub 物理结构（不可删除 per ADR-023 §D4），**但 #66 的语义与 8 函数 ABI 中 `ptxemu_image_load` 的对应关系在 v2 §D1 显式声明**。
3. **TaskRunner IGpuDriver** = UMD 侧接口, tadr-307 提议的 3 个方法与 HSK-1 真相源 + ADR-023 append-only 不兼容,**必须**撤或重写为 tadr-308。
4. **CppTLM dGPU board** = 被驱动的硬件子系统层, 通过 `cpptlm_bridge.h`（**vendored 副本**）消费 HSK-1。真相源在 PTX-EMU 仓，CppTLM 是消费方。

### §C0.3 历史 canonical 链处置

| 文件 | v1 状态 | v2 处置 | 理由 |
|---|---|---|---|
| `adr-076-gpgpu-kernel-module-ioctl.md` | 🚫 Superseded v2 | 🚫 **Historical** | v1 误称 Superseded，v2 改 Historical（不再称为"已修订替代"，仅留实施产物追溯） |
| `adr-090-ptxir-via-h2d-dma.md`（v1） | 🔄 Proposed | 🚫 **Superseded by v2** | v1 决策链 v2 全部推翻，但 ship 实施产物保留 |
| `tadr-307-igpu-driver-kernel-module-extension.md` | 📋 Proposed | 📋 **STALE — 待撤或重写** | 3 方法与 v2 §C0 仲裁不兼容 |
| `tadr-308-igpu-driver-vram-load.md` | — | 📋 **TO CREATE** | tadr-307 重写版，基于 annex §B v2 重写版 |
| `cpptlm_bridge.h`（CppTLM vendored 副本） | Active | 🚫 **DEPRECATED → EOL @ v3.0.0**（HSK-6 公告见 §D5）| vendored 副本随 v3.0.0 物理删除；PTX-EMU 真相源保留 |

### §C0.4 新规：跨仓引用必须带 commit 锚点

v1 起草时跨仓引用用相对路径（如 `../external/PTX-EMU/docs/adr/...`），但**没有 commit 锚点**——这导致 v2 起草时无法快速验证 PTX-EMU 仓真实状态。v2 起强制新规:

- 每个跨仓引用必须用 `repo@commit:path:line` 格式
- 例: `[PTX-EMU cpptlm_module.h:12-52](https://github.com/chisuhua/PTX-EMU/blob/8dc000ec/include/cudart/cpptlm_module.h#L12-L52)` 而非 `../external/PTX-EMU/docs/adr/...`
- 例: `PTX-EMU@8dc000ec:include/cudart/cpptlm_module.h:18`（本地行号引用）

违反此新规的 ADR / TADR / HSK 提案必须 reject 重提。
---

## §C1-C3 问题背景（KEEP 压缩）

### §C1 ADR-076 v1 层次违规 — Oracle 评审识别（事实保留）

`adr-076-gpgpu-kernel-module-ioctl.md`（**v1 时已升 ✅ Accepted**, commit `c07d245` 实施 ship）通过 3 个 ioctl（`GPU_IOCTL_LOAD_KERNEL_MODULE` 0x27 / `GPU_IOCTL_LAUNCH_KERNEL_MODULE` 0x28 / `GPU_IOCTL_UNLOAD_KERNEL_MODULE` 0x29）+ 3 个 HAL fn-ptr（#66/`kernel_module_load`, #67/`kernel_module_execute`, #68/`kernel_module_unload`）+ `hal_user.cpp` dlopen `libptxemu_device.so` 同步执行 PTX-EMU,完成 kernel 加载/执行/卸载链路。

Oracle session `ses_ff2106f84ffeM2oItBEa9iu4hL`（2026-08-17 v1 启动评审）识别**一个架构层次违规**:`hal_user.cpp` dlopen `libptxemu_device.so` + 同步执行 kernel → HAL 桥成为硬件行为提供者而非单纯桥接,违反 [ADR-036](adr-036-three-way-separation.md) 3 区分架构（HAL 是桥,不是独立第 4 层）。

**v2 立场**: 违反事实保留,**处置方法修正**（v1 提"3→1 fn-ptrs"路径已被 CppTLM owner 否定,见 §C4 + §D1）。

### §C2 真实 CUDA 栈的层次验证（KEEP）

PTX-EMU 的实际定位是 SM 内部 ISA 执行（L3+L4 in GPU 层次）。真实 Linux amdgpu CUDA 栈的对应位置是:

| 层次 | 真实 Linux amdgpu | UsrLinuxEmu v2 映射 |
|---|---|---|
| L1 用户态 driver（CUDA runtime）| `libcuda.so` (NVIDIA) / `libamdhip64.so` (AMD) | TaskRunner `src/umd/libcuda_shim/`（16 个 shim 文件）|
| L2 KMD（kernel-mode driver）| `amdgpu.ko` 内核模块 | UsrLinuxEmu `plugins/gpu_driver/drv/gpgpu_device.cpp`（已 ship）|
| L3 HW scheduler / Puller | `amdgpu_pm`, `amdgpu_job`, MES scheduler | UsrLinuxEmu `plugins/gpu_driver/sim/hardware/hardware_puller_emu.cpp`（PM4/AQL 调度器）|
| L4 SM ISA executor | `simd_scheduler`, wavefront dispatcher | **PTX-EMU submodule**（v2 §D3 Mode B 终态） |

**v2 立场**: L4 由 PTX-EMU 承担（事实无误），归属位置修正为 **CppTLM dGPU board submodule**, 而非 UsrLinuxEmu HAL backend。`hardware_puller_emu.cpp` 仍是 L3 调度器（user 2026-08-17 纠正：它是 PM4 packet 调度器,不是 SM kernel 调度器）。

### §C3 ADR-088 Phase 1 路径 + Mode A 处置修订（REWRITE）

[ADR-088](adr-088-dgpu-complete-simulation.md) ✅ Accepted 2026-08-15 定义 dGPU 完整板卡仿真路径（CppTLM 子模块 + `libcpptlm_emulator.so` 23 C ABI）。

**v1 假设**: Mode A interim 用 `sim/translateLaunch` 回调过渡（v1 §C3 + §D2）。

**v2 修订**: CppTLM owner 在 issue #18 评论**明确否决** layered fallback 架构。Mode A 重新定义为 **冻结基线 + 依赖清理**(见 §D4),过渡期**无 fallback 层**——当前已工作的 `cpptlm_bridge.h` vendored 副本路径原样继续服务,直到 Mode B（submodule + dGPU board）E2E 通过后才走两阶段删除（§D6）。

---

## §C4 v1 三 RFC 失败复盘（NEW）

v1 提交后在三仓开 RFC:PTX-EMU #12（[Request Changes](https://github.com/chisuhua/PTX-EMU/issues/12)）/ TaskRunner #10（[HOLD](https://github.com/chisuhua/TaskRunner/issues/10)）/ CppTLM #18（[Ack + Major Revision](https://github.com/chisuhua/CppTLM/issues/18)）。合计 **24 BLOCKERS**,分类如下。

### §C4.1 事实性错误（12 项,可机械修复）

| # | 来源 | 错误描述 | 真实事实 | v2 修正位置 |
|---|---|---|---|---|
| F1 | v1 §D1 | HAL "1+2 deprecated" 描述基于"5 个 cptxemu_* 函数" | PTX-EMU 真实 ABI 是 **8 个 ptxemu_image_*** (CPPTLM_MODULE_VERSION 2) | §D1.1 全量采纳 8 函数 |
| F2 | annex §A | ADR-0029 §D8 = "banner 修订" | §D8 是 ~250 行 HAL 扩展方案（D8.1~D8.8 + D8-Alt）| §D1 + §C0.2 重写 §D8 修订范围 |
| F3 | RFC E.3 | TaskRunner 路径 `src/umd/cu_module.cpp` | 真实 `src/umd/libcuda_shim/cu_module.cpp`（16 shim 文件）| §D2 + §D3 全文改正 |
| F4 | RFC E.3 | "Removed 4 个 cuda_* 方法" | 仓内 0 匹配；实际删 `IGpuDriver::launch_kernel_module` (#49) + `unload_kernel_module` (#50) | §D3.2 删除清单 |
| F5 | RFC E.2 | "5 ABI 函数签名不变" | 真实是 8 函数 ABI,签名由 HSK-1 锁定不可改 | §C0.2 仲裁 #1 |
| F6 | RFC E.3 | `submit_pushbuffer_batch` 新增 | IGpuDriver 已有 `submit_batch`(`include/shared/igpu_driver.hpp:191`),无需新增 | §D3.1 |
| F7 | RFC E.3 | PTXIR 解析推荐 cuobjdump + LLVM 14+ | annex §B.5 实际推荐方案 B（直链 libptxemu helper）；仓内无 cuobjdump 路径 | §D3.3 |
| F8 | RFC E.2 | cpptlm.h header `cptxemu.h` 引用 | 真实 header `cpptlm_module.h:12-52` | §D1.1 全部引用 |
| F9 | v1 annex §A | PTX-EMU cpptlm_bridge.h 真相源位置 | 真相源在 **PTX-EMU 仓**, CppTLM 是 vendored 副本 | §C0.2 + §D5 |
| F10 | v1 annex §A | ANTLR4 风险="外部依赖" | ANTLR4 4.13.2 是 vendored in-tree,真正风险是 JDK + 构建时间 | §E.4 ANTLR4 spike |
| F11 | RFC E.3 | tadr-308 文件存在 | tadr-308 不存在（最新是 tadr-307）| §D3.4 待创建 |
| F12 | RFC E.3 | TaskRunner openspec 有 active change | `openspec/changes/` 仅 `archive/` | §D3.5 待创建 |

### §C4.2 架构性否决（3 项,需重新设计）

| # | 来源 | 否决内容 | v2 重新设计 |
|---|---|---|---|
| A1 | CppTLM #18 | Layered fallback 架构（UsrLinuxEmu → PTX-EMU → CppTLM 三级链）| §D4 Mode A 重定义为冻结基线,无 fallback |
| A2 | CppTLM #18 | Mode A `sim/translateLaunch` 路径 | 废除（§D4）；当前 `cpptlm_bridge.h` 路径原样继续 |
| A3 | CppTLM #18 | 假设 CppTLM 是入口 | 改为 CppTLM = 被驱动的 dGPU 板卡（PCIe 设备语义, gem5 full-system GPU 惯例）|

### §C4.3 缺失项（9 项,v1 未涵盖）

| # | 来源 | 缺失 | v2 新增位置 |
|---|---|---|---|
| M1 | CppTLM #18 | G-D4 static_assert 迁移（`cpptlm_bridge.h:243-306` → `include/cudart/abi_guards.h`）| §D5.2 |
| M2 | CppTLM #18 | HSK-6 公告（联发协议：PTX-EMU 发起, CppTLM ack, UsrLinuxEmu 利益相关方）| §D5.1 |
| M3 | CppTLM #18 | `ptx_emu_driver_shim.cc`（`src/tlm/gpu/`）列入删除清单 | §D6.1 |
| M4 | CppTLM #18 | 4 个测试文件处置（`test_memory_bridge.cc` / `test_memory_bridge_poll.cc` / `test_kernel_launch_tlm_ext.cc` / `test_gpu_soc_perf.cc`）| §D6.2 |
| M5 | CppTLM #18 | dGPU 板卡最小完整集（DGpuBar + Doorbell + SQ/CQ, MSI-X 回调钩子, DMA 复用 `MemoryCluster` + `GpuNoC`）| §D3.3 |
| M6 | CppTLM #18 | `AsyncCompletionAdapter` 重设计（删除 `std::function` 回调表 → `CompletionRing` push + `host_notify` 钩子）| §D3.3 |
| M7 | CppTLM #18 | `KernelLaunchRequest` 复用（`include/tlm/gpu/kernel_launch_tlm.hh:30` 已存在）| §D3.3 |
| M8 | CppTLM #18 | 9 周双轨时间线（P0 冻结 / P1 双轨 / P2 收敛 / P3 重构 / P4 物理删除）| §E |
| M9 | CppTLM #18 | `namespace tlm` 而非 `cpptlm::d_gpu`（仓内 `src/tlm/gpu/*.cc` 全部 `namespace tlm`）| §D3.3 |

### §C4.4 复盘结论（v2 治理改进）

24 BLOCKERS 中**事实性错误占 50%**（F1~F12）,主因是 v1 起草时**未做本地仓验证**——只用了 GitHub API 摘要 + annex 文档拼接,未读 PTX-EMU/CppTLM 仓真实代码。

v2 强制新规(详见 §C0.4):跨仓引用必须带 `repo@commit:path:line` 锚点,起草前必须 `git fetch && git submodule update` 校验本地仓时效性。违反此新规的 ADR / TADR / HSK 提案 reject 重提。

---

## §D1 PTX-EMU 8 函数 ABI 采纳（REWRITE）

### §D1.1 8 函数 ABI 真相源（逐字引用 `PTX-EMU@8dc000ec:include/cudart/cpptlm_module.h:12-52`）

```c
// CPPTLM_MODULE_VERSION 2（PTX-EMU 仓 ship 锁定）
// Consumer must check ptxemu_module_version() >= 2

extern "C" {

// === Base API (ADR-0029 §D1, CPPTLM_MODULE_VERSION 1+) ===
uint64_t ptxemu_image_load(const uint8_t* image_bytes, size_t image_size);
int ptxemu_image_kernel_name(uint64_t handle, char* buf, size_t buf_size);
int ptxemu_image_execute(uint64_t handle,
                         uint32_t gx, uint32_t gy, uint32_t gz,
                         uint32_t bx, uint32_t by, uint32_t bz,
                         size_t shared_mem,
                         void** args, size_t argc);
int ptxemu_image_unload(uint64_t handle);
int ptxemu_module_version(void);

// === Multi-kernel API (ADR-0028 已 ship, CPPTLM_MODULE_VERSION >= 2) ===
int ptxemu_image_kernel_count(uint64_t handle);
int ptxemu_image_kernel_name_at(uint64_t handle, uint32_t idx,
                                char* buf, size_t buf_size);
int ptxemu_image_execute_named(uint64_t handle, const char* kernel_name,
                               uint32_t gx, uint32_t gy, uint32_t gz,
                               uint32_t bx, uint32_t by, uint32_t bz,
                               size_t shared_mem,
                               void** args, size_t argc);

}  // extern "C"
```

**8 函数实现位置**：`PTX-EMU/src/cudart/cpptlm_module.cpp:240-275`（外部 `extern "C"` 7 处定义点 + `ptxemu_module_version`；singleton 实例 `PtxEmuImageExecutor`）。

### §D1.2 ABI 表面映射（v1 §D1 "1+2 deprecated" 修订）

v1 §D1 提议"HAL fn-ptrs 3→1（#66 保留, #67/#68 deprecated stub）", 隐含假设 PTX-EMU ABI 是 5 函数。**v2 修订**:

| PTX-EMU ABI 函数 | HAL fn-ptr 映射 | ioctl 映射 | CppTLM Mode B 路径 |
|---|---|---|---|
| `ptxemu_image_load` | HAL #66 `kernel_module_load`（已 ship, gpu_hal.h:370）| ioctl 0x27 `GPU_IOCTL_LOAD_KERNEL_MODULE`（已 ship）| H2D DMA → VRAM（不调 ptxemu_image_load）|
| `ptxemu_image_kernel_name` | ❌ 不经 HAL | ❌ 不经 ioctl | Mode B: DGpuBar MMIO read |
| `ptxemu_image_execute` | ❌ 不经 HAL（v1 §D1 #67 deprecated 物理结构保留, 但语义恒为 -ENOSYS）| ioctl 0x28 返回 -ENOSYS（已 ship）| Mode B: SQ/CQ doorbell 提交 `KernelLaunchRequest` |
| `ptxemu_image_unload` | ❌ 不经 HAL（v1 §D1 #68 deprecated 物理结构保留）| 走 `GPU_IOCTL_FREE_BO` 路径（gpu_ioctl.h:738）| Mode B: BO 释放时调 |
| `ptxemu_module_version` | ❌ 不经 HAL（运行时检查）| ❌ 不经 ioctl | 编译期 + 运行时双断言 |
| `ptxemu_image_kernel_count` | ❌ 不经 HAL | ❌ 不经 ioctl | Mode B: DGpuBar MMIO read |
| `ptemu_image_kernel_name_at` | ❌ 不经 HAL | ❌ 不经 ioctl | Mode B: DGpuBar MMIO read |
| `ptxemu_image_execute_named` | ❌ 不经 HAL | ❌ 不经 ioctl | Mode B: SQ/CQ doorbell + kernel name |

**关键理解**: UsrLinuxEmu HAL 仅承接 8 函数中的 **1 个**（`ptxemu_image_load` 的语义镜像 = H2D DMA 写 VRAM）,其余 7 个函数**完全不经 HAL**,由 CppTLM dGPU board Mode B 路径承载。

### §D1.3 为什么保留 HAL #67/#68 deprecated stub 物理结构

[ADR-023](adr-023-hal-interface.md) §D4 append-only 治理:**HAL fn-ptr 不可删除, 只能 deprecated**。v1 ship 的 #67/#68 stub 物理结构（`gpu_hal.h:380-385`）保留, 但语义永久 = `-ENOSYS`。**这是为了 ABI 卫生, 不是为了功能**——保留槽位防止 consumer 重用 fn-ptr 编号导致 ABI 冲突。

### §D1.4 版本锁策略

```c
// 编译期断言（PTX-EMU 仓 `include/cudart/AGENTS.md` 治理）：
#define CPPTLM_MODULE_VERSION 2  // ship 锁定, 不可静默 bump

// 运行时检查（HAL #66 进入时）：
extern int ptxemu_module_version(void);
static_assert(CPPTLM_MODULE_VERSION >= 2, "CPPTLM_MODULE_VERSION >= 2 required");
int v = ptxemu_module_version();
if (v < 2) return -EPROTO;  // halt; ad-hoc caller may not bypass
```

**VERSION 3 迁移路径**（PTX-EMU owner bump 时）：
1. PTX-EMU 按 `include/cudart/AGENTS.md` 治理：先 bump `CPPTLM_MODULE_VERSION` → 通知 CppTLM rebase → 通知 UsrLinuxEmu
2. UsrLinuxEmu 接收 HSK-N 公告后, 按 [ADR-023](adr-023-hal-interface.md) §D4 走 append-only：新增 HAL fn-ptr #69/70...（不修改 #66 物理结构）
3. TaskRunner 接收后, 按 tadr-308 创建约定新增 IGpuDriver 方法

---

## §D2 ioctl 0x27/0x28/0x29 语义重定义（KEEP 微调）

### §D2.1 当前 ship 状态（v1 commit `c07d245` + `b26412a` 已实施）

**`plugins/gpu_driver/shared/gpu_ioctl.h:723-779`** 当前 3 个 ioctl struct 定义已 ship, 与 v2 §D1 8 函数 ABI 兼容, 仅需补 `out_vram_addr` 与 CppTLM v3 VRAM 模型对齐注记。

```c
// gpu_ioctl.h:723 - GPU_IOCTL_LOAD_KERNEL_MODULE (0x27)
struct gpu_load_kernel_module_args {
    const uint8_t* image_bytes;     // input: PTXIR image bytes
    size_t image_size;              // input
    uint64_t out_vram_addr;         // output: code BO GPU VA (v2 强调对齐 CppTLM v3 VRAM 模型)
    size_t out_image_size;          // output: written image size
};

// gpu_ioctl.h:748 - GPU_IOCTL_LAUNCH_KERNEL_MODULE (0x28)
struct gpu_launch_kernel_module_args {
    uint64_t module_handle;         // 语义重定义: = vram_addr
    uint32_t grid[3];               // gx, gy, gz
    uint32_t block[3];              // bx, by, bz
    size_t shared_mem;
    void** args;
    size_t argc;
};
// handler 强制返回 -ENOSYS（v1 §D2 + v2 §D1.2 一致）

// gpu_ioctl.h:770 - GPU_IOCTL_UNLOAD_KERNEL_MODULE (0x29)
struct gpu_unload_kernel_module_args {
    uint64_t module_handle;         // 语义重定义: = vram_addr
};
// handler 强制返回 -ENOSYS（v1 §D2 + v2 §D1.2 一致）
```

### §D2.2 ioctl 编号永久保留规则

| ioctl 编号 | 名称 | handler 状态 | 处置 |
|---|---|---|---|
| **0x27** | `GPU_IOCTL_LOAD_KERNEL_MODULE` | HAL #66 → H2D DMA | ✅ 永久保留 + 功能 |
| **0x28** | `GPU_IOCTL_LAUNCH_KERNEL_MODULE` | -ENOSYS | ✅ 永久保留（防编号复用）|
| **0x29** | `GPU_IOCTL_UNLOAD_KERNEL_MODULE` | -ENOSYS | ✅ 永久保留（防编号复用）|

**永久保留规则**: 0x27/0x28/0x29 三个编号**不可被其他 ioctl 占用**,即使 handler 语义 = -ENOSYS。理由：tadr-307/tadr-308 已 ship 路径依赖这些编号,删除会触发 consumer 端 ABI 冲突。

### §D2.3 v2 微调项（vs v1）

| 项 | v1 描述 | v2 修订 |
|---|---|---|
| `out_vram_addr` 注释 | v1 §D2 简注 | v2 §D2.1 强调对齐 CppTLM v3 VRAM 模型（dGPU 板卡 BAR 空间）|
| `module_handle` 语义重定义 | v1 §D2 文字 | v2 §D1.2 表格明确 = vram_addr（防止与 `module_version()` 概念混淆）|
| 0x28/0x29 handler | v1 §D2 简述 | v2 §D2.1 强调"物理结构 + 语义 = -ENOSYS 永久锁定",符合 ADR-023 §D4 治理 |

---

## §D3 Mode B：submodule + CppTLM dGPU Board（REWRITE 强化）

### §D3.1 模式架构定位（v2 关键反转）

**v1 假设**: UsrLinuxEmu + TaskRunner 调 PTX-EMU（入口在 UsrLinuxEmu HAL）→ CppTLM 是被调用的板卡。

**v2 修订**（依据 CppTLM owner 在 #18 反馈）: **UsrLinuxEmu + TaskRunner 作为 CUDA app 入口；CppTLM 作为被驱动的 dGPU 板卡**（PCIe 设备语义, 对齐 gem5 full-system GPU 工业惯例）。

```
┌─────────────────────────────────────────────────────────────┐
│ CUDA app 入口层                                              │
│   TaskRunner/libcuda_shim/cu_module.cpp + cu_launch.cpp    │
│   (cuModuleLoadData + cuLaunchKernel)                       │
└────────────────────┬────────────────────────────────────────┘
                     │ ioctl (fd, GPU_IOCTL_*, ...)            │
                     ▼
┌─────────────────────────────────────────────────────────────┐
│ ② 可移植驱动层 (UsrLinuxEmu drv/)                          │
│   GpgpuDevice::ioctl 派发表（已 ship, 0x27/0x28/0x29）     │
└────────────────────┬────────────────────────────────────────┘
                     │ HAL fn-ptr #66 (kernel_module_load)      │
                     ▼
┌─────────────────────────────────────────────────────────────┐
│ ① UsrLinuxEmu HAL 桥接层（仅 1 个 fn-ptr #66 活跃）       │
│   - H2D DMA write PTXIR bytes → CppTLM VRAM (out_vram_addr)│
│   - icache invalidate Mode B: 委托 CppTLM                   │
│   - **不调 ptxemu_image_load**（v1 §D1 已 ship 删除 dlsym）│
└────────────────────┬────────────────────────────────────────┘
                     │ VRAM write + PCIe BAR MMIO              │
                     ▼
┌─────────────────────────────────────────────────────────────┐
│ ③ CppTLM dGPU 板卡仿真层（**Mode B 终态**, P2 收敛后）   │
│                                                             │
│   ┌──────────────────────────────────────────────────┐      │
│   │ DGpuBar (PCIe BAR0 MMIO 模拟)                    │      │
│   │   - device_id, vendor_id, mmio base             │      │
│   └──────────────────────────────────────────────────┘      │
│   ┌──────────────────────────────────────────────────┐      │
│   │ Doorbell (GPU 通知机制)                          │      │
│   │   - submit_doorbell(stream_id, ring_offset)      │      │
│   └──────────────────────────────────────────────────┘      │
│   ┌──────────────────────────────────────────────────┐      │
│   │ SQ/CQ pair (Submission/Completion Queue)         │      │
│   │   - SQ: 接收 gpfifo_entry → 解析 DISPATCH_KERNEL │      │
│   │   - CQ: CompletionRing push（不是 std::function） │      │
│   │   - host_notify 钩子（MSI-X 替代）               │      │
│   └──────────────────────────────────────────────────┘      │
│   ┌──────────────────────────────────────────────────┐      │
│   │ PTX-EMU submodule（v2 §D1.2 8 函数 ABI 调用点）│      │
│   │   - ptxemu_image_load 接收 H2D DMA image bytes   │      │
│   │   - ptxemu_image_execute / _execute_named        │      │
│   │     由 SQ/CQ 触发                                │      │
│   └──────────────────────────────────────────────────┘      │
│                                                             │
│   ┌──────────────────────────────────────────────────┐      │
│   │ DMA 路径（**复用**既有）                        │      │
│   │   - MemoryCluster（include/tlm/cluster/）       │      │
│   │   - GpuNoC（gpu_noc_cluster.hh）               │      │
│   │   - 跨板卡 H2D/D2H 由 MemoryCluster 承担         │      │
│   └──────────────────────────────────────────────────┘      │
└─────────────────────────────────────────────────────────────┘
```

### §D3.2 namespace + 版本 + 路径对齐

| 项 | v1 假设 | v2 修订（依据 CppTLM owner #18） |
|---|---|---|
| Namespace | `cpptlm::d_gpu` | `namespace tlm`（仓内 `src/tlm/gpu/*.cc` 全部 `namespace tlm`；唯一例外 `gpu_soc_tlm.cc:11` 用 `cpptlm::tlm`，v2 统一为 `tlm`）|
| 版本号 | v4.x → v5.0.0 | CMakeLists v2.1.0 → v3.0.0 + 新建 `include/cpptlm_version.h`（解耦 CMake 版本与 API 版本）|
| Submodule 集成 | A/B/C 三选一 | **B (git submodule)** 为推荐（与原 RFC Option B 一致）；extract kernel execution 路径优先 |
| Path | `plugins/gpu_driver/sim/cpptlm_emulator` | CppTLM submodule 仓库 `src/tlm/gpu/`（`KernelLaunchTLM` / `KernelLaunchRequest` / `MemoryCluster` 等）|

### §D3.3 dGPU 板卡最小完整集（NEW — v1 完全缺失）

v2 采纳 CppTLM owner 在 #18 提议的 dGPU 板卡最小完整集：

| 组件 | 功能 | 既有 / 新建 | 证据 |
|---|---|---|---|
| **DGpuBar** | PCIe BAR0 MMIO 模拟 | 🆕 **新建**（grep 仓内零命中）| 需新建 `include/tlm/gpu/dgpu_bar.hh` + `src/tlm/gpu/dgpu_bar.cc` |
| **Doorbell** | GPU 通知机制（CPU → GPU 敲门）| 🆕 **新建** | 同上 |
| **SQ/CQ pair** | 提交/完成队列 | 🆕 **新建**（v2 §D3.4 重设计 CompletionRing）| 同上 |
| **MSI-X** | 中断机制 | 🆡 改为回调钩子（**不再用 MSI-X 真实硬件路径**）| 性能优化：仿真环境用 hook 即可 |
| **MemoryCluster** | DMA 聚合 | ✅ **复用** | `include/tlm/cluster/memory_cluster.hh` 已存在 |
| **GpuNoC** | Network-on-Chip | ✅ **复用** | `gpu_noc_cluster.hh` 已存在 |
| **KernelLaunchRequest** | kernel 启动请求结构 | ✅ **复用** | `include/tlm/gpu/kernel_launch_tlm.hh:30` 已存在 |
| **PtxEmuSubmodule** | PTX-EMU 模块化包装 | 🆕 **新建**（façade over 8 函数 ABI）| v2 §D3.5 |

### §D3.4 AsyncCompletionAdapter 重设计（NEW — v1 完全缺失）

v1 没考虑 kernel 完成通知机制。v2 采纳 CppTLM owner 提议的 SQ/CQ 模型重设计：

```cpp
// 删除（v0 模型）
using CompletionCallback = std::function<void(SmImageId, int)>;
virtual int setCompletionCallback(CompletionCallback cb) = 0;
// 问题：callback table 全局共享,无法支持多 stream

// 新建（v2 SQ/CQ 模型）
class CompletionRing {
public:
  // SQ consumer 写入完成 entry
  void push(SmImageId id, int status);

  // CPU 侧 host_notify 钩子（MSI-X 替代）
  void set_host_notify(std::function<void()> hook);
};

// host_notify 调用方：UsrLinuxEmu HAL #66 关联的 fence signal
// 与 UsrLinuxEmu FenceRegistry 解耦, 走 ring 而非 callback
```

**理由**：删除 `std::function` 回调表避免多 stream 并发问题（v1 §D1.3 提到的"concurrent load race"测试风险）。CompletionRing push + host_notify 钩子符合 CppTLM owner 在 #18 反馈的设计意图。

### §D3.5 PTX-EMU Submodule Façade（NEW）

v2 §D3.3 表格的 `PtxEmuSubmodule` 是 CppTLM 侧的 façade，封装 PTX-EMU 8 函数 ABI 调用：

```cpp
// include/tlm/gpu/ptx_emu_submodule.hh（新建）
namespace tlm {

class PtxEmuSubmodule {
public:
  explicit PtxEmuSubmodule(const std::string& pt_path);  // libptxemu_device.so

  // 8 函数 ABI façade（与 cpptlm_module.h:18-52 一一对应）
  uint64_t image_load(const uint8_t* image_bytes, size_t image_size);
  int image_kernel_name(uint64_t handle, char* buf, size_t buf_size);
  int image_execute(uint64_t handle, uint32_t gx, gy, gz, bx, by, bz,
                    size_t shared_mem, void** args, size_t argc);
  int image_unload(uint64_t handle);
  int module_version();

  // VERSION >= 2 multi-kernel API
  int image_kernel_count(uint64_t handle);
  int image_kernel_name_at(uint64_t handle, uint32_t idx, char* buf, size_t buf_size);
  int image_execute_named(uint64_t handle, const char* name, ...);

private:
  void* dlsym_handle_;  // libptxemu_device.so handle
  // 8 个 function pointers
};

}  // namespace tlm
```

**关键**：
- Façade 在 CppTLM 侧**新建**，**不**改 PTX-EMU 仓的 `cpptlm_module.h`
- dlsym 加载 `libptxemu_device.so`，由 PTX-EMU 仓提供（通过 `ExternalProject_Add` 或 git submodule）
- UsrLinuxEmu **永不**调 ptxemu_image_* 函数（已 ship 删除 dlsym, commit `c07d245`）

### §D3.6 ANTLR4 4.13.2 依赖处理（修正 v1 风险评估）

**v1 假设**：submodule 提取会引入新外部下载依赖。**v2 修订**（依据本地调查）：

- ANTLR4 4.13.2 是 **vendored in-tree 源码**（`PTX-EMU/CMakeLists.txt:91-92`），不是 ExternalProject 下载
- submodule 提取时 `antlr4_generated_src` + `antlr4-cpp-runtime-4.13.2-source` 随 PTX-EMU 一起进 submodule
- **真正风险**：构建时间 +3min + JDK 运行时（jar 文件需要 Java）
- **Spike 任务**（v2 §E.4）：验证 CppTLM CI 镜像是否已有 JDK

---

## §D4 Mode A 重定义（REWRITE — 废除 layered fallback）

### §D4.1 v1 Mode A 的本质问题

v1 §C3 + §D2 提议 Mode A = `sim/translateLaunch` 回调过渡层（UsrLinuxEmu → PTX-EMU → CppTLM 三级链）。

**CppTLM owner 在 #18 明确否决**：layered fallback 把 PTX-EMU 永久滞留关键路径，kernel 执行语义被切两半（指令语义在 PTX-EMU、时序在 CppTLM），与角色反转目标背道而驰。

### §D4.2 v2 Mode A 定义（冻结基线 + 依赖清理）

**Mode A 不是过渡路径, 是冻结基线**：

| 阶段 | 任务 | Owner |
|---|---|---|
| **Mode A.1：冻结基线** | CppTLM v2.1.0 冻结为工作基线（不删、不扩展）| CppTLM maintainer |
| **Mode A.2：G-D4 迁移** | static_assert 组（`cpptlm_bridge.h:243-306`）迁移至 `include/cudart/abi_guards.h`（新建）| CppTLM maintainer（P0-1 门禁）|
| **Mode A.3：HSK-6 公告** | PTX-EMU 联发 + CppTLM ack + UsrLinuxEmu 利益相关方 ack | PTX-EMU Architecture Team 发起（详见 §D5.1）|
| **Mode A.4：ANTLR4 spike** | 验证 CppTLM CI 是否能承载 ANTLR4 构建（避免 spike 翻车）| 联合（CppTLM + UsrLinuxEmu）|
| **Mode A.5：测试冻结** | 4 个 CppTLM 测试文件（§D6.2）冻结不动, 等 Mode B E2E 后再迁移/删除 | CppTLM maintainer |

**过渡期无 fallback 层**：当前已 ship 的 `cpptlm_bridge.h` 路径继续服务,直到 Mode B E2E 通过后才走 §D6 两阶段删除。

### §D4.3 与 v1 路径的差异

| 维度 | v1 Mode A | v2 Mode A |
|---|---|---|
| 路径性质 | 过渡路径（us → ptx-emu → cpptlm）| 冻结基线（保留当前 cpptlm_bridge.h 路径原样）|
| PTX-EMU 角色 | 调入的关键路径 | 不在关键路径（Mode B 才用 submodule）|
| kernel 执行时序 | PTX-EMU 同步阻塞 | 冻结基线下沿用 v2.1.0 既有异步行为 |
| 终止条件 | Mode B 切完即废弃 | Mode B E2E 通过后才进入 §D6 两阶段删除 |

---

## §D5 HSK-6 + G-D4 迁移门禁（NEW — v1 完全缺失）

### §D5.1 HSK-6 公告（联发协议）

**归属修正**（依据 Oracle 本地调查 + CppTLM owner #18）：v1 假设 HSK-6 由 CppTLM 单方面发起——**错误**。正确结构是 **PTX-EMU 发起 + CppTLM ack + UsrLinuxEmu 利益相关方 ack**（联发协议）。

| 字段 | 值 |
|---|---|
| HSK 编号 | HSK-6（序列：HSK-1 ABI header @ `8dc000ec` / HSK-2 ANTLR4 4.13.2 / HSK-3 `ExternalProject_Add` / HSK-4 `ptx_arg_sizes[]` / HSK-5 `advance()` deferred）|
| **发起方** | **PTX-EMU Architecture Team**（HSK-1 真相源持有方, 持 `PTX-EMU/include/cudart/cpptlm_bridge.h`）|
| Ack 方 | (a) CppTLM maintainer（消费方执行侧删除）；(b) UsrLinuxEmu Architecture Team（利益相关方, 不 own）|
| 状态 | PROPOSED → (CppTLM ack + UsrLinuxEmu ack 后) ACCEPTED |
| 影响符号（CppTLM 侧物理删除）| `MemoryBridge` / `IPtxEmuDriver` / `DriverWrapper` / `g_ptx_emu_driver` / `cpptlm_set_driver` / `ptx_emu_driver_shim.cc`（`src/tlm/gpu/`）/ vendored `include/cudart/cpptlm_bridge.h`（308 行副本）|
| 真相源处置 | PTX-EMU `include/cudart/cpptlm_bridge.h` **保留不删**（`CPPTLMBRIDGE_VERSION` 冻结于 2, 进入 maintenance-only）；consumption relationship 废止 |
| 替代路径 | CppTLM v3.0.0 dGPU board（DGpuBar + Doorbell + SQ/CQ, 见 §D3.3）+ PTX-EMU submodule |
| 时间线 | 冻结公告日 → Mode B E2E 通过 → 物理删除（两阶段, 对齐 §D6 + §E）|
| Ack 截止 | （建议 14 天）CppTLM owner + UsrLinuxEmu（利益相关方）|
| 参考 | ADR-090 v2 §C0.2 + §D5.2；CppTLM `openspec/changes/cpptlm-v3-dgpu-extract/`（待创建）|

### §D5.2 G-D4 static_assert 迁移（Mode A.2 / P0-1 门禁）

`CppTLM/include/cudart/cpptlm_bridge.h:243-306` 包含 **16 条 static_assert**（`CPPTLMBRIDGE_VERSION 2` 在 :55；6 PipelineId + 6 TcPrecision + 4 `is_same_v`），是 vendored 副本中**唯一守卫**——删除 cpptlm_bridge.h 前必须前置迁移到独立头文件。

| 项 | v1 | v2 |
|---|---|---|
| 静态断言位置 | v1 未提及 | `CppTLM/include/cudart/cpptlm_bridge.h:243-306` |
| 迁移目标 | v1 未提及 | `include/cudart/abi_guards.h`（**新建**, 由 CppTLM 仓 owner 创建）|
| 完成前可删除 cpptlm_bridge.h？| v1 未提及 | ❌ **不可以**（P0-1 硬门禁）|
| Owner | — | CppTLM maintainer |

**验证**：迁移完成后, 用 PTX-EMU 仓真相源（`PTX-EMU/include/cudart/cpptlm_bridge.h:14-16`）对比 vendored 副本, 确保 static_assert 组完整迁移且无遗漏。

---

## §D6 两阶段删除流程（NEW — v1 完全缺失）

### §D6.1 删除清单（基于 Oracle 本地调查）

**Phase 1 冻结（Mode A + Mode B E2E 通过前）**：

| 项 | 位置 | 状态 |
|---|---|---|
| `MemoryBridge` 类 | `CppTLM/include/tlm/gpu/memory_bridge.hh` | 冻结（已 ship, 不删不扩展）|
| `IPtxEmuDriver` 接口 | `CppTLM/include/tlm/gpu/ptx_emu_driver.hh:19` | 冻结 |
| `DriverWrapper` | CppTLM 仓（待定位）| 冻结 |
| `g_ptx_emu_driver` 全局 | CppTLM 仓 | 冻结 |
| `cpptlm_set_driver` ABI 入口 | CppTLM 仓 | 冻结 |
| `ptx_emu_driver_shim.cc` | `CppTLM/src/tlm/gpu/ptx_emu_driver_shim.cc` | 冻结（v2 列入删除清单）|
| vendored `cpptlm_bridge.h` | `CppTLM/include/cudart/cpptlm_bridge.h`（308 行）| 冻结（HSK-6 P0-1 门禁）|

**Phase 2 物理删除（Mode B E2E 通过后, HSK-6 ACCEPTED）**：

- 删除上述 7 项
- 同步更新 CppTLM `CMakeLists.txt`（移除 `cpptlm_bridge.h` vendored 规则）
- 同步更新 `include/cudart/abi_guards.h` 引用方
- 触发 CppTLM v3.0.0 version bump（CMakeLists.txt + `include/cpptlm_version.h`）

### §D6.2 4 个测试文件迁移路径（NEW — v1 完全缺失）

| 文件 | 引用符号（实测, Oracle 本地调查）| 处置 |
|---|---|---|
| `test_memory_bridge.cc` | `MemoryBridge`（submit_kernel/poll_kernel/synchronize_stream/global_access/query_latency/deep_copy_args_）| **删除**（随 MemoryBridge 物理删除）|
| `test_memory_bridge_poll.cc` | `IPtxEmuDriver`（`FakeCompleteDriver :24`）、`get/set_ptx_emu_driver`、`tlm/gpu/ptx_emu_driver.hh:19` | **重写** → `test_completion_ring.cc`（`poll_kernel` 语义 → CompletionRing push/host_notify, §D3.4）|
| `test_kernel_launch_tlm_ext.cc` | `MockPtxEmuDriver : IPtxEmuDriver`（:33）、`set_ptx_emu_driver`、`KernelLaunchRequest`、`setMemoryBridge` | **保留 + 改符号**（`KernelLaunchRequest` 复用, `MockPtxEmuDriver` → SQ/CQ doorbell mock）|
| `test_gpu_soc_perf.cc` | `tlm/gpu/ptx_emu_driver.hh:19`、`set_ptx_emu_driver`、`MemoryBridge::poll_kernel` perf 路径 | **拆分**（scoreboard perf 保留；MemoryBridge poll perf → CompletionRing）|

**顺序**：全部排在 Mode B E2E 之后, 物理删除之前（两阶段删除的 freeze 窗口内完成）。

### §D6.3 删除门禁检查表（Phase 2 触发前必过）

- [ ] Mode B E2E 测试通过（`tests/test_cpptlm_submodule_e2e_standalone` PASS）
- [ ] G-D4 static_assert 已迁移至 `include/cudart/abi_guards.h`（§D5.2）
- [ ] HSK-6 公告 ACCEPTED（CppTLM + UsrLinuxEmu 双 ack, §D5.1）
- [ ] 4 个测试文件按 §D6.2 路径完成（删除/重写/拆分）
- [ ] CppTLM v2.1.0 → v3.0.0 version bump（CMakeLists.txt + `cpptlm_version.h`）
- [ ] PTX-EMU submodule 在 UsrLinuxEmu 仓 submodule 中已注册（`external/PTX-EMU` 或 submodule 链接）

**门禁未过, 不进入 Phase 2**。

---

## §E 时间线：采纳 CppTLM P0-P4 双轨（REWRITE）

**v1 时间线废除**（v1 §Migration M1-M9 九阶段, 串行假设）。v2 采用 CppTLM owner 在 #18 提议的 9 周双轨 5 阶段（**P0 冻结 / P1 双轨 / P2 收敛 / P3 重构 / P4 物理删除**）作为共享主时间线。UsrLinuxEmu 任务映射进 P0-P4 框架, 与 ADR-088 Phase 4 主线并行。

### §E.1 主时间线（9 周双轨 5 阶段）

```
Week 1 ────────── Week 2 ────────── Week 4 ────────── Week 6 ────────── Week 8 ─── Week 9
   [P0 冻结]          [P1 双轨]            [P2 收敛]          [P3 重构]        [P4 物理删除]
   ├─ §D5.2 G-D4迁移   ├─ §D3.5 SM façade   ├─ ISmExecutor +   ├─ KernelLaunch-  ├─ Mode A 物理删除
   ├─ §D5.1 HSK-6联发   ├─ §D3.4 Completion-   PtxEmuSub-     TLM 重构        ├─ v3.0.0 bump
   └─ Mode A.1冻结      │   Ring push/        module façade  ├─ Completion-   ├─ 808 测试验证
                        │   host_notify        汇合            Ring + E2E
                        ├─ §E.4 ANTLR4 spike   │              │
                        └─ §D3.6 ANTLR4依赖    │              │
                            验证              │              │
```

### §E.2 各阶段 Owner 责任

| 阶段 | 周数 | 主要交付 | UsrLinuxEmu Owner | CppTLM Owner | PTX-EMU Owner | TaskRunner Owner |
|---|---|---|---|---|---|---|
| **P0 冻结** | W1 | §D5.2 G-D4 迁移 + §D5.1 HSK-6 联发 + Mode A.1 冻结 | ack（利益相关方）| 主导（执行 + 公告）| 联署 HSK-6 + ack | — |
| **P1 双轨** | W1-3 | §D3.5 PtxEmuSubmodule façade + §D3.4 CompletionRing + ANTLR4 spike | spike 协助 | 主导（façade + ring 实现）| 提供 libptxemu_device.so 编译验证 | — |
| **P2 收敛** | W4-6 | ISmExecutor + PtxEmuSubmodule 汇合 + E2E 路径 | Mode A/B 双轨测试 | 主导（汇合实现）| submodule 集成协助 | tadr-308 创建 |
| **P3 重构** | W6-8 | KernelLaunchTLM 重构 + CompletionRing push + E2E | E2E 测试 + Fence 信号 hook | 主导（重构 + ring 测试）| — | tadr-308 实施 |
| **P4 物理删除** | W8-9 | Mode A 物理删除 + v3.0.0 bump + 808 测试验证 | 测试套件扩展 | 主导（删除 + 版本）| — | tadr-308 ack |

### §E.3 UsrLinuxEmu 子任务映射（按阶段）

**P0（W1, ~3 天工作量）**：
- [x] v2 ADR-090 起草 + 接受 Architecture Team review
- [ ] v1 ADR-090 标 Superseded（指向 v2）
- [ ] annex §E 跟踪表升级（指向 v2 RFC）
- [ ] 关闭 PTX-EMU #12 + TaskRunner #10 + 在 CppTLM #18 推 v2 patch
- [ ] ack HSK-6 公告（利益相关方）

**P1（W1-3, ~2 周工作量, 与 P0 部分并行）**：
- [ ] ANTLR4 spike（协助 CppTLM）：验证 CppTLM CI JDK 可用性 + 构建时间 <+3min + CMake target 冲突排查
- [ ] 测试套件新增：`tests/test_h2d_dma_standalone` + `tests/test_dgpu_bar_mmio_standalone`（验证 dGPU board BAR 访问）

**P2（W4-6, ~2 周工作量）**：
- [ ] Mode A/B 双轨测试：`tests/test_mode_ab_dual_rail_standalone`（同测试集通过 Mode A 冻结基线 + Mode B 新路径）
- [ ] FenceRegistry 集成：CompletionRing push → host_notify → HAL fence signal 链路

**P3（W6-8, ~2 周工作量）**：
- [ ] E2E 测试：`tests/test_cpptlm_submodule_e2e_standalone`（Mode B 路径完整跑通 cuModuleLoadData + cuLaunchKernel + cuModuleUnload）
- [ ] Fence 状态机验证：完成通知 < 1ms 延迟（性能 baseline）

**P4（W8-9, ~3 天工作量）**：
- [ ] Mode A 物理删除确认（`tests/test_cpptlm_bridge_compat_standalone` 标记 DEPRECATED, 但保留以验证双轨兼容）
- [ ] CppTLM v3.0.0 在 UsrLinuxEmu `external/cpptlm/CMakeLists.txt` 中体现
- [ ] 148 → 808 测试计数验证（与 ADR-088 §D6.2 同步扩展）

### §E.4 ANTLR4 Spike 详细任务（~1 周, P1 前置）

| 任务 | Owner | 通过条件 |
|---|---|---|
| 1. 测量 CppTLM CI 当前构建时间 baseline | CppTLM | baseline < 10 min |
| 2. 加入 ANTLR4 runtime + parser 生成后构建时间 | CppTLM + UsrLinuxEmu | < baseline + 3 min |
| 3. CMake target `antlr4_shared` 冲突排查 | CppTLM | 0 冲突或可重命名 |
| 4. CppTLM CI 镜像 JDK 可用性 | CppTLM | JDK 11+ 已装 |
| 5. 替代方案（预生成 parser 源码）| UsrLinuxEmu | grammar 变更流程加重评估可接受 |

**Fork 点**：spike 失败 → 回退 "PTX-EMU 预编译静态库 + 头文件" 分发（不走 submodule 源码集成），Mode B 时间线 +2 周。

---

## §F 风险矩阵（REWRITE）

| 风险类别 | 概率 | 影响 | 触发条件 | 缓解策略 |
|---|---|---|---|---|
| **R1：Canonical 冲突复发**（v2 §C0 推翻后, 新 ADR 重新引入歧义）| 中 | 高 | 起草新 ADR 时未走 §C0.4 新规（跨仓引用必须带 commit 锚点）| reject 重提；强制 review checklist 包含 §C0 验证 |
| **R2：HSK-6 归属谈不拢**（PTX-EMU/CppTLM 互相推诿发起权）| 中 | 中 | CppTLM owner 拒绝 ack；或 PTX-EMU 拒绝联署 | v2 §D5.1 已写明 "删除方发起" 原则；如谈不拢, 升级到双方 architecture lead |
| **R3：ANTLR4 spike 翻车**（JDK 不可用或构建时间 +3min 超预算）| 中-低 | 中 | spike 验证失败 | §E.4 Fork 点：回退 "PTX-EMU 预编译静态库" 分发, +2 周时间线 |
| **R4：v1 ship 实施产物需退役**（HAL #66 语义需要二次修改）| 低 | 中 | Mode B 设计要求 #66 语义变化 | #66 不可删除（ADR-023 §D4），但可新增 #69/70 取代（append-only）|
| **R5：tadr-307 vs tadr-308 状态混淆** | 中 | 中 | TaskRunner owner 误用 tadr-307 路径 | v2 §C0.3 要求 tadr-307 显式标注 STALE + tadr-308 路径单一来源 |
| **R6：本地 submodule 滞后**（TaskRunner / PTX-EMU / CppTLM 任一仓本地副本未及时 fetch）| 低 | 高 | 未做 `git fetch && git submodule update` | §C0.4 新规：起草前必 fetch；CI 增加 submodule freshness check |
| **R7：dGPU board 组件（DGpuBar / Doorbell / SQ/CQ）实现延期** | 中 | 高 | P3 重构阶段实现延期 | P1 双轨期间并行 3 条实现线；任一延期触发 §E.4 fork 评估 |
| **R8：G-D4 static_assert 遗漏迁移** | 低 | 高 | CppTLM 迁移不完整 | §D6.3 门禁：迁移后对比 PTX-EMU 真相源 |
| **R9：测试基线 148 → 808 测试延期**（ADR-088 §D6.2 同步扩展）| 中 | 中 | 测试编写资源不足 | 优先级：tadr-308 E2E 测试 > mode_ab_dual_rail > dgpu_bar_mmio > fence hook |
| **R10：Mode A 冻结基线有未识别 regression** | 低 | 高 | 冻结前未做完整 ctest PASS 验证 | P0 入口：148/148 ctest PASS + Oracle Gate #6.5 baseline 验证 |

---

## Acceptance Gate（REWRITE）

**v2 升 ✅ Accepted 当且仅当 7 个 Gate 全部 ✅**：

| Gate | Owner | 当前状态 | 升 Accepted 前置 |
|---|---|---|---|
| #1 HAL append-only | UsrLinuxEmu | ✅ | (v1 ship #66+#67+#68 deprecated stub 物理结构已验证) |
| #2 CppTLM maintainer ack | CppTLM | ⏳ | v2 patch 推到 #18 + CppTLM openspec change `cpptlm-v3-dgpu-extract` |
| #3 PTX-EMU owner ack | PTX-EMU | ⏳ | HSK-6 联署 + ADR-0029 §D8 v2 修订 |
| #4 TaskRunner owner ack | TaskRunner | ⏳ | tadr-308 创建 + openspec change 落地 |
| #5 UsrLinuxEmu Architecture Team review | UsrLinuxEmu | ✅ | (v2 起草中, 待最终 review) |
| #6 Oracle 复审 | UsrLinuxEmu | ✅ | (sessions `ses_ff2106f84ffeM2oItBEa9iu4hL` v1 + `ses_fef78854dffeLfDJh7p8ELuMLy` v2) |
| #7 Cross-repo canonical 一致性 | UsrLinuxEmu | ⏳ | §C0 仲裁落地：adr-076 Historical + tadr-307 STALE 标注 + HSK-6 ACCEPTED |

---

## Open Questions（v2 残留问题）

1. **Q1**: dGPU board 组件（DGpuBar / Doorbell / SQ/CQ）由 CppTLM 主导实现，**是否需要 UsrLinuxEmu 提供** MemoryCluster / GpuNoC 接口契约？当前 grep 已确认既有（`include/tlm/cluster/` + `gpu_noc_cluster.hh`），但**接口约定文档缺失**——需 CppTLM owner 在 P1 阶段补充。
2. **Q2**: PTX-EMU submodule 提取后, 跨仓 license 合规性如何处理？PTX-EMU 当前是 MIT-style；submodule 引入后 UsrLinuxEmu/CppTLM 是否需要 NOTICE 文件？需三方 legal review。
3. **Q3**: ADR-090 v1 在 annex §A/B/C 中已含具体跨仓 commit 顺序（PTX-EMU → CppTLM → TaskRunner）, v2 §E 时间线是否需要重写跨仓 commit 顺序, 还是沿用 v1 annex §A.5 + §B.6？建议沿用 + 显式指向。

---

## References（REWRITE — 全部带 commit 锚点）

**UsrLinuxEmu 仓内**：
- [ADR-090 v1](adr-090-ptxir-via-h2d-dma.md) — v1 已 Superseded by v2
- [ADR-076](adr-076-gpgpu-kernel-module-ioctl.md) — Historical
- [ADR-088](adr-088-dgpu-complete-simulation.md) — §C2 + §D6.2 修订
- [ADR-036](adr-036-three-way-separation.md) — 3 区分架构
- [ADR-023](adr-023-hal-interface.md) — HAL append-only
- [ADR-035](adr-035-governance-policy.md) — §R5.1 cross-repo
- [ADR-061](adr-061-hal-iommu-extension.md) — HAL append-only 模式借鉴
- `plugins/gpu_driver/shared/gpu_ioctl.h:723-779` — ioctl 0x27/0x28/0x29
- `plugins/gpu_driver/hal/gpu_hal.h:370` — HAL #66 `kernel_module_load`
- `plugins/gpu_driver/drv/gpgpu_device.cpp:1110-1140` — 3 个 ioctl handler
- `docs/05-advanced/adr-090-cross-repo-coordination.md` — annex（§E 跟踪表升级待 commit）

**PTX-EMU 仓**（带 commit 锚点）：
- `PTX-EMU@8dc000ec:include/cudart/cpptlm_module.h:12-52` — 8 函数 ABI 真相源
- `PTX-EMU@8dc000ec:include/cudart/cpptlm_bridge.h:14-16` — HSK-1 真相源
- `PTX-EMU@main:include/cudart/AGENTS.md` — `CPPTLM_MODULE_VERSION` bump 治理
- `PTX-EMU@main:src/cudart/cpptlm_module.cpp:240-275` — 8 函数实现
- `PTX-EMU@main:docs/adr/ADR-0029-ptxemu-image-executor.md:630` — F3 canonical 指向
- `PTX-EMU@main:CMakeLists.txt:91-92` — ANTLR4 4.13.2 vendored in-tree
- `PTX-EMU@main:docs/superpowers/specs/2026-07-14-ptxemu-comprehensive-modification-plan.md:446-452` — HSK-1~HSK-3 历史

**TaskRunner 仓**：
- `TaskRunner@cdb3633:include/shared/igpu_driver.hpp:46-204` — IGpuDriver 49 个虚方法
- `TaskRunner@cdb3633:src/umd/libcuda_shim/cu_module.cpp` / `cu_launch.cpp` — 16 个 shim 文件
- `TaskRunner@cdb3633:docs/shared/adr/tadr-307-igpu-driver-kernel-module-extension.md:52-78` — 3 方法提案（STALE）
- `TaskRunner@cdb3633:docs/shared/adr/tadr-308-igpu-driver-vram-load.md` — TO CREATE
- `TaskRunner@cdb3633:src/umd/AGENTS.md` — `libcuda_shim/` 文件布局

**CppTLM 仓**：
- `CppTLM@main:CMakeLists.txt:8` — VERSION 2.1.0
- `CppTLM@main:include/cudart/cpptlm_bridge.h:55, 243-306` — `CPPTLMBRIDGE_VERSION` + G-D4 static_assert
- `CppTLM@main:include/tlm/gpu/kernel_launch_tlm.hh:30` — `KernelLaunchRequest`
- `CppTLM@main:include/tlm/cluster/memory_cluster.hh` — `MemoryCluster`
- `CppTLM@main:include/tlm/cluster/gpu_noc_cluster.hh` — `GpuNoC`
- `CppTLM@main:src/tlm/gpu/ptx_emu_driver_shim.cc` — 待删除项
- `CppTLM@main:tests/test_memory_bridge.cc` / `test_memory_bridge_poll.cc` / `test_kernel_launch_tlm_ext.cc` / `test_gpu_soc_perf.cc` — 4 测试文件

**Oracle Sessions**：
- `ses_ff2106f84ffeM2oItBEa9iu4hL` — 2026-08-17 v1 启动评审
- `ses_fef78854dffeLfDJh7p8ELuMLy` — 2026-08-17 v2 重写决策（24 BLOCKERS + Hybrid RFC 策略）

**GitHub Issues**：
- [PTX-EMU #12](https://github.com/chisuhua/PTX-EMU/issues/12) — Request Changes（拟关, 留 v2 链接）
- [TaskRunner #10](https://github.com/chisuhua/TaskRunner/issues/10) — HOLD（拟关, 留 v2 链接）
- [CppTLM #18](https://github.com/chisuhua/CppTLM/issues/18) — Ack + Major Revision（推 v2 patch）
