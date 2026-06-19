# Design: ssot-v0-1-7-comprehensive-fix

> **依赖**: `proposal.md` 已完成
> **作用**: 解释 HOW 实施 v0.1.7 SSOT 综合修复（17 项偏差）

## Context

**当前状态**（v0.1.6 审计完成后）:
- SSOT `post-refactor-architecture.md` 691 行，13 章节，状态 "🔄 待评审"
- v0.1.6 审计报告（commit `211b48c`）发现 25 项偏差，1 P0 必修
- 5 个并行 follow-up changes 正在准备中（覆盖剩余 8 项代码/文档侧偏差）

**本 change 范围**:
- 17 项 SSOT/文档侧偏差（详见 proposal.md）
- 全部为 doc-only 改动，零代码修改
- 单一 commit，docs-audit.sh --strict 守门

**约束**:
- 所有附录 A struct 字段定义必须**直接从源代码复制**（不"设计"）
- §1.7 / §1.8 更新措辞与原 SSOT 风格对齐
- SSOT 状态卡顿（🔄 → ✅）必须在本 change commit 时升（按 proposal 决策 D1）
- 单一 commit，不拆 PR

## Goals / Non-Goals

**Goals**:
- 修复 17 项 SSOT/文档侧偏差
- 补全附录 A（9 个 struct 完整化，1 P0 必修 + 8 P2 文档完整性）
- 刷新 §1.7 表格（3 项 P3 自描述过期）
- 闭环 §1.8 元数据（3 项 P2/P3 自指 + 状态卡顿 + 闭环证据）
- 跨文件更新（`docs/README.md` ADR-022 状态，§1.5 src/kernel 计数）
- 单一 commit + openspec archive

**Non-Goals**:
- 不修复任何代码（A1 #2 / A1 #3 / A1 #4 / A1 #1）→ 由 `fix-sim-hardware-layout` + `cleanup-shadow-dead-code` 负责
- 不修复任何 GTest 残留文档（A2 #1, #2）→ 由 `cleanup-gtest-residue` 负责
- 不修复 AGENTS.md 反向引用（A3 #2）→ 由 `fix-agents-md-ssot-link` 负责
- 不修复 orphan spec Purpose（A3 #3）→ 由 `cleanup-orphan-spec-purpose` 负责
- 不升 SSOT 到 v1.0（v1.0 需等所有 6 个 follow-up changes 合并后再开小 change）
- 不修改 `openspec/changes/` 内任何其他 change 的 spec.md
- 不修改 archive 目录

## Decisions

### D1: SSOT 状态卡顿时序 = 本 change commit 时升 ✅

**决策**: 本 change commit 时，SSOT 头部"🔄 待评审" → "✅ Approved（v0.1.7）"

**理由**:
- v0.1.7 合并的 17 项偏差**全部**为 SSOT/文档侧，闭环不依赖代码 fix
- "v0.1.6 审计完成"本身就是一次正式评审
- 其他 follow-up changes 修复的是代码/docs，独立于 SSOT 状态
- 状态卡顿（v0.1.5 → v0.1.6 → v0.1.7）跨多个 change 反而更清晰

**含义**:
- 本 change 内含"SSOT 状态升 ✅ Approved"作为 block 3 的一部分
- "v1.0 升版"是独立决策（本 change 不做，留待未来）

### D2: 附录 A struct 字段定义源 = 直接 grep 源代码

**决策**: 8 个新补录的 struct 字段定义**全部从源代码复制**（`plugins/gpu_driver/shared/gpu_ioctl.h` / `gpu_queue.h`）

**理由**:
- 偏差风险 ≈ 0（复制而非设计）
- 风格与 SSOT 现有 4 个 struct 一致（`u32` / `u64` / `gpu_va_space_handle_t` / `gpu_queue_handle_t`）
- 类型别名 `uint32_t` ↔ `u32` / `uint64_t` ↔ `u64` 保持源码原貌（不强行转换）

**含义**:
- 每个 struct 用 `<pre>` 代码块嵌入
- 块间空行分隔
- 行数对齐源代码（用空格 padding 而非 tab）

### D3: 附录 A struct 顺序 = 按 IOCTL 编号

**决策**: 8 个新补录的 struct 在附录 A 内按对应 IOCTL 编号顺序排列

| 顺序 | Struct | 对应 IOCTL | 编号 |
|------|--------|-----------|------|
| 1 | `gpu_mmu_event_cb_args` | `GPU_IOCTL_REGISTER_MMU_EVENT_CB` | 0x02 |
| 2 | `gpu_firmware_cb_args` | `GPU_IOCTL_REGISTER_FIRMWARE_CB` | 0x03 |
| 3 | `gpu_map_bo_args` | `GPU_IOCTL_MAP_BO` | 0x12 |
| 4 | `gpu_wait_fence_args` | `GPU_IOCTL_WAIT_FENCE` | 0x13 |
| 5 | `gpu_register_gpu_args` | `GPU_IOCTL_REGISTER_GPU` | 0x32 |
| 6 | `gpu_queue_map_ring_args` | `GPU_IOCTL_MAP_QUEUE_RING` | 0x42 |
| 7 | `gpu_queue_info_args` | `GPU_IOCTL_QUERY_QUEUE` | 0x43 |
| 8 | `gpu_ring_header` | (gpfifo ring buffer 内部结构) | - |

**理由**:
- 与 SSOT §1.6 附录 A 表格"参数结构"列的 IOCTL 编号顺序一致
- 阅读者按 IOCTL 编号查阅 struct 时一目了然
- 现有 4 个 struct（`gpu_pushbuffer_args` / `gpu_alloc_bo_args` / `gpu_va_space_args` / `gpu_queue_args`）按原顺序保留（不重排），新 struct 插在合适位置

### D4: A4 #1 `va_space_handle` 注释格式

**决策**: 在 `gpu_pushbuffer_args.va_space_handle` 字段后附 C 风格注释 + commit 引用

```c
struct gpu_pushbuffer_args {
  u32 stream_id;
  u64 entries_addr;
  u32 count;
  u32 flags;
  u64 fence_id;
  u64 va_space_handle;  // Phase 2 校验字段；sentinel 0 = 跳过校验（向后兼容）。H-1 commit 0272970。
};
```

**理由**:
- "sentinel 0 = 跳过校验"在先（解释语义）
- "H-1 commit `0272970`"在后（可追溯到 git 历史）
- 单行注释而非多行 Doxygen，保持 SSOT 现有 struct 风格简洁

### D5: §1.7 新增 3 行格式

**决策**: §1.7 表格新增行严格按现有 9 行格式（"来源 | 声明 | 实际"）

```markdown
| `CONTRIBUTING.md` | Catch2 | Catch2 |
| `docs/03-development/adding-devices.md` | Catch2 | Catch2 |
| `docs/06-reference/glossary.md` | Catch2 | Catch2 |
```

**理由**:
- 现有 9 行已统一为 "声明 | 实际" 两列对比
- 新增 3 行保持一致风格
- "声明 / 实际" 都是 Catch2 —— 因为 CONTRIBUTING.md 中的 GTest 残留（"可选"段）将由独立 change `cleanup-gtest-residue` 处理，本 change 只在 §1.7 表格反映"声明：Catch2"的预期状态

**含义**:
- A2 #2 偏差（CONTRIBUTING.md GTest 残留）的**修复**由 `cleanup-gtest-residue` 做
- 本 change 只在 §1.7 表格新增"该行应在"（声明=实际=Catch2）的状态
- 偏差本身标记为 "covered by other change"，**不在本 change 修复**

### D6: §1.8.1 闭环证据小节内容

**决策**: §1.8.1 小节包含 3 段

```markdown
#### §1.8.1 闭环证据（v0.1.6 审计确认）

v0.1.6 SSOT 深度审计（change `ssot-deep-audit`，commit `211b48c`）确认本 gap 已闭环：

- `post-refactor-architecture.md` 存在（47,232 字节，691 行）
- 28 个 `docs/` + AGENTS.md + README.md 范围内文件交叉引用本 SSOT
- 42 个项目全局 `.md` 文件引用本 SSOT
- 详细审计报告：[`docs/02_architecture/audit-reports/v0.1.6-audit.md`](audit-reports/v0.1.6-audit.md)（25 项偏差：🔴 1 / 🟠 4 / 🟡 14 / 🟢 6）
- 4 区域覆盖：§1.2 硬件仿真层 / §1.7 测试框架 / §1.8 权威空白 / 附录 A struct 字段
```

**理由**:
- "闭环"用词与 v0.1.6 审计 A3 #1 偏差的"建议：关闭（gap closed）"对齐
- 数字（28/42/25）直接来自 v0.1.6 审计报告（不重新计算）
- 4 区域列表是 SSOT 章节编号 + 主题，给读者快速定位
- 不再写"本文正是为此而写"（避免自指）

### D7: SSOT 变更记录 v0.1.7 条目格式

**决策**: 与 v0.1.1 ~ v0.1.6 现有条目保持完全一致格式

```markdown
| 2026-06-17 | 0.1.7 全面修复 | Sisyphus | **SSOT 侧 17 项偏差综合修复**（change `ssot-v0-1-7-comprehensive-fix`）：① 附录 A 补全 9 个 struct（A4 #1-#9，含 1 项 P0 必修 `gpu_pushbuffer_args.va_space_handle` + 8 个 P2 struct 定义）；② §1.7 表格刷新（A2 #3-#5：ADR-010 状态、AGENTS.md 描述、3 源补录）；③ §1.8 闭环证据小节（A3 #1, #4, #6）；④ `docs/README.md` ADR-022 状态陈旧修复（A3 #5）；⑤ §1.5 src/kernel cpp 计数 14→12 校准（A1 #5）；⑥ SSOT 状态升 "✅ Approved（v0.1.7）"；本次 commit 不改任何代码（设计 D5：审计与修复分离）|
```

**理由**:
- 与 v0.1.5 / v0.1.6 现有变更记录条目风格一致（表格 + 编号 + 简述 + commit）
- 详细执行情况见 v0.1.6 审计报告，本条目只列"做了什么"和"对应偏差编号"

## Risks / Trade-offs

| Risk | Impact | Mitigation |
|------|--------|------------|
| **R1**: 附录 A 新增 9 个 struct 定义（每个 5-10 行）→ SSOT 文件增大 50-100 行 | 低 | 仍是单文件可读；附录 A 本就该完整化 |
| **R2**: §1.7 表格新增 3 行 + 3 行内容修改 → 可能影响表格对齐 | 中 | 用 markdown 表格语法（不依赖空格对齐）|
| **R3**: SSOT 状态升 ✅ Approved 时机过早（其他 follow-up changes 还没合并）| 中 | D1 已决策：v0.1.7 是 SSOT 侧闭环，独立于代码 fix |
| **R4**: §1.8.1 闭环证据小节的数字（28/42/25）随时间过期 | 低 | 数字是 v0.1.6 审计时的快照；在小节标题写明"v0.1.6 审计确认" |
| **R5**: A4 #1 `va_space_handle` 字段在 H-1 commit `0272970` 已有，再次记录可能冗余 | 低 | 附录 A 是"代码定义形式化"，不冗余；§1.3 narrative 才是"业务流描述" |
| **R6**: docs-audit.sh 因附录 A 新增内容触发新检查项 | 低 | docs-audit 主要检查 markdown 链接/格式，struct 定义是 `<pre>` 代码块不触发 |

## Migration Plan

```
Phase 1: 准备 + 验证 (5 min)
  1.1 重新读 v0.1.6 审计报告 + 4 个 agent 原始报告
  1.2 grep 源代码确认 8 个新补录 struct 的字段（防止"复制"时漏字段）

Phase 2: 附录 A 完整化 (15 min)
  2.1 A4 #1: gpu_pushbuffer_args 末尾追加 va_space_handle 字段
  2.2 A4 #2-#6: 5 个 IOCTL 配套 struct 补录（按 D3 顺序）
  2.3 A4 #7-#9: 3 个 gpu_queue.h struct 补录

Phase 3: §1.7 表格刷新 (5 min)
  3.1 A2 #3: ADR-010 行更新
  3.2 A2 #4: 新增 3 行
  3.3 A2 #5: AGENTS.md 行更新

Phase 4: §1.8 自描述更新 (5 min)
  4.1 A3 #1: §1.8.1 闭环证据小节
  4.2 A3 #4: SSOT 头状态升 ✅
  4.3 A3 #6: §1.8 自指表述改写

Phase 5: 跨文件 + 跨章节更新 (5 min)
  5.1 A3 #5: docs/README.md ADR-022 状态
  5.2 A1 #5: §1.5 src/kernel cpp 计数 14→12

Phase 6: 变更记录 + 状态 (3 min)
  6.1 v0.1.7 变更记录条目
  6.2 SSOT 底部 footer 更新（最后更新日期）

Phase 7: 验证 (3 min)
  7.1 bash tools/docs-audit.sh --strict（必须 36/36 PASS）
  7.2 make -j4 -C build（必须 100% 编译通过，纯文档也应 verify）

Phase 8: 提交 + 归档 (2 min)
  8.1 git add + git commit（单 commit，message 模板见 proposal.md）
  8.2 openspec archive ssot-v0-1-7-comprehensive-fix --yes
```

**总估时**: ~37 min（不计 review 时间）

**Rollback**:
- 全部回滚：git revert + openspec 不归档（保持 change active）
- 部分回滚：git revert 特定 block（每个 block 用独立 commit？—— 不，本设计是单 commit）
- 单 commit 设计意味着任何回滚都是全部回滚；如果需要更细粒度，回滚后重新切片成多个 commit

## Open Questions

无（proposal §开放问题已通过 D1-D7 解决）。

未来可能性（如需 v0.1.8+ 审计）:
- D3 附录 A struct 顺序可扩展为"按 IOCTL 编号分组"（在附录 A 顶部加 IOCTL 编号表索引）
- D6 §1.8.1 闭环证据可扩展为"§1.8.x 历次审计时间线"小节
- D1 SSOT 状态机可扩展为"v0.1.x → v1.0 升版 checklist"（每次合并对应一个状态转换）
