# Design — ADR 治理刷新 v2

## 目标

把 22 个 Accepted ADR 的 `**状态**:` 字段统一为单一格式,修复 ADR-027 不一致,刷新 `docs/README.md` 计数。

## 标准化格式选择

候选格式与权衡:

| 候选 | 优点 | 缺点 |
|---|---|---|
| `✅ 已接受` | 简洁,中文友好 | 缺英文提示 |
| `已接受 (Accepted)` | 中英双显 | 无 emoji,与索引表样式不一致 |
| `✅ 已接受 (Accepted)` | **emoji + 中英双显 + 与索引表完全对齐** | 略长 |
| `✅ 已接受 (v1)` | 标记版本号 | 但 v1 含义不统一(022/024/031 已用,其他不用) |

**决定**:采用 `**状态**: ✅ 已接受 (Accepted)` 作为统一格式。

理由:
1. 与 `docs/00_adr/README.md` 索引表样式完全一致(索引表就用 `✅ 已接受` 列)
2. 中英双显,便于国际化协作
3. v1 后缀移除:不是所有 ADR 都有显式 v1 修订记录,加 v1 会引入虚假信号

## 改动分组

按文件类型分 3 组 commit:

1. **22 个 ADR 文件**:`**状态**:` 行替换(sed 风格批量)
2. **`docs/00_adr/README.md`**:22 行索引表状态列更新 + ADR-027 行修复
3. **`docs/README.md`**:ADR 计数 + 编号说明段更新 + `06-reference/adr.md` 引用检查

**合并决策**:为减少 commit noise,合并为 **1 个 commit**(符合之前 9 个 proposal.md 修复的"批量格式修复"惯例)

## 验证

- `bash tools/docs-audit.sh --strict` 通过
- `git grep "已接受"` 输出只剩 22 行 + 索引表若干行(无遗漏的旧格式)
- `git grep "🔄 提议中"` 不再返回 ADR-027 行
- `docs/README.md` 中"ADR" 计数与 `ls docs/00_adr/adr-*.md | wc -l` 一致(31)

## 不做的事

- 不新增 ADR 编号(025-031 维持 v0/v0.1 占位/Deferred 状态)
- 不修改 ADR-011~014(保留提议中状态,合法 backlog)
- 不修改 ADR-025/026/028~030(保留 Deferred 状态,Phase 3 触发后才升级)
- 不修改 `tools/docs-audit.sh`(本次不新增 ADR 状态一致性检查;留作未来增量)