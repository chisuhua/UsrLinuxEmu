# Tasks: docs-tadr-mirror-sync

> **状态**: 📋 PROPOSED
> **目标**: TADR mirror 段反映 TaskRunner main HEAD

## 1. 收集 TADR 清单（5 分钟）

- [ ] 1.1 `cd external/TaskRunner && ls docs/shared/adr/`
- [ ] 1.2 列出当前 main HEAD 的 TADR 文件
- [ ] 1.3 记录新增 / 变更的 TADR（对比上次 sync）

## 2. 更新 README（15 分钟）

- [ ] 2.1 读 `docs/00_adr/README.md` "TaskRunner TADR mirror" 段
- [ ] 2.2 加 / 改 TADR-301（IGpuDriver extension）
- [ ] 2.3 加 / 改 TADR-302（mem_pool_export）
- [ ] 2.4 验证每个 mirror 行 link target 在 submodule HEAD 存在

## 3. 验证（5 分钟）

- [ ] 3.1 `bash tools/docs-audit.sh --strict` §6 通过
- [ ] 3.2 检查所有 TADR link resolve to existing file
- [ ] 3.3 docs-audit 总 Exit code 0

## 4. commit / push（5 分钟）

- [ ] 4.1 commit：`docs(adr): mirror TaskRunner TADR-301 (IGpuDriver 28→46) + TADR-302 (mem_pool_export)`
- [ ] 4.2 push & open PR
