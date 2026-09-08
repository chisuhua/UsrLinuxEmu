# Tasks: 5.5.7-cpptlm-cp-real-ification — dGPU E2E 主线 #2 CP 真实化

> **TDD 纪律**: 每个 task 顺序 = Write test → Verify fail → Implement → Verify pass → Commit
> **状态**: 🔄 Proposed v1.0（2026-09-09）
> **前置基线**: 5.5.6 dGPU E2E 主线 P0 ship（commit c63a9f3 + 4bf7508 + 0e300ef）+ Oracle 9.5/10
> **工期**: 1-2 周
> **关联**: [proposal.md](proposal.md) + [design.md](design.md) + [specs/cpptlm-cp-real-ification/spec.md](specs/cpptlm-cp-real-ification/spec.md)

---

## §1 任务总览

| Wave | 子任务 | 工期 | 依赖 |
|------|--------|:---:|------|
| **P5.NEW-A** | `test_bridge_kcpptlm_profile_real_standalone` 5 用例 + 决策 D.1 落地 | 0.5-1 周 | 5.5.6 ABI 通道 |
| **P5.NEW-B** | 决策 D.2 adapter op 接入点 + TaskRunner 集成路径 | 0.5 周 | D.1 |
| **P5.NEW-C** | 决策 D.3 ctest WORKING_DIRECTORY 策略 | 0.5 周 | D.1 + D.2 |
| **总计** | | **1.5-2 周** | |

---

## §2 P5.NEW-A：profile 真实化验证 + D.1 决策

### 任务 A.1：建立测试骨架

- [ ] **Write test**: `tests/sim_hardware/test_bridge_kcpptlm_profile_real_standalone.cpp`
  - `TEST_CASE("bridge: kCpptlm mmio_read ret==0 with dgpu_board_v1.json", "[profile][mmio]")`
  - `TEST_CASE("bridge: kCpptlm mmio_write ret==0 with dgpu_board_v1.json", "[profile][mmio]")`
  - `TEST_CASE("bridge: kCpptlm backdoor_read ret==0 with dgpu_board_v1.json", "[profile][backdoor]")`
  - `TEST_CASE("bridge: kCpptlm backdoor_write ret==0 with dgpu_board_v1.json", "[profile][backdoor]")`
  - `TEST_CASE("bridge: 5.5.7 profile gate", "[profile][gate]")`
- [ ] **Verify fail**: 4 数据通路函数 ret == -110（-ETIMEDOUT，无 CP attach）
- [ ] **Implement 骨架**: 沿用 5.5.6 `test_bridge_kcpptlm_data_path_standalone` 模式，加 topology_path="configs/dgpu_board_v1.json"
- [ ] **Verify pass**: gate 测试验证 ABI 通道已通（!= -ENOSYS），D.1 决策后定 ret==0 强约束

### 任务 A.2：决策 D.1 -ETIMEDOUT 语义

- [ ] **Write decision**: `decisions/D1-etimedout-semantics.md`
  - 选项 X：接受 -ETIMEDOUT 为正常返回（5.5.7 SKIP ret==0 强约束，5.5.8 启动时 attach CP）
  - 选项 Y：修复 CppTLM profile（跨仓改动，超出 5.5.7 scope）
  - 选项 Z：UsrLinuxEmu 端 register noop callback（影响 5.5.6 行为）
  - 推荐 X
- [ ] **Verify fail**: D.1 未决策前，profile 测试无法稳定 gate
- [ ] **Implement decision**: D.1 记录 + 反馈到 A.1 测试
- [ ] **Verify pass**: D.1 决策后，profile 测试按决策结果稳定（SKIP 或 REQUIRE）

### 任务 A.3：回归测试

- [ ] ctest 174/174 PASS（+5 新测试）
- [ ] docs-audit PASS
- [ ] Oracle 最终审查 ≥ 9.0/10

---

## §3 P5.NEW-B：决策 D.2 adapter op 接入点

### 任务 B.1：决策 D.2 adapter op 接入点

- [ ] **Write decision**: `decisions/D2-adapter-op-access.md`
  - 选项 P：TaskRunner 直调 `gpu_hal_ops.adapter_*`（HAL 契约层）
  - 选项 Q：GpgpuDevice ioctl 派发表分发（drv/ 改动）
  - 选项 R：双轨（TaskRunner 默认 P，GpgpuDevice 可选 Q）
  - 推荐 P
- [ ] **Verify fail**: D.2 未决策前，5.5.8 TaskRunner 集成路径模糊
- [ ] **Implement decision**: D.2 记录 + 反馈到 5.5.8 立项
- [ ] **Verify pass**: D.2 决策后，5.5.8 启动路径明确

---

## §4 P5.NEW-C：决策 D.3 ctest WORKING_DIRECTORY

### 任务 C.1：决策 D.3 ctest cwd

- [ ] **Write decision**: `decisions/D3-ctest-cwd.md`
  - 选项 A：ctest WORKING_DIRECTORY 改 CppTLM（影响所有 169 个测试）
  - 选项 B：测试内 chdir（不可重入）
  - 选项 C：测试内绝对路径 topology_path（硬编码）
  - 选项 D：保持当前模式（profile 测试需显式从 CppTLM 跑）
  - 推荐 D
- [ ] **Verify fail**: D.3 未决策前，profile 测试运行方式模糊
- [ ] **Implement decision**: D.3 记录 + 反馈到 5.5.7 文档
- [ ] **Verify pass**: D.3 决策后，profile 测试运行方式明确

---

## §5 关键路径

**P5.NEW-A.1** → **P5.NEW-A.2** (D.1) → **P5.NEW-A.3** (回归) → **P5.NEW-B.1** (D.2) → **P5.NEW-C.1** (D.3)

**总工时**: 0.5-1 + 0.5 + 0.5 = **1.5-2 周**

---

## §6 风险与回退

### 风险 1：CppTLM profile 行为与文档不符（中）

**症状**：ret == 0 期望但实际返 -ETIMEDOUT/-EINVAL
**回退**：D.1 决策 X 接受 -ETIMEDOUT，profile 测试 SKIP ret==0 强约束

### 风险 2：adapter op 接入点影响 drv/ 边界（中）

**症状**：选 Q 导致 drv/ 改动，破坏 5.5.6 G1-G4 边界契约
**回退**：D.2 决策 P TaskRunner 直调，零 drv/ 改动

### 风险 3：ctest cwd 改动影响其他测试（高）

**症状**：选 A 导致 169 个测试 cwd 改变，可能连锁失败
**回退**：D.3 决策 D 保持当前模式，零全局影响

### 风险 4：真实 ABI 验证引入 flaky（与 backdoor_endpoint 同样）（中）

**症状**：profile 测试 33% 失败率
**回退**：沿用 0e300ef flaky fix 模式：CHECK(rd_ret != -ENOSYS) + INFO 解释

---

## §7 跨引用

- [proposal.md](proposal.md) — Why/What/Capabilities/Impact
- [design.md](design.md) — 技术设计
- [specs/cpptlm-cp-real-ification/spec.md](specs/cpptlm-cp-real-ification/spec.md) — capability 规范
- [前置 5.5.6](../archive/2026-09-08-2026-09-08-5-5-6-cpptlm-ep-binding/) — ABI 通道基线
- [kcpptlm-archive-audit](../archive/2026-09-08-2026-09-08-kcpptlm-archive-audit/) — 前置审计
- [ADR-091](../../00_adr/adr-091-pci-driver-architecture-and-four-quadrant.md) — 4 象限
- [ADR-092](../../00_adr/adr-092-hal-adapter-and-bypass-binding.md) — HAL adapter binding
- [ADR-023](../../00_adr/adr-023-hal-interface.md) — HAL append-only

---

**任务作者**: UsrLinuxEmu Architecture Team
**创建日期**: 2026-09-09
**预期完成**: 2026-09-23（1-2 周后）
