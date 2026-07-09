# Change: sim-fence-id-comments-ssot

> **状态**: ✅ COMPLETED（2026-07-09，commit `e4b3378` 已合并）
> **优先级**: 🟢 P3（纯注释 hygiene，非阻塞）
> **创建**: 2026-07-09
> **来源**: `sim-fence-id-base-cleanup` (C-03) 归档时的 Metis follow-up 建议
> **依赖**: 无（`sim-fence-id-base-cleanup` 已归档）
> **工作目录**: ~~`openspec/changes/2026-07-09-sim-fence-id-comments-ssot/`~~ → `openspec/changes/archive/2026-07-09-sim-fence-id-comments-ssot/`
> **完成证据**: `e4b3378 docs(sim+openspec): fence_id SSOT 化收尾 — 实施 C-13 + 归档 C-03`
> **测试通过**: 86/86 ctest PASS, 0 编译 warnings

## Why

`sim-fence-id-base-cleanup`（C-03）已经把 `gpu_drm_driver.cpp` + `gpgpu_device.cpp` 的代码层 fence_id 比较从 `(1ULL << 32)` 替换为 `SIM_FENCE_ID_BASE` 宏（见 `13477ff` + `7740a75`），但 **sim 层的 SSOT 注释仍以 magic 字面量叙述 fence_id 范围**：

- `plugins/gpu_driver/sim/fence_id.h:6-9`：背景段写 `[1, (1<<32) - 1]` 和 `[(1<<32), INT64_MAX]`
- `plugins/gpu_driver/sim/fence_id.h:21`：Range constants 注释写 `[(1<<32), INT64_MAX]`
- `plugins/gpu_driver/sim/fence_id.cpp:5`：设计注释写 `SIM_FENCE_ID_BASE (1<<32)`

**问题**：未来若有人提议把 fence 边界从 `2^32` 调整到 `2^40`，注释里的 `(1<<32)` 与 `(1ULL << 32)` 不一致会导致 reader 困惑（drv 代码已无此歧义）。**SSOT 不完整：** `SIM_FENCE_ID_BASE` 是 single source of truth，但 sim 层注释反向引入了字面量叙事。

> 注：本次只动 SSOT 文件（`fence_id.h` + `fence_id.cpp`），**drv/test/README 注释里的字面量叙事不在范围内**（它们是叙述性，与 SSOT 不强耦合；如需统一可开后续 cleanup change）。

## What Changes

### 替换清单

| 文件 | 行 | 旧 | 新 |
|------|----|----|----|
| `sim/fence_id.h` | 6-8 | `背景：现有 HAL 层 (hal_fence_create) 分配 driver 层 fence_id，范围 [1, (1<<32) - 1]` | `背景：现有 HAL 层 (hal_fence_create) 分配 driver 层 fence_id，范围 [1, SIM_FENCE_ID_BASE - 1]` |
| `sim/fence_id.h` | 8 | `范围 [(1<<32), INT64_MAX]` | `范围 [SIM_FENCE_ID_BASE, SIM_FENCE_ID_MAX]` |
| `sim/fence_id.h` | 21 | `/* Range constants (Fix-1: sim 层 fence_id 范围 [(1<<32), INT64_MAX]) */` | `/* Range constants (Fix-1: sim 层 fence_id 范围 [SIM_FENCE_ID_BASE, SIM_FENCE_ID_MAX]) */` |
| `sim/fence_id.cpp` | 5 | `next_sim_fence_id_ 原子计数器，从 SIM_FENCE_ID_BASE (1<<32) 开始` | `next_sim_fence_id_ 原子计数器，从 SIM_FENCE_ID_BASE 开始` |

### 范围不变量

- **不改宏定义**（`#define SIM_FENCE_ID_BASE (1ULL << 32)` 与 `#define SIM_FENCE_ID_MAX INT64_MAX` 保持字面量形式 — 这是定义点，字面量合法）
- **不改 sim 层 fence 检查代码**（`fence_id < SIM_FENCE_ID_BASE` / `> static_cast<uint64_t>(SIM_FENCE_ID_MAX)` 已用宏）
- **不改 drv/test/README 注释**（见 Why 段末）

## Acceptance

- [ ] `grep -RIn --include="*.h" --include="*.cpp" "(1<<32)\|INT64_MAX" plugins/gpu_driver/sim/fence_id.{h,cpp}` 仅命中 `#define SIM_FENCE_ID_BASE (1ULL << 32)` 与 `#define SIM_FENCE_ID_MAX INT64_MAX`（定义点合法）
- [ ] 上述 4 处替换全部完成
- [ ] `ctest -R fence_id` PASS（断言 fence 边界行为不退化）
- [ ] `ctest -R phase31` PASS（kfd_phase31 验证 ioctl 派发不变)
- [ ] `ctest` 全绿
- [ ] 0 编译警告

## 测试方法

```bash
cd build && cmake .. && make -j4
ctest -R "fence_id|phase31"
ctest                                      # 完整回归
# 注释 SSOT 专项验证
grep -RIn "(1<<32)\|INT64_MAX" plugins/gpu_driver/sim/fence_id.{h,cpp}
# 预期：仅 2 行（SIM_FENCE_ID_BASE / SIM_FENCE_ID_MAX 的 #define 行）
```

## Cross-Repo 影响

无。纯 UsrLinuxEmu sim 层注释 hygiene。

## Out of Scope（明确排除）

| 项 | 位置 | 后续 cleanup 候选 |
|----|------|------------------|
| drv 注释 `INT64_MAX` 字面量 | `gpgpu_device.cpp:427`, `gpu_drm_driver.cpp:278` | 可单独开 `drv-fence-id-comments-ssot` |
| drv 注释 `≥ 1<<32` 字面量 | `gpu_drm_driver.cpp:484` | 同上 |
| 文档注释 | `sim/README.md:15` | 可并入 `sim-fence-id-readme-ssot` |
| 测试注释 `1<<32` 字面量 | `test_fence_id_lifecycle_standalone.cpp:6,7,61,107,108` 等 | **保留**：测试应在 assert boundary 时使用字面量以便与原始 KFD 语义对照 |

## Dependencies

- 无（前置 C-03 已归档于 `archive/2026-07-07-sim-fence-id-base-cleanup/`）
