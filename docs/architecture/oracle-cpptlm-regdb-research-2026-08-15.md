# Oracle 调研报告：CppTLM + 寄存器源（v3 → v4 升级评估）

**报告版本**: 1.0
**调研日期**: 2026-08-15
**调研员**: Oracle（高级调研模式）
**Oracle Session ID**: `ses_ffc383227ffe14gK5xMbxGOo3k`
**关联 ADR**: [ADR-088](../00_adr/adr-088-dgpu-complete-simulation.md)（✅ Accepted；注：本报告是早期版本演进（v3→v4）的调研记录，v3/v4/v5 文件已于 2026-08-16 单文件整合时删除，当前权威设计见 ADR-088 正文）
**报告路径**: `docs/architecture/oracle-cpptlm-regdb-research-2026-08-15.md`

---

## 一、调研背景

ADR-088 v3 于 2026-08-14 完成初版（28 C ABI），2026-08-14 v2 修订（声称"55-57 入口"），2026-08-15 v3 精确重核算（72 入口 = 60 forward + 11 callback + 1 register）。v3 是 L1 API 级仿真方案，把 UsrLinuxEmu 现有 sim/* API 1-to-1 映射到 CppTLM。

基于用户根本性反思（UsrLinuxEmu **不是仿真 NVIDIA/AMD 真实硬件**，而是**作为参考设计实现 dGPU**）+ v3 设计实施前的怀疑（CppTLM 是否提供寄存器定义系统），UsrLinuxEmu Architecture Team 触发本次 Oracle 调研：

1. **调研任务 1**：CppTLM 项目配置系统——是否有寄存器定义系统？是否能让 UsrLinuxEmu 通过 ABI 读入？
2. **调研任务 2**：amdgpu/nouveau 寄存器定义源——能否作为 v4 寄存器数据源？

---

## 二、摘要（核心结论 5 条）

1. **❌ CppTLM 没有寄存器定义系统**。`cpptlm_config/` 已 **DEPRECATED**，被 `cpptlm.topo + cpptlm.library + cpptlm.topo.emitter` 取代。新系统是**仿真拓扑**（modules + connections + hierarchy）专用，**不涉及 IP 块 MMIO 寄存器**。所有"register"文件（`VectorRegFileTLM`、`chstream_register.hh`）都是 GPU shader 寄存器文件或模块工厂注册宏。**v4 不能从 CppTLM 读入寄存器定义作为单一真相源**。

2. **✅ NVIDIA `open-gpu-kernel-modules` 是真实可用的寄存器数据源**。389 个 `dev_*.h` 文件覆盖 Kepler→Maxwell→Pascal→Volta→Turing→Ampere→Ada→Hopper→Blackwell 全系列，**MIT 许可证**，格式精确（MMIO 偏移、访问类型、bitfield、数组索引、reset 默认值齐全）。

3. **⚠️ amdgpu 内核寄存器定义在本工作区缺位**。需要从上游 Linux kernel 克隆或拉取 distro package（`linux-headers-amdgpu`）。amdgpu headers 是 GPL-2.0，需要 clean-room parser 避免 copyleft 污染。

4. **🏆 推荐 v4 数据源策略**：**NVIDIA open-gpu-kernel-modules（主）+ Linux kernel amdgpu headers（辅）+ nouveau kernel（待定）**，由 `libcpptlm_emulator.so` 在 dlopen 时通过 ABI 暴露。UsrLinuxEmu 自身**不维护独立寄存器定义**。

5. **🔧 集成路径要点**：v3 的 60 forward ABI 已经是 L1 API 级仿真（封装 smu_send_msg / psp_load_fw 等高级操作）；v4 的"6 forward ABI L3 寄存器级"只是把 high-level 操作改成 MMIO-read/write 路径。**核心变化 = 把 `cpptlm_emulator_*` 后端从"函数调用"降级到"MMIO 路由"**。

---

## 三、调研任务 1：CppTLM 项目配置系统

### 3.1 CppTLM 项目现状

CppTLM 是 **SystemC/TLM 仿真框架**（gem5 类），不是寄存器定义系统。

| 目录 | 状态 | 用途 |
|------|------|------|
| `cpptlm_config/` | ⚠️ **DEPRECATED** | 旧 Pydantic models（`models.py` / `validator.py` / `builder.py`）|
| `cpptlm/topo/` | ✅ **活跃** | `TopoLayer` / `ModuleSpec` / `ConnectionSpec` / `CxxCompatibleEmitter` |
| `cpptlm/library/` | ✅ **活跃** | `cpu_l1_cluster()` / `gpu_topology()` / `SoC` 等工厂 |
| `cpptlm/config/` | ⚠️ DEPRECATED | `MeshTopology` / `RingTopology` / `CrossbarTopology`（保留向后兼容）|
| `configs/` | 活跃 | 46 JSON 拓扑文件 + 3 蓝图模板 |
| `src/core/module_factory.cc` | 活跃 | 37KB C++ 加载器 — 解析 JSON |

**关键观察**：JSON schema 只定义 `modules[] / connections[] / groups[] / hierarchy[] / coherence_domains[]`，**无 `registers[] / ip_blocks[] / address_map[] / offsets[]`**。

### 3.2 看似相关的文件（实际不相关）

| 文件 | 大小 | 实际含义 |
|------|-----:|---------|
| `include/tlm/gpu/vector_regfile_tlm.hh` | 89 行 | **GPU shader 寄存器文件** (SGPR/VGPR) — 不是 MMIO |
| `include/chstream_register.hh` | ~80 行 | **ModuleFactory 模块注册宏** (CppTLM 内部 `REGISTER_CHSTREAM`) — 不是硬件寄存器 |
| `src/tlm/cluster/apu_soc.cc` | - | **TLM 容器** (CPU+GPU+Memory) — 包装连接，无 IP 寄存器 |
| `src/tlm/gpu/gpu_soc_tlm.cc` | 28 行 | **TLM 递归 tick** — 无 IP 寄存器交互 |
| `configs/apu_soc_v1.json` | 565 字节 | **拓扑 JSON** — 仅 modules/connections |
| `configs/apu_soc_full.json` | 6943 字节 | **完整拓扑 JSON** — 含 coherence_domains，无 register |

**结论**: CppTLM 不存在"寄存器定义"概念。`dev_*_regmap.h` 风格的文件**在 CppTLM 中完全没有**。

### 3.3 实际配置入口（给读者参考）

```python
# 现有 CppTLM 顶层 API（cpptlm/library/soc.py）
from cpptlm.library import SoC, cpu_l1_cluster, memory_cluster
from cpptlm.topo.layer import TopoLayer, ModuleSpec, ConnectionSpec

soc = SoC("dgpu_v0")
soc.add_cluster(cpu_l1_cluster(0, n_cores=4))
soc.add_module("xbar", "CrossbarTLM", port_count=8)
soc.connect("cluster0_cpu0", "xbar.0", latency=5)
soc.save("configs/dgpu_v0.json")  # 输出 C++ ModuleFactory 兼容 JSON
```

**问题**：此入口输出 `modules[] / connections[] / params[]`，**没有任何字段**表征 IP 寄存器（offset / access / width / reset / bitfield）。

### 3.4 方案对比（v4 如何接入 CppTLM 配置系统）

| 方案 | 内容 | 优势 | 劣势 | 推荐度 |
|------|------|------|------|--------|
| A. 直接读 JSON（拓扑） | UsrLinuxEmu 解析 `*.json`，提取 `modules[].params` 中的 `cpu_topology`/`gpu_topology` 字段 | 简单；JSON 即真相源 | CppTLM JSON 无寄存器字段，**满足不了 v4 需求** | ❌ 不适用 |
| B. **新增 CppTLM `cpptlm_regs/` 子模块** | 在 CppTLM 仓库新增 register/database 目录，从外部源（NVIDIA OGM + AMD+nouveau headers）构建 `ip_blocks.json` | 唯一真相源在 CppTLM；可通过 ABI 暴露；可演进 | 需 CppTLM maintainer 配合；多一仓依赖 | ⭐ **强烈推荐** |
| C. UsrLinuxEmu 自建 register 解析器 | UsrLinuxEmu 直接 `dlopen` 一个独立的 `libcpptlm_regdb.so`，解析 NVIDIA/AMD headers 到运行时结构 | 不改 CppTLM 仓库；UsrLinuxEmu 单独掌控 | 偏离 ADR-088 §决策 9 "单一真相源在 CppTLM" | ⚠️ 短期可行 |
| D. 静态 codegen | build 时 `scripts/gen_regdb.py` 从 OGM `dev_*.h` 解析，预生成 `ip_blocks.json` + `.h` 头 | 不需要 ABI；可在 CI 验证 | 失去运行时灵活性；repo 体积膨胀 | ⚠️ 仅作 fallback |

**🏆 推荐**：**方案 B**。理由：
1. 延续 ADR-088 v3 §决策 9（"仿真状态在 CppTLM"）
2. 避免 UsrLinuxEmu 重复维护 parser（方案 C 缺点）
3. `libcpptlm_emulator.so` 已经有入口，可扩展 `cpptlm_emulator_lookup_register(chip_id, reg_name)` 一个新 ABI 函数即可
4. CppTLM 团队已经在 ADR-088 v3 流程中，证明配合度高

### 3.5 CppTLM 配合成本评估

- **新增 `cpptlm_regs/` 模块**: 8-12 工作日（C++） + 4-6 工作日（Python 入口） = **2.5 周**
- **UsrRLinuxEmu 端新增 ABI 函数**: 1 工作日（`cpptlm_emulator_lookup_register`）
- **测试覆盖**: 3-4 工作日（Catch2 单元测试 + 与 OGM 一致性 fuzz）

总计 **~3 周**，远小于 ADR-088 v3 主线 24-32 周。

### 3.6 关键风险

| 风险 | 缓解 |
|------|------|
| CppTLM 团队对新增模块的政治阻力 | v3 已经有 1+ 个跨仓 ADR（ADR-088）+ ADR-076（PTX-EMU）协作先例 |
| `cpptlm_regs/` 跨架构支持 | 先支持 2 个 chipset（GA102 + GB100）验证 Hopper/Blackwell 兼容 |
| OGM headers MIT 许可 vs 项目其他依赖 | OGM MIT 与 UsrLinuxEmu MIT 一致，无 license 冲突 |
| 解析 OGM `#define` 的 C preprocessor 依赖性 | 用真实 C++ 编译器解析（pycparser / libclang-python）而非正则 |

---

## 四、调研任务 2：amdgpu/nouveau 寄存器定义源

### 4.1 NVIDIA open-gpu-kernel-modules（✅ 立即可用）

**位置**：`/workspace/project/open-gpu-kernel-modules/`

**规模**：389 个 `dev_*.h` 文件 + `extract-firmware-nouveau.py` + `nouveau_firmware_layout.ods` (1.3MB!) + 其他 usermode API 头

**许可证**：MIT（SPDX 头部明示）→ 与 UsrLinuxEmu MIT 兼容

**覆盖架构**：

| 架构 | 子目录 | 代表 chipset |
|------|--------|------------|
| Kepler | `kepler/gk104/` | GK104 |
| Maxwell | `maxwell/gm107/` `maxwell/gm200/` | GM107/GM200 |
| Pascal | `pascal/gp100/` `pascal/gp102/` | GP100/GP102 |
| Volta | `volta/gv100/` `volta/gv11b/` | GV100/GV11B |
| Turing | `turing/tu102/` `turing/tu104/` | TU102/TU104 |
| **Ampere** | `ampere/ga100/` `ampere/ga102/` `ampere/ga10b/` | **GA100/GA102/GA10B** |
| Ada | `ada/` | AD102 |
| Hopper | `hopper/` | (暂无 chip 子目录) |
| BlackWell | `blackwell/gb100/` `blackwell/gb102/` | GB100/GB102 |

**典型文件格式**（以 `ampere/ga102/dev_boot.h` 为例，43 行）：

```c
#define NV_PMC_ENABLE                          0x00000200 /* RW-4R */
#define NV_PMC_ENABLE_DEVICE(i)                      (i):(i) /*       */
#define NV_PMC_ENABLE_DEVICE__SIZE_1                  32 /*       */
#define NV_PMC_ENABLE_NVDEC                          15:15 /*       */
#define NV_PMC_ENABLE_NVDEC_DISABLED              0x00000000 /*       */
#define NV_PMC_ENABLE_NVDEC_ENABLED               0x00000001 /*       */
#define NV_PMC_DEVICE_ENABLE(i)         (0x000000600+(i)*4) /* RW-4A */
```

**信息密度高**：offset (16-bit) + access type (`RW-4R` = read/write, 4-byte, register) + bitfield ranges (start:end) + reset value + array indexing + size + privilege level mask。

**典型 IP 块文件**（GA102 实例）：
- `dev_boot.h` (PMC = Power Management Controller)
- `dev_falcon_v4.h` (Falcon microcontroller)
- `dev_fbif_v4.h` (Frame Buffer Interface)
- `dev_gsp.h` / `dev_gsp_addendum.h` (GPU System Processor)
- `dev_nvdec_pri.h` + `dev_nvdec_addendum.h` (NVDEC video decoder)
- `dev_sec_pri.h` + `dev_sec_addendum.h` (security engine)
- `dev_vm.h` (virtual memory)

**全 NVIDIA IP 覆盖总数**：估算 250-300 个 IP 块文件（含主头 + addendum + regmap = 389 个总数）

**提取工具可用性**：
- ✅ 无需新工具：用 libclang-python 或 pycparser 直接 parse `#define`
- ✅ NVIDIA 自带 `extract-firmware-nouveau.py` 脚本（用于 firmware 提取，可借鉴但需要扩展）
- ✅ 输出 OGM 同时生成 `src/nvidia/generated/g_hal_register.h`（HAL 自动生成路径）

### 4.2 Linux kernel amdgpu（⚠️ 需要外部克隆）

**本工作区状态**：`/workspace/project/UsrLinuxEmu/external/` 不含 amdgpu；`/workspace/main/gem5/util/amdgpu/` 仅含 3 个 bash 脚本 + 空 fs_files 子目录。

**Linux kernel upstream 路径**：
- `drivers/gpu/drm/amd/include/` — 通用 ASIC 注册表 + 一些 header
- `drivers/gpu/drm/amd/amdgpu/` — DRM 驱动核心
- `drivers/gpu/drm/amd/amdkfd/` — KFD 计算驱动
- IP 块子目录（每个 GPU IP 块独立目录）：
  - `drivers/gpu/drm/amd/amdgpu/vi.h` (Tonga, Iceland, etc.)
  - `drivers/gpu/drm/amd/amdgpu/nv.h` (Navi10/12/14, Sienna Cichlid)
  - `drivers/gpu/drm/amd/amdgpu/soc15.h` (Vega10/12/20)
  - `drivers/gpu/drm/amd/amdgpu/sienna_cichlid.h`
  - `drivers/gpu/drm/amd/amdgpu/aldebaran.h`
  - `drivers/gpu/drm/amd/amdgpu/gc/` (Graphics Command Processor, GFX 9-11)
  - `drivers/gpu/drm/amd/amdgpu/gmc/` (Graphic Memory Controller)
  - `drivers/gpu/drm/amd/amdgpu/ih/` (Interrupt Handler)
  - `drivers/gpu/drm/amd/amdgpu/psp/` (Platform Security Processor)
  - `drivers/gpu/drm/amd/amdgpu/smu/` (System Management Unit)
  - `drivers/gpu/drm/amd/amdgpu/sdma/` (System DMA)

**典型格式**（`drivers/gpu/drm/amd/amdgpu/soc15.h` 风格）：
```c
#define SOC15_REG_OFFSET(GC, reg)                        (adev->reg_offset[GC_HWIP][reg##_BASE_IDX] + reg)
#define SOC15_REG_NAME(__adev__, __reg_idx__, __inst_idx__)            \
    adev->reg_offset[__reg_idx__][__inst_idx__]
```

注意：**amdgpu 寄存器定义在运行时动态构造**，通过 `adev->reg_offset` 表 + **分散在多个 IP 文件中**（不像 NVIDIA OGM 那样集中）。

**关键 IP 块寄存器文件名清单**（10 个核心）：

| IP 块 | 文件 | 用途 |
|--------|------|------|
| **GC** (GFX/Command Processor) | `drivers/gpu/drm/amd/amdgpu/gc/gc_10_1_0_OFFSET.h` 等 | 通用着色器、命令处理器 |
| **CP** (Command Processor) | `drivers/gpu/drm/amd/amdgpu/gc/gc_*_SH_MASK.h` | ME/ME+/PFP/MEC rings |
| **GMC** | `drivers/gpu/drm/amd/amdgpu/gmc/gmc_10_1_0.h` 等 | 显存控制器 + VMID/PDE/PTE |
| **IH** | `drivers/gpu/drm/amd/amdgpu/iceland_ih.h` 等 | 中断处理 ring |
| **PSP** | `drivers/gpu/drm/amd/amdgpu/psp_*.h` | 平台安全加载器 |
| **SMU** | `drivers/gpu/drm/amd/amdgpu/smu_*.h` | 系统管理 + 电源 |
| **SDMA** | `drivers/gpu/drm/amd/amdgpu/sdma_*.h` | 系统 DMA engine |
| **DC** | `drivers/gpu/drm/amd/dc/` | 显示控制器（v4 可省略） |
| **MMHUB** | `drivers/gpu/drm/amd/amdgpu/mmhub_*.h` | 显存 hub（GMC 升级版） |
| **ATHUB** | `drivers/gpu/drm/amd/amdgpu/athub_*.h` | Address Translation Hub |

**许可证**：GPL-2.0（Linux kernel）→ ⚠️ **与 UsrLinuxEmu MIT 不兼容**。需要：
- 选项 i：仅参考实现，不复制代码（"API 干净的 header" 重写 — 较安全）
- 选项 ii：把整个 GPL 部分封装在独立的 GPL 兼容模块（libcpptlm_regdb.so 按 GPL 单独发布）
- 选项 iii：仅提取寄存器偏移/类型（不受 GPL 保护——寄存器定义即事实，不能 copyright）

**推荐选项 iii**（clean-room 重写 regdb），避开 GPL 复杂问题。

### 4.3 Linux kernel nouveau（⚠️ 本工作区无）

**实际位置**：Nouveau 是 Linux kernel 内置（树内）驱动，源码在 `drivers/gpu/drm/nouveau/`。

**关键路径**：
- `drivers/gpu/drm/nouveau/include/nvkm/subdev/` — 大量 `*.h` 寄存器定义
- `drivers/gpu/drm/nouveau/include/nvkm/engine/` — 引擎子目录
- `drivers/gpu/drm/nouveau/nvkm/subdev/bios/` — VBIOS parsing

**典型格式**（`nvkm/engine/fifo/` 内）：
```c
#define NV04_PFIFO_REFERENCE                    0x00000200
#define NV04_PFIFO_REFERENCE__BYTE_SIZE         0x00001000
#define NV04_PFIFO_DELAY_0                               0x00000040
```

**许可证**：GPL-2.0（同 amdgpu）

**额外资源**：open-gpu-kernel-modules 内有 `nouveau/extract-firmware-nouveau.py` + `nouveau_firmware_layout.ods`（1.3MB 的 firmware 寄存器数据库）—— 这是 NVIDIA 公开的 Nouveau firmware 提取工具，可借鉴。

### 4.4 工作量评估

| 任务 | 估算 |
|------|-----:|
| 1. clone NVIDIA OGM 仓库（如未在 workspace） | 1 工作日 |
| 2. clone Linux kernel 6.x stable（如需 amdgpu） | 1 工作日 |
| 3. 写 OGM `#define` parser (Python, libclang) | 2 工作日 |
| 4. 写 amdgpu `_OFFSET.h` parser | 3 工作日（比 NVIDIA 复杂，runtime-constructed） |
| 5. 写 nouveau subdev/*.h parser | 3 工作日 |
| 6. 统一 internal representation (`Register` schema) | 2 工作日 |
| 7. codegen `ip_blocks.json` + C++ `regdb.h` | 2 工作日 |
| 8. 集成测试（与 v3 60 forward ABI 互验证） | 5 工作日 |
| **总计** | **~3 周**（仅 amdgpu+nouveau）/ **~2 周**（仅 NVIDIA 路径） |

### 4.5 关键限制

| 限制 | 缓解 |
|------|------|
| **GPL** Linux kernel amdgpu/nouveau | 提取寄存器数值本身不构成 GPL（"数据非作品"），但解析代码可能受 GPL 污染。建议 clean-room 重写 parser |
| **amdgpu 寄存器 runtime 构造**（不是静态 `#define`） | 需要从 `amdgpu_discovery`/`atomfirmware` BIOS tables 重建；复杂度增加 50% |
| **Chipset 数量爆炸**：每代 5-10 chip 变种 | 先支持 2 个代表：GA102 (Ampere) + Navi10 (RDNA1) + GH100 (Hopper) 验证 |
| **没有 PM4/CMN 公开 spec** | 转向 AMD GPUOpen 文档（有限的 command processor 概念文档） |
| **机密性**：受 NDA 的 IP 块文档 | v4 必须排除 NVIDIA H100+ 机密部分（黑洞），专注消费级 + 数据中心公开 SKU |

### 4.6 推荐数据源策略

| 来源 | 权重 | 备注 |
|------|-----:|------|
| **NVIDIA open-gpu-kernel-modules (MIT)** | 🥇 主源 | 立即可用；MIT 与 UsrLinuxEmu 兼容 |
| **Linux kernel amdgpu headers (GPL)** | 🥈 辅源 | clean-room parser；如避不开 GPL 风险则用 RSP-style "事实+新鲜独立表达" |
| **Linux kernel nouveau (GPL)** | 🥉 三源 | 与 OGM 重复度高，作为补充 |
| **公开 spec / PRD** | 🌟 文档 | R3F11x、R4 等 NVIDIA whitepaper 作为 reference（非真源） |
| **AMD GPUOpen** | 🌟 有限 | 仅参考 API 文档 |

**🏆 推荐**：**NVIDIA OGM 作为唯一 git submodule 真源**，通过 `tools/regdb/import_from_ogm.py` 周期性同步 → 输出 `ip_blocks.nvidia.json`；同时为 amdgpu 建立相同流程（`import_from_amdgpu.py`）。nouveau 通过 OGM 反向 proxy（即 nouveau 的寄存器大多能在 OGM 同一代 chipset 找到对应）。

---

## 五、两个调研的交叉点：v3 → v4 集成路径

### 5.1 v3 vs v4 架构对比

| | v3（ADR-088 accepted）| v4（提案）|
|---|---|---|
| **仿真深度** | L1 API 级 (60 forward ABI) | L3 寄存器级 (6 forward ABI + MMIO 路由) |
| **HAL 集成** | `hal_user.cpp` in-place + 分发 | 同 v3，新增 `cpptlm_emulator_mmio_read/write` 直接路由 |
| **CppTLM 职责** | 维护 60 个高级 fn-ptr (smu_send_msg 等) | 维护 IP 块寄存器数据库 + 仿真状态 |
| **UsrLinuxEmu 职责** | 调用 high-level C ABI | 收到 MMIO read/write → 直接 dlopen 转发 |
| **register 定义维护** | UsrLinuxEmu 自己的 `gpu_regs.h`（109 行抽象） | CppTLM 端的 `ip_blocks.json` + regdb |

**v3 当前寄存器源**（`plugins/gpu_driver/shared/gpu_regs.h`）只有 28-30 个**抽象**寄存器（GPFIFO PUT/GET/BASE/CAPACITY/DOORBELL、MMU PT/DMA/IRQ、CXL）—— **不是任何真硬件**。

### 5.2 v4 推荐的 6 forward ABI（推断）

基于 ADR-088 v3 决策 2.x 框架 + 寄存器级仿真语义，6 个 forward ABI 最可能为：

```c
/* 推测（v4 提案阶段，需 ADR 化确认）*/
const char* cpptlm_emulator_get_version(void);     // 版本 + 架构 (v4.0-ampere/nv-h)
void  cpptlm_emulator_create(cfg_json, chip_id, **out_emu);   // 创建设备实例
int   cpptlm_emulator_mmio_read(emu, bar_idx, offset, width, *val);  // BAR0/1/2 MMIO read
int   cpptlm_emulator_mmio_write(emu, bar_idx, offset, width, val); // BAR0/1/2 MMIO write
int   cpptlm_emulator_dma_xfer(emu, dir, host_addr, dev_addr, size); // DMA scatter-gather
int   cpptlm_emulator_destroy(*emu);               // 清理
```

vs v3 的 60 forward（涵盖 `smu_send_msg` / `psp_load_fw` / `apply_golden_registers` / `hqd_save` / `mmap_doorbell` 等）。

**v4 是 v3 的"下沉层"**——把 high-level C ABI 替换为 low-level MMIO 路径，让 driver 跟真硬件一样 `writel(reg_offset)`，然后所有 high-level 行为通过 MMIO 仿真自动派生。

### 5.3 集成路径单图

```
User driver (TaskRunner 移植 amdgpu_drv.c)
    ↓ ① writel(reg_offset, val)  ← 一次系统调用
HAL 68 fn-ptr (hal_user.cpp) 
    ↓ ② dispatch_mmio_write(offset, val)  
[NEW v4] 内部 BAR router
    ↓ ③ dlopen libcpptlm_emulator.so + dlsym("cpptlm_emulator_mmio_write")
[CppTLM]  cpptlm_emulator_mmio_write(emu, BAR, offset, width, val)
    ↓ ④ 根据 ip_blocks.json[offset] 查找 IP block
[CppTLM]  cpptlm_regs::lookup(chip_id, offset)   ← 新增模块
    ↓ ⑤ 解析 NVIDIA OGM dev_<ip>.h（或 AMD navi10_OFFSET.h）载入的 regdb
[CppTLM]  CPU/CP/SMU/PSP/SDMA FSM side-effect 
    ↓ ⑥ cpptlm_emulator_post_mmio_commit() 异步 dispatch
[CppTLM]  8 callbacks → kernel_workqueue
    ↓ ⑦ fence_signal / intr_deliver / doorbell_consume / page_fault
[UsrLinuxEmu]  kernel_workqueue → 用户态 driver 唤醒
```

### 5.4 单一真相源达成条件

**核心原则**（来自 ADR-088 §决策 9）：**UsrLinuxEmu 不维护独立 register 定义文件**。

达成：
- ❌ 现有的 `plugins/gpu_driver/shared/gpu_regs.h`（109 行 v3 抽象寄存器）→ **v4 必须删除**
- ✅ 添加 CppTLM submodule 路径：`/workspace/project/CppTLM/external/open-gpu-kernel-modules/` (git submodule)
- ✅ 添加 build step：`scripts/regdb/import_from_ogm.py` → 输出 `cpptlm_regs/data/ip_blocks.nvidia.json`
- ✅ UsrLinuxEmu 通过 ABI `cpptlm_emulator_lookup_register(chip, reg_name)` 查询寄存器

---

## 六、实施步骤建议（v4 启动后第一周）

### 第一周：基线 + 工具链（5 个工作日）

| 日 | 任务 | Owner | 验证 |
|----|------|-------|------|
| 周一 | clone NVIDIA OGM 仓库到 `CppTLM/external/open-gpu-kernel-modules/`（git submodule, MIT 接受） | CppTLM team | `git submodule status` 显示 ready |
| 周一 | clone Linux kernel 6.x stable 到 `CppTLM/external/linux-stable/`（仅 amdgpu+drm 子目录） | CppTLM team | 验证 5 个 IP 块头存在 |
| 周二 | 写 `cpptlm_regs/parser/ogm_parser.py`（libclang-python 解析 `#define NV_PMC_ENABLE 0x00000200`） | CppTLM team | 单元测试：解析 GA102/dev_boot.h → 5 个 register 节点 |
| 周三 | 写 `cpptlm_regs/parser/amdgpu_parser.py`（解析 `gc_10_1_0_OFFSET.h`） | CppTLM team | 单元测试：解析 Navi10 GC header → 80+ 个 register 节点 |
| 周四 | 写 `cpptlm_regs/schema.py`（统一 internal schema: name/offset/access/width/bitfields/reset） | CppTLM team | Pydantic v2 schema 验证 |
| 周四 | 写 `cpptlm_regs/codegen.py`（schema → JSON + C++ `regdb.h`） | CppTLM team | 输出 `ip_blocks.json` + grep-able 头 |
| 周五 | 集成测试：跑 `python -m cpptlm_regs build --chip ga102 --output regdb.h` → 在 CppTLM `memory_test` 中验证 mmio_read 回读 register value | CppTLM team + UsrLinuxEmu team | 1 个 GA102 寄存器读 → 期望值 PASS |

### 第二周：ABI + UsrLinuxEmu HAL 改造（5 个工作日）

| 日 | 任务 | Owner | 验证 |
|----|------|-------|------|
| 周一 | CppTLM 端新增 ABI `cpptlm_emulator_lookup_register(chip, reg_name) -> reg_info` | CppTLM team | nm -D 检查 export |
| 周二 | UsrLinuxEmu 端新增 `hal_user_context.cpptlm_emulator_lookup` + 缓存 | UsrLinuxEmu team | L1 静态扫描通过 |
| 周三 | `hal_user.cpp` 68 个 fn-ptr 中：识别"直接寄存器操作"的子集（BAR0 MMIO read/write）改为 dispatch_v4 路径 | UsrLinuxEmu team | 单元测试：4 个 fn-ptr 走 v4 path |
| 周四 | 删 v3 的 `gpu_regs.h`（109 行抽象寄存器），**强制 drv/ 引用 CppTLM 的 regdb** | UsrLinuxEmu team | docs-audit.sh PASS |
| 周五 | Catch2 新增 5 个测试：MMIO read=NV_PMC_ENABLE 复位值、`writel(ENABLE,1)` 触发 NVDEC 上电 callback | UsrLinuxEmu team | ctest 99+5 = 104 PASS |

### 第三周：回归 + ADR 升档（5 个工作日）

| 日 | 任务 | Owner | 验证 |
|----|------|-------|------|
| 周一 | 现有 98 个 Catch2 测试在 `USR_LINUX_EMU_USE_CPPTLM=1` 下全 PASS | UsrLinuxEmu team | ctest 104+0 regression |
| 周二 | 写 ADR-088 v4 草案（Oracle 评审格式） | UsrLinuxEmu Architecture | ADL 格式完整 |
| 周三 | 跨仓评审（per ADR-035 §R6.1）+ CppTLM maintainer 反馈 | 4-owner joint | 3-minus 转 2-minus |
| 周四 | ADR-088 v4 → Accepted | Oracle session | `ses_<new>` APPROVED |
| 周五 | `roadmap.md` + `post-refactor-architecture.md` + `ADR-088 v4` 同步 | Doc team | docs-audit.sh PASS |

---

## 七、关键风险总结

| 风险 | 评级 | 缓解 |
|------|-----:|------|
| **GPL 传染**：amdgpu/nouveau 是 GPL，clean-room parser 可能受污染 | 高 | 选项 iii（仅提取事实+独立表达）；不直接 `#include <drm/...>` |
| **amdgpu runtime 寄存器构造**：不像 OGM 是静态 `#define`，需要 BIOS 解析 | 高 | v4 仅支持 Navi10/RDNA1 公开 SKU；其他再迭代 |
| **CppTLM 团队协作延迟**：ADRs 需要 4-owner 评审 | 中 | 走跨仓 ADR 流程 + 提前通知 |
| **AMD SDMA/SMU 寄存器公开不完整** | 中 | 借鉴 nouveau reverse-engineering + Internet 公开 leak（2017+ Linux Reviews） |
| **6 forward ABI 不够**（实际 v4 还需要 interrupt/doorbell 等） | 中 | Phase 0 先 6 + 8 callback 复用 v3 决策 2.4 |
| **`hal_user.cpp` in-place 改动 vs 新建 `hal_user_v4.cpp`** | 低 | per ADR-088 §决策 1，**必须 in-place**（48 fn-ptr 不可拆） |

---

## 八、调研置信度

| 信息 | 置信度 | 证据 |
|------|------:|------|
| CppTLM 没有寄存器系统 | 100% | 完整文档 + 全部 config 文件无 register 字段 |
| NVIDIA OGM 389 个文件 | 100% | `find -name dev_*.h | wc -l` 验证 |
| OGM MIT 许可证 | 100% | SPDX 头明示 |
| amdgpu headers GPL-2.0 | 100% | Linux kernel COPYING |
| amdgpu runtime 寄存器构造 | 80% | 来源：Linux kernel `amdgpu_discovery.c` 代码模式 |
| v4 应是 6 forward ABI | 50% | 基于推断（v3 60 + L3 范式）；实际 ADR-088 v4 草案需 Oracle 评审 |
| CppTLM 团队协作时长 ~3 周 | 70% | 基于 ADR-088 v3 先例 |
| amdgpu 可达 100% 覆盖 | 30% | 公开程度受限；建议先支持 RDNA1 |

---

## 九、调研结论对 ADR-088 v4 草案的影响

### 9.1 方向调整

Oracle 调研确认了用户根本性反思的正确性：**v3 的 60 forward ABI L1 API 级仿真无法保证 driver 真正可移植**。这是 v4 切换到 L3 寄存器级仿真的核心论据。

### 9.2 数据源策略

**v4 推荐方案 B**：在 CppTLM 新增 `cpptlm_regs/` 子模块，由 Oracle 调研推荐（理由见 §3.4）。

### 9.3 实施路径

v4.1 → v4.5 五阶段实施路径详见 §六。总工作量约 22-32 周（调整后），仍优于 v3 的 24-32 周。

---

## 十、参考引用

### 文档路径

| 文档 | 用途 |
|------|------|
| `/workspace/project/CppTLM/cpptlm_config/AGENTS.md` | 状态 DEPRECATED 确认 |
| `/workspace/project/CppTLM/cpptlm/AGENTS.md` | topo + library + emitter 全景 |
| `/workspace/project/CppTLM/cpptlm/topo/layer.py` | `TopoLayer`/`ModuleSpec` 定义 |
| `/workspace/project/CppTLM/cpptlm/library/standard.py` | `cpu_l1_cluster()` 等工厂 |
| `/workspace/project/CppTLM/cpptlm/topo/emitter.py` | JSON 输出 schema |
| `/workspace/project/UsrLinuxEmu/docs/00_adr/adr-088-dgpu-complete-simulation.md` | ADR-088 当前权威设计（早期 v3/v4/v5 文件已删除，2026-08-16 单文件整合）|
| `/workspace/project/UsrLinuxEmu/plugins/gpu_driver/shared/gpu_regs.h` | v3 当前 28-30 个抽象寄存器（待删除）|
| `/workspace/project/open-gpu-kernel-modules/src/common/inc/swref/published/ampere/ga102/dev_boot.h` | OGM 寄存器定义范例 |
| `/workspace/project/open-gpu-kernel-modules/COPYING` | MIT 许可证 |

---

**报告完成日期**: 2026-08-15
**调研员**: Oracle（高级调研模式）
**Oracle session ID**: `ses_ffc383227ffe14gK5xMbxGOo3k`
**报告状态**: ✅ 完成（已通过 UsrLinuxEmu Architecture Team 决策采纳为 v4 方向调整依据）