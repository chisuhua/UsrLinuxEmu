# Tasks: 5.5.8-cpptlm-kernel-dispatch-dma — dGPU E2E 主线 #3 CP Kernel Dispatch + DMA

> **TDD 纪律**: 每个 task 顺序 = Write test → Verify fail → Implement → Verify pass → Commit
> **状态**: 🔄 Proposed v1.0（2026-09-09）
> **前置基线**: 5.5.6 ABI 通道（Oracle 9.5/10）+ 5.5.7.1 P5.NEW-A profile 验证（Oracle 9.4/10 + D.1 Accepted）
> **工期**: 2-3 周
> **关联**: [proposal.md](proposal.md) + [design.md](design.md) + [specs/cpptlm-kernel-dispatch/spec.md](specs/cpptlm-kernel-dispatch/spec.md)

---

## §1 任务总览

| Wave | 子任务 | 工期 | 依赖 |
|------|--------|:---:|------|
| **P5.NEW-X.1** | CP attach helper 实施（消除 -ETIMEDOUT） | 0.5-1 周 | 5.5.7.1 D.1 Accepted |
| **P5.NEW-X.2** | `ret == 0` 强约束回归（5.5.7.1 测试升级） | 0.5 周 | P5.NEW-X.1 |
| **P5.NEW-X.3** | CommandProcessor 实施 | 0.5-1 周 | P5.NEW-X.2 |
| **P5.NEW-X.4** | DMA Engine 实施 | 0.5 周 | P5.NEW-X.3 |
| **P5.NEW-X.5** | TaskRunner 集成（D.2 决策 P 落地） | 0.5 周 | P5.NEW-X.4 |
| **总计** | | **2.5-3.5 周** | |

**关键路径**: P5.NEW-X.1 → X.2 → X.3 → X.4 → X.5

---

## §2 P5.NEW-X.1: CP attach helper 实施

### 任务 X.1.1：建立 CP attach 测试骨架

- [ ] **Write test**: `tests/sim_hardware/test_cp_attach_standalone.cpp`
  - `TEST_CASE("cp_attach: registers backdoor_cb without error", "[cp_attach]")`
  - `TEST_CASE("cp_attach: registers dma_translate_cb without error", "[cp_attach]")`
  - `TEST_CASE("cp_attach: NULL emu returns -EINVAL", "[cp_attach]")`
  - `TEST_CASE("cp_attach: with NULL callbacks (already registered)", "[cp_attach]")`
  - `TEST_CASE("cp_attach: idempotent (multiple calls safe)", "[cp_attach]")`
- [ ] **Verify fail**: cp_attach 函数未实现，测试编译失败（链接错误）
- [ ] **Implement 骨架**: `sim_hardware/src/cpptlm/cp_attach.cpp` + 暴露头文件
- [ ] **Verify pass**: 5 个测试用例全 PASS

### 任务 X.1.2：CP attach 集成到 bridge.cpp

- [ ] **Modify**: `sim_hardware/src/cpptlm/bridge.cpp` init() 阶段在 `create(path)` / `create_by_id(1)` 之后调 `cp_attach(emu)`
- [ ] **Verify**: 5.5.7.1 测试 `test_bridge_kcpptlm_profile_real_standalone` 5 个 TEST_CASE 仍 PASS（CP attach 是 noop，不影响 ABI 通道）

### 任务 X.1.3：回归测试

- [ ] ctest 169 baseline 保持（+1 新 binary test_cp_attach_standalone = 170 total）
- [ ] docs-audit PASS
- [ ] Oracle P5.NEW-X.1 审查 ≥ 9.0/10

---

## §3 P5.NEW-X.2: `ret == 0` 强约束回归

### 任务 X.2.1：升级 5.5.7.1 测试断言

- [ ] **Modify**: `tests/sim_hardware/test_bridge_kcpptlm_profile_real_standalone.cpp` 5 个 TEST_CASE
  - `CHECK(ret != -ENOSYS)` → `REQUIRE(ret == 0)`（mmio_read / mmio_write / backdoor_read / backdoor_write）
  - gate probe: `CHECK(ret != -ENOSYS)` → `REQUIRE(ret == 0)`
- [ ] **Verify fail** (若 CP attach 失败): 5 个测试 FAIL（-ETIMEDOUT 仍然存在）
- [ ] **Verify pass** (CP attach 成功): 5 个测试 3 次稳定 PASS

### 任务 X.2.2：回归测试

- [ ] ctest 170/170 PASS（+1 新 binary，含 P5.NEW-X.1 的 test_cp_attach_standalone）
- [ ] 3 次连续从 CppTLM cwd 跑 profile 测试全 PASS（验证 CP attach 消除 -ETIMEDOUT）
- [ ] **Opt-in 模式运行命令（独立复现）**：
  ```bash
  cd /workspace/project/CppTLM && \
    LD_PRELOAD=$(gcc -print-file-name=libasan.so) ASAN_OPTIONS=detect_leaks=0 \
    /workspace/project/UsrLinuxEmu/build/bin/test_bridge_kcpptlm_profile_real_standalone "[profile]"
  ```
- [ ] Oracle P5.NEW-X.2 审查 ≥ 9.0/10

---

## §4 P5.NEW-X.3: CommandProcessor 实施

### 任务 X.3.1：建立 CommandProcessor 测试骨架

- [ ] **Write test**: `tests/sim_hardware/test_command_processor_standalone.cpp`
  - `TEST_CASE("command_processor: PM4 packet submit success", "[cp][pm4]")`
  - `TEST_CASE("command_processor: ring buffer consume one entry", "[cp][ring]")`
  - `TEST_CASE("command_processor: dispatch to engine", "[cp][dispatch]")`
  - `TEST_CASE("command_processor: NULL buffer returns -EINVAL", "[cp]")`
  - `TEST_CASE("command_processor: queue overflow returns -ENOSPC", "[cp]")`
  - `TEST_CASE("command_processor: shutdown drains queue", "[cp]")`
  - `TEST_CASE("command_processor: ring buffer size boundary", "[cp][ring]")`
- [ ] **Verify fail**: command_processor 函数未实现
- [ ] **Implement**: `sim_hardware/src/cpptlm/command_processor.cpp`
  - PM4 packet submit
  - Ring buffer (consumer thread)
  - Dispatch to engine
- [ ] **Verify pass**: 7 个测试用例全 PASS

### 任务 X.3.2：回归测试

- [ ] ctest 171/171 PASS（+1 新 binary）
- [ ] docs-audit PASS
- [ ] Oracle P5.NEW-X.3 审查 ≥ 9.0/10

---

## §5 P5.NEW-X.4: DMA Engine 实施

### 任务 X.5.1：建立 DMA Engine 测试骨架

- [ ] **Write test**: `tests/sim_hardware/test_dma_engine_standalone.cpp`
  - `TEST_CASE("dma_engine: register callback success", "[dma]")`
  - `TEST_CASE("dma_engine: transfer submit success", "[dma]")`
  - `TEST_CASE("dma_engine: transfer complete callback fires", "[dma]")`
  - `TEST_CASE("dma_engine: identity mapping (iova == pa)", "[dma]")`
  - `TEST_CASE("dma_engine: NULL callback returns -EINVAL", "[dma]")`
  - `TEST_CASE("dma_engine: queue full returns -ENOSPC", "[dma]")`
- [ ] **Verify fail**: dma_engine 函数未实现
- [ ] **Implement**: `sim_hardware/src/cpptlm/dma_engine.cpp`
  - register dma_translate_cb（已由 CP attach 注册，可复用）
  - Transfer submit / complete
- [ ] **Verify pass**: 6 个测试用例全 PASS

### 任务 X.5.2：回归测试

- [ ] ctest 172/172 PASS（+1 新 binary）
- [ ] docs-audit PASS
- [ ] Oracle P5.NEW-X.4 审查 ≥ 9.0/10

---

## §6 P5.NEW-X.5: TaskRunner 集成（D.2 决策 P 落地）

### 任务 X.5.0：TaskRunner 子模块 setup（前置于 X.5.1）

- [ ] **前置检查**: `ls -la external/TaskRunner/UsrLinuxEmu` 确认符号链接是否存在
- [ ] **若不存在**，二选一：
  - (a) 创建符号链接：`cd external/TaskRunner && ln -s ../../ UsrLinuxEmu`
  - (b) TaskRunner CMakeLists 显式加 `include_directories(${UsrLinuxEmu_SOURCE_DIR}/plugins/gpu_driver/hal)`
- [ ] **Verify**: 任务 X.5.1 编译通过

### 任务 X.5.1：TaskRunner 直调 3 adapter op

- [ ] **Write test**: `tests/sim_hardware/test_taskrunner_adapter_integration_standalone.cpp` (如需)
  - `TEST_CASE("taskrunner: adapter_get_info populates fields", "[taskrunner][adapter]")`
  - `TEST_CASE("taskrunner: adapter_open returns valid handle", "[taskrunner][adapter]")`
  - `TEST_CASE("taskrunner: adapter_close releases handle", "[taskrunner][adapter]")`
- [ ] **Implement**: `external/TaskRunner/integrate_usrlx_emu.cpp`
  - 直调 `hal->adapter_get_info(hal, &info)`
  - 直调 `hal->adapter_open(hal, dev_id, &handle)`
  - 直调 `hal->adapter_close(hal, handle)`
- [ ] **Verify pass**: TaskRunner 3 op 集成测试 PASS

### 任务 X.5.2：回归测试

- [ ] ctest 173/173 PASS（+1 新 binary = 172 + 1 = 173）
- [ ] docs-audit PASS
- [ ] Oracle P5.NEW-X.5 审查 ≥ 9.0/10

---

## §7 关键路径

**P5.NEW-X.1** (CP attach) → **X.2** (ret==0 强约束) → **X.3** (CommandProcessor) → **X.4** (DMA) → **X.5** (TaskRunner)

**总工时**: 0.5-1 + 0.5 + 0.5-1 + 0.5 + 0.5 = **2.5-3.5 周**

---

## §8 风险与回退

### 风险 1：CppTLM callback 签名不匹配（中）

**症状**：`register_backdoor_cb` / `register_dma_translate_cb` 签名与 CppTLM 实际不匹配
**回退**：5.5.8.1 sub-change 实施前先 `dlsym` 验证签名，参考 CppTLM/include/abi/cpptlm_emulator.h

### 风险 2：CP attach 后 -ETIMEDOUT 仍偶发（中）

**症状**：5.5.8 P5.NEW-X.2 ret==0 强约束测试 flaky
**回退**：重新决策 D.1（Y 跨仓修复路径：修改 dgpu_board_v1.json profile）

### 风险 3：TaskRunner 跨子模块 include gpu_hal.h 编译失败（低）

**症状**：TaskRunner CMakeLists 找不到 UsrLinuxEmu include path
**回退**：在 TaskRunner CMakeLists.txt 加 `include_directories(${UsrLinuxEmu_SOURCE_DIR}/plugins/gpu_driver/hal)`

### 风险 4：169 ctest baseline 回归（低）

**症状**：CP attach 副作用影响已有 5.5.7.1 测试
**回退**：CP attach 设计为 noop callback（不修改 ABI 行为），理论无影响；实测若回归则回滚 P5.NEW-X.1.2

---

## §9 跨引用

- [proposal.md](proposal.md) — Why/What/Capabilities/Impact
- [design.md](design.md) — 技术设计
- [specs/cpptlm-kernel-dispatch/spec.md](specs/cpptlm-kernel-dispatch/spec.md) — capability 规范
- [前置 5.5.7.1 P5.NEW-A commit `2cf4bc5`](../2026-09-09-5-5-7-cpptlm-cp-real-ification/) — Oracle 9.4/10 PASS
- [5.5.6-archive](../archive/2026-09-08-2026-09-08-5-5-6-cpptlm-ep-binding/) — ABI 通道基线
- [D.1 Accepted](../2026-09-09-5-5-7-cpptlm-cp-real-ification/decisions/D1-etimedout-semantics.md) — CP attach 前置
- [D.2 adapter op 接入点](../2026-09-09-5-5-7-cpptlm-cp-real-ification/decisions/D2-adapter-op-access.md) — TaskRunner 直调
- [D.3 ctest WORKING_DIRECTORY](../2026-09-09-5-5-7-cpptlm-cp-real-ification/decisions/D3-ctest-cwd.md) — opt-in 模式

---

**任务作者**: UsrLinuxEmu Architecture Team
**创建日期**: 2026-09-09
**预期完成**: 2026-09-30（2-3 周后）
