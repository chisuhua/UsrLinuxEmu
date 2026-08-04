## Context

Stage 4.7.1 foundation change（commit `11a0a2b`）已在 `struct gpu_hal_ops` 上按 append-only 规则追加 9 个 mem_pool fn-ptrs（call site 最多的一组）：

- `mem_pool_create`
- `mem_pool_destroy`
- `mem_pool_alloc`
- `mem_pool_alloc_async`
- `mem_pool_free`
- `mem_pool_free_async`
- `mem_pool_set_attr`
- `mem_pool_get_attr`
- `mem_pool_trim`

`gpu_hal.h` 同时提供对应的零开销 inline wrappers `hal_mem_pool_*`。Foundation 阶段的两个关键签名约束需要在本 change 兼容：

- `mem_pool_set_attr` / `mem_pool_get_attr` 使用 `const void* value, uint64_t value_size`（与 `sim_mem_pool_set_attr` 实际签名匹配），不是 `uint64_t`
- `mem_pool_free` 是 stub（sim 中无对应函数；sim 用 `sim_mem_pool_destroy` 释放整 pool）
- `mem_pool_alloc_async` / `mem_pool_free_async` 是 stub（sim 中为占位）

当前 ② drv 层在以下文件直接包含 `sim/mem_pool.h` 并调用 27 处 `sim_mem_pool_*`：

- `plugins/gpu_driver/drv/gpgpu_device.cpp`（约 15 处）
- `plugins/gpu_driver/drv/gpu_drm_driver.cpp`（约 12 处）

本 change 是 Stage 4.7.2 Phase 2 B-class 的第二个 removal，在 `removal-graph` 验证 foundation 模式后执行。call site 数量翻倍但模式相同：每个 `sim_mem_pool_X(args)` 替换为 `hal_mem_pool_X(hal_, args)`，仅 set_attr/get_attr 需要将 typed buffer + size 作为参数传入。完成后 L2 违规计数从 7 降至 5。

**架构依据：**

- **ADR-072 §Decision 4 revised** — B-class 使用 1 个 foundation + N 个 removals 的修复路径
- **ADR-023 §Decision 4** — `struct gpu_hal_ops` 只能 append-only 扩展；本 change 不新增或重排 fn-ptrs
- **ADR-023 §Decision 5** — ② 驱动代码仅通过 HAL fn-ptrs 访问 ③ sim

## Goals / Non-Goals

**Goals:**

- 从 `gpgpu_device.cpp` 和 `gpu_drm_driver.cpp` 移除 `#include "sim/mem_pool.h"`
- 将两个 drv 文件中的 27 处 `sim_mem_pool_*` 调用 1:1 迁移到对应的 `hal_mem_pool_*` inline wrappers
- 处理 `mem_pool_set_attr` / `mem_pool_get_attr` 的 `void* + size` 签名差异：drv 侧调用方需提供 typed buffer + `sizeof(*buffer)`
- `mem_pool_free` 等 stub 行为通过 HAL wrapper 透明传递（drv 期望 no-op 返回 0，stub 满足该语义）
- 保持调用顺序、参数、错误处理路径和性能特性不变
- 通过测试证明 drv mem_pool 路径经 HAL 间接到达 sim
- 将 L2 违规计数从 7 降至 5
- 完整 ctest 130/130 PASS，docs-audit PASS

**Non-Goals:**

- 不修改 `sim/mem_pool.h`、`sim/mem_pool.cpp`、`hal_user.cpp` 或 `hal_mock.cpp` 中已 ship 的 foundation 实现
- 不实现 `sim_mem_pool_free` / `sim_mem_pool_alloc_async` / `sim_mem_pool_free_async`（foundation 阶段已 stub，真实化是此 change 范畴之外的 sim 增强）
- 不新增、删除或重排 `struct gpu_hal_ops` fn-ptrs
- 不处理 graph、stream_capture、gpu_queue_emu、hardware_puller_emu 的其他 removal
- 不处理 `kfd_events.c` 对 `sim_event.h` 的独立违规

## Approach

### Step 1: 先建立 HAL 路径回归约束

在相关 mem_pool 测试中增加或调整覆盖，验证 drv mem_pool 操作通过 HAL mem_pool 接口执行；保留现有 sim mem_pool 独立行为测试。先运行目标测试确认新增约束在迁移前能够暴露直接 `sim_mem_pool_*` 路径或缺失的 HAL 路径证明。

### Step 2: 迁移 `gpgpu_device.cpp` 中的 mem_pool 调用

- 移除 `#include "sim/mem_pool.h"`
- 将文件内约 15 处 `sim_mem_pool_*` 调用按函数名和参数 1:1 替换为对应 `hal_mem_pool_*` wrapper
- `mem_pool_set_attr` / `mem_pool_get_attr` 调用需将原 `uint64_t val` / `uint64_t* out` 改为 typed buffer + `sizeof(*buffer)`（编译期类型检查）
- 不改变原有调用顺序、返回值处理或错误路径

### Step 3: 迁移 `gpu_drm_driver.cpp` 中的 mem_pool 调用

- 移除 `#include "sim/mem_pool.h"`
- 将文件内约 12 处 `sim_mem_pool_*` 调用按函数名和参数 1:1 替换为对应 `hal_mem_pool_*` wrapper
- 处理 set_attr/get_attr 的签名差异
- 不改变原有调用顺序、返回值处理或错误路径

### Step 4: 静态边界与 ABI 验证

- 确认 `drv/` 中不再直接包含 `sim/mem_pool.h`，也不再直接调用 `sim_mem_pool_*`
- 确认 `grep -rn '#include.*"sim/' plugins/gpu_driver/drv/` 恰好输出 5 行（7 - 2）
- 确认 `struct gpu_hal_ops` fn-ptr 总数仍为 46，证明 append-only foundation 契约未被本 removal 改动

### Step 5: 回归与文档门禁

- 运行 mem_pool 相关测试，验证 sim mem_pool 行为保持不变
- 运行完整 ctest，要求 130/130 PASS
- 运行 docs-audit，要求 PASS

## Risks

| Risk | Likelihood | Mitigation |
|------|------------|------------|
| `hal_mem_pool_set_attr` / `hal_mem_pool_get_attr` 签名与 drv 原 `uint64_t val` 期望不匹配 | Medium | 编译器和 lint 会立即暴露；将 typed buffer + size 显式传入并执行 mem_pool 行为测试 |
| `mem_pool_free` 在 foundation 阶段是 stub（no-op 返回 0），drv 期望该语义但真实释放可能后续由独立 change 真实化 | Low | 本 change 维持现有 stub 行为；若后续真实化，跟进独立 follow-up change |
| 27 个 call site 中有遗漏迁移 | Medium | 对两个 in-scope 文件执行静态检查，要求 `sim_mem_pool_*` 直接调用为 0，且 `sim/mem_pool.h` include 为 0 |
| 间接或回调模式 call site 与直接调用模式签名不同 | Medium | 优先迁移直接调用；间接/回调模式 call site 单独处理并按 1:1 包装策略记录 |
| HAL 间接调用改变错误传播或调用顺序 | Low | 严格执行 1:1 替换，不重构周边控制流，并以现有 mem_pool 测试和完整 ctest 验证等价行为 |
| L2 计数受其他并行变更影响 | Low | 以本 change 基线 7 → 5 为验收口径；静态命令必须恰好输出 5 行 |
