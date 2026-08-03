# Tasks: Stage 4 Portability L2 — Linux 6.12 LTS Build Harness

> 实施 ADR-072 L2 验证框架，把 drv/ 编译到 Linux 6.12 LTS 内核源码树作为独立模块。

---

## 1. Build harness skeleton

- [ ] 1.1 创建 `tools/ci/l2-portability/` 目录
- [ ] 1.2 创建 `tools/ci/l2-portability/build-drv-against-linux-6.12.sh` (design.md §"Build script design" skeleton)
- [ ] 1.3 创建 `tools/ci/l2-portability/README.md`：操作说明 + 已知假设
- [ ] 1.4 chmod +x on build script
- [ ] 1.5 Local smoke test：`./build-drv-against-linux-6.12.sh` 至少能启动（kernel fetch 阶段 OK）

## 2. Symlink strategy

- [ ] 2.1 drv/ symlink to `drivers/gpu/emu-drv/` 在 kernel tree 内
- [ ] 2.2 `include/` symlink strategy：要么只暴露 drv/ 直接依赖的子目录（`include/linux_compat/`, `include/kernel/`），要么完整 symlink
- [ ] 2.3 Verify symlinks 不破坏 KPI 三区分边界（grep 测试）

## 3. CI workflow

- [ ] 3.1 创建 `.github/workflows/l2-portability.yml`
- [ ] 3.2 matrix：ubuntu-20.04 + ubuntu-22.04 × kernel 6.6 + 6.12（共 4 job 配置）
- [ ] 3.3 build log artifact 上传策略（on failure）
- [ ] 3.4 PR 触发路径：仅当 `plugins/gpu_driver/drv/**` 或 `include/**` 变更
- [ ] 3.5 必需的 GitHub Actions 权限（contents: read / pull-requests: write 用于 auto-comment）

## 4. 首次 baseline 运行

- [ ] 4.1 Local run 在 Ubuntu 22.04 + kernel 6.12 LTS：build OK + warnings ≤ baseline threshold
- [ ] 4.2 Document baseline warnings（如果 N>0）→ 分类：fixable via include path vs 需要 `linux_compat/` extension
- [ ] 4.3 记录 baseline 输出到 `docs/04-building/portability-l2-baseline-2026-08.md`

## 5. Documentation

- [ ] 5.1 创建 `docs/04-building/portability-l2-guide.md`：操作手册
  - [ ] 5.1.1 Prerequisites：cache dir、kernel tarball、build tools (gcc, make, libelf-dev)
  - [ ] 5.1.2 完整流程：从 clean checkout 到 L2 PASS
  - [ ] 5.1.3 常见错误：mismatch list interpretation
  - [ ] 5.1.4 ADR-072 §L2 rules 的项目级映射
- [ ] 5.2 更新 `docs/README.md` 中 04-building 索引 + 链接
- [ ] 5.3 更新顶层 `AGENTS.md` §"构建命令"提及 L2 流程

## 6. Compatibility gap analysis (if warnings > 0)

- [ ] 6.1 分类 warnings：fixable via include path vs 需要 `linux_compat/` shim extension
- [ ] 6.2 Per ADR-035 §Rule 5.3：include path-only changes 不视为 drv/ 修改（保持 L2 验收 pass）
- [ ] 6.3 大于 0 个 mismatch 时：是否需要 sibling change 处理 linux_compat 扩展
- [ ] 6.4 如果 warnings = 0：直接 flip Stage 4 整体验收 §② 至 [x]

## 7. 集成验证

- [ ] 7.1 Pre-commit hook 不破：现有 hooks 继续工作
- [ ] 7.2 Local CI 模拟：`act` (GitHub Actions local runner) 验证 workflow 语法
- [ ] 7.3 第一条 PR 在本 change merged 后触发 L2 workflow：expected to PASS
- [ ] 7.4 docs-audit clean：`tools/docs-audit.sh --strict` 输出无新增失败

## 8. 维护与归档

- [ ] 8.1 `openspec archive` 本 change
- [ ] 8.2 INDEX.md 同步
- [ ] 8.3 roadmap Stage 4 整体验收 §② 翻 [x]（如果 L2 PASS）
- [ ] 8.4 `stage-4-bar-ioremap.md` §"Stage 4 整体验收" — ② checkbox closed
- [ ] 8.5 `gap-analysis.md` §"基础设施差距 §6 总结" 更新（如需要）

---

## 估计工作量

| Phase | Tasks | 估计 |
|-------|-------|------|
| Build script + symlink (T1-T2) | 1.1-2.3 + smoke test | 3-5 hrs |
| CI workflow (T3) | 3.1-3.5 | 2-3 hrs |
| Baseline + docs (T4-T5) | 4.1-5.3 | 4-6 hrs (含可能的 gap 分析) |
| Gap analysis (T6, conditional) | 6.1-6.4 | 0-8 hrs (if warnings=0, skip) |
| Integration (T7-T8) | 7.1-8.5 | 1-2 hrs |
| **Total** | | **10-24 hrs (1-3 sessions)** |
