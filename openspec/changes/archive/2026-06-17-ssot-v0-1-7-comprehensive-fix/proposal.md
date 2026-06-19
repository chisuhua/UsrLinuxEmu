# Change: ssot-v0-1-7-comprehensive-fix

> **状态**: ✅ Completed（2026-06-17，本 change 归档时即闭环）
> **创建**: 2026-06-17
> **来源**: v0.1.6 SSOT 审计报告（change `ssot-deep-audit`，commit `211b48c`）的 17 项 SSOT 侧偏差
> **关联 SSOT**: `docs/02_architecture/post-refactor-architecture.md`（v0.1.6 审计完成 → v0.1.7 全面修复）
> **关联审计报告**: `docs/02_architecture/audit-reports/v0.1.6-audit.md`

## Why

v0.1.6 审计（change `ssot-deep-audit`）发现 SSOT 25 项偏差：
- 🔴 1 项 P0（A4 #1: 附录 A `gpu_pushbuffer_args` 缺 `va_space_handle` 字段，H-1 commit `0272970` 后未同步）
- 🟠 4 项 P1
- 🟡 14 项 P2
- 🟢 6 项 P3

按 v0.1.6 审计设计 D5（"审计与修复分离"），25 项偏差应由独立 follow-up changes 修复。本 change 负责其中 **17 项 SSOT 侧偏差**（剩 8 项由 5 个并行 changes 处理，详见本文末"关联 changes"）。

### 本 change 覆盖的 17 项偏差

| 来源 | 编号 | 简述 | 严重度 |
|------|------|------|--------|
| A1 | #5 | §1.5 `src/kernel/` cpp 计数 14→12 校准 | 🟢 P3 |
| A2 | #3 | §1.7 ADR-010 行 "未实施" → "✅ 已接受 Catch2" | 🟢 P3 |
| A2 | #4 | §1.7 表格缺 3 个源（CONTRIBUTING.md / adding-devices.md / glossary.md）| 🟢 P3 |
| A2 | #5 | §1.7 AGENTS.md 行 "未表态" → "Catch2 明确表态" | 🟢 P3 |
| A3 | #1 | §1.8 闭环证据小节（引用 v0.1.6 审计报告）| 🟢 P3 |
| A3 | #4 | SSOT 状态"🔄 待评审"卡顿（v0.1.7 合并后升 ✅ Approved）| 🟡 P2 |
| A3 | #5 | `docs/README.md` ADR-022 状态陈旧（"未启动" → ✅）| 🟡 P2 |
| A3 | #6 | §1.8 自指表述（"本文正是为此而写" → 过去时态）| 🟢 P3 |
| **A4** | **#1** | **附录 A `gpu_pushbuffer_args` 缺 `u64 va_space_handle` 字段（H-1 后未同步）**| **🔴 P0** |
| A4 | #2 | 附录 A 缺 `struct gpu_mmu_event_cb_args` 完整字段 | 🟡 P2 |
| A4 | #3 | 附录 A 缺 `struct gpu_firmware_cb_args` 完整字段 | 🟡 P2 |
| A4 | #4 | 附录 A 缺 `struct gpu_map_bo_args` 完整字段 | 🟡 P2 |
| A4 | #5 | 附录 A 缺 `struct gpu_wait_fence_args` 完整字段 | 🟡 P2 |
| A4 | #6 | 附录 A 缺 `struct gpu_register_gpu_args` 完整字段 | 🟡 P2 |
| A4 | #7 | 附录 A 缺 `struct gpu_queue_map_ring_args` 完整字段 | 🟡 P2 |
| A4 | #8 | 附录 A 缺 `struct gpu_queue_info_args` 完整字段 | 🟡 P2 |
| A4 | #9 | 附录 A 缺 `struct gpu_ring_header` 完整字段（含 `volatile` + `reserved[32]`）| 🟡 P2 |

### 剩余 8 项偏差（其他独立 changes 处理）

| 来源 | 编号 | 简述 | 对应 change |
|------|------|------|------------|
| A1 | #1 | sim/scheduler/translator 嵌套未在 §1.2 体现 | `cleanup-shadow-dead-code` |
| A1 | #2 | sim/hardware/ 布局分裂（.cpp 在 sim/ 根）| `fix-sim-hardware-layout` |
| A1 | #3 | sim/doorbell_emu.cpp 空壳（1 行 stub）| `cleanup-shadow-dead-code` |
| A1 | #4 | sim/buddy_allocator.cpp + sim/fence_sim.cpp 是 dead code | `cleanup-shadow-dead-code` |
| A2 | #1 | .github/copilot-instructions.md 残留 "Google Test" | `cleanup-gtest-residue` |
| A2 | #2 | CONTRIBUTING.md 残留 `apt install libgtest-dev` | `cleanup-gtest-residue` |
| A3 | #2 | AGENTS.md 0 处反向引用 SSOT | `fix-agents-md-ssot-link` |
| A3 | #3 | 3 个 `openspec/specs/*.md` TBD Purpose 占位 | `cleanup-orphan-spec-purpose` |

**Why now**:
- **1 项 P0 必修（A4 #1）** —— 阻碍 SSOT 作为"权威架构说明"的可信度；新贡献者按附录 A 初始化 `gpu_pushbuffer_args` 会缺 8 字节尾部字段，触发 ABI 兼容路径
- **8 项 P2 附录 A 完整性** —— 文档与代码不同步；新驱动开发者按 SSOT 初始化 struct 时缺字段或漏配置
- **3 项 P3 §1.7 表格自描述过期** —— SSOT 自我审计机制的可信度（§1.7 表格说"未实施"，但 ADR-010 实际已接受）

**Why single change（vs 拆分）**:
- 全部 17 项偏差都改 SSOT/文档**同一文件族**（`post-refactor-architecture.md` + `docs/README.md`）
- v0.1.6 审计（change `ssot-deep-audit`）和 v0.1.2 勘误（commit `4e5d5ea`）都"merged all SSOT changes in 1 commit"已建立模式
- 拆分到 5+ 个 changes 会导致**同一文件多处微改** → review 噪声 / 冲突
- 按"职责"切分（SSOT 整体 vs 单文件）是 OpenSpec 推荐的 change 粒度

## What Changes

**新能力**: `ssot-v0-1-7-comprehensive-fix` —— v0.1.6 审计的 SSOT 侧综合修复

**实施 4 大块**（按章节分组）:

### 块 1: 附录 A 完整化（A4 #1-#9，共 9 项）

- **A4 #1 必修**: `gpu_pushbuffer_args` 末尾追加 `u64 va_space_handle;` 字段 + H-1 注释
- **A4 #2-#6**: 补录 5 个 IOCTL 配套 struct 完整字段定义（`gpu_mmu_event_cb_args` / `gpu_firmware_cb_args` / `gpu_map_bo_args` / `gpu_wait_fence_args` / `gpu_register_gpu_args`）
- **A4 #7-#8**: 补录 2 个 gpu_queue.h struct 完整字段定义（`gpu_queue_map_ring_args` / `gpu_queue_info_args`）
- **A4 #9**: 补录 `gpu_ring_header` 完整字段定义（含 `volatile` 限定符 + `reserved[32]` 注释）

所有 8 个新补录的 struct 字段**直接从源代码 `plugins/gpu_driver/shared/gpu_ioctl.h` / `gpu_queue.h` 复制**（不"设计"，零偏差风险）。

### 块 2: §1.7 表格刷新（A2 #3, #4, #5，共 3 项）

- **A2 #3**: 行 9（ADR-010）"声明: 提议\"迁移到 GTest\"" / "实际: 未实施" → "声明: ✅ 已接受 Catch2" / "实际: Catch2（ADR 自身已对齐）"
- **A2 #4**: 表格新增 3 行（CONTRIBUTING.md / adding-devices.md / glossary.md），声明列与实际列均为 "Catch2"
- **A2 #5**: 行 3（AGENTS.md）"—（未表态）" → "Catch2（明确反对 GTest）"

### 块 3: §1.8 自描述更新（A3 #1, #4, #6，共 3 项）

- **A3 #1**: §1.8 新增"§1.8.1 闭环证据"小节，引用 `audit-reports/v0.1.6-audit.md`（25 项偏差的统计 + 4 区域链接）
- **A3 #4**: SSOT 头"🔄 待评审（v0.1.6 审计已完成，等待 fix 合并后升 ✅ Approved）" → "✅ Approved（v0.1.7）"
- **A3 #6**: "本文正是为此而写" → "已于 v0.1.5 创建本文闭环此 gap（v0.1.6 审计确认闭环证据见 §1.8.1）"

### 块 4: 跨文件 / 跨章节更新（A3 #5 + A1 #5，共 2 项）

- **A3 #5**: `docs/README.md` L222 "❌ **未启动**：022" → "✅ v1（operator-level emulation）"
- **A1 #5**: SSOT §1.5 `src/kernel/*.cpp` 计数 v0.1.1 声称 14 → 12（实际 12 个）

### 块 5: 变更记录

- SSOT §变更记录追加 v0.1.7 条目
- SSOT 底部"最后更新"日期更新到 2026-06-17，commit hash 留待本 change commit 时填

## Capabilities

### New Capabilities
- `ssot-v0-1-7-comprehensive-fix`: v0.1.6 审计的 SSOT 侧综合修复

### Modified Capabilities
- 无（其他 capability 不涉及 SSOT 修复本身）

## Impact

| 影响项 | 范围 | 风险 |
|--------|------|------|
| SSOT 文件 | `post-refactor-architecture.md` 多处编辑（附录 A / §1.7 / §1.8 / §0 / 状态 / 变更记录 / footer）| 中（多处编辑需仔细 review）|
| 文档侧 | `docs/README.md` ADR-022 状态行（1 行修改）| 低 |
| 测试 | 无影响（纯文档）| 极低 |
| 编译 | 无影响（纯文档）| 极低 |
| 跨 repo | 无影响 | 极低 |
| docs-audit | 必须仍 36/36 PASS | 中（附录 A 新增 8 个 struct 定义可能触发 markdown 链接或字数检查）|

**风险缓解**:
- 所有附录 A struct 字段定义**直接从源代码复制**（grep -A n 取实际行）—— 偏差风险 ≈ 0
- §1.7 / §1.8 更新基于 v0.1.6 审计报告的"建议"列 —— 措辞可与原 SSOT 风格对齐
- 单一 commit，docs-audit.sh --strict 守门
- 块与块之间用空行分隔，每个 struct 定义独立 `<pre>` 代码块 —— 易于 review

## 开放问题（待 design.md 解决）

1. **状态字段时序**: SSOT 头"🔄 待评审"应在本 change commit 时升 ✅，还是等所有 6 个 follow-up changes 合并后升？建议：本 change commit 时升 ✅（v0.1.7 合并的 SSOT 侧偏差已完成审计闭环；其他 follow-up changes 是代码/docs 侧，独立于 SSOT 状态）
2. **§1.8.1 闭环证据小节标题**: 用 "§1.8.1 闭环证据" / "§1.8.1 审计元数据" / "§1.8.1 历史审计" 哪个？建议："§1.8.1 闭环证据"（与 SSOT 章节编号风格一致；"审计"用"闭环"更直接）
3. **附录 A struct 顺序**: 8 个新补录 struct 按 IOCTL 编号顺序（与 SSOT 表格的"参数结构"列对齐）还是按字母顺序？建议：按 IOCTL 编号顺序（与 SSOT 表格一致）
4. **A4 #1 `va_space_handle` 注释**: H-1 注释应写"Phase 2 校验必填字段（commit `0272970`）"还是"sentinel: 0 = 跳过校验"？建议：两者都写（sentinel 在前，commit 在后）

**建议在 design.md 阶段解决**：本 change 仅做 SSOT 完整化 + 表格刷新 + 状态卡顿修复；所有"措辞风格"在 design.md 阶段定稿。

## 关联 Changes（与本 change 并行/后续）

| Change | 覆盖偏差 | 关系 |
|--------|----------|------|
| `fix-sim-hardware-layout` | A1 #2 | **代码+文档**，独立 commit；不依赖本 change |
| `cleanup-shadow-dead-code` | A1 #1, #3, #4 | **代码+文档**，独立 commit；不依赖本 change（建议在 `fix-sim-hardware-layout` 之后做）|
| `cleanup-gtest-residue` | A2 #1, #2 | 文档，独立 commit；不依赖本 change |
| `fix-agents-md-ssot-link` | A3 #2 | 文档，独立 commit；不依赖本 change |
| `cleanup-orphan-spec-purpose` | A3 #3 | 文档，独立 commit；不依赖本 change |

**6 个 follow-up changes 全部完成后** → SSOT 可升 v1.0（独立小 change 或本 change 内"v1.0 release"任务，可选）。
