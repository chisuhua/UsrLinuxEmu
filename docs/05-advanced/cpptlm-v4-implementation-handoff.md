# CppTLM Implementation Handoff Spec: UsrLinuxEmu dGPU 板卡仿真

**文档版本**: 4.0（**范围收窄 + 修订：CppTLM 仅 dGPU 板卡，23 ABI**；系统 IOMMU + CXL.mem 移至 UsrLinuxEmu `src/system_hw/`，**不在本 spec 范围**）
**日期**: 2026-08-16（v3.0 范围声明修订 + v4.0 正文全面重写）
**接收方**: CppTLM maintainer team
**来源方**: UsrLinuxEmu Architecture Team
**关联 ADR**: [ADR-088](../00_adr/adr-088-dgpu-complete-simulation.md)（✅ Accepted；dGPU 参考设计——完整硬件子系统仿真；**唯一权威**）
**状态**: 🔄 Draft for CppTLM Review（**实施前可审查并提出意见**）

> ⚠️ **🚨 本 spec 关键约束（v4.0 重写后）**：
>
> 1. **CppTLM 范围严格限于 dGPU 板卡**（23 ABI）：BAR MMIO + 寄存器元数据 + 4 callback + register_callbacks + 多板卡枚举 + PCIe Config Space + MSI-X + backdoor + DMA translate cb
> 2. **不仿真系统 IOMMU**——由 UsrLinuxEmu `src/system_hw/iommu/` 内部模块提供
> 3. **不仿真 CXL.mem 设备**——由 UsrLinuxEmu `src/system_hw/cxl_memdev/` 内部模块提供
> 4. **不仿真 CPU / 系统内存 / 中断控制器 / 系统总线**——如果 `linux_compat/` 内部实现需要，由 UsrLinuxEmu 单独提供
> 5. **本 spec 是 v4.0 全面重写版**：v3.0 之前的 31 ABI 设计（含 v4.6 IOMMU + v4.7 CXL）已**完全删除**，本版本严格遵循 [ADR-088 §D5](../00_adr/adr-088-dgpu-complete-simulation.md) 的 23 ABI 清单
> 6. **ABI 清单以 ADR-088 §D5 为唯一权威**——任何与 ADR-088 不符的描述以 ADR-088 为准

---

## 0. 范围与变更说明（请先读这一节）

**本 spec 是 v4.0 全面修订版本，**基于 ADR-088 范围收窄后**的 23 ABI 实施清单**。

**为什么需要 v4.0 修订**：
- v3.0 文档顶部声明 "23 ABI 范围收窄"，但正文/附录仍描述 v5 31 ABI（含 IOMMU + CXL）——内部矛盾
- v3.0 附录 A 列出 `cpptlm_iommu_domain` + `cpptlm_cxl_memdev` 两个 CppTLM 模块——这违反 ADR-088 §D1 "CppTLM 仅仿真 dGPU 板卡"
- v3.0 缺失 ADR-088 §D3.8 新增的 `cpptlm_emulator_register_dma_translate_cb`（1 ABI）
- v3.0 版本字符串与 ADR-088 §D6.2 不一致（v3.0 用 `"v4.0-dgpu-v0"`；ADR-088 用 `"v1.0-dgpu-v0"`）

**v4.0 关键修正**：
1. **正文与附录严格按 ADR-088 §D5 重排**：基础 11 + 板卡扩展 8 + MSI-X 3 + DMA translate cb 1 = **23 ABI**（**不再含 IOMMU 5 + CXL 4**）
2. **删除所有 IOMMU/CXL 描述**：附录 A.3（v4.6）+ A.4（v4.7）+ A.6 中 `cpptlm_iommu_domain` / `cpptlm_cxl_memdev` 模块
3. **新增 §4.x DMA translate callback ABI**（per ADR-088 §D3.8）
4. **修正版本字符串**：`cpptlm_emulator_get_version()` 返回 `"v1.0-dgpu-v0"`
5. **修正时间估算**：按 23 ABI 重新核算，CppTLM 5-7 周（vs v3.0 错误声称 20-27 周）

**v4.0 仿真范围**（与 ADR-088 §C2 完全一致）：
- ✅ **dGPU 板卡**（23 ABI）：BAR MMIO + 寄存器元数据 + 4 callback + register_callbacks + 多板卡枚举 + PCIe Config Space + MSI-X + backdoor + DMA translate cb
- ✅ **DMA 地址转换 callback**（1 ABI 包含在 23 内）：dGPU DMA 时回调 UsrLinuxEmu 系统 IOMMU 翻译 IOVA→PA
- ❌ **不仿真系统 IOMMU**：由 UsrLinuxEmu `src/system_hw/iommu/` 内部模块提供（非 CppTLM 范围）
- ❌ **不仿真 CXL.mem 设备**：由 UsrLinuxEmu `src/system_hw/cxl_memdev/` 内部模块提供（非 CppTLM 范围）
- ❌ **不仿真 CPU / 系统内存 / 中断控制器 / 系统总线**

**对 CppTLM team 的关键问题**（v4.0 修订后）：
- P1：23 ABI 工作量（基础 11 + 板卡扩展 8 + MSI-X 3 + DMA translate cb 1）是否在 CppTLM 团队承担范围内？
- P2：v4.0 是否需要新的 CppTLM 内部模块（`cpptlm_pcie_device` / `cpptlm_msix_table`）？**不需要** `cpptlm_iommu_domain` / `cpptlm_cxl_memdev`（已移出范围）
- P3：v4.0 backdoor 接口是否遵循 gem5 MemBackdoor 模式？（是）
- P4：v4.0 ABI 命名约定（23 个）是否需调整以保持 CppTLM 风格一致？
- P5：v4.0 分阶段实施（基础 → 板卡 → MSI-X → DMA translate）vs 一次性完整实施 哪个更可接受？

---

## 1. 执行摘要（请先读这一节）

UsrLinuxEmu v4.0 计划通过 `libcpptlm_emulator.so` SHARED 库提供**dGPU 板卡完整仿真**（**23 ABI**：16 forward C ABI + 4 callback typedef + 1 register + 2 callback 注册函数；详细清单见 [ADR-088 §D5](../00_adr/adr-088-dgpu-complete-simulation.md)）。这与 CppTLM 现有的"仿真拓扑"模块（`cpptlm.topo + cpptlm.library`）互补——**v4.0 不要求 CppTLM 改造现有拓扑系统**，而是**新增 1 个子模块 `cpptlm_regs/`**（Python）+ **若干 C++ 模块**专门提供寄存器定义 + PCIe Config Space + MSI-X + backdoor + DMA translate cb。

**关键约束（请确认）**：
1. **不破坏现有 CppTLM 接口**：`cpptlm_topo.py`、`cpptlm_library/` 等保持不变
2. **`libcpptlm_emulator.so` 是 SHARED 库**：需把现有 `cpptlm_core` (STATIC) 改造为支持 SHARED 构建（per ADR-088）
3. **`cpptlm_regs/` 是新模块**：Pydantic schema + parser + codegen，遵循 CppTLM 现有 Python 模块约定
4. **本次仅涉及 CppTLM 阶段 1（3-4 周）+ 阶段 2a（2-3 周）**：阶段 2b/3/4 由 UsrLinuxEmu team 主导（per ADR-088 §D2）

**关键问题**（需 CppTLM team 反馈）：
- P1：`cpptlm_core` 改造为 SHARED 库的改动范围？影响其他下游用户吗？
- P2：新增 `cpptlm_regs/` 子模块是否影响 CppTLM 现有 Python 依赖结构？
- P3：6 ABI C 函数的命名空间、参数类型、错误码约定是否需调整？
- P4：`cpptlm_emulator_lookup_register()` 返回的 `reg_info_t` 结构体字段是否完整？
- P5：4 callback typedef 命名（`cpptlm_intr_deliver_cb_t` 等）是否与 CppTLM 现有命名约定一致？

---

## 2. 范围与工作量

### 2.1 CppTLM team 负责范围（**per ADR-088 §D2 阶段 1 + 2a，总 5-7 周**）

| 阶段 | 周 | Owner | 交付物 | 新增 ABI |
|------|---:|-------|--------|---------:|
| **阶段 1：PCIe 基础 + 多板卡 + backdoor** | 3-4 | CppTLM team | `cpptlm_pcie_device` 模块 + Config Space 模拟 + 多板卡支持 + backdoor | +8 |
| **阶段 2a：MSI-X + DMA translate cb** | 2-3 | CppTLM team | `cpptlm_msix_table` 模块 + DMA translate callback 注册路径 | +4 |
| **CppTLM 合计** | **5-7** | — | 基础 11 + 板卡扩展 8 + MSI-X 3 + DMA translate cb 1 = **23 ABI** | +12 |

> **v4.0 修订说明**：相比 v3.0 声称的 "v4.1（2 周）+ v4.2-v4.7 扩展（20-27 周）"，v4.0 按 ADR-088 范围收窄后，**CppTLM 总工作量从 22-32 周降至 5-7 周**（IOMMU 5 + CXL 4 + 部分 v4.5 拆分已移至 UsrLinuxEmu `src/system_hw/`）。

**阶段 1 工作量拆解**（3-4 周，per ADR-088）：

| 周 | 任务 | 产出 | 新增 ABI |
|----|------|------|---------:|
| Week 1 | `cpptlm_pcie_device` 基础类 + Config Space 256B 数组 + capability chain 链表 | 5 个 capability 实现（PM / MSI-X / PCIe / Vendor Specific）| 0（基础） |
| Week 2 | 多板卡支持：`std::vector<DeviceInstance>` + device id 索引 | 2 ABI（get_device_count + get_device_info）| +2 |
| Week 3 | `cpptlm_pcie_host` 总线枚举模拟（PCIe 总线扫描 + BAR 分配）| 1 ABI（create_by_id）+ 2 ABI（pcie_config_read/write）| +3 |
| Week 4 | backdoor 实现（参考 gem5 MemBackdoor）：`uint8_t* ptr()` + invalidation callback | 3 ABI（backdoor_read/write + register_backdoor_cb）| +3 |

**阶段 2a 工作量拆解**（2-3 周，per ADR-088）：

| 周 | 任务 | 产出 | 新增 ABI |
|----|------|------|---------:|
| Week 5-6 | `cpptlm_msix_table` 硬件仿真：pending bitmap + interrupt vector table + PBA 表 | 3 ABI（msix_init/update_pending/clear_pending）| +3 |
| Week 6 | `cpptlm_emulator_register_dma_translate_cb` 注册路径 + DMA 调用点接入（dGPU DMA → 系统 IOMMU 回调）| 1 ABI | +1 |

### 2.2 CppTLM team 不涉及的范围

| 阶段 | Owner | 说明 |
|------|-------|------|
| 系统 IOMMU 仿真 | UsrLinuxEmu team | `src/system_hw/iommu/` 内部模块（5 函数：domain_alloc / attach_dev / map / unmap / iova_to_phys）|
| CXL.mem 仿真 | UsrLinuxEmu team | `src/system_hw/cxl_memdev/` 内部模块（4 函数：memdev_read / memdev_write / pmem_init / flush）|
| HAL 改造 | UsrLinuxEmu team | `hal_user.cpp` 68 fn-ptrs 按 ADR-088 P4 保持不变（per ADR-023 §D4 append-only 治理）；新增 CppTLM backend 通过 if/else 分支动态 dispatch |
| `linux_compat/` 重构 | UsrLinuxEmu team | 桥接层（仅 Linux 内核 API 表面 ~2500-3000 行；dGPU 行为委托 CppTLM 23 ABI，系统硬件行为委托 `src/system_hw/`）|
| 集成测试 | UsrLinuxEmu + TaskRunner team | 98 Catch2 测试 + amdgpu driver 移植 |
| 端到端零修改验证 | UsrLinuxEmu team | 5 个 driver 必测函数（per ADR-088 §D7.1）在真 Linux 6.6+ 内核源码树下编译通过（Gate 4.5，不链接）|
| v5.5+ 扩展（VFIO / IOMMUFD / CXL.cache / CXL 2.0+ HDM / Nested IOMMU domain）| 双方 | v5.5+ 评估（per ADR-088 §Open Questions）|

---

## 3. 新增 `cpptlm_regs/` 子模块架构

### 3.1 目录结构（请确认是否符合 CppTLM Python 模块约定）

```
CppTLM/
├── cpptlm_regs/                          # 新增子模块
│   ├── __init__.py                       # 模块入口（暴露 public API）
│   ├── schema.py                          # Pydantic v2 模型
│   │                                     # - Register (offset, access, width, reset, bitfields)
│   │                                     # - IPBlock (name, base_offset, registers[])
│   │                                     # - BAR (bar_idx, base_addr, size, ip_blocks[])
│   │                                     # - RegDB (chip_id, version, ips[], bars[], side_effects[])
│   │
│   ├── parser/
│   │   ├── __init__.py
│   │   └── custom_parser.py              # 解析 dgpu_v0_*.yaml
│   │
│   ├── codegen.py                        # schema → C++ regdb.h
│   │                                     # 输出：constexpr RegisterDef entries[]
│   │
│   ├── dgpu_v0/                          # 默认 dGPU v0 寄存器集（4 个 YAML）
│   │   ├── registers.yaml                # 45 个寄存器定义
│   │   ├── ip_blocks.yaml                # 11 个 IP 块组织
│   │   ├── bar_layout.yaml               # BAR 映射
│   │   └── side_effects.yaml             # 寄存器写入触发的行为（FSM）
│   │
│   ├── tests/
│   │   ├── test_parser.py
│   │   ├── test_codegen.py
│   │   ├── test_lookup.py
│   │   └── test_yaml_validity.py         # 验证 4 个 YAML 符合 schema
│   │
│   └── README.md                         # cpptlm_regs 单独 README
│
├── src/tlm/gpu/                          # 现有（不修改）
│   └── ...
└── src/core/                             # 现有（v4.1 需小修改）
    └── cpptlm_regs_lookup.{hh,cc}        # 新增（v4.1 周四）
```

### 3.2 Pydantic 模型设计（`schema.py`）

```python
# CppTLM/cpptlm_regs/schema.py
from pydantic import BaseModel, Field
from enum import Enum
from typing import List, Optional

class AccessType(str, Enum):
    """寄存器访问类型"""
    RO = "RO"    # Read-Only
    RW = "RW"    # Read-Write
    WO = "WO"    # Write-Only
    RW1C = "RW1C"  # Read, Write-1-to-Clear
    RWC = "RWC"  # Read-Write-Clear

class BitField(BaseModel):
    """位域定义"""
    name: str
    msb: int = Field(ge=0, le=63)
    lsb: int = Field(ge=0, le=63)
    reset: int = 0
    description: Optional[str] = None

class Register(BaseModel):
    """单个寄存器定义"""
    name: str
    offset: int = Field(description="BAR 内偏移（16-bit）")
    access: AccessType
    width: int = Field(default=4, ge=1, le=8, description="字节宽度（1/2/4/8）")
    reset: int = 0
    description: Optional[str] = None
    bitfields: List[BitField] = Field(default_factory=list)

class IPBlock(BaseModel):
    """IP 块（如 CP/GMC/IH/SDMA 等）"""
    name: str
    base_offset: int
    description: Optional[str] = None
    registers: List[Register]

class BAR(BaseModel):
    """BAR 映射定义"""
    bar_idx: int = Field(ge=0, le=5)
    base_addr: int = Field(description="64-bit 系统物理地址")
    size: int = Field(description="BAR 大小（字节）")
    description: Optional[str] = None

class SideEffect(BaseModel):
    """寄存器写入触发的副作用（如 FSM 状态机迁移）"""
    trigger: str = Field(description="触发条件，如 'writel(GOLDEN_REG, !=0)'")
    action: str = Field(description="执行动作，如 'transition(CP, IDLE → FETCH)'")

class RegDB(BaseModel):
    """完整寄存器数据库"""
    chip_id: str = Field(description="如 'dgpu-v0'")
    version: str = Field(description="如 'v0.1.0'")
    description: Optional[str] = None
    ips: List[IPBlock]
    bars: List[BAR]
    side_effects: List[SideEffect] = Field(default_factory=list)

    class Config:
        # regdb.h ABI stability 政策（per ADR-088 §D6.2）
        # v0.x 主版本内 ABI 稳定，YAML 仅追加（不修改现有字段）
        schema_extra = "Pydantic v2 schema, see ADR-088 §D6.2"
```

### 3.3 `cpptlm_regs/dd_gpu/registers.yaml` 示例

```yaml
# CppTLM/cpptlm_regs/dgpu_v0/registers.yaml
chip_id: dgpu-v0
version: 0.1.0
description: UsrLinuxEmu dGPU v0 reference design register set

ips:
  - name: GPU_ID
    base_offset: 0x0000
    description: GPU ID and Version
    registers:
      - name: CHIP_ID
        offset: 0x0000
        access: RO
        width: 4
        reset: 0xDEADBEEF
        description: Hardware chip identifier
      - name: VERSION
        offset: 0x0004
        access: RO
        width: 4
        reset: 0x00040000  # v0.1.0

  - name: CP  # Command Processor
    base_offset: 0x1000
    description: Ring buffer + command processor
    registers:
      - name: CP_RING_BASE
        offset: 0x1000
        access: RW
        width: 4
        reset: 0x00000000
        description: Ring buffer base address (low 32-bit)
      - name: CP_RING_SIZE
        offset: 0x1004
        access: RW
        width: 4
        reset: 0x00000800  # 2KB default
      # ... 继续定义 CP_RING_READ_PTR, CP_RING_WRITE_PTR, CP_DOORBELL,
      # CP_CONTROL, CP_STATUS, CP_INTERRUPT_MASK

# ... 其他 10 个 IP 块（GMC, IH, SDMA, Fence, HQD, KFD, PM, Error, Misc）

bars:
  - bar_idx: 0
    base_addr: 0x10000000
    size: 0x100000  # 1MB MMIO (CP/GMC/IH/SDMA/Fence/HQD/KFD/PM/Error/Misc)
    description: BAR0 - MMIO registers
  - bar_idx: 2
    base_addr: 0x20000000
    size: 0x10000000  # 256MB VRAM aperture
    description: BAR2 - VRAM backing store
  - bar_idx: 5
    base_addr: 0x30000000
    size: 0x1000  # 4KB doorbell page
    description: BAR5 - Doorbell page (1024 slots × 8 bytes)

side_effects:
  - trigger: "writel(CP_DOORBELL, !=0)"
    action: "CP.state = IDLE → FETCH (fetch first IB from ring buffer)"
  - trigger: "writel(PM_RESET, 1)"
    action: "trigger FLR; call reset_complete_cb after completion"
  - trigger: "writel(PM_STATE, 3)"
    action: "transition D0 → D3; call power_cb(3)"
```

---

## 4. ABI 契约详细规格

### 4.1 6 Forward C ABI（`libcpptlm_emulator.so` 导出）

```c
/* CppTLM/include/cpptlm_emulator.h（新增公共头文件） */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

/* 版本字符串：当前 "v1.0-dgpu-v0"（per ADR-088 §D6.2 Semver 锁定）*/
const char* cpptlm_emulator_get_version(void);

/* 创建设备实例（成功返回 opaque handle；失败返回 NULL） */
void* cpptlm_emulator_create(
    const char* regdb_yaml_path,  /* IN: dgpu_v0/registers.yaml */
    const char *chip_id             /* IN: must match YAML.chip_id; else NULL */
);

/* BAR MMIO 读（核心 ABI，per v4 §D1）
 * 返回值：0=成功, -1=emu==NULL, -2=offset超范围, -3=width非法, -4=val==NULL */
int cpptlm_emulator_mmio_read(
    void   *emu,        /* IN: 设备实例 */
    uint8_t bar_idx,    /* IN: 0/1/2/5（v4 dGPU 无 BAR1/3/4）*/
    uint32_t offset,    /* IN: BAR 内偏移 */
    uint8_t width,      /* IN: 1/2/4/8 字节 */
    uint64_t *val       /* OUT: 读取值 */
);

/* BAR MMIO 写（核心 ABI，同返回码约定） */
int cpptlm_emulator_mmio_write(
    void   *emu,
    uint8_t bar_idx,
    uint32_t offset,
    uint8_t width,
    uint64_t val
);

/* 查询寄存器元数据（per v4 §D1 P3 修复：chip_id 冗余校验语义）
 * 返回值：0=找到, -1=未找到 */
int cpptlm_emulator_lookup_register(
    void        *emu,
    const char  *reg_name,   /* IN: e.g. "CP_DOORBELL" */
    reg_info_t  *info        /* OUT: {offset, access, width, reset} */
);

/* reg_info_t 结构体定义（v4 新增，需 CppTLM 头文件中定义） */
typedef struct {
    uint32_t offset;     /* BAR 内偏移 */
    uint8_t  access;     /* AccessType 枚举（0=RO, 1=RW, 2=WO, ...）*/
    uint8_t  width;      /* 字节宽度 */
    uint16_t reserved;   /* 对齐填充 */
    uint32_t reset;      /* 复位值 */
} reg_info_t;

/* 销毁设备实例 */
int cpptlm_emulator_destroy(void *emu);

#ifdef __cplusplus
}
#endif
```

### 4.2 4 Callback typedef（事件通知）

```c
/* CppTLM/include/cpptlm_emulator_callbacks.h（新增）*/

/* MSI-X 中断传递（per v4 §D1） */
typedef int (*cpptlm_intr_deliver_cb_t)(
    uint32_t vector,    /* 中断向量（0-7，per v4 默认 8 个）*/
    uint64_t user_data  /* 中断上下文（来自 register_callbacks）*/
);

/* 错误事件（ERROR_STATUS 寄存器写入时触发） */
typedef int (*cpptlm_error_cb_t)(
    uint32_t err_code
);

/* FLR 重置完成（PM_RESET 寄存器触发后） */
typedef int (*cpptlm_reset_complete_cb_t)(void);

/* D0/D3 电源切换（PM_STATE 寄存器写入时触发） */
typedef int (*cpptlm_power_cb_t)(
    uint32_t state      /* 0=D0, 3=D3 */
);

/* 一次性注册所有 callback（per v4 §D1） */
int cpptlm_emulator_register_callbacks(
    void                          *emu,
    cpptlm_intr_deliver_cb_t       intr_cb,
    cpptlm_error_cb_t              error_cb,
    cpptlm_reset_complete_cb_t     reset_cb,
    cpptlm_power_cb_t              power_cb
);
```

### 4.3 DMA 地址转换 callback ABI（**v4.0 新增**，per ADR-088 §D3.8）

> **本节是 v4.0 文档新增内容**——v3.0 及更早版本缺失此 ABI。该 ABI 是 dGPU DMA 路径访问 UsrLinuxEmu 系统 IOMMU 的唯一通道，**端到端零修改可移植承诺的核心**。

```c
/* CppTLM/include/cpptlm_emulator_callbacks.h（新增 dma_translate 部分）*/

/* DMA 地址转换 callback typedef（v4.0 新增，per ADR-088 §D3.8）
 *
 * 由 UsrLinuxEmu src/system_hw/iommu/ 实现（内部模块）；
 * dGPU 执行 DMA（如 ring buffer fetch / SDMA 传输）时，CppTLM 通过此回调
 * 向系统 IOMMU 请求 IOVA→PA 翻译——对齐真硬件 PCIe DMA remapping 语义：
 * 真实硬件中 dGPU 发出 IOVA 事务，经 PCIe 到达系统 IOMMU（VT-d/AMD-Vi）翻译。
 */
typedef int (*cpptlm_dma_translate_cb_t)(
    uint64_t  iova,         /* IN: dGPU DMA 请求的 IOVA */
    size_t    size,         /* IN: 访问大小 */
    uint64_t* phys          /* OUT: 翻译后的主机物理地址 */
);

/* 注册 DMA 地址转换 callback（v4.0 新增，1 ABI） */
int cpptlm_emulator_register_dma_translate_cb(
    void*                     emu,   /* IN: 设备实例 */
    cpptlm_dma_translate_cb_t cb     /* IN: callback 函数指针 */
);
/* 注：typedef 为参数类型不单独计数 */
```

**错误路径**（与 MSI-X 错误中断协同）：

- CppTLM dGPU 触发 DMA → 调用 `cpptlm_dma_translate_cb` 翻译 IOVA→PA
- IOMMU 域未映射 / IOVA 不在 domain 内 / unmap in-flight → callback 返回**非 0**
- CppTLM 收到非 0：模拟 PCIe DMA abort 上报——沿用 `cpptlm_error_cb_t` 通道（基础 callback 已定义），error_code = `CPPTLM_DMA_TRANSLATION_FAULT`（枚举值在实施 spec 中分配）
- driver 通过 `request_irq(error_vector)` + MSI-X error vector 接收 fault 上报

**多 domain / PASID 边界**：

- 当前 callback 签名 `(iova, size, *phys)` 隐含 **device-level translation**（PASID=0）——真硬件中设备发起 DMA 不带 PASID，经系统 IOMMU 单级翻译
- SVM / per-process PASID 场景留待 v5.5+ 评估（`src/system_hw/iommu/` 的 `attach_dev(dom, dev_id, pasid)` 已预留 PASID 框架，但 callback 签名需届时扩展）
- 当前 SVM 路径依赖 IOMMU fault 注入（per ADR-088 §C4 / §D8），无需 PASID 翻译 callback

**注册时序**（详见 ADR-088 §D4 末尾"DMA translate cb 注册时序保证"）：

- 必须在 `cpptlm_emulator_create_by_id()` 之后、首次 DMA 调用之前注册
- 违反时序：CppTLM 检测 cb == NULL，模拟 PCIe RequesterCompleterAbort
- Gate 2 测试覆盖 cb 注册时机 + 违反时序的 abort 行为

### 4.4 命名约定审查请求

请 CppTLM team 确认：
- 函数命名：`cpptlm_emulator_*` 前缀（vs 现有 `cpptlm_*` 前缀统一）
- typedef 命名：`cpptlm_*_cb_t` 后缀（与 CppTLM 现有 typedef 约定一致？）
- 头文件位置：`CppTLM/include/cpptlm_emulator.h`（公共）vs `CppTLM/src/core/cpptlm_regs_lookup.h`（内部）

---

## 5. 现有 CppTLM 代码变更

### 5.1 `libcpptlm_emulator.so` SHARED 构建（**强制**）

**问题**：现有 `cpptlm_core` 是 STATIC 库，无法被 UsrLinuxEmu 通过 `dlopen()` 加载。

**修改位置**：`CppTLM/src/CMakeLists.txt`

```cmake
# 现状（v4 启动前）
add_library(cpptlm_core STATIC ${CORE_SOURCES})

# v4.1 改造后
add_library(cpptlm_core SHARED ${CORE_SOURCES})
# + 新增 cpptlm_emulator SHARED target
add_library(cpptlm_emulator SHARED
    src/core/cpptlm_regs_lookup.cc       # 新增（C++ lookup API）
    src/core/cpptlm_emulator_api.cc     # 新增（6 forward C ABI 实现）
    src/core/cpptlm_emulator_callbacks.cc # 新增（callback dispatch）
target_link_libraries(cpptlm_emulator PUBLIC cpptlm_core)
set_target_properties(cpptlm_emulator PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    C_VISIBILITY_PRESET hidden
    # 仅导出 cpptlm_emulator_* 符号
)
```

**审查点**（请 CppTLM team 确认）：
- ✅ P1：`cpptlm_core` STATIC → SHARED 改造是否影响现有用户？（需 CppTLM team 评估）
- ✅ P1：visibility hidden 设置是否与 CppTLM 现有约定一致？
- ✅ P1：是否需要保留 STATIC 构建选项以向后兼容？（建议默认 SHARED，文档说明迁移路径）

### 5.2 新增 C++ 文件（v4.1 周四产出）

**`CppTLM/src/core/cpptlm_regs_lookup.cc`** 核心实现：

```cpp
// CppTLM/src/core/cpptlm_regs_lookup.cc
#include "cpptlm_regs_lookup.h"
#include <unordered_map>
#include <mutex>

namespace cpptlm {

// 编译期由 codegen 生成的 register table（constexpr）
extern "C" {
extern const RegisterDef k_register_table_v4_dgpu_v0[];
extern const size_t k_register_table_v4_dgpu_v0_size;
}

// 运行时注册表 cache（per-device）
struct DeviceRegDB {
    std::unordered_map<uint32_t, const RegisterDef*> by_offset;
    std::unordered_map<std::string, const RegisterDef*> by_name;
    char chip_id[64];
};

DeviceRegDB* cpptlm_regs_load(const char* yaml_path, const char* chip_id) {
    // 调用 cpptlm_regs Python 模块（通过 subprocess 或 pybind11）
    // 简化方案：subprocess 调用 `python -m cpptlm_regs build <yaml> --output regdb.h`
    // 后再由 C++ 编译
    // （实施细节由 CppTLM team 决定）
    // ...
    return new DeviceRegDB();
}

const RegisterDef* cpptlm_regs_lookup_by_offset(
    DeviceRegDB* db, uint8_t bar_idx, uint32_t offset, uint8_t width
) {
    auto it = db->by_offset.find(offset);
    if (it == db->by_offset.end()) return nullptr;
    const RegisterDef* reg = it->second;
    if (reg->width != width) return nullptr;  // width mismatch
    return reg;
}

const RegisterDef* cpptlm_regs_lookup_by_name(
    DeviceRegDB* db, const char* name
) {
    auto it = db->by_name.find(name);
    return it != db->by_name.end() ? it->second : nullptr;
}

} // namespace cpptlm
```

**`CppTLM/src/core/cpptlm_emulator_api.cc`** C ABI 实现：

```cpp
// CppTLM/src/core/cpptlm_emulator_api.cc
#include "cpptlm_emulator.h"
#include "cpptlm_regs_lookup.h"

extern "C" {

const char* cpptlm_emulator_get_version(void) {
    return "v1.0-dgpu-v0";  // per ADR-088 §D6.2 Semver 锁定
}

void* cpptlm_emulator_create(const char* regdb_yaml_path, const char* chip_id) {
    if (!regdb_yaml_path || !chip_id) return nullptr;
    
    auto* device = cpptlm::cpptlm_regs_load(regdb_yaml_path, chip_id);
    if (!device) return nullptr;
    
    // 校验 chip_id 一致性（per ADR-088 §D1）
    if (strcmp(device->chip_id, chip_id) != 0) {
        delete device;
        return nullptr;
    }
    return device;
}

int cpptlm_emulator_mmio_read(void* emu, uint8_t bar_idx, uint32_t offset,
                             uint8_t width, uint64_t* val) {
    if (!emu || !val) return -1;
    if (width != 1 && width != 2 && width != 4 && width != 8) return -3;
    
    auto* device = static_cast<cpptlm::DeviceRegDB*>(emu);
    const auto* reg = cpptlm::cpptlm_regs_lookup_by_offset(
        device, bar_idx, offset, width);
    if (!reg) return -2;  // offset 超范围
    
    *val = reg->current_value;
    return 0;
}

int cpptlm_emulator_mmio_write(void* emu, uint8_t bar_idx, uint32_t offset,
                              uint8_t width, uint64_t val) {
    // 类似 mmio_read + 触发 side effects
    // ...
}

int cpptlm_emulator_destroy(void* emu) {
    if (!emu) return -1;
    delete static_cast<cpptlm::DeviceRegDB*>(emu);
    return 0;
}

} // extern "C"
```

---

## 6. 构建系统集成

### 6.1 CppTLM 端构建配置

**修改文件**：`CppTLM/src/CMakeLists.txt`

```cmake
# 新增 cpptlm_regs 子模块（Python）
add_custom_target(cpptlm_regs_build
    COMMAND ${Python3_EXECUTABLE} -m cpptlm_regs build
        --chip dgpu-v0
        --output ${CMAKE_BINARY_DIR}/generated/cpptlm_regs
    DEPENDS ${CMAKE_SOURCE_DIR}/cpptlm_regs/dgpu_v0/*.yaml
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Building cpptlm_regs from YAML to C++ header"
)

# 新增 libcpptlm_emulator.so
add_library(cpptlm_emulator SHARED
    src/core/cpptlm_emulator_api.cc
    src/core/cpptlm_emulator_callbacks.cc
    src/core/cpptlm_regs_lookup.cc
    ${CMAKE_BINARY_DIR}/generated/cpptlm_regs/regdb.h
)
target_link_libraries(cpptlm_emulator PUBLIC cpptlm_core)
add_dependencies(cpptlm_emulator cpptlm_regs_build)
target_include_directories(cpptlm_emulator PUBLIC
    ${CMAKE_BINARY_DIR}/generated/cpptlm_regs
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
```

### 6.2 Python 依赖

```toml
# CppTLM/pyproject.toml（新增）
[tool.poetry.dependencies]
pydantic = "^2.0"  # v4 引入（如 CppTLM 已有则跳过）
pyyaml = "^6.0"
```

**审查点**（请 CppTLM team 确认）：
- ✅ P2：CppTLM 是否已有 Pydantic 依赖？（如无，v4.1 需添加）
- ✅ P2：CppTLM 是否使用 Poetry / pip / setuptools？影响 `pyproject.toml` 配置

---

## 7. 测试要求

### 7.1 单元测试（CppTLM 端）

```python
# CppTLM/cpptlm_regs/tests/test_parser.py
import pytest
from cpptlm_regs.parser.custom_parser import parse_yaml
from cpptlm_regs.schema import RegDB

def test_dgpu_v0_yaml_validity():
    """验证 4 个 YAML 符合 schema 且无错误"""
    db = parse_yaml("cpptlm_regs/dgpu_v0/registers.yaml")
    assert db.chip_id == "dgpu-v0"
    assert len(db.ips) == 11  # 11 个 IP 块
    assert len(db.bars) == 4   # BAR0/1/2/5（建议省略 BAR1）

def test_register_offset_uniqueness():
    """验证寄存器 offset 在 IP 块内不重复"""
    db = parse_yaml("cpptlm_regs/dgpu_v0/registers.yaml")
    for ip in db.ips:
        offsets = [r.offset for r in ip.registers]
        assert len(offsets) == len(set(offsets)), f"重复 offset in {ip.name}"

def test_codegen_output_validity():
    """验证 codegen 生成的 C++ header 格式正确"""
    from cpptlm_regs.codegen import generate_regdb_h
    output = generate_regdb_h(db)
    assert "constexpr RegisterDef" in output
    assert "k_register_table_v4_dgpu_v0" in output
```

### 7.2 集成测试（与 UsrLinuxEmu 协作）

**位置**：`CppTLM/tests/integration/test_cpptlm_emulator_api.cc`（新增）

```cpp
// CppTLM/tests/integration/test_cpptlm_emulator_api.cc
#include <gtest/gtest.h>
#include "cpptlm_emulator.h"

class CpptlmEmulatorApiTest : public ::testing::Test {
protected:
    void SetUp() override {
        emu_ = cpptlm_emulator_create(
            "cpptlm_regs/dgpu_v0/registers.yaml", "dgpu-v0");
    }
    void TearDown() override {
        cpptlm_emulator_destroy(emu_);
    }
    void* emu_;
};

TEST_F(CpptlmEmulatorApiTest, VersionReturnsV1) {
    const char* ver = cpptlm_emulator_get_version();
    ASSERT_STREQ(ver, "v1.0-dgpu-v0");  // per ADR-088 §D6.2
}

TEST_F(CpptlmEmulatorApiTest, MmioReadChipId) {
    uint64_t val = 0;
    int rc = cpptlm_emulator_mmio_read(emu_, 0, 0x0000, 4, &val);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(val, 0xDEADBEEF);  // per registers.yaml
}

TEST_F(CpptlmEmulatorApiTest, MmioReadReturnsErrorOnOutOfRange) {
    uint64_t val = 0;
    int rc = cpptlm_emulator_mmio_read(emu_, 0, 0xFFFF, 4, &val);
    EXPECT_EQ(rc, -2);  // offset 超范围
}

// ... 更多测试覆盖 4 callback + lookup_register
```

### 7.3 验收门槛

**CppTLM team 必须满足**：
- ✅ `cpptlm_regs` Python 模块 100% 单元测试通过
- ✅ `libcpptlm_emulator.so` 编译产出，`nm -D` 显示 **19 个 cpptlm_emulator_* 符号**（per ADR-088 §D5 23 ABI = 基础 6 forward + 4 callback typedef + 1 register（callbacks）+ 1 register（backdoor）+ 1 register（DMA translate）+ 8 板卡扩展 forward + 3 MSI-X forward = 19 forward/register；**4 callback typedef 是 C 类型定义不导出符号；详见附录 A.6**）
- ✅ `cpptlm_emulator_get_version()` 返回 `"v1.0-dgpu-v0"`（per ADR-088 §D6.2 Semver 锁定）
- ✅ `cpptlm_regs/dgpu_v0/registers.yaml` 解析生成 45 个 register
- ✅ DMA translate callback 注册路径完整：注册后 dGPU DMA 调用 → IOMMU callback → IOVA→PA 翻译
- ✅ MSI-X 硬件仿真完整：pending bitmap + interrupt dispatch + clear
- ✅ PCIe Config Space 256B + capability chain 完整（PM/MSI-X/PCIe/Vendor Specific）
- ✅ 多板卡支持：`get_device_count()` 返回正确数值
- ✅ backdoor 性能 ≥ MMIO 路径的 2x（避免 transaction 开销）
- ✅ 集成测试（与 UsrLinuxEmu 协作 Gate 1+2a 阶段）通过

---

## 8. 协作流程

### 8.1 Review Checkpoints

| 检查点 | 时间 | 内容 |
|--------|------|------|
| **CP1（架构 review）** | v4.1 周一启动前 | 本 spec 文档完整 review + 问题汇总 |
| **CP2（接口 review）** | v4.1 周二结束 | 6 ABI + 4 callback + 1 register 接口签名确认 |
| **CP3（构建 review）** | v4.1 周三结束 | `libcpptlm_emulator.so` 编译验证 |
| **CP4（集成 review）** | v4.1 周五结束 | 与 UsrLinuxEmu team v4.2 集成测试 |

### 8.2 Review 反馈渠道

- **GitHub**: 在 [UsrLinuxEmu 仓 issue #88-cpptlm-team-handoff](https://github.com/chisuhua/UsrLinuxEmu/issues) 下回复
- **Email**: architecture@usrlxinuxemu.example
- **Cross-team sync**: 每周四 14:00 UTC（per ADR-035 §R5.1）

### 8.3 关键决策点（请 CppTLM team 在 CP1 前回复）

| 决策点 | CppTLM team 选项 |
|--------|-------------------|
| **D1：libcpptlm_emulator.so 命名** | (a) 沿用 `libcpptlm_emulator.so`（推荐）/ (b) 改为 `libcpptlm_v4.so` |
| **D2：cpptlm_regs 子模块位置** | (a) `CppTLM/cpptlm_regs/`（顶层目录）/ (b) `CppTLM/src/python/cpptlm_regs/` |
| **D3：Pydantic 依赖引入** | (a) v4.1 引入 Pydantic v2 / (b) 用 dataclasses 替代（无新依赖）|
| **D4：codegen 输出格式** | (a) C++ `constexpr RegisterDef[]`（推荐）/ (b) JSON 文件（运行时解析）|
| **D5：cpptlm_regs_load 实现** | (a) subprocess 调用 Python（简单）/ (b) pybind11 嵌入（性能更好）|

---

## 9. 接口示例（端到端使用）

### 9.1 UsrLinuxEmu 端使用（hal_user.cpp）

```cpp
// UsrLinuxEmu/plugins/gpu_driver/hal/hal_user.cpp
#include <dlfcn.h>
#include "cpptlm_emulator.h"

class CpptlmEmulator {
    void* handle;
    void* emu;
    // 函数指针
    const char* (*get_version)();
    void* (*create)(const char*, const char*);
    int (*mmio_read)(void*, uint8_t, uint32_t, uint8_t, uint64_t*);
    int (*mmio_write)(void*, uint8_t, uint32_t, uint8_t, uint64_t);
    int (*lookup_register)(void*, const char*, reg_info_t*);
    int (*destroy)(void*);
    int (*register_callbacks)(void*, ...);
public:
    CpptlmEmulator(const char* yaml, const char* chip) {
        handle = dlopen("libcpptlm_emulator.so", RTLD_NOW);
        get_version = dlsym(handle, "cpptlm_emulator_get_version");
        create = dlsym(handle, "cpptlm_emulator_create");
        // ... dlsym 其他
        emu = create(yaml, chip);
    }
    ~CpptlmEmulator() {
        destroy(emu);
        dlclose(handle);
    }
    int read(uint8_t bar, uint32_t off, uint8_t width, uint64_t* val) {
        return mmio_read(emu, bar, off, width, val);
    }
};

// v4 关键路径：driver 写 MMIO → HAL 拦截 → CpptlmEmulator.write()
int hal_user_cp_doorbell_write(void* ctx, uint32_t queue_id) {
    auto* hal = static_cast<hal_user_context*>(ctx);
    uint64_t val = (1ULL << 32) | queue_id;
    return hal->cpptlm_emulator->read(0, 0x1010, 4, &val);  // CP_DOORBELL offset 0x1010
}
```

### 9.2 端到端测试场景

```bash
# 1. CppTLM team 构建 libcpptlm_emulator.so
cd /workspace/project/CppTLM
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make cpptlm_emulator -j4
ls -la libcpptlm_emulator.so  # 应产出

# 2. 验证 23 ABI 导出符号（per ADR-088 §D5）
nm -D libcpptlm_emulator.so | grep cpptlm_emulator_ | wc -l
# 预期：19（= 23 ABI - 4 callback typedef；typedef 不导出为符号）
# 预期符号列表：
# 基础 6 forward：
#   cpptlm_emulator_create, cpptlm_emulator_destroy, cpptlm_emulator_get_version,
#   cpptlm_emulator_lookup_register, cpptlm_emulator_mmio_read, cpptlm_emulator_mmio_write
# 板卡扩展 8：
#   cpptlm_emulator_get_device_count, cpptlm_emulator_get_device_info,
#   cpptlm_emulator_create_by_id, cpptlm_emulator_pcie_config_read,
#   cpptlm_emulator_pcie_config_write, cpptlm_emulator_backdoor_read,
#   cpptlm_emulator_backdoor_write, cpptlm_emulator_register_backdoor_cb
# MSI-X 3：
#   cpptlm_emulator_msix_init, cpptlm_emulator_msix_update_pending,
#   cpptlm_emulator_msix_clear_pending
# Register 1：
#   cpptlm_emulator_register_callbacks
# DMA translate 1：
#   cpptlm_emulator_register_dma_translate_cb
# （4 callback typedef 是类型定义，不导出符号）

# 3. 验证版本字符串（per ADR-088 §D6.2）
strings libcpptlm_emulator.so | grep "v1.0-dgpu-v0"
# 预期：v1.0-dgpu-v0

# 4. UsrLinuxEmu 端集成测试
cd /workspace/project/UsrLinuxEmu
USR_LINUX_EMU_USE_CPPTLM=1 \
USR_LINUX_EMU_CPPTLM_REGDB=/workspace/project/CppTLM/cpptlm_regs/dgpu_v0/registers.yaml \
./build/bin/test_gpu_cpptlm_smoke  # CppTLM 集成测试（Gate 1+2a 阶段添加）
```

---

## 10. Review 检查清单（CppTLM team）

请在 CP1（v4.1 周一启动前）完成 review 并回复：

### 10.1 架构层面
- [ ] `cpptlm_regs/` 子模块位置是否符合 CppTLM Python 模块约定？
- [ ] `libcpptlm_emulator.so` SHARED 构建是否影响其他下游用户？
- [ ] `cpptlm_core` STATIC → SHARED 改造的兼容性影响范围？

### 10.2 接口层面
- [ ] 6 forward ABI 函数命名是否清晰（`cpptlm_emulator_*` 前缀）？
- [ ] 4 callback typedef 命名是否符合 CppTLM 约定？
- [ ] `reg_info_t` 结构体字段是否完整（offset/access/width/reset）？
- [ ] 错误码约定（-1/-2/-3/-4）是否需要调整？

### 10.3 构建层面
- [ ] CMake 配置（5.1 节）是否与 CppTLM 现有约定一致？
- [ ] Pydantic 依赖引入（6.2 节）是否可行？
- [ ] visibility hidden 设置（5.1 节）是否正确？

### 10.4 测试层面
- [ ] 单元测试覆盖（7.1 节）是否充分？
- [ ] 集成测试位置（7.2 节）是否合适？
- [ ] 验收门槛（7.3 节）是否合理？

### 10.5 协作层面
- [ ] Review checkpoints（8.1 节）时间是否合理？
- [ ] 反馈渠道（8.2 节）是否方便？
- [ ] 关键决策点 D1-D5（8.3 节）是否有其他选项？

---

## 11. 时间线与里程碑（**per ADR-088 §D2**）

```
2026-08-16 (今天，v4.0 spec 发布)
    ↓
[CP1] 阶段 1 启动前 — CppTLM team review 本 spec + 提交问题
    ↓
2026-08-23 (预计) 阶段 1 启动（CppTLM team 主导）
    ├─ Week 1: cpptlm_pcie_device 基础 + Config Space 256B + capability chain
    ├─ Week 2: 多板卡支持 + get_device_count/info
    ├─ Week 3: cpptlm_pcie_host + create_by_id + pcie_config_read/write
    ├─ Week 4: backdoor 实现（参考 gem5 MemBackdoor）
    ├─ [Gate 1] 阶段 1 完成：8 ABI 新增 + 98 Catch2 测试 mode B 全 PASS
    ↓
2026-09-20 (预计) 阶段 1 交付
    ↓
2026-09-21 (预计) 阶段 2a 启动（CppTLM team 主导；与 UsrLinuxEmu 阶段 2b 并行）
    ├─ Week 5-6: cpptlm_msix_table（pending bitmap + vector table + PBA）
    ├─ Week 6: cpptlm_emulator_register_dma_translate_cb 注册路径
    ├─ [Gate 2] 阶段 2a+2b 完成：4 ABI 新增（MSI-X 3 + DMA translate 1）+ 23 ABI 冻结
    ↓
2026-10-11 (预计) CppTLM 23 ABI 冻结
    ↓
[UsrLinuxEmu 后续阶段]
    ├─ 阶段 3：CXL.mem sim（UsrLinuxEmu 内部模块，2-3 周，CppTLM 不涉及）
    ├─ 阶段 4：linux_compat/ 重构 + HAL 改造 + 总集成（8-10 周，CppTLM 仅参与集成测试）
    └─ 阶段 5：文档同步（1 周）
    ↓
[Gate 4.5] 真实 amdgpu driver 端到端零修改验证（per ADR-088 §D7.1 5 个必测函数）
```

**总工期**：**约 24-32 周**（per ADR-088 §D2）

- **CppTLM 范围**：5-7 周（阶段 1 + 阶段 2a；**仅 dGPU 板卡 23 ABI**）
- **UsrLinuxEmu 范围**：13-17 周（阶段 2b + 3 + 4 + 5；系统 IOMMU + CXL.mem + linux_compat/ 重构）
- **关键路径算式**（per ADR-088 §D2 完整算式）：
  - max(CppTLM 5-7 周 ∥ UsrLinuxEmu 13-17 周) + 阶段 4 集成 5-7 周 + 文档 1 周 + buffer 1 周 + Gate 累计验证 4-6 周
  - = 13-17 + 5-7 + 1 + 1 + 4-6 = **24-32 周**（下限 24 = 13 + 5 + 1 + 1 + 4；上限 32 = 17 + 7 + 1 + 1 + 6）

---

## 12. 参考资料

### 12.1 上游文档

| 文档 | 用途 |
|------|------|
| [ADR-088](../00_adr/adr-088-dgpu-complete-simulation.md) | 主 ADR（✅ Accepted）|
| [Oracle 调研报告](../architecture/oracle-cpptlm-regdb-research-2026-08-15.md) | CppTLM 能力边界 + 寄存器数据源调研 |
| [ADR-035 治理规则](../00_adr/adr-035-governance-policy.md) | 跨仓协作流程（§R5）|

### 12.2 CppTLM 现有文档

| 文档 | 用途 |
|------|------|
| `CppTLM/cpptlm_config/AGENTS.md` | 旧 config 系统（DEPRECATED 参考）|
| `CppTLM/cpptlm/AGENTS.md` | 现有 topo + library + emitter 全景 |
| `CppTLM/cpptlm/topo/emitter.py` | JSON schema 参考 |
| `CppTLM/src/core/module_factory.cc` | C++ JSON 加载器参考（37KB）|

### 12.3 Oracle 评审（v4 二次评审）

| Session ID | 用途 |
|------------|------|
| `ses_ffc383227ffe14gK5xMbxGOo3k` | Oracle 调研（CppTLM + 寄存器源）|
| `ses_ffc20fad3ffe4XSCbxcO2sIi2j` | Oracle 二次评审（APPROVED-WITH-CONDITIONS）|

---

## 13. CppTLM team 反馈模板

请 CppTLM team 在 CP1 后用以下模板回复：

```markdown
## CppTLM team Review Feedback

**Reviewer**: [CppTLM maintainer 姓名]
**Date**: YYYY-MM-DD
**Checkpoint**: CP1 / CP2 / CP3 / CP4（请标注）

### 1. 架构层面反馈
- [ ] 同意 / [ ] 不同意 + 说明
- ...

### 2. 接口层面反馈
- [ ] 同意 / [ ] 不同意 + 说明
- ...

### 3. 构建层面反馈
- [ ] 同意 / [ ] 不同意 + 说明
- ...

### 4. 测试层面反馈
- [ ] 同意 / [ ] 不同意 + 说明
- ...

### 5. 关键决策点回复
- D1（libcpptlm_emulator.so 命名）：[选项]
- D2（cpptlm_regs 子模块位置）：[选项]
- D3（Pydantic 依赖引入）：[选项]
- D4（codegen 输出格式）：[选项]
- D5（cpptlm_regs_load 实现）：[选项]

### 6. 其他问题
- ...

**Signature**: [CppTLM maintainer]
```

---

**本文档结束**。请 CppTLM team 在 v4.1 周一启动前完成 review 并提交反馈。UsrLinuxEmu Architecture Team 会在 24 小时内回复确认。如需跨团队会议协调，请通过 [GitHub issue #88-cpptlm-team-handoff](https://github.com/chisuhua/UsrLinuxEmu/issues) 预约。

**最后更新**: 2026-08-16（**v4.0 全面重写**：删除 v3.0 附录 A.3 + A.4 + A.6 中 IOMMU/CXL 内容；新增 §4.3 DMA translate callback ABI；附录 A 按 ADR-088 §D5 重排为 23 ABI（16 forward + 4 callback typedef + 1 register callbacks + 1 register backdoor + 1 register DMA translate）；版本字符串修正为 `"v1.0-dgpu-v0"`；时间估算修正为 CppTLM 5-7 周（per ADR-088 §D2））
**v4.0 关联 Oracle session ID**: 不适用（v4.0 是文档修订，非新评审）

---

## 附录 A：23 ABI 完整清单（**per ADR-088 §D5，唯一权威**）

> **v4.0 全面重写说明**：本附录严格按 [ADR-088 §D5](../00_adr/adr-088-dgpu-complete-simulation.md) 的 23 ABI 编排。v3.0 附录描述的 31 ABI（v4.6 IOMMU 5 + v4.7 CXL 4）已**完全删除**——这些 ABI 现由 UsrLinuxEmu `src/system_hw/iommu/` + `src/system_hw/cxl_memdev/` 内部模块实现（**不在 CppTLM 交付范围**）。
>
> **本附录结构**：
> - A.1 基础 6 forward ABI
> - A.2 4 callback typedef + 1 register ABI（共 5 项，typedef 不导出符号）
> - A.3 板卡扩展 8 ABI（多板卡 2 + create_by_id 1 + PCIe Config 2 + backdoor 3）
> - A.4 MSI-X 3 ABI
> - A.5 DMA translate 1 ABI（v4.0 新增）
> - A.6 总计：**16 forward + 4 callback typedef + 1 register + 2 callback 注册函数 = 23 ABI**
> - A.7 CppTLM 新增模块（**仅 3 个**，不含 IOMMU/CXL）
> - A.8 23 ABI vs 旧 31 ABI 对比
> - A.9 关键决策点（v4.0 重新设计）

### A.1 基础 6 forward ABI（`libcpptlm_emulator.so` 导出）

| # | 类别 | ABI | 用途 |
|---|------|-----|------|
| 1 | 基础 | `cpptlm_emulator_get_version` | 版本握手（返回 `"v1.0-dgpu-v0"`）|
| 2 | 基础 | `cpptlm_emulator_create` | 创建设备实例（by yaml）|
| 3 | 基础 | `cpptlm_emulator_mmio_read` | BAR MMIO 读 |
| 4 | 基础 | `cpptlm_emulator_mmio_write` | BAR MMIO 写 |
| 5 | 基础 | `cpptlm_emulator_lookup_register` | 寄存器元数据查询 |
| 6 | 基础 | `cpptlm_emulator_destroy` | 销毁实例 |

### A.2 基础 callback + register（4 typedef + 1 register，共 5 项）

| # | 类别 | ABI / typedef | 用途 |
|---|------|---------------|------|
| 7 | 基础 typedef | `cpptlm_intr_deliver_cb_t` | MSI-X 中断传递 callback 类型 |
| 8 | 基础 typedef | `cpptlm_error_cb_t` | 错误事件 callback 类型 |
| 9 | 基础 typedef | `cpptlm_reset_complete_cb_t` | FLR 完成 callback 类型 |
| 10 | 基础 typedef | `cpptlm_power_cb_t` | D0/D3 切换 callback 类型 |
| 11 | 基础 register | `cpptlm_emulator_register_callbacks` | 一次性绑定 4 callback |

### A.3 板卡扩展 8 ABI（多板卡 + PCIe Config + backdoor）

| # | 子类 | ABI | 用途 |
|---|------|-----|------|
| 12 | 多板卡 | `cpptlm_emulator_get_device_count` | 板卡枚举总数 |
| 13 | 多板卡 | `cpptlm_emulator_get_device_info` | 板卡基本信息（PCI ID / VRAM / BAR size / MSI-X vectors）|
| 14 | 多板卡 | `cpptlm_emulator_create_by_id` | 按 device id 创建实例 |
| 15 | PCIe Config | `cpptlm_emulator_pcie_config_read` | PCIe Config Space 读（offset 0x00-0xFFF）|
| 16 | PCIe Config | `cpptlm_emulator_pcie_config_write` | PCIe Config Space 写 |
| 17 | backdoor | `cpptlm_emulator_backdoor_read` | backdoor 调试读（参考 gem5 MemBackdoor）|
| 18 | backdoor | `cpptlm_emulator_backdoor_write` | backdoor 调试写 |
| 19 | backdoor | `cpptlm_emulator_register_backdoor_cb` | 注册 backdoor 反向通知 callback（参数 `cpptlm_backdoor_invalidation_cb_t`）|

### A.4 MSI-X 硬件仿真 3 ABI

| # | ABI | 用途 |
|---|-----|------|
| 20 | `cpptlm_emulator_msix_init` | MSI-X 初始化（table_size / mask）|
| 21 | `cpptlm_emulator_msix_update_pending` | MSI-X pending 更新（触发中断）|
| 22 | `cpptlm_emulator_msix_clear_pending` | MSI-X pending 清除 |

### A.5 DMA 地址转换 callback 1 ABI（**v4.0 新增，per ADR-088 §D3.8**）

| # | ABI | 用途 |
|---|-----|------|
| 23 | `cpptlm_emulator_register_dma_translate_cb` | 注册 DMA 地址转换 callback（参数 `cpptlm_dma_translate_cb_t`）——dGPU DMA 时回调 UsrLinuxEmu `src/system_hw/iommu/` 翻译 IOVA→PA |

### A.6 总计：23 ABI 编排

| 类别 | 数量 | 累计 | 备注 |
|------|----:|----:|------|
| 基础 forward | 6 | 6 | A.1 |
| 基础 callback typedef | 4 | 10 | A.2（typedef 不导出符号）|
| 基础 register | 1 | 11 | A.2 |
| 板卡扩展 forward | 7 | 18 | A.3（get_device_count / info / create_by_id / pcie_config_read / write / backdoor_read / write）|
| 板卡扩展 register | 1 | 19 | A.3（register_backdoor_cb）|
| MSI-X forward | 3 | 22 | A.4 |
| DMA translate register | 1 | 23 | A.5 |
| **总计** | — | **23** | 16 forward + 4 callback typedef + 1 register（callbacks）+ 1 register（backdoor）+ 1 register（DMA translate）= 23 |

**导出符号数计算**：`nm -D libcpptlm_emulator.so | grep cpptlm_emulator_ | wc -l` 应返回 **19**（= 23 ABI - 4 callback typedef；typedef 是 C 类型定义，不导出为运行时符号）

### A.7 CppTLM 新增模块（**仅 3 个**，不含 IOMMU/CXL）

| 模块 | 实施阶段 | 工作量 | 用途 |
|------|---------|------:|------|
| `cpptlm_regs/` | 阶段 1 准备 | 2 周 | Pydantic schema + parser + codegen + dgpu_v0/4 YAML |
| `cpptlm_pcie_device` | 阶段 1 | 3-4 周（包含 cpptlm_pcie_host） | PCIe Config Space 256B + 5 capability + 多板卡 + 总线枚举 |
| `cpptlm_msix_table` | 阶段 2a | 1-2 周 | MSI-X pending bitmap + interrupt vector table + PBA 表 |

**已移出 CppTLM 范围**（v3.0 → v4.0 修订）：
- ❌ `cpptlm_iommu_domain`（IOMMU 域操作 + page table walker）→ 移至 UsrLinuxEmu `src/system_hw/iommu/`
- ❌ `cpptlm_cxl_memdev`（CXL.mem 设备仿真）→ 移至 UsrLinuxEmu `src/system_hw/cxl_memdev/`

### A.8 23 ABI vs 旧版对比

| 维度 | v3.0 错误声称 | v4.0 实际 | 差异 |
|------|---------------|-----------|------|
| **ABI 总数** | 31 | **23** | **-8**（删除 IOMMU 5 + CXL 4 - 新增 DMA translate 1）|
| **CppTLM 工作量** | 20-27 周 | **5-7 周** | **-15 周**（IOMMU + CXL 实施工作已转 UsrLinuxEmu）|
| **CppTLM 模块数** | 5 | **3** | -2（删除 iommu_domain + cxl_memdev）|
| **IOMMU 归属** | CppTLM | UsrLinuxEmu `src/system_hw/` | 拓扑对齐真硬件（IOMMU 为系统级组件）|
| **CXL 归属** | CppTLM | UsrLinuxEmu `src/system_hw/` | 拓扑对齐真硬件（CXL.mem 为独立 PCIe 设备）|
| **DMA translate** | ❌ 缺失 | ✅ **新增** | dGPU DMA → 系统 IOMMU 回调 |
| **仿真层级** | L3 + PCIe/IOMMU/CXL | L3 + dGPU 子系统（PCIe Config + MSI-X + backdoor + DMA cb）| 严格按 ADR-088 §C2 拓扑 |
| **零修改边界** | "真硬件端到端"（声称） | **真硬件端到端**（实际）| 拓扑对齐才能真正实现 |
| **多板卡支持** | ✅ v4.5 5 ABI | ✅ 板卡扩展 2 ABI（get_count + get_info）+ create_by_id | 同 |
| **backdoor 调试** | ✅ gem5 MemBackdoor | ✅ gem5 MemBackdoor + register_backdoor_cb | 同 |
| **MSI-X 仿真** | ✅ v4.6 3 ABI | ✅ MSI-X 3 ABI | 同 |

### A.9 CppTLM 团队 5 个关键决策点（v4.0 重新设计）

| 决策点 | 选项 |
|--------|------|
| **D1 v4.0 工作量承担** | (a) 完整 5-7 周（阶段 1 + 2a，推荐）/ (b) 仅阶段 1（3-4 周）/ (c) 协商折中 |
| **D2 新增 CppTLM 模块** | (a) 全部 3 个模块（regs / pcie_device / msix_table，推荐）/ (b) 仅阶段 1 必需 |
| **D3 backdoor 接口** | (a) 遵循 gem5 MemBackdoor 模式 + register_backdoor_cb 反向通知（推荐）/ (b) CppTLM 自定义 |
| **D4 23 ABI 命名** | (a) 保持 `cpptlm_emulator_*` 前缀 + 后缀语义（推荐）/ (b) CppTLM 风格调整 |
| **D5 分阶段实施** | (a) 阶段 1（3-4 周）→ 阶段 2a（2-3 周）顺序（推荐）/ (b) 一次性 / (c) 并行（与 UsrLinuxEmu 阶段 2b 并行）|

**反馈模板见 §13 末尾**（已包含 v4.0 决策点）。
**UsrLinuxEmu Architecture Team**: [architecture@usrlxinuxemu.example](mailto:architecture@usrlxinuxemu.example)