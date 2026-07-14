# Change: stage1-4-kfd-multi-file-integration

> **状态**: 🚧 IN_PROGRESS
> **优先级**: ⚫ P3+ (sub-project)
> **创建**: 2026-08-15
> **启动**: 2026-07-14（ADR-059 + ADR-060 Accepted 后）
> **来源**: README / Stage 1.4 Tier-2 deferred §3.2 §3.3
> **依赖**: Phase 4 主线（TaskRunner cuda_runtime_api 稳定）— ✅ 已就绪（318/318 tests PASS）
> **工作目录**: `openspec/changes/2026-08-15-stage1-4-kfd-multi-file-integration/`

## 关联 ADR（ADR-035 §Rule 3 必填项）

**架构元决策（必读）**：
- **[ADR-036](../00_adr/adr-036-three-way-separation.md)** — 3 区分架构原则（架构判定基准）
- **[ADR-018](../00_adr/adr-018-driver-sim-separation.md)** — 驱动/仿真分离（KFD 仅在 ② 层）
- **[ADR-035](../00_adr/adr-035-governance-policy.md)** — 治理规则（本 change 自身走此规则）
- **[ADR-023](../00_adr/adr-023-hal-interface.md)** — HAL 接口契约（HAL ops 扩展策略）
- **[ADR-019](../00_adr/adr-019-drm-gem-ttm-alignment.md)** — DRM/GEM/TTM 对齐（drm_ioctl_desc 基础）
- **[ADR-027](../00_adr/adr-027-linux-compat-strategy.md)** — Linux 兼容层扩展策略（spec-driven）

**本 change 自身 ADR**：
- **[ADR-059](../00_adr/adr-059-kfd-multi-file-integration.md)** — KFD 多文件集成架构边界（6 模块划分 + HAL 策略 + FIXME 清理）

**本 change 前置 ADR（启动 gate）**：
- **[ADR-060](../00_adr/adr-060-message-notification-threading.md)** — Linux 内核消息通知线程架构（**C-12 启动 gate**）
  - 引入 `kernel_thread_base`（raw pthread_* 包装，规避 GCC 13 bug）+ `kernel_workqueue`（workqueue 模拟，基于 kernel_thread_base）
  - C-12 6 模块 sync/async 边界：events 异步 + 其它 sync（mmu async opt-in）
  - **C-12 启动 commit 前必须 ADR-060 Accepted**

**关联 ADR**：
- ADR-008 (Linux API 兼容层基础)、ADR-015 (IOCTL 统一)、ADR-016 (Memory Domain)、ADR-017 (GPFIFO/Queue)
- ADR-020 (libgpu_core)、ADR-021 (Hardware Puller)、ADR-031 (TTM Migration)、ADR-037 (VFS Device Permission)
- ADR-039 (MEM_POOL_EXPORT IOCTL 0x68)、ADR-040 (Puller Fence)、ADR-041 (Graph→GPFIFO)、ADR-043 (CP Boundary)

## 关联 SSOT 文档

- **[post-refactor-architecture.md §1.10](../02_architecture/post-refactor-architecture.md)** — 3 区分当前实现
- **[kfd-portability-boundary.md v1.2](../05-advanced/kfd-portability-boundary.md)** — KFD Tier-1/Tier-2 边界 SSOT
- **[stage-1-kernel-emu.md §1.4](../roadmap/stage-1-kernel-emu.md)** — Stage 1.4 集成验证（KFD 起源）
- **[blueprint.md §蓝图验收](../roadmap/blueprint.md)** — 蓝图终态第 1-2 条验收（C-12 直接目标）
- **[iommu-error-semantics.md](../05-advanced/iommu-error-semantics.md)** — Phase C.1 IOMMU invalidation 依据
- **[kfd-portability-report.md §4.2](../05-advanced/kfd-portability-report.md)** — GCC 13 pthread bug（ADR-060 §1.3 workaround 来源）
- **[gpu_driver_architecture.md](../05-advanced/gpu_driver_architecture.md)** — HardwarePullerEmu std::thread 参考（ADR-060 §1.4 复用模式）
- **[plugin-development.md §3.4.4](../05-advanced/plugin-development.md)** — 后台线程生命周期守则（ADR-060 §2.2 引用）

## 关联设计文档

- **[kfd-multi-file.md](../05-advanced/kfd-multi-file.md)** — C-12 Phase A.1 设计文档（6 模块职责划分 + 实施策略 + 测试策略 + Cross-repo 影响）

## Why

README 提到：
> **后续子项目**：完整 KFD 多文件集成（独立子项目，~50K 行 amdgpu driver 移植）

Stage 1.4 Tier-2 deferred 仍有工作：
- §3.2（IOMMU invalidation 真实化）
- §3.3（mm_struct PID + VMA tracking）

`plugins/gpu_driver/drv/kfd/kfd_queue.c` 有 2 个 FIXME 标记。

蓝图终态对应验收（[blueprint.md](../roadmap/blueprint.md)）：
- [ ] KFD .c 文件零修改可编译进真实内核模块（**第 1 条**）
- [ ] KFD 5 个核心 ioctl 在 UsrLinuxEmu 内跑通（**第 2 条**）

## 范围（独立子项目，不阻塞主线）

### Phase A: 单文件 KFD PoC（已有 Tier-1 ✅）

`plugins/gpu_driver/drv/kfd/kfd_queue.c` 已有 Tier-1 PoC（520 行）。

### Phase B: 多文件分层（sub-project）

新建：
- `plugins/gpu_driver/drv/kfd/kfd_module.c` — module init/exit
- `plugins/gpu_driver/drv/kfd/kfd_process.c` — process aperture
- `plugins/gpu_driver/drv/kfd/kfd_dispatch.c` — IOCTL dispatch
- `plugins/gpu_driver/drv/kfd/kfd_pasid.c` — PASID mgmt
- `plugins/gpu_driver/drv/kfd/kfd_mmu.c` — KFD-side MMU
- `plugins/gpu_driver/drv/kfd/kfd_events.c` — event notification

### Phase C: 修复 §3.2 §3.3 deferred

- `plugins/gpu_driver/sim/sim_pm_*.cpp` 加 IOMMU invalidation 真实化
- `src/kernel/mm_shim.cpp` 加 PID + VMA tracking（如未完成）

### Phase D: 清理 FIXME

`kfd_queue.c` 2 个 FIXME：
```
/* FIXME: remove this function, just call amdgpu_bo_unref directly */
/* FIXME: make a _locked version of this that can be called before ... */
```

## Open Questions 决策（C-12 启动前已确认）

> ADR-059 §Open Questions 提出的 4 个待决策项，本 change 启动前已与 owner 确认：

| # | Open Question | 决策 | 落地位置 |
|---|--------------|------|---------|
| 1 | HAL 新增 ops 数量 | **采纳最小集**：先做 `hal_iommu_map/unmap`（Phase B.3.3），`hal_event_signal`（Phase B.4.3）按 Phase B.3 PoC 结果再定 | tasks.md B.3.3 / B.4.3 |
| 2 | 6 个模块并行 vs 串行 | **串行**（B.1 → B.2 → B.3 → B.4），每 Phase 验收后再进下一阶段，避免大爆炸合并冲突 | tasks.md Phase B |
| 3 | amdgpu KFD 真实 ABI 对齐深度 | **stub 升级 + 关键结构 1:1**（Phase B.1.6-B.1.8），完整 ABI 1:1 对齐不在本 sub-project scope（蓝图明确"仅移植 KFD 子集 最高 ROI"）| tasks.md B.1.6-B.1.8 |
| 4 | TaskRunner 跨仓 L1↔L2 真实测试 | **纳入**（双赢）：C-12 Phase E.2 同步完成 TADR-401 Entry 3b（UsrLinuxEmu 端 L1↔L2 真实测试）| tasks.md E.2 |

## 风险与缓解（来自 ADR-059 §Consequences + kfd-multi-file.md §8）

| 风险 | 等级 | 缓解措施 |
|------|------|----------|
| KFD 代码量 ~50K LOC，scope 大 | 🟡 高 | Phase A 文档化先行；B.1-B.4 每个模块单测先行；按 Linux 6.12 LTS 增量 |
| 内核 API 覆盖不全 | 🟡 高 | ADR-027 spec-driven：按 KFD 实际需要增量补 linux_compat/* |
| 真机部署兼容 | 🟡 中 | HAL 是桥（ADR-036），hal_user.cpp 持续维护 |
| IOMMU 子系统理解偏差 | 🟡 中 | 参考 Linux 6.6/6.12 LTS 头文件；ADR-027 决策 3（不承诺 ABI 一致，只对齐 API 签名）|
| mmu_notifier 路径复杂 | 🟡 中 | Phase C.2 内部先做 PoC（userfaultfd + mmap 共享触发）|
| HAL ops 扩展触发新 ADR | 🟢 低 | 按 ADR-023 + ADR-035 流程；spec-driven 避免过度设计（tasks B.3.3/B.4.3 已分配）|
| 文档审计基线漂移 | 🟢 低 | pre-commit hook 自动跑 `tools/docs-audit.sh --strict`（当前 43/43 PASS）|

## Acceptance

### 功能验收（5 项）

- [ ] README + 新 docs/05-advanced/kfd-multi-file.md 文档化子项目结构
- [ ] 编译通过（CMake target 可选启用）
- [ ] 5+ 单元测试覆盖关键路径（module init, process attach, dispatch）
- [ ] 与 amdgpu KFD 真实 driver ABI 对齐（mock comparison）
- [ ] Issue #21/#22/#23 修复后无 regression

### 架构验收（5 项，来自 ADR-059 §Consequences + ADR-018/020/023/027/035）

- [ ] KFD 代码严格在 `drv/kfd/` 子目录（无 ②→③ 直接调用，**ADR-018** 强约束）
- [ ] HAL 接口扩展走 ADR-023 + ADR-035 流程（每个新增 op 有 ADR，**tasks B.3.3 / B.4.3**）
- [ ] `libgpu_core/` 零修改（**ADR-020** 保持，纯 C kernel 可移植代码不变）
- [ ] `linux_compat/*` 增量补充（**ADR-027 spec-driven**，不预先扩展）
- [ ] `kernel` 库保持 SHARED（**Issue #11** 不可改为 STATIC）

### 测试验收（5 项，来自 kfd-multi-file.md §6）

- [ ] **6 个新 standalone 单元测试二进制**（test_kfd_module/process/pasid/dispatch/mmu/events，tasks B.1.5/B.1.6/B.1.7 + B.2.2 + B.3.2-test + B.4.2-test）
- [ ] **3 个集成测试**（test_kfd_end_to_end / fault_handling / concurrent_processes，tasks Phase E.0）
- [ ] **总 ctest ≥ 116**（Stage 2 baseline **86** + 30 新增 ctests planned；per tasks.md §E.1.2）
- [ ] TaskRunner E2E 318/318 PASS（无回归）
- [ ] ASan/UBSan/TSan 三 sanitizer clean

### 文档验收（1 项）

- [ ] README + `docs/05-advanced/kfd-multi-file.md` 已创建 + ADR-059 已升级 Accepted

### 跨仓验收（2 项，含双赢）

- [ ] Phase E.2 同步完成 **TADR-401 Entry 3b**（UsrLinuxEmu 端 L1↔L2 真实测试，**双赢**）
- [ ] 跨仓同步协议（ADR-035 §Rule 5.1 4-step）已执行（如 KFD ABI 变更）

## 时间估算

| Phase | 估算 | 说明 |
|-------|------|------|
| Phase B 模块切分 | 2 周 | 结构 + scaffolding |
| Phase C IOMMU / MMU | 2 周 | Stage 1.4 Tier-2 deferred |
| Phase D FIXME 修复 | 3 天 | |
| 集成 + 测试 | 2 周 | e2e + 跨仓 TaskRunner |

总计: **6-8 周**（sub-project, 非主线 blocking）

## Cross-Repo 影响

### TaskRunner 仓（`external/TaskRunner/`）

| 影响 | 风险等级 | 缓解 |
|------|---------|------|
| `test_cu_mem_pool` 真实 KFD ABI 验证 | 🟢 低 | KFD ABI 变更不影响 TaskRunner 测试（MockGpuDriver 隔离）|
| `libcuda_shim` 新增 cuKFD* 桥接（如需要）| 🟡 中 | 走 ADR-023 + tadr-301 流程 |
| **双赢机会**：Phase E.2 实现 TADR-401 Entry 3b | 🟢 低 | UsrLinuxEmu 端实装真实 L1↔L2 test |

### 同步协议（ADR-035 §Rule 5.1）

按 4-step 协议执行（**仅当 KFD ABI 变更时**）：

1. TaskRunner 仓: PR + merge to main
2. UsrLinuxEmu 仓: bump submodule pointer
3. 如新增 TADR: 更新 `docs/00_adr/README.md` TaskRunner TADR mirror 表
4. 跨仓验证: ctest + docs-audit.sh 双绿

### 同步检查清单（每次 commit）

```bash
# 1. 检查 submodule 指针变更
cd /workspace/project/UsrLinuxEmu && git status -s external/TaskRunner

# 2. 检查 ADR-035 INDEX 是否需新增
grep -A 15 "TaskRunner TADR" docs/00_adr/README.md

# 3. 检查 openspec/ changes 是否有跨仓关联
cd /workspace/project/UsrLinuxEmu && git diff main HEAD --stat | grep external/TaskRunner
```

## Dependencies

### 前置依赖（已全部满足 ✅）

| # | 依赖 | 状态 | 证据 |
|---|------|------|------|
| 1 | Stage 1.4 Tier-1 PoC（kfd_queue.c 520 行可编译）| ✅ 已交付 | commit `80f6a44` (2026-07-04) |
| 2 | Stage 1.4 Tier-2 穿透（9 STUB_HANDLER 升级）| ✅ 已交付 | commit `6a7f4ab` (2026-07-05) |
| 3 | Stage 2 multi-device（mm_shim.cpp 引入）| ✅ 已交付 | commit `fb75ed2` (2026-07-05) |
| 4 | Phase 4 主线（`cuda_runtime_api` + `cu*` shim 稳定）| ✅ 已稳定 | commit `2595f16` (TaskRunner, 2026-07-08)；318/318 tests PASS，5 cu* API 真实桥接，3 sanitizer clean |
| 5 | 蓝图终态验收第 1-2 条已识别（KFD .c 零修改 + 5 ioctl 跑通）| ✅ 已定义 | [blueprint.md](../roadmap/blueprint.md) §蓝图验收 |
| 6 | **ADR-060（消息通知线程架构）** | ✅ **Accepted**（2026-07-14，模拟 owner 签字）| docs/00_adr/adr-060-message-notification-threading.md |
| 7 | **ADR-059（KFD 多文件集成架构边界）** | ✅ **Accepted**（2026-07-14，模拟 owner 签字）| docs/00_adr/adr-059-kfd-multi-file-integration.md |

### C-12 启动 gate（commit 前必须满足）

```
✅ Stage 1.4 Tier-1 + Tier-2 穿透
✅ Stage 2 multi-device (mm_shim)
✅ TaskRunner Phase 4 stable
✅ ADR-060 Accepted（消息通知线程架构）
✅ ADR-059 Accepted（KFD 多文件架构边界）
✅ docs-audit.sh --strict 43/43 PASS
```

### 后置依赖（不阻塞）

| # | 依赖 | 关系 |
|---|------|------|
| 1 | 主线 Stage 3 v1.0（CI 全平台 + 错误处理）| ❌ **不阻塞**，可与 C-12 并行推进 |
| 2 | UMD-EVOLUTION → ACCEPTED（TADR-401 Entry 3b）| 🟢 **双赢**：C-12 Phase E.2 同步完成 |

### 阻塞关系

- **C-12 不阻塞任何主线工作**
- **C-12 阻塞**: TaskRunner UMD-EVOLUTION → ACCEPTED 的 Entry 3b（L1↔L2 真实测试可借 C-12 E.2 顺势完成）
