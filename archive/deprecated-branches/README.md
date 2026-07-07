# Deprecated Branches Index

> **用途**: 记录已被删除的远程分支，作为历史归档。
>
> 当一个远程分支包含重要内容但被删除时，在删除前必须先在本目录创建 README 条目，
> 记录分支名、最终 commit hash、内容摘要、删除原因等信息。
>
> **最后更新**: 2026-07-07

---

## 删除记录

### 2026-07-07 · `copilot/refine-development-steps`

| 字段 | 值 |
|------|-----|
| **分支名** | `origin/copilot/refine-development-steps` |
| **最终 commit** | `6e64363f9a1f76a9c267da170d449d02e7d15aa3` |
| **Merge base with main** | `d9eadbd696f4d2087613e6c03033d397bc6f2f0e` |
| **作者** | `copilot-swe-agent[bot] <198982749+Copilot@users.noreply.github.com>` |
| **Co-author** | `chisuhua <16367148+chisuhua@users.noreply.github.com>` |
| **Merge base 日期** | 2026-02-25 |
| **删除日期** | 2026-07-07 |
| **删除原因** | 包含重大破坏性内容，未合并到 main，被项目所有者拒绝 |
| **合并状态** | ❌ **未合并到 main**（含 6 个独有 commit）|

#### 独有 commit 列表（未在 main 中）

| Commit | 作者 | 说明 |
|--------|------|------|
| `8869c37` | copilot-swe-agent[bot] | Initial plan |
| `528e58d` | copilot-swe-agent[bot] | Initial plan for linux_compat layer implementation |
| `7a5ffe4` | copilot-swe-agent[bot] | Add linux_compat layer: cdev, device, sync, interrupt, pci, resource, module, debug headers with tests |
| `25d19a4` | copilot-swe-agent[bot] | Remove build directory from git tracking |
| `38bf73d` | copilot-swe-agent[bot] | Implement Linux kernel API compatibility layer (Phases 2–5) |
| `6e64363` | chisuhua | Merge branch 'main' into copilot/refine-development-steps |

#### 内容摘要

- **总变更**: 987 文件, +40,671 行 / **−94,725 行**
- **文件删除**: **517 个** ⚠️
- **文件新增**: 数百个（主要在 `_codeql_build_dir/` — 应该是 gitignored 的 build artifacts）

#### 🔴 关键问题（为何拒绝合并）

1. **Build artifacts 污染**: 引入 `_codeql_build_dir/` 整个目录（应为 gitignored）
2. **关键工具被删除**:
   - `AGENTS.md`（项目权威开发指南）
   - `tools/docs-audit.sh`（文档审计工具）
   - `docs/00_adr/README.md`（ADR 索引）
   - `.clang-format`, `.clang-tidy`, `.editorconfig`（开发配置）
3. **关键测试被删除**:
   - `tests/test_va_space.cpp`
   - `tests/test_uvm_drm_lifecycle_standalone.cpp`
   - `tests/test_vfs_path_standalone.cpp`
   - `tests/test_zone_device_standalone.cpp`
   - `tests/test_uvm_headers_standalone.cpp`
   - `tests/test_update_queue_runtime_standalone.cpp`
4. **Archive 内容被批量删除**:
   - `archive/old_gpu_device/*`（24+ 文件）
   - `archive/system_b_drivers/gpu/*`（24+ 文件）
   - `archive/system_b_examples/*`（4 文件）
   - `archive/historical-plans-2026-06-15/*`（8 文件）
   - `archive/orphaned_simulator/gpu/*`（部分）

#### 恢复说明

如需恢复该分支内容：

```bash
# 通过 reflog 在本地找回（90 天内）
git reflog | grep copilot/refine-development-steps
git checkout -b copilot/refine-development-steps-recovered <commit-hash>

# 或从 GitHub Events API 恢复（如果事件未被清理）
gh api repos/chisuhua/UsrLinuxEmu/events --paginate | jq '.[] | select(.type=="DeleteEvent")'
```

**注意**: 90 天后本地 reflog 可能被 GC，建议短期内若需要恢复请立即备份。

#### 决策记录

- **决策人**: 项目所有者（chisuhua）
- **决策依据**: docs-audit 工具链一致性 + 关键测试/工具保护 + archive 内容不可删原则
- **关联变更**: 本次清理随 `stage-3-v1.0-docs-cleanup` 合并后执行（commit `25884e9`）

---

## 维护说明

### 新增条目

当删除一个有内容的远程分支时：

1. 复制本文件中的"删除记录"模板
2. 填充分支名、最终 commit、内容摘要、删除原因
3. 更新"最后更新"日期
4. 提交变更（commit message 建议：`docs(archive): record deprecation of <branch-name>`）

### 模板

```markdown
### YYYY-MM-DD · `<branch-name>`

| 字段 | 值 |
|------|-----|
| **分支名** | `origin/<branch-name>` |
| **最终 commit** | `<hash>` |
| **Merge base with main** | `<hash>` |
| **作者** | `<author>` |
| **删除日期** | YYYY-MM-DD |
| **删除原因** | <why> |
| **合并状态** | ❌ / ✅ |

#### 独有 commit 列表（未在 main 中）

| Commit | 作者 | 说明 |
|--------|------|------|
| ... |

#### 内容摘要

- **总变更**: ...
- **文件删除**: ...
- **文件新增**: ...

#### 🔴 关键问题（如有）

...

#### 恢复说明

...
```