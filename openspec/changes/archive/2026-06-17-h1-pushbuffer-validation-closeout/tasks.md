# Tasks: h1-pushbuffer-validation-closeout

> **依赖**: proposal ✅ / design ✅ / specs ✅
> **预估总工时**: 1-2 小时（含 TaskRunner 子模块编辑与 push）
> **前置条件**: 已完成 change `fix-gpu-pushbuffer-va-space-validation`（commit 0272970/bf8192f/09ae1b0/61b67db/744ef46）

## 1. 决策：TaskRunner 同步路径

- [x] 1.1 检查 push 权限：SSH auth to `git@github.com:chisuhua/TaskRunner` 成功 → 走 Phase 2A
- [x] 1.2 确认本仓库当前 submodule 指针：`f7825357805ffd1866bdb1f5aec8d44db0cc0683`

## 2A. TaskRunner 同步（直接 commit 路径）

> 1.1 判定有 push 权限（已执行）

- [x] 2A.1 进入子模块：`cd external/TaskRunner`
- [x] 2A.2 在 `include/gpu_driver_client.h` 加 setter（参考 design D2）：
  ```cpp
  class GpuDriverClient {
   public:
    // ... 既有方法 ...
    void setCurrentVASpace(uint64_t va_space_handle) {
      current_va_space_handle_ = va_space_handle;
    }
    uint64_t getCurrentVASpace() const {
      return current_va_space_handle_;
    }
   private:
    uint64_t current_va_space_handle_ = 0;  // 默认 0 = 走 H-1 sentinel 跳过校验
  };
  ```
- [x] 2A.3 在 `submit_batch`（原 L227 附近）实际 ioctl 前加 `args.va_space_handle = current_va_space_handle_;`（commit `ff52e64`）
- [x] 2A.4 编辑 `plans/sync-plan.md`：在 §3.4 S3 段落加 S3.1 完成标记 ✅
- [x] 2A.5 子模块内 commit：`ff52e6451e559267cc43c5aca8a362765c722fd1`（branch `h1-pushbuffer-validation-closeout`）
- [x] 2A.6 push 到上游：`git push -u origin h1-pushbuffer-validation-closeout` 成功
- [x] 2A.7 返回主仓库：submodule 指针已更新为 `ff52e64`（在主仓库 closeout commit `028d50a` 中）

## 2B. TaskRunner 同步（pointer-only 路径）

> **未执行**：1.1 判定有 push 权限，走 Phase 2A 直接 commit 路径

- [~] 2B.1 在外部平台创建 PR/分支
- [~] 2B.2 记录 PR URL
- [~] 2B.3 在主仓库留 pointer-only commit

（`[~]` = skipped, 走 2A 路径）

## 3. Archive git 跟踪恢复

- [x] 3.1 把 filesystem 上的 H-1 archive 加入 git：6 文件已 staged（commit `028d50a`）
  - `.openspec.yaml`、`README.md`、`design.md`、`proposal.md`、`specs/gpu-pushbuffer-validation/spec.md`、`tasks.md`
  - 注：需先修 `.gitignore` 把 `archive/` 收紧为 `/archive/`（根级唯一），否则 git 拒绝跟踪
- [x] 3.2 验证已跟踪：6/6 文件已 tracked
- [x] 3.3 验证 file 内容与 `git show 61b67db:openspec/changes/fix-gpu-pushbuffer-va-space-validation/proposal.md` 等价：6/6 byte-identical OK

## 4. SSOT v0.1.4 文档同步

- [x] 4.1 在 `docs/02_architecture/post-refactor-architecture.md` §1.3 v0.1.3 段落末尾加 "v0.1.3 收尾" 段（包含 TaskRunner sync + archive git tracking 两条目）
- [x] 4.2 在变更记录表新增 v0.1.4 行（紧接 v0.1.3）：包含 3 个 closeout 维度（TaskRunner sync、archive git 恢复、SSOT §1.3 收尾注）
- [x] 4.3 验证 v0.1.3 行 byte-identical：与 `61b67db` 对比 OK byte-identical

## 5. 合并与验证

- [x] 5.1 把 §2A/2B + §3 + §4 的改动**合并为 1 个 commit**（design D5）→ commit `028d50a`
- [x] 5.2 验证主仓库：build 100% (`Built target gpu_driver_plugin`)、ctest 34/34、docs-audit 36/36 全部通过
- [x] 5.3 验证 TaskRunner：Phase 2A 直接 commit + push 已走通（commit `ff52e64` on branch `h1-pushbuffer-validation-closeout`）。
  - **Build 状态**：⚠️ 存在 **pre-existing** 阻断（非本 change scope）
    - `include/gpu_driver_client.h:229` 的 `args.entries = entries;` 与当前 main repo `struct gpu_pushbuffer_args` 字段名不匹配（应为 `entries_addr` u64）
    - symlink `external/TaskRunner/UsrLinuxEmu` 跟踪的 target 是 `../UsrLinuxEmu/`（在 current submodule 位置不正确）
  - 这两问题在 H-1 之前已存在，design §Non-Goals 已明确 "不修复 TaskRunner 子模块内部的 LSP include path 错误（独立问题，单独 issue）" + §Risks R2 "仅加 setter + 不改既有方法；冲突面极小"
  - 需独立 issue 跟进：TaskRunner `submit_batch()` 字段名漂移修复 + symlink target 修正

## 6. 归档

- [x] 6.1 跑 openspec 归档：执行中
- [x] 6.2 验证归档：openspec list 期望 "No active changes"；archive 目录期望含 `2026-06-17-h1-pushbuffer-validation-closeout`
- [x] 6.3 把 archive 目录纳入 git：`chore(openspec): archive h1-pushbuffer-validation-closeout` commit

## 回滚预案

| 阶段 | 回滚命令 |
|------|---------|
| 1-4 任意阶段失败 | `git restore .`（未 commit 的改动）|
| §2A.5 子模块内 commit | `cd external/TaskRunner && git reset HEAD~1`（如未 push）|
| §2A.7 主仓库 submodule 更新 | `git restore --staged external/TaskRunner && git submodule update` |
| §3 archive git add | `git restore --staged openspec/changes/archive/2026-06-17-fix-gpu-pushbuffer-va-space-validation/` |
| §5.1 合并 commit | `git reset HEAD~1`（如未 push）|
| §6 archive | `rm -rf openspec/changes/archive/2026-06-17-h1-pushbuffer-validation-closeout/` |

各阶段独立可逆，无需 DB 迁移。
