# PCIe EP 双仓打通实施路径（Stage 1 聚焦）

> **定位**: 本文档是 **UsrLinuxEmu ↔ CppTLM 双仓 PCIe EP 打通**（dGPU E2E 主线 #1，Stage 5.5.6 + Stage 1.1-1.4 + Stage 2.1）的**聚焦实施路径**。
> **与 pcie-bus-bridge-roadmap.md 关系**: 本文档**只覆盖 PCIe EP打通**（Stage 1 聚焦）；pcie-bus-bridge-roadmap.md 是**总 roadmap**（5.5.1-5.5.5 + 5.5.6-5.5.9 + 5.5.10+，500 行）。本文档<300 行，专注可执行。
> **状态**: Draft v0.1 (2026-09-09)
> **关联索引**: [pcie-bus-bridge-roadmap.md](pcie-bus-bridge-roadmap.md)（总 roadmap）+ [CppTLM/docs/soc_arch/architecture/18-pcie-endpoint-entry.md](https://github.com/CppTLM/docs/soc_arch/architecture/18-pcie-endpoint-entry.md)（双仓入口）

---

## §1 范围与目标

### §1.1 PCIe EP打通 = Stage 1（基础必备 5+4 步 + 性能增强 1 步）

| 类别 | 范围 | 本文档 |
|------|------|--------|
| **本文档覆盖** | CppTLM 5+4 步（5.0-6.0 周）+ UsrLinuxEmu 5.5.7/8 重启 | ✅ |
| 不在本文档 | 5.5.1-5.5.5（已 ship 归档）、5.5.10+（PF 虚拟化）、SOC 内部架构细节 | 见 pcie-bus-bridge-roadmap.md |

### §1.2 用户战略决策（2026-09-09）

> **先打通 CppTLM PCIe EP 协同流程，再考虑 CommandProcessor**。

**前置阻塞**：UsrLinuxEmu 5.5.6 接线真实但 7 个 CppTLM ABI 函数为 NO-OP/stub/死路（Oracle 审查 FAIL 3.5/10）。**不修复 CppTLM，5.5.7/8 无法真实化**。

---

## §2 实施路径（5+4 步 + 5.0-6.0 周）

### §2.1 总览

```
阶段 1 基础必备（4.0-5.0 周，跨仓 blocker）        阶段 2 性能增强（1 周）
┌───────────────┬───────────────┬─────────────────────┐
│  阶段 1.1 PCIe │  阶段 1.2      │  阶段 1.3 DMA 引擎    │
│  EP 基础      │  MSI-X 中断    │  (1.3a/1.3b/1.3c/1.3d)│
│  (0.5-1 周)   │  (0.5 周)      │  (2.5-3 周)          │
├───────────────┼───────────────┼─────────────────────┤
│  阶段 1.4 电源管理│              │  阶段 2.1 P2P+RBAR  │
│  (0.5 周)     │              │  (1 周)              │
└───────────────┴───────────────┴─────────────────────┘
```

### §2.2 5+4 步详细表

| 步 | 标题 | 工期 | 修复 CppTLM 错误 | 关键产出 |
|----|------|:---:|-----------------|---------|
| **1.1** | PCIe EP 基础 | 0.5-1 周 | #3 #5 #6 #7 | config space 真实化 + 4 data path roundtrip + race 修复 |
| **1.2** | MSI-X 中断 | 0.5 周 | #4 | intr_cb 真实触发 + trigger_irq_async 接线 |
| **1.3a** | PCIe SDMA 基础 | 1 周 | — | Ring Buffer + RPTR/WPTR + Doorbell + SG |
| **1.3b** | D2D SDMA 路径 | 0.5-1 周 | — | NoC 数据面 + 显存控制器 bypass |
| **1.3c** | dma_translate + GART/IOMMU + CP→SDMA | 0.5 周 | #2 | 地址翻译链 + PM4 DMA opcode 0x4600-0x4900 |
| **1.3d** | SDMA 完成通知 | 0.5 周 | #4 | Fence + MSI-X 接线 |
| **1.4** | 电源管理 | 0.5 周 | — | D0/D3 + ASPM |
| **2.1** | P2P + Resizable BAR | 1 周 | — | ARI 路由 + RBAR Cap |

### §2.3 关键路径（双仓同步）

```
CppTLM 5+4 步完成（5.0-6.0 周）
  ↓ ↓ ↓ 每子阶段 commit
UsrLinuxEmu 端 follow-up（同步 commit）
  ├─→ 阶段 1.1 后: profile 测试升级 (data assertion)
  ├─→ 阶段 1.3a 后: SDMA Ring Buffer 协议确认
  ├─→ 阶段 1.3c 后: dma_translate_cb 真实化（#2 修复）
  ├─→ 阶段 1.3d 后: MSI-X 中断链真实（#4 修复）
  └─→ 全 5+4 步后:
       ├─→ 5.5.7 重启 (CommandProcessor 真实化)
       ├─→ 5.5.8 重启 (kernel dispatch + DMA)
       └─→ 5.5.9 真机双轨验证
```

---

## §3 跨仓同步点

| 时机 | UsrLinuxEmu 端 | CppTLM 端 | 验证 |
|------|----------------|----------|------|
| 阶段 1.1 后 | 测试断言升级 (data assertion) | config + 4 data path + race 修复 | `ctest -R profile_real` |
| 阶段 1.2 后 | — | intr_cb 真实触发（200ms ≥1）| `test_msix_*` |
| 阶段 1.3a 后 | — | Ring + RPTR/WPTR + SG | `test_sdma_ring_rptr_wptr` |
| 阶段 1.3b 后 | D2D 不走 host_out 断言 | NoC 数据面 | `test_d2d_noc_path` |
| 阶段 1.3c 后 | dma_translate identity 断言 | translate_cb 真实化（#2）| `test_dma_translate_iommu` |
| 阶段 1.3d 后 | msix 触发后 driver ISR | Fence + MSI-X（#4）| `test_sdma_fence` |
| 阶段 1.4 后 | reset path | D0/D3 + ASPM | `test_pm_state` |
| 阶段 2.1 后 | P2P + RBAR 接入 | ARI + RBAR Cap | `test_p2p_dma` |

---

## §4 关键决策（已固化）

| ID | 决策 | 选择 |
|----|------|------|
| D.1 | -ETIMEDOUT 语义 | X（接受 + CppTLM 修复 race） |
| D.2 | adapter op 接入点 | P（TaskRunner 直调 HAL adapter_*）|
| D.3 | ctest WORKING_DIRECTORY | D（保持 opt-in 模式） |
| D.4 | 5 端口 wire-format | 冻结（5 端口索引顺序锁定）|
| D.5 | Ring Buffer 引入 | 作为 desc_in 前端（不变 wire-format）|
| D.6 | SDMA 完成通知 | MSI-X + Fence 双轨 |

详见 [pcie-bus-bridge-roadmap.md §修订记录 v0.2.3](pcie-bus-bridge-roadmap.md)。

---

## §5 验证清单

### §5.1 阶段完成（必查）

- [ ] 阶段 1.1：4 数据通路 roundtrip + config space 真实化
- [ ] 阶段 1.2：msix_update_pending 触发 intr_cb
- [ ] 阶段 1.3a：Ring Buffer + RPTR/WPTR + SG + Doorbell 绑定
- [ ] 阶段 1.3b：D2D 路径不经过 PCIe（host_out 零事务）
- [ ] 阶段 1.3c：dma_translate_cb 真实调用（identity mapping）
- [ ] 阶段 1.3d：Fence + MSI-X 接线（#4 修复）
- [ ] 阶段 1.4：D0 ↔ D3 + ASPM
- [ ] 阶段 2.1：P2P + Resizable BAR
- [ ] Oracle 终审 ≥ 9.0/10

### §5.2 跨仓集成

- [ ] CppTLM `tests/abi/test_cpptlm_emulator_abi.cc` 全 PASS
- [ ] UsrLinuxEmu `test_bridge_kcpptlm_profile_real_standalone` 5/5 + data assertion
- [ ] ctest 双向全绿 + docs-audit PASS
    - [ ] drv/ 零修改 + HAL append-only + **23 ABI** 不变（22 = 5.5.6 绑定子集）+ 5 端口 wire-format 冻结

---

## §6 风险与回退

| 风险 | 等级 | 回退 |
|------|:---:|------|
| 阶段 1.1 mmio race 修复影响 5.5.7.1 | 中 | timeout 延长 + sim_loop tick 频率提升 |
| 阶段 1.3 SDMA 重构（descriptor 直投 → Ring Buffer）| 高 | 阶段 1.3c 过渡期保留 5 端口 wire-format |
| 阶段 1.3c dma_translate_cb 触发 UsrLinuxEmu cb 错误 | 中 | cb 失败 fallback（phys = iova）|
| 阶段 1.3d 中断链修复后 msix 不稳定 | 中 | 测试 100ms 超时 + retry 1 |
| 阶段 1.4 电源管理影响 profile 加载 | 低 | profile 加载强制 D0 |

---

## §7 关联资源

### §7.1 双仓核心文档

| 文档 | 角色 |
|------|------|
| [pcie-bus-bridge-roadmap.md](pcie-bus-bridge-roadmap.md) | 总 roadmap（500 行，5.5.1-5.5.10+ 全覆盖）|
| [pcie-endpoint-architecture.md](pcie-endpoint-architecture.md) | 驱动侧架构 SSOT |
| [CppTLM/docs/soc_arch/architecture/16-pcie-endpoint-architecture.md](https://github.com/CppTLM/docs/soc_arch/architecture/16-pcie-endpoint-architecture.md) | 硬件侧架构 SSOT |
| [CppTLM/docs/soc_arch/architecture/17-sdma-engine-design.md](https://github.com/CppTLM/docs/soc_arch/architecture/17-sdma-engine-design.md) | SDMA 内部设计（11 章节）|
| [CppTLM/docs/soc_arch/architecture/18-pcie-endpoint-entry.md](https://github.com/CppTLM/docs/soc_arch/architecture/18-pcie-endpoint-entry.md) | 双仓入口 |
| [CppTLM/docs/roadmap/pcie-ep-cpptlm-collaboration-roadmap.md](https://github.com/CppTLM/docs/roadmap/pcie-ep-cpptlm-collaboration-roadmap.md) | CppTLM 5+4 步 roadmap |

### §7.2 openspec change

- CppTLM: [`2026-09-09-cpptlm-pcie-ep-foundation`](https://github.com/CppTLM/openspec/changes/2026-09-09-cpptlm-pcie-ep-foundation/)（14 ADDED Requirements）
- UsrLinuxEmu: `2026-09-09-5-5-7-cpptlm-cp-real-ification/` + `2026-09-09-5-5-8-cpptlm-kernel-dispatch-dma/`（Deferred）

### §7.3 已 ship 代码

- CppTLM `sim_hardware/src/cpptlm/`（5.5.6 P4.NEW-A/B/C/D + B.5）—— 接线真实
- UsrLinuxEmu `sim_hardware/src/cpptlm/` + `plugins/gpu_driver/hal/hal_cpptlm.cpp` —— 5.5.6 ship

---

## §8 修订记录

- **v0.1** (2026-09-09, Draft): 初版,聚焦 Stage 1（PCIe EP打通）实施路径
  - §1 范围与目标（PCIe EP打通 = Stage 1）
  - §2 实施路径（5+4 步 + 5.0-6.0 周）
  - §3 跨仓同步点（8 个时序检查清单）
  - §4 关键决策（D.1-D.6 6 条固化）
  - §5 验证清单（阶段完成 + 跨仓集成）
  - §6 风险与回退（5 条）
  - §7 关联资源（6 核心文档 + 3 change + 已 ship 代码）
- **待 v0.2**: CppTLM 阶段 1.3a 实施后追加（实际 Ring Buffer wire-format 验证）

---

**文档定位**：本文档专注**可执行的聚焦路径**，300 行内。深度设计内容（SDMA 内部协议、地址翻译链、Packet 格式）见 CppTLM SDMA 设计文档；架构全貌见 pcie-endpoint-architecture.md；总 roadmap 见 pcie-bus-bridge-roadmap.md。