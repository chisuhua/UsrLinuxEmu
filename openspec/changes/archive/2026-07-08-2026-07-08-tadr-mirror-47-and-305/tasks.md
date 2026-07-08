# Tasks: tadr-mirror-47-and-305

> **状态**: ✅ COMPLETED — 2026-07-08（commit `f679763` + `aac4be5`）
> **实施者**: Sisyphus
> **依赖**: TaskRunner submodule `2595f16` ✅ 已落地
> **关联**: 前置 change `archive/2026-07-08-2026-07-07-docs-tadr-mirror-sync/`（已 archive）

## Phase 1: Submodule Bump

- [x] **1.1** 确认 TaskRunner submodule HEAD 为 `2595f16`
  ```bash
  git -C external/TaskRunner rev-parse HEAD
  # 期望: 2595f165e0879bba0d9c1c2a8ecada7e5ff3f1fa
  ```

- [x] **1.2** 确认 tadr-305 文件存在
  ```bash
  ls external/TaskRunner/docs/shared/adr/tadr-305-mempool-export-shareable.md
  # 期望: 文件存在
  ```

- [x] **1.3** 确认 tadr-302-mempool-export-shareable.md 已被重命名（不再存在）
  ```bash
  ! ls external/TaskRunner/docs/shared/adr/tadr-302-mempool-export-shareable.md 2>/dev/null
  # 期望: exit 非 0（不存在）
  ```

- [x] **1.4** 在 UsrLinuxEmu 仓 bump 子模块指针
  ```bash
  GIT_MASTER=1 git add external/TaskRunner
  GIT_MASTER=1 git commit -m "chore(submodule): bump TaskRunner to 2595f16 (Phase 4 bridge + tadr-305 rename)"
  ```

## Phase 2: 镜像段更新 (`docs/00_adr/README.md`)

- [x] **2.1** 修改 line 288（tadr-301 行描述）
  ```diff
  -| [tadr-301](...) | IGpuDriver 28→46 方法契约 (...) | [ADR-032](...) |
  +| [tadr-301](...) | IGpuDriver 28→47 方法契约 (H-5 新增, H-3.5 + Phase 3 + Phase 4 扩展, 2026-07-07 PR #7, Phase 4 tadr-305) | [ADR-032](...), tadr-305 |
  ```

- [x] **2.2** 在 `tadr-304` 行后新增 `tadr-305` 行（line 292 后）
  ```markdown
  | [tadr-305](../external/TaskRunner/docs/shared/adr/tadr-305-mempool-export-shareable.md) | IGpuDriver::memPoolExportShareable 契约 (Phase 4 新增 47 方法) | tadr-301, [ADR-039](adr-039-mem-pool-export-ioctl.md) |
  ```

- [x] **2.3** 验证所有 `../external/TaskRunner/docs/shared/adr/tadr-*.md` 链接可解析
  ```bash
  # 列出所有 tadr 链接并验证
  grep -oE '\.\./external/TaskRunner/docs/shared/adr/tadr-[0-9]+-[a-z-]+\.md' docs/00_adr/README.md | while read link; do
    full="docs/00_adr/$link"
    [ -f "$full" ] || echo "MISSING: $full"
  done
  ```

## Phase 3: 修补 adr-039 过时引用

- [x] **3.1** 修改 `docs/00_adr/adr-039-mem-pool-export-ioctl.md:8`
  ```diff
  -**关联 TaskRunner TADR**: tadr-302 (sync primitives — pending mempool export extension)
  +**关联 TaskRunner TADR**: tadr-305 (IGpuDriver::mem_pool_export_shareable, Phase 4 新增 47 方法)
  ```

- [x] **3.2** 修改 `docs/00_adr/adr-039-mem-pool-export-ioctl.md:83`
  ```diff
  -TaskRunner `tadr-302` (sync primitives) 将引用本 ADR 的 IOCTL 编号和结构体定义，作为 `cuMemPoolExportToShareableHandle` Stub/C-ABI 层的基础。Phase 4 的 TaskRunner 实现（`phase3-real-impl-bridge-extended`）依赖于本 IOCTL 已合并到 UsrLinuxEmu main。
  +TaskRunner `tadr-305` (mempool_export_shareable) 已引用本 ADR 的 IOCTL 编号和结构体定义，作为 `cuMemPoolExportToShareableHandle` Stub/C-ABI 层的基础。TaskRunner commit `2595f16` (2026-07-08) 已完成真实桥接，依赖项（UsrLinuxEmu GPU_IOCTL_MEM_POOL_EXPORT 0x68，PR #27 commit f315c3e）已合并。
  ```

- [x] **3.3** 全仓搜索遗留 `tadr-302` mempool 引用（应只剩 sync-primitives）
  ```bash
  grep -rn "tadr-302" docs/ plugins/ src/ tests/ AGENTS.md | grep -v "sync-primitives"
  # 期望: 无输出
  ```

## Phase 4: 验证

- [x] **4.1** docs-audit 严格模式通过
  ```bash
  bash tools/docs-audit.sh --strict
  # 期望: ✅ Passed: 43+, ❌ Failed: 0, ⚠️ Warnings: 0, exit 0
  ```

- [x] **4.2** ctest 全测试通过（无代码改动，预期不退化）
  ```bash
  cd build && ctest --output-on-failure
  # 期望: 85/85 pass
  ```

## Phase 5: 提交

- [x] **5.1** 提交 docs(adr) 改动
  ```bash
  GIT_MASTER=1 git add docs/00_adr/README.md docs/00_adr/adr-039-mem-pool-export-ioctl.md
  GIT_MASTER=1 git diff --cached --stat
  GIT_MASTER=1 git commit -m "docs(adr): sync TaskRunner TADR mirror (TADR-301 47-method, TADR-305)"
  ```

- [x] **5.2** 验证最近 3 个 commit
  ```bash
  GIT_MASTER=1 git log --oneline -3
  # 期望:
  # <new>  docs(adr): sync TaskRunner TADR mirror (TADR-301 47-method, TADR-305)
  # <prev> chore(submodule): bump TaskRunner to 2595f16 (Phase 4 bridge + tadr-305 rename)
  # <prev> docs(openspec): add 12 change skeletons + archive C-01/C-02 (Phase 3 stabilization roadmap)
  ```

## Archive 触发条件

满足以下全部条件后可 archive 此 change：
- [x] 所有 phase 任务完成
- [x] `git log` 显示本 change 关联的 2 个 commit 已落地
- [x] `openspec list` 不再显示本 change（已 archive）
- [x] `openspec/changes/2026-07-08-tadr-mirror-47-and-305/` 移动到 `openspec/changes/archive/2026-07-08-2026-07-08-tadr-mirror-47-and-305/`

Archive 命令：
```bash
openspec archive 2026-07-08-tadr-mirror-47-and-305 --skip-specs --yes
```