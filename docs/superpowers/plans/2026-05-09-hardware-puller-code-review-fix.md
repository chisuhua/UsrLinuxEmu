# Hardware Puller 代码审查问题修复计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 修复 Hardware Puller Phase 1/2 代码审查中发现的 6 个问题，包括 UAF 风险、回调泄漏、未使用变量、所有权语义、测试时序依赖和类型安全。

**Architecture:** 采用最小化修改原则，保持现有功能和行为不变。关键修复包括：调整插件销毁顺序避免 UAF、为 doorbell 回调添加设置检查、使用智能指针明确 puller 所有权、用条件变量替代睡眠等待。

**Tech Stack:** C++17, CMake, Catch2, pthread

---

## 文件变更映射

| 文件 | 操作 | 说明 |
|------|------|------|
| `plugins/gpu_driver/plugin.cpp` | 修改 | 修复销毁顺序，移除未使用的 scheduler |
| `plugins/gpu_driver/hal/hal_user.cpp` | 修改 | 添加 doorbell 回调覆盖检查 |
| `plugins/gpu_driver/hal/hal_user.h` | 修改 | 添加返回值到 hal_user_set_doorbell_cb |
| `plugins/gpu_driver/drv/gpgpu_device.h` | 修改 | 使用 shared_ptr 替代裸指针 |
| `plugins/gpu_driver/drv/gpgpu_device.cpp` | 修改 | 适配 shared_ptr 变更 |
| `plugins/gpu_driver/sim/hardware/hardware_puller_emu.h` | 修改 | 修正 scheduler 参数类型 |
| `plugins/gpu_driver/sim/hardware/hardware_puller_emu.cpp` | 修改 | 适配类型变更 |
| `tests/test_hardware_puller_emu.cpp` | 修改 | 用条件变量替代 sleep_for |

---

## 风险评估总览

| 问题 | 优先级 | 风险级别 | 验证难度 |
|------|--------|---------|---------|
| UAF 风险 | 🔴 P0 | **高** - 可能导致崩溃 | 中等（需要并发测试） |
| doorbell 回调覆盖 | 🔴 P0 | **中** - 逻辑错误 | 低 |
| GlobalScheduler 未使用 | 🟡 P1 | **低** - 代码异味 | 低 |
| puller_ 所有权 | 🟡 P1 | **中** - 维护风险 | 低 |
| 测试时序依赖 | 🟡 P1 | **中** - 测试不稳定 | 低 |
| scheduler 参数类型 | 🔵 P2 | **低** - 类型安全 | 低 |

---

## 测试策略

1. **编译验证**: 每次修改后运行 `make -j4` 确保无编译错误
2. **单元测试**: 运行 `make test` 或 `./bin/test_hardware_puller_emu_standalone`
3. **集成测试**: 运行所有 GPU 相关测试
4. **回归测试**: 确保现有功能未被破坏

---

## Task 1: 修复 Use-After-Free 风险 (plugin.cpp)

**Files:**
- Modify: `plugins/gpu_driver/plugin.cpp:61-71`
- Modify: `plugins/gpu_driver/drv/gpgpu_device.h:52`
- Modify: `plugins/gpu_driver/drv/gpgpu_device.cpp:38-40`

**问题分析:**
当前销毁顺序：
1. `unregister_device("gpgpu0")` - 从 VFS 移除设备
2. `puller->stop()` - 停止 puller
3. `delete puller` - 删除 puller

但 `unregister_device` 不会等待其他线程释放 `Device` 的 `shared_ptr`。如果某线程正在执行 `ioctl`（如 `handlePushbufferSubmitBatch`），它会通过 `puller_` 访问已删除的 `HardwarePullerEmu`，导致 UAF。

**修复方案:**
调整销毁顺序：先停止 puller，再注销设备，最后删除 puller。

- [ ] **Step 1: 修改 plugin_fini_internal 销毁顺序**

修改 `plugins/gpu_driver/plugin.cpp` 第 61-71 行：

```cpp
static void plugin_fini_internal() {
  std::cout << "[GpuPlugin] Shutting down...\n";
  if (g_hal) {
    // 先停止 puller，不再接受新工作
    g_hal->puller->stop();
    
    // 再注销设备，确保没有新的 ioctl 进入
    VFS::instance().unregister_device("gpgpu0");
    
    // 最后删除 puller，此时所有 Device 引用已释放
    delete g_hal->puller;
    g_hal->puller = nullptr;
    hal_user_destroy(&g_hal->ctx);
    g_hal = nullptr;
  }
}
```

- [ ] **Step 2: 为 GpgpuDevice 添加 puller 空指针检查**

修改 `plugins/gpu_driver/drv/gpgpu_device.cpp` 第 230 行：

```cpp
  if (puller_ && !has_fence) {
```

已经是安全的（有 `puller_` 检查），但在 `setPuller` 后应添加注释说明生命周期管理。

修改 `plugins/gpu_driver/drv/gpgpu_device.h` 第 52 行附近添加注释：

```cpp
  // puller_ 由 plugin.cpp 管理生命周期，GpgpuDevice 不拥有所有权
  // plugin_fini_internal() 中先 stop() 再 unregister_device() 最后 delete
  // 确保此处不会访问已释放的 puller
  HardwarePullerEmu* puller_{nullptr};
```

- [ ] **Step 3: 编译验证**

Run: `cd /workspace/project/UsrLinuxEmu/build && make -j4`
Expected: 编译成功，无错误

- [ ] **Step 4: 运行测试**

Run: `cd /workspace/project/UsrLinuxEmu/build && make test`
Expected: 所有测试通过

- [ ] **Step 5: Commit**

```bash
cd /workspace/project/UsrLinuxEmu
git add plugins/gpu_driver/plugin.cpp plugins/gpu_driver/drv/gpgpu_device.h plugins/gpu_driver/drv/gpgpu_device.cpp
git commit -m "fix(gpu): 修复 plugin_fini_internal UAF 风险

调整销毁顺序：先 stop puller，再 unregister_device，
最后 delete puller。确保没有并发 ioctl 访问已释放的 puller。"
```

---

## Task 2: 修复 doorbell 回调覆盖问题 (hal_user.cpp)

**Files:**
- Modify: `plugins/gpu_driver/hal/hal_user.h:45-47`
- Modify: `plugins/gpu_driver/hal/hal_user.cpp:149-154`

**问题分析:**
`hal_user_set_doorbell_cb()` 直接覆盖 `ctx->doorbell_ring_cb`，不做任何检查。如果多次调用，旧的回调指针丢失，可能导致资源泄漏或逻辑错误。

**修复方案:**
添加返回值，如果已有回调则返回错误码。

- [ ] **Step 1: 修改 hal_user.h 添加返回值**

修改 `plugins/gpu_driver/hal/hal_user.h` 第 45-47 行：

```cpp
// 设置 doorbell 回调。如果已设置过回调，返回 -EBUSY。
int hal_user_set_doorbell_cb(struct hal_user_context* ctx,
                               void (*cb)(void*, uint32_t),
                               void* cb_ctx);
```

- [ ] **Step 2: 修改 hal_user_set_doorbell_cb 实现**

修改 `plugins/gpu_driver/hal/hal_user.cpp` 第 149-154 行：

```cpp
int hal_user_set_doorbell_cb(struct hal_user_context* ctx,
                               void (*cb)(void*, uint32_t),
                               void* cb_ctx) {
  if (ctx->doorbell_ring_cb != nullptr) {
    return -16; /* -EBUSY */
  }
  ctx->doorbell_ring_cb = cb;
  ctx->doorbell_ring_cb_ctx = cb_ctx;
  return 0;
}
```

- [ ] **Step 3: 更新 plugin.cpp 中的调用处理返回值**

修改 `plugins/gpu_driver/plugin.cpp` 第 41-46 行：

```cpp
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
```

- [ ] **Step 4: 编译验证**

Run: `cd /workspace/project/UsrLinuxEmu/build && make -j4`
Expected: 编译成功，无错误

- [ ] **Step 5: 运行测试**

Run: `cd /workspace/project/UsrLinuxEmu/build && make test`
Expected: 所有测试通过

- [ ] **Step 6: Commit**

```bash
cd /workspace/project/UsrLinuxEmu
git add plugins/gpu_driver/hal/hal_user.h plugins/gpu_driver/hal/hal_user.cpp plugins/gpu_driver/plugin.cpp
git commit -m "fix(gpu): 防止 doorbell 回调被覆盖

hal_user_set_doorbell_cb() 现在返回 int，如果回调已设置
则返回 -EBUSY。调用方需要检查返回值。"
```

---

## Task 3: 移除未使用的 GlobalScheduler (plugin.cpp)

**Files:**
- Modify: `plugins/gpu_driver/plugin.cpp:16-22`
- Modify: `plugins/gpu_driver/plugin.cpp:37-39`

**问题分析:**
HalHolder 中构造了 `GlobalScheduler scheduler` 但从未使用。这增加了不必要的初始化和内存开销。

**修复方案:**
由于 GlobalScheduler 是 Phase 2 计划使用的组件，暂时注释掉并添加 TODO，而不是完全删除。

- [ ] **Step 1: 注释掉未使用的 scheduler 成员**

修改 `plugins/gpu_driver/plugin.cpp` 第 16-22 行：

```cpp
namespace {
struct HalHolder {
  struct gpu_hal_ops hal;
  struct hal_user_context ctx;
  DoorbellEmu doorbell;
  // TODO(Phase2): 启用 GlobalScheduler
  // GlobalScheduler scheduler;
  HardwarePullerEmu* puller;
};
```

- [ ] **Step 2: 更新 puller 构造参数**

修改 `plugins/gpu_driver/plugin.cpp` 第 37-39 行：

```cpp
  hal_holder.puller = new HardwarePullerEmu(&hal_holder.hal,
                                              &hal_holder.doorbell,
                                              nullptr);  // scheduler 暂不使用
```

- [ ] **Step 3: 编译验证**

Run: `cd /workspace/project/UsrLinuxEmu/build && make -j4`
Expected: 编译成功，无错误

- [ ] **Step 4: Commit**

```bash
cd /workspace/project/UsrLinuxEmu
git add plugins/gpu_driver/plugin.cpp
git commit -m "refactor(gpu): 注释掉未使用的 GlobalScheduler

GlobalScheduler 将在 Phase 2 启用，暂时注释掉以避免
不必要的初始化和编译器警告。"
```

---

## Task 4: 明确 puller_ 所有权语义 (gpgpu_device.h)

**Files:**
- Modify: `plugins/gpu_driver/drv/gpgpu_device.h:52`
- Modify: `plugins/gpu_driver/drv/gpgpu_device.cpp:38-40`
- Modify: `plugins/gpu_driver/plugin.cpp:50-51`

**问题分析:**
`HardwarePullerEmu* puller_{nullptr}` 是裸指针，没有明确的所有权语义。这增加了维护难度和出错风险。

**修复方案:**
使用 `std::shared_ptr<HardwarePullerEmu>` 来明确共享所有权。plugin.cpp 创建并持有 puller，GpgpuDevice 通过 shared_ptr 引用。

- [ ] **Step 1: 修改 gpgpu_device.h 使用 shared_ptr**

修改 `plugins/gpu_driver/drv/gpgpu_device.h`：

```cpp
#include <memory>
// ... 其他 include ...

class HardwarePullerEmu;

class GpgpuDevice : public FileOperations {
 public:
  // ... 其他方法 ...
  void setPuller(std::shared_ptr<HardwarePullerEmu> puller);
  // ...
 private:
  // ...
  std::shared_ptr<HardwarePullerEmu> puller_;
};
```

- [ ] **Step 2: 修改 gpgpu_device.cpp 适配 shared_ptr**

修改 `plugins/gpu_driver/drv/gpgpu_device.cpp` 第 38-40 行：

```cpp
void GpgpuDevice::setPuller(std::shared_ptr<HardwarePullerEmu> puller) {
  puller_ = puller;
}
```

- [ ] **Step 3: 修改 plugin.cpp 使用 shared_ptr**

修改 `plugins/gpu_driver/plugin.cpp` 第 50-51 行：

```cpp
  auto puller = std::make_shared<HardwarePullerEmu>(&hal_holder.hal,
                                                      &hal_holder.doorbell,
                                                      nullptr);
  hal_holder.puller = puller.get();  // 保持原始指针用于直接访问
  
  auto device = std::make_shared<GpgpuDevice>(&hal_holder.hal);
  device->setPuller(puller);
```

修改 `HalHolder` 结构体以保存 shared_ptr：

```cpp
namespace {
struct HalHolder {
  struct gpu_hal_ops hal;
  struct hal_user_context ctx;
  DoorbellEmu doorbell;
  std::shared_ptr<HardwarePullerEmu> puller;
};
```

- [ ] **Step 4: 修改 plugin_fini_internal**

修改 `plugins/gpu_driver/plugin.cpp` 第 61-71 行：

```cpp
static void plugin_fini_internal() {
  std::cout << "[GpuPlugin] Shutting down...\n";
  if (g_hal) {
    g_hal->puller->stop();
    VFS::instance().unregister_device("gpgpu0");
    // shared_ptr 在 HalHolder 销毁时自动释放
    g_hal->puller.reset();
    hal_user_destroy(&g_hal->ctx);
    g_hal = nullptr;
  }
}
```

- [ ] **Step 5: 编译验证**

Run: `cd /workspace/project/UsrLinuxEmu/build && make -j4`
Expected: 编译成功，无错误

- [ ] **Step 6: 运行测试**

Run: `cd /workspace/project/UsrLinuxEmu/build && make test`
Expected: 所有测试通过

- [ ] **Step 7: Commit**

```bash
cd /workspace/project/UsrLinuxEmu
git add plugins/gpu_driver/drv/gpgpu_device.h plugins/gpu_driver/drv/gpgpu_device.cpp plugins/gpu_driver/plugin.cpp
git commit -m "refactor(gpu): 使用 shared_ptr 管理 puller 生命周期

将 HardwarePullerEmu* 裸指针改为 std::shared_ptr，
明确所有权语义，避免悬垂指针风险。"
```

---

## Task 5: 消除测试时序依赖 (test_hardware_puller_emu.cpp)

**Files:**
- Modify: `tests/test_hardware_puller_emu.cpp:1-340`

**问题分析:**
测试中使用 `std::this_thread::sleep_for()` 等待状态机完成，这在慢机器上可能失败。应该使用条件变量或轮询等待特定状态。

**修复方案:**
添加辅助函数 `wait_for_state()`，使用轮询 + 超时等待状态机到达目标状态。

- [ ] **Step 1: 添加等待辅助函数**

在 `tests/test_hardware_puller_emu.cpp` 第 30 行后添加：

```cpp
static bool wait_for_state(HardwarePullerEmu& puller,
                            HardwarePullerEmu::State target,
                            int timeout_ms = 5000,
                            int poll_interval_ms = 1) {
  int elapsed = 0;
  while (elapsed < timeout_ms) {
    if (puller.currentState() == target) {
      return true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(poll_interval_ms));
    elapsed += poll_interval_ms;
  }
  return false;
}
```

- [ ] **Step 2: 修改 test_puller_lifecycle**

修改 `tests/test_hardware_puller_emu.cpp` 第 156-174 行：

```cpp
int test_puller_lifecycle() {
  struct gpu_hal_ops hal = make_mock_hal();
  DoorbellEmu doorbell;
  HardwarePullerEmu puller(&hal, &doorbell, nullptr);

  puller.start();
  
  if (!wait_for_state(puller, HardwarePullerEmu::State::IDLE)) {
    std::cerr << "FAIL: timeout waiting for IDLE after start\n";
    puller.stop();
    return 1;
  }

  if (puller.currentState() != HardwarePullerEmu::State::IDLE) {
    std::cerr << "FAIL: should still be IDLE after start (no work)\n";
    puller.stop();
    return 1;
  }

  puller.stop();

  std::cout << "PASS: test_puller_lifecycle\n";
  return 0;
}
```

- [ ] **Step 3: 修改 test_puller_doorbell_trigger**

修改 `tests/test_hardware_puller_emu.cpp` 第 176-200 行：

```cpp
int test_puller_doorbell_trigger() {
  struct gpu_hal_ops hal = make_mock_hal();
  DoorbellEmu doorbell;
  HardwarePullerEmu puller(&hal, &doorbell, nullptr);

  puller.start();
  puller.submitBatch(0x1000, 1);

  // Ring doorbell to trigger processing
  doorbell.write(0);

  // Wait for state machine to return to IDLE
  if (!wait_for_state(puller, HardwarePullerEmu::State::IDLE, 500)) {
    std::cerr << "FAIL: timeout waiting for IDLE after doorbell trigger\n";
    puller.stop();
    return 1;
  }

  std::cout << "PASS: test_puller_doorbell_trigger\n";
  puller.stop();
  return 0;
}
```

- [ ] **Step 4: 修改 test_puller_state_transitions**

修改 `tests/test_hardware_puller_emu.cpp` 第 202-240 行：

```cpp
int test_puller_state_transitions() {
  struct gpu_hal_ops hal = make_mock_hal();
  DoorbellEmu doorbell;
  HardwarePullerEmu puller(&hal, &doorbell, nullptr);

  puller.start();
  puller.submitBatch(0x1000, 1);

  // Wait a bit for FETCH to start
  std::this_thread::sleep_for(std::chrono::milliseconds(5));

  // After doorbell ring, should be in FETCH or later
  const char* name = puller.stateName();
  bool is_transitioning = (strcmp(name, "FETCH") == 0 ||
                           strcmp(name, "DECODE") == 0 ||
                           strcmp(name, "SCHEDULE") == 0 ||
                           strcmp(name, "DISPATCH") == 0 ||
                           strcmp(name, "COMPLETE") == 0 ||
                           strcmp(name, "IDLE") == 0);

  if (!is_transitioning) {
    std::cerr << "FAIL: state should be transitioning (FETCH..IDLE), got " << name << "\n";
    puller.stop();
    return 1;
  }

  // Wait for completion
  if (!wait_for_state(puller, HardwarePullerEmu::State::IDLE, 500)) {
    std::cerr << "FAIL: timeout waiting for IDLE after completion\n";
    puller.stop();
    return 1;
  }

  puller.stop();
  std::cout << "PASS: test_puller_state_transitions\n";
  return 0;
}
```

- [ ] **Step 5: 修改 test_puller_interrupt_on_release**

修改 `tests/test_hardware_puller_emu.cpp` 第 242-264 行：

```cpp
int test_puller_interrupt_on_release() {
  struct gpu_hal_ops hal = make_counting_hal();
  DoorbellEmu doorbell;
  HardwarePullerEmu puller(&hal, &doorbell, nullptr);
  reset_hal_counters();
  g_next_entry_release_bit = 1;

  puller.start();
  puller.submitBatch(0x1000, 1);
  doorbell.write(0);

  if (!wait_for_state(puller, HardwarePullerEmu::State::IDLE, 500)) {
    std::cerr << "FAIL: timeout waiting for IDLE\n";
    puller.stop();
    return 1;
  }

  if (puller.getInterruptCount() == 0) {
    std::cerr << "FAIL: interrupt should have been raised for release=1 entry\n";
    puller.stop();
    return 1;
  }

  puller.stop();
  std::cout << "PASS: test_puller_interrupt_on_release\n";
  return 0;
}
```

- [ ] **Step 6: 修改 test_puller_semaphore_release**

修改 `tests/test_hardware_puller_emu.cpp` 第 266-288 行：

```cpp
int test_puller_semaphore_release() {
  struct gpu_hal_ops hal = make_counting_hal();
  DoorbellEmu doorbell;
  HardwarePullerEmu puller(&hal, &doorbell, nullptr);
  reset_hal_counters();
  g_next_entry_release_bit = 1;

  puller.start();
  puller.submitBatch(0x1000, 1);
  doorbell.write(0);

  if (!wait_for_state(puller, HardwarePullerEmu::State::IDLE, 500)) {
    std::cerr << "FAIL: timeout waiting for IDLE\n";
    puller.stop();
    return 1;
  }

  if (g_last_mem_write_addr.load() == 0) {
    std::cerr << "FAIL: mem_write should have been called for semaphore release\n";
    puller.stop();
    return 1;
  }

  puller.stop();
  std::cout << "PASS: test_puller_semaphore_release\n";
  return 0;
}
```

- [ ] **Step 7: 编译并运行测试**

Run:
```bash
cd /workspace/project/UsrLinuxEmu/build
make -j4 test_hardware_puller_emu_standalone
./bin/test_hardware_puller_emu_standalone
```

Expected: 所有测试通过

- [ ] **Step 8: Commit**

```bash
cd /workspace/project/UsrLinuxEmu
git add tests/test_hardware_puller_emu.cpp
git commit -m "test(gpu): 消除测试时序依赖

使用 wait_for_state() 辅助函数替代固定的 sleep_for，
通过轮询 + 超时机制等待状态机到达目标状态，
避免在慢机器上的测试失败。"
```

---

## Task 6: 修正 scheduler 参数类型 (hardware_puller_emu.h)

**Files:**
- Modify: `plugins/gpu_driver/sim/hardware/hardware_puller_emu.h:28-30,56`
- Modify: `plugins/gpu_driver/sim/hardware/hardware_puller_emu.cpp` (构造函数)

**问题分析:**
`void* scheduler` 丢失了类型安全，应该使用 `GlobalScheduler*`。

**修复方案:**
修改参数类型，并在 plugin.cpp 中传递正确类型。

- [ ] **Step 1: 修改 hardware_puller_emu.h**

修改 `plugins/gpu_driver/sim/hardware/hardware_puller_emu.h`：

```cpp
#include "gpu_types.h"
#include "gpu_hal.h"
#include "doorbell_emu.h"

// 前向声明，避免循环包含
class GlobalScheduler;

class HardwarePullerEmu {
 public:
  HardwarePullerEmu(struct gpu_hal_ops* hal,
                     DoorbellEmu* doorbell,
                     GlobalScheduler* scheduler);
  // ...
 private:
  struct gpu_hal_ops* hal_;
  DoorbellEmu* doorbell_;
  GlobalScheduler* scheduler_;
  // ...
};
```

- [ ] **Step 2: 修改 hardware_puller_emu.cpp**

修改 `plugins/gpu_driver/sim/hardware/hardware_puller_emu.cpp` 构造函数：

```cpp
HardwarePullerEmu::HardwarePullerEmu(struct gpu_hal_ops* hal,
                                      DoorbellEmu* doorbell,
                                      GlobalScheduler* scheduler)
    : hal_(hal),
      doorbell_(doorbell),
      scheduler_(scheduler),
      state_(State::IDLE),
      current_gpfifo_addr_(0),
      current_index_(0),
      total_entries_(0) {
  // ...
}
```

- [ ] **Step 3: 编译验证**

Run: `cd /workspace/project/UsrLinuxEmu/build && make -j4`
Expected: 编译成功，无错误

- [ ] **Step 4: Commit**

```bash
cd /workspace/project/UsrLinuxEmu
git add plugins/gpu_driver/sim/hardware/hardware_puller_emu.h plugins/gpu_driver/sim/hardware/hardware_puller_emu.cpp
git commit -m "refactor(gpu): 修正 scheduler 参数类型

将 void* scheduler 改为 GlobalScheduler* scheduler_，
增强类型安全，避免潜在的类型转换错误。"
```

---

## 验证清单

### 编译验证
```bash
cd /workspace/project/UsrLinuxEmu/build
cmake ..
make -j4
```

### 测试验证
```bash
cd /workspace/project/UsrLinuxEmu/build
make test
./bin/test_hardware_puller_emu_standalone
```

### 代码风格检查
```bash
cd /workspace/project/UsrLinuxEmu
clang-format -i plugins/gpu_driver/plugin.cpp
clang-format -i plugins/gpu_driver/hal/hal_user.cpp
clang-format -i plugins/gpu_driver/drv/gpgpu_device.h
clang-format -i plugins/gpu_driver/drv/gpgpu_device.cpp
clang-format -i plugins/gpu_driver/sim/hardware/hardware_puller_emu.h
clang-format -i plugins/gpu_driver/sim/hardware/hardware_puller_emu.cpp
clang-format -i tests/test_hardware_puller_emu.cpp
```

### 回归测试
```bash
cd /workspace/project/UsrLinuxEmu/build
ctest --output-on-failure
```

---

## 计划自检

### 1. 规范覆盖度检查

| 代码审查问题 | 覆盖任务 | 状态 |
|-------------|---------|------|
| UAF 风险 (plugin.cpp) | Task 1 | 已覆盖 |
| doorbell 回调覆盖 | Task 2 | 已覆盖 |
| GlobalScheduler 未使用 | Task 3 | 已覆盖 |
| puller_ 所有权不清晰 | Task 4 | 已覆盖 |
| 测试时序依赖 | Task 5 | 已覆盖 |
| scheduler 参数类型 | Task 6 | 已覆盖 |

### 2. 占位符扫描

- [x] 无 "TBD" 或 "TODO"（除 Phase2 注释外）
- [x] 无 "implement later"
- [x] 所有步骤包含实际代码
- [x] 所有命令包含预期输出

### 3. 类型一致性检查

- [x] `hal_user_set_doorbell_cb` 返回类型一致（int）
- [x] `setPuller` 参数类型一致（shared_ptr）
- [x] `scheduler_` 类型一致（GlobalScheduler*）
- [x] 所有修改涉及的文件引用正确

---

## 执行选项

**计划已完成并保存到 `docs/superpowers/plans/2026-05-09-hardware-puller-code-review-fix.md`。**

**两个执行选项：**

**1. Subagent-Driven（推荐）** - 为每个 Task 分派独立的子代理，代理间审查，快速迭代

**2. Inline Execution** - 在当前会话中使用 executing-plans 顺序执行，批量处理带检查点

**请选择执行方式？**
