## Context

Stage 4.7.1 foundation change（commit `11a0a2b`）已在 `struct gpu_hal_ops` 上按 append-only 规则追加 7 个 graph fn-ptrs：

- `graph_create`
- `graph_destroy`
- `graph_add_kernel_node`
- `graph_add_memcpy_node`
- `graph_instantiate`
- `graph_launch`
- `graph_destroy_exec`

`gpu_hal.h` 同时提供对应的零开销 inline wrappers `hal_graph_*`，`hal_user_init()` 将这些 fn-ptrs 委托给 `sim_graph_*`。当前 ② drv 层仍在以下文件直接包含 `sim/graph.h` 并调用 `sim_graph_*`：

- `plugins/gpu_driver/drv/gpgpu_device.cpp`
- `plugins/gpu_driver/drv/gpu_drm_driver.cpp`

这两个直接 include 构成 2 个 L2 违规。本 change 是 Stage 4.7.2 Phase 2 B-class 的第一个 removal，完成后 L2 违规计数从 9 降至 7，并验证 foundation 模式可端到端工作。

**架构依据：**

- **ADR-072 §Decision 4 revised** — B-class 使用 1 个 foundation + N 个 removals 的修复路径
- **ADR-023 §Decision 4** — `struct gpu_hal_ops` 只能 append-only 扩展；本 change 不新增或重排 fn-ptrs
- **ADR-023 §Decision 5** — ② 驱动代码仅通过 HAL fn-ptrs 访问 ③ sim

## Goals / Non-Goals

**Goals:**

- 从 `gpgpu_device.cpp` 和 `gpu_drm_driver.cpp` 移除 `#include "sim/graph.h"`
- 将两个 drv 文件中的全部 `sim_graph_*` 调用 1:1 迁移到对应的 `hal_graph_*` inline wrapper
- 保持调用顺序、参数、错误处理路径和性能特性不变
- 通过测试证明 drv graph 路径经 HAL 间接到达 sim，且 `test_sim_graph_standalone` 继续通过
- 将 L2 违规计数从 9 降至 7，同时保持 `struct gpu_hal_ops` fn-ptr 总数为 46
- 完整 ctest 130/130 PASS，docs-audit PASS

**Non-Goals:**

- 不修改 `sim/graph.h`、`sim/graph.cpp` 或 `hal_user.cpp` 中已 ship 的 foundation 实现
- 不新增、删除或重排 `struct gpu_hal_ops` fn-ptrs
- 不处理 mem_pool、stream_capture、gpu_queue_emu、hardware_puller_emu 的后续 removal
- 不处理 `kfd_events.c` 对 `sim_event.h` 的独立违规
- 不补强 foundation stub；若发现缺口，另建 follow-up change

## Approach

### Step 1: 先建立 HAL 路径回归约束

在 `tests/test_sim_graph_standalone.cpp` 增加或调整覆盖，验证 drv graph 操作通过 HAL graph 接口执行，保留现有 sim graph 独立行为测试。先运行目标测试确认新增约束能暴露当前直接 sim 路径或缺失的 HAL 路径证明。

### Step 2: 迁移 `gpgpu_device.cpp`

- 移除 `#include "sim/graph.h"`
- 将文件内所有 `sim_graph_*` 调用按函数名和参数 1:1 替换为对应 `hal_graph_*` wrapper，并传入现有 HAL 实例
- 不改变原有调用顺序、返回值处理或错误路径

### Step 3: 迁移 `gpu_drm_driver.cpp`

- 移除 `#include "sim/graph.h"`
- 将文件内所有 `sim_graph_*` 调用按函数名和参数 1:1 替换为对应 `hal_graph_*` wrapper，并传入现有 HAL 实例
- 不改变原有调用顺序、返回值处理或错误路径

### Step 4: 静态边界与 ABI 验证

- 确认 `drv/` 中不再直接包含 `sim/graph.h`，也不再直接调用 `sim_graph_*`
- 确认 `grep -rn '#include.*"sim/' plugins/gpu_driver/drv/` 恰好输出 7 行（9 - 2）
- 确认 `struct gpu_hal_ops` fn-ptr 总数仍为 46，证明 append-only foundation 契约未被本 removal 改动

### Step 5: 回归与文档门禁

- 运行 `test_sim_graph_standalone`，验证 sim graph 行为保持不变
- 运行完整 ctest，要求 130/130 PASS
- 运行 docs-audit，要求 PASS

## Risks

| Risk | Likelihood | Mitigation |
|------|------------|------------|
| `hal_graph_*` wrapper 与原 `sim_graph_*` 参数签名不一致 | Low | foundation commit `11a0a2b` 已提供 7 个一一对应的 fn-ptrs 与 wrappers；编译和目标测试立即暴露不匹配 |
| 某个 drv graph 调用遗漏迁移 | Medium | 对两个 in-scope 文件执行静态检查，要求 `sim_graph_*` 直接调用为 0，且 `sim/graph.h` include 为 0 |
| HAL 间接调用改变错误传播或调用顺序 | Low | 严格执行 1:1 替换，不重构周边控制流，并以现有 graph 测试和完整 ctest 验证等价行为 |
| 误改 HAL ABI | Low | 本 change 禁止修改 foundation 实现，并验证 `struct gpu_hal_ops` fn-ptr 总数保持 46 |
| L2 计数受其他并行变更影响 | Low | 以本 change 基线 9 → 7 为验收口径；静态命令必须恰好输出 7 行 |
