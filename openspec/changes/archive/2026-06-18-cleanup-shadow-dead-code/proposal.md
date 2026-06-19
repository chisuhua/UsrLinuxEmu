# Change: cleanup-shadow-dead-code

> **状态**: ✅ Completed（2026-06-18，本 change 归档时即闭环）
> **创建**: 2026-06-17
> **来源**: v0.1.6 SSOT 审计 A1 #1 + #3 + #4（3 个 P2 残留）
> **关联 SSOT**: `docs/02_architecture/post-refactor-architecture.md` §1.2
> **关联审计报告**: `docs/02_architecture/audit-reports/v0.1.6-audit.md`

## Why

v0.1.6 SSOT 审计（commit `211b48c`）3 个 P2 残留未在 v0.1.7 + 4 P1 change 中闭合：

**A1 #1 (🟡 P2)**：SSOT §1.2 line 122-123 描述未体现嵌套结构
```
│   • sim/scheduler/  : GlobalScheduler (FIFO + engine routing)   │
│                       + GpfifoToLaunchParamsTranslator           │
```
实际：`GpfifoToLaunchParamsTranslator` 在 `sim/scheduler/translator/` 嵌套子目录

**A1 #3 (🟡 P2)**：`sim/hardware/doorbell_emu.cpp` 是 1 行空壳
```cpp
#include "doorbell_emu.h"
```
`DoorbellEmu` 实际是 header-only class（所有方法在 `sim/hardware/doorbell_emu.h` 内联）

**A1 #4 (🟡 P2)**：`sim/buddy_allocator.cpp` + `sim/fence_sim.cpp` 是 dead code
- `SimBuddyAllocator`（buddy_allocator.cpp:12-44）：定义但 0 实例化
- `FenceSim`（fence_sim.cpp:9-32）：定义但 0 实例化
- 文件头注释明确"影子编译"

**Why now**:
- 4 P1 change 已完成（commit `3e306d1`/`3faa3a7`/`880eb4f` + cleanup-orphan-spec-purpose），仅剩此 3 P2
- 死代码是技术债，未来开发者会被误导以为这些类需要维护
- 删除前需完整测试验证无外部依赖

## What Changes

**新能力**: `cleanup-shadow-dead-code` —— 清理 sim/ 层 3 处 P2 残留

**实施**（5 步骤）:
1. **A1 #1**: SSOT §1.2 line 122-123 修订：明确 `translator/` 嵌套
2. **A1 #3**: 删除 `plugins/gpu_driver/sim/hardware/doorbell_emu.cpp`（1 行空壳）
3. **A1 #4 删除**: 删除 `sim/buddy_allocator.cpp` + `sim/fence_sim.cpp`（2 个 dead code 文件）
4. **A1 #4 CMake**: 更新 `sim/CMakeLists.txt` 编译清单（移除 3 个文件）
5. **SSOT §1.2 line 126**: 移除 `sim/buddy_allocator.cpp, sim/fence_sim.cpp (shadow 编译)` 行（不再存在）

## Capabilities

### New Capabilities
- 无

### Modified Capabilities
- 无

## Impact

| 影响项 | 范围 | 风险 |
|--------|------|------|
| 代码 | 3 文件删除 + CMakeLists.txt 2-3 行编辑 | 中（删代码需测试验证）|
| SSOT | §1.2 line 122-123, 126 修订 | 低 |
| 测试 | 需重跑全部 sim/ 相关测试 | 中 |

**风险缓解**:
- 删除前 grep 所有引用（已确认 0 实例化）
- 删除后跑 `make -j4 -C build` + `ctest` + 关键测试
- SSOT 与代码双向同步

## 关联 Changes

- v0.1.6 审计 25 项偏差的最后 3 项
- 与 `ssot-0-section-refresh` + `adr-024-status-upgrade` 完全独立（不同文件，可并行）
- 完成后 v0.1.6 审计 25 项偏差 **100% 闭合**