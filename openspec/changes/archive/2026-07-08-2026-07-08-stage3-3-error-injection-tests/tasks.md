# Tasks: stage3-3-errno-and-error-injection

> **状态**: 📋 PROPOSED → 🔄 READY
> **目标**: errno 审计 + 32 handler × 4 errno 矩阵 error injection 测试
> **设计决策**: hal_mock_state 注入（非 env-var）、58 cases / 84 assertions 覆盖、opt-in CI

---

## Step 0: 准备工作站（30 分钟）

- [ ] 0.1 确认 C-02 已完成（main HEAD 已包含 kNumIoctls=32 + 全部 20 个 Phase 3/4 handler）
- [ ] 0.2 `git pull && git submodule update --init` 到最新 main
- [ ] 0.3 ctest 全量确认 baseline 85/85 pass
- [ ] 0.4 docs-audit --strict 确认 baseline 43/43 pass, 0 warning

## Step 1: test_gpu_plugin 扩展 — 无效参数注入（1 天）

把以下 test case 追加到 `tests/test_gpu_plugin.cpp`（遵循现有 `TEST_CASE_METHOD(GpuPluginTestFixture, ...)` 模式）：

### 1A: Stream Capture error paths（3 cases）

```cpp
// STREAM_CAPTURE_BEGIN: stream_id=0xFF
// STREAM_CAPTURE_END: bogus stream_id
// STREAM_CAPTURE_STATUS: stream_id=0 (never opened)
```
- [ ] 1.1 `GPU_IOCTL_STREAM_CAPTURE_BEGIN invalid stream_id` → expect -EINVAL
- [ ] 1.2 `GPU_IOCTL_STREAM_CAPTURE_END bogus stream_id` → expect -EINVAL
- [ ] 1.3 `GPU_IOCTL_STREAM_CAPTURE_STATUS bogus stream_id` → expect -EINVAL

### 1B: Graph error paths（5 cases）

```cpp
// GRAPH_DESTROY: graph_handle=0
// GRAPH_ADD_KERNEL_NODE: graph_handle=0
// GRAPH_ADD_MEMCPY_NODE: graph_handle=0
// GRAPH_INSTANTIATE: graph_handle=0
// GRAPH_LAUNCH: exec_handle=0
// GRAPH_DESTROY_EXEC: exec_handle=0
```
- [ ] 1.4 `GPU_IOCTL_GRAPH_DESTROY zero handle` → expect -EINVAL
- [ ] 1.5 `GPU_IOCTL_GRAPH_ADD_KERNEL_NODE zero graph handle` → expect -EINVAL
- [ ] 1.6 `GPU_IOCTL_GRAPH_ADD_MEMCPY_NODE zero graph handle` → expect -EINVAL
- [ ] 1.7 `GPU_IOCTL_GRAPH_INSTANTIATE zero graph handle` → expect -EINVAL
- [ ] 1.8 `GPU_IOCTL_GRAPH_LAUNCH zero exec handle` → expect -EINVAL
- [ ] 1.9 `GPU_IOCTL_GRAPH_DESTROY_EXEC zero exec handle` → expect -EINVAL

### 1C: Mem Pool error paths（9 cases）

```cpp
// MEM_POOL_CREATE: props.size=0
// MEM_POOL_DESTROY: pool_handle=0
// MEM_POOL_ALLOC: pool_handle=0
// MEM_POOL_ALLOC_ASYNC: pool_handle=0
// MEM_POOL_FREE_ASYNC: va=0
// MEM_POOL_SET_ATTR: attr=0xFF (invalid enum) → -ENOSYS
// MEM_POOL_GET_ATTR: attr=0xFF → -ENOSYS
// MEM_POOL_TRIM: pool_handle=0
// MEM_POOL_EXPORT: pool_handle=0
```
- [ ] 1.10 `GPU_IOCTL_MEM_POOL_CREATE zero size` → expect -EINVAL
- [ ] 1.11 `GPU_IOCTL_MEM_POOL_DESTROY zero pool` → expect -EINVAL
- [ ] 1.12 `GPU_IOCTL_MEM_POOL_ALLOC zero pool` → expect -EINVAL
- [ ] 1.13 `GPU_IOCTL_MEM_POOL_ALLOC_ASYNC zero pool` → expect -EINVAL
- [ ] 1.14 `GPU_IOCTL_MEM_POOL_FREE_ASYNC zero va` → expect -EINVAL
- [ ] 1.15 `GPU_IOCTL_MEM_POOL_SET_ATTR invalid attr` → expect -ENOSYS
- [ ] 1.16 `GPU_IOCTL_MEM_POOL_GET_ATTR invalid attr` → expect -ENOSYS
- [ ] 1.17 `GPU_IOCTL_MEM_POOL_TRIM zero pool` → expect -EINVAL
- [ ] 1.18 `GPU_IOCTL_MEM_POOL_EXPORT zero pool` → expect -EINVAL

## Step 2: 新建 test_error_inject_standalone.cpp — HAL mock 注入（1 天）

### 2.1: 创建测试文件（在 `tests/` 下）

文件结构：

```
tests/
  test_error_inject_standalone.cpp     ← 主测试文件，Catch2 TEST_CASE
```

不使用 fixture（不走 VFS.open + ModuleLoader）——直接构造 `GpgpuDevice` + mock HAL：

```cpp
// Catch2 TEST_CASE pattern
#define CATCH_CONFIG_MAIN
#include "catch_amalgamated.hpp"
#include "gpu_driver/drv/gpgpu_device.h"
#include "gpu_driver/hal/gpu_hal.h"
#include "gpu_driver/hal/hal_mock.h"
#include "gpu_driver/shared/gpu_ioctl.h"

using namespace usr_linux_emu;

struct HalInjectionFixture {
  struct gpu_hal_ops hal;
  struct hal_mock_state state;
  std::shared_ptr<GpgpuDevice> device;

  HalInjectionFixture() {
    hal_mock_init(&hal, &state);
    device = std::make_shared<GpgpuDevice>(&hal);
  }

  long ioctl(unsigned long request, void* arg) {
    return device->ioctl(0, request, arg);
  }
};
```

### 2.2: 注入 case 列表

- [ ] 2.1 `HAL_INJECT ALLOC_BO mem_alloc fails` → `state.mem_alloc_result = -ENOMEM` → expect -ENOMEM
- [ ] 2.2 `HAL_INJECT ALLOC_BO mem_alloc + handle alloc both fail` → `state.mem_alloc_result = -ENOMEM` + trigger handle exhaust → expect -ENOMEM
- [ ] 2.3 `HAL_INJECT FREE_BO mem_free fails` → `state.mem_free_result = -ENOMEM` → expect -ENOMEM
- [ ] 2.4 `HAL_INJECT PUSHBUFFER_SUBMIT_BATCH fence_create fails` → `state.fence_create_result = -ENOMEM` → expect -ENOMEM
- [ ] 2.5 `HAL_INJECT CREATE_QUEUE handle alloc fails` → 循环 allocate 到耗尽 → expect -ENOMEM
- [ ] 2.6 `HAL_INJECT WAIT_FENCE fence_read fails` → `state.fence_read_result = -EINVAL` → expect -EINVAL

### 2.3: 清理

每个 TEST_CASE 结束时：
- [ ] 验证 `hal_mock_state` 的调用计数 `*_count` 递增（证明 HAL 被调用了）
- [ ] fixture 析构自动清理

## Step 3: CMake 注册（30 分钟）

- [ ] 3.1 `tests/CMakeLists.txt` 加 `test_error_inject_standalone` 目标：

```cmake
set(CATCH2_GPU_TESTS
    ...
    test_error_inject_standalone.cpp
)
```

- [ ] 3.2 确认 `gpu_hal_mock` 库已在 test target 的 link 路径上（Catch2 框架负责 link `gpu_driver_plugin` → 需确保 mock 可访问）

→ 需要新增的 link 依赖：

```cmake
target_link_libraries(test_error_inject_standalone PRIVATE
    gpu_driver_plugin
    gpu_hal_mock
    kernel
    Catch2::Catch2WithMain
)
```

### 3.1: 注意 CMake 依赖

`test_error_inject_standalone` 链接的是 `gpu_driver_plugin`（即 `plugin_gpu_driver.so` 的代码）但不走 `dlopen`。需要用 `gpu_hal_mock` 替代 `gpu_hal_user`。

需要检查：
- `gpu_driver_plugin` target 是否强制 link `hal_user`？ → 若是，需将 `hal_user` 设为可选，让 test binary 可选 `hal_mock`。
- 或者：用 `target_sources` 直接包含 `drv/gpgpu_device.cpp` + `hal_mock` 而非整个 plugin target。

→ 在 Step 3 实施时根据实际 CMake 结构做选择。

## Step 4: errno 审计收尾（半天）

- [ ] 4.1 确认 `gpgpu_device.cpp` 中无 `return -1` 残留（已由 `fc6f854` 修）
- [ ] 4.2 确认 sim 层函数（`sim_stream_capture_*`、`sim_graph_*`、`sim_mem_pool_*`）返回标准 errno
- [ ] 4.3 生成 errno 矩阵表格 doc：

```
| handler | -EFAULT | -EINVAL | -ENOMEM | -ENOSYS | -ETIMEDOUT |
|---------|---------|---------|---------|---------|------------|
| ALLOC_BO | ✅ null | ✅ domain=0 | ✅ hal_mem_alloc fail / handle full | n/a | n/a |
| STREAM_CAPTURE_BEGIN | ✅ null | ✅ bogus stream | f" | n/a | n/a |
| ... | | | | | |
```
- [ ] 4.4 将矩阵 doc 输出到 `tests/error-inject-coverage.md`（或提案附录）

## Step 5: 验证（半天）

- [ ] 5.1 构建：`cd build && cmake .. && make -j4` → compile clean
- [ ] 5.2 新增测试：`./bin/test_error_inject_standalone` → 全部 PASS
- [ ] 5.3 扩展后的 plugin 测试：`./bin/test_gpu_plugin -s` → 全部 PASS（原有 34 + 新增 18 = 52 cases）
- [ ] 5.4 全量：`ctest --output-on-failure` → 全部 PASS（85 + 1 新 = 86 binaries）
- [ ] 5.5 `bash tools/docs-audit.sh --strict` → 43/43 PASS, 0 warnings

## Step 6: 提交（30 分钟）

- [ ] 6.1 `git add` 所有变更（tests/ + CMakeLists.txt）
- [ ] 6.2 多个 atomic commits：

```bash
# 1. errno audit 矩阵 + doc
git commit -m "docs(gpu): errno coverage matrix for Phase 3 IOCTL handlers

Documents all 32 ioctl handlers × {EFAULT, EINVAL, ENOMEM, ENOSYS}
error path coverage per Issue #24 §3.3."

# 2. test_gpu_plugin error-path cases
git commit -m "test(gpu): add 18 error-path test cases for Phase 3/4 IOCTLs

Covers stream_capture/graph/mem_pool invalid params:
- -EINVAL for zero/bogus handles
- -ENOSYS for invalid MEM_POOL_SET/GET_ATTR attr"

# 3. new test_error_inject_standalone binary
git commit -m "test(gpu): add HAL-mock error injection test binary

Links gpu_hal_mock to control mem_alloc/fence_create/fence_read
failures through the GpgpuDevice ioctl path.
Adds 6 HAL injection cases covering -ENOMEM/-EINVAL paths."

# 4. CMake registration
git commit -m "build(tests): register test_error_inject_standalone

New binary linked against gpu_driver_plugin + gpu_hal_mock +
kernel + Catch2."
```

## 增量计数

| 指标 | 新值 |
|------|------|
| test_gpu_plugin test cases | 34 → 52 (+18) |
| 新二进制 | test_error_inject_standalone (6 cases) |
| 总 ctest 二进制数 | 85 → 86 |
| 覆盖 assertion | ~84 new |
| 代码行（测试） | ~750 new |
