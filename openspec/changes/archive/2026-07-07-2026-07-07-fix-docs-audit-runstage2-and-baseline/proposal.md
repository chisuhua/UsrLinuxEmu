# Change: fix-docs-audit-runstage2-and-baseline

> **状态**: 📋 PROPOSED
> **优先级**: 🔴 P0（阻塞 main CI + PR #26）
> **创建**: 2026-07-07
> **阻塞**: PR #26 merge, main CI green, C-02/C-05
> **工作目录**: `openspec/changes/2026-07-07-fix-docs-audit-runstage2-and-baseline/`

## Why

docs-audit.sh 脚本两个 pre-existing bug，使 CI docs-audit job 持续 FAIL：

1. **`RUN_STAGE2` 未初始化**（line 720-721）：脚本只初始化 `RUN_ARCH/IOCTL/ADR/DOC/BUILD/SYNC`，漏了 `RUN_STAGE2`。bash 在 strict 模式下报错 `[: : integer expression expected`。本应在 line 49-58 后加 `RUN_STAGE2=0`。

2. **§2.6 期望值硬编码 13**（line 364-371）：硬编码为 pre-PR-#20 baseline `13`，实际 `kNumIoctls` 已经因 PR #20 + PR #27 涨到 16，PR #26 合入后会到 31。文档与代码真实状态 drift。

**当前影响**：
- main HEAD `5d8c200` CI: docs-audit FAIL（阻塞 main CI）
- PR #26: docs-audit FAIL（阻塞 PR #26 merge）
- 阻塞所有 Stage 3 + Phase 4 后续 PR

## What Changes

### 1. RUN_STAGE2 初始化修复

**`tools/docs-audit.sh` line 49-58 区域**：
```bash
RUN_ARCH=0
RUN_IOCTL=0
RUN_ADR=0
RUN_DOC=0
RUN_BUILD=0
RUN_SYNC=0
RUN_STAGE2=0      # ← 新增
```

### 2. §2.6 期望值动态化

**`tools/docs-audit.sh` line 362-371**：
- 当前：硬编码 expected `13`
- 改为：检查 kNumIoctls 是否 ≥ baseline (baseline 在脚本顶部 `BASELINE_KNUMIOCTLS=13`)，warn if < baseline or != post-PR expectation
- 给每个 phase 维护独立的 expected 值（注释化）

## Acceptance

- [ ] `bash tools/docs-audit.sh --strict` 退出码为 0
- [ ] CI docs-audit job ✅ PASS
- [ ] 不引入新 warning（warning count ≤ 0）
- [ ] PR #26 mergeable
- [ ] main HEAD CI 恢复 all green

## 测试方法

```bash
# 本地验证
bash tools/docs-audit.sh --strict
echo "exit=$?"  # 期望 0

# 复用现有 84 测试
cd build && ctest
```

## Cross-Repo 影响

无。纯 UsrLinuxEmu toolchain 内部修复。

## Dependencies

None

## Unblocks

- C-02 (stage3-ioctl-dispatch-completeness)
- C-05 (stage3-3-errno-coverage-audit)
- 所有后续 PR merge
