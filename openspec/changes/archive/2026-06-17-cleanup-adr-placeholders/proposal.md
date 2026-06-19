# Change: cleanup-adr-placeholders

> **状态**: ✅ Completed（2026-06-17，本 change 归档时即闭环）
> **创建**: 2026-06-17
> **来源**: docs/02_architecture/post-refactor-architecture.md §3.3（ADR 治理）+ C 审计中识别
> **关联 SSOT**: `docs/02_architecture/post-refactor-architecture.md` §3.3, `docs/PRD.md` §ADR 引用

## Why

2026-06-16 commit `40d2fda` 为了"close the ADR numbering gap"一次性补了 8 份占位 ADR（022, 025-030, 031），每份都是「决策待定 / 候选项 A/B/C/D」骨架。这一举措**关掉了 docs-audit §3.2 的红色告警**，但产生了**新的技术债**：

1. **PRD.md 与 post-refactor-architecture.md §1.6 引用这些 ADR 编号时隐含"决策已存在"的语义**——而实际上没有任何决策。
2. **6 份通用占位（025-030）的"候选项 A/B/C"清单没有 owner 也没有 deadline**——处于永久提议状态。
3. **2 份主题具体占位（022 Compute Unit, 031 TTM）有清晰的实施钩子**（与 `libgpu_core/`、`HardwarePullerEmu`、`handle_alloc_bo` 等真实代码挂钩），但未利用。

**Why now**：
- H-1 实施后审计追踪（SSOT v0.1.3）建立完整，治理债成为下一阶段最显眼的问题
- Phase 3 启动需要至少 1-2 份真实 ADR 指引方向（不能继续靠"看 ADR-022 占位"判断方向）
- 不清理的话，未来每次 grep "ADR-022" 都会返回 68 行占位文件，浪费 reviewer 时间

## What Changes

**8 份占位 ADR 的明确处理**：

| ADR | 当前 | 目标 | 工作量 |
|-----|------|------|--------|
| **ADR-022** GPU Compute Unit Emulation | 占位 | ✅ v1 已接受（operator-level emulation 决策） | 4-6h |
| **ADR-031** TTM Migration Priority | 占位 | ✅ v1 已接受（TTM 包装 libgpu_core/buddy） | 3-4h |
| **ADR-025** Phase 3+ Placeholder | 占位 | ⏸️ 转为"Deferred until Phase 3 trigger" | 30 min |
| **ADR-026** Phase 3+ Placeholder | 占位 | ⏸️ 同上 | 30 min |
| **ADR-028** Phase 3+ Placeholder | 占位 | ⏸️ 同上 | 30 min |
| **ADR-029** Phase 3+ Placeholder | 占位 | ⏸️ 同上 | 30 min |
| **ADR-030** Phase 3+ Placeholder | 占位 | ⏸️ 同上 | 30 min |

**文档同步**：
- `docs/02_architecture/post-refactor-architecture.md` §3.3 ADR 治理：更新治理状态说明
- `docs/PRD.md`：移除对 ADR-025~030 的"决策已存在"式引用，改为"占位已转为 deferred"标注
- `docs/00_adr/README.md` §"编号 gap 治理"：补一句"025-030 已明确 deferral policy"

**无代码改动**（纯治理 + 文档工作）。

## Capabilities

### New Capabilities
- `adr-placeholder-cleanup`: 8 份占位 ADR 的最终状态（v1 已接受 或 explicit deferred）

### Modified Capabilities
- 无

## Impact

| 影响项 | 范围 | 风险 |
|--------|------|------|
| ADR 文件 | 8 份 .md 全部改 status | 低（明确治理动作）|
| 引用方 | `docs/PRD.md` + `post-refactor-architecture.md` + 其他引用 ADR-022/025-030/031 的文档 | 低（语义对齐）|
| 代码 | 无 | 0 |
| 测试 | 无 | 0 |
| CI | `tools/docs-audit.sh` 仍 36/36 PASS（无新检查项）| 0 |
| Phase 3+ roadmap | 后续 owner 有清晰方向 | **正向** |

**关键设计原则**：
- **不批量塞决策**：025-030 候选 A/B/C/D 都不强制选一个（避免"为填而填"）
- **明确触发条件**：deferred ADR 必须有"什么时候重新打开"的标准（如"Phase 3 启动 + 网络插件原型 commit"）
- **保留历史**：原占位内容作为"讨论历史"附录保留，不删除

## 关键决策预告（详见 design.md）

- **ADR-022 v1 决策**：采用 **operator-level emulation**（预定义 kernel template，如 `add_vec4`, `matmul_4x4`）。理由：与现有 `HardwarePullerEmu` FSM 衔接最自然；不动现有 `libgpu_core`；覆盖 TaskRunner 端到端验证需要的"kernel 真的执行"语义。**不做指令级解释**（ISA-level 解释留 Phase 3+ 后续 ADR）。
- **ADR-031 v1 决策**：TTM 作为**包装层**（thin wrapper over `libgpu_core/gpu_buddy`）。buddy allocator 是 TTM 的 page pool；TTM 加 BO metadata + placement 策略层。**不做 full TTM**（swapout 留 Phase 3+，UsrLinuxEmu 无 swap）。
- **ADR-025-030 状态**：⏸️ 显式 deferred，每个加 "Phase 3 Trigger" 段，明确什么时候重新打开。

## 开放问题（待 design.md 解决）

1. ADR-022 operator-level emulation 的"template 集合"边界：是否需要暴露给用户配置（yaml/JSON）？还是硬编码在代码里？建议硬编码 v1，Phase 3+ 再讨论配置化。
2. ADR-031 TTM 包装层文件名：`include/linux_compat/drm/ttm.h` vs `plugins/gpu_driver/sim/ttm_emu.h`？建议前者（与其他 linux_compat 头并列），但与 ADR-027 v1 决策 3 "不跟踪 LTS" 有微小冲突——TTM 头需要稳定签名。design.md 解决。
