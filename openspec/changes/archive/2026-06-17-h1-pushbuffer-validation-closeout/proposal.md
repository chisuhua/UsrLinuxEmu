# Change: h1-pushbuffer-validation-closeout

> **状态**: 🔄 Proposed（待用户在 `/opsx-apply` 中执行）
> **创建**: 2026-06-17
> **前置依赖**: change `fix-gpu-pushbuffer-va-space-validation`（已完成 5/6 任务组，2 项遗留）
> **关联 SSOT**: `docs/02_architecture/post-refactor-architecture.md` §1.3 v0.1.3, §变更记录

## Why

H-1（`fix-gpu-pushbuffer-va-space-validation`）5/6 任务组已完成、build + ctest 34/34 + docs-audit 36/36 全部通过，但 `tasks.md` 中**两项遗留**导致 H-1 闭环不完整：

1. **任务 §1.2 跨仓库同步未做**：`external/TaskRunner/include/gpu_driver_client.h:227` 仍用 `struct gpu_pushbuffer_args args = {};`（零初始化），新字段 `va_space_handle=0` 走 sentinel 分支绕过校验。TaskRunner 的 pushbuffer 调用**事实上没有走 VA Space 校验**，与 H-1 设计的"全链路 Phase 2 强制"承诺不符。
2. **任务 §6.2 archive 目录 git 跟踪丢失**：commit `744ef46` "update" 删除了原 change 目录的 git 跟踪，但 archive 目录的 `git add` 未执行。当前 `openspec/changes/archive/2026-06-17-fix-gpu-pushbuffer-va-space-validation/` 文件**在 filesystem 存在但 git 不可见**，新人 `git clone` 后 archive 是空的——审计追溯链断裂。

**Why now**：
- 跨仓库 sync 推迟越久，TaskRunner 与主仓库的 IOCTL 结构体 ABI 漂移越大；下一次 H-1 类变更时 TaskRunner 会先构建失败
- archive 目录如果误删（`rm -rf`）就**永久丢失**了原始 change 决策的设计记录（proposal.md 写的 Why / design.md 写的 D1-D6 决策）
- 距 H-1 完成已数小时，事后补做比回炉重做成本低

## What Changes

**1. 跨仓库同步（TaskRunner）**：
- 在 `external/TaskRunner/` 创建同步 commit（独立 PR 模式或子模块内 commit，取决于用户工作流）
- `external/TaskRunner/include/gpu_driver_client.h` 在提交 pushbuffer 处补 `va_space_handle` 字段（如果调用方有 VA Space handle 则透传，否则留 0）
- 同步 `external/TaskRunner/plans/sync-plan.md` 记录 S3.1 → "已加 va_space_handle 透传"
- 验证 `external/TaskRunner` 自身构建通过

**2. 恢复 archive 目录 git 跟踪**：
- `git add openspec/changes/archive/2026-06-17-fix-gpu-pushbuffer-va-space-validation/` 把 filesystem 文件纳入 git
- 验证 `git ls-tree HEAD openspec/changes/archive/` 显示文件
- 不修改文件内容（保留 H-1 完成时的状态）

**3. 文档同步**：
- SSOT `post-refactor-architecture.md` §1.3 v0.1.3 段落加注 "TaskRunner 已同步，change h1-pushbuffer-validation-closeout"
- 变更记录表新增 v0.1.4 行，引用本 change

**4. 验证 + 归档**：
- build / ctest / docs-audit 仍全部通过
- `openspec archive h1-pushbuffer-validation-closeout` 归档

## Capabilities

### New Capabilities
- `gpu-pushbuffer-validation-deployment`: 跟踪 H-1 验证特性的**部署完整性**（跨仓库 + git 历史）。一旦所有 Requirements 满足，本 capability 可归档。

### Modified Capabilities
- 无（H-1 主 capability `gpu-pushbuffer-validation` 的 spec 不需修改——closeout 是部署/治理层问题，不是行为层变化）

## Impact

| 影响项 | 范围 | 风险 |
|--------|------|------|
| 跨仓库 PR | `external/TaskRunner/include/gpu_driver_client.h` + 可能的 plans/sync-plan.md | **低**：零初始化保持 ABI 兼容；显式传 va_space_handle 是新行为，可选 |
| Submodule 指针 | `external/TaskRunner` 的 gitlink | **低**：取决于用户工作流（submodule update vs 独立 PR）|
| Archive git 跟踪 | `openspec/changes/archive/2026-06-17-...` | **零**：只 `git add`，不改内容 |
| SSOT 文档 | `post-refactor-architecture.md` §1.3 + 变更记录 | **低**：仅追加 v0.1.4 行 |
| 用户工作流 | 决定是 `git submodule update` 直接 commit 还是另开 TaskRunner 仓库 PR | **决策点**：见 design §D1 |

## 开放问题（待 design.md 解决）

1. **TaskRunner 同步方式**：
   - 选项 A：直接 commit 到 `external/TaskRunner/`（submodule 内修改，push 到其上游）—— 简单但需要 push 权限
   - 选项 B：仅在主仓库留指针（"TaskRunner PR 在 upstream #XX"），不直接 commit —— 推迟实际修改
   - 建议：选项 A（如果用户有 push 权限），否则选项 B
2. **Archive 内容是否要修改**：filesystem 上的 archive 文件 mtime 是 14:28-14:31（H-1 实施时），但 `git rm` 后没有 commit。`git add` 时是否会触发 openspec 对历史归档的"再处理"？需要测试。
