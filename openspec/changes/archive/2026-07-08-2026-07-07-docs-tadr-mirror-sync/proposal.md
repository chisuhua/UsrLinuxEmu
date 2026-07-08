# Change: docs-tadr-mirror-sync

> **状态**: 📋 PROPOSED
> **优先级**: 🟢 P2
> **创建**: 2026-07-07
> **依赖**: 无（可与 C-02/C-03 并行）
> **工作目录**: `openspec/changes/2026-07-07-docs-tadr-mirror-sync/`

## Why

ADR-035 §Rule 5.1 Step 3：
> 如新增 TADR-NNN：更新 docs/00_adr/README.md "TaskRunner TADR mirror" 段

TaskRunner 仓最近添加/更新：
- `TADR-301` IGpuDriver 28→31→46 扩展（Phase 3.1+3.2）
- `TADR-302` mem_pool_export_shareable 新方法

UsrLinuxEmu `docs/00_adr/README.md` 的 TADR mirror 段需同步。

## What Changes

### 文件改动

- **`docs/00_adr/README.md`** "TaskRunner TADR mirror" 段：
  - 添加 TADR-301 row（IGpuDriver 31→46 extension）
  - 添加 TADR-302 row（mem_pool_export）
  - 验证其他 TADR 链接未 stale

### 检测方法

```bash
ls external/TaskRunner/docs/shared/adr/      # TADR 文件清单
grep "TaskRunner TADR\|TADR-" docs/00_adr/README.md  # 当前 mirror 行
```

## Acceptance

- [ ] `docs/00_adr/README.md` "TaskRunner TADR mirror" 段反映 TADR main HEAD
- [ ] TADR-301 / TADR-302 已 mirror
- [ ] 无 stale 链接
- [ ] docs-audit §6.4-§6.7 通过（cross-repo doc consistency）
- [ ] commit：`docs(adr): sync TaskRunner TADR mirror (TADR-301, TADR-302)`

## 测试方法

```bash
bash tools/docs-audit.sh --strict  # §6 检查通过
ls external/TaskRunner/docs/shared/adr/  # 列举实际文件
```

## Cross-Repo 影响

无代码改动。纯文档同步。
