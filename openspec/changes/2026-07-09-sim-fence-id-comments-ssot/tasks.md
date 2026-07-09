# Tasks: sim-fence-id-comments-ssot

> **状态**: 📋 PROPOSED
> **目标**: `sim/fence_id.h` + `sim/fence_id.cpp` 注释层完成 SSOT 化（使用宏名而非字面量）

## 1. 替换 `sim/fence_id.h` 注释（5 分钟）

- [ ] 1.1 行 6-8：背景段 `[1, (1<<32) - 1]` → `[1, SIM_FENCE_ID_BASE - 1]`
- [ ] 1.2 行 8：`[(1<<32), INT64_MAX]` → `[SIM_FENCE_ID_BASE, SIM_FENCE_ID_MAX]`
- [ ] 1.3 行 21：`/* Range constants (Fix-1: sim 层 fence_id 范围 [(1<<32), INT64_MAX]) */` → `/* Range constants (Fix-1: sim 层 fence_id 范围 [SIM_FENCE_ID_BASE, SIM_FENCE_ID_MAX]) */`

## 2. 替换 `sim/fence_id.cpp` 设计注释（2 分钟）

- [ ] 2.1 行 5：`从 SIM_FENCE_ID_BASE (1<<32) 开始` → `从 SIM_FENCE_ID_BASE 开始`（删去 `(1<<32)` 旁注，因其已在 .h 宏定义处说明）

## 3. 验证（10 分钟）

- [ ] 3.1 `grep -RIn "(1<<32)\|INT64_MAX" plugins/gpu_driver/sim/fence_id.{h,cpp}` 仅返回 2 行（`#define SIM_FENCE_ID_BASE (1ULL << 32)` + `#define SIM_FENCE_ID_MAX INT64_MAX`），其余位置都已替换
- [ ] 3.2 `cd build && make -j4` 0 warnings（仅注释改动也应无 warning）
- [ ] 3.3 `ctest -R fence_id` PASS（断言 fence 边界行为）
- [ ] 3.4 `ctest -R phase31` PASS（kfd_phase31 ioctl 派发）
- [ ] 3.5 `ctest` 全绿

## 4. commit / push（5 分钟）

- [ ] 4.1 commit：`docs(sim): unify fence_id range comments to use SIM_FENCE_ID_BASE / SIM_FENCE_ID_MAX macros`
- [ ] 4.2 push & open PR
- [ ] 4.3 PR 描述 closes（部分）Metis follow-up 给 `sim-fence-id-base-cleanup` 归档 review
