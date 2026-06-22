# ADR 治理刷新 v2 — 状态字段格式标准化 + ADR-027 不一致修复

## Why

ADR 目录存在三类治理漂移,影响机器可审计性与新人 onboarding:

1. **ADR-027 文件 vs 索引表不一致**(真实 bug):文件正文 `**状态**: ✅ 已接受`,但 `docs/00_adr/README.md` 索引表标记为 `🔄 提议中 (Phase 3+ 占位骨架)`。
2. **22 个 Accepted ADR 的状态字段 4 种格式混用**:`已接受` / `✅ 已接受` / `已接受 (Accepted)` / `✅ 已接受 (v1)`。docs-audit 无法做格式一致性检查,人眼看也累。
3. **`docs/README.md` ADR 计数过时**:声称"24 份 ADR,025-031 未启动",实际已 31 份且 025-031 已有 v0/v0.1 占位骨架。

## What Changes

- **修复 ADR-027 文件↔索引不一致**:以文件正文(✅ Accepted)为准,更新 `docs/00_adr/README.md` 索引表对应行
- **标准化 22 个 Accepted ADR 的状态字段**(22 文件 + 索引表):统一为 `**状态**: ✅ 已接受 (Accepted)`
- **同步 `docs/00_adr/README.md` 索引表** 中 22 个 Accepted 行的状态列
- **刷新 `docs/README.md`** ADR 计数与编号说明段(24→31;025-031 状态纠正)
- **检查并修正 `docs/README.md` 中 `06-reference/adr.md` 引用**(如不存在则改为 `00_adr/README.md`)

无 BREAKING 变更(纯文档治理)。

## Capabilities

### New Capabilities

无(不新增 spec 域)

### Modified Capabilities

无(不修改 spec REQUIREMENTS,只改 doc 格式)

## Impact

- **受影响文件**(共 25+):
  - `docs/00_adr/adr-001..adr-031.md`(22 个 Accepted 文件 1 行修改)
  - `docs/00_adr/README.md`(索引表 22+ 行 + ADR-027 修复)
  - `docs/README.md`(ADR 计数 + 编号说明段)
- **不影响**:代码、API、测试、CI、构建
- **风险评估**:零功能风险,只影响文档格式与一致性
- **回滚成本**:单 commit revert 即可