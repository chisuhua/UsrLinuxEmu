# ADR-052: AQL / PM4 Packet Native Support

**状态**: 📋 PROPOSED（Phase 6）
**日期**: 2026-07-09
**提案人**: Sisyphus（GPU CP 蓝图完整性填充）
**关联 ADR**: ADR-042 (Pushbuffer Method Encoding — UsrNative format), ADR-017 (GPFIFO/Queue — AQL reference)
**关联 Change**: 无（Phase 6 规划）

---

## Context

ADR-042 定义了 UsrNative 编码（UsrLinuxEmu 自定义简化 pushbuffer 格式）。当 TaskRunner 走 ROCm/HIP 路径时，需要与真实 AQL 64 字节标准包兼容；走 CUDA 路径时可能与 NVIDIA PM4 method 格式对齐。

- **AMD AQL**（HSA 1.2 规范）：64 字节定长 packet，类型包括 `KERNEL_DISPATCH`、`BARRIER_AND`、`BARRIER_OR`、`VENDOR_SPECIFIC`。包含 `completion_signal` handle、`kernel_object`、`kernarg_address` 字段。
- **NVIDIA PM4**（GF100+ format）：32-bit word packing，`method_addr` + `subchannel` + `NI/INC` + variable `data_count`。

### 与 ADR-042 的关系

本 ADR 不替代 ADR-042。两者通过 `gpu_gpfifo_entry.format` 字段共存：

| format 值 | 编码 | 用途 |
|-----------|------|------|
| 0 (default) | UsrNative（ADR-042） | 测试简化路径 |
| 1 | AQL（本 ADR） | ROCm/HIP 兼容 |
| 2 | PM4（本 ADR） | CUDA 对齐 |

UsrNative 保持为默认格式，AQL/PM4 按需启用。

---

## Decision

### D1: gpu_gpfifo_entry 新增 format 字段

```c
typedef struct {
    uint8_t  format;       // 新增：0=UsrNative, 1=AQL, 2=PM4
    uint32_t method;       // 现有
    uint64_t payload[8];   // 现有（AQL 模式下 payload 按 hsa_kernel_dispatch_packet_t 解析）
    // ... 其他字段
} gpu_gpfifo_entry;
```

### D2: AQL packet 解析

`GpfifoToLaunchParamsTranslator` 新增 `format == FORMAT_AQL` 分支：

```cpp
if (entry.format == FORMAT_AQL) {
    auto *pkt = reinterpret_cast<const hsa_kernel_dispatch_packet_t*>(entry.payload);
    LaunchParams params;
    params.kernel_addr = pkt->kernel_object;
    params.kernargs = reinterpret_cast<uint64_t>(pkt->kernarg_address);
    params.grid_x = pkt->grid_size_x;
    params.block_x = pkt->workgroup_size_x;
    // ...
    // completion_signal 映射到 sim_timeline_semaphore（ADR-049）
}
```

### D3: PM4 解析（Phase 6.5，deferred）

PM4 解析更为复杂（method 地址空间编码、NI/INC 控制、subchannel 路由），Phase 6 先实现 AQL，PM4 延后至 Phase 6.5。

### D4: format 选择

`gpu_gpfifo_entry.format` 由 drv handler 在构建 entry 时设置。AQL mode 通过 ioctl flag 或 build mode 选择（`-DUSE_AQL_PACKETS=ON`）。

---

## Consequences

- ✅ ROCm/HIP 路径可用——AQL 64 字节标准包兼容
- ✅ UsrNative 不被废弃——通过 format 字段共存
- ✅ format 字段仅 1 byte，对现有结构体影响极小
- ⚠️ PM4 延后至 Phase 6.5，TaskRunner CUDA 路径在 PM4 完成前只能用 UsrNative
- ⚠️ AQL `completion_signal` 需要 ADR-049 的 timeline semaphore 支持

### Phase 6 触发条件

- ADR-049 (timeline semaphore) ✅ Accepted
- TaskRunner 确认需要 ROCm/HIP 路径（AQL）