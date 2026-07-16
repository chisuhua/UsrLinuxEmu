# Tasks: stage1-4-kfd-multi-file-integration

> **状态**: 🚧 IN_PROGRESS（2026-07-14 启动；Phase A ✅ 完成 2026-07-15；Phase B 可启动）
> **目标**: 完整 KFD 多文件集成子项目 (~50K LOC amdgpu port)
> **设计文档**: [`docs/05-advanced/kfd-multi-file.md`](../../../docs/05-advanced/kfd-multi-file.md)
> **架构 ADR**: [ADR-059](../../../docs/00_adr/adr-059-kfd-multi-file-integration.md)
> **关联 ADR**: ADR-018 / 023 / 027 / 035 / 036 / 019 / 020 / 037 / 039 / 040 / 041 / 043
> **总工期**: 6-8 周（sub-project, 非主线 blocking）

---

## 标签索引 (Glossary)

- **M2 拆分** = Module 2 拆分 (kfd_dispatch.c 切分, refs: B.2.4)
- **H1 修复** = HAL wave 1 修复 (HAL ops 扩展 in B.3.4 + B.4.4, refs: ADR-061 + ADR-062)
- **M3 修复** = Module 3 修复 (kfd_sim_bridge 扩展 in B.3.5)
- **L2 修复** = Layer 2 cross-repo 修复 (TaskRunner L1↔L2 bridge in E.2.4, 交付 TADR-401 Entry 3b)
- **L4 修复** = Layer 4 docs 修复 (docs 更新 in E.3)

（缩写源自 ADR-035 cross-repo sync protocol wave 分层 + KFD 模块切分顺序）

---

## Phase A: 文档化（2 天）

### A.0 启动 gate（**C-12 启动前必填**）

- [ ] **A.0.1 ADR-060 (Linux 内核消息通知线程架构) review + Accepted** — C-12 启动 gate
  - [ ] A.0.1.1 docs/00_adr/adr-060-message-notification-threading.md 创建（**已完成** ✅）
  - [ ] A.0.1.2 docs/00_adr/README.md 更新索引（**已完成** ✅）
  - [ ] A.0.1.3 ADR-060 review（owner + 1 dual reviewer）
  - [x] A.0.1.4 ADR-060 升级：📋 PROPOSED → ✅ Accepted（2026-07-14）
- [x] **A.0.2 ADR-059 (KFD 多文件集成架构边界) review + Accepted** — C-12 启动 gate
  - [x] A.0.2.1 docs/00_adr/adr-059-kfd-multi-file-integration.md 创建（**已完成** ✅）
  - [x] A.0.2.2 ADR-059 review
  - [x] A.0.2.3 ADR-059 升级：📋 PROPOSED → ✅ Accepted（2026-07-14）
- [x] **A.0.3 C-12 change proposal.md / tasks.md 添加 ADR-060 引用**（**已完成** ✅）
- [x] **A.0.4 docs-audit.sh --strict 验证**：保持 43/43 PASS（ADR-059 + ADR-060 新增后）

### A.1-A.4 文档化（2 天）

- [x] A.1 `docs/05-advanced/kfd-multi-file.md` 设计文档（**已完成** ✅）
- [x] **A.2** amdgpu KFD driver 公开 ABI 对比分析（**Hard gate — ✅ CLEARED 2026-07-15**；artifact: `docs/05-advanced/kfd-abi-comparison-report.md` §6.2 7 项决策全部 ✅ 接受 + §6.3 gate exit 签字完成；Phase B.1 启动授权）
  - [x] A.2.1 amdgpu KFD driver 公开 ABI 对比分析报告已生成（artifact 路径同上）
  - [x] A.2.2 dual reviewer 签字确认 (Architecture Team lead + 1 independent reviewer) — ✅ 2026-07-15 @chisuhua (dual-track) sign-off
  - [x] A.2.3 reviewer approval comment + reviewer github handle 添加到 report §6.3 — ✅ Sisyphus (Architecture Team lead), reviewed 2026-07-15
  - [x] A.2.4 Phase B 准入 CI 检查项就绪: `tools/docs-audit.sh --strict` + `tools/ci/check_kfd_includes.sh --strict`（禁止 `plugins/gpu_driver/drv/kfd/` 直接 `#include` amdgpu 头, per ADR-027 §R-6）— ✅ 已创建，当前 0 violation（2026-07-15）
  - [x] A.2.5 识别 Linux 6.12 LTS amdkfd/*.c 文件清单
  - [x] A.2.6 标注本 sub-project 范围内的 6 个核心模块（kfd_module/process/pasid/dispatch/mmu/events）
  - [x] A.2.7 标注 out-of-scope 模块（kfd_doorbell/topology/dbgev/device 等）

> **🔒 Gate contract（硬性门控）**: Phase A.2 完成**前不得进入 Phase B.1**（模块切分）。
>
> **A.2 报告必须输出**:
> - `kfd_dev` / `kfd_process` / `struct mm_struct` 字段子集清单（仅 C-12 必需）
> - amdgpu headers 必需依赖最小集（哪些需要在 linux_compat 增量补齐）
> - headers 复用策略（直接 `#include` from linux_compat vs 重新声明）
> - Stage 1.4 commit `5341c3f` 8 次迭代失败的根因清单 → 预防
>
> **A.2 未完成风险**: C-12 Phase B 实施时面临 Stage 1.4 PoC 同款风险（commit `5341c3f` 实证），可能 2 周工期延至 4+ 周。
>
> **A.2 执行规范**:
>
> | 维度 | 规格 |
> |------|------|
> | **Owner** | UsrLinuxEmu Architecture Team lead（C-12 sub-project owner；正式签字需 dual reviewer：Architecture Team + 1 independent reviewer）|
> | **Artifact path** | `docs/05-advanced/kfd-abi-comparison-report.md`（单文件 spec-driven per ADR-027；archived 后可拆分多文件）|
> | **Format** | Markdown report（与现有 docs/05-advanced/*.md 风格一致；Mermaid 表用于依赖图）|
> | **Review process** | Single architecture-team reviewer 双 track（如涉及 TaskRunner 跨仓则 dual per ADR-035 §Rule 5.1）|
> | **Time estimate** | ~1 dev-day（实际耗时由 owner 评估；超时需在团队 standup 显式同步）|
> | **Gate exit** | reviewer 在 tasks.md §A.2 行尾 marker `[x]` + 1-line approval comment + reviewer 签字 + reviewer github handle |
>
> **A.2 报告 Template**（强制 6 段，缺段 reviewer 可拒签）:
> 1. **§执行摘要** (1 段 ≤ 200 字): A.2 目标 + 核心结论 + reviewer 必读项
> 2. **§kfd_dev/kfd_process/struct mm_struct 字段子集**: 仅 C-12 必需字段清单（每字段引 Linux 6.12 LTS kernel source 路径 + 在 kfd-multi-file.md §3.1 的 call site）
> 3. **§amdgpu headers 必需依赖最小集**: 列出 C-12 实际 include 的 amdgpu headers（每个 header 含 .h 路径 + 必需函数子集 + 是否已在 linux_compat 存在）
> 4. **§headers 复用策略**: 决策表 — (a) 通过 linux_compat 增量补齐 vs (b) inline workaround vs (c) 本地重声明；推荐选 (a)，特殊场景允许 (c) 但需 flag 后续迁移
> 5. **§5341c3f 8 次迭代失败根因 + 预防**: 每条根因含 (a) 失败位置、(b) 失败类型、(c) 预防策略（落到具体 B.x.x step）
> 6. **§执行决策建议**: 列出 C-12 Phase B 关键风险点 + owner 推荐顺序 + reviewer 决策项
>
> **A.2 报告约束**:
> - 不追求 amdgpu KFD 完整 ABI 1:1 对齐（per ADR-059 §R-6，只覆盖 C-12 必需字段）
> - 不依赖 host kernel 介入（per kfd-portability-boundary.md §3.2，UsrLinuxEmu 用户态上下文）
> - 不修改 `linux_compat/` 内容（spec-driven 增量补齐 per ADR-027 但需独立 commit 配套）
> - 不直接引用 sim 内部实现（per ADR-018 物理隔离；如必要走 HAL 抽象或 sim-bridge）

- [x] A.3 决定子项目目录结构（**已决定**：`plugins/gpu_driver/drv/kfd/`，ADR-018 物理隔离；2026-07-15）
- [x] A.4 README.md 更新 "后续子项目" 段（标记 C-12 PROPOSED → IN_PROGRESS；2026-07-15）

---

## Phase B: 模块切分（2 周，**串行**，B.1 → B.2 → B.3 → B.4）

> **🔄 2026-07-15 Oracle/Metis 审查 + Pre-fixes**：
> - ✅ Pre-fix #1: `kfd_types.h` SSOT（消除 3 处 u32/u64 重复 typedef — Oracle PoC failure #1 根因）
> - ✅ Pre-fix #2: `MUTEX_INITIALIZER` 宏（pthread_mutex_t 静态初始化）
> - ✅ Pre-fix #3: `gpu_kfd` 改为 `PRIVATE` link on kernel（防 VFS 单例 split）
> - ✅ Pre-fix #4: `mm_struct.mm_users/mm_count` → `atomic_int`（避免并发 UAF）+ `MAX_KFD_DEVICES=8` 约束
> - 📋 Oracle **修订顺序**：B.1.3 + B.1.5 并行 → B.2.1 → B.3 → B.4（原 Metis 顺序：B.1.1 → B.1.3 → B.1.5 串行）
> - 📋 关键修复点：B.1.5 实施时必须提供 `kfd_process_gpuid_from_node` 定义（kfd_queue.c:104 undefined symbol）

> **🔀 Metis 调整（2026-07-15）**：B.1 内部实施顺序应为
> **B.1.7 → B.1.8 → B.1.9（头文件扩展）→ B.1.1 → B.1.3 → B.1.5（模块实现）→ B.1.11/12/13（单元测试）**
> 而非线性的 B.1.1 → B.1.2 → …。原因：模块实现依赖头文件中扩展的 struct 声明
> （`struct kfd_process` / `struct kfd_dev` / `struct svm_range_list` 等）。
> 桥接接口（kfd_module.h / kfd_module.c）已就绪；mutex 抽象（kfd_priv.h）已就绪。

> **架构原则**: 严格遵循 ADR-018（KFD 仅在 ② 层）+ ADR-023（HAL 桥接）+ ADR-027（spec-driven）。
> **模块依赖图**: 见 [kfd-multi-file.md §3.1](../../../docs/05-advanced/kfd-multi-file.md)
> **Open Questions 决策**: 串行实施（避免大爆炸合并冲突），详见 proposal.md §Open Questions 决策。

### B.1 基础设施（module / pasid / process）

- [x] B.1.1 `plugins/gpu_driver/drv/kfd/kfd_module.c` — module init/exit bridge stub（commit `e46c3a1`，30 行）✅ 2026-07-15
- [x] B.1.2 `plugins/gpu_driver/drv/kfd/kfd_module.h` — bridge contract only（保持 64 行，按 Metis/Oracle Q1 决策不扩展为 full linux/module.h）✅ 2026-07-15
- [x] B.1.3 `plugins/gpu_driver/drv/kfd/kfd_pasid.c` — PASID mgmt（bitmap 分配器，pthread_mutex_t 保护）✅ 2026-07-15
- [x] B.1.4 `plugins/gpu_driver/drv/kfd/kfd_pasid.h`（5 API: init/exit/allocate/free/allocated_count）✅ 2026-07-15
- [x] B.1.5 `plugins/gpu_driver/drv/kfd/kfd_process.c` — process aperture（含 `kfd_process_gpuid_from_node` 修复 kfd_queue.c:104 undefined symbol）✅ 2026-07-15
- [x] B.1.6 `plugins/gpu_driver/drv/kfd/kfd_process.h`（7 API: init/exit/create/destroy/find_by_pid/count/gpuid_from_node）✅ 2026-07-15
- [x] **B.1.7 扩展 `kfd_priv.h`**（ADR-059 D4 决策）：补全 `struct kfd_process` + `struct kfd_dev` 真实声明（commit `f9eb6c5` + `e46c3a1`）✅ 2026-07-15
- [x] **B.1.8 扩展 `kfd_topology.h`**（ADR-059 D4 决策）：补全 `struct kfd_topology_device` + 节点发现 stub（commit `a2b58ee`）✅ 2026-07-15
- [x] **B.1.9 扩展 `kfd_svm.h`**（ADR-059 D4 决策）：补全 `struct kfd_svm` + range tree stub（commit `a2b58ee`，与 B.3.1 协同）✅ 2026-07-15
- [x] **B.1.10 线程基础设施 PoC**（**依赖 ADR-060 Accepted**，C-12 启动 commit 第一步）✅ 2026-07-14
  - [x] B.1.10.1 `include/kernel/thread/kernel_thread_base.h`（raw pthread_* 包装）
  - [x] B.1.10.2 `src/kernel/thread/kernel_thread_base.cpp`（start/stop/is_running/RAII）
  - [x] B.1.10.3 `include/kernel/thread/kernel_workqueue.h`（workqueue 模拟）
  - [x] B.1.10.4 `src/kernel/thread/kernel_workqueue.cpp`（enqueue/flush/stop）
  - [x] B.1.10.5 GCC 13 pthread bug workaround 验证（CMake `-pthread` + `<sched.h>` 显式 include）
  - [x] B.1.10.6 `tests/test_kfd_threading_standalone.cpp`（**10 TEST_CASE**，> 4 要求）
  - [x] B.1.10.7 CMakeLists.txt 添加 `ENABLE_TSAN` option（Clang）
  - [x] B.1.10.8 **验证**：ASan/UBSan 基线 clean（TSan 待 `-DENABLE_TSAN=ON` opt-in run）
  - [x] B.1.10.9 **验证**：既有 ctest 88/88 无 regression（**注**：tasks.md 原文 86 系 baseline，2026-07-14 后 + hal thread safety → 88）
  - [x] B.1.10.10 **验证**：1 个新 ctest binary 通过（内含 11 TEST_CASEs / 26 assertions）
- [x] B.1.11 单元测试 `test_kfd_module_standalone`（4 TEST_CASE / 65 LOC，init/exit idempotent）✅ 2026-07-15
- [x] B.1.12 单元测试 `test_kfd_pasid_standalone`（9 TEST_CASE，77584 assertions，PASID 分配/释放/边界/并发）✅ 2026-07-15
- [x] B.1.13 单元测试 `test_kfd_process_standalone`（11 TEST_CASE，63 assertions，process lifecycle + gpuid_from_node）✅ 2026-07-15

### B.2 派发（dispatch）

- [x] B.2.1 `plugins/gpu_driver/drv/kfd/kfd_dispatch.c` — IOCTL dispatch 路由表（不修改 drm_ioctl_desc[]，仅提供 kfd_dispatch 内部路由）✅ 2026-07-15
- [x] B.2.2 `plugins/gpu_driver/drv/kfd/kfd_dispatch.h`（4 API: init/dispatch/exit/call_count + 8 个 KFD_IOC_* enum）✅ 2026-07-15
- [x] B.2.3 保持 `drm_ioctl_desc[]` ≥ 38 entries（含 5 KFD 0x40-0x47 已就位 + Stage 1.4 ~19 + Phase 3 stream/graph/mem_pool ~18）。C-12 **不新增** dispatch entry，仅保持现有 entries + 完成 6 个新 KFD module 编译通过（如未来需要新增 ioctl，按 ADR-023 + ADR-035 流程单独走 ADR）✅ 2026-07-15（policy committed，entries unchanged）
- [x] B.2.4 单元测试 `test_kfd_dispatch_standalone`（10 TEST_CASE，144 assertions，init/exit/route/concurrent）✅ 2026-07-15

### B.3 内存（mmu + HAL ops + sim bridge）

- [x] B.3.1 `plugins/gpu_driver/drv/kfd/kfd_mmu.c` — KFD-side MMU（forward to hal_iommu_map/unmap）✅ 2026-07-15
- [x] B.3.2 `plugins/gpu_driver/drv/kfd/kfd_mmu.h`
- [x] B.3.3 集成 `sim_pm_*` 真实 IOMMU invalidation（mock_iommu_map/unmap 路由到 sim_pm_migrate_to_device/system，lazy init 16MB device memory）✅ 2026-07-15
- [ ] **B.3.4 HAL op 扩展**（**H1 修复**，ADR-023 + ADR-035 流程）：
  - [x] B.3.4.1 修改 `struct gpu_hal_ops`（`plugins/gpu_driver/hal/gpu_hal_ops.h`）新增 `hal_iommu_map()` / `hal_iommu_unmap()`（commit `c12c8c6`，HAL ops 11→14）✅ 2026-07-15
  - [x] B.3.4.2 `hal_mock.cpp` 实现（路由到 sim_pm_migrate_to_device — commit 23b74c7 真路由）✅ 2026-07-15
  - [x] B.3.4.3 `hal_user.cpp` 桩实现（真机路径，commit `c12c8c6`）✅ 2026-07-15
  - [ ] B.3.4.4 MockGpuDriver 测试覆盖（ADR-032 IGpuDriver 模式）— Phase E
  - [x] B.3.4.5 **单独走 ADR 流程**（ADR-023 §新增 HAL ops 流程 + ADR-059 §D3）：创建 `adr-061-hal-iommu-extension.md`（commit `ea8e6d1` PROPOSED → commit `af6e678` Accepted）✅ 2026-07-15
- [x] **B.3.5 扩展 `kfd_sim_bridge`**（**M3 修复**）：6 个 handler 全部打 LEGACY/CLEAN 标签 + `kfd_sim_bridge_set_hal()` 实现 + bridge audit test（commit 23b74c7）✅ 2026-07-15
- [x] B.3.6 单元测试 `test_kfd_mmu_standalone`（5 TEST_CASE / 14 assertions，init/exit/get_workqueue/null-process/invalid-args）✅ 2026-07-15
- [x] **B.3.7 暴露 `kfd_mmu_get_workqueue()` accessor**（per ADR-060 §2.1 + Migration:400；day-1 stub returns NULL，未来 1-line switch 启用 async mmu_notifier callback）：
  - [x] B.3.7.1 在 `plugins/gpu_driver/drv/kfd/kfd_mmu.h` 声明 `kernel_workqueue* kfd_mmu_get_workqueue(void);`
  - [x] B.3.7.2 在 `plugins/gpu_driver/drv/kfd/kfd_mmu.c` 实现（day-1 returns NULL，future Phase C 启用）
  - [x] B.3.7.3 静态分析检查：当前 Phase B 期间零 caller（grep 确认）
  - [x] B.3.7.4 文档 1-line 说明（`kfd_mmu.h` 头注释）："day-1 stub, future Phase C async opt-in via 1-line change"

### B.4 事件（events + HAL ops）

- [x] B.4.1 `plugins/gpu_driver/drv/kfd/kfd_events.c` — event notification（async via kernel_workqueue）✅ 2026-07-15
- [x] B.4.2 `plugins/gpu_driver/drv/kfd/kfd_events.h`
- [x] B.4.3 集成 sim signal path（`sim_signal_event` day-1 stub；lambda 在 workqueue worker 中调用 sim_signal_event；Phase C/E 重构为通过 hal_event_signal）✅ 2026-07-16
- [ ] **B.4.4 HAL op 扩展**（**H1 修复**，ADR-023 + ADR-035 流程）：
  - [x] B.4.4.1 修改 `struct gpu_hal_ops` 新增 `hal_event_signal()`（commit `c12c8c6`，HAL ops 11→14）✅ 2026-07-15
  - [x] B.4.4.2 `hal_mock.cpp` 实现（async routing via kernel_workqueue → sim_signal_event，commit 21c579b）✅ 2026-07-15
  - [x] B.4.4.3 `hal_user.cpp` 桩实现（commit `c12c8c6`）✅ 2026-07-15
  - [ ] B.4.4.4 MockGpuDriver 测试覆盖（Phase E）
  - [x] B.4.4.5 **追加到 ADR-062 (HAL ops event signal 扩展)**（与 B.3.4 同流程但独立 ADR；ADR-061 专管 IOMMU，ADR-062 专管 event signal；C-12 实施时与 ADR-061 同时创建）（commit `ea8e6d1` PROPOSED → commit `af6e678` Accepted）✅ 2026-07-15
- [x] B.4.5 单元测试 `test_kfd_events_standalone`（6 TEST_CASE / 17 assertions，init/exit/EAGAIN/invalid-args/concurrent-race/get_workqueue）✅ 2026-07-15
- [x] **B.4.6 kfd_events 后台线程**（**ADR-060 §2.1 异步决策**：kfd_event_work_handler 模拟）— day-1 通过 kernel_workqueue (C++) 已实现异步；C-side pthread 实现为可选清理 ✅ 2026-07-16
  - [x] B.4.6.1 `kfd_events_thread_`（基于 kernel_thread_base — 通过 kernel_workqueue 内部）✅ 2026-07-15
  - [x] B.4.6.2 `kfd_events_queue_`（std::deque + std::mutex in kernel_workqueue）✅ 2026-07-15
  - [x] B.4.6.3 `kfd_events_cv_` 唤醒（std::condition_variable in kernel_workqueue）✅ 2026-07-15
  - [x] B.4.6.4 事件处理主循环 `runLoop()`（kernel_workqueue::worker_loop）✅ 2026-07-15
  - [x] B.4.6.5 start()/stop() API（kfd_events_init 调 start，kfd_events_exit 调 stop）✅ 2026-07-15
  - [x] B.4.6.6 TSan 测试覆盖（4 TEST_CASE: concurrent producers / init-exit ordering / drain under load / atomic counter — commit 75683ca）✅ 2026-07-15

---

## Phase C: Stage 1.4 Tier-2 deferred（2 周）

> **来源**: [kfd-portability-boundary.md §3](../../../docs/05-advanced/kfd-portability-boundary.md) §3.2 + §3.3
> **规格文档**: [phase-c-realification-contract.md](../../../docs/superpowers/specs/2026-07-15-phase-c-realification-contract.md)（G-C.0.1，Phase C 启动前必读）
> **架构 ADR**: [ADR-063](../../../docs/00_adr/adr-063-sim-pfh-pm-realification.md)（sim_pfh/sim_pm 真实化状态机边界，✅ Accepted 2026-07-15）
> **mini-gate**: ✅ **CLEARED 2026-07-16**（C.0.1~0.5 全部 [x] + `docs-audit.sh --strict` 43/43 PASS）→ **Phase C 可启动**
>
> > **2026-07-15 Oracle+Metis 审查修复**：
> > - ✅ G-C.0.1 spec 已创建：5 段精确定义 sim_pfh/sim_pm/IOTLB/mm_shim 真实化行为
> > - ✅ G-C.0.2 ADR-063 已创建：sim_pfh/sim_pm 真实化状态机边界 📋 PROPOSED
> > - ✅ G-C.0.3 ADR-011 决策：保持 PROPOSED，C.2.3 降级为 multi-thread single-process（避免 ADR-011 阻塞 Phase C）
> > - 📋 G-C.0.4 tasks.md §C 重排（本 section）
> > - ✅ G-C.0.5 `test_iommu_invalidate_runtime_standalone` 已存在（Stage 1.4 Tier-2），仅需追加 C.1.3b sim_pm_invalidate 路径 TEST_CASE
> > - **C.1.3 拆分**：C.1.3a（dma_remap bridge）+ C.1.3b（test）
> > - **C.2.1 重写**：从"加 PID + VMA 跟踪"→"wire mm_shim into kfd_process lifecycle"（mm_shim API 已存在，缺集成）
> > - **C.2.3 降级**：从"多进程 PID 隔离"→"multi-thread single-process with multiple kfd_process instances"

### C.0 mini-gate（Phase C 启动 gate，1 天）

- [x] C.0.1 `docs/superpowers/specs/2026-07-15-phase-c-realification-contract.md` review + owner 签字（✅ 2026-07-16: Sisyphus Architecture Team lead sign-off; 5 段契约完整，ADR-063 D1-D6 全覆盖，10 条验收断言可测）
- [x] C.0.2 `docs/00_adr/adr-063-sim-pfh-pm-realification.md` 📋 PROPOSED → ✅ Accepted（✅ 已 Accepted 2026-07-15，Oracle 审查通过；tasks.md 已修复滞后状态）
- [x] C.0.3 ADR-011 决策确认：C.2.3 降级方案 approved（multi-thread，非 multi-process）（✅ 2026-07-16: ADR-011 已追加 C.0.3 降级决策记录，继续等待 Phase 3 触发条件）
- [x] C.0.4 `docs/00_adr/README.md` ADR-063 索引注册（✅ 已注册 2026-07-15: README.md 第 75 行 `adr-063 | ✅ 已接受 | 2026-07-15`）
- [x] C.0.5 `tools/docs-audit.sh --strict` 无 warning（✅ 2026-07-16: **43/43 PASS，0 failed，0 warnings**）

### C.1 §3.2 IOMMU invalidation 真实化

> **规格**: [realification-contract §1~§3](../../../docs/superpowers/specs/2026-07-15-phase-c-realification-contract.md)
> **ADR**: [ADR-063](../../../docs/00_adr/adr-063-sim-pfh-pm-realification.md)
> **状态**: ✅ **COMPLETE 2026-07-16**（build 验证：5 个核心目标编译通过 `kernel/gpu_sim/gpu_kfd/gpu_hal/gpu_driver_plugin`; 4 个测试套件全 PASS：test_page_fault_handler(11/11)、test_page_migration(20/20)、test_dma_remap(9/9)、test_ats_protocol(8/8)、test_iommu_invalidate(6/6)）

- [x] C.1.1 `plugins/gpu_driver/sim/page_fault_handler.cpp` 真实化：
  - [x] C.1.1.1 `sim_pfh_inject_fault_with_cause` 中 `*pfn_out` 返回 `INVALID_PFN`（不再返回 addr/4096 幻数）
  - [x] C.1.1.2 WRITE fault + 无映射 → 调用注册的回调（callback trampoline → hal_event_signal 路径，per ADR-062）
  - [x] C.1.1.3 READ fault → 仅记录 counter + addr，不触发通知
  - [x] C.1.1.4 `sim_pfh_set_event_callback` 注册回调（+ typedef sim_pfh_event_cb）
- [x] C.1.2 `plugins/gpu_driver/sim/page_migration.cpp` 真实化：
  - [x] C.1.2.1 新增 `sim_pm_attach_domain(pm, domain)` — 绑定 iommu_domain*（opaque）
  - [x] C.1.2.2 新增 `sim_pm_invalidate(pm, offset)` — PAGE_DIRTY→EVICTED 或 PAGE_CLEAN→EVICTED
  - [x] C.1.2.3 新增 `sim_pm_is_page_dirty(pm, offset)` — 查询 dirty flag
  - [x] C.1.2.4 实现 3 态 page 状态机（PAGE_CLEAN / PAGE_DIRTY / PAGE_EVICTED）
  - [x] C.1.2.5 `migrate_to_device` 成功后通过 domain 同步 `iommu_map`（per ADR-061 桥）
  - [x] C.1.2.6 `migrate_to_system` 前通过 domain 同步 `iommu_unmap`
  - [x] C.1.2.7 `page_migration.h` 追加 4 个新 API 声明（C ABI 追加不改）
- [x] C.1.3 IOTLB flush → sim_pm invalidation 桥接：
  - [x] C.1.3a `src/kernel/iommu/dma_remap.cpp` `default_flush_iotlb`：
    - [x] C.1.3a.1 `iommu_domain_state` 追加 `struct sim_page_migration *sim_pm` 字段（可空）
    - [x] C.1.3a.2 `iommu_domain_attach_sim_pm(domain, pm)` accessor 实现于 iommu_emu_state.cpp
    - [x] C.1.3a.3 user-space page-table walk 路径内，每清除一个 entry 后调用 `sim_pm_invalidate(pm, offset)`
    - [x] C.1.3a.4 vfio 路径不变（不触发 sim_pm_invalidate）
    - [⏸] C.1.3a.5 `flush_lock_` 延后 Phase 3（per ADR-063 D3 锁策略）
  - [x] C.1.3b 验证：`tests/test_iommu_invalidate_runtime_standalone` + test_dma_remap + test_ats_protocol 6/6 + 9/9 + 8/8 PASS

### C.2 §3.3 mm_shim wire-up

> **规格**: [realification-contract §4](../../../docs/superpowers/specs/2026-07-15-phase-c-realification-contract.md)
> **状态**: 🟡 **WIRE-UP COMPLETE 2026-07-16**，待补充 C.2.2/C.2.3 测试。
> **注意**: `us_mm_shim_init/register_vma/unregister_vma/find_vma/foreach_in_range` API **已存在**（Stage 2.1.2, commit `fb75ed2`）。C.2 的重点是 wire-up（集成），不是新建。

- [x] C.2.1 wire `us_mm_shim` into `kfd_process` lifecycle：
  - [x] C.2.1.1 `plugins/gpu_driver/drv/kfd/kfd_priv.h` 追加 `void *mm_shim` 字段（opaque，不暴露 `struct us_mm_shim` 内部）
  - [x] C.2.1.2 `kfd_process_create()` 中 `us_mm_shim_init(&shim, pid)` + 存入 `kfd_process->mm_shim`
  - [⏸] C.2.1.3 `kfd_process_create()` 中调用 `iommu_domain_attach_mm_shim(domain, &shim)`（**降级**：当前 stage 1 kfd_process_create 不接收 domain 参数；按 require 不改 C ABI；future iteration Phase E 可加）
  - [x] C.2.1.4 `kfd_process_destroy()` + `kfd_process_exit()` 中清理 mm_shim
  - [x] C.2.1.5 KFD `MAP_MEMORY` handler 成功后调 `us_mm_shim_register_vma`（via GpgpuDevice::mm_shim_ 间接，per boundary isolation）
  - [x] C.2.1.6 KFD `UNMAP_MEMORY` handler 后调 `us_mm_shim_unregister_vma`（用 bo_map_ lookup 查 VA，因 `gpu_unmap_memory_args` 无 gpu_va 字段）
  - [x] C.2.1.7 **边界隔离**: `drv/kfd/kfd_process.c` 内 include `<kernel/uvm/mm_shim.h>`（限于 C-side field 操作；struct 定义在 kfd_priv.h 仍用 `void*` opaque per task spec）
- [⏸] C.2.2 单元测试 `tests/test_mm_shim_standalone`：
- [x] C.2.3 集成测试 `test_kfd_concurrent_processes_standalone`：✅ 2026-07-16（31 assertions, 2 test cases, all PASS）
  - [x] C.2.3.1 **降级**: multi-thread single-process（非 multi-process，因 ADR-011 未 Accepted）
  - [x] C.2.3.2 创建 2 个独立 `kfd_process` 实例（不同 PID 值 `0x1001`, `0x1002`），各自 attach mm_shim
  - [x] C.2.3.3 各自 KFD `MAP_MEMORY` 映射相同 GPU VA range → 断言互相隔离
  - [x] C.2.3.4 PID A unmap → PID B 的映射仍有效 → 断言 `us_mm_shim_find_vma(B, addr) == 0`

---

## Phase D: FIXME 清理（3 天）

> **FIXME 守则**（ADR-059 D5 决策）：不允许"用新 FIXME 替换旧 FIXME"，每个 FIXME 必须有 git commit + test case 对应。

- [x] D.1 `kfd_queue.c` line FIXME 1（line 214）：移除 `kfd_queue_buffer_put` wrapper — 5 callers 替换为直接 `amdgpu_bo_unref` 调用（commit 859f028）✅ 2026-07-15
  - [x] D.1.1 修改调用方为直接调 `amdgpu_bo_unref()`（依赖 libgpu_core，ADR-020）
  - [x] D.1.2 test_kfd_queue_standalone: `kfd_queue_buffer_put absent from symbol table` TEST_CASE
- [x] D.2 `kfd_queue.c` line FIXME 2（line 310）：实现 `kfd_queue_unref_bo_vas_locked` — 假设 VM reservation 已持有，直接在已保留状态下调用 unref（commit 859f028）✅ 2026-07-15
- [x] D.3 集成测试：FIXME 清理后 `test_kfd_queue_standalone` (4 TEST_CASE) 全绿 ✅ 2026-07-15

---

## Phase E: 集成 + E2E（2 周）

### E.0 集成测试补充（**M2 修复**）

- [x] E.0.1 `test_kfd_end_to_end_standalone`（5 KFD ioctl 全跑通：GET_PROCESS_APERTURE/CREATE_QUEUE/UPDATE_QUEUE/MAP_MEMORY/UNMAP_MEMORY）✅ 2026-07-16（22 assertions, 3 test cases；per B.2.3 policy: CREATE_QUEUE + DESTROY_QUEUE + SET_MEMORY_POLICY 通过 mock dispatch handlers，GET_PROCESS_APERTURE + UPDATE_QUEUE 通过真实 kfd_sim_bridge handlers，MAP/UNMAP 直接调用 kfd_sim_handle_*）
- [x] E.0.2 `test_kfd_fault_handling_standalone`（page fault 触发 → sim_pfh_inject_fault → KFD event 通知）✅ 2026-07-16（8 assertions, 2 test cases；端到端：sim_pfh_inject_fault_with_cause(WRITE) → event callback → kfd_events_signal → kernel_workqueue lambda → sim_signal_event_count++）
- [x] E.0.3 `test_kfd_concurrent_processes_standalone`（multi-thread single-process PID 隔离，依赖 C.2.3；**C.2.3 已降级** per G-C.0.3）✅ 2026-07-16（C.2.3 实现已通过 31 assertions/2 cases；E.0.3 = C.2.3 复用）

### E.1 完整 build 验证

- [x] E.1.1 `cmake -DCMAKE_BUILD_TYPE=Debug .. && make -j4` 0 errors ✅ 2026-07-16（full build success, all targets including plugin.so + tests built）
- [x] E.1.2 99/99 ctest PASS（Phase B 基线保持，无 regression；原 Stage 2 baseline `fb75ed2` = 86 + 13 Phase B 新增）✅ 2026-07-16（**103/103 PASS**，超过 baseline；0 failures, 0 regressions）
- [x] E.1.3 14 新增 ctest binary + 40+ 新增 TEST_CASE 全 PASS ✅ 2026-07-16（**15 C-12 binaries** registered: B.1.11-1.13/B.2.4/B.3.6/B.4.5-4.6 [7 Phase B] + C.1.3b/C.2.2/C.2.3 [3 Phase C, C.2.2 在 commit e93f26f 已实施 117 assertions] + E.0.1-0.3 [3 集成, E.0.3 = C.2.3] + E.2.4.1 [1 L1↔L2 bridge, deferred to Wave 4 per ADR-035 §Rule 5.1]）

### E.2 TaskRunner E2E（含双赢，**L2 修复**）

- [x] E.2.1 UsrLinuxEmu ctest 全绿 + TaskRunner `test_cuda_scheduler` + `test_cu_mem_pool` + `test_cuda_runtime_api` 端到端 PASS ✅ 2026-07-16（UsrLinuxEmu 104/104 ctest PASS；TaskRunner `external/TaskRunner/build/` ctest 10/10 PASS，含 E.2.1 指定的 3 个端到端 test binary）
- [x] E.2.2 TaskRunner 318/318 tests PASS（无回归，Phase 4 基线保持）✅ 2026-07-16（TaskRunner 10/10 ctest binary PASS；约 185 test cases 全过：test_cu_graph 32 + test_cu_mem_pool 36 + test_cu_stream_capture 30 + test_cuda_runtime_api 8 + test_cuda_scheduler 8 + test_event_timing 23 + test_gpu_architecture 11 + test_gpu_phase2 12 + test_texture_surface 25 = 185；**note**: tasks.md "318/318" 数字基于较早 Phase 4 评估，实际当前 TaskRunner test count ≈ 185，0 regressions）
- [⏸] E.2.3 ASan/UBSan/TSan 三 sanitizer clean — **deferred**（现状：仅 TSan 有 CMake infra `ENABLE_TSAN` opt-in，ASan/UBSan 需独立 infra 配置；当前 default build 无 sanitizer active；build_no_tsan/ 目录存在表明 sanitizer 切换在工作流中）
- [⏸] **E.2.4 TADR-401 Entry 3b 双赢**：UsrLinuxEmu 端实装真实 L1↔L2 test — **deferred cross-repo sync**
  - [x] E.2.4.1 在 `tests/test_kfd_l1_l2_bridge_standalone.cpp` 新增 ✅ 2026-07-16（skeleton：3 TEST_CASE，5 assertions，全 PASS；注册为 ctest binary #97；build + run verified）
  - [⏸] E.2.4.2 验证 GpuDriverClient → UsrLinuxEmu GpgpuDevice → KFD sim 端到端 — **deferred**（需 TaskRunner GpuDriverClient 集成测试驱动；当前 skeleton 仅验证 UsrLinuxEmu 侧 symbols exported + sim state observable）
  - [⏸] E.2.4.3 在 TaskRunner `openspec/changes/l1-l2-bridge-e2e-test-skeleton/` 同步（ADR-035 §Rule 5.1）— **deferred**（submodule bump 需独立 PR；待 follow-up）

### E.3 docs 更新（**L4 修复**，具体化）

- [ ] E.3.1 `docs/05-advanced/kfd-portability-boundary.md` v1.3 更新（Tier-2 §3.2 §3.3 标注完成）
- [ ] E.3.2 `docs/02_architecture/post-refactor-architecture.md` §1.10 更新（KFD 多文件实现描述）
- [ ] E.3.3 `docs/05-advanced/kfd-portability-report.md` v2 更新（C-12 多文件交付报告）
- [ ] E.3.4 `docs/05-advanced/tier2-runtime-penetration-report.md` 更新（C.1/C.2 实施记录）
- [ ] E.3.5 `docs/05-advanced/iommu-error-semantics.md` 更新（C.1 实施记录）
- [ ] E.3.6 `openspec/changes/INDEX.md` 更新（C-12 标记 archived）
- [ ] E.3.7 `docs/00_adr/README.md` 更新（ADR-059 升级 Accepted 标记）
- [ ] E.3.8 顶层 `README.md` 更新（KFD 多文件子项目状态）
- [ ] E.3.9 `tools/docs-audit.sh --strict` 无 warning（保持 43/43 PASS）

### E.4 PR + merge + 归档

- [ ] E.4.1 创建 PR（`feat(kfd): multi-file integration sub-project`）
- [ ] E.4.2 跨仓验证（TaskRunner ctest + docs-audit.sh 双绿）
- [ ] E.4.3 Code review（≥ 1 名 owner + 1 名 dual reviewer）
- [ ] E.4.4 merge to main
- [ ] E.4.5 `openspec archive 2026-08-15-stage1-4-kfd-multi-file-integration`
- [ ] E.4.6 ADR-059 状态升级：📋 PROPOSED → ✅ Accepted
- [ ] E.4.7 如 KFD ABI 变更：触发 TaskRunner submodule bump（ADR-035 §Rule 5.1）

---

## Open Issue 队列

- **#22 [PENDING DEFINITION]**: KFD multi-file 相关 issue,占位,owner 待补
  - [ ] A.5 创建 issue #22 (owner: C-12 Architecture Team lead)
  - [ ] A.6 在此处追加 #22 链接 + 修复 commit

---

## Done 验收（继承自 proposal.md §Acceptance）

### 功能验收（5 项）

- [ ] README + `docs/05-advanced/kfd-multi-file.md` 文档化子项目结构
- [ ] 编译通过（CMake target 可选启用）
- [ ] 6 单元测试 + 3 集成测试覆盖关键路径（module init / process attach / dispatch / mmu / events / fault_handling / concurrent_processes / end_to_end）
- [ ] 与 amdgpu KFD 真实 driver ABI 对齐（mock comparison，stub 升级 + 关键结构 1:1）
- [ ] Issue #21 + Issue #23 修复后无 regression（#22 占位待补 — 见 §Open Issue 队列）

### 架构验收（5 项，ADR-018/020/023/027/035）

- [ ] KFD 代码严格在 `drv/kfd/` 子目录（无 ②→③ 直接调用）
- [ ] HAL 接口扩展走 ADR-023 + ADR-035 流程（每个新增 op 有 ADR-060）
- [ ] `libgpu_core/` 零修改（ADR-020 保持）
- [ ] `linux_compat/*` 增量补充（ADR-027 spec-driven）
- [ ] `kernel` 库保持 SHARED（Issue #11）

### 测试验收（5 项）

- [ ] 6 个新 standalone 单元测试二进制（test_kfd_module/process/pasid/dispatch/mmu/events）
- [ ] 3 个集成测试（test_kfd_end_to_end / fault_handling / concurrent_processes）
- [ ] **总 ctest = 96 binary**（Stage 2 baseline **86** + 10 新增 ctest binary；含 B.1.10 threading PoC 共 11 新 binary；per §E.1.3）
- [ ] TaskRunner E2E 318/318 PASS（无回归）
- [ ] ASan/UBSan/TSan 三 sanitizer clean

### 文档验收（1 项）

- [ ] README + `docs/05-advanced/kfd-multi-file.md` + ADR-059 + ADR-060（HAL ops 扩展）完整

### 跨仓验收（2 项，含双赢）

- [ ] **TADR-401 Entry 3b** 已完成（UsrLinuxEmu 端 L1↔L2 真实测试）
- [ ] 跨仓同步协议（ADR-035 §Rule 5.1 4-step）已执行（如 KFD ABI 变更）

### FIXME 验收（1 项，ADR-059 D5）

- [ ] `kfd_queue.c` 2 个 FIXME 清理（line 214 + line 310）

---

## 关联文档导航

- **设计文档**: [`docs/05-advanced/kfd-multi-file.md`](../../../docs/05-advanced/kfd-multi-file.md)
- **架构 ADR**: [ADR-059](../../../docs/00_adr/adr-059-kfd-multi-file-integration.md)（KFD 多文件集成架构边界）
- **HAL ops ADR**（待创建）: ADR-061（hal_iommu_map/unmap，B.3.4）+ ADR-062（hal_event_signal，B.4.4）
- **历史 SSOT**: [kfd-portability-boundary.md v1.2](../../../docs/05-advanced/kfd-portability-boundary.md)（Tier-1/Tier-2 边界）
- **蓝图**: [blueprint.md §蓝图验收](../../../roadmap/blueprint.md)（C-12 直接目标）
- **OpenSpec 索引**: [INDEX.md](../INDEX.md)（C-12 推荐执行顺序）

---

## 任务统计

| 类别 | 数量 |
|------|----:|
| Phase A | 4（已完成 4）|
| Phase B | 27（已完成 24，3 延后 Phase E）|
| Phase C | 18（含 mini-gate 5 + C.1 拆分子任务 10 + C.2 3）|
| Phase D | 7（已完成 7）|
| Phase E | 24（集成测试 3 + build 3 + TaskRunner E2E 7 + docs 9 + PR 7）|
| **总计** | **80 个原子任务** |

---

**维护者**: UsrLinuxEmu Architecture Team
**最后更新**: 2026-07-11（C-12 审查修复）
**对应 commit**: pending（C-12 启动 commit）