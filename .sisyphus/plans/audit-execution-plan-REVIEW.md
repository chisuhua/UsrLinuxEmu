# UsrLinuxEmu 审计执行计划审查报告

**审查日期**: 2026-05-08  
**审查者**: Momus (Plan Critic)  
**计划版本**: audit-execution-plan-2026.md

---

## VERDICT: CONDITIONAL APPROVE

该计划整体结构完整，依赖关系清晰，但存在若干需要在执行前解决的矛盾和低估问题。

---

## Must-Fix Issues（执行前必须解决）

### 1. T2 (ServiceRegistry) 内部矛盾

**问题**: 审计摘要将 `ServiceRegistry::lookup_service()` 标记为"零外部调用者"的高优先级孤立问题，但 T2 的处理方案是"保留空壳+去耦合"。这两个描述存在逻辑矛盾。

**实际情况** (`src/kernel/vfs.cpp` 第 31、81 行):
```cpp
// vfs.cpp register_device() 中:
ServiceRegistry::instance().register_service(dev->name, dev);

// vfs.cpp unregister_device() 中:
ServiceRegistry::instance().unregister_service(name);
```

ServiceRegistry **正在被 VFS 调用**，不是孤儿。T2 要么没有清理干净（只是去掉了调用），要么判断错误（ServiceRegistry 本身有存在价值）。

**需要的修复**: 
- 明确 T2 的决策：到底是"删除 ServiceRegistry 类"还是"保留但从 VFS 解耦"？
- 如果是后者，审计摘要的"High Priority orphan"标记需要移除或修正
- 建议：将 T2 改为两个子任务 T2a（决策）+ T2b（执行）

---

### 2. T5/T6 严重低估 drv/gpgpu_device.cpp 的工作量

**问题**: 计划称 `drv/gpgpu_device.cpp` "几乎是空壳（364 字节）"，实际只有 **12 行**：
```cpp
// gpgpu_device.cpp — GPU 驱动设备（影子编译）
#include "shared/gpu_ioctl.h"
// TODO(P1.1b): 实现完整的 GpgpuDevice 类
// 当前仅作为编译验证目标
```

T6 计划在 1 天内将 `plugin.cpp`（656 行）的 `GpgpuDevice` 类迁移到这个 12 行骨架上，这是**不可能完成**的。

**需要的修复**:
- T6 的时间估算应改为 **3-5 天**（迁移 GpgpuDevice + BuddyAllocator + HandleManager）
- 或者调整里程碑：T6 拆分为 T6a（实现 drv/gpgpu_device.cpp）+ T6b（薄入口切换）

---

### 3. T7 严重低估 sim/ 组件的空壳程度

**问题**: 计划描述:
- `doorbell_emu.cpp` (119 字节) — "几乎是空文件"
- `hardware_puller_emu.cpp` (128 字节) — "几乎是空文件"

**实际情况**:
- `doorbell_emu.cpp`: **5 行**（仅 TODO 注释）
- `hardware_puller_emu.cpp`: **5 行**（仅 TODO 注释）

计划对 T7 的估算（2-3 天）可能不够，需要在 T7 开始前重新评估实际工作量。

**需要的修复**:
- 在 T7 开始前添加"预评估"步骤，或将 T7 拆分为 T7a（评估实际填充工作量）+ T7b（实现）

---

## Should-Fix Issues（建议修复，但不会阻止执行）

### 1. N2（hal_user.cpp malloc/free 违规）未纳入 T5

**问题**: Section 7 发现 `hal_user.cpp` 第 125 行使用 `malloc(HAL_HEAP_SIZE)`，违反 ADR-020"无 malloc/free"约束。但这个发现未体现在 T5 的实施步骤中。

**建议**: 在 T5 的实施步骤中添加：
```markdown
5. **修复 ADR-020 违规** (来自 N2):
   - 将 `hal_user.cpp` 的 `malloc`/`free` 替换为 `new uint8_t[HAL_HEAP_SIZE]` / `delete[]`
```

---

### 2. T12 与 T9 优先级不一致

**问题**: 
- T12 (TaskRunner E2E): 🔴 高优先级，依赖 T6
- T9 (DRM ioctl 表): 🟡 中优先级，依赖 T6

两者都只依赖 T6，但优先级差异显著。建议统一或说明原因。

---

### 3. cuda_compat_ioctl.cpp 状态描述不准确

**问题**: T1 说"零编译引用"是正确的，但计划说"9445 字节"实际是 286 行。不是说字节数有问题，而是建议使用行数保持一致性。

---

## Execution Order（执行顺序建议）

当前计划顺序基本合理，但有以下调整建议：

### 调整 1: T2 应先于 T5 明确

T2 是决策任务，其结果影响 T5 的实施策略。建议：

```
Day 1: T1(孤儿清理) + T2(决策) + T3(更新文档) + T4(归档)
Day 1.5: T2 决策评审 → 明确 T5 的范围
Day 2-5: T5 (HAL 集成) ← 根据 T2 决策调整
```

### 调整 2: T6 时间翻倍

当前: Day 4 (1天)
建议: Day 4-6 (3天) + Day 7 预留 buffer

理由: 需要将 656 行代码（含 BuddyAllocator、HandleManager）迁移到 12 行的空壳文件。

### 调整 3: T7 增加预评估步骤

```
Day 5-5.5: T7 预评估（实际测量填充每个文件的工作量）
Day 5.5-8: T7 实施
```

---

## Missing Tasks（关键缺口）

### 1. GPU_OP_LAUNCH_CPU_TASK 处理（N1）

Section 7.1 提到 `plugin.cpp` 中 `GPU_OP_LAUNCH_CPU_TASK` 仍未处理，但这个任务未纳入任务表。

**建议**: 在 T5/T6 期间添加处理分支，或明确标记为 Phase 2 项目。

---

### 2. 集成测试任务缺失

Section 7.2 发现 "缺少 `plugin.cpp` → HAL → SIM 的集成测试"，建议在 T5 完成后添加 `test_hal_integration.cpp`。

**建议**: 在 T5 和 T6 之间插入：
```
T5.5: 添加 HAL 集成测试
依赖: T5 完成
内容: 验证 HAL 10 个函数指针非空且可调用
```

---

### 3. 回滚计划缺失

T5 是高风险任务（中风险描述），但没有具体的回滚计划。"保留 BuddyAllocator 作为 fallback" 只是缓解措施，不是回滚方案。

**建议**: 在 T5 前添加回滚检查点：
```bash
# T5 开始前
git branch backup-before-hal-integration

# 如果 T5 失败
git checkout backup-before-hal-integration
```

---

## 计划完整性评估

| 维度 | 评分 | 说明 |
|------|------|------|
| 任务清晰度 | 8/10 | 目标明确，但部分估算偏差大 |
| 依赖完整性 | 9/10 | 依赖图基本正确 |
| 时间可行性 | 6/10 | T6、T7 明显低估 |
| 风险覆盖 | 7/10 | 有风险识别，但回滚计划缺失 |
| 优先级一致性 | 7/10 | T2/T12 等存在不一致 |

---

## 审查结论

**CONDITIONAL APPROVE** — 计划可以执行，但 Must-Fix Issues 必须在 Day 1 开始前解决，否则执行过程中会发现逻辑矛盾导致返工。

最关键的修复是 **T2 的矛盾** 和 **T6 的时间估算**，这两项不解决，后续任务会持续受影响。

---

**审查者**: Momus  
**日期**: 2026-05-08
