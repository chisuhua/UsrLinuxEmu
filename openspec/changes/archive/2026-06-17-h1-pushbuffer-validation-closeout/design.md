# Design: h1-pushbuffer-validation-closeout

> **依赖**: `proposal.md` 已完成
> **作用**: 解释 HOW 收尾 H-1，而非 WHY（WHY 见 proposal）

## Context

**H-1 完成状态**（2026-06-17 16:42 收尾）：

| 任务组 | 状态 |
|--------|------|
| 1.1 协议扩展（`gpu_ioctl.h` 加 `va_space_handle`）| ✅ `0272970` |
| 1.2 跨仓库 TaskRunner 同步 | ❌ **本 change 处理** |
| 2. 驱动实现（`handlePushbufferSubmitBatch` 2 段校验）| ✅ `bf8192f` |
| 3. 测试（4 cases）| ✅ `09ae1b0` |
| 4. 文档同步（SSOT v0.1.3 + ioctl-commands.md）| ✅ `09ae1b0` + `61b67db` |
| 5. 验证（build / ctest 34/34 / docs-audit 36/36）| ✅ |
| 6.1 提交（5 commits + 1 update）| ✅ |
| 6.2 archive 操作 | ⚠️ **部分**——本 change 处理 git 跟踪恢复 |
| 6.3 SSOT 变更记录 commit hash | ✅ `61b67db` |

**TaskRunner 当前状态**（`external/TaskRunner/include/gpu_driver_client.h:227`）：

```cpp
struct gpu_pushbuffer_args args = {};  // 零初始化 → va_space_handle = 0 → 走 sentinel 跳过校验
if (ioctl(fd_, GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH, &args) < 0) { ... }
```

**Archive 当前状态**：
- filesystem: `openspec/changes/archive/2026-06-17-fix-gpu-pushbuffer-va-space-validation/` 含 5 文件（README/design/proposal/tasks/specs/gpu-pushbuffer-validation/spec.md）+ .openspec.yaml
- git: 0 tracked files in that path（commit `744ef46` 删了原 change dir 但 archive dir 未 `git add`）

## Goals / Non-Goals

**Goals**:
- H-1 的 `va_space_handle` 校验在 TaskRunner 调用方"可被触发"（即不强制每个 call site 都传，但提供 API 透传路径）
- 原始 H-1 change 在 git log 中可追溯
- SSOT v0.1.4 记录 closeout 完成

**Non-Goals**:
- 不强制 TaskRunner 的每个 pushbuffer 调用都创建/关联 VA Space（那是 Phase 3+ 任务，不在本次 scope）
- 不重写 TaskRunner 的 GpuDriverClient API surface
- 不改 H-1 主 capability `gpu-pushbuffer-validation` 的 spec（行为层未变）
- 不修复 TaskRunner 子模块内部的 LSP include path 错误（独立问题，单独 issue）

## Decisions

### D1: TaskRunner 同步方式 = 优先直接 commit，备选外部 PR

**决策**：默认 `cd external/TaskRunner && <edit> && git commit && cd .. && git add external/TaskRunner && git commit`；如果用户没有 TaskRunner push 权限或想保持上游纯净，则改用"留指针"模式（只在主仓库 commit message 里写 TaskRunner PR #XX，submodule 指针不变）。

**理由**：
- 子模块内直接 commit 是 git submodule 标准操作，与 H-1 的 `external/TaskRunner` submodule pointer 模式一致
- 备选"留指针"模式保留独立性，但延长了 ABI 漂移窗口

**判别条件**：
```bash
# 在执行前先检查
cd external/TaskRunner && git remote -v
# 如果上游是 chisuhua/TaskRunner 且有 push 权限 → 走 A
# 否则 → 走 B
```

### D2: `va_space_handle` 透传 API = 简单加 setter，不改 submit 签名

**决策**：在 `GpuDriverClient` 加一个 `setCurrentVASpace(uint64_t handle)` 方法（或在构造函数接受），提交 pushbuffer 时自动填入 `args.va_space_handle`。`ioctl()` 签名不变。

**理由**：
- 保持 ABI 兼容（已有调用方不需改）
- 调用方可选择"有 VA Space 时透传 / 没有时留 0"——渐进式
- 与 H-1 的 `va_space_handle==0` 向后兼容设计（D1 of H-1）一致

**备选**：
- A. 改 `submitPushbuffer()` 签名加 `va_space_handle` 参数 —— 强制每个 caller 决定，更严格但破坏现有调用方
- B. 加 setter（决策 D2）—— 灵活

### D3: Archive 恢复策略 = `git add` filesystem 文件，不重新生成

**决策**：`git add openspec/changes/archive/2026-06-17-fix-gpu-pushbuffer-va-space-validation/` 把现有文件纳入跟踪。

**理由**：
- 文件内容是 H-1 完成时的最终状态（design D1-D6、spec requirements 完整）
- 重新 `openspec archive` 会试图"再次归档"，可能引发工具混乱（同一个 change 不应被 archive 两次）
- 简单 `git add` 不改任何内容，最小侵入

**备选**：
- A. `rm -rf openspec/changes/archive/...` + 重新 `openspec archive` —— 风险：openspec 可能报"已归档"错误
- B. `git add`（决策 D3）—— 安全

### D4: SSOT v0.1.4 增量 = 仅追加 v0.1.4 行，不动 v0.1.3

**决策**：在变更记录表新增一行 "v0.1.4 收尾"，引用本 change；不改 v0.1.3 的内容（已稳定）。

**理由**：
- 变更记录是 append-only 日志
- v0.1.3 已 commit `61b67db`，回改会破坏 git blame

### D5: 不创建独立 commit 编号段，沿用本 change 的 1 个 commit

**决策**：closeout 全部 3 件事（TaskRunner sync / archive git / SSOT v0.1.4）合并到 1 个 commit `fix(h1-closeout): complete H-1 cross-repo sync + archive git tracking`。

**理由**：
- 3 件事是同一目的的不同面（"完成 H-1 闭环"）
- 拆 3 commit 反而模糊——读者要跨 commit 拼接才能理解全貌
- 1 commit + 详细 commit message 比 3 commit + 3 个关联的 cross-ref 链接更易追溯

**备选**：
- 拆 3 commit 提升 git bisect 精度 —— 收益小（本 change 风险已低）

## Risks / Trade-offs

| Risk | Impact | Mitigation |
|------|--------|------------|
| **R1**: TaskRunner push 权限缺失 → commit 到 detached HEAD | 中 | D1 备选方案：仅留指针模式 |
| **R2**: TaskRunner 透传 API 与上游 master 分支冲突 | 低 | 仅加 setter + 不改既有方法；冲突面极小 |
| **R3**: `git add` archive 时 openspec CLI 不识别已归档 change | 低 | 不跑 `openspec archive` 再次归档；纯 `git add` 是 git 操作，与 openspec 状态机无关 |
| **R4**: v0.1.4 行与 v0.1.3 行格式不一致 → docs-audit 警告 | 极低 | 复制 v0.1.3 的格式 |
| **R5**: 子模块 commit 破坏主仓库 CI | 低 | TaskRunner 自身 CI 在它自己的仓库；主仓库 CI 只看 gitlink 指向 |
| **R6**: 用户原意是"清理"archive 而非"恢复" | 中 | proposal.md §What Changes 写明 "不修改文件内容"；用户可在 apply 时确认 |

## Migration Plan

1. **Phase 1: TaskRunner 同步**（如果 D1 选 A）
   - `cd external/TaskRunner`
   - 编辑 `include/gpu_driver_client.h`：加 `setCurrentVASpace(uint64_t)` + 提交时透传
   - 编辑 `plans/sync-plan.md`：在 S3.1 加完成标记
   - `git add -A && git commit -m "feat(client): plumb va_space_handle through GpuDriverClient"`
   - `cd ../.. && git add external/TaskRunner && git commit`（主仓库更新子模块指针）

2. **Phase 2: Archive git 跟踪恢复**
   - `git add openspec/changes/archive/2026-06-17-fix-gpu-pushbuffer-va-space-validation/`
   - 与 Phase 3 合并到同一 commit

3. **Phase 3: SSOT v0.1.4**
   - 编辑 `docs/02_architecture/post-refactor-architecture.md`：
     - §1.3 v0.1.3 段落加注"TaskRunner 已同步，change h1-pushbuffer-validation-closeout"
     - 变更记录表新增 v0.1.4 行
   - 与 Phase 1+2 合并到同一 commit

4. **Phase 4: 验证 + 归档**
   - `make -j4` 100%
   - `ctest` 34/34
   - `bash tools/docs-audit.sh --strict` 36/36
   - `openspec archive h1-pushbuffer-validation-closeout` 归档
   - `git add openspec/changes/archive/2026-06-17-h1-pushbuffer-validation-closeout/`（新增的归档目录）
   - `git commit -m "chore(openspec): archive h1-pushbuffer-validation-closeout"`

5. **Rollback**：
   - Phase 1 失败 → `git submodule update` 恢复子模块到原 commit；`git revert` 主仓库 commit
   - Phase 2 失败 → `git rm --cached openspec/changes/archive/...` 取消跟踪
   - Phase 3 失败 → `git checkout HEAD -- docs/02_architecture/post-refactor-architecture.md`
   - 各 Phase 独立可逆

## Open Questions

无（proposal §开放问题已通过 D1-D5 解决）。

D1 的"直接 commit vs 留指针"由用户在 apply 时根据 push 权限判断。
