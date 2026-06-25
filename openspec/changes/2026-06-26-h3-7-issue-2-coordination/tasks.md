# H-3.7 任务清单：ADR-034 Issue #2 (ioctl path 绕过 GpuQueueEmu 抽象层)

## Phase A: TaskRunner 端协调（Day 1-3，2-3 人/天）

### A1. openspec change 创建（DONE 2026-06-26）
- [x] 创建目录 `openspec/changes/2026-06-26-h3-7-issue-2-coordination/`
- [x] 编写 `.openspec.yaml`
- [x] 编写 `README.md`
- [x] 编写 `proposal.md`
- [x] 编写 `design.md`
- [x] 编写 `tasks.md`

### A2. tadr-105 更新（DONE 2026-06-26）
- [x] 添加 §H-3.7 段（Issue #2 协调启动）
- [x] 更新 Issue #2 状态表
- [x] 更新 Deferred/Current/Mitigation 段

### A3. 调研文档
- [ ] `docs/test-fixture/research/gpu-queue-emu-api-2026-06-26.md` — GpuQueueEmu API 表面文档
- [ ] `docs/test-fixture/research/ioctl-mmap-equivalence-2026-06-26.md` — ioctl vs mmap 等价性测试设计

### A4. GitHub issue 草稿
- [ ] 在 UsrLinuxEmu 仓开 issue：标题 + 标签 + 引用 + 提议方案

## Phase B: UsrLinuxEmu owner 评估（Day 4-7，3-5 人/天）

### B1. owner 评估
- [ ] 评估提议方案（Phase A 提交）
- [ ] 确认 GpuQueueEmu 接口状态（`getQueue()` + `submit()`）
- [ ] 确认是否需要扩展 `GpuQueueEmu` 方法

### B2. 实际代码修改（PR 1）
- [ ] 重构 `gpgpu_device.cpp:284-300` — 委托 GpuQueueEmu 抽象层
- [ ] 编译验证（gpgpu_device.cpp + gpu_queue_emu.cpp）
- [ ] 运行 test_gpu_pushbuffer_validation（4 cases）— 行为等价性
- [ ] 运行 test_gpu_phase2（12 cases）— 无回归
- [ ] 运行 test_gpu_plugin — 无回归

### B3. 错误码语义化（PR 2，可选）
- [ ] 区分 `-EINVAL` / `-ENOENT` / `-EBUSY`
- [ ] 更新 `gpu_ioctl.h` 注释
- [ ] 更新 `ioctl-commands.md` 文档

## Phase C: 跨仓同步（Day 8-10，1-2 人/天）

### C1. UsrLinuxEmu 端 commit
- [ ] commit message: `refactor(gpu): route pushbuffer through GpuQueueEmu abstraction`
- [ ] push to main
- [ ] 更新 ADR-034 §Issue #2 状态 → Accepted

### C2. TaskRunner 端同步
- [ ] bump submodule 指针（到 UsrLinuxEmu 最新 commit）
- [ ] tadr-105 §Issue #2 状态 → Accepted
- [ ] 更新 `docs/00_adr/README.md` mirror

### C3. archive
- [ ] archive 本 openspec change
- [ ] 更新 `archive/2026-06-26-h3-7-issue-2-coordination/` 状态 → ACCEPTED

## Phase D: H-3.8 启动（Day 11+，待定）

### D1. H-3.8 准备
- [ ] Issue #1 (u32→u64) 协调启动
- [ ] 评估 ABI 变更影响范围
- [ ] 准备 deprecated alias 方案

## 验证清单

- [ ] `gpgpu_device.cpp` 编译通过
- [ ] test_gpu_pushbuffer_validation 4 cases 通过
- [ ] test_gpu_phase2 12 cases 通过
- [ ] test_gpu_plugin 通过
- [ ] CLI `cuda_queue` 命令正常
- [ ] 跨仓同步完成（submodule + tadr-105 + mirror）
- [ ] 文档归档（openspec change + ADR-034 + tadr-105）

## 依赖关系

```
A1 (openspec) → A2 (tadr-105) → A3 (调研) → A4 (GitHub issue)
     ↓
B1 (owner 评估) → B2 (PR 1) → B3 (PR 2, optional)
     ↓
C1 (commit) → C2 (sync) → C3 (archive)
     ↓
D1 (H-3.8 启动)
```

## 风险追踪

| 风险 | 状态 | 缓解 |
|------|------|------|
| R1: GpuQueueEmu 接口缺失 | 🟡 监控 | 重构前检查接口 |
| R2: 性能回退 | 🟡 监控 | getQueue O(1)，开销可忽略 |
| R3: 行为不一致 | 🟡 监控 | 重构前记录行为，重构后对比 |
| R4: 与 H-3.6 冲突 | 🟢 已缓解 | H-3.6 已合并，基于最新代码 |
| R5: 跨仓同步延迟 | 🟡 监控 | TaskRunner 端先完成文档 |
| R6: 过度抽象 | 🟢 已缓解 | 最小改动原则 |

## 时间估算

| 阶段 | 工作量 | 阻塞条件 |
|------|-------|---------|
| Phase A | 2-3 人/天 | 无（DONE） |
| Phase B | 3-5 人/天 | UsrLinuxEmu owner 响应 |
| Phase C | 1-2 人/天 | Phase B 完成 |
| Phase D | TBD | Phase C 完成 |
| **总计** | **6-10 人/天** | 取决于 owner 响应速度 |
