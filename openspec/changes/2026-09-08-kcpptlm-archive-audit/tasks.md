# Tasks: kcpptlm-archive-audit — 归档勾选 vs 代码事实逐项核对

> **审计日期**: 2026-09-08
> **审计对象**: [kcpptlm-backend-binding change](openspec/changes/archive/2026-09-07-kcpptlm-backend-binding-with-handle-and-adapter-info/) 15 项 tasks
> **审计基线**: HEAD `0a8a53f` | 164/164 ctest PASS
> **审计方法**: 文件存在性 + 关键行号 grep + 与 Oracle 路线图分析报告交叉验证

---

## 审计矩阵

| # | Tasks 项 | 归档勾选 | 实际状态 | 证据（文件:行号） | 判定 |
|---|---------|:---:|---|---|:---:|
| **1.1** | `gpu_adapter_info_t` 结构（9 字段）| ✅ | `plugins/gpu_driver/shared/gpu_types.h:145-156` 结构完整，字段全 | `gpu_types.h:155 bar_sizes[6]` `gpu_types.h:156 gpu_adapter_info_t` | ✅ 真完成 |
| **1.2** | `gpu_adapter_handle_t` 类型（uint64_t）| ✅ | `include/shared/gpu_hal_handles.h:32 typedef uint64_t gpu_adapter_handle_t;` | `gpu_hal_handles.h:32` | ✅ 真完成 |
| **1.3** | `backdoor_endpoint.h` ABI 声明 | ✅ | header 含 5 函数 + `ule_dgpu_space` 枚举 | `backdoor_endpoint.h:36-40` 5 函数声明 + `:15-20` 枚举 | ✅ 真完成 |
| **2.1** | **`backdoor_endpoint.cpp` 真实实现** | ✅ | **❌ stub 文件存在，但 5 weak symbol 全 `-ENOSYS`** | `sim_hardware/src/cpptlm/backdoor_endpoint_stub.cpp`（80 行 stub）+ `sim_hardware/src/cpptlm/` 下**无 `backdoor_endpoint.cpp`** | ❌ **虚假完成** |
| **2.2** | **`bridge.cpp` kCpptlm dlopen 22 ABI** | ✅ | **❌ `bridge.cpp:65-67` kCpptlm 分支 `return -ENOSYS`** | `bridge.cpp:65-67` Show me 65-67: `if (params.backend == CpptlmBackendKind::kCpptlm) { return -ENOSYS; }` | ❌ **虚假完成** |
| **2.3** | **`host_bridge.cpp` bypass/full 自动分发** | ✅ | **❌ 无 `bypass_get_mode` 引用，分发未实施** | `host_bridge.cpp:84-92` 有 `bypass_read/write` 函数，但 grep `bypass_get_mode` 0 结果 | ❌ **虚假完成** |
| **3.1** | `gpu_hal.h` 68→71 追加 3 fn-ptr | ✅ | `gpu_hal.h` 已含 `adapter_get_info/open/close` 3 fn-ptr | L382-384 adapter #69/70/71（per driver-stack-flow-roadmap P3.4） | ✅ 真完成 |
| **3.2** | `hal_mock.cpp` 3 fn-ptr | ✅ | 已实现（mock 返回固定数据） | `plugins/gpu_driver/hal/hal_mock.cpp` grep `adapter_get_info` | ✅ 真完成 |
| **3.3** | `hal_user.cpp` 3 fn-ptr | ✅ | 已实现（基于 pci_driver + Discovery 固件表）| `plugins/gpu_driver/hal/hal_user.cpp` grep `adapter_get_info` | ✅ 真完成 |
| **3.4** | **`hal_cpptlm.cpp` 真实 backend** | ✅ | **❌ 3 op 全 -ENOSYS + TODO 标"Requires BackdoorEndpoint"** | `hal_cpptlm.cpp:44-65` 3 op body: `(void)memset(out_info, 0, sizeof(*out_info)); return -ENOSYS;` + `TODO: dlopen cpptlm_emulator.so` | ❌ **虚假完成** |
| **4.1** | `test_backdoor_endpoint_standalone.cpp` | ✅ | 文件存在（未核内容）| `tests/sim_hardware/test_backdoor_endpoint_standalone.cpp` ✅ | ⚠️ 文件存在未核 |
| **4.2** | `test_pcie_bypass_vs_full_standalone.cpp` | ✅ | 文件存在（未核内容）| `tests/sim_hardware/test_pcie_bypass_vs_full_standalone.cpp` ✅ | ⚠️ 文件存在未核 |
| **4.3** | `test_hal_adapter_info_standalone.cpp` | ✅ | 文件存在（未核内容）| `tests/test_hal_adapter_info_standalone.cpp` ✅ | ⚠️ 文件存在未核 |
| **4.4** | `tests/CMakeLists.txt` 注册新测试 | ✅ | 已注册（grep `test_backdoor_endpoint` `test_pcie_bypass_vs_full` `test_hal_adapter_info` 在 CMake）| `tests/CMakeLists.txt` grep 三测试名 | ✅ 真完成 |
| **4.5** | **ctest 全绿零回归** | ✅ | **❌ 未实际验证新测试与 hal_cpptlm 真实交互** — 164/164 ctest 基线不含新断言对真实 CppTLM binding 的覆盖 | ctest 输出仅验证 mock 通路，不含 backdoor_endpoint.cpp 真实现 → hal_cpptlm 真调用链路 | ❌ **虚假完成** |

---

## 审计结果统计

| 判定 | 项数 | 占比 | 关键影响 |
|---|---:|---:|---|
| ✅ 真完成 | 7 | 47% | 结构/类型/header/HAL mock+user — 无需 5.5.6 重做 |
| ❌ 虚假完成 | 5 | 33% | **sim_hardware/hal 真实 CppTLM binding 全部未实施** —— 5.5.6 主线起点 |
| ⚠️ 文件存在未核 | 3 | 20% | 4.1/4.2/4.3 测试文件存在但未核内容；可能测的是 stub（需 5.5.6 实施后跑通） |

**关键发现**：5 项虚假完成**全部集中在 sim_hardware/hal 的真实 CppTLM binding**（Tasks 2.1/2.2/2.3/3.4/4.5）——这正是 5.5.6 dGPU E2E 主线第一步的核心前置。

---

## 5.5.6 准确基线推导

### 从零实现项（5.5.6 必须新建/重写）

| 子任务 | 起始状态 | 5.5.6 实施内容 | 工期 |
|---|---|---|:---:|
| `backdoor_endpoint.cpp` | ❌ 无文件（只有 stub） | 创建新文件：5 函数 + PendingReq + future 跨线程 + 100ms 超时（per kcpptlm tasks 2.1 描述）| 1-2 周 |
| `bridge.cpp:65-67` kCpptlm 分支 | ❌ `return -ENOSYS` | dlopen `libcpptlm_emulator.so` + 绑定 22 ABI（19 原始 + 3 扩展：adapter_open/close/get_adapter_info）| 1-2 周 |
| `host_bridge.cpp` bypass/full 分发 | ❌ 仅有 bypass_read/write 直调 | 加 `bypass_get_mode()` 调用 + 自动分发至 `mmio_read/write` (Full) 或 `backdoor_read/write` (Bypass) | 0.5-1 周 |
| `hal_cpptlm.cpp` 真实 backend | ❌ 3 op stub | dlopen `cpptlm_emulator.so` + 调 `cpptlm_emulator_get_adapter_info/open/close` | 1 周 |

### 委托 fallback 项（hal_cpptlm 只需填 3 op + 委托）

| 组件 | 5.5.6 实施 |
|---|---|
| `hal_cpptlm_init` | 仅填 `adapter_get_info/open/close` 3 fn-ptr + **其余 68 fn-ptr 委托 `hal_user_init`/`hal_mock_init`** |
| `plugin.cpp:119` | 加 backend 选择点（env `ULE_HAL_BACKEND=cpptlm/mock/user`）+ 默认 mock |

### 已完成（5.5.6 不动）

- `gpu_adapter_info_t` 结构 / `gpu_adapter_handle_t` 类型 / `backdoor_endpoint.h` ABI / `gpu_hal.h` 71 fn-ptr / `hal_mock.cpp` + `hal_user.cpp` 3 op / `default_topology.json`

---

## 归档修正建议

**kcpptlm-backend-binding change 归档目录不撤销**（保留 commit 历史 + header + 测试文件 + tasks 5 项真完成）。

**tasks.md 勾选修正**（仅修改勾选记录，无代码变更）：
- 2.1 / 2.2 / 2.3 / 3.4 / 4.5 改为 `[ ]`（虚假完成）
- 1.1 / 1.2 / 1.3 / 3.1 / 3.2 / 3.3 / 4.1 / 4.2 / 4.3 / 4.4 保持 `[x]`（真完成或文件存在）

**归档记录增补**：在 kcpptlm change 目录加 `AUDIT.md` 引用本审计报告，说明"5 项虚假完成由 2026-09-08-kcpptlm-archive-audit 审计识别"。

---

## 5.5.6 立项依据总结

| 维度 | 原始（kcpptlm tasks 全勾的乐观估计） | 审计后（实际基线） |
|---|---|---|
| 工期 | 1-2 周（增量实现） | **4-6 周（从零实现 4 组件）** |
| 风险 | 低（接口已 ship） | **高（接口有 stub，需重写；外部 CppTLM 仓需 gate 通过）** |
| 前置 | kcpptlm 已 ship | **本审计 + CppTLM 侧 hard gate** |
| 5.5.7/5.5.8/5.5.9 依赖 | 顺畅 | **5.5.6 必须完成才能启动 5.5.7** |

**建议**：5.5.6 立项以本审计为前置基线，工期 4-6 周，5.5.7/5.5.8/5.5.9 顺序推迟相应周期。
