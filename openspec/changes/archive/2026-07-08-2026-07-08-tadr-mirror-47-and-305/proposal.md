# Change: tadr-mirror-47-and-305

> **状态**: 📋 PROPOSED
> **优先级**: 🟢 P2
> **创建**: 2026-07-08
> **前置 change**: 2026-07-07-docs-tadr-mirror-sync（已 archive，因目标发生变化）
> **依赖**: TaskRunner submodule `2595f16`（含 `tadr-305` rename）
> **工作目录**: `openspec/changes/2026-07-08-tadr-mirror-47-and-305/`

## Why

ADR-035 §Rule 5.1 Step 3：
> 如新增 TADR-NNN：更新 `docs/00_adr/README.md` "TaskRunner TADR mirror" 段

2026-07-07 至 2026-07-08 期间，TaskRunner 仓发生了两次需要镜像同步的变更：

1. **TADR-301 方法数扩展**：canonical 文件已从 46 方法扩展到 **47 方法**（Phase 4 新增 `mem_pool_export_shareable`）。本仓 README.md:288 仍写"28→46"。
2. **新增 TADR-305**：TaskRunner 把 `tadr-302-mempool-export-shareable` 重命名为 `tadr-305-mempool-export-shareable` 以解决与 `tadr-302-sync-primitives` 的编号冲突（commit `2595f16`，2026-07-08）。本仓 README.md 完全缺少 tadr-305 行。

附加修补：canonical `tadr-301-igpu-driver-contract.md` 引用了 tadr-305，本仓 `adr-039-mem-pool-export-ioctl.md:8,83` 仍有指向 `tadr-302 (sync primitives)` 的过时引用，必须改为 `tadr-305`。

**为什么不修复 `superpowers/plans/2026-06-24-h5-taskrunner-scope-clarification.md` 中"28 methods"**：
该计划是 2026-06-24 写时的历史快照（当时 IGpuDriver 确实只有 28 方法）。改它会破坏历史记录语义。该文件不在镜像段范围内。

## What Changes

### 1. 子模块指针 bump

`external/TaskRunner`: `fbcbe44` → `2595f16`

新提交链：
- `48a5c34` openspec(archive): archive phase3-real-impl-bridge (Phase 4 complete)
- `80d4596` docs(sync-plan): mark Phase 3.1+3.2 Step 4 + Phase 4 real-impl-bridge complete (v2.4.1)
- `2595f16` feat(gpu-driver-client): complete Phase 4 bridge (mem_pool_export_shareable real forwarder) + rename tadr-302 → tadr-305

### 2. `docs/00_adr/README.md` 镜像段修改

**修改 line 288（tadr-301 行）**：
```diff
-| [tadr-301](...) | IGpuDriver 28→46 方法契约 (H-5 新增, H-3.5 + Phase 3 扩展, 2026-07-07 PR #7) | [ADR-032](...) |
+| [tadr-301](...) | IGpuDriver 28→47 方法契约 (H-5 新增, H-3.5 + Phase 3 + Phase 4 扩展, 2026-07-07 PR #7, Phase 4 tadr-305) | [ADR-032](...), tadr-305 |
```

**新增 line 292 后（tadr-305 行）**：
```markdown
| [tadr-305](../external/TaskRunner/docs/shared/adr/tadr-305-mempool-export-shareable.md) | IGpuDriver::memPoolExportShareable 契约 (Phase 4 新增 47 方法) | tadr-301, [ADR-039](adr-039-mem-pool-export-ioctl.md) |
```

### 3. `docs/00_adr/adr-039-mem-pool-export-ioctl.md` 修补

**Line 8**：
```diff
-**关联 TaskRunner TADR**: tadr-302 (sync primitives — pending mempool export extension)
+**关联 TaskRunner TADR**: tadr-305 (IGpuDriver::mem_pool_export_shareable, Phase 4 新增 47 方法)
```

**Line 83**：
```diff
-TaskRunner `tadr-302` (sync primitives) 将引用本 ADR 的 IOCTL 编号和结构体定义，作为 `cuMemPoolExportToShareableHandle` Stub/C-ABI 层的基础。Phase 4 的 TaskRunner 实现（`phase3-real-impl-bridge-extended`）依赖于本 IOCTL 已合并到 UsrLinuxEmu main。
+TaskRunner `tadr-305` (mempool_export_shareable) 已引用本 ADR 的 IOCTL 编号和结构体定义，作为 `cuMemPoolExportToShareableHandle` Stub/C-ABI 层的基础。TaskRunner commit `2595f16` (2026-07-08) 已完成真实桥接，依赖项（UsrLinuxEmu GPU_IOCTL_MEM_POOL_EXPORT 0x68，PR #27 commit f315c3e）已合并。
```

## Acceptance

- [ ] `external/TaskRunner` 指针为 `2595f16`
- [ ] `docs/00_adr/README.md` line 288 描述含"28→47"
- [ ] `docs/00_adr/README.md` line 292 后存在 `tadr-305` 行
- [ ] `docs/00_adr/README.md` 所有 `../external/TaskRunner/docs/shared/adr/tadr-*.md` 链接可解析（docs-audit 0 死链）
- [ ] `docs/00_adr/adr-039-mem-pool-export-ioctl.md:8` 引用 `tadr-305`
- [ ] `docs/00_adr/adr-039-mem-pool-export-ioctl.md:83` 引用 `tadr-305`
- [ ] `bash tools/docs-audit.sh --strict` exit 0，0 warnings
- [ ] `ctest` 仍 85/85 pass（无代码改动，应保持）

## 提交拆分

| # | 提交 | 类型 | 范围 |
|---|------|------|------|
| 1 | `chore(submodule): bump TaskRunner to 2595f16 (Phase 4 bridge + tadr-305 rename)` | chore | `external/TaskRunner` 指针 |
| 2 | `docs(adr): sync TaskRunner TADR mirror (TADR-301 47-method, TADR-305)` | docs | `docs/00_adr/README.md` + `docs/00_adr/adr-039-mem-pool-export-ioctl.md` |

## 测试方法

```bash
# 1. 子模块状态确认
git -C external/TaskRunner rev-parse HEAD  # 期望: 2595f16...
ls external/TaskRunner/docs/shared/adr/ | grep "tadr-305"  # 期望: tadr-305-mempool-export-shareable.md

# 2. 镜像段验证
grep -A 1 "tadr-301" docs/00_adr/README.md  # 期望: 47 方法
grep -A 1 "tadr-305" docs/00_adr/README.md  # 期望: 47 方法 row

# 3. 死链检查
bash tools/docs-audit.sh --strict  # 期望: exit 0, 43 passed

# 4. 全测试
cd build && ctest --output-on-failure  # 期望: 85/85 pass
```

## 相关链接

- TaskRunner commit `2595f16`（Phase 4 bridge + tadr-305 rename）
- TaskRunner change `mempool-export-shareable-real-bridge`（openspec/changes/mempool-export-shareable-real-bridge/）
- UsrLinuxEmu PR #27 (f315c3e, MEM_POOL_EXPORT IOCTL 0x68)
- UsrLinuxEmu PR #26 (edeee6e, ioctl dispatch coverage)
- ADR-035 §Rule 5.1 Step 3（cross-repo mirror sync 规则）
- 前置 change：`archive/2026-07-08-2026-07-07-docs-tadr-mirror-sync/`（因目标变更已 archive）