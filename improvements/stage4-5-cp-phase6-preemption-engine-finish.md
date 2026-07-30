# stage4-5-cp-phase6-preemption-engine-finish

**优先级**: P1 | **来源**: ADR-046 + ADR-045 — Stage 4.5 Phase 6 Preemption 引擎收尾
**阶段**: stage-4 | **分类**: core-impl
**类型**: functional

## 架构依据

Stage 4.5 第一阶段（`stage4-5-cp-phase6-preemption-timeline-sem`）已完成 Priority Scheduling 升级、Timeline Semaphore（ADR-049 ✅ Accepted）、ADR-040 迁移、HAL Ops 扩展和 Sim C-ABI Backdoor，但 **Preemption Engine 核心**（ADR-046 PROPOSED）的 MQD state save/restore 和 fence 表未完成。

根据拆分方案 B（Preemption + 最小跨引擎 fence），Timeline Semaphore 已作为跨引擎 fence 基础完成，Preemption 引擎的 mid-batch context save/restore + quantum 管理 + fence 语义需要补齐。

ADR-046 定义了 Dispatch-level 抢占协议：高优先级 batch 提交 → saveContext → 标记 PREEMPTED → 激活高优先级通道 → 完成后 restoreContext → 从保存点继续执行。Save/Restore 复用 ADR-054 MQD 结构。

## 范围

- **In Scope**:
  - `mqd_state_preempt()` 接入 preemption 流程：ACTIVE → PREEMPTED，保存 gpfifo_addr/index/entries
  - `mqd_state_resume()` 接入 resume 流程：PREEMPTED → ACTIVE，恢复 gpfifo 位置
  - 边界处理：IDLE 通道 no-op、double-preempt no-op、resume on non-PREEMPTED -EINVAL
  - 每通道 pending fence 表（`std::unordered_map<fence_id_t, SemHandle>`），驱动侧实现，不修改 `mqd.h` ABI
  - Fence 语义：preempt→resume 间隙不 signal fence，fence signal 绑定到 resumed batch 完成
  - `test_preemption_standalone`：所有状态转换 + fence 语义 + IB jump_stack 安全
  - Sanitizer 验证：ASan/UBSan + TSan 全绿
  - Docs audit 通过

- **Out Scope**:
  - Wavefront-level 抢占（ADR-046 D4，Phase 6 不实现）
  - Green Context / PDL（属于 Phase 7，ADR-056）
  - Predication（独立提案 `stage4-5-cp-phase6-predication-aql`）
  - AQL/PM4 支持（独立提案 `stage4-5-cp-phase6-predication-aql`）

## 关键场景

- GIVEN 高优先级通道提交新 batch WHEN 当前通道正在执行 mid-batch THEN Puller 在 batch 边界触发 preempt，调用 `mqd_state_preempt()` 保存状态，切换到高优先级通道
- GIVEN preempted 通道的 batch 被恢复 WHEN 高优先级通道完成 THEN 调用 `mqd_state_resume()` 恢复 gpfifo 位置，从保存点继续执行
- GIVEN preempt 触发时通道为 IDLE WHEN `mqd_state_preempt()` 被调用 THEN 返回 0 no-op，不改变状态
- GIVEN 通道已被 PREEMPTED WHEN 再次收到 preempt THEN no-op 返回 0
- GIVEN 通道为 ACTIVE 状态 WHEN `mqd_state_resume()` 被调用 THEN 返回 -EINVAL
- GIVEN batch 处于 preempted 状态 WHEN 关联 fence 被查询 THEN fence 不 signal，直到 resumed batch 完成
- GIVEN 每通道 pending fence 表 WHEN 通道被 preempt THEN 所有 pending fence 保持未 signal 状态，绑定到 resumed batch

## 技术约束

- MUST 复用 ADR-054 MQD 结构中的 `PreemptContext` 字段，不新增 ABI
- MUST preempt 检查点仅在 Puller FSM batch 边界（DISPATCH→FETCH）触发，不在 mid-entry 中断
- MUST 每通道 pending fence 表为驱动侧实现，不暴露到 `mqd.h` 头文件
- MUST NOT 在 preempt→resume 间隙 signal 任何 fence
- MUST 实现 IB jump_stack 安全：`jump_stack_` 非空（IB 链执行中）时延迟抢占至 IB 链完成（对齐 timeline-sem 已归档实现与"MUST NOT mid-entry 抢占"约束）；jump_stack 为空边界抢占时，resume 执行结果与未抢占对照组逐字节一致
- MUST 通过 ASan/UBSan + TSan sanitizer 测试
- SHOULD 复用已归档 change 中已有的 Puller FSM PREEMPT_CHECK 状态定义

## 验收标准

- [ ] `mqd_state_preempt()` 正确保存 ACTIVE→PREEMPTED 状态，IDLE 通道 no-op，double-preempt no-op
- [ ] `mqd_state_resume()` 正确恢复 PREEMPTED→ACTIVE 状态，non-PREEMPTED 返回 -EINVAL
- [ ] 每通道 pending fence 表在 preempt→resume 间隙不 signal
- [ ] `test_preemption_standalone` 覆盖所有状态转换 + fence 语义 + IB jump_stack 安全
- [ ] `SANITIZER=asan-ubsan ./build.sh test` 全绿
- [ ] `SANITIZER=tsan ./build.sh test` 全绿
- [ ] `tools/docs-audit.sh --strict` PASS
- [ ] 无新 IOCTL 号暴露（`grep GPU_IOCTL_*` 前后对比）