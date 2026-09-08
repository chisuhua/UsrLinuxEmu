# Design: 5.5.6-cpptlm-ep-binding — 真实 CppTLM EP 绑定技术设计

> **设计状态**: Draft v1.0（与 proposal.md 同步）
> **关联**: [proposal.md](proposal.md)
> **前置基线**: [kcpptlm-archive-audit](../2026-09-08-kcpptlm-archive-audit/)
> **关联 ADR**: [ADR-091](../00_adr/adr-091-pci-driver-architecture-and-four-quadrant.md) ✅ / [ADR-092](../00_adr/adr-092-hal-adapter-and-bypass-binding.md) 🔄 / [ADR-023](../00_adr/adr-023-hal-interface.md) ✅

---

## §1 总体架构

### 1.1 端到端 dGPU 链路（5.5.6 覆盖范围）

```
[用户态 ioctl]
    ↓ GPU_IOCTL_*
[GpgpuDevice::ioctl 表]（5.5.6 不改）
    ↓
[gpu_hal_ops 71 fn-ptr]（ADR-023 append-only 形状不变）
    ↓ 3 adapter op → hal_cpptlm（真实 CppTLM binding）
    ↓ 其余 68 fn-ptr → hal_user/hal_mock 委托（**组合策略**）
    ↓
[hal_cpptlm 3 op → backdoor_endpoint (ule_dgpu_*)]
    ↓
[backdoor_endpoint 真实实现 → bridge kCpptlm dlopen]
    ↓
[libcpptlm_emulator.so 24 ABI]（CppTLM v0.5.0-MVP 已 ship）
    ↓
[DgpuBoard (CppTLM 仓)] → SOC → pcie_ep + sdma + cp + tmu + sq + cq + gpu + vram
```

### 1.2 5.5.6 vs 后续 Stage 边界

| 阶段 | 范围 | 5.5.6 交付 |
|---|---|---|
| 5.5.6（本 change） | 真实 EP binding（PCIe EP + 端到端通路） | backdoor_endpoint.cpp + bridge.cpp kCpptlm + host_bridge.cpp bypass/full + hal_cpptlm.cpp + plugin.cpp backend |
| 5.5.7 | CommandProcessor 真实化 | puller/queue submit 经 CppTLM TLP（依赖 5.5.6 EP 通路）|
| 5.5.8 | kernel dispatch + DMA | ioctl 表穿透 + CppTLM backdoor DMA |
| 5.5.9 | 真机双轨验证 | drv/ 零修改 L2 build |

**5.5.6 必须提供 EP 通路**——5.5.7/5.5.8/5.5.9 才能在其之上实现 CP/DMA/真机。

---

## §2 接口契约

### 2.1 backdoor_endpoint.cpp 5 函数契约

```cpp
// sim_hardware/include/cpptlm/backdoor_endpoint.h（已 ship，无需修改）

int ule_dgpu_acquire(uint32_t dev_id, ule_dgpu_handle_t* out_handle);
// 实现：cpptlm_emulator_create() + cpptlm_emulator_open(dev_id, &handle)
// 返回：0 成功；-EINVAL 参数；-ENOSYS CppTLM 未加载；-EBUSY 已 acquire；-ETIMEDOUT 超时
// 超时：100ms wall-clock（异步提交 + future 等待）

int ule_dgpu_get_adapter_info(ule_dgpu_handle_t handle, ule_dgpu_adapter_info* out_info);
// 实现：cpptlm_emulator_get_adapter_info() + 字段映射（cpptlm_device_info → ule_dgpu_adapter_info）
// 返回：0 成功；-EINVAL 参数；-ENOSYS CppTLM 未加载

int ule_dgpu_read(ule_dgpu_handle_t handle, ule_dgpu_space space, uint64_t offset,
                  void* buf, size_t len);
int ule_dgpu_write(ule_dgpu_handle_t handle, ule_dgpu_space space, uint64_t offset,
                   const void* buf, size_t len);
// 实现：根据 space 分发
//   kConfig (0)   → cpptlm_emulator_pcie_config_read/write
//   kBarMmio (1)  → cpptlm_emulator_mmio_read/write
//   kBarVram (2)  → cpptlm_emulator_backdoor_read/write
//   kAxiDirect (3) → -EOPNOTSUPP（v0.2 范围外）

int ule_dgpu_release(ule_dgpu_handle_t handle);
// 实现：cpptlm_emulator_close()
// 返回：0 成功；-EINVAL 参数；-ENOSYS CppTLM 未加载
```

### 2.2 CppTLM ABI 绑定表（24 个符号）

CppTLM v0.5.0-MVP ship 24 个 `cpptlm_emulator_*` 符号（外部仓已冻结）。本 change 绑定清单：

```cpp
// sim_hardware/src/cpptlm/bridge.cpp kCpptlm 分支新增 dlopen + dlsym
struct CpptlmSymbols {
  void* create;                       // cpptlm_emulator_create
  void* create_by_id;                 // cpptlm_emulator_create_by_id
  void* destroy;                      // cpptlm_emulator_destroy
  void* open;                         // cpptlm_emulator_open
  void* close;                        // cpptlm_emulator_close
  void* get_version;                  // cpptlm_emulator_get_version
  void* get_device_count;             // cpptlm_emulator_get_device_count
  void* get_device_info;              // cpptlm_emulator_get_device_info
  void* get_adapter_info;             // cpptlm_emulator_get_adapter_info
  void* mmio_read;                    // cpptlm_emulator_mmio_read
  void* mmio_write;                   // cpptlm_emulator_mmio_write
  void* backdoor_read;                // cpptlm_emulator_backdoor_read
  void* backdoor_write;               // cpptlm_emulator_backdoor_write
  void* pcie_config_read;             // cpptlm_emulator_pcie_config_read
  void* pcie_config_write;            // cpptlm_emulator_pcie_config_write
  void* msix_init;                    // cpptlm_emulator_msix_init
  void* msix_update_pending;          // cpptlm_emulator_msix_update_pending
  void* msix_clear_pending;           // cpptlm_emulator_msix_clear_pending
  void* lookup_register;              // cpptlm_emulator_lookup_register
  void* register_callbacks;           // cpptlm_emulator_register_callbacks
  void* register_backdoor_cb;         // cpptlm_emulator_register_backdoor_cb
  void* register_dma_translate_cb;    // cpptlm_emulator_register_dma_translate_cb
};
```

**dlopen 顺序**：
1. `dlopen("libcpptlm_emulator.so", RTLD_NOW | RTLD_GLOBAL)`
2. 任一 dlsym 返回 NULL → 整个 binding 失败 → 回退 mock + WARN 日志
3. 全部成功 → 写入 `CpptlmBridge::Impl::cpptlm_syms` 字段
4. `CpptlmBridge::init()` 设 `backend = kCpptlm` + 调 `cpptlm_syms.create()`

### 2.3 host_bridge.cpp bypass/full 自动分发

```cpp
// 当前 host_bridge.cpp:88-94（bypass_read/write 直接调 bridge->mmio_*）
// 改造为：根据 bypass_get_mode() 分发

int host_bridge_bypass_read(uint8_t bar, uint64_t offset, void* dst, size_t len) {
  CpptlmBridge* bridge = CpptlmBridge_get();
  if (!bridge) return -EINVAL;
  
  BypassMode mode = bypass_get_mode();
  if (mode == BypassMode::kFull) {
    // Full 模式：走真实 TLP → mmio_read
    return bridge->mmio_read(bar, offset, dst, len);
  } else if (mode == BypassMode::kBypass) {
    // Bypass 模式：直接 backdoor → 调 CppTLM 或 mock 直读
    return bridge->backdoor_read(bar, offset, dst, len);  // 新增方法
  } else {
    // kPartial 或未知：保持当前 mmio_read 行为（与 v0.2 一致）
    return bridge->mmio_read(bar, offset, dst, len);
  }
}
```

**bridge.cpp 新增** `backdoor_read/write` 方法（delegating 到 dlopen 的 cpptlm 符号）。

### 2.4 hal_cpptlm.cpp 组合策略

```cpp
void hal_cpptlm_init(struct gpu_hal_ops* hal, void* ctx) {
  // 步骤 1：组合策略 - 先调 hal_user_init 填 68 fn-ptr
  hal_user_init(hal, static_cast<hal_user_context*>(ctx));
  
  // 步骤 2：覆盖 3 个 adapter fn-ptr 为 CppTLM 真实调用
  hal->adapter_get_info = cpptlm_adapter_get_info;
  hal->adapter_open = cpptlm_adapter_open;
  hal->adapter_close = cpptlm_adapter_close;
  
  // 步骤 3：保留 ctx（hal_user 已设）
  hal->ctx = ctx;
}
```

**adapter_get_info 实现**（hal_cpptlm.cpp）：
```cpp
int cpptlm_adapter_get_info(void* ctx, gpu_adapter_info_t* out_info) {
  if (!out_info) return -EINVAL;
  
  // 调 backdoor_endpoint 真实实现
  ule_dgpu_handle_t handle;
  int ret = ule_dgpu_acquire(0, &handle);
  if (ret != 0) return ret;
  
  ule_dgpu_adapter_info info;
  ret = ule_dgpu_get_adapter_info(handle, &info);
  if (ret != 0) { ule_dgpu_release(handle); return ret; }
  
  // 字段映射：ule_dgpu_adapter_info → gpu_adapter_info_t
  out_info->vendor_id = info.vendor_id;
  out_info->device_id = info.device_id;
  // ... 其余 7 字段
  
  ule_dgpu_release(handle);
  return 0;
}
```

### 2.5 plugin.cpp:119 backend 选择点

```cpp
// 当前：硬编码 hal_user_init
// 改造：env ULE_HAL_BACKEND 选择

void init_hal_for_device(HalHolder& h, const DiscoveredDevice& dev) {
  const char* backend_env = std::getenv("ULE_HAL_BACKEND");
  
  if (backend_env && std::string(backend_env) == "cpptlm") {
    hal_cpptlm_init(&h.hal, &h.ctx);
  } else if (backend_env && std::string(backend_env) == "user") {
    hal_user_init(&h.hal, &h.ctx);
  } else {
    // 默认 mock（保留 164/164 ctest 基线）
    hal_mock_init(&h.hal, &h.mock_state);
  }
}
```

---

## §3 数据流

### 3.1 adapter_open 数据流

```
hal.adapter_open(ctx, out_handle)
  ↓ cpptlm_adapter_open
ule_dgpu_acquire(dev_id=0, out_handle)
  ↓ cpptlm_emulator_create() → cpptlm_emulator_t*
  ↓ cpptlm_emulator_open(0, &handle)
  ↓ PendingReq { future, timeout=100ms }
  ↓ submit to CppTLM sim thread queue
  ↓ std::future::wait_for(100ms)
  ↓ 返回 handle
  ↓ 写入 *out_handle
```

### 3.2 读写数据流（kBarMmio 路径）

```
ule_dgpu_read(handle, kBarMmio, offset, buf, len)
  ↓ space == kBarMmio → 分发
cpptlm_emulator_mmio_read(handle, bar, offset, buf, len)
  ↓ CppTLM 内核 DgpuBoard::mmio_read（async inject + future wait）
  ↓ 数据填充 buf
  ↓ 返回 0
```

### 3.3 bypass_read 数据流

```
host_bridge_bypass_read(bar, offset, dst, len)
  ↓ bypass_get_mode() == kFull
bridge->mmio_read(bar, offset, dst, len)  // 真实 TLP 走 CppTLM
  ↓ 或 kBypass 模式
bridge->backdoor_read(bar, offset, dst, len)  // 新增：直读 CppTLM backdoor
  ↓ 完成
```

---

## §4 关键设计决策

### 4.1 选定的 dlopen 路径

**默认查找顺序**（`sim_hardware/src/cpptlm/bridge.cpp`）：
1. `dlopen("libcpptlm_emulator.so", RTLD_NOW | RTLD_GLOBAL)` —— 系统动态链接器路径
2. 失败 → 显式 `dlopen("../CppTLM/build/lib/libcpptlm_emulator.so", ...)` —— sibling 仓开发期回退
3. 失败 → WARN 日志 + 回退 `CpptlmBackendKind::kMock`

**理由**：开发期不依赖 `LD_LIBRARY_PATH` 配置；CI 走系统路径。

### 4.2 错误处理矩阵

| 场景 | 错误码 | 日志 | 行为 |
|---|:-:|---|---|
| libcpptlm_emulator.so 缺失 | -ENOSYS | WARN "CppTLM not found, using mock" | 回退 mock |
| dlsym 失败（ABI 不匹配）| -ENOSYS | WARN "Cpptlm ABI missing: %s" | 回退 mock |
| ule_dgpu_acquire 超时 | -ETIMEDOUT | ERROR "CppTLM 100ms timeout" | 不回退（cpptlm 路径失败）|
| cpptlm_emulator_open 返回错误 | -EBUSY | ERROR | 不回退 |

### 4.3 测试夹具（test fixture）

```cpp
// tests/sim_hardware/test_backdoor_endpoint_real_standalone.cpp
struct CpptlmTestFixture {
  void SetUp() {
    // 检查 libcpptlm_emulator.so 是否可用
    handle_ = dlopen("libcpptlm_emulator.so", RTLD_NOW);
    if (!handle_) {
      // SKIP：mock 路径在另一个测试覆盖
      SKIP("libcpptlm_emulator.so not found, see test_backdoor_endpoint_standalone");
    }
  }
  
  void TearDown() {
    if (handle_) dlclose(handle_);
  }
  
  void* handle_ = nullptr;
};

TEST_CASE_METHOD(CpptlmTestFixture, "backdoor_endpoint: acquire + get_adapter_info", "[cpptlm][backdoor]") {
  ule_dgpu_handle_t h;
  REQUIRE(ule_dgpu_acquire(0, &h) == 0);
  
  ule_dgpu_adapter_info info;
  REQUIRE(ule_dgpu_get_adapter_info(h, &info) == 0);
  REQUIRE(info.vendor_id != 0);
  
  REQUIRE(ule_dgpu_release(h) == 0);
}
```

### 4.4 超时与死锁防护

- 所有跨线程调用走 100ms wall-clock 超时
- 超时后 `cpptlm_emulator_close()` 强制回收（`/proc/self/fd` 检查）
- 测试用 `sleep_for(50ms)` 模拟工作负载验证超时分支
- 析构函数严格顺序：poison pill → join → 析构 EventQueue（CppTLM 侧遵守）

---

## §5 兼容性

### 5.1 HAL append-only（ADR-023）

- ✅ `gpu_hal_ops` struct 形状不变（71 fn-ptr）
- ✅ 仅 3 个 adapter fn-ptr 内容替换（从 -ENOSYS 到真实调用）
- ✅ 其余 68 fn-ptr 通过 hal_user_init 委托，不修改 hal_cpptlm 自身

### 5.2 既有 ctest 零回归

- ✅ 默认 backend 是 `user`（mock），保留 164/164 ctest 基线
- ✅ 真实 cpptlm backend 通过 env `ULE_HAL_BACKEND=cpptlm` 触发
- ✅ 5 个新测试独立 binary，与既有测试不共享状态

### 5.3 `drv/` 零修改（ADR-036）

- ✅ 不修改 `plugins/gpu_driver/drv/`
- ✅ 不修改 `gpu_ioctl.h` IOCTL 编号
- ✅ 不修改 GpgpuDevice::ioctl 表

---

## §6 实施步骤映射

| 实施步骤 | 设计章节 | 工期 |
|---|---|:-:|
| P4.NEW-A: backdoor_endpoint.cpp 真实实现 | §2.1, §3.1-3.2, §4.2 | 1-2 周 |
| P4.NEW-B: bridge.cpp kCpptlm dlopen | §2.2, §3.3, §4.1, §4.2 | 1-2 周 |
| P4.NEW-C: host_bridge.cpp bypass/full 分发 | §2.3, §3.3 | 0.5-1 周 |
| P4.NEW-D: hal_cpptlm.cpp 真化 + plugin.cpp backend | §2.4-2.5, §4.1, §4.4 | 1 周 |
| 5 个测试 binary | §4.3, §4.4 | 1 周 |
| 总计 | | **4.5-7 周**（vs 估算 4-6 周，含缓冲）|

---

## §7 跨引用

- [proposal.md](proposal.md) — Why/What/Capabilities/Impact
- [tasks.md](tasks.md) — TDD 5 步拆解
- [specs/cpptlm-real-backend/spec.md](specs/cpptlm-real-backend/spec.md) — capability 规范
- [ADR-091](../00_adr/adr-091-pci-driver-architecture-and-four-quadrant.md) — 4 象限布局
- [ADR-092](../00_adr/adr-092-hal-adapter-and-bypass-binding.md) — HAL adapter binding
- [ADR-023](../00_adr/adr-023-hal-interface.md) — HAL append-only
- [kcpptlm-archive-audit](../2026-09-08-kcpptlm-archive-audit/) — 前置基线

---

**设计者**: UsrLinuxEmu Architecture Team
**设计日期**: 2026-09-08
