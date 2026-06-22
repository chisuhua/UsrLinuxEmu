# H-3 Phase2 OpenSpec 计划审查反馈

**审查对象**: `external/TaskRunner/plans/2026-06-19-h3-phase2-openspec-skeleton/`
**审查日期**: 2026-06-22
**审查方**: UsrLinuxEmu Architecture Team
**目标读者**: TaskRunner owner
**结论**: ⚠️ **NEEDS_REVISION** — 修复 4 项必改后可 APPROVED

---

## 一、TL;DR

H-3 plan（Phase 2 VA Space + Queue lifecycle）骨架结构完整，4-artifact 齐全，D1-D5 决策清晰，上游 ioctl 引用准确，**4 个必改项**中 1 项为**事实错误**（前置依赖 H-2.5 实际已 archived），3 项为**实现/规约偏差**（守卫日志缺失 + spec 内部矛盾 + 路径歧义）。骨架扎实，修复量小。

**预期时间**：4 处必改 + 7 处建议改 ≈ 1-2 小时工作量。

---

## 二、上下文（2026-06-22 现状）

| 维度 | 现状 |
|------|------|
| H-2.5 (`h2-5-architecture-foundation`) | ✅ **已 archived** 于 `openspec/changes/archive/2026-06-19-h2-5-architecture-foundation/`（今天 12:00 迁移完成）|
| H-2 (`h2-phase2-openspec-skeleton`) | ⚠️ DEPRECATED（2026-06-19 同日废弃），保留为 Path D 决策证据 |
| H-3 (`h3-phase2-openspec-skeleton`) | ⚠️ DRAFT（今天 01:25-01:28 创建），待 review + 激活 |
| `sync-plan.md` S5 同步点 | ✅ **已完成**（commit `0f0d5af` 12:07，UsrLinuxEmu commit `c64301c`）|
| H-3 激活路径 | `mv plans/2026-06-19-h3... → openspec/changes/h3-phase2-management`（需 review 通过后） |

**关键含义**：H-3 的**前置依赖已全部满足**，可以激活，但需先修复本文列出的 4 项必改。

---

## 三、必改项（Blocking）— 4 项

### B1 [CRITICAL] — 前置依赖状态事实错误

**问题**：H-3 plan 全文 5 处把 H-2.5 标为"必须先完成"，但 H-2.5 **已 archived**。

| 文件 | 行号 | 当前措辞（节选）|
|------|------|------------------|
| `.openspec.yaml` | 4 | `prerequisite: H-2.5` |
| `README.md` | 6 | "**前置依赖**: **H-2.5** `h2-5-architecture-foundation` 必须先完成" |
| `proposal.md` | 6 | "⚠️ **H-2.5** `h2-5-architecture-foundation`（**必须先完成**）" |
| `design.md` | 9-19 | Context table 中 H-2.5 标 ⏳ 待完成 |
| `tasks.md` | 5, 7-15 | 整个 §1 是 H-2.5 完成性检查 |

**证据**：
- `ls openspec/changes/archive/` 显示 `2026-06-19-h2-5-architecture-foundation/` 存在
- TaskRunner 仓 `git log` 显示 commit `82b13f7` (12:00) "docs: remove H-2.5 openspec skeleton from plans/ (moved to UsrLinuxEmu)"
- `sync-plan.md` commit `0f0d5af` (12:07) S5 标 ✅

**建议改法**：

```yaml
# .openspec.yaml line 4
- 改前: prerequisite: H-2.5
+ 改后: prerequisite: h2-5-architecture-foundation (archived 2026-06-19 in UsrLinuxEmu openspec/changes/archive/)
```

```markdown
# README.md / proposal.md / design.md - 统一措辞
- 改前: 必须先完成 / ⏳ 待完成
+ 改后: ✅ H-2.5 已 archived (2026-06-19), 已通过 S5 同步验证
```

```markdown
# tasks.md §1 - 改为 verification 步骤
- 改前: [ ] 1.1 验证 H-2.5 ... (6 项 prereq 检查)
+ 改后: [✅] 1.1 验证 H-2.5 归档状态 — `ls openspec/changes/archive/2026-06-19-h2-5-architecture-foundation/` (2026-06-19 archived)
- 改前: 任何 1.1-1.5 未通过 → 立即停止 H-3
+ 改后: 任何 1.1-1.5 失败 → 报告并升级到 H-2.5 维护者（已 archived，无需阻塞）
```

**验证步骤**：
```bash
ls /workspace/project/UsrLinuxEmu/openspec/changes/archive/2026-06-19-h2-5-architecture-foundation/
# 应输出: proposal.md  design.md  tasks.md  specs/  README.md
```

---

### B2 [MODERATE] — spec.md 守卫期望 vs design.md 实现不一致

**问题**：spec.md R3/R4 描述的守卫行为与 design.md 中的实际代码实现有差距。

| 守卫 | spec.md 期望 | design.md 实际（行号）| 差距 |
|------|-------------|---------------------|------|
| R3 `register_gpu(0, ...)` | 返回 -1 + **错误日志**（line 71-73）| 仅返回 -1，无输出（line 170-184）| 缺日志 |
| R4 `create_queue(priority>100)` | 返回 0 + 错误日志（line 100-102）| 返回 0，无日志（line 188-207）| 缺日志 |
| R4 `create_queue(ring_buffer_size==0)` | 返回 0 + 错误日志（line 100-102）| **未实现**该检查（line 188-207）| 缺整个检查 + 日志 |

**建议改法**（design.md `register_gpu` 代码段，line 170-184）：

```cpp
// 改前:
if (va_space_handle == 0) return -1;

// 改后:
if (va_space_handle == 0) {
    std::cerr << "[GpuDriverClient] register_gpu: rejected H-1 sentinel (va_space_handle=0)"
              << std::endl;
    return -1;
}
```

**建议改法**（design.md `create_queue` 代码段，line 188-207）：

```cpp
// 改前:
if (priority > 100) return 0;

// 改后:
if (priority > 100) {
    std::cerr << "[GpuDriverClient] create_queue: invalid priority " << priority
              << " (valid range: 0-100)" << std::endl;
    return 0;
}
if (ring_buffer_size == 0) {
    std::cerr << "[GpuDriverClient] create_queue: invalid ring_buffer_size 0"
              << std::endl;
    return 0;
}
```

**验证步骤**：
```bash
# 在 TaskRunner 仓跑对应 test case
cd external/TaskRunner
./build/bin/test_gpu_phase2 --test-case="register_gpu_va_space_handle_zero_guard"
./build/bin/test_gpu_phase2 --test-case="create_queue_invalid_priority"
./build/bin/test_gpu_phase2 --test-case="create_queue_zero_ring_buffer_size"
# 预期: 全部通过 + stderr 包含相应错误信息
```

---

### B3 [MODERATE] — spec.md R9 scenario 2 内部矛盾

**问题**：spec.md R9 (line 191-211) 两个 scenario 自相矛盾。

| Scenario | 内容 |
|----------|------|
| R9 前置条件（line 192）| "Tests MUST NOT trigger real `/dev/gpgpu0` ioctl calls" |
| R9 scenario 2（line 209-211）| "switching the constructor argument to `GpuDriverClient*` (real) yields identical test outcomes" |

`GpuDriverClient*` 是真 ioctl 路径，必然触发真设备调用，与前置条件直接冲突。

**建议改法**：

```markdown
# spec.md line 207-211 - 删除或修改
- 改前: #### Scenario: Mock swap roundtrip
         - **WHEN** a caller swaps `MockGpuDriver*` to `GpuDriverClient*` via the same `IGpuDriver*` slot
         - **THEN** the test re-runs identical assertions against the real client
         - **AND** switching the constructor argument to `GpuDriverClient*` (real) yields identical test outcomes (same assertions on return values)

+ 改后: #### Scenario: Mock swap roundtrip
         - **WHEN** a caller swaps `MockGpuDriver*` to `CudaStub*` via the same `IGpuDriver*` slot
         - **THEN** the test re-runs identical assertions against the in-process mock
         - **AND** no real `/dev/gpgpu0` ioctl calls occur (mock-only contract per R9 line 192)
```

**理由**：与 CudaStub 互切（都是 in-process mock）符合"不触发真 ioctl"约束；如果要验证 `GpuDriverClient*` 路径，应在**集成测试阶段**（独立 change）做，不在 unit test scope。

---

### B4 [MODERATE] — `plans/sync-plan.md` 路径歧义

**问题**：proposal.md + tasks.md 引用 `plans/sync-plan.md`，但实际位置在 `external/TaskRunner/plans/sync-plan.md`（UsrLinuxEmu 根目录无 `plans/sync-plan.md`）。

| 文件 | 行号 | 当前 |
|------|------|------|
| `proposal.md` | 104 | "plans/sync-plan.md line 247-249 改为 '✅ 已完成'" |
| `tasks.md` | 108 | "编辑 plans/sync-plan.md line 247-249" |

**建议改法**：

```markdown
# proposal.md line 104
- 改前: `plans/sync-plan.md` line 247-249
+ 改后: `external/TaskRunner/plans/sync-plan.md` line 247-249

# tasks.md line 108
- 改前: 编辑 plans/sync-plan.md line 247-249
+ 改后: 编辑 external/TaskRunner/plans/sync-plan.md line 247-249
```

**验证步骤**：
```bash
ls /workspace/project/UsrLinuxEmu/external/TaskRunner/plans/sync-plan.md
ls /workspace/project/UsrLinuxEmu/plans/sync-plan.md 2>&1
# 预期: 前者存在，后者 No such file or directory
```

---

## 四、建议改项（Non-blocking）— 7 项

### N1 [MINOR] — H-1 方法名命名不一致

`design.md` 用 `setCurrentVASpace` (CamelCase)，`spec.md` line 171, 173 用 `set_current_va_space` (snake_case)。

```markdown
# spec.md line 171, 173
- 改前: set_current_va_space / get_current_va_space
+ 改后: setCurrentVASpace / getCurrentVASpace
```

理由：与 H-1 现有代码 + design.md + README.md 4:1 票一致。

### N2 [MINOR] — tasks.md §5.5 缺少 R6 scenarios 2, 3 测试

spec.md R6 定义 3 个 scenarios，tasks.md §5.5 只列 1 个 test case。

```markdown
# tasks.md §5.5 - 补充 2 个 test case
- 增加: [ ] 5.5b Test 10b: r2_mapping_truncation_loses_upper_bits
        - 流程: caller 保存 queue_handle=0x100000001 → 截断为 uint32_t stream_id=1 自行跟踪
        - 验证: 后续 destroy_queue() 用截断的 stream_id 派生 handle 找不到 → -ENOENT
- 增加: [ ] 5.5c Test 10c: r2_mapping_custom_counter_diverges
        - 流程: caller 维护自己的计数器 next_id_=1,2,3... 不等于 LOW32(queue_handle)
        - 验证: submit_batch(stream_id=next_id_) 触发 -EINVAL
```

### N3 [MINOR] — 日期占位符未填

```markdown
# proposal.md line 105
- 改前: S5 ✅ Phase 2 管理 API (2026-06-XX)
+ 改后: S5 ✅ Phase 2 管理 API (2026-06-22)
```

### N4 [MINOR] — H-2.5 路径引用缺日期前缀

```markdown
# proposal.md line 141, 148 / design.md line 258
- 改前: openspec/changes/archive/h2-5-architecture-foundation/
+ 改后: openspec/changes/archive/2026-06-19-h2-5-architecture-foundation/
```

### N5 [MINOR] — design.md D1 "维护" vs "保留"语义模糊

```markdown
# design.md line 73
- 改前: GpuDriverClient 不在内部维护 current_va_space_handle_（该字段在 H-1 时期存在，H-3 之后继续保留为透传缓存，但不在 create_va_space() 中自动 set）

+ 改后: GpuDriverClient 保留 current_va_space_handle_ 字段（H-1 兼容性），但不在 create_va_space() 中自动赋值；该字段仅作为 setCurrentVASpace() 显式调用的缓存，create_va_space() 不修改其值。
```

### N6 [MINOR] — spec.md R2 vs R3 守卫日志行为不一致

R2 (line 55-57) 守卫静默无日志，R3 (line 71-73) 守卫要求日志。建议统一为 R2 风格（sentinel 静默）：

```markdown
# spec.md line 71-73
- 改前: - **AND** an error message is logged (guard explicitly rejects H-1 sentinel)
+ 改后: - **AND** no error is logged (H-1 sentinel is a programming error, not runtime error; consistent with R2)
```

### N7 [MINOR] — tasks.md §5.6 CMakeLists.txt 缺 doctest linkage

```cmake
# tasks.md line 75-79 - 补全
- 改前: add_executable(test_gpu_phase2 tests/test_gpu_phase2.cpp)
        add_test(NAME test_gpu_phase2 COMMAND test_gpu_phase2)

+ 改后: add_executable(test_gpu_phase2 tests/test_gpu_phase2.cpp)
        target_link_libraries(test_gpu_phase2 doctest_with_main)
        add_test(NAME test_gpu_phase2 COMMAND test_gpu_phase2)
```

---

## 五、验证通过的引用（无需修改）

| 引用 | 位置 | 实际 | 状态 |
|------|------|------|------|
| `gpu_ioctl.h:166` | spec.md R1 | `GPU_IOCTL_CREATE_VA_SPACE` | ✅ |
| `gpu_ioctl.h:177` | spec.md R2 | `GPU_IOCTL_DESTROY_VA_SPACE` | ✅ |
| `gpu_ioctl.h:184` | spec.md R3 | `GPU_IOCTL_REGISTER_GPU` | ✅ |
| `gpu_ioctl.h:203` | spec.md R4 | `GPU_IOCTL_CREATE_QUEUE` | ✅ |
| `gpu_ioctl.h:217` | spec.md R5 | `GPU_IOCTL_DESTROY_QUEUE` | ✅ |
| `gpgpu_device.cpp:260-262` | design.md R2 mapping | `attached_queues` lookup | ✅ |
| `gpgpu_device.cpp:412` | design.md D2/R2 | `next_queue_handle_++` | ✅ |
| `gpgpu_device.cpp:284-300` | design.md R2 | `hal_doorbell_ring(hal_, args->stream_id)` | ✅ |

所有上游 ioctl 编号 + 行号引用均经 UsrLinuxEmu 仓实际代码验证。

---

## 六、保留的优点

- ✅ 4-artifact 结构完整（proposal/design/tasks/specs/*.md）
- ✅ D1-D5 全部 FINALIZED，决策矩阵 C/B/B/B/B 清晰
- ✅ 9 ADDED Requirements + 10 test cases 完整
- ✅ 5 个方法签名含完整 guard 逻辑
- ✅ R2 Mapping Contract 是核心约束，三处一致描述（spec.md R6 / design.md §R2 / tasks.md §5.5）
- ✅ 4 阶段 Migration Plan + Rollback 表
- ✅ 7 风险项 + 影响评级
- ✅ 跨仓同步步骤明确（含 submodule 处理）
- ✅ 3 个 owner-flagged issues 显式 defer 到 H-7 ADR，scope 边界清晰
- ✅ "Open Questions: 无" 与 D1-D5 "FINALIZED" 一致

---

## 七、推荐修复流程（TaskRunner owner 视角）

### 步骤 1 — 应用 B1 修复（约 10 分钟）

```bash
cd /path/to/TaskRunner
git checkout -b fix/h3-review-feedback
# 编辑 5 个文件: .openspec.yaml + README.md + proposal.md + design.md + tasks.md
# 应用 B1 修复（前置依赖状态同步）
```

### 步骤 2 — 应用 B2 修复（约 15 分钟）

```bash
# 编辑 design.md line 170-184 (register_gpu) + line 188-207 (create_queue)
# 应用 B2 修复（守卫 + 日志）
```

### 步骤 3 — 应用 B3 + B4 修复（约 10 分钟）

```bash
# 编辑 spec.md line 207-211 (B3)
# 编辑 proposal.md line 104 + tasks.md line 108 (B4)
```

### 步骤 4 — 应用 N1-N7 修复（可选，约 20 分钟）

按本文 §四 顺序应用 7 项 MINOR 修改。

### 步骤 5 — 验证（约 10 分钟）

```bash
# 在 TaskRunner 仓
cd /path/to/TaskRunner
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j4 test_gpu_phase2
./bin/test_gpu_phase2
# 预期: 10/10 test cases 通过（含 B2 新增的 2 个守卫测试 + N2 新增的 2 个 R6 测试 = 12/12）

# 验证 4-artifact 完整性
cd /path/to/TaskRunner
openspec validate plans/2026-06-19-h3-phase2-openspec-skeleton/
# 预期: schema 通过 + 9 ADDED Requirements + 0 contradictions
```

### 步骤 6 — 提交 + 跨仓 PR（约 15 分钟）

```bash
git add .
git commit -m "fix(plan): address H-3 review feedback (B1-B4 + N1-N7)

- B1 CRITICAL: sync H-2.5 archived status (no longer 'must complete first')
- B2 MODERATE: align spec.md R3/R4 guards with design.md (logs + ring_size==0 check)
- B3 MODERATE: fix spec.md R9 scenario 2 internal contradiction (MockGpuDriver ↔ CudaStub)
- B4 MODERATE: fix sync-plan.md path ambiguity (external/TaskRunner/plans/...)
- N1-N7 MINOR: naming consistency, test coverage, date placeholders, paths, semantic clarity

Refs: UsrLinuxEmu docs/07-integration/h3-plan-review-feedback.md
Test plan: 10 + 4 (B2+N2) = 14 doctest cases, all green"

git push origin fix/h3-review-feedback
# 在 GitHub 创建 PR 指向 TaskRunner main
# PR description 引用本 review feedback 文档
```

### 步骤 7 — 跨仓同步触发 H-3 激活

PR 合并后：
```bash
# 在 UsrLinuxEmu 仓
cd /workspace/project/UsrLinuxEmu
git submodule update --remote external/TaskRunner
mv external/TaskRunner/plans/2026-06-19-h3-phase2-openspec-skeleton \
   openspec/changes/h3-phase2-management
# 然后执行 h3 tasks.md §8 验证 + 归档流程
```

---

## 八、附录

### 8.1 时间线（2026-06-22 TaskRunner 仓关键节点）

| 时间 | Commit | 事件 |
|------|--------|------|
| 01:19-01:28 | — | h2 + h3 openspec 骨架创建/标记 DEPRECATED |
| 11:47 | `4834d5a` | IGpuDriver 抽象接口添加 |
| 11:49 | `1684fa1` | H-2.5 实施完成（D6-D11 FINAL）|
| 11:50 | `65c54fb` | H-2.5 + H-3 + DEPRECATED H-2 骨架归档为 git 记录 |
| 12:00 | `82b13f7` | H-2.5 骨架从 plans/ 移除（已迁移到 UsrLinuxEmu）|
| 12:07 | `0f0d5af` | **★ sync-plan.md 同步 S5 ✅ 状态**（H-3 激活前置条件明确化）|

### 8.2 h2 vs h3 关系图

```
2026-06-19  H-1 closeout (PR #6)
     │
     ├─► DEPRECATED H-2 (Oracle review 揭示 GpuDriverClient 是 dead code)
     │
     └─► 2026-06-22 11:47-12:07
              ├─ 11:47 H-2.5 IGpuDriver 抽象 (4834d5a)
              ├─ 11:49 H-2.5 实施完成 (1684fa1)
              ├─ 11:50 骨架归档为 git 记录 (65c54fb)
              ├─ 12:00 H-2.5 迁移到 UsrLinuxEmu (82b13f7) ← 实际 archive
              └─ 12:07 sync-plan.md 同步 S5 ✅ (0f0d5af)
                     │
                     └─► 2026-06-22 01:25-01:28  H-3 DRAFT 创建
                                 │
                                 └─► ⚠️ 待激活（需先 review 通过本文 B1-B4）
```

### 8.3 相关文档（UsrLinuxEmu 仓内）

- `docs/00_adr/adr-015-gpu-ioctl-unification.md` — System C ioctl 统一
- `docs/00_adr/adr-017-gpfifo-queue-abstraction.md` — Queue 抽象（与 H-3 直接相关）
- `docs/00_adr/adr-024-user-mode-queue-submission.md` — 用户态队列提交（R2 mapping 来源）
- `docs/02_architecture/post-refactor-architecture.md` — 重构后架构 SSOT
- `docs/06-reference/ioctl-commands.md` — ioctl 完整参考
- `docs/07-integration/taskrunner-index.md` — 跨仓协同工作文档索引
- `plugins/gpu_driver/shared/gpu_ioctl.h` — Canonical ioctl 定义（System C）
- `plugins/gpu_driver/drv/gpgpu_device.cpp` — 驱动实现（行号引用已验证）

### 8.4 上游 owner-flagged issues（defer 到 H-7 ADR，不在 H-3 scope）

| Issue | 简述 | 状态 |
|-------|------|------|
| stream_id u32 vs queue_handle u64 | 类型不匹配，依赖 LOW32 隐式截断 | ⏸️ H-7 ADR |
| ioctl 路径绕过 GpuQueueEmu | 走 GPFIFO_BASE 直接通道，不用 Queue 实例 | ⏸️ H-7 ADR |
| attached_queues 弱校验 | 跨 VA Space 不拦截 | ⏸️ H-7 ADR |

---

**最后更新**: 2026-06-22
**维护者**: UsrLinuxEmu Architecture Team
**对应审查目录**: `external/TaskRunner/plans/2026-06-19-h3-phase2-openspec-skeleton/`
**反馈渠道**: TaskRunner PR 引用本文件；或 UsrLinuxEmu Issue 同步
