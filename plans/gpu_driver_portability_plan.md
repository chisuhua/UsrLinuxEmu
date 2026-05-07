# UsrLinuxEmu GPU 驱动可移植平台 — 全局实施计划

**版本**: v1.0
**日期**: 2026-05-07
**关联文档**: PRD.md、各 ADR-xxx 文档、sync-plan.md

---

## 一、全局总览

### 1.1 已完成状态

```
ADR 完成状态：
Phase 1 ADR ████████████████████████████ 12/12 ✅ 全部就绪
Phase 2 ADR ████░░░░░░░░░░░░░░░░░░░░░░  0/4  ❌ 待讨论
Phase 3 ADR ████░░░░░░░░░░░░░░░░░░░░░░  0/4  ❌ 可推迟

PRD        ████████████████████████████ 草稿 ✅ 待确认
```

### 1.2 三阶段时间线

```
Phase 1: 核心驱动框架          Phase 2: 功能补齐              Phase 3: 移植验证
═══════════════════════════════ ═══════════════════════════════ ═══════════════════════
ADR 018 代码分离               [ADR讨论] GPU Core Emu         [ADR讨论] DMA API
ADR 019 DRM骨架                [ADR讨论] PCIe模型              [ADR讨论] 并发模型
ADR 020 libgpu_core            [ADR讨论] 中断模型              [ADR讨论] 可移植性度量
ADR 021 Puller状态机            [ADR讨论] linux_compat补齐     
ADR 023 HAL接口                                                
                               ┌─── 实施 ───┐                  ┌─── 实施 ───┐
┌─────────── 实施 ───────────┐ │ ADR-022    │                  │ ADR-026    │
│ 1.1 拆分 plugin.cpp        │ │ ADR-024    │                  │ ADR-028    │
│ 1.2 提取 libgpu_core       │ │ ADR-025    │                  │ ADR-030    │
│ 1.3 实现 HAL               │ │ ADR-027    │                  │ ADR-031    │
│ 1.4 DRM ioctl 表驱动        │ └────────────┘                  └────────────┘
│ 1.5 Puller 状态机           │                  ┌─ 最终验证 ──┐
│ 1.6 TaskRunner 联调验证      │                  │ kernel编译  │
└────────────────────────────┘                  │ 遗留清理     │
                                                └────────────┘
```

---

## 二、Phase 1：核心驱动框架

**目标**: 可移植的驱动代码骨架跑通、TaskRunner 全链路连通

### 2.1 任务列表

| 序号 | 任务 | 对应 ADR | 产出物 | 前置依赖 |
|------|------|---------|--------|---------|
| **P1.1** | **代码目录拆分** | ADR-018 | `drv/`、`sim/`、`hal/` 目录 + CMakeLists | 无 |
| **P1.2** | **BuddyAllocator 提取为纯 C** | ADR-020 | `libgpu_core/buddy.c` + 测试 | P1.1 |
| **P1.3** | **HAL 接口定义 + 用户态实现** | ADR-023 | `hal/gpu_hal.h` + `hal/hal_user.cpp` | P1.1 |
| **P1.4** | **DRM ioctl 表驱动** | ADR-019 | `drv/gpu_drm_driver.cpp` + GEM 骨架 | P1.3 |
| **P1.5** | **Hardware Puller 状态机** | ADR-021 | `sim/hardware/` + DoorbellEmu + GlobalScheduler | P1.3 |
| **P1.6** | **TaskRunner 联调验证** | sync-plan | 通过所有 S0-S4 测试 | P1.4 + P1.5 |

### 2.2 ADR 讨论节点

Phase 1 **没有阻塞性的 ADR 待讨论**。所有 Phase 1 的 ADR（018-021、023）已讨论并写入完成。

### 2.3 验证标准

```
P1.1: 拆分后编译通过，test_gpu_plugin.cpp 全部测试通过
P1.2: libgpu_core 独立编译，test_buddy.c 全部通过
P1.3: HAL 接口可在 mock 和 user 两种实现之间切换
P1.4: ioctl 通过 DRM 表驱动分发，行为与拆分支前一致
P1.5: Puller 状态机可处理 PUSHBUFFER_SUBMIT_BATCH，走完 IDLE→FETCH→DECODE→ISSUE→COMPLETE
P1.6: TaskRunner cuda_alloc → cuda_launch → cuda_wait 全链路成功
```

---

## 三、Phase 2：功能补齐

**目标**: 补齐真实 GPU 驱动应有的功能模块，增强可移植性基础

### 3.1 ADR 讨论节点（Phase 2 实施前必须完成）

进入 Phase 2 实施前，需要先讨论并写入以下 4 个 ADR。建议在 Phase 1 完成 P1.5（Puller 状态机）后、P1.6（联调验证）进行期间并行讨论。

```
┌─ Phase 1 收尾 ─┐     ┌─ ADR 讨论 ─┐     ┌─ Phase 2 实施 ─┐
│ P1.5 Puller    │────▶│ [讨论] 022 │────▶│ 2.1 GPU Core   │
│ P1.6 联调验证   │     │ [讨论] 024 │     │ 2.2 PCIe       │
└────────────────┘     │ [讨论] 025 │     │ 2.3 中断       │
                       │ [讨论] 027 │     │ 2.4 compat补齐 │
                       └────────────┘     └────────────────┘
```

#### ADR-022: GPU 计算单元仿真

| 项目 | 内容 |
|------|------|
| **讨论时机** | P1.5 完成后 |
| **核心问题** | Puller 把 kernel launch 分发到 gpu_core_emu 后，gpu_core_emu 怎么"执行"？ |
| **决策点** | 仿真 warp 调度吗？模拟线程执行吗？需要维护哪些状态？ |
| **预计时长** | 1 次讨论 + 1 次写入 |

#### ADR-024: PCIe 设备模型移植策略

| 项目 | 内容 |
|------|------|
| **讨论时机** | Phase 1 末期 |
| **核心问题** | PcieEmu 抽象基类怎么映射到内核的 `struct pci_driver`？ |
| **决策点** | BAR/MSI-X/MMIO 在用户态和内核态的对应关系 |
| **预计时长** | 1 次讨论 + 1 次写入 |

#### ADR-025: 中断处理模型迁移

| 项目 | 内容 |
|------|------|
| **讨论时机** | ADR-024 之后 |
| **核心问题** | 用户态怎么模拟中断？到内核后怎么映射到 `request_irq`？ |
| **决策点** | callback 模式 vs 条件变量；tasklet 仿真 vs workqueue 仿真 |
| **预计时长** | 1 次讨论 + 1 次写入 |

#### ADR-027: linux_compat 完成计划

| 项目 | 内容 |
|------|------|
| **讨论时机** | 可独立进行，与以上并行 |
| **核心问题** | 缺 cdev/device/sync/interrupt/pci/module，先补哪些？模拟精度？ |
| **决策点** | 优先级排序、精度要求、与内核版本绑定策略 |
| **预计时长** | 1 次讨论 + 1 次写入 |

### 3.2 实施任务

| 序号 | 任务 | 对应 ADR | 前置依赖 |
|------|------|---------|---------|
| **P2.1** | GPU Core Emu — warp/wavefront 调度仿真 | ADR-022 | ADR-022 讨论完成 |
| **P2.2** | PCIe DMA/MSI-X 仿真增强 | ADR-024 | ADR-024 讨论完成 |
| **P2.3** | 中断模型用户态/内核态双实现 | ADR-025 | P2.2 |
| **P2.4** | linux_compat 补齐（cdev、device、sync、interrupt 等） | ADR-027 | ADR-027 讨论完成 |
| **P2.5** | TTM 域间迁移实现（VRAM/GTT/CPU） | ADR-016, ADR-019 | P2.1 |
| **P2.6** | 多队列 + VA Space（GPU_IOCTL_CREATE_QUEUE 等） | ADR-017 | P2.5 |
| **P2.7** | dma-buf/PRIME 接口实现 | ADR-019 Q5 | P2.5 |

### 3.3 验证标准

```
P2.1: gpu_core_emu 可模拟 1 个 warp 的 launch→execute→complete
P2.2: PCIe BAR 读写正确，MSI-X 中断可触发
P2.3: 中断 callback 在用户态正确触发，中断结构体在 linux_compat 中定义
P2.4: linux_compat 覆盖率 ≥ 80%（按 linux_driver_compatibility_plan.md 标准）
P2.5: TTM BO 可在 VRAM 和 GTT 之间迁移，test_ttm_migration 通过
P2.6: 创建 2 个 queue，各提交独立 entry，互不干扰
P2.7: dma-buf 导出/导入接口返回 -ENOSYS 但不崩溃（正式实现 Phase 3）
```

---

## 四、Phase 3：移植验证

**目标**: 验证 `drv/` 代码可在真实 Linux 6.8 内核上编译，完成遗留清理

### 4.1 ADR 讨论节点

| ADR | 核心问题 | 讨论时机 |
|-----|---------|---------|
| **ADR-026** | DMA API（dma_map/dma_unmap）在用户态和内核态的差异如何处理 | Phase 2 末期 |
| **ADR-028** | std::thread → kthread、std::mutex → spinlock 的映射策略 | Phase 2 末期 |
| **ADR-030** | 可移植性规则 10 条的自动化检查和 CI 门禁 | Phase 2 末期 |
| **ADR-031** | 实施优先级（已基本被 PRD 覆盖，可合并到 Phase 3 计划中） | 视需要 |

### 4.2 实施任务

| 序号 | 任务 | 前置依赖 |
|------|------|---------|
| **P3.1** | Linux 6.8 内核环境搭建 + `drv/` 代码复制到内核树 | P2.6 |
| **P3.2** | `test_portability.sh` 自动检查 10 条可移植性规则 | ADR-030 讨论完成 |
| **P3.3** | 修复编译错误（针对内核 `-Werror`、`__iomem`、`__user` 标注等） | P3.1 |
| **P3.4** | DMA API 用户态模拟实现（`dma_alloc_coherent` 等） | ADR-026 讨论完成 |
| **P3.5** | 并发模型适配层（`kthread` wrapper、`spinlock` wrapper） | ADR-028 讨论完成 |
| **P3.6** | 删除遗留代码（`cuda_compat_ioctl.cpp`、`GPGPU_*` deprecated 标记） | adr-015 Phase 3 |

### 4.3 验证标准

```
P3.1: drv/ 代码可在 Linux 6.8 内核下编译（`make -C /lib/modules/... M=drv/`）
P3.2: test_portability.sh 全部 10 条规则通过
P3.3: 零编译 warning（Werror）
P3.4: DMA API 在用户态通过基本分配/释放测试
P3.5: 并发模型 wrapper 通过基本功能测试
P3.6: 遗留代码已删除，git grep "cuda_compat\|GPGPU_" 只返回 adr-015 文档
```

---

## 五、依赖关系图

```
Phase 1                    Phase 2                     Phase 3
══════════                  ══════════                  ══════════

ADR-018 代码分离 ──────┐
                       ├──▶ P1.3 HAL ──────┐
ADR-020 libgpu_core ───┘                   │
                                           ├──▶ P1.4 DRM ───┐
ADR-023 HAL接口 ───────────────────────────┘                │
                                                            ├──▶ P1.5 Puller ───┐
                                                                                │
ADR-019 DRM骨架 ───────────────────────────────────────────────────────────────┘
                                                                                │
                                                        [讨论 ADR-022] ──────┐ │
                                                        [讨论 ADR-024] ────┐ │ │
                                                        [讨论 ADR-025] ──┐ │ │ │
                                                        [讨论 ADR-027] ┐ │ │ │ │
                                                                       ▼ ▼ ▼ ▼
                                                                  ┌───────────┐
                                                                  │ Phase 2   │
                                                                  │ 实施任务    │
                                                                  └───────────┘
                                                                       │
                                                                       ▼
                                                                  ┌───────────┐
                                                                  │ Phase 3   │
                                                                  │ 移植验证    │
                                                                  └───────────┘
```

---

## 六、风险与缓解

| 风险 | 影响阶段 | 概率 | 缓解措施 |
|------|---------|------|---------|
| Phase 1 拆分 plugin.cpp 破坏现有测试 | P1.1 | 中 | 每次拆分后运行全部 ioctl 测试对比行为 |
| ADR 讨论耗时过长影响 Phase 2 实施 | Phase 2 → 2 | 高 | 每个 ADR 限 1 次讨论 + 1 次写入，超时先基于建议+推进 |
| 内核环境搭建困难 | P3.1 | 中 | 先基于 Ubuntu 20.04/22.04 LTS 官方内核 |
| `drv/` 代码包含隐性 C++ 依赖 | P3.2 | 高 | P1.1 就开始用 `test_portability.sh` 约束，不等到 P3 |
| linux_compat API 与内核实际 API 有差异 | P2.4 → P3.1 | 中 | 锁定到 Linux 6.8 版本，差异清单化 |

---

## 七、关键节点汇总

| 节点 | 内容 | 决策 |
|------|------|------|
| **N1** | PRD 最终确认 | 确定 3 个 Phase 范围、约束规则 |
| **N2** | Phase 1 全部实施完成 | 进入 Phase 2 ADR 讨论 |
| **N3** | ADR-022/024/025/027 讨论完成 | 写入 Phase 2 ADR 文档 |
| **N4** | Phase 2 全部实施完成 | 进入 Phase 3 |
| **N5** | ADR-026/028/030/031 讨论完成 | 写入 Phase 3 ADR 文档 |
| **N6** | Phase 3 全部实施完成 — drv/ 内核编译通过 | **毕业** |

---

## 八、与 sync-plan 的对应关系

| sync-plan 同步点 | 本计划的对应阶段 | 状态 |
|-----------------|----------------|------|
| S0（符号链接方向） | 已完成 | ✅ |
| S1（GET_DEVICE_INFO） | Phase 1 中覆盖 | 🔄 |
| S2（ALLOC_BO） | Phase 1 中覆盖 | 🔄 |
| S3（PUSHBUFFER） | Phase 1 P1.5-P1.6 覆盖 | 🔄 |
| S4（端到端集成） | Phase 1 P1.6 联调验证 | 🔄 |
| S5（VA Space/Queue） | Phase 2 P2.6 覆盖 | 🔄 |

---

**维护者**: UsrLinuxEmu Architecture Team

**最后更新**: 2026-05-07
