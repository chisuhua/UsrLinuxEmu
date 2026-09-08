# Proposal: kcpptlm-archive-audit — 归档勾选 vs 代码事实审计

> **状态**: 🔍 **审计完成**（2026-09-08，本 change 为审计类，产出基线文档，不修改任何 sim_hardware / plugins / tests / src 应用代码）
> **关联**: [kcpptlm-backend-binding](openspec/changes/archive/2026-09-07-kcpptlm-backend-binding-with-handle-and-adapter-info/) ✅ Archived | [Oracle 路线图分析报告]（commit `0a8a53f` 后续 commit）| [ADR-092](../00_adr/adr-092-hal-adapter-and-bypass-binding.md) 🔄 Proposed | [ADR-091](../00_adr/adr-091-pci-driver-architecture-and-four-quadrant.md) ✅ Accepted v0.2
> **审计目标**: 核实 kcpptlm-backend-binding change 归档勾选与代码事实的一致性，产出 5.5.6 dGPU E2E 主线立项的准确基线
> **审计者**: UsrLinuxEmu Architecture Team + Oracle 路线图分析（独立验证）
> **关联 commit**: 当前 HEAD `0a8a53f` | 164/164 ctest PASS

---

## Why

用户提出 dGPU E2E 优先优先级（PCIe EP → CommandProcessor → kernel dispatch + DMA），要求路线图调整。Oracle 路线图分析（报告已落档）发现：

1. **kcpptlm-backend-binding change（2026-09-07 归档）tasks.md 15 项全部 `[x]` 勾选**声称"打通 kCpptlm 真实 backend + 22 ABI 绑定 + backdoor_endpoint.cpp 真实实现"，但代码事实显示 5 项实质性实现缺口（虚假完成率 33%）。
2. **5.5.6 dGPU E2E 主线立项**的最大前置依赖是"已 ship 的真实 CppTLM binding"——但实际代码状态远未达到；必须以审计结论修正基线认知，避免工期低估 2-3 倍。
3. **归档勾选残留**是普遍性问题（之前 Change-1 pci-driver-refactor 也有 32 项子步骤未勾选），需要建立系统性审计机制。

## What

本 change 为**纯审计类**，无代码变更，产出：

1. **审计矩阵**：15 项 tasks 逐项核对（✅ 真完成 / ❌ 虚假完成 / ⚠️ 待核），含文件:行号证据
2. **5.5.6 准确基线**：从审计结果推导的 5.5.6 变更范围（从零实现项 + 委托 fallback 项）
3. **归档修正建议**：tasks 勾选与代码事实的同步方案（不撤销归档，仅修正勾选记录）

## Audit Findings（核心结论）

### 1. 审计矩阵摘要

| 类别 | 项数 | 占比 |
|---|---:|---:|
| ✅ 真完成 | 7 | 47% |
| ❌ 虚假完成 | 5 | 33% |
| ⚠️ 未核内容（测试文件存在但未核） | 3 | 20% |

**最大风险**：5 项虚假完成集中在 sim_hardware / hal 的**真实 CppTLM binding**（Tasks 2.1/2.2/2.3/3.4/4.5）——这正是 5.5.6 dGPU E2E 主线第一步的核心前置。

### 2. 5.5.6 准确基线

5.5.6 立项应基于以下"从零实现"清单（**不是增量实现**）：

| 子任务 | 当前状态 | 5.5.6 需做 |
|---|---|---|
| `backdoor_endpoint.cpp` 真实实现 | ❌ stub 文件 9-07 修改（5 weak -ENOSYS） | **从零实现**（PendingReq + future 跨线程，per kcpptlm tasks 2.1） |
| `bridge.cpp` kCpptlm 分支 | ❌ L65-67 `return -ENOSYS` | **从零实现**（dlopen 22 ABI + 19 原始 + 3 扩展） |
| `host_bridge.cpp` bypass/full 分发 | ❌ 未见 `bypass_get_mode` 引用 | **从零实现**（per kcpptlm tasks 2.3） |
| `hal_cpptlm.cpp` 真实 backend | ❌ 3 op 全 -ENOSYS + TODO | **从零实现**（dlopen cpptlm_emulator.so + 调 adapter ABI） |
| `plugin.cpp:119` backend 选择 | ❌ 硬编码 `hal_user_init` | **新增 backend 选择点**（env 切换 + 默认 mock） |
| `default_topology.json` 修复 | ✅ 已修复（commit `d8532fa`，Wave 2） | 已 done |

**附录：组合而非替代策略（Oracle 风险 3 缓解）**：
- `hal_cpptlm_init` 仅填 3 个 adapter op + **其余 68 fn-ptr 委托 hal_user/hal_mock**
- 这避免 hal_cpptlm 切换后大面积 -ENOSYS，且符合 ADR-023 append-only

### 3. 5.5.6 工期重估

| 原始假设（基于 tasks 全勾） | 审计后实际 |
|---|---|
| "5.5.6 = 增量实现"（1-2 周） | **5.5.6 = 从零实现 4 个组件**（4-6 周） |
| hal_cpptlm.cpp 已 ship | hal_cpptlm.cpp 是 stub + 3 TODO，需从零写 |

**建议**：5.5.6 立项以审计后基线为依据，工期预算 4-6 周（vs 原始 1-2 周假设）。

## Why Now

1. **用户优先级已明确**：dGPU E2E 优先 → 5.5.6 是关键路径
2. **kcpptlm 审计缺口不能拖**：5.5.6 立项若按"增量"排期会导致 5.5.7/5.5.8/5.5.9 全部推迟
3. **CppTLM sibling 仓已构建**（`../CppTLM/build/lib/libcpptlm_emulator.so`）→ 外部仓 hard gate 部分已具备
4. **164/164 ctest PASS 基线稳定** → 可在此基础上做新组件开发

## Impact

| 受影响 | 影响 |
|---|---|
| `sim_hardware/src/cpptlm/backdoor_endpoint.cpp` | 5.5.6 创建（从 stub 替换） |
| `sim_hardware/src/cpptlm/bridge.cpp:65-67` | 5.5.6 真实 dlopen 实现 |
| `sim_hardware/src/pcie/host_bridge.cpp` | 5.5.6 bypass/full 分发 |
| `plugins/gpu_driver/hal/hal_cpptlm.cpp` | 5.5.6 真实 backend 实现 |
| `plugins/gpu_driver/plugin.cpp:119` | 5.5.6 backend 选择点 |
| 归档 change `kcpptlm-backend-binding` | 仅修正 tasks 勾选记录（不撤销归档） |

## Out of Scope

- 5.5.7/5.5.8/5.5.9（CommandProcessor / kernel dispatch / DMA / 真机验证）——后续 change 立项
- 5.5.3-5.5.5（PCIe Tier 3-8 细化）——后台轨道 P1，不在本审计范围
- CppTLM sibling 仓改动（`../CppTLM`）——外部仓，本仓无写权限

## Cross-Reference

- [Oracle 路线图分析报告]（commit `0a8a53f` 后续 commit 输出，标记 F-b 关键发现）
- [kcpptlm-backend-binding/tasks.md](openspec/changes/archive/2026-09-07-kcpptlm-backend-binding-with-handle-and-adapter-info/tasks.md)（归档原文，15 项勾选）
- [driver-stack-flow-roadmap.md P3.4](../docs/roadmap/driver-stack-flow-roadmap.md)（hal_cpptlm 头注释修正）
- [pcie-bus-bridge-roadmap.md §修订记录 v0.2.2](../docs/roadmap/pcie-bus-bridge-roadmap.md)（5.5.6-5.5.9 主线定义 — 同步更新）

---

**审计者签名**: UsrLinuxEmu Architecture Team
**审计日期**: 2026-09-08
**审计结果**: 5 项虚假完成（33%）→ 5.5.6 工期重估 4-6 周（从零实现 4 组件）
