## Context

Stage 4.7.1 foundation change（commit `11a0a2b`）已在 `struct gpu_hal_ops` 上按 append-only 规则追加 3 个 stream_capture fn-ptrs（最简单的一组，纯 C 函数，无 class 类型或 opaque handle 复杂度）：

- `stream_capture_begin`
- `stream_capture_end`
- `stream_capture_status`

`gpu_hal.h` 同时提供对应的零开销 inline wrappers `hal_stream_capture_*`。

Foundation 阶段的唯一约束：原 `sim_stream_capture_status` 使用 `sim_stream_capture_status_t*`（C++ enum 类型）；foundation 阶段用 `reinterpret_cast` 以 `uint32_t*` pass-through（layout 兼容，single uint32_t 字段）。drv 调用经 HAL wrapper 时也使用 `uint32_t*` layout 兼容 pass-through。

当前 ② drv 层在以下文件直接包含 `sim/stream_capture.h` 并调用 8 处 `sim_stream_capture_*`：

- `plugins/gpu_driver/drv/gpgpu_device.cpp`（约 4 处）
- `plugins/gpu_driver/drv/gpu_drm_driver.cpp`（约 4 处）

本 change 是 Stage 4.7.2 Phase 2 B-class 的第三个 removal（在 `removal-graph` + `removal-mem-pool` 之后）。3 个 fn-ptrs 全部为 C 函数，是最低风险的 removal。完成后 L2 违规计数从 5 降至 3。

**架构依据：**

- **ADR-072 §Decision 4 revised** — B-class 使用 1 个 foundation + N 个 removals 的修复路径
- **ADR-023 §Decision 4** — `struct gpu_hal_ops` 只能 append-only 扩展；本 change 不新增或重排 fn-ptrs
- **ADR-023 §Decision 5** — ② 驱动代码仅通过 HAL fn-ptrs 访问 ③ sim

## Goals / Non-Goals

**Goals:**

- 从 `gpgpu_device.cpp` 和 `gpu_drm_driver.cpp` 移除 `#include "sim/stream_capture.h"`
- 将两个 drv 文件中的 8 处 `sim_stream_capture_*` 调用 1:1 迁移到对应的 `hal_stream_capture_*` inline wrappers
- 保持 `sim_stream_capture_status` 的 status 参数 pass-through 语义（`uint32_t*` layout 兼容）
- 保持调用顺序、参数、错误处理路径和性能特性不变
- 通过测试证明 drv stream_capture 路径经 HAL 间接到达 sim
- 将 L2 违规计数从 5 降至 3
- 完整 ctest 130/130 PASS，docs-audit PASS

**Non-Goals:**

- 不修改 `sim/stream_capture.h`、`sim/stream_capture.cpp`、`hal_user.cpp` 或 `hal_mock.cpp` 中已 ship 的 foundation 实现
- 不调整 `sim_stream_capture_status_t` 类型本身（仍留在 sim/，通过 layout 兼容 pass-through）
- 不新增、删除或重排 `struct gpu_hal_ops` fn-ptrs
- 不处理 graph、mem_pool、gpu_queue_emu、hardware_puller_emu 的其他 removal
- 不处理 `kfd_events.c` 对 `sim_event.h` 的独立违规

## Approach

### Step 1: 先建立 HAL 路径回归约束

在相关 stream_capture 测试中增加或调整覆盖，验证 drv stream_capture 操作通过 HAL stream_capture 接口执行；保留现有 sim stream_capture 独立行为测试。先运行目标测试确认新增约束在迁移前能够暴露直接 `sim_stream_capture_*` 路径或缺失的 HAL 路径证明。

### Step 2: 迁移 `gpgpu_device.cpp` 中的 stream_capture 调用

- 移除 `#include "sim/stream_capture.h"`
- 将文件内约 4 处 `sim_stream_capture_*` 调用按函数名和参数 1:1 替换为对应 `hal_stream_capture_*` wrapper
- `sim_stream_capture_status` 调用保留 `uint32_t*` layout 兼容 pass-through（经 HAL wrapper 透明传递）
- 不改变原有调用顺序、返回值处理或错误路径

### Step 3: 迁移 `gpu_drm_driver.cpp` 中的 stream_capture 调用

- 移除 `#include "sim/stream_capture.h"`
- 将文件内约 4 处 `sim_stream_capture_*` 调用按函数名和参数 1:1 替换为对应 `hal_stream_capture_*` wrapper
- 保持 `uint32_t*` layout 兼容 pass-through
- 不改变原有调用顺序、返回值处理或错误路径

### Step 4: 静态边界与 ABI 验证

- 确认 `drv/` 中不再直接包含 `sim/stream_capture.h`，也不再直接调用 `sim_stream_capture_*`
- 确认 `grep -rn '#include.*"sim/' plugins/gpu_driver/drv/` 恰好输出 3 行（5 - 2）
- 确认 `struct gpu_hal_ops` fn-ptr 总数仍为 46，证明 append-only foundation 契约未被本 removal 改动

### Step 5: 回归与文档门禁

- 运行 stream_capture 相关测试，验证 sim stream_capture 行为保持不变
- 运行完整 ctest，要求 130/130 PASS
- 运行 docs-audit，要求 PASS

## Risks

| Risk | Likelihood | Mitigation |
|------|------------|------------|
| `sim_stream_capture_status` 在 drv 调用方使用 `sim_stream_capture_status_t*` 而非 `uint32_t*` | Low | foundation 阶段已通过 `reinterpret_cast` 强制 `uint32_t*` pass-through；drv 跟随相同模式 |
| `hal_stream_capture_*` wrapper 与原 `sim_stream_capture_*` 参数签名不一致 | Low | foundation commit `11a0a2b` 已提供 3 个一一对应的 fn-ptrs 与 wrappers；编译和目标测试立即暴露不匹配 |
| 8 个 call site 中有遗漏迁移 | Low | 对两个 in-scope 文件执行静态检查，要求 `sim_stream_capture_*` 直接调用为 0，且 `sim/stream_capture.h` include 为 0 |
| HAL 间接调用改变错误传播或调用顺序 | Low | 严格执行 1:1 替换，不重构周边控制流，并以现有 stream_capture 测试和完整 ctest 验证等价行为 |
| L2 计数受其他并行变更影响 | Low | 以本 change 基线 5 → 3 为验收口径；静态命令必须恰好输出 3 行 |
