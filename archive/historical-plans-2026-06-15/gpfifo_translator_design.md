# GPU GPFIFO → TaskRunner LaunchParams 翻译层设计

**状态**: 设计中
**创建日期**: 2026-05-09
**维护者**: Sisyphus

---

## 1. 背景

### 1.1 问题

`HardwarePullerEmu` 的 `scheduler_` 参数已预留但未连接。当前的 `GlobalScheduler` 是空壳，`EngineDispatchFn` 从未被调用。

```
HardwarePullerEmu::runLoop()
    │
    ├─ FETCH: 读取 gpu_gpfifo_entry
    ├─ DECODE: 解析 method type
    ├─ SCHEDULE: GlobalScheduler::enqueue()  ← 未连接
    └─ DISPATCH: EngineDispatchFn()          ← 从未调用
```

### 1.2 目标

实现 `gpu_gpfifo_entry` → `LaunchParams` 翻译层，使 HardwarePuller 能通过 GlobalScheduler 调用 TaskRunner 的 `CudaScheduler::submit_launch()`。

---

## 2. 接口分析

### 2.1 源侧: `gpu_gpfifo_entry` (UsrLinuxEmu)

```cpp
struct gpu_gpfifo_entry {
  u32 valid : 1;
  u32 priv : 1;
  u32 method : 12;    // GPU_OP_LAUNCH_KERNEL=0x100, GPU_OP_MEMCPY=0x102, etc.
  u32 subchannel : 3;
  u32 _reserved : 15;
  u64 payload[7];     // Method arguments (opaque)
  u64 semaphore_va;    // Completion semaphore
  u32 semaphore_value;
  u32 release : 1;     // Release on completion
  u32 _pad : 31;
};
```

**Payload 编码** (已确认于 `gpu_drm_driver.cpp` 和 `gpgpu_device.cpp`):

| Method | payload[0] | payload[1] | payload[2] |
|--------|-----------|-----------|-----------|
| GPU_OP_LAUNCH_KERNEL | kernel_idx | grid_dim (packed) | block_dim (packed) |
| GPU_OP_MEMCPY | src_addr | dst_addr | size |
| GPU_OP_MEMSET | dst_addr | val | size |
| GPU_OP_FENCE | - | - | - |

**Grid/Block 压缩格式** (来自 `GpuDriverClient::submit_launch()`):

```cpp
// Grid: grid_x | (grid_y << 16) | (grid_z << 24)
entry.payload[1] = grid_x | (grid_y << 16) | (static_cast<uint64_t>(grid_z) << 24);

// Block: block_x | (block_y << 8) | (block_z << 16)
entry.payload[2] = block_x | (block_y << 8) | (static_cast<uint64_t>(block_z) << 16);
```

### 2.2 目标侧: `LaunchParams` (TaskRunner)

```cpp
struct LaunchParams {
  const char* kernel_name{nullptr};
  void* params{nullptr};           // Kernel arguments struct pointer
  size_t params_size{0};

  uint32_t grid_dim_x{1};
  uint32_t grid_dim_y{1};
  uint32_t grid_dim_z{1};

  uint32_t block_dim_x{256};
  uint32_t block_dim_y{1};
  uint32_t block_dim_z{1};

  uint32_t shared_mem_bytes{0};
};
```

### 2.3 缺失字段

| LaunchParams 字段 | gpu_gpfifo_entry 来源 | 说明 |
|-------------------|----------------------|------|
| kernel_name | 无直接映射 | 需要 kernel_idx → name 表 |
| params | 无直接映射 | GPU 直接访问设备内存，不需要 host 指针 |
| params_size | 无直接映射 | 同上 |
| grid_dim_x/y/z | payload[1] 解压 | 见压缩格式 |
| block_dim_x/y/z | payload[2] 解压 | 见压缩格式 |
| shared_mem_bytes | 无 | 默认 0 |

---

## 3. 翻译层架构

### 3.1 组件关系

```
┌─────────────────────────────────────────────────────────────────┐
│                    HardwarePullerEmu                              │
│  runLoop() → SCHEDULE → GlobalScheduler::enqueue()               │
└─────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────┐
│                     GlobalScheduler                               │
│  queue_: std::queue<WorkItem>                                   │
│  dispatch_fn_: EngineDispatchFn                                  │
│                                                                 │
│  dequeue() → dispatch_fn_(entry, engine)  ← 调用回调             │
└─────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────┐
│              GpfifoToLaunchParamsTranslator                      │
│                                                                 │
│  translate(const gpu_gpfifo_entry&, EngineType)                  │
│      │                                                         │
│      ├─ unpackGridDim(payload[1]) → grid_dim_x/y/z             │
│      ├─ unpackBlockDim(payload[2]) → block_dim_x/y/z            │
│      └─ lookupKernelName(kernel_idx) → kernel_name               │
│                                                                 │
│  operator()(entry, engine) → translator_.translate()              │
└─────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────┐
│                      CudaScheduler                                │
│  submit_launch(LaunchParams) → CUDA API / Stub 模式              │
└─────────────────────────────────────────────────────────────────┘
```

### 3.2 翻译函数签名

```cpp
// plugins/gpu_driver/sim/scheduler/gpfifo_translator.h

#pragma once

#include "gpu_types.h"
#include "cuda_stub.hpp"  // TaskRunner::LaunchParams

#include <string>
#include <map>

namespace usr_linux_emu {

class GpfifoToLaunchParamsTranslator {
public:
  GpfifoToLaunchParamsTranslator();

  // 翻译 gpu_gpfifo_entry → LaunchParams
  // 返回 true 表示成功，false 表示无法翻译（如未知 kernel_idx）
  bool translate(const gpu_gpfifo_entry& entry,
                 taskrunner::LaunchParams* out_params);

  // 注册 kernel name 表
  void registerKernel(uint32_t kernel_idx, const char* kernel_name);

  // 提取引擎类型
  EngineType getEngineType(const gpu_gpfifo_entry& entry);

private:
  // 解压 grid dimension
  void unpackGridDim(uint64_t packed, uint32_t* out_x,
                     uint32_t* out_y, uint32_t* out_z);

  // 解压 block dimension
  void unpackBlockDim(uint64_t packed, uint32_t* out_x,
                      uint32_t* out_y, uint32_t* out_z);

  std::map<uint32_t, std::string> kernel_names_;
};

}  // namespace usr_linux_emu
```

### 3.3 回调链设置

```cpp
// plugin.cpp 中的初始化

// 1. 创建翻译器
auto translator = std::make_unique<GpfifoToLaunchParamsTranslator>();
translator->registerKernel(0, "simple_kernel");
translator->registerKernel(1, "matmul_kernel");

// 2. 设置调度器回调
scheduler.setDispatchCallback(
    [translator = translator.get(), cuda_scheduler](
        const gpu_gpfifo_entry& entry,
        EngineType engine) {
      taskrunner::LaunchParams params;
      if (!translator->translate(entry, &params)) {
        std::cerr << "Failed to translate GPFIFO entry\n";
        return;
      }

      if (engine == EngineType::COMPUTE) {
        auto result = cuda_scheduler.submit_launch(params);
        if (result.status != 0) {
          std::cerr << "Kernel launch failed\n";
        }
      }
      // COPY 引擎类似处理...
    }
);

// 3. 创建 CudaScheduler
auto cuda_scheduler = std::make_unique<taskrunner::CudaScheduler>();
cuda_scheduler->initialize(true);  // Stub 模式

// 4. 注入到 puller
hal_holder.puller = new HardwarePullerEmu(
    &hal_holder.hal,
    &hal_holder.doorbell,
    &scheduler  // 现在会被使用
);
```

---

## 4. 关键实现细节

### 4.1 Grid/Block 维度解压

```cpp
void GpfifoToLaunchParamsTranslator::unpackGridDim(uint64_t packed,
                                                     uint32_t* out_x,
                                                     uint32_t* out_y,
                                                     uint32_t* out_z) {
  *out_x = packed & 0xFFFF;
  *out_y = (packed >> 16) & 0xFF;
  *out_z = (packed >> 24) & 0xFF;
}

void GpfifoToLaunchParamsTranslator::unpackBlockDim(uint64_t packed,
                                                      uint32_t* out_x,
                                                      uint32_t* out_y,
                                                      uint32_t* out_z) {
  *out_x = packed & 0xFF;
  *out_y = (packed >> 8) & 0xFF;
  *out_z = (packed >> 16) & 0xFF;
}
```

### 4.2 翻译函数

```cpp
bool GpfifoToLaunchParamsTranslator::translate(
    const gpu_gpfifo_entry& entry,
    taskrunner::LaunchParams* out_params) {

  if (entry.method == GPU_OP_LAUNCH_KERNEL) {
    uint32_t kernel_idx = static_cast<uint32_t>(entry.payload[0]);

    auto it = kernel_names_.find(kernel_idx);
    if (it == kernel_names_.end()) {
      return false;  // 未知 kernel
    }

    out_params->kernel_name = it->second.c_str();
    out_params->params = nullptr;      // GPU 直接访问
    out_params->params_size = 0;

    uint32_t gx, gy, gz;
    uint32_t bx, by, bz;
    unpackGridDim(entry.payload[1], &gx, &gy, &gz);
    unpackBlockDim(entry.payload[2], &bx, &by, &bz);

    out_params->grid_dim_x = gx;
    out_params->grid_dim_y = gy;
    out_params->grid_dim_z = gz;
    out_params->block_dim_x = bx;
    out_params->block_dim_y = by;
    out_params->block_dim_z = bz;
    out_params->shared_mem_bytes = 0;

    return true;
  }

  return false;  // 非 kernel 操作暂不支持
}
```

---

## 5. 依赖关系

```
plugins/gpu_driver/
├── sim/
│   ├── scheduler/
│   │   ├── global_scheduler.h      [已存在，需扩展]
│   │   ├── global_scheduler.cpp
│   │   └── gpfifo_translator.h     [新增]
│   │   └── gpfifo_translator.cpp   [新增]
│   └── hardware/
│       └── hardware_puller_emu.cpp [runLoop 调用 enqueue]
└── plugin.cpp                       [初始化回调链]

external/TaskRunner/
├── include/
│   ├── cuda_stub.hpp               [LaunchParams 定义]
│   └── cuda_scheduler.hpp          [CudaScheduler 定义]
└── src/
    └── cuda_scheduler.cpp
```

---

## 6. 测试策略

### 6.1 单元测试

```cpp
// tests/test_gpfifo_translator.cpp

TEST_CASE("unpack grid dimensions") {
  GpfifoToLaunchParamsTranslator translator;
  uint32_t gx, gy, gz;

  // 典型值: grid(256, 16, 1)
  uint64_t packed = 256 | (16 << 16) | (1 << 24);
  translator.unpackGridDim(packed, &gx, &gy, &gz);
  REQUIRE(gx == 256);
  REQUIRE(gy == 16);
  REQUIRE(gz == 1);
}

TEST_CASE("translate LAUNCH_KERNEL") {
  GpfifoToLaunchParamsTranslator translator;
  translator.registerKernel(0, "test_kernel");

  gpu_gpfifo_entry entry = {};
  entry.method = GPU_OP_LAUNCH_KERNEL;
  entry.payload[0] = 0;           // kernel_idx
  entry.payload[1] = 256 | (16 << 16);  // grid
  entry.payload[2] = 64 | (1 << 8);     // block

  taskrunner::LaunchParams params;
  bool ok = translator.translate(entry, &params);

  REQUIRE(ok == true);
  REQUIRE(params.grid_dim_x == 256);
  REQUIRE(params.block_dim_x == 64);
}
```

### 6.2 集成测试

```cpp
// tests/test_puller_scheduler_integration.cpp

TEST_CASE("puller → scheduler → translator → cuda_scheduler") {
  // 1. Mock HAL + DoorbellEmu
  struct gpu_hal_ops hal = make_mock_hal();
  DoorbellEmu doorbell;
  GlobalScheduler scheduler;

  // 2. 创建真正的翻译器
  auto translator = std::make_unique<GpfifoToLaunchParamsTranslator>();
  translator->registerKernel(0, "simple_kernel");

  // 3. Mock CudaScheduler (不调用真实 CUDA)
  int launch_count = 0;
  taskrunner::LaunchParams last_params;

  scheduler.setDispatchCallback(
    [&](const gpu_gpfifo_entry& entry, EngineType engine) {
      taskrunner::LaunchParams params;
      if (translator->translate(entry, &params)) {
        launch_count++;
        last_params = params;
      }
    }
  );

  // 4. 创建 Puller（scheduler 参数有效）
  HardwarePullerEmu puller(&hal, &doorbell, &scheduler);
  puller.start();

  // 5. 提交 batch
  puller.submitBatch(0x1000, 1);
  doorbell.write(0);

  // 6. 等待处理
  std::this_thread::sleep_for(100ms);

  REQUIRE(launch_count > 0);  // 确认回调被调用
  REQUIRE(last_params.grid_dim_x > 0);
}
```

---

## 7. 已知限制

1. **MEMCPY/MEMSET 操作**: 翻译器只处理 `GPU_OP_LAUNCH_KERNEL`。MEMCPY 需要通过 `CudaScheduler::submit_memcpy_*` 处理。
2. **Kernel 参数传递**: 当前设计假设 kernel 参数在 GPU 内存中（GPU 直接访问）。如果需要 host 指针，需要扩展 `gpu_gpfifo_entry` 或使用不同的协议。
3. **Semaphore 语义**: `semaphore_va` / `semaphore_value` / `release` 字段需要在 `CudaScheduler` 侧实现对应的信号机制。

---

## 8. 后续工作

1. **Phase A**: 实现 `GpfifoToLaunchParamsTranslator` 类
2. **Phase B**: 在 `GlobalScheduler::dequeue()` 中调用 `dispatch_fn_` (当前是空实现)
3. **Phase C**: 在 `plugin.cpp` 中连接回调链
4. **Phase D**: 添加 MEMCPY/MEMSET 支持
5. **Phase E**: Semaphore 信号机制集成