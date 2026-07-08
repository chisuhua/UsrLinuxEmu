# Tasks: fix-docs-audit-runstage2-and-baseline

> **状态**: 📋 PROPOSED
> **目标**: 让 `bash tools/docs-audit.sh --strict` 退出码 0

## 1. RUN_STAGE2 修复（5 分钟）

- [ ] 1.1 检查 `tools/docs-audit.sh` line 49-58 区域当前状态
- [ ] 1.2 在 `RUN_SYNC=0` 之后添加 `RUN_STAGE2=0`
- [ ] 1.3 本地验证：`bash tools/docs-audit.sh` exit 0
- [ ] 1.4 本地验证：`bash tools/docs-audit.sh --strict` exit 0
- [ ] 1.5 commit：`fix(docs-audit): initialize RUN_STAGE2 to suppress bash arithmetic warning`
- [ ] 1.6 push 并触发 CI 重跑

## 2. §2.6 期望值动态化（15 分钟）

- [ ] 2.1 找到 `tools/docs-audit.sh` line 362-371 当前代码
- [ ] 2.2 在脚本顶部 globals 添加：`BASELINE_KNUMIOCTLS=13`（pre-Phase 3 baseline）
- [ ] 2.3 §2.6 check 改为：
  - PASS if `kNumIoctls >= BASELINE_KNUMIOCTLS`
  - WARN if `kNumIoctls != expected current value`（expected = `BASELINE + phase additions`，注释化各 phase）
- [ ] 2.4 本地验证：warning count ≤ 1（原 1 bash warning 已修）
- [ ] 2.5 commit：`fix(docs-audit): §2.6 drift check — make kNumIoctls baseline dynamic`
- [ ] 2.6 push 并触发 CI

## 3. 验证 / 完成

- [ ] 3.1 main CI 全绿（3 build configs + docs-audit = 4 ✅）
- [ ] 3.2 PR #26 mergeable（CI green）
- [ ] 3.3 向 PR #26 owner 通知："docs-audit 已修，可 merge"
