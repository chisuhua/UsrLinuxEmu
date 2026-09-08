# D.1: -ETIMEDOUT 语义决策

> **状态**: 🔄 Proposed（5.5.7 P5.NEW-A.2）
> **决策日期**: 2026-09-09
> **影响范围**: 5.5.7 profile 测试 + 5.5.8 kernel dispatch 启动条件
> **关联**: [proposal.md](../proposal.md) §Why / [design.md](../design.md) §3.3

---

## §1 问题陈述

### 现象

5.5.6 ship 后，从 `/workspace/project/CppTLM` cwd 跑 profile 测试：

```bash
$ cd /workspace/project/CppTLM && \
  LD_PRELOAD=$(gcc -print-file-name=libasan.so) ASAN_OPTIONS=detect_leaks=0 \
  /workspace/project/UsrLinuxEmu/build/bin/test_bridge_kcpptlm_data_path_standalone
[bridge] OK: resolved 22 CppTLM ABI symbols (libcpptlm_emulator.so loaded for bridge.cpp)
===============================================================================
All tests passed (9 assertions in 5 test cases)
```

但 INFO 日志显示真实 ret 值：
- `mmio_read ret=-22 (or 4)` — 来自 CppTLM `DGpuBoard` 内部检查
- `mmio_write ret=0` — profile 支持
- `backdoor_read ret=4` — partial read
- `backdoor_write ret=0` — profile 支持

在某些 CppTLM 状态下，`mmio_read` 返 `-110`（-ETIMEDOUT），根因推测为 `DGpuBoard` 初始化时若未注册 `register_backdoor_cb` / `register_dma_translate_cb`，MMIO read 进入 wait queue 超时。

### 影响

- 5.5.7 profile 测试若 REQUIRE `ret == 0` 强约束，会 flaky（受 CppTLM 内部状态影响）
- 5.5.8 kernel dispatch 启动时需 attach CP，-ETIMEDOUT 可能在 CP attach 后消失
- TaskRunner 集成时若 user 模式跑测试，可能误判 -ETIMEDOUT 为真实失败

---

## §2 决策选项

### X. 接受 -ETIMEDOUT 为正常返回

**含义**：5.5.7 profile 测试 SKIP `ret == 0` 强约束，仅验证 ABI 通道已通（`ret != -ENOSYS`）。5.5.8 启动时 attach CP，-ETIMEDOUT 自然消失。

**优点**：
- 零 5.5.6 行为改动
- 零跨仓改动（CppTLM 端 profile 不动）
- profile 测试稳定（无 flaky）
- 与 5.5.6 P4.NEW-B.5 已 ship 的 `CHECK(rd_ret != -ENOSYS)` 模式一致

**缺点**：
- profile 测试不验证 `ret == 0`，5.5.8 启动时仍需做一次 attach CP 后的回归
- 5.5.9 真机验证时若发现 profile 与真机行为不一致，需重新决策

**5.5.8 影响**：必须先 attach CP 才能验证 ret == 0 强约束。

### Y. 修复 CppTLM profile

**含义**：在 `dgpu_board_v1.json` 加 CP 默认 attach 配置或修改 `DGpuBoard` 初始化逻辑。

**优点**：
- profile 测试可直接验证 `ret == 0`
- 5.5.8 启动条件简化

**缺点**：
- 跨仓改动（`/workspace/project/CppTLM/`）
- 超出 5.5.7 scope（5.5.7 是 verify-only）
- 需要 CppTLM 仓 PR + review
- 增加 5.5.7 工期 1-2 周

**5.5.8 影响**：无需 attach CP 即可验证 ret == 0。

### Z. UsrLinuxEmu 端 register noop callback

**含义**：bridge.cpp init 时调 `cpptlm_emulator_register_backdoor_cb(emu, noop)` + `cpptlm_emulator_register_dma_translate_cb(emu, noop)`，消除 -ETIMEDOUT 根因。

**优点**：
- 零跨仓改动
- profile 测试可直接验证 `ret == 0`
- 5.5.8 启动条件简化

**缺点**：
- 影响 5.5.6 已 ship 的行为（bridge init 副作用增加）
- 需 Oracle 复审（5.5.6 Oracle 9.4/10 PASS 状态可能不适用）
- noop callback 在 5.5.9 真机验证时可能掩盖真实行为

**5.5.8 影响**：无需 attach CP 即可验证 ret == 0。

---

## §3 推荐决策

**选项 X**（接受 -ETIMEDOUT 为正常返回）。

**理由**：
1. 5.5.7 scope 是 verify-only，零代码改动
2. 与 5.5.6 P4.NEW-B.5 已 ship 的 `CHECK(rd_ret != -ENOSYS)` 模式一致
3. 5.5.8 启动时自然要 attach CP（CommandProcessor 是 5.5.7 的下一步），-ETIMEDOUT 会在那时消失
4. 避免跨仓改动（Y）和 5.5.6 行为改动（Z）

**反馈到 5.5.7 测试**：
- `test_bridge_kcpptlm_profile_real_standalone` 中 4 数据通路测试断言改为 `CHECK(rd_ret != -ENOSYS)` + INFO 解释 -ETIMEDOUT 是 CppTLM profile 行为
- gate 测试验证 ABI 通道已通，ret == 0 强约束由 5.5.8 attach CP 后验证

**反馈到 5.5.8 启动条件**：
- 5.5.8 P5.NEW-X.1 第一步：实现 CP attach helper，验证 4 数据通路函数 ret == 0
- 5.5.8 立项 tasks.md 必须明确"CP attach 是 5.5.8 启动前置"

---

## §4 决策记录

| 字段 | 值 |
|------|-----|
| **决策 ID** | D.1 |
| **主题** | -ETIMEDOUT 语义 |
| **推荐选项** | X |
| **影响范围** | 5.5.7 profile 测试 + 5.5.8 启动条件 |
| **决策状态** | 🔄 Proposed（待 Oracle 确认） |
| **决策人** | Sisyphus (主对话 agent) |
| **决策日期** | 2026-09-09 |

---

## §5 跨引用

- [proposal.md](../proposal.md) — Why/What/Capabilities/Impact
- [design.md §3.3](../design.md) — -ETIMEDOUT 语义决策点
- [D.2: adapter op 接入点](D2-adapter-op-access.md)
- [D.3: ctest WORKING_DIRECTORY](D3-ctest-cwd.md)
- [5.5.6 P4.NEW-B.5 commit `4bf7508`](../../../../sim_hardware/src/cpptlm/bridge.cpp) — ABI 通道基线
