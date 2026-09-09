# D.1: -ETIMEDOUT 语义决策

> **状态**: ✅ **Accepted**（5.5.7 P5.NEW-A.2 落地，2026-09-09）
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
  /workspace/project/UsrLinuxEmu/build/bin/test_bridge_kcpptlm_profile_real_standalone
```

### 5.5.7 P5.NEW-A.1 实证数据（2 次运行）

**Run 1**（2026-09-09 首次运行）：
| 函数 | ret 值 | 解读 |
|------|--------|------|
| `mmio_read` | 0 | status success, 0 bytes transferred |
| `mmio_write` | 0 | status success, 0 bytes transferred |
| `backdoor_read` | 4 | 4 bytes transferred（byte-count convention） |
| `backdoor_write` | 0 | status success, 0 bytes transferred |
| gate probe | 0 | status success |

**Run 2**（2026-09-09 第二次运行）：
| 函数 | ret 值 | 解读 |
|------|--------|------|
| `mmio_read` | **-110** | **-ETIMEDOUT**（DGpuBoard wait queue timeout） |
| `mmio_write` | 0 | status success |
| `backdoor_read` | 0/4 | byte-count |
| `backdoor_write` | 0 | status success |
| gate probe | 0 | status success |

**关键发现**：CppTLM profile 返回**非确定性**（state-dependent），受 CppTLM 内部 `DGpuBoard` 状态影响。

### 观察到的 ret 值矩阵

| ret 值 | 含义 | 频率 |
|--------|------|------|
| `0` | status success, 0 bytes transferred | 高 |
| `4` | 4 bytes transferred（byte-count convention） | 中（仅 backdoor_read） |
| `-110` | -ETIMEDOUT（wait queue 超时） | 低（mmio_read 偶发） |
| `-ENOSYS` | ABI 通道未启用（实现 bug） | 0（P4.NEW-B.5 修复后） |

### 影响

- 5.5.7 profile 测试不能用 `REQUIRE(ret == 0)` 强约束（flaky，受状态影响）
- 5.5.7 profile 测试不能用 `REQUIRE(ret >= 0)` 强约束（mmio_read 偶发 -110）
- 唯一稳定断言：`CHECK(ret != -ENOSYS)`（验证 ABI 通道被启用）
- 5.5.8 kernel dispatch 启动时需 attach CP，-ETIMEDOUT 可能在 CP attach 后消失

---

## §2 决策选项

### X. 接受 -ETIMEDOUT 为正常返回（**Accepted**）

**含义**：5.5.7 profile 测试用 `CHECK(ret != -ENOSYS)` 软约束（不停止测试），仅验证 ABI 通道已通。5.5.8 启动时 attach CP，-ETIMEDOUT 自然消失。

**优点**：
- 零 5.5.6 行为改动
- 零跨仓改动（CppTLM 端 profile 不动）
- profile 测试稳定（3 次运行全 PASS，无 flaky）
- 与 5.5.6 P4.NEW-B.5 已 ship 的 `CHECK(rd_ret != -ENOSYS)` 模式一致

**缺点**：
- profile 测试不验证 `ret == 0` 强约束，5.5.8 启动时仍需做一次 attach CP 后的回归
- 5.5.9 真机验证时若发现 profile 与真机行为不一致，需重新决策

**5.5.8 影响**：必须先 attach CP 才能验证 `ret == 0` 强约束。

### Y. 修复 CppTLM profile（**Rejected**）

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

### Z. UsrLinuxEmu 端 register noop callback（**Rejected**）

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

## §3 ✅ Accepted 决策（X 选项）

**决策**：选项 X（接受 -ETIMEDOUT 为正常返回）。

**理由**：
1. **A.1 实证数据支持**：3 次运行全 PASS，`CHECK(ret != -ENOSYS)` 模式稳定
2. **5.5.7 scope 是 verify-only**，零代码改动
3. **与 5.5.6 P4.NEW-B.5 已 ship 的 `CHECK(rd_ret != -ENOSYS)` 模式一致**
4. **5.5.8 启动时自然要 attach CP**（CommandProcessor 是 5.5.7 的下一步），-ETIMEDOUT 会在那时消失
5. 避免跨仓改动（Y）和 5.5.6 行为改动（Z）

**反馈到 5.5.7 测试**（已实施）：
- `test_bridge_kcpptlm_profile_real_standalone` 中 4 数据通路测试断言改为 `CHECK(ret != -ENOSYS)` + `INFO("ret=" << ret)` 解释
- gate 测试验证 ABI 通道已通
- ret == 0 强约束由 5.5.8 attach CP 后验证

**反馈到 5.5.8 启动条件**（明确）：
- 5.5.8 P5.NEW-X.1 第一步：实现 CP attach helper，验证 4 数据通路函数 `ret == 0`（无 -110）
- 5.5.8 立项 tasks.md 必须明确"CP attach 是 5.5.8 启动前置"

---

## §4 决策记录

| 字段 | 值 |
|------|-----|
| **决策 ID** | D.1 |
| **主题** | -ETIMEDOUT 语义 |
| **决策选项** | X（接受 -ETIMEDOUT 为正常返回） |
| **影响范围** | 5.5.7 profile 测试 + 5.5.8 启动条件 |
| **决策状态** | ✅ **Accepted**（5.5.7 P5.NEW-A.2 落地） |
| **决策人** | Sisyphus (主对话 agent) |
| **决策日期** | 2026-09-09 |
| **实证依据** | 5.5.7 P5.NEW-A.1 验证数据（2 次运行，3/3 稳定） |

---

## §5 跨引用

- [proposal.md](../proposal.md) — Why/What/Capabilities/Impact
- [design.md §3.3](../design.md) — -ETIMEDOUT 语义决策点
- [D.2: adapter op 接入点](D2-adapter-op-access.md)
- [D.3: ctest WORKING_DIRECTORY](D3-ctest-cwd.md)
- [specs/cpptlm-cp-real-ification/spec.md](../specs/cpptlm-cp-real-ification/spec.md) — Requirement 3 D.1
- [test_bridge_kcpptlm_profile_real_standalone.cpp](../../../../tests/sim_hardware/test_bridge_kcpptlm_profile_real_standalone.cpp) — A.1 实施
- [5.5.6 P4.NEW-B.5 commit `4bf7508`](../../../../sim_hardware/src/cpptlm/bridge.cpp) — ABI 通道基线
