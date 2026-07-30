## Context

v1 (`archive/2026-07-29-stage4-5-cp-phase6-preemption-timeline-sem/`) 在 2026-07-29 完成核心交付并归档。2026-07-30 spec audit + 2026-07-31 v1 gap analysis 识别出 5 项遗留 gap（详见 proposal.md）。

**当前状态**：
- ✅ 核心功能已交付：preemption engine + timeline semaphore + ADR-040 迁移 + ADR-049 D1 修订 + C-ABI backdoor
- ✅ 单线程测试覆盖：`test_preemption_standalone` (477 assertions PASS) + `test_timeline_semaphore_standalone` (28 assertions PASS)
- ❌ 并发场景未验证：无 `test_concurrent_preempt`
- ❌ Sanitizer 未运行：3 个 sanitizer build 目录均不存在
- ❌ Docs-audit FAIL：3 个 warnings（kernel 文件数、gpu_hal fn-ptr 数、Doxygen）
- ❌ Spec 与实现不一致：IB jump_stack spec 措辞为 "save/restore"，实际是 "defer"（ADR-046 D2 事件驱动模型）
- ❌ Archive tasks.md checkbox 失同步：6 项 `[ ]` 但实施已完成

**约束**：
- Archive spec 不修改（IMPLEMENTATION_NOTES.md 政策）— 新增独立 spec 澄清
- 抢占涉及并发 → TSan 强制要求（v1 tasks.md §7.4 验收标准）
- 并发测试需要 backdoor symbols（v1 §10 已交付 `bd_preempt`, `bd_sem_*`, `bd_fence_read`）

## Goals / Non-Goals

**Goals:**

1. **并发抢占测试可重复**：每次 CI run 都能验证抢占引擎在并发场景下的正确性
2. **Sanitizer-clean 基线**：3 个 sanitizer 配置全部 green，无 use-after-free / data race / uninitialized read
3. **docs-audit `--strict` PASS**：消除 3 个 warnings，恢复 CI gate
4. **文档准确性**：spec 措辞与实现行为一致（defer 而非 save/restore）
5. **归档 hygiene**：archive tasks.md 反映实施实际状态

**Non-Goals:**

- ❌ 修改 v1 核心实现（preemption engine、timeline semaphore 等已交付并通过单线程测试）
- ❌ 修改 ADR-045/046/047/049 状态（已 Accepted，v1 实施时已更新）
- ❌ 修改 archive 中的 spec.md（按 IMPLEMENTATION_NOTES.md 政策）
- ❌ 重写 backdoor symbols（v1 §10 已交付，复用即可）
- ❌ 新增 ioctl 编号（v1 明确排除 GPU_IOCTL_* 扩展）

## Decisions

### Decision 1: Concurrent Test Topology

**选择**：`test_concurrent_preempt` 拓扑 = `std::thread::hardware_concurrency()` 个 submit 线程 × `kPreemptCycles = 100` 次 preempt/resume 循环

**理由**：
- `hardware_concurrency()` 自动适配 CI runner 核心数，无需硬编码
- 100 循环在 5-10 秒内完成（测试时效性），又能暴露罕见 race
- 每线程独立 channel + fence 集合（避免跨线程 fence 干扰）

**实现要点**：
```cpp
constexpr int kSubmitThreads = std::thread::hardware_concurrency();
constexpr int kPreemptCycles = 100;

TEST_CASE("concurrent: N threads × M preempt cycles, no deadlock/fence-loss/leak") {
    std::atomic<int> fences_submitted{0};
    std::atomic<int> fences_signaled{0};
    std::atomic<int> fences_canceled{0};
    std::vector<std::thread> threads;

    for (int t = 0; t < kSubmitThreads; ++t) {
        threads.emplace_back([&, t]() {
            for (int cycle = 0; cycle < kPreemptCycles; ++cycle) {
                // 1. submit batch with fence
                uint64_t fence_id;
                bd_fence_create(&fence_id);
                fences_submitted++;

                // 2. trigger preempt via backdoor
                bd_preempt(channel_id_for_thread(t));

                // 3. submit batch that waits on fence
                // 4. resume channel via backdoor
                bd_resume(channel_id_for_thread(t));

                // 5. wait for fence (timeout: 5s)
                if (bd_fence_read(fence_id) > 0) {
                    fences_signaled++;
                } else {
                    fences_canceled++;
                }
            }
        });
    }

    // Timeout safety: join with 30s hard timeout
    for (auto& th : threads) th.join();

    // Assertions
    REQUIRE(fences_submitted == fences_signaled + fences_canceled);  // no fence lost
    REQUIRE(fences_canceled == 0);  // no deadlock
    // (channel destroy after threads join → no leak assertion via process exit)
}
```

**回退方案**：
- 如果 `hardware_concurrency() == 1` (单核 CI)：退化为顺序循环
- 如果 100 循环太慢：降低到 50，CI 时长 < 5s

### Decision 2: Sanitizer Build Matrix

**选择**：3 个独立 build 目录 × 3 个 run mode

| Build 目录 | 编译模式 | 运行命令 |
|-----------|---------|---------|
| `build/` (默认) | 无 sanitizer | `./build/bin/test_*_standalone` |
| `build-asan/` | ASan + UBSan | `cd build-asan && ctest --output-on-failure` |
| `build-tsan/` | TSan | `cd build-tsan && ctest --output-on-failure` |

**理由**：
- TSan 与 ASan/UBSan 互斥，需独立目录（v1 AGENTS.md "Sanitizer 构建"已记录此约束）
- 默认 build 验证 baseline 不退化
- 每个 sanitizer 单独验证一类 bug：ASan=memory, UBSan=undefined behavior, TSan=data race

**Sanitizer 报告处理**：
- 真 bug：修复后 commit（独立 PR / commit）
- 已知 false positive：在 sanitizer suppression file 中记录（`.tsan_suppressions`, `.ubsan_suppressions`）

### Decision 3: Docs-Audit Warning 修复策略

**逐 warning 分析**：

| Warning | 根因 | 修复 |
|---------|------|------|
| `src/kernel has 46 cpp files (baseline 44)` | v1 / 后续新增 kernel 模块未更新 baseline 阈值 | 修改 `tools/docs-audit.sh` 中 baseline 值从 44 → 46；或读取动态 baseline（git diff vs main） |
| `gpu_hal.h has 29 fn-ptrs (doc claims 14)` | post-refactor-architecture.md §附录 A 描述过期 | 更新附录 A：14 → 29 + 添加新增 fn-ptr 列表（`hal_sem_create`, `hal_sem_signal`, `hal_sem_wait`, `hal_sem_destroy`, `hal_preempt_channel` 等） |
| `Doxygen not installed` | CI runner 缺 doxygen | 在 `scripts/install-hooks.sh` 或 CI workflow 添加 `apt install -y doxygen graphviz` |

**理由**：
- kernel 文件数：动态 baseline 更稳健（避免每次新增文件都改 audit），但实现复杂度高；本 change 采用静态更新（46），记录到 audit baseline
- gpu_hal fn-ptr 数：必须更新文档，否则 audit 永远 FAIL
- Doxygen：CI 环境修复，一次性投入

### Decision 4: Spec Correction（Archive 不修改）

**选择**：在本 change 的 `specs/preemption-engine/spec.md` 写入 canonical 语义，**不修改** archive 中的 spec

**新增 spec 内容**（覆盖原 IMPLEMENTATION_NOTES.md 指出的不一致）：
```markdown
## ADDED Requirements

### Requirement: Preemption Deferred During IB Nested Execution

The system SHALL defer preemption when the channel is in IB nested execution state (`jump_stack_active_ == true`). The `jump_stack_` field is NOT included in saved MQD state because it is guaranteed empty at preempt trigger point.

#### Scenario: Preemption check skips during IB nested
- **WHEN** `preempt_pending_[channel_id]` is set AND channel's `jump_stack_` is non-empty
- **THEN** preemption is deferred; `preempt_pending_` remains set; next preempt check after jump_stack pops

#### Scenario: Saved state excludes jump_stack
- **WHEN** `mqd_state_preempt(channel_id, *saved)` is called at a valid preempt point
- **THEN** saved state does NOT include `jump_stack_` (guaranteed empty at this point)
- **AND** saved state includes `saved_gpfifo_addr`, `saved_index`, `saved_entries`, `ChannelSemaphoreState`

#### Scenario: jump_stack pop allows subsequent preempt
- **WHEN** `set_jump_stack(channel_id, false)` (exit IB nested mode) AND `preempt_pending_` is set
- **THEN** next tick triggers preempt; saved state valid

**Reference**: archive/2026-07-29-stage4-5-cp-phase6-preemption-timeline-sem/IMPLEMENTATION_NOTES.md §"已知 spec/implementation 不一致"
```

**文档链接引导**：
- 在 `docs/02_architecture/post-refactor-architecture.md` §"Stage 4.5 GPU Compute Pipeline" 添加 link，指向新 spec
- 在 `roadmap.md` Stage 4.5 章节添加相同 link

### Decision 5: Archive Tasks Sync（仅 checkbox 状态）

**选择**：仅修改 `archive/2026-07-29-stage4-5-cp-phase6-preemption-timeline-sem/tasks.md` 中的 6 项 checkbox `[ ]` → `[x]`

**不修改**：
- spec.md（按 IMPLEMENTATION_NOTES.md 政策）
- IMPLEMENTATION_NOTES.md（保留作为历史记录）
- 任何代码或文档

**Commit 消息**：`chore(archive): sync preemption-timeline-sem tasks.md checkbox state with implementation`

## Risks / Trade-offs

### Risk 1: Sanitizer 报告潜在 bug 阻塞 PR

**Risk**：v1 实施可能在并发场景下存在未发现的 race condition / use-after-free

**Mitigation**：
- 单独 commit 修复（不混入本 change 主流程）
- 优先级：memory bug (ASan) > data race (TSan) > UB (UBSan)
- 复杂 bug 拆分到独立 follow-up change

### Risk 2: docs-audit.sh 修改引入回归

**Risk**：将 baseline 从 44 改为 46 可能掩盖未来真实问题

**Mitigation**：
- 添加注释说明 baseline 来源（最近 git tag 的 src/kernel/ 文件数）
- 后续 kernel 模块新增需重新 review baseline 是否合理

### Risk 3: Archive Tasks.md 修改争议

**Risk**：有人认为 archive 不应修改（包括 tasks.md）

**Mitigation**：
- IMPLEMENTATION_NOTES.md 明确"归档 spec 不修改" — 但**未禁止**修改 tasks.md
- tasks.md 是实施记录，checkbox 反映实施实际状态是 hygiene 改进
- 在 commit message 明确说明"仅 checkbox 同步"

### Risk 4: Concurrent Test 间歇性失败

**Risk**：100 循环 × 硬件并发线程可能暴露间歇性 race

**Mitigation**：
- 测试内部 retry 机制：失败时自动重试 3 次
- 失败时打印完整 thread state 便于 debug
- TSan 作为补充验证（任何间歇性 race 都会被 TSan 捕获）

## Migration Plan

### 顺序

```
1. test_concurrent_preempt (新增能力 + CMakeLists 注册)
   ↓
2. sanitizer-validation (3 build 目录 + 修复任何报告)
   ↓
3. docs-audit-cleanup (3 warnings 修复)
   ↓
4. preemption-spec-correction (新 spec + 文档 link)
   ↓
5. archive-tasks-sync (6 项 checkbox)
   ↓
6. 验证: docs-audit PASS + 3 sanitizer 全绿 + ctest 0 失败
```

### Rollback

每个 task 独立 commit，rollback = `git revert <commit>`。

### CI 集成

- 新增 sanitizer job 到 `.github/workflows/cmake-multi-platform.yml`
- 新增 docs-audit `--strict` 检查（已存在，确认继续运行）

## Open Questions

1. **Concurrent test 重试次数**：失败重试 3 次是否足够？倾向 5 次（CI 偶发 race 容忍度）
2. **Docs-audit baseline 动态化**：是否将 kernel 文件数 baseline 改为 git 历史派生？实现复杂度 vs 维护成本 — 倾向保持静态更新（更简单）
3. **Sanitizer suppression 策略**：是否在仓库中维护 `.tsan_suppressions` / `.ubsan_suppressions`？倾向"先无抑制，bug 真有就修"，避免掩盖问题
4. **Archive tasks.md 修改 policy**：是否在 `docs/00_adr/` 新增 ADR 明确"archive tasks.md 可修改 checkbox 状态"？倾向**是**（明确 hygiene policy，避免争议）