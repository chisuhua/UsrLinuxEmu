# D.3: CTest WORKING_DIRECTORY 决策

> **状态**: 🔄 Proposed（5.5.7 P5.NEW-C.1）
> **决策日期**: 2026-09-09
> **影响范围**: 5.5.7 profile 测试运行模式 + 全部 ctest baseline
> **关联**: [proposal.md](../proposal.md) §Why / [design.md](../design.md) §3.2

---

## §1 问题陈述

### 现状

5.5.6 ship 后，profile-aware 测试（如 `test_bridge_kcpptlm_data_path_standalone`）使用 `topology_path = "configs/dgpu_board_v1.json"`，相对路径依赖 cwd。

CTest `WORKING_DIRECTORY` 默认为 `${PROJECT_SOURCE_DIR}`（UsrLinuxEmu 根），导致：
- 从 UsrLinuxEmu cwd 跑：找不到 `configs/dgpu_board_v1.json`（路径在 CppTLM 仓）
- 从 CppTLM cwd 跑：能找到，但需要用户显式切换目录

```bash
# 当前模式（5.5.6 ship 后）
$ cd /workspace/project/UsrLinuxEmu && \
  LD_PRELOAD=... ASAN_OPTIONS=... ctest --test-dir build
# 169/169 PASS（mock 路径，profile 测试 SKIP）

$ cd /workspace/project/CppTLM && \
  LD_PRELOAD=... ASAN_OPTIONS=... \
  /workspace/project/UsrLinuxEmu/build/bin/test_bridge_kcpptlm_data_path_standalone
# 5/5 PASS（profile 路径，需要显式 cwd 切换）
```

### 影响

- 5.5.7 profile 测试运行方式需明确
- 5.5.8 kernel dispatch 验证可能引入更多 profile-aware 测试
- 全部 169 个 ctest baseline 不能受影响

---

## §2 决策选项

### A. ctest WORKING_DIRECTORY 改 CppTLM

**含义**：在 `tests/CMakeLists.txt` 中修改 `add_catch_test` / `add_sim_test`，将 `WORKING_DIRECTORY` 改为 `/workspace/project/CppTLM`。

**优点**：
- profile 测试自动从正确 cwd 跑
- ctest 一键运行所有 profile-aware 测试

**缺点**：
- 影响所有 169 个测试的 cwd，可能连锁失败
  - `ModuleLoader::load_plugins("plugins")` 插件相对路径会断
  - 配置文件 `*.json` 相对路径会断
  - 已有测试的硬编码路径会断
- 跨平台风险（Windows 路径分隔符）

**5.5.8 影响**：所有 ctest baseline 需重新验证。

### B. 测试内 chdir

**含义**：profile 测试内 `chdir("/workspace/project/CppTLM")`。

**优点**：
- 局部影响（仅 profile 测试）
- ctest baseline 不变

**缺点**：
- 不可重入（多线程 CI 风险）
- chdir 影响整个进程，其他测试在同 binary 跑时受影响
- Catch2 TEST_CASE 间共享进程状态

**5.5.8 影响**：需 catch 多个测试间 cwd 状态。

### C. 测试内绝对路径 topology_path

**含义**：在 profile 测试内硬编码 `topology_path = "/workspace/project/CppTLM/configs/dgpu_board_v1.json"`。

**优点**：
- 零全局影响
- 跨 cwd 稳定
- 跨平台一致（绝对路径）

**缺点**：
- 硬编码路径，迁移性差（换机器需改）
- 与 5.5.6 P4.NEW-B.5 已 ship 的相对路径模式不一致

**5.5.8 影响**：需修改 5.5.6 P4.NEW-B.5 测试的 topology_path 风格。

### D. 保持当前模式（profile 测试需显式从 CppTLM 跑）

**含义**：profile 测试保留相对路径，依赖用户显式 opt-in（从 CppTLM cwd 跑）。

**优点**：
- 零全局影响（5.5.7 无 ctest baseline 改动）
- 与 5.5.6 P4.NEW-B.5 一致
- 显式 opt-in 避免 CI 噪声（无 CppTLM 仓时 profile 测试 SKIP）

**缺点**：
- 用户需知道运行模式（文档成本）
- 不能 ctest 一键跑所有 profile 测试

**5.5.8 影响**：所有 5.5.7+ 文档明确 opt-in 模式。

---

## §3 推荐决策

**选项 D**（保持当前模式）。

**理由**：
1. 5.5.7 scope 是 verify-only，零 ctest baseline 改动
2. 与 5.5.6 P4.NEW-B.5 已 ship 的相对路径模式一致
3. 显式 opt-in 避免 CI 噪声（169 baseline 不变）
4. 用户文档成本可控（5.5.7 spec.md 明确 opt-in 模式）
5. A 选项风险最高（影响所有 169 测试），B 选项不可重入，C 选项破坏 5.5.6 一致性

**反馈到 5.5.7**：
- 零代码改动（5.5.7 是 verify-only）
- `test_bridge_kcpptlm_profile_real_standalone` 沿用 5.5.6 `test_bridge_kcpptlm_data_path_standalone` 的 `topology_path = "configs/dgpu_board_v1.json"` 相对路径模式
- `cpptlm_lib_available()` helper 沿用 5.5.6 模式（dlopen 三个路径 + SKIP guard）
- 文档明确 opt-in：从 CppTLM cwd 跑 `build/bin/test_bridge_kcpptlm_profile_real_standalone [profile]`

**反馈到 5.5.8 启动条件**：
- 5.5.8 立项 tasks.md 必须明确"profile-aware 测试沿用 opt-in 模式，文档明确运行命令"
- 5.5.8 kernel dispatch 验证测试同样 opt-in

---

## §4 决策记录

| 字段 | 值 |
|------|-----|
| **决策 ID** | D.3 |
| **主题** | ctest WORKING_DIRECTORY |
| **推荐选项** | D |
| **影响范围** | 5.5.7 profile 测试运行模式 + 全部 ctest baseline |
| **决策状态** | 🔄 Proposed（待 Oracle 确认） |
| **决策人** | Sisyphus (主对话 agent) |
| **决策日期** | 2026-09-09 |

---

## §5 跨引用

- [proposal.md](../proposal.md) — Why/What/Capabilities/Impact
- [design.md §3.2](../design.md) — ctest WORKING_DIRECTORY 决策点
- [D.1: -ETIMEDOUT 语义](D1-etimedout-semantics.md)
- [D.2: adapter op 接入点](D2-adapter-op-access.md)
- [5.5.6 P4.NEW-B.5 commit `4bf7508`](../../../../sim_hardware/src/cpptlm/bridge.cpp) — topology_path 引入
- [5.5.6 P4.NEW-B.5 followup commit `0e300ef`](../../../../tests/sim_hardware/test_bridge_kcpptlm_data_path_standalone.cpp) — opt-in 模式基线
