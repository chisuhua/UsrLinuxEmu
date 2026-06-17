# Design: cleanup-adr-placeholders

> **依赖**: `proposal.md` ✅
> **作用**: HOW 实施 8 份 ADR 的最终状态

## Context

**现状**（commit `40d2fda` 之后）：
- `docs/00_adr/adr-022-gpu-compute-unit-emulation.md` — 68 行，5 个开放问题
- `docs/00_adr/adr-025-030-phase3-placeholder.md` — 5 份通用占位（每份 30-40 行）
- `docs/00_adr/adr-031-ttm-migration-priority.md` — 58 行，5 个开放问题
- 共 7 份待治理（025-030 是同一模板的 6 份）

**约束**：
- 不得批量塞决策（"为填而填"会让 ADR 失去 signal 价值）
- 025-030 的"候选 A/B/C"必须保留作为未来 owner 的讨论起点
- v0.1.2 SSOT 已在 references 中标注这些 ADR 处于"提议中"，治理动作要让"提议中"状态要么前进到"已接受"要么明确进入"deferred"

**相关代码钩子**（022 / 031 真正可填的依据）：

- `libgpu_core/gpu_buddy.{h,c}` — 纯 C buddy allocator，ADR-020 产物
- `plugins/gpu_driver/sim/hardware/hardware_puller_emu.cpp` — FSM IDLE→FETCH→DECODE→SCHEDULE→DISPATCH→COMPLETE
- `plugins/gpu_driver/drv/gpgpu_device.cpp` — `handle_alloc_bo` / `handle_map_bo`
- `include/linux_compat/drm/drm_*.h` — DRM stubs（drm_ioctl / drm_gem / drm_driver）

## Goals / Non-Goals

**Goals**:
- 把 2 份具体主题占位（022 / 031）转化为真实决策（v1，已接受）
- 把 6 份通用占位（025-030）转化为显式 deferral（带触发条件）
- 同步更新 PRD / SSOT 引用语义
- 不新增代码（除非 ADR-031 v1 决策需要）

**Non-Goals**:
- 不实施 GPU Compute Unit Emulation 的实际代码（这是 ADR-022 v1 决定**之后**的另一个 change）
- 不实施 TTM 包装的实际代码（同样）
- 不重新设计 SSOT §3.3 的 ADR 治理规则——只应用现有规则

## Decisions

### D1: ADR-022 v1 决策 = Operator-Level Emulation（不是指令级）

**决策**：Compute Unit Emulation 走"预定义 kernel template"路线，提供 4 个 v1 template：
- `add_vec4`（float4 加法）
- `mul_vec4`（float4 乘法）
- `memcpy_h2d_via_pull`（利用现有 pushbuffer 路径的 memcpy 模板）
- `noop`（测试占位）

**理由**：
- 与现有 `HardwarePullerEmu` FSM 无缝衔接：FETCH 阶段解析 template name，DISPATCH 阶段调用对应 C++ 实现
- TaskRunner 端到端验证可以基于这 4 个 template 测"kernel 真的执行"语义
- 不需要实现 ISA 解释器（避免 1-2 月工作量）
- 4 个 template 足以验证 Compute Unit 概念正确性

**备选方案**：
- **A. 指令级解释（ISA-level）**：实现 RISC-V vector 子集或 AMD GCN 子集。**否决**：工作量 4-8 周；与 `libgpu_core` 路径重叠。
- **B. 性能模型（cycle-accurate）**：只算延迟不执行。**否决**：无法满足 TaskRunner "kernel 真的执行"需求。
- **C. 不实现 Compute Unit（永久）**：与 ADR-022 主题相悖。

### D2: ADR-031 v1 决策 = TTM 包装 libgpu_core/buddy（不是替代）

**决策**：TTM 作为 thin wrapper layer；`libgpu_core/gpu_buddy` 是底层 page pool；TTM 加 BO metadata + placement 策略。

**理由**：
- 不破坏 ADR-020 提取的纯 C buddy allocator（已用于 `handle_alloc_bo`）
- 满足 ADR-019 §6 提到的"Phase 2 TTM 迁移路径不清晰"的痛点
- 包装层 thin → 1-2 周可实施

**新文件位置**（若实施时）：`include/linux_compat/drm/ttm.h`（与其他 drm_*.h 并列）

**备选方案**：
- **A. TTM 替代 buddy**：把 buddy 代码搬到 TTM 内部。**否决**：违反 ADR-020 治理意图。
- **B. 完整 TTM（含 swapout）**：UsrLinuxEmu 无 swap，**否决**。v1 注释明确"swapout 留 Phase 3+"。
- **C. 跳过 TTM（永久）**：与 ADR-019 路径不一致。

### D3: ADR-025-030 状态 = "⏸️ Deferred until Phase 3 trigger"

**决策**：每份 ADR 改 3 处：
1. Status 行：`🔄 提议中 — 占位骨架` → `⏸️ 显式 Deferred — 待 Phase 3 触发`
2. 删除"候选项 A/B/C/D"段（不强制选）
3. 新增"Phase 3 Trigger"段，明确什么时候重新打开

**每个 ADR 的触发条件**（写入文件）：
- **ADR-025**（多进程 / 沙箱 / 异步 IO）：第一个第三方 .so 插件提交 OR Phase 3 网络设备原型 commit
- **ADR-026**（多 GPU / 热插拔）：第二个 `gpu_driver` 插件原型 OR 第一个设备热插拔测试
- **ADR-028**（网络/存储设备架构）：`drivers/network/` 或 `drivers/storage/` 第一个文件 commit
- **ADR-029**（性能/Tracing/错误注入）：CI 增加 benchmark 步骤 OR 第一个 tracepoint 用例
- **ADR-030**（调试接口）：gdb/lldb 集成 issue 创建 OR 第一个 state-serialization 用例

**理由**：
- 显式 deferred 比"无限期 🔄" 状态有 signal：未来 grep 可区分"暂不需要"和"被遗忘"
- 触发条件具体到 commit 事件——可自动检测

**备选方案**：
- **A. 全部 ❌ 拒绝（out of scope）**：太激进，025-030 都是合理候选
- **B. 全部填一个候选**：失去"候选项"的多视角价值
- **C. 维持现状**：不接受（这就是本 change 要解决的债）

### D4: 文件结构 = 保留历史段（不删除占位原文）

**决策**：deferred ADR 在文件顶部加新状态段，**保留**原"候选项 A/B/C"内容作为 `## 讨论历史（v0 占位）`附录。

**理由**：
- 未来 owner 重新打开 ADR 时能立刻看到当时的讨论起点
- 避免"重复发明轮子"——之前的人已经想过 ABCD 方案
- 文档体积略增但 git history 干净

### D5: docs-audit 无需新检查项

**决策**：本 change 不改 `tools/docs-audit.sh`。已有的"ADR 022/025-031 present"检查项已守住存在性；"decision status"是叙事性而非机器可检。

**理由**：新增"ADR 状态必须非占位"检查项会过于激进（ADR-022 v0 必然以占位身份存在过）。

### D6: PR 拆分 = 3 个独立 commit

**决策**：tasks.md 阶段 6 拆分为：
- commit 1: ADR-022 v1 填决策
- commit 2: ADR-031 v1 填决策
- commit 3: 6 份通用占位（025-030）+ 文档同步

**理由**：3 个 commit 独立可 revert；reviewer 可分别 review。

## Risks / Trade-offs

| Risk | Impact | Mitigation |
|------|--------|------------|
| **R1**: ADR-022 v1 决策（operator-level）未来 Phase 3 推翻 → 重做 | 中 | v1 决策明确标注"4 个 template 仅 v1"；Phase 3 触发条件写"第 5 个 template 需求出现" |
| **R2**: ADR-031 TTM 包装层命名（`drm/ttm.h`）与未来真内核 TTM 头冲突 | 中 | design D2 已说明；v1 仅函数签名兼容，结构体布局不保证 |
| **R3**: 025-030 触发条件难以自动检测（如"第一个 .so 提交"模糊）| 低 | 触发条件表述具体到 commit / issue 编号；留 owner 主观判断空间 |
| **R4**: PRD.md 引用更新遗漏某些文件 | 低 | tasks.md §4 列 4 个文档逐一检查 |
| **R5**: 6 份通用占位被某 reviewer 视为"偷懒"，要求填决策 | 中 | proposal §Why 已论证"不批量塞决策"原则；D3 备选已列 |
| **R6**: 与 ADR-027 v1（linux_compat）有微小冲突（TTM 头签名稳定性）| 低 | ADR-027 v1 决策 3 "不跟踪 LTS" 适用于 linux_compat 整体；TTM 是显例"稳定签名 vs 完整 ABI" 的取舍点，design §D2 标注 |

## Migration Plan

**Rollout**：
1. **Phase 1**: ADR-022 v1（commit 1）
2. **Phase 2**: ADR-031 v1（commit 2）
3. **Phase 3**: 025-030 deferral + 文档同步（commit 3）

每 Phase 独立可 merge。失败回滚到上一个 commit。

**Rollback**：任一 commit 失败可独立 `git revert`；不需要 schema migration。

**Archive 触发**：本 change 完成后 `openspec archive cleanup-adr-placeholders`，8 份 ADR 的"状态迁移"成为永久 SSOT 记录。

## Open Questions

无（proposal §开放问题已通过 D1-D6 解决）。

如未来需要：D1 可扩展第 5 个 template（基于 Phase 3 触发）；D2 可升级为 full TTM。
