# Change: fix-sim-hardware-layout

> **状态**: ✅ Completed（2026-06-18，本 change 归档时即闭环）
> **创建**: 2026-06-17
> **来源**: v0.1.6 审计 A1 #2（`sim/hardware/` 布局分裂）
> **关联 SSOT**: `docs/02_architecture/post-refactor-architecture.md` (v0.1.7 §1.2)
> **关联审计报告**: `docs/02_architecture/audit-reports/v0.1.6-audit.md`

## Why

v0.1.6 审计 A1 #2 🟠 P1：`sim/hardware/` 目录仅含 .h 文件，对应 .cpp 实现在 `sim/` 根。

**实际状态（验证）**:
```
plugins/gpu_driver/sim/
├── CMakeLists.txt
├── buddy_allocator.cpp          (shadow 编译死代码，A1 #4)
├── doorbell_emu.cpp             (在 sim/ 根，但对应 class 在 sim/hardware/)
├── fence_sim.cpp                (shadow 编译死代码，A1 #4)
├── gpu_queue_emu.cpp
├── gpu_queue_emu.h
├── hardware/                    (只含 .h)
│   ├── doorbell_emu.h
│   └── hardware_puller_emu.h
├── hardware_puller_emu.cpp      (在 sim/ 根，但对应 class 在 sim/hardware/)
└── scheduler/
    ├── global_scheduler.cpp
    ├── global_scheduler.h
    └── translator/
        ├── gpfifo_translator.cpp
        └── gpfifo_translator.h
```

**SSOT §1.2 line 124 描述**（误导）:
```
│   • sim/hardware/   : HardwarePullerEmu (FSM), DoorbellEmu      │
```
读者会自然假设 .h+.cpp 都在 `sim/hardware/`，但 .cpp 实际在 `sim/` 根。

**Why now**:
- A1 #2 是 P1 高优（影响 §1.2 可信度）
- v0.1.7 commit `5fa1f71` 明确"fix-sim-hardware-layout"作为 follow-up change
- 这是 v0.1.6 审计 4 个 P1 中**唯一涉及代码移动**的，需谨慎

## What Changes

**新能力**: `fix-sim-hardware-layout` —— 修复 `sim/hardware/` 布局分裂

**实施选项**:

### 选项 A（推荐）：移动 .cpp 到 `sim/hardware/`
- `plugins/gpu_driver/sim/doorbell_emu.cpp` → `plugins/gpu_driver/sim/hardware/doorbell_emu.cpp`
- `plugins/gpu_driver/sim/hardware_puller_emu.cpp` → `plugins/gpu_driver/sim/hardware/hardware_puller_emu.cpp`
- 更新 `sim/CMakeLists.txt` 编译路径
- 更新 `#include "hardware/doorbell_emu.h"` 路径
- 重新 build + 跑 `test_doorbell_emu_standalone` + `test_hardware_puller_emu_standalone` 验证

### 选项 B（备选，更低风险）：保持现状 + 更新 SSOT
- 仅修改 `post-refactor-architecture.md` §1.2 line 124 加注释"实现位于 sim/ 根"
- 无代码改动

**本 change 采用选项 A**（更彻底解决偏差），但保留选项 B 作为回滚备选。

## Capabilities

### New Capabilities
- 无

### Modified Capabilities
- 无

## Impact

| 影响项 | 范围 | 风险 |
|--------|------|------|
| 代码 | 2 文件移动 + CMakeLists 调整 | **中**（include 路径改动）|
| SSOT | §1.2 line 124 简化（无注释需求）| 低 |
| 编译 | 需重新跑 build | 中 |
| 测试 | 2 个 standalone 测试需重跑 | 中 |
| 归档/历史 | 无 | 极低 |

**风险缓解**:
- 移动前 grep 所有 `#include` 引用，确保同步更新
- 移动后跑 `make -j4 -C build` + `make test` + 关键测试 `test_doorbell_emu_standalone` + `test_hardware_puller_emu_standalone`
- 回滚预案完整

## 关联 Changes

- 本 change 是 v0.1.6 审计 A1 #2 的修复
- 可与其他 3 个 P1 change **完全并行**（不同文件族，无冲突）
- 注：A1 #1/#3/#4（translator 嵌套、doorbell 空壳、shadow 死代码）由 `cleanup-shadow-dead-code` 单独处理，**不**在本 change 范围
