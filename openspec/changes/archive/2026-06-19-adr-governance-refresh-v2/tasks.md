# Tasks — ADR 治理刷新 v2

## 1. 准备

- [x] 1.1 用 `grep -lP '^\*\*状态\*\*: (✅ )?已接受( \(Accepted\))?$' docs/00_adr/adr-*.md` 确认 22 个目标文件清单
- [x] 1.2 用 `git show HEAD:docs/00_adr/README.md | grep -c "✅ 已接受"` 记录改动前行数作为对照

## 2. 标准化 22 个 ADR 文件的状态字段

- [x] 2.1 把 ADR-001~009 的 `**状态**: 已接受` → `**状态**: ✅ 已接受 (Accepted)`(9 文件)
- [x] 2.2 把 ADR-010 的 `**状态**: ✅ 已接受` → `**状态**: ✅ 已接受 (Accepted)`
- [x] 2.3 把 ADR-015~021 + ADR-023 的 `**状态**: 已接受 (Accepted)` → `**状态**: ✅ 已接受 (Accepted)`(8 文件,无变化但走流程)
- [x] 2.4 把 ADR-022/024/031 的 `**状态**: ✅ 已接受 (v1)` → `**状态**: ✅ 已接受 (Accepted)`(3 文件,去 v1 后缀)
- [x] 2.5 验证:`git grep "已接受" docs/00_adr/adr-*.md` 输出全部为 `**状态**: ✅ 已接受 (Accepted)`

## 3. 修复 ADR-027 不一致 + 更新索引表

- [x] 3.1 把 `docs/00_adr/README.md` 索引表中 ADR-027 行的 `🔄 提议中 (Phase 3+ 占位骨架)` → `✅ 已接受 (Phase 3 触发后细化)`
- [x] 3.2 同步更新 22 个 Accepted ADR 的索引表状态列(从混合格式统一为 `✅ 已接受`)

## 4. 刷新 docs/README.md

- [x] 4.1 "ADR | 24 | 90%" → "ADR | 31 | 95%"
- [x] 4.2 ADR 编号说明段:"24 份 ADR,编号范围 001–024" → "31 份 ADR,编号范围 001–031(含 022, 025–031 为占位/Deferred)"
- [x] 4.3 检查 `06-reference/adr.md` 是否存在;不存在则把引用改为 `00_adr/README.md`
- [x] 4.4 更新"最后更新"日期为 2026-06-19 + 当前 commit hash

## 5. 验证 + 提交

- [x] 5.1 `bash tools/docs-audit.sh --strict` 必须通过(36 passed / 0 failed 维持)
- [x] 5.2 `git diff --stat` 确认改动文件数(预期 25: 22 ADR + README 索引表 + docs/README.md)
- [x] 5.3 1 个 commit, message `docs(adr): standardize status field format across 22 Accepted ADRs (governance refresh v2)`
- [x] 5.4 通过 `openspec archive adr-governance-refresh-v2` 归档

## 6. 收尾

- [x] 6.1 验证 `openspec list` 重新回到 "No active changes found"
- [x] 6.2 报告归档结果给用户