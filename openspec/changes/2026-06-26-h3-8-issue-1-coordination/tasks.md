# Tasks: H-3.8 ADR-034 Issue #1 修复协调

> **依赖**: proposal ✅, design ✅
> **预估总工时**: 1-2 周（TaskRunner 端协调）+ UsrLinuxEmu owner 端 3-5 天（实际代码修改）
> **前置条件**: H-3 phase2-management ✅, H-5 scope clarification ✅, H-5.1 scope cleanup ✅, H-3.5 followup ✅, H-3.6 issue-3-coordination ✅, H-3.7 issue-2-coordination ✅
> **后续约束**: H-3.8 shippable 后，H-7 deferred 3 issues 全部解决

## Phase A：TaskRunner 端协调（1-2 周，Day 1-10）

### A.1 创建 openspec change 目录结构

- [x] A.1.1 创建 `openspec/changes/2026-06-26-h3-8-issue-1-coordination/` 目录
- [x] A.1.2 创建 `.openspec.yaml`（schema: spec-driven, status: PROPOSED）
- [x] A.1.3 创建 `README.md`（一句话摘要）
- [x] A.1.4 创建 `proposal.md`（Why / What / Impact / Non-Goals）
- [x] A.1.5 创建 `design.md`（Context / Goals / Decisions / Strategy / Risks）
- [x] A.1.6 创建 `tasks.md`（本文件）
- [x] A.1.7 创建 `specs/gpu-h7-issue-1-abi-widening/spec.md`（MODIFIED Requirements）

### A.2 创建 GitHub issue 草稿（待贴到 UsrLinuxEmu 仓）

- [ ] A.2.1 编写 issue title：`H-7 Issue #1: stream_id u32 ABI 类型不匹配 — 提议拓宽到 u64`
- [ ] A.2.2 编写 issue body：问题描述 + 触发位置 + 推荐方案 + 调研支撑
- [ ] A.2.3 添加 label：`h7-deferred`、`phase3-prerequisite`、`abi-change`、`breaking-change-with-deprecation`
- [ ] A.2.4 交叉引用：ADR-034 §Issue #1 + tadr-105 + 本 change + tadr-007 R2 mapping
- [ ] A.2.5 提议方案：u32 → u64 + deprecated alias `stream_id_compat`
- [ ] A.2.6 包含调研支撑（AMD/NVIDIA u64 handle 模式 + 6 月过渡期）

### A.3 创建 u64 边界值测试设计文档

- [ ] A.3.1 创建 `docs/test-fixture/research/u64-boundary-test-design-2026-06-26.md`
- [ ] A.3.2 编写 `next_queue_handle_` 接近 UINT32_MAX 测试设计
- [ ] A.3.3 编写 UINT32_MAX → UINT32_MAX+1 提交测试设计
- [ ] A.3.4 编写 R2 mapping 移除后行为一致性测试设计
- [ ] A.3.5 引用 AMD/NVIDIA 调研结论（handle 范围参考）

### A.4 创建 ABI 向后兼容测试设计文档

- [ ] A.4.1 创建 `docs/test-fixture/research/abi-backward-compat-test-design-2026-06-26.md`
- [ ] A.4.2 编写旧调用方传 `stream_id_compat` 测试设计
- [ ] A.4.3 编写新调用方传 `stream_id`（u64）测试设计
- [ ] A.4.4 编写双驱动版本共存期测试设计（旧 driver + 新 driver）
- [ ] A.4.5 编写 deprecation warning 日志测试设计（6 月过渡期）

### A.5 创建 AMD/NVIDIA handle 模式调研综合

- [ ] A.5.1 创建 `docs/shared/research/gpu-queue-id-patterns-2026-06-26.md`
- [ ] A.5.2 AMD ROCm KFD `HSA_QUEUEID` u64 pointer 模式
- [ ] A.5.3 AMD ROCm HSA Runtime `hsa_queue_t*` 模式
- [ ] A.5.4 NVIDIA CUDA `CUstream` opaque pointer 模式
- [ ] A.5.5 NVIDIA UVM `uvm_va_block_region_t` packed handle 模式
- [ ] A.5.6 TaskRunner 推荐方案论证

### A.6 更新跨仓 PR 模板（添加 H-3.8 行）

- [ ] A.6.1 修改 `docs/07-integration/cross-repo-h7-template.md`
- [ ] A.6.2 添加 H-3.8 历史 PR 范例行
- [ ] A.6.3 添加 ABI 拓宽专章（commit message + PR description 模板）
- [ ] A.6.4 添加 deprecation period 提醒

### A.7 更新 tadr-105 §Trigger Conditions 段 + §H-3.8 段

- [ ] A.7.1 阅读 `docs/test-fixture/adr/tadr-105-h7-deferred.md` 当前状态
- [ ] A.7.2 添加 §H-3.8 段（Issue #1 主动修复协调 + 提议方案 + 实施分工）
- [ ] A.7.3 更新 §Consequences Current 段：H-3.8 进行中
- [ ] A.7.4 更新 §Last Updated 时间戳

### A.8 验证 Phase A

- [ ] A.8.1 运行 `tools/docs-audit.sh` 验证目录结构
- [ ] A.8.2 验证 6 文件 openspec change 结构完整
- [ ] A.8.3 验证所有新文件 frontmatter / 路径引用正确
- [ ] A.8.4 验证 0 docs-audit error

---

## Phase B：跨仓 PR 协调（待 UsrLinuxEmu owner 启动，Day 4-7）

### B.1 提交 GitHub issue

- [ ] B.1.1 TaskRunner owner 在 UsrLinuxEmu 仓开 GitHub issue（用 A.2 草稿）
- [ ] B.1.2 通知 UsrLinuxEmu owner（Slack / Email / Project lead）
- [ ] B.1.3 等待 owner 评估（accept / request changes / reject）
- [ ] B.1.4 如 accept：进入 B.2
- [ ] B.1.5 如 reject：记录决策到 tadr-105 §Issue #1 Rejected Reason 段

### B.2 UsrLinuxEmu owner 实施 PR 1（待 owner 启动）

- [ ] B.2.1 Owner 在 UsrLinuxEmu 仓创建分支 `feat/h7-issue-1-abi-widening`
- [ ] B.2.2 修改 `gpu_ioctl.h:43`：`stream_id` u32 → u64 + 新增 `stream_id_compat` + `flags_extended`
- [ ] B.2.3 修改 `gpgpu_device.cpp:262`：移除 `static_cast` + 增加 backward compat fallback 逻辑
- [ ] B.2.4 更新错误日志使用 `effective_stream_id`（不再截断）
- [ ] B.2.5 验证编译：`cd UsrLinuxEmu/build && make -j4`
- [ ] B.2.6 跑全部测试：`./build/bin/test_gpu_ioctl_standalone` + `./build/bin/test_gpu_plugin`
- [ ] B.2.7 提交 PR（含 issue 引用 + 测试结果 + 跨仓影响说明 + deprecation period 说明）

### B.3 跨仓 PR 协议（按 ADR-035 §Rule 5.1）

- [ ] B.3.1 步骤 1：UsrLinuxEmu 仓 PR merged
- [ ] B.3.2 步骤 2：TaskRunner 仓 submodule 指针 bump
- [ ] B.3.3 步骤 3：TaskRunner 仓 tadr-105 状态更新
- [ ] B.3.4 步骤 4：UsrLinuxEmu 仓 mirror + ADR-034 状态更新

### B.4 下游 caller 通知

- [ ] B.4.1 识别所有使用 `gpu_pushbuffer_args.stream_id` 的 caller
- [ ] B.4.2 通知 caller 升级到 u64 ABI（CHANGELOG.md + GitHub discussion）
- [ ] B.4.3 6 月过渡期开始时记录 start date

---

## Phase C：状态同步（PR 1 merged 后，Day 8-10）

### C.1 TaskRunner 仓 submodule bump

- [ ] C.1.1 `cd external/TaskRunner && git fetch origin main`
- [ ] C.1.2 `git checkout main && git pull`
- [ ] C.1.3 `cd /workspace/project/UsrLinuxEmu && git add external/TaskRunner`
- [ ] C.1.4 `git commit -m "chore(submodule): bump TaskRunner to <hash> (H-3.8)"`
- [ ] C.1.5 `git push origin main`

### C.2 TaskRunner 端 tadr-105 状态更新

- [ ] C.2.1 修改 `docs/test-fixture/adr/tadr-105-h7-deferred.md` §Issue #1 状态
- [ ] C.2.2 添加 §H-3.8 Completion 段（含 commit 链 + 跨仓 PR 链接 + deprecation period start date）
- [ ] C.2.3 修改 §Consequences Current 段：H-7 deferred 3 issues 全部解决
- [ ] C.2.4 提交 commit：`docs(adr): tadr-105 §Issue #1 → Accepted (H-3.8)`
- [ ] C.2.5 推送 + 跨仓 submodule bump

### C.3 UsrLinuxEmu 仓 archive + 状态更新

- [ ] C.3.1 `cd /workspace/project/UsrLinuxEmu`
- [ ] C.3.2 `openspec archive 2026-06-26-h3-8-issue-1-coordination -y`
- [ ] C.3.3 修改 `docs/00_adr/adr-034-h7-deferred-registry.md` §Issue #1 状态：⏸️ → ✅ Accepted
- [ ] C.3.4 修改 `docs/00_adr/README.md` TaskRunner TADR mirror 段（如有变化）
- [ ] C.3.5 提交 commit + push

---

## Phase D：PR 2（6 月后，2026-12-26+，废弃 alias 字段清理）

### D.1 UsrLinuxEmu owner 评估 PR 2 范围

- [ ] D.1.1 评估 6 月过渡期内下游 caller 升级情况
- [ ] D.1.2 评估 `stream_id_compat` 字段使用率（应接近 0）
- [ ] D.1.3 决策：是否启动 PR 2 / 延期 / 永久保留 alias

### D.2 PR 2 实施（待 D.1 决策）

- [ ] D.2.1 移除 `stream_id_compat` 字段
- [ ] D.2.2 移除 `gpgpu_device.cpp` 中 backward compat fallback 逻辑
- [ ] D.2.3 更新所有 caller（kernel module + userspace helper）
- [ ] D.2.4 跨仓 PR（按 B.3 协议）

### D.3 PR 2 状态同步

- [ ] D.3.1 tadr-105 §H-3.8 段扩展（如 PR 2 实施）
- [ ] D.3.2 ADR-034 §Issue #1 完整解决方案更新
- [ ] D.3.3 archive 本 change（如未 archive）

---

## Verification Checklist（最终验收）

- [ ] A.8.4 验证 docs-audit 0 error
- [ ] B.2.5 验证 UsrLinuxEmu 仓编译通过
- [ ] B.2.6 验证 UsrLinuxEmu 仓全部测试通过
- [ ] B.4.1 验证所有 caller 已识别 + 通知
- [ ] C.1.5 验证 TaskRunner submodule bump 推送
- [ ] C.2.5 验证 tadr-105 状态更新推送
- [ ] C.3.3 验证 ADR-034 状态更新
- [ ] 全部 commit 历史清晰可追溯

## Dependencies

```
H-3 phase2-management (✅ 2026-06-22)
    ↓
H-5 taskrunner-scope-clarification (✅ 2026-06-24)
    ↓
H-5.1 taskrunner-scope-cleanup (✅ 2026-06-25)
    ↓
H-3.5 followup-test-fixture-cleanup (✅ 2026-06-25)
    ↓
H-3.6 issue-3-coordination (✅ 2026-06-25, Issue #3 修复)
    ↓
H-3.7 issue-2-coordination (✅ 2026-06-25, Issue #2 修复)
    ↓
H-3.8 issue-1-coordination (📋 PROPOSED, 2026-06-26 ← 本 change)
    ↓
[6 月过渡期: 2026-06-26 ~ 2026-12-26]
    ↓
PR 2: 废弃 alias 字段清理 (📋 待 6 月后启动)
    ↓
H-7 deferred 全部解决 (✅ 2026-12-26)
```

## Status Tracking

- **Phase A**: 📋 进行中 (TaskRunner 端协调文档准备)
- **Phase B**: ⏸️ 待 UsrLinuxEmu owner 启动
- **Phase C**: ⏸️ 待 PR 1 merged
- **Phase D**: ⏸️ 6 月后（2026-12-26+）

## Reference Files

- **TaskRunner tadr-105**: [`external/TaskRunner/docs/test-fixture/adr/tadr-105-h7-deferred.md`](../../../../external/TaskRunner/docs/test-fixture/adr/tadr-105-h7-deferred.md)
- **TaskRunner tadr-007 R2 mapping**: [`external/TaskRunner/docs/test-fixture/adr/tadr-007-r2-mapping.md`](../../../../external/TaskRunner/docs/test-fixture/adr/tadr-007-r2-mapping.md)
- **TaskRunner tadr-006 H-3.5**: [`external/TaskRunner/docs/test-fixture/adr/tadr-006-h3-phase2-lifecycle.md`](../../../../external/TaskRunner/docs/test-fixture/adr/tadr-006-h3-phase2-lifecycle.md)
- **UsrLinuxEmu ADR-034**: [`docs/00_adr/adr-034-h7-deferred-registry.md`](../../00_adr/adr-034-h7-deferred-registry.md)
- **H-3.6 openspec change**: [`archive/2026-06-26-h3-6-issue-3-coordination/`](../../changes/archive/2026-06-26-h3-6-issue-3-coordination/) (template)
- **H-3.7 openspec change**: [`archive/2026-06-26-h3-7-issue-2-coordination/`](../../changes/archive/2026-06-26-h3-7-issue-2-coordination/) (template)