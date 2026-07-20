# Tasks: stage3-3-errno-coverage-audit

> **状态**: 🔄 IN-PROGRESS（Reactivated 2026-07-21）
> **目标**: sim/ 层 12 处 `return -1` → 标准 Linux errno + 测试覆盖

## 实施清单

### 1. `sim/fence_id.cpp` — 1 处修复

- [ ] 1.1 L54: `return -1` → `return -EINVAL`（无效 fence_id 范围）
- [ ] 1.2 验证：`grep "return -1" plugins/gpu_driver/sim/fence_id.cpp` 返回 0

### 2. `sim/gpu_queue_emu.cpp` — 2 处修复

- [ ] 2.1 L29: `return -1`（shm_addr 为空）→ `return -ENOMEM`
- [ ] 2.2 L35: `return -1`（mmap 映射失败）→ `return -ENOMEM`
- [ ] 2.3 验证：`grep "return -1" plugins/gpu_driver/sim/gpu_queue_emu.cpp` 返回 0

### 3. `sim/graph.cpp` — 5 处修复

- [ ] 3.1 L184: `return -1` → `return -ENOMEM`（node 分配失败）
- [ ] 3.2 L202: `return -1` → `return -EINVAL`（无效 kernel_id）
- [ ] 3.3 L223: `return -1` → `return -ENOMEM`（exec 分配失败）
- [ ] 3.4 L240: `return -1` → `return -EINVAL`（无效操作类型）
- [ ] 3.5 L292: `return -1` → `return -ENOMEM`（资源分配失败）
- [ ] 3.6 验证：`grep "return -1" plugins/gpu_driver/sim/graph.cpp` 返回 0

### 4. `sim/stream_capture.cpp` — 4 处修复

- [ ] 4.1 L49: `return -1` → `return -ENOMEM`（buffer 为空）
- [ ] 4.2 L51: `return -1` → `return -EINVAL`（无效 stream_id）
- [ ] 4.3 L53: `return -1` → `return -ENOSYS`（reachable? unreachable 标注保留）
- [ ] 4.4 L63: `return -1` → `return -EINVAL`（无效 capture 状态）
- [ ] 4.5 验证：`grep "return -1" plugins/gpu_driver/sim/stream_capture.cpp` 返回 0

### 5. 测试补充

- [ ] 5.1 `tests/test_sim_errno_standalone.cpp`：≥ 5 个 Catch2 TEST_CASE 覆盖关键 error path
  - fence_id 无效 → expects `-EINVAL`
  - graph_create 参数无效 → expects `-EINVAL`
  - stream_capture buffer 为空 → expects `-ENOMEM`
  - gpu_queue_emu mmap 失败 → expects `-ENOMEM`
  - graph exec 分配失败 → expects `-ENOMEM`
- [ ] 5.2 集成到 `tests/CMakeLists.txt`
- [ ] 5.3 `ctest --output-on-failure`：104/104 PASS + N new test PASS

### 6. 最终验收

- [ ] 6.1 `grep -r "return -1" plugins/gpu_driver/sim/` 返回 0 匹配
- [ ] 6.2 `ctest` 全绿（104 + N 新测试）
- [ ] 6.3 docs-audit 无新 warning
- [ ] 6.4 `openspec archive stage3-3-errno-coverage-audit`