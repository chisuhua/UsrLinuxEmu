# GPU Callback 集成实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 修复 `plugin.cpp` 中 GlobalScheduler 与 HardwarePullerEmu 之间的 callback 链断裂问题，确保 `submit_launch → GPFIFO → doorbell → puller → scheduler → callback` 端到端流水线正常工作。

**Architecture:** 在 `plugin_init_internal()` 中为 `GlobalScheduler` 设置 `LaunchParamsCallback`，将翻译后的 kernel 启动参数传递给 TaskRunner。通过现有测试扩展验证 callback 被正确触发。

**Tech Stack:** C++17, Catch2 (tests), CMake, UsrLinuxEmu 插件系统

---

## 1. 执行摘要

### 1.1 问题定义

当前 GPU 仿真插件的 callback 链在 `GlobalScheduler` 处断裂：

- `HardwarePullerEmu::runLoop()` 在 DISPATCH 阶段调用 `scheduler_->enqueue()`
- `GlobalScheduler::enqueue()` 调用 `translator_.translate(entry)`
- `GpfifoToLaunchParamsTranslator::translate()` 检查 `launch_cb_`，但**该 callback 从未被设置**
- 结果：TaskRunner 无法接收到 kernel 启动通知

### 1.2 根本原因

`plugins/gpu_driver/plugin.cpp` 的 `plugin_init_internal()` 中：

```cpp
// 创建了 scheduler，但从未设置 callback
HalHolder hal_holder;
hal_holder.puller = std::make_shared<HardwarePullerEmu>(
    &hal_holder.hal, &hal_holder.doorbell, &hal_holder.scheduler);
// 缺失: hal_holder.scheduler.setLaunchCallback(...)
```

### 1.3 修复范围

| 组件 | 改动 | 风险 |
|------|------|------|
| `plugin.cpp` | 添加 `setLaunchCallback()` 调用 | 低 |
| `test_gpu_callback_integration.cpp` | 新增 callback 验证测试 | 低 |
| `tests/CMakeLists.txt` | 添加新测试到构建 | 低 |

**预估工作量：** 1-2 小时
**预估影响面：** 仅 GPU 插件初始化路径

---

## 2. 问题分析

### 2.1 Callback 链完整流程

```
TaskRunner (submit_launch)
    │
    │ ioctl(GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH)
    ▼
GpgpuDevice::handlePushbufferSubmitBatch()
    │
    ├─ puller_->submitBatch(gpfifo_addr, count)  [写入 GPFIFO entries]
    └─ hal_doorbell_ring(hal_, stream_id)        [触发 doorbell]
           │
           ▼
    hal_user_set_doorbell_cb callback ──> DoorbellEmu::write(queue_id)
           │
           ▼
    DoorbellEmu::setCallback ──> HardwarePullerEmu::onDoorbell(qid)
           │
           ▼
    HardwarePullerEmu::runLoop()
        │
        ├── State::FETCH ──> fetchEntry()           [从 GPFIFO 读取 entry]
        ├── State::DECODE ──> 检查 entry 类型
        ├── State::SCHEDULE ──> 准备调度
        └── State::DISPATCH ──> scheduler_->enqueue(entry, engine)
                                    │
                                    ▼
                            GlobalScheduler::enqueue()
                                │
                                ├─ translator_.translate(entry)
                                │       │
                                │       ▼
                                │   GpfifoToLaunchParamsTranslator::translate()
                                │       │
                                │       ├─ 解析 kernel_idx → kernel_name
                                │       ├─ 解包 grid_dim / block_dim
                                │       └─ 调用 launch_cb_(kernel_name, grid_*, block_*, shared_mem)
                                │               │
                                │               ▼
                                │        launch_cb_ 为空，callback 丢失
                                │
                                └─ WorkItem 入队
```

### 2.2 关键数据结构

**LaunchParamsCallback 签名**（`gpfifo_translator.h:23-26`）：

```cpp
using LaunchParamsCallback = std::function<void(
    const char* kernel_name,
    uint32_t grid_x, uint32_t grid_y, uint32_t grid_z,
    uint32_t block_x, uint32_t block_y, uint32_t block_z,
    uint32_t shared_mem)>;
```

**GpfifoToLaunchParamsTranslator 编码约定**（`gpfifo_translator.h:17-20`）：

```cpp
// payload[0]: kernel_idx (uint32_t)
// payload[1]: grid_dim (packed: grid_x | (grid_y << 16) | (grid_z << 24))
// payload[2]: block_dim (packed: block_x | (block_y << 8) | (block_z << 16))
```

**现有内核注册**（`gpgpu_device.cpp:47-48`）：

```cpp
registered_kernels_["simple_kernel"] = 0;
registered_kernels_["matmul_kernel"] = 1;
```

---

## 3. 实施步骤

### Task 1: 编写 callback 验证测试（TDD - RED）

**目标：** 创建失败的测试，证明当前 callback 未被调用。

**方案选择：** 创建 `test_gpu_callback_integration.cpp` 作为 sim 测试，直接测试 HardwarePullerEmu + GlobalScheduler 的 callback 连接。

原因：
- `test_gpu_plugin.cpp` 使用全局静态 `PluginLifecycle`，无法注入自定义 callback
- sim 测试可以直接操作组件，更精确地验证 callback 链
- 无需修改现有测试基础设施

---

- [ ] **Step 1: 创建 test_gpu_callback_integration.cpp**

创建 `tests/test_gpu_callback_integration.cpp`：

```cpp
/*
 * test_gpu_callback_integration.cpp — 验证 HardwarePullerEmu → GlobalScheduler → Callback 链
 */

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <thread>

#include "doorbell_emu.h"
#include "hardware_puller_emu.h"
#include "scheduler/global_scheduler.h"
#include "gpu_types.h"

// 模拟的 gpu_hal_ops，仅实现 mem_read
static int mock_mem_read(void* ctx, uint64_t addr, void* out, size_t size) {
  (void)ctx;
  (void)addr;
  // 返回预定义的 gpfifo entry
  static gpu_gpfifo_entry entry = {};
  entry.valid = 1;
  entry.method = GPU_OP_LAUNCH_KERNEL;
  entry.payload[0] = 0;  // kernel_idx = 0
  entry.payload[1] = 1 | (2u << 16) | (3u << 24);  // grid=(1,2,3)
  entry.payload[2] = 4 | (5u << 8) | (6u << 16);   // block=(4,5,6)

  if (size >= sizeof(entry)) {
    std::memcpy(out, &entry, sizeof(entry));
    return 0;
  }
  return -1;
}

static int mock_mem_write(void* ctx, uint64_t addr, const void* data, size_t size) {
  (void)ctx;
  (void)addr;
  (void)data;
  (void)size;
  return 0;
}

int test_callback_chain() {
  std::cout << "=== test_callback_chain ===\n";

  // 测试状态
  std::atomic<bool> callback_called(false);
  std::string captured_kernel_name;
  uint32_t captured_grid_x = 0, captured_grid_y = 0, captured_grid_z = 0;
  uint32_t captured_block_x = 0, captured_block_y = 0, captured_block_z = 0;

  // 创建组件
  struct gpu_hal_ops hal = {};
  hal.mem_read = mock_mem_read;
  hal.mem_write = mock_mem_write;

  DoorbellEmu doorbell;
  GlobalScheduler scheduler;

  // 设置 callback（这是本次修复的核心）
  scheduler.setLaunchCallback(
      [&](const char* kernel_name, uint32_t grid_x, uint32_t grid_y, uint32_t grid_z,
          uint32_t block_x, uint32_t block_y, uint32_t block_z, uint32_t shared_mem) {
        callback_called.store(true);
        captured_kernel_name = kernel_name;
        captured_grid_x = grid_x;
        captured_grid_y = grid_y;
        captured_grid_z = grid_z;
        captured_block_x = block_x;
        captured_block_y = block_y;
        captured_block_z = block_z;
        (void)shared_mem;
      });

  // 注册内核
  scheduler.registerKernel(0, "simple_kernel");
  scheduler.registerKernel(1, "matmul_kernel");

  // 创建 puller
  HardwarePullerEmu puller(&hal, &doorbell, &scheduler);
  puller.start();

  // 模拟 GPFIFO 提交
  uint64_t gpfifo_addr = 0x10000000;
  uint32_t entry_count = 1;
  puller.submitBatch(gpfifo_addr, entry_count);

  // 触发 doorbell
  doorbell.write(0);

  // 等待 puller 线程处理
  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  puller.stop();

  // 验证 callback 被调用
  if (!callback_called.load()) {
    std::cerr << "FAIL: callback was not called\n";
    return 1;
  }

  if (captured_kernel_name != "simple_kernel") {
    std::cerr << "FAIL: expected kernel_name='simple_kernel', got '" << captured_kernel_name
              << "'\n";
    return 1;
  }

  if (captured_grid_x != 1 || captured_grid_y != 2 || captured_grid_z != 3) {
    std::cerr << "FAIL: grid mismatch, expected (1,2,3), got (" << captured_grid_x << ","
              << captured_grid_y << "," << captured_grid_z << ")\n";
    return 1;
  }

  if (captured_block_x != 4 || captured_block_y != 5 || captured_block_z != 6) {
    std::cerr << "FAIL: block mismatch, expected (4,5,6), got (" << captured_block_x << ","
              << captured_block_y << "," << captured_block_z << ")\n";
    return 1;
  }

  std::cout << "PASS: test_callback_chain\n";
  return 0;
}

int test_callback_without_registration() {
  std::cout << "=== test_callback_without_registration ===\n";

  std::atomic<bool> callback_called(false);
  std::string captured_kernel_name;

  struct gpu_hal_ops hal = {};
  hal.mem_read = mock_mem_read;
  hal.mem_write = mock_mem_write;

  DoorbellEmu doorbell;
  GlobalScheduler scheduler;

  scheduler.setLaunchCallback(
      [&](const char* kernel_name, uint32_t grid_x, uint32_t grid_y, uint32_t grid_z,
          uint32_t block_x, uint32_t block_y, uint32_t block_z, uint32_t shared_mem) {
        callback_called.store(true);
        captured_kernel_name = kernel_name;
        (void)grid_x;
        (void)grid_y;
        (void)grid_z;
        (void)block_x;
        (void)block_y;
        (void)block_z;
        (void)shared_mem;
      });

  // 注意：不注册内核，kernel_idx 0 会映射为 "unknown"

  HardwarePullerEmu puller(&hal, &doorbell, &scheduler);
  puller.start();

  puller.submitBatch(0x10000000, 1);
  doorbell.write(0);

  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  puller.stop();

  if (!callback_called.load()) {
    std::cerr << "FAIL: callback was not called\n";
    return 1;
  }

  if (captured_kernel_name != "unknown") {
    std::cerr << "FAIL: expected kernel_name='unknown', got '" << captured_kernel_name << "'\n";
    return 1;
  }

  std::cout << "PASS: test_callback_without_registration\n";
  return 0;
}

int main() {
  int result = 0;

  std::cout << "=== GPU Callback Integration Tests ===\n";

  result |= test_callback_chain();
  result |= test_callback_without_registration();

  if (result == 0) {
    std::cout << "\n=== ALL TESTS PASSED ===\n";
  } else {
    std::cout << "\n=== SOME TESTS FAILED ===\n";
  }

  return result;
}
```

---

- [ ] **Step 2: 在 CMakeLists.txt 中注册新测试**

在 `tests/CMakeLists.txt` 的 `SIM_TESTS` 列表中添加新测试：

```cmake
set(SIM_TESTS
    test_gpu_ringbuffer.cpp
    test_queue_puller_integration.cpp
    test_hardware_puller_emu.cpp
    test_global_scheduler.cpp
    test_gpfifo_translator.cpp
    test_gpu_callback_integration.cpp  # 新增
)
```

---

- [ ] **Step 3: 编译并运行测试（应失败）**

```bash
cd /workspace/project/UsrLinuxEmu/build
make -j4 test_gpu_callback_integration_standalone
```

运行测试：

```bash
cd /workspace/project/UsrLinuxEmu
./build/bin/test_gpu_callback_integration_standalone
```

**Expected Output (FAILED):**

```
=== GPU Callback Integration Tests ===
=== test_callback_chain ===
FAIL: callback was not called
...
=== SOME TESTS FAILED ===
```

这证明了 plugin.cpp 中 callback 未设置的问题。

---

### Task 2: 修复 plugin.cpp 中的 callback 连接（GREEN）

**目标：** 在 `plugin_init_internal()` 中为 GlobalScheduler 设置 LaunchParamsCallback。

**Files:**
- Modify: `plugins/gpu_driver/plugin.cpp`

---

- [ ] **Step 4: 在 plugin.cpp 中添加 callback 设置**

在 `plugins/gpu_driver/plugin.cpp` 中，找到 `plugin_init_internal()` 函数，在 `hal_holder.puller->start()` 之前添加 callback 设置：

```cpp
static int plugin_init_internal() {
  std::cout << "[GpuPlugin] Initializing...\n";

  static HalHolder hal_holder;
  hal_user_init(&hal_holder.hal, &hal_holder.ctx);
  g_hal = &hal_holder;

  hal_holder.puller = std::make_shared<HardwarePullerEmu>(&hal_holder.hal,
                                                          &hal_holder.doorbell,
                                                          &hal_holder.scheduler);

  // 新增：设置 LaunchParamsCallback
  hal_holder.scheduler.setLaunchCallback(
      [](const char* kernel_name, uint32_t grid_x, uint32_t grid_y, uint32_t grid_z,
         uint32_t block_x, uint32_t block_y, uint32_t block_z, uint32_t shared_mem) {
        std::cout << "[GpuPlugin] LaunchCallback: kernel=" << kernel_name
                  << " grid=(" << grid_x << "," << grid_y << "," << grid_z << ")"
                  << " block=(" << block_x << "," << block_y << "," << block_z << ")"
                  << " shared_mem=" << shared_mem << "\n";
        // TODO: 将参数传递给 TaskRunner
        // 当前仅打印日志，后续可扩展为调用 TaskRunner API
        (void)shared_mem;
      });

  // 注册已知内核（与 GpgpuDevice 中的注册保持一致）
  hal_holder.scheduler.registerKernel(0, "simple_kernel");
  hal_holder.scheduler.registerKernel(1, "matmul_kernel");
  // 新增结束

  int ret = hal_user_set_doorbell_cb(&hal_holder.ctx,
      [](void* cb_ctx, uint32_t queue_id) {
        auto* dh = static_cast<HalHolder*>(cb_ctx);
        dh->doorbell.write(queue_id);
      },
      &hal_holder);
  if (ret != 0) {
    std::cerr << "[GpuPlugin] Failed to set doorbell callback: " << ret << "\n";
    return ret;
  }

  hal_holder.puller->start();
  // ... 剩余代码不变
}
```

**关键注意点：**

1. **callback 设置的时机**：必须在 `puller->start()` 之前设置，否则 HardwarePullerEmu 线程可能在 callback 设置前就开始处理 entry。

2. **内核注册**：`registerKernel()` 必须在 submit 之前调用，否则 translator 会将所有 kernel_idx 映射为 "unknown"。当前 GpgpuDevice 注册了 `simple_kernel`(0) 和 `matmul_kernel`(1)，这里需要保持一致。

3. **callback 内容**：当前仅打印日志。后续 TaskRunner 集成时，可以替换为实际的 kernel 启动调用。

---

- [ ] **Step 5: 重新编译并运行测试（应通过）**

```bash
cd /workspace/project/UsrLinuxEmu/build
make -j4 test_gpu_callback_integration_standalone
```

运行测试：

```bash
cd /workspace/project/UsrLinuxEmu
./build/bin/test_gpu_callback_integration_standalone
```

**Expected Output (PASSED):**

```
=== GPU Callback Integration Tests ===
=== test_callback_chain ===
PASS: test_callback_chain
=== test_callback_without_registration ===
PASS: test_callback_without_registration

=== ALL TESTS PASSED ===
```

---

### Task 3: 端到端验证

- [ ] **Step 6: 运行所有 GPU 相关测试**

```bash
cd /workspace/project/UsrLinuxEmu/build
make -j4
ctest -R "gpu" --output-on-failure
```

**Expected:** 所有测试通过，包括：
- `test_gpu_ioctl_standalone`
- `test_gpu_ioctl_number_standalone`
- `test_gpu_plugin`
- `test_gpu_submit_standalone`
- `test_gpu_memory`
- `test_gpu_mmap_standalone`
- `test_gpu_mmap_and_submit_standalone`
- `test_gpu_mmap_bar`
- `test_gpu_register_standalone`
- `test_gpu_regs_standalone`
- `test_gpu_ringbuffer_standalone`
- `test_hardware_puller_emu_standalone`
- `test_global_scheduler_standalone`
- `test_gpfifo_translator_standalone`
- `test_gpu_callback_integration_standalone`（新增）

---

- [ ] **Step 7: 运行完整测试套件**

```bash
cd /workspace/project/UsrLinuxEmu/build
ctest --output-on-failure
```

**Expected:** 100% 测试通过

---

### Task 4: 代码审查和清理

- [ ] **Step 8: 运行静态分析**

```bash
cd /workspace/project/UsrLinuxEmu
clang-tidy -p build/ plugins/gpu_driver/plugin.cpp
```

检查是否有警告：
- 未使用的变量
- 潜在的内存问题
- 线程安全问题

---

- [ ] **Step 9: 代码格式化**

```bash
cd /workspace/project/UsrLinuxEmu
clang-format -i plugins/gpu_driver/plugin.cpp
clang-format -i tests/test_gpu_callback_integration.cpp
```

---

- [ ] **Step 10: 提交变更**

```bash
cd /workspace/project/UsrLinuxEmu
git add plugins/gpu_driver/plugin.cpp
git add tests/test_gpu_callback_integration.cpp
git add tests/CMakeLists.txt
git add plans/callback-integration-plan.md
git commit -m "fix(plugin): connect GlobalScheduler LaunchParamsCallback

修复 plugin.cpp 中 GlobalScheduler 的 launch callback 未设置问题。

- 在 plugin_init_internal() 中为 scheduler 设置 LaunchParamsCallback
- 注册 simple_kernel(0) 和 matmul_kernel(1) 内核映射
- 添加 test_gpu_callback_integration.cpp 验证 callback 链
- 确保 HardwarePullerEmu → GlobalScheduler → GpfifoToLaunchParamsTranslator 链路完整

Issue: callback 链在 translator 处断裂，launch_cb_ 为空导致
       TaskRunner 无法接收 kernel 启动通知。

Fixes: plugin.cpp 在 puller->start() 前添加 setLaunchCallback()"
```

---

## 4. 验证计划

### 4.1 单元测试验证

| 测试 | 目标 | 验证点 |
|------|------|--------|
| `test_callback_chain` | callback 链完整性 | callback 被调用、参数正确解析 |
| `test_callback_without_registration` | 边界情况 | 未注册内核返回 "unknown" |
| `test_gpu_plugin` 现有测试 | 回归测试 | ioctl 路径未被破坏 |
| `test_global_scheduler` 现有测试 | 回归测试 | scheduler 功能正常 |

### 4.2 手动验证

运行 plugin 并观察日志输出：

```bash
cd /workspace/project/UsrLinuxEmu
./build/bin/test_gpu_plugin "[gpu][ioctl][submit]"
```

**期望看到新增日志：**

```
[GpuPlugin] LaunchCallback: kernel=simple_kernel grid=(1,1,1) block=(2,1,1) shared_mem=0
```

### 4.3 性能验证

本次改动无性能影响：
- 仅添加一个 lambda callback 赋值
- 无额外内存分配
- 无额外线程创建

---

## 5. 风险和缓解措施

### 5.1 风险矩阵

| 风险 | 概率 | 影响 | 缓解措施 |
|------|------|------|----------|
| callback 设置时机过晚 | 低 | 高 | 确保在 `puller->start()` 前设置 |
| 内核注册不一致 | 中 | 中 | 与 GpgpuDevice 保持一致，后续统一注册机制 |
| 多线程竞争 | 低 | 中 | callback 仅被读取，atomic 保证可见性 |
| 测试不稳定 | 中 | 低 | 使用 200ms 等待，如不稳定可增加等待时间或使用条件变量 |

### 5.2 回滚计划

如出现问题，回滚步骤：

```bash
cd /workspace/project/UsrLinuxEmu
git revert HEAD  # 或手动还原 plugin.cpp
cd build && make -j4
ctest --output-on-failure
```

### 5.3 后续工作

**Phase 2（可选）：** 将 callback 连接到实际的 TaskRunner API

```cpp
// 当前：仅打印日志
hal_holder.scheduler.setLaunchCallback(
    [](const char* kernel_name, ...) {
      std::cout << "..." << std::endl;
    });

// 未来：调用 TaskRunner
hal_holder.scheduler.setLaunchCallback(
    [](const char* kernel_name, uint32_t grid_x, ...) {
      task_runner_launch_kernel(kernel_name, grid_x, ...);
    });
```

**Phase 3（可选）：** 实现 ADR-024 用户态 Queue（UMQ）
- 替换 ioctl 路径为 Queue 路径
- 支持多流并发提交

---

## 6. 附录

### 6.1 文件清单

| 文件 | 操作 | 说明 |
|------|------|------|
| `plugins/gpu_driver/plugin.cpp` | 修改 | 添加 setLaunchCallback 和 registerKernel |
| `tests/test_gpu_callback_integration.cpp` | 创建 | callback 链集成测试 |
| `tests/CMakeLists.txt` | 修改 | 添加 test_gpu_callback_integration 到 SIM_TESTS |
| `plans/callback-integration-plan.md` | 创建 | 本文档 |

### 6.2 参考文档

- `docs/07-integration/` — 集成文档
- `plugins/gpu_driver/sim/scheduler/translator/gpfifo_translator.h` — Callback 签名定义
- `plugins/gpu_driver/sim/scheduler/global_scheduler.h` — Scheduler API
- `plugins/gpu_driver/sim/hardware/hardware_puller_emu.h` — Puller 状态机

---

**计划版本:** 1.0
**创建日期:** 2026-05-13
**作者:** AI Agent
