# Tasks: stage3-3-errno-coverage-audit

> **状态**: ✅ COMPLETED（2026-07-21）
> **目标**: sim/ 层 12 处 `return -1` → 标准 Linux errno + 测试覆盖

## 实施清单

### 1. `sim/fence_id.cpp` — 1 处修复

- [x] 1.1 L54: `return -1` → `return -EINVAL`（无效 fence_id 范围）
- [x] 1.2 验证：`grep "return -1" plugins/gpu_driver/sim/fence_id.cpp` 返回 0

### 2. `sim/gpu_queue_emu.cpp` — 2 处修复

- [x] 2.1 L29: `return -1`（shm_addr 为空）→ `return -EFAULT`
- [x] 2.2 L35: `return -1`（mmap 映射失败）→ `return -EINVAL`
- [x] 2.3 验证：`grep "return -1" plugins/gpu_driver/sim/gpu_queue_emu.cpp` 返回 0

### 3. `sim/graph.cpp` — 5 处修复

- [x] 3.1 L184: `return -1` → `return -EINVAL`（graph_handle 未找到）
- [x] 3.2 L202: `return -1` → `return -EINVAL`（graph_handle 未找到, add_kernel_node）
- [x] 3.3 L223: `return -1` → `return -EINVAL`（graph_handle 未找到, add_memcpy_node）
- [x] 3.4 L240: `return -1` → `return -EINVAL`（graph_handle 未找到, instantiate）
- [x] 3.5 L292: `return -1` → `return -EINVAL`（exec_handle 未找到, destroy_exec）
- [x] 3.6 验证：`grep "return -1" plugins/gpu_driver/sim/graph.cpp` 返回 0

### 4. `sim/stream_capture.cpp` — 4 处修复

- [x] 4.1 L49: `return -1` → `return -EINVAL`（double-begin → INVALID 转换）
- [x] 4.2 L51: `return -1` → `return -EINVAL`（begin on INVALID state）
- [x] 4.3 L53: `return -1` → `return -ENOSYS`（unreachable 标注保留）
- [x] 4.4 L63: `return -1` → `return -EINVAL`（end on non-ACTIVE state）
- [x] 4.5 验证：`grep "return -1" plugins/gpu_driver/sim/stream_capture.cpp` 返回 0

### 5. 测试补充

- [x] 5.1 `tests/test_sim_errno_standalone.cpp`：8 个 Catch2 TEST_CASE 覆盖 12 个 errno 审计点
  - fence_id 无效 → `-EINVAL`
  - graph destroy/add_kernel/instantiate/destroy_exec 未知 handle → `-EINVAL`
  - stream_capture double-begin/end non-ACTIVE/null out → `-EINVAL`
- [x] 5.2 集成到 `tests/CMakeLists.txt` (CATCH2_SIM_TESTS 列表)
- [x] 5.3 `ctest --output-on-failure`：105/105 PASS（104 原有 + 1 新增 test_sim_errno_standalone）

### 6. 最终验收

- [x] 6.1 `grep -r "return -1" plugins/gpu_driver/sim/` 返回 0 匹配
- [x] 6.2 `ctest` 全绿（105/105 PASS）
- [x] 6.3 docs-audit 无新 warning（0 failed, 0 warnings）
- [x] 6.4 `openspec archive stage3-3-errno-coverage-audit`

### 7. 附加：测试期望值同步

- [x] 7.1 `tests/test_fence_id_lifecycle_standalone.cpp`: sim_fence_id_check 期望 `-EINVAL`
- [x] 7.2 `tests/test_sim_graph_standalone.cpp`: 5 个 REQUIRE 改为 `-EINVAL`
- [x] 7.3 `tests/test_sim_stream_capture_standalone.cpp`: 2 个 REQUIRE 改为 `-EINVAL`
- [x] 7.4 `tests/test_gpu_ringbuffer.cpp`: attachSharedMemory 期望 `-EFAULT`/`-EINVAL`

## Commit 列表

| Commit | 用途 |
|--------|------|
| `91dfd75` | fix(sim): replace bare return -1 with Linux errno |
| `8ef5361` | test(sim): update existing sim tests to expect Linux errno |
| `b0d9d90` | fix(test): restore missing opening brace |
| `c68f9d6` | fix(test): update test_gpu_ringbuffer attachSharedMemory expectations |
| `6bddffd` | test(sim): add test_sim_errno_standalone |
