# sim-hardware-foundation-tier1-tier2: 详细设计（mock-first）

> **版本**: v0.4 (2026-09-03, Metis 复审 A-01~A-14 + G-01~G-10 修复版)
> **状态**: 🔄 Proposed v0.4（待 Oracle + Metis 双审查）
> **关联**: [proposal.md](./proposal.md) v0.4 + [tasks.md](./tasks.md) v0.4 + [specs/spec.md](./specs/spec.md) v0.4
> **关联 ADR**: [ADR-091](../00_adr/adr-091-pci-driver-architecture-and-four-quadrant.md) ✅ Accepted v0.2
> **关联文档**: [4 象限架构文档](../02_architecture/four-quadrant-architecture.md) v0.2 + [CppTLM v4 实施 Handoff](../05-advanced/cpptlm-v4-implementation-handoff.md) v4.0 Draft（未验证/未合并）

> **核心定位（v0.4）**：本 Change 是 **mock-first Q3 PCIe 基础**。真实 CppTLM 23 ABI 桥接为 **gated follow-up**，本 Change 不实施、不验证、不虚构任何真实 CppTLM 调用。

---

## §1 范围与现状

### §1.1 仓库现状基线（与 proposal.md §Current Facts F1-F9 完全一致；任何冲突以 proposal 为准）

- F1：本仓库无任何 CppTLM 头/库/符号/submodule；`external/` 仅含 `json/nlohmann/json.hpp`
- F2：CppTLM 23 ABI（ADR-088 §D5 / cpptlm-v4-handoff 附录 A）仍为 **Draft for CppTLM Review**，无 maintainer ack / release tag / 本地构建
- F3：sim_hardware 全部 7 个实现文件为 Wave 1C 占位（`-ENOSYS` / no-op / 不解析 JSON）
- F4：`sim_hardware/CMakeLists.txt` 为 `add_library(sim_hardware INTERFACE)`，cpptlm_core 链接行已注释
- F5：`sim_hardware/topology/default_topology.json` 是**截断的非法 JSON**（末尾 `"prefetcha` 被截断）
- F6：当前真实 PCI 文件 `plugins/pci_driver/probe.cpp`；`pci_probe.cpp` 不存在，`pci_setup_bus.cpp` / `pcie_enable_device.cpp` 不存在
- F7：Change-1 已归档：`openspec/changes/archive/2026-09-03-2026-09-03-pci-driver-refactor/`
- F8：当前 ctest 基线 151/151 PASS（Change-1 归档记录）
- F9：本 Change 是计划文档，不修改 sim_hardware / plugins / tests / src / docs 应用代码

### §1.2 文档意图（v0.4 vs v0.3 的关键差异）

| v0.3 假设 | v0.4 实际 |
|----------|----------|
| `cpptlm_emulator.h` C ABI 已可用 | ❌ 仓库内不存在 |
| `libcpptlm_core.so` 可链 | ❌ 文档草案，未验证 |
| 真实 `PcieEndpointIP` 17-port 可构造 | ❌ 仅有 4 个 opaque handle；真实 API 未验证 |
| `pci_probe.cpp` 存在 | ❌ 实际是 `probe.cpp`（设备模型） |
| M2 单一里程碑 | ✅ M2a/M2b/M2c 拆分（全部在 mock 达成）|
| 32 tasks / 6-8 周 / 3,100-3,500 LOC | ✅ 28 tasks / 4-6 周（Phase 0 通过后）/ ~1,250-1,500 LOC（mock-first）|

---

## §2 Phase 0 Hard Gate（首个硬性验收 Gate）

| Gate | 检查项 | 通过标准 |
|------|--------|---------|
| **P0-G1** | sim_hardware target 是 INTERFACE，无 cpptlm 链接 | `sim_hardware/CMakeLists.txt` 含 `add_library(sim_hardware INTERFACE)`；`target_link_libraries(sim_hardware INTERFACE cpptlm_core)` 被注释或不存在 |
| **P0-G2** | 仓库无 CppTLM 头/库/符号 | `find . -name '*cpptlm*' -not -path './sim_hardware/*' -not -path './openspec/*'` 仅命中 ADR/handoff 文档 |
| **P0-G3** | default_topology.json 当前非法 | `python3 -c 'import json; json.load(open("sim_hardware/topology/default_topology.json"))'` 抛 `JSONDecodeError` |
| **P0-G4** | 真实 PCI 文件为 `probe.cpp`，非 `pci_probe.cpp` | `ls plugins/pci_driver/` 含 `probe.cpp`，不含 `pci_probe.cpp` / `pci_setup_bus.cpp` / `pcie_enable_device.cpp` |
| **P0-G5** | 本 Change 仅改 artifacts 4 文件 | `git diff --stat` 仅含 `openspec/changes/2026-09-03-sim-hardware-foundation-tier1-tier2/` 下 4 个文件 |

**解锁条件**：P0-G1~G5 全部通过 → 记录到 T0.1 验证产物 → 进入 Phase 1。

---

## §3 CppTLM 状态与 Gate

| 项 | 声明 |
|----|------|
| 仓库内 CppTLM | ❌ 不存在（F1）；任何 `cpptlm_emulator_*` 调用均为虚构 |
| CppTLM 23 ABI | ❌ 未验证（F2：Draft，无 ack/release/local build）|
| 本 Change 是否实现真实 CppTLM 桥接 | ❌ **不实现**。真实 CppTLM = gated follow-up |
| mock backend 是否视为真实 CppTLM 验证 | ❌ **不视为**。mock 仅验证本仓库契约（API 签名/错误码/状态机/topology 解析）|
| M2a/M2b/M2c 与真实 CppTLM 关系 | ✅ M2a/M2b/M2c **全部在 mock backend 上达成**；真实 CppTLM 端到端属 follow-up |
| 对下游（ADR-088/090）影响 | ✅ **无阻碍**。本 Change API = 下游真实 CppTLM 适配面 |

---

## §4 Target 架构决策

### §4.1 推荐：`sim_hardware` (INTERFACE) + `sim_hardware_mock` (STATIC)

```text
sim_hardware                  INTERFACE   (API 契约；header-only)
sim_hardware_mock             STATIC      (mock backend 实现；本 Change 实施)
sim_hardware_cpptlm           optional gated target (follow-up；本 Change 不创建)
```

### §4.2 理由

- INTERFACE 保证 "API 契约先行 + 零二进制耦合"，避免单一库吃掉实现细节
- mock STATIC 保证 "无外部依赖可独立测试"
- 真实 backend 以同 API 追加（`sim_hardware_cpptlm`），不破坏契约
- 替代方案（直接把 sim_hardware 转 STATIC）会混淆 API 边界 / mock 实现 / backend 选择

### §4.3 链接方式

```cmake
# 消费方（pci_driver 等）
target_link_libraries(pci_driver_plugin PRIVATE sim_hardware sim_hardware_mock)
```

### §4.4 Q3 独立性约束

- sim_hardware_mock 不可 `#include` `kernel/*` 或 `linux_compat/*`
- sim_hardware_mock 不可调用 `plugins/pci_driver/` 或 `plugins/iommu_driver/` 任何符号
- sim_hardware_mock 可调用 `external/json/nlohmann/json.hpp`（header-only）

### §4.5 静态状态规则（Q3 单例）

- 一个进程内仅可加载一个 `sim_hardware_*` 实现 target
- 测试与 plugin 不可同时加载两个实例
- `bypass_get_mode()` / `topology_get_loaded()` 等全局状态查询必须经过同一实现实例

---

## §5 Canonical API 契约（mock-first）

### §5.1 CpptlmBridge 公开 API

```cpp
namespace usr_linux_emu::sim_hardware {

enum class CpptlmBackendKind {
  kMock = 0,
  kCpptlm = 1,  // gated follow-up
};

using IntrDeliverCb = std::function<void(uint32_t vector, void* ctx)>;

struct CpptlmBridgeInitParams {
  CpptlmBackendKind backend = CpptlmBackendKind::kMock;
  const char* topology_path = nullptr;
  uint32_t flags = 0;
};

class CpptlmBridge {
 public:
  CpptlmBridge();
  ~CpptlmBridge();

  int init(const CpptlmBridgeInitParams& params);
  void destroy();

  int mmio_read(uint8_t bar, uint64_t offset, void* buf, size_t len);
  int mmio_write(uint8_t bar, uint64_t offset, const void* buf, size_t len);

  int config_read(uint16_t offset, uint32_t* value);
  int config_write(uint16_t offset, uint32_t value);

  int register_msix_callback(IntrDeliverCb cb, void* ctx);
};

}  // namespace usr_linux_emu::sim_hardware
```

### §5.2 公开 API 与内部辅助的边界

| 项 | 公开/内部 | 备注 |
|----|----------|------|
| `CpptlmBridge::*` | 公开 | §5.1 |
| `host_bridge_*` | 公开 | §5.3 |
| `bypass_apply_mode/get_mode` | 公开 | §5.4 |
| `topology_load_json/write_default_json` | 公开 | §5.5 |
| `platform_load/get` | 公开 | §5.6 |
| `bar_read32/bar_write32` scalar wrappers | 公开（可选） | §5.7 |
| `*_internal_*` | 内部 | 仅 mock backend 自用 |

### §5.3 Host Bridge API

```cpp
namespace usr_linux_emu::sim_hardware {

struct DiscoveredDevice {
  uint16_t bdf;          // bus:device.function packed
  uint16_t vendor_id;
  uint16_t device_id;
  uint32_t class_code;
  char     endpoint_kind[32];
};

int host_bridge_enumerate(DiscoveredDevice* devices, size_t max_devices,
                          size_t* out_count);
int host_bridge_bypass_read(uint8_t bar, uint64_t offset, void* dst, size_t len);
int host_bridge_bypass_write(uint8_t bar, uint64_t offset, const void* src,
                             size_t len);

}  // namespace usr_linux_emu::sim_hardware
```

参数规则：

- `out_count` 必填；空指针返回 `-EINVAL`
- `max_devices == 0` 且 `devices == nullptr` 是合法查询形式
- `max_devices > 0` 且 `devices == nullptr` 返回 `-EINVAL`
- `bar` 必须 ∈ [0, 5]
- `offset + len` 不允许 64-bit 溢出
- 不允许越界访问已分配 BAR
- 空 buffer + `len > 0` 返回 `-EINVAL`

### §5.4 Bypass API

```cpp
enum class BypassMode : uint8_t {
  kFull = 0,    // 正常 RC 路径
  kBypass = 1,  // 软件旁路
  kPartial = 2, // 部分旁路（按需）
};

enum class DrainPolicy : uint8_t {
  kGracefulDrain = 0,    // 等 in-flight 完成
  kImmediateAbort = 1,   // 立即 abort（test-only）
};

int bypass_apply_mode(BypassMode mode, DrainPolicy policy);
BypassMode bypass_get_mode();
```

- 显式整数值；不依赖枚举声明顺序
- mode 切换通过 `std::atomic<BypassMode>` 保护
- in-flight TLP 计数通过 `std::atomic<size_t>` 保护
- `kImmediateAbort` 仅在测试 `BypassTestScope` 内允许；生产路径不允许

### §5.5 Topology Loader API

```cpp
struct TopologyDevice {
  uint16_t bdf;
  uint16_t vendor_id;
  uint16_t device_id;
  uint32_t class_code;
  char     endpoint_kind[32];
  struct Bar {
    uint8_t  index;
    uint64_t size_bytes;
    bool     prefetchable;
    bool     is_mmio;
    bool     is_64bit;
  } bars[6];
  size_t bar_count;
};

struct Topology {
  uint32_t schema_version;       // 当前 = 1
  char     platform_name[32];
  bool     enable_root_complex;
  bool     enable_link_layer;
  bool     enable_phy;
  BypassMode default_bypass_mode;
  DrainPolicy default_drain_policy;
  TopologyDevice devices[16];
  size_t device_count;
};

int topology_load_json(const char* path, Topology* out);
int topology_write_default_json(const char* path, const Topology* src);
```

### §5.6 Platform API

```cpp
struct PlatformConfig {
  CpptlmBackendKind backend = CpptlmBackendKind::kMock;
  char topology_path[256];
  uint32_t flags = 0;
};

int platform_load(const PlatformConfig& cfg);
const PlatformConfig& platform_get();
```

### §5.7 BAR Scalar Wrappers（可选）

```cpp
inline int bar_read32(uint8_t bar, uint64_t offset, uint32_t* value);
inline int bar_write32(uint8_t bar, uint64_t offset, uint32_t value);
```

- wrappers 内部调用 `mmio_read/write(buf, sizeof(uint32_t))`
- 不引入新的 backend 契约
- 必须支持 `len ∈ {1, 2, 4, 8}` 的底层 buffer API

### §5.8 错误码表（统一）

| 条件 | 返回 |
|------|------|
| 成功 | 0 |
| 空必填指针 | `-EINVAL` |
| BAR 越界 / offset 越界 / offset+len 溢出 / len 不对齐 | `-EINVAL` |
| topology 文件不存在 | `-ENOENT` |
| topology JSON 非法 / 缺 required 字段 | `-EINVAL` |
| 枚举无设备 | `-ENODEV` |
| backend 未初始化 / 已销毁 | `-ENODEV` |
| mock 不支持的 backend (kCpptlm) | `-ENOSYS` |
| backend I/O 失败 | `-EIO` |
| DrainPolicy kGracefulDrain 超时 | `-EBUSY` |
| lifecycle 违规（destroy 后访问 / init 重入）| `-EINVAL` |

### §5.9 Lifecycle 表

| 调用序列 | 行为 |
|---------|------|
| `init()` 之前任何 MMIO/config/MSIX | `-ENODEV` |
| `init()` 重入（已 init 后再 init）| `-EBUSY` |
| `init()` 失败后 | bridge 处于"未初始化"状态，可重新 `init()` |
| `destroy()` 后再调用任何方法 | UB（对象已销毁）；测试应构造新对象 |
| `register_msix_callback()` 在 destroy 前未注销 | 析构期注销 callback（不调用回调）|

### §5.10 线程安全表

| 方法 | 并发安全 |
|------|---------|
| `init/destroy` | 与任何其他方法互斥 |
| `mmio_read/write/config_read/write` | 多线程安全（mock 内部 mutex） |
| `register_msix_callback` | 与 MMIO/config 并发安全；与 destroy 互斥 |
| `bypass_apply_mode/get_mode` | atomic；与任何方法并发安全 |
| `topology_load_json` | 与其他 loader 调用互斥 |

---

## §6 Backend 选择与加载策略

### §6.1 默认 backend = mock

```cmake
option(SIM_HARDWARE_ENABLE_CPPTLM "Enable CppTLM backend (gated follow-up)" OFF)
```

- `OFF`（默认）：仅构建 `sim_hardware_mock`
- `ON`：CMake 必须能找到 `cpptlm_emulator.h` 和 `libcpptlm_emulator.so`；找不到则 `FATAL_ERROR`

### §6.2 符号解析

- mock backend：纯本仓库代码，无外部符号
- CppTLM backend（follow-up）：`dlopen` 或链接期链接（CMake 选项决定）

### §6.3 与 kernel SHARED 单例规则的关系

- 现有 Issue #11：`kernel` 必须是 SHARED，否则 VFS 单例割裂
- sim_hardware_mock 是 STATIC，但其全局状态（如 bypass mode）仅在加载该 .a 的二进制内可见
- test 直接链 mock → 一个实例
- pci_driver_plugin 链 mock → 一个实例
- **不得**让 test 与 plugin 同时链 mock（会双实例）
- **建议**：test 通过 plugin 路径加载 mock（不直接链）

---

## §7 Q3 独立性约束

- sim_hardware_mock 不可依赖 Q1 (`kernel/*`)、Q2 (`plugins/*_driver/*`)、Q4 (`plugins/gpu_driver/sim/*`)
- sim_hardware_mock 可依赖外部 header-only：nlohmann/json（已 vendored）
- pci_driver_plugin 可依赖 sim_hardware + sim_hardware_mock（Q2 → Q3）
- iommu_driver_plugin 不依赖 sim_hardware（本 Change 范围内）

---

## §8 Bypass 模式与状态机

### §8.1 显式枚举值

```cpp
enum class BypassMode : uint8_t {
  kFull = 0,
  kBypass = 1,
  kPartial = 2,
};
enum class DrainPolicy : uint8_t {
  kGracefulDrain = 0,
  kImmediateAbort = 1,
};
```

### §8.2 状态机

```
            apply_mode(Full, GracefulDrain)
   ┌──────────────────────────────────────────┐
   │                                          ▼
[Full] ◄───── apply_mode(Full, *) ─────── [Partial]
   ▲                                          │
   │                                          ▼
   │      apply_mode(Bypass, GracefulDrain)  [Bypass]
   └────────────── apply_mode(Bypass, *) ◄────┘
                                              │
                                  apply_mode(Partial, ImmediateAbort)
                                              ▼
                                          [Partial abort]
```

### §8.3 Drain accounting

- 每个 MMIO/config 操作 `enter_tlp_count++` 在操作前，操作后 `enter_tlp_count--`
- `kGracefulDrain`：循环 `while (enter_tlp_count > 0 && !timeout)`
- `kImmediateAbort`：立即 `enter_tlp_count = 0`；**仅** `BypassTestScope` 允许

### §8.4 测试 gate

- M2c：3 态切换全部成功（Full ↔ Bypass ↔ Partial）
- Graceful drain：drain 成功（无 in-flight 时）
- Graceful drain timeout：drain 超时返回 `-EBUSY`
- Immediate abort：仅 test scope；非 test scope 返回 `-EACCES`
- Concurrent read：`bypass_get_mode()` 在并发下稳定

---

## §9 Topology JSON Schema

### §9.1 当前 `default_topology.json` 状态

```text
[ERROR] JSONDecodeError: Unterminated string starting at line 19
```

修复是实施任务 T2.1（不在本 Change 修订范围）。

### §9.2 修复后的 Schema（必须）

```json
{
  "schema_version": 1,
  "platform": "pc-x86-mock",
  "pcie": {
    "root_complex": {"type": "PcieRootComplexMock", "enabled": true},
    "link_layer":   {"type": "PcieLinkLayer",       "enabled": true},
    "phy":          {"type": "PciePhyDigitalCtrl",  "enabled": false},
    "bypass_mux":   {"default_mode": "Full", "default_drain_policy": "GracefulDrain"}
  },
  "devices": [
    {
      "bdf": "0000:01:00.0",
      "vendor_id": "0x10DE",
      "device_id": "0x1234",
      "class_code": "0x030200",
      "endpoint_kind": "PcieEndpointMock",
      "bars": [
        {"index": 0, "size_bytes": 16777216, "prefetchable": false, "is_mmio": true,  "is_64bit": false},
        {"index": 1, "size_bytes": 16777216, "prefetchable": true,  "is_mmio": true,  "is_64bit": true}
      ]
    }
  ]
}
```

### §9.3 Schema 字段规则

| 字段 | 必填 | 校验 |
|------|------|------|
| `schema_version` | 是 | == 1 |
| `platform` | 是 | non-empty string |
| `pcie.root_complex.enabled` | 是 | bool |
| `pcie.bypass_mux.default_mode` | 是 | ∈ {"Full", "Bypass", "Partial"} |
| `devices[].bdf` | 是 | 正则 `^[0-9A-Fa-f]{4}:[0-9A-Fa-f]{2}:[0-9A-Fa-f]{2}\.[0-9A-Fa-f]$` |
| `devices[].vendor_id/device_id` | 是 | 0x0000~0xFFFF |
| `devices[].bars[].index` | 是 | ∈ [0, 5] 且唯一 |
| `devices[].bars[].is_64bit` | 是 | 若 true，占两个 BAR slot（index+1 必须未使用）|

### §9.4 错误处理

- 缺 required 字段 → `-EINVAL`
- 类型错误 → `-EINVAL`
- BDF 不匹配正则 → `-EINVAL`
- BAR 64-bit 占用冲突 → `-EINVAL`
- 未知字段 → `-EINVAL`（严格 schema）

---

## §10 BAR Buffer/Scalar Adapter

### §10.1 buffer API 是契约

```cpp
int mmio_read(uint8_t bar, uint64_t offset, void* buf, size_t len);
int mmio_write(uint8_t bar, uint64_t offset, const void* buf, size_t len);
```

### §10.2 scalar wrapper 是适配

```cpp
inline int bar_read32(uint8_t bar, uint64_t offset, uint32_t* value) {
  return CpptlmBridge_get()->mmio_read(bar, offset, value, sizeof(uint32_t));
}
inline int bar_write32(uint8_t bar, uint64_t offset, uint32_t value) {
  return CpptlmBridge_get()->mmio_write(bar, offset, &value, sizeof(uint32_t));
}
```

### §10.3 测试必须使用 buffer API

- M2b 测试：`mmio_write(bar, offset, &value, sizeof(value))` 后 `mmio_read(bar, offset, &readback, sizeof(readback))`
- 不再使用 `bar_allocate(1, 0, 0x10, 0xE0000008)` 之类的 scalar 幻象调用（那是真实 CppTLM 假设）

### §10.4 对齐规则

- `offset % sizeof(T) == 0` 才允许 width T ∈ {1, 2, 4, 8} 的访问
- 非对齐访问返回 `-EINVAL`
- `len` 必须是 sizeof(T) 的整数倍

---

## §11 MSI-X Callback Adapter

### §11.1 契约（mock-only）

```cpp
using IntrDeliverCb = std::function<void(uint32_t vector, void* ctx)>;

int CpptlmBridge::register_msix_callback(IntrDeliverCb cb, void* ctx);
```

- cb 存储在 bridge 实例（非 `thread_local`）
- ctx 由调用方管理生命周期；bridge 在 destroy 时自动注销
- mock 后端可通过内部 API 触发 callback（如 `mock_inject_msix(vector)` 测试辅助）

### §11.2 真实 CppTLM callback 桥接（follow-up）

- 真实 callback 是 C ABI：`int (*cpptlm_intr_deliver_cb_t)(uint32_t vector, uint64_t user_data)`
- 桥接：写一个 C-compatible trampoline 把 `user_data` 强转为 `CpptlmBridge*`，调用其 `IntrDeliverCb`
- 禁止使用 `thread_local` 存储 callback（跨实例不工作；非主线程触发不可靠）

### §11.3 测试范围

- M2 mock：注册 callback → mock 触发 → callback 收到正确 vector
- M2 mock：未注册 callback → mock 触发 → silent drop（无 UB）
- M2 mock：destroy 时注销 callback → 不调用已注销 callback

---

## §12 17-port Endpoint（deferred）

### §12.1 当前状态

`sim_hardware/include/cpptlm/endpoint.h` 仅有 4 个 opaque handle：

```cpp
void* bar_router_h;
void* completer_h;
void* requester_h;
void* msix_h;
```

### §12.2 v0.4 决策

- 真实 17-port composition deferred to Change-3（ADR-091 §D5 L5 SR-IOV）
- 本 Change 仅提供 **mock 单 EP**（`pcie_endpoint_create` 返回有效 mock handle，含 bar_router/completer/requester/msix 4 个 mock 子对象）
- 不实现 BAR ↔ RC ↔ Req/Comp 之间真实连接
- 不实现 SR-IOV VF（0 个 VF）

### §12.3 真实 17-port composition 前置（follow-up）

- CppTLM 提供真实 C++ composition API
- 端口类型、所有权、连接方向、生命周期确认
- 测试覆盖每个端口的连接正确性 + 销毁对称性

---

## §13 PCI Probe 边界

### §13.1 当前真实文件

- `plugins/pci_driver/probe.cpp`（既有；`PcieEmuImpl` 设备模型）
- `plugins/pci_driver/pci_access.cpp`、`pci_cap.cpp`、`pci_msi.cpp`、`pci_iommu_integration.cpp`

### §13.2 本 Change 修改

- 修改 `probe.cpp`：**最小侵入**，新增可选调用点调 `host_bridge_enumerate` 填充 `pci_dev` 字段（不修改现有 PcieEmuImpl 行为）
- 新增 `pci_setup_bus.cpp`：调 mock BAR allocate + resource assignment
- 新增 `pcie_enable_device.cpp`：调 `config_write(PCI_COMMAND, MEMORY|IO)` 启用设备
- **不新增** `pci_probe.cpp`、`pci_scan_slot.cpp`（与既有架构不兼容）
- **不重命名** `probe.cpp`

### §13.3 PCI probe 集成调用链（mock）

```
VFS::open("/dev/gpgpu0") → GpgpuDevice::ioctl(PCI_PROBE)
  → host_bridge_enumerate(devs, 16, &count)
  → fill pci_dev (BDF, vendor_id, device_id)
  → pci_setup_bus(pci_dev) → mock bar_allocate
  → pcie_enable_device(pci_dev) → mock config_write(PCI_COMMAND)
  → return to ioctl handler
```

---

## §14 IOMMU 边界（deferred）

### §14.1 本 Change 范围

- ❌ **不实施** `plugins/iommu_driver/iommu_attach_pci.cpp`
- ❌ **不实施** `plugins/iommu_driver/iommu_invalidate_hw.cpp`
- ❌ **不修改** `plugins/iommu_driver/invalidate.cpp`（沿用 ADR-091 v0.2 决策）

### §14.2 边界声明

- Q3 提供 enumerated BDF 和 device identity 作为后续 IOMMU attach 的输入
- Q2 iommu_driver 不在本 Change 调用 sim_hardware
- ADR-091 §C1.2 列出 IOMMU 范围裁剪，本 Change 不裁剪（保持 Change-1 边界）

### §14.3 与 ADR-088 的关系

- ADR-088 §C2 明确：系统 IOMMU 由 UsrLinuxEmu 仿真（不是 CppTLM）
- 本 Change 不实现系统 IOMMU = 沿用 ADR-088 决策
- Change-3/4 实施 IOMMU hardware invalidation 时再单独提 change

---

## §15 M2a/M2b/M2c 验证清单

### §15.1 M2a（mock 枚举）

- ✅ 加载合法 `default_topology.json`
- ✅ `host_bridge_enumerate` 返回 1 设备（vendor_id=0x10DE, device_id=0x1234, BDF=0000:01:00.0）
- ✅ 边界：`max_devices == 0` → 仅查询数量
- ✅ 边界：空 topology → `*out_count = 0` 返回 0（不是 -ENODEV）
- ✅ 错误：畸形 topology → `-EINVAL`
- ✅ 错误：缺文件 → `-ENOENT`

### §15.2 M2b（BAR 往返）

- ✅ `mmio_write(bar, offset, &value, 4)` 后 `mmio_read(bar, offset, &back, 4)` 一致
- ✅ BAR 未分配时访问 → `-EINVAL`
- ✅ offset 越界 → `-EINVAL`
- ✅ 非对齐 → `-EINVAL`
- ✅ 错误：bar > 5 → `-EINVAL`
- ✅ 错误：buffer=nullptr + len>0 → `-EINVAL`

### §15.3 M2c（Bypass 3 态）

- ✅ `apply_mode(Full, Graceful)` → `get_mode() == Full`
- ✅ `apply_mode(Bypass, Graceful)` → `get_mode() == Bypass`
- ✅ `apply_mode(Partial, Graceful)` → `get_mode() == Partial`
- ✅ Graceful drain 成功（无 in-flight）
- ✅ Graceful drain 超时 → `-EBUSY`
- ✅ Immediate abort 仅 `BypassTestScope` 允许
- ✅ Concurrent `get_mode()` 稳定

### §15.4 M2 组合

M2a + M2b + M2c 全部通过 = M2 达成

### §15.5 latency informational

- latency 数据（如 `250-700ns`）仅记录，不作为 pass/fail
- 真实 CppTLM latency 验证属 follow-up

---

## §16 测试 plan（4 new + 1 modified）

| 文件 | 状态 | 覆盖 |
|------|------|------|
| `tests/sim_hardware/test_topology_standalone.cpp` | 🆕 NEW | 合法/非法/缺文件/schema 字段/round trip |
| `tests/sim_hardware/test_cpptlm_bridge_mock_standalone.cpp` | 🆕 NEW | init/lifecycle/BAR buffer/config/MSI-X callback |
| `tests/sim_hardware/test_pcie_host_bridge_mock_standalone.cpp` | 🆕 NEW | 枚举 + bypass read/write |
| `tests/sim_hardware/test_pcie_bypass_mock_standalone.cpp` | 🆕 NEW | Bypass 3 态 + DrainPolicy |
| `tests/plugins/test_pci_driver_standalone.cpp` | 🔁 MODIFIED | 既有（Change-1 创建）扩展 probe 集成 |

Catch2 框架；不可引入 GTest。

---

## §17 28-task 执行 map

完整 task 列表在 `tasks.md`。Wave 划分：

| Wave | 任务 ID | 任务数 |
|------|---------|-------:|
| Wave 0（Phase 0） | T0.1-T0.4 | 4 |
| Wave 1（mock target + backend seam） | T1.1-T1.4 | 4 |
| Wave 2（topology + bridge + BAR） | T2.1-T2.4 | 4 |
| Wave 3（bypass + adapter） | T3.1-T3.4 | 4 |
| Wave 4（PCI probe 集成） | T4.1-T4.3 | 3 |
| Wave 5（tests） | T5.1-T5.5 | 5 |
| Wave 6（M2 + 回归） | T6.1-T6.4 | 4 |
| **Total** | | **28** |

每任务含 TDD 五步：write failing test → verify fail → implement → verify pass → commit。

---

## §18 风险与回滚

| # | 风险 | 缓解 |
|---|------|------|
| 1 | mock 与真实 CppTLM 行为差异 | design §10/§11/§5.8 锁定契约 + 错误码；latency informational |
| 2 | 真实 CppTLM ABI 未冻结 | 本 Change API = 适配面；ABI 变化隔离在 `sim_hardware_cpptlm` backend 内 |
| 3 | pci_driver 静态链 sim_hardware_mock 的耦合 | mock backend 显式命名；未来 backend 同 API 替换 |
| 4 | mock 与 plugin 双实例（kernel SHARED 单例规则） | §4.5 静态状态规则；test 走 plugin 路径加载 |
| 5 | `default_topology.json` 修复未在 Wave 0 完成 | T2.1 显式任务 + T0.4 验收条件 |
| 6 | Change-2 范围蔓延（新增 IOMMU/PCI 文件） | §5/§13/§14 显式边界 + tasks.md 任务计数冻结 |

**回滚**：本 Change 是 additive（仅新增 + 最小侵入）；不修改 Change-1 archived 内容。回滚 = `git revert <commit hash>`。

---

## §19 与 ADR / Change 关系

### §19.1 前置依赖

- **ADR-091 v0.2 ✅ Accepted**（4 象限目录布局）：已归档 Change-1 实施
- **Change-1 pci-driver-refactor ✅ Shipped**（archived 2026-09-03）

### §19.2 关联（不修改）

- **ADR-088**（CppTLM 23 ABI Draft）：作为契约参考，不修改
- **CppTLM v4 Handoff**（Draft）：作为契约参考，不修改
- **ADR-090 v2**（H2D DMA）：BAR0 MMIO 走 PCIe bus，本 Change 提供 PCIe bus 契约面

### §19.3 下游解锁

- **Change-3**（Tier 3+5+6 Link Layer + SR-IOV + Completion）：API 契约已就绪
- **真实 CppTLM follow-up**：`sim_hardware_cpptlm` backend 可独立立项

---

**Status**: 🔄 Proposed v0.4（Metis 复审修复版；待 Oracle + Metis 双审查）
