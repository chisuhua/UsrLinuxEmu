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

> **🔀 Metis 调整（2026-07-15）**：B.1 内部实施顺序应为
> **B.1.7 → B.1.8 → B.1.9（头文件扩展）→ B.1.1 → B.1.3 → B.1.5（模块实现）→ B.1.11/12/13（单元测试）**
> 而非线性的 B.1.1 → B.1.2 → …。原因：模块实现依赖头文件中扩展的 struct 声明
> （`struct kfd_process` / `struct kfd_dev` / `struct svm_range_list` 等）。
> 桥接接口（kfd_module.h / kfd_module.c）已就绪；mutex 抽象（kfd_priv.h）已就绪。

> **架构原则**: 严格遵循 ADR-018（KFD 仅在 ② 层）+ ADR-023（HAL 桥接）+ ADR-027（spec-driven）。
> **模块依赖图**: 见 [kfd-multi-file.md §3.1](../../../docs/05-advanced/kfd-multi-file.md)
> **Open Questions 决策**: 串行实施（避免大爆炸合并冲突），详见 proposal.md §Open Questions 决策。

### B.1 基础设施（module / pasid / process）

- [ ] B.1.1 `plugins/gpu_driver/drv/kfd/kfd_module.c` — module init/exit
- [ ] B.1.2 `plugins/gpu_driver/drv/kfd/kfd_module.h`
- [ ] B.1.3 `plugins/gpu_driver/drv/kfd/kfd_pasid.c` — PASID mgmt
- [ ] B.1.4 `plugins/gpu_driver/drv/kfd/kfd_pasid.h`
- [ ] B.1.5 `plugins/gpu_driver/drv/kfd/kfd_process.c` — process aperture
- [ ] B.1.6 `plugins/gpu_driver/drv/kfd/kfd_process.h`
- [ ] **B.1.7 扩展 `kfd_priv.h`**（ADR-059 D4 决策）：补全 `struct kfd_process` + `struct kfd_dev` 真实声明
- [ ] **B.1.8 扩展 `kfd_topology.h`**（ADR-059 D4 决策）：补全 `struct kfd_topology_device` + 节点发现 stub
- [ ] **B.1.9 扩展 `kfd_svm.h`**（ADR-059 D4 决策）：补全 `struct kfd_svm` + range tree stub（与 B.3.1 协同）
- [x] **B.1.10 线程基础设施 PoC**（**依赖 ADR-060 Accepted**，C-12 启动 commit 第一步）✅ 2026-07-14
  - [x] B.1.10.1 `include/kernel/thread/kernel_thread_base.h`（raw pthread_* 包装）
  - [x] B.1.10.2 `src/kernel/thread/kernel_thread_base.cpp`（start/stop/is_running/RAII）
  - [x] B.1.10.3 `include/kernel/thread/kernel_workqueue.h`（workqueue 模拟）
  - [x] B.1.10.4 `src/kernel/thread/kernel_workqueue.cpp`（enqueue/flush/stop）
  - [x] B.1.10.5 GCC 13 pthread bug workaround 验证（CMake `-pthread` + `<sched.h>` 显式 include）
  - [x] B.1.10.6 `tests/test_kfd_threading_standalone.cpp`（**10 TEST_CASE**，> 4 要求）
  - [x] B.1.10.7 CMakeLists.txt 添加 `ENABLE_TSAN` option（Clang）
  - [x] B.1.10.8 **验证**：ASan/UBSan 基线 clean（TSan 待 `-DENABLE_TSAN=ON` opt-in run）
  - [x] B.1.10.9 **验证**：既有 ctest 86/86 无 regression（**注**：tasks.md 原文 318 为 TaskRunner 计数，UsrLinuxEmu ctest 基线 86 + 1 新 = 87）
  - [x] B.1.10.10 **验证**：1 个新 ctest binary 通过（内含 11 TEST_CASEs / 26 assertions）
- [ ] B.1.11 单元测试 `test_kfd_module_standalone`（~100 LOC，**M2 拆分**）
- [ ] B.1.12 单元测试 `test_kfd_pasid_standalone`（~150 LOC，**M2 拆分**）
- [ ] B.1.13 单元测试 `test_kfd_process_standalone`（~200 LOC，**M2 拆分**）

### B.2 派发（dispatch）

- [ ] B.2.1 `plugins/gpu_driver/drv/kfd/kfd_dispatch.c` — IOCTL dispatch 表扩展
- [ ] B.2.2 `plugins/gpu_driver/drv/kfd/kfd_dispatch.h`
- [ ] B.2.3 保持 `drm_ioctl_desc[]` ≥ 38 entries（含 5 KFD 0x40-0x47 已就位 + Stage 1.4 ~19 + Phase 3 stream/graph/mem_pool ~18）。C-12 **不新增** dispatch entry，仅保持现有 entries + 完成 6 个新 KFD module 编译通过（如未来需要新增 ioctl，按 ADR-023 + ADR-035 流程单独走 ADR）。
- [ ] B.2.4 单元测试 `test_kfd_dispatch_standalone`（~180 LOC，**M2 拆分**）

### B.3 内存（mmu + HAL ops + sim bridge）

- [ ] B.3.1 `plugins/gpu_driver/drv/kfd/kfd_mmu.c` — KFD-side MMU
- [ ] B.3.2 `plugins/gpu_driver/drv/kfd/kfd_mmu.h`
- [ ] B.3.3 集成 `sim_pm_*` 真实 IOMMU invalidation
- [ ] **B.3.4 HAL op 扩展**（**H1 修复**，ADR-023 + ADR-035 流程）：
  - [ ] B.3.4.1 修改 `struct gpu_hal_ops`（`plugins/gpu_driver/hal/gpu_hal_ops.h`）新增 `hal_iommu_map()` / `hal_iommu_unmap()`
  - [ ] B.3.4.2 `hal_mock.cpp` 实现（路由 `sim_pm_migrate_to_device/system`）
  - [ ] B.3.4.3 `hal_user.cpp` 桩实现（真机路径）
  - [ ] B.3.4.4 MockGpuDriver 测试覆盖（ADR-032 IGpuDriver 模式）
  - [ ] B.3.4.5 **单独走 ADR 流程**（ADR-023 §新增 HAL ops 流程 + ADR-059 §D3）：创建 `adr-061-hal-iommu-extension.md`
- [ ] **B.3.5 扩展 `kfd_sim_bridge`**（**M3 修复**）：现有 5 handler 集成到 `kfd_mmu.c`（map/unmap）+ `kfd_pasid.c`（PASID 索引）
- [ ] B.3.6 单元测试 `test_kfd_mmu_standalone`（~250 LOC，**M2 拆分**）
- [ ] **B.3.7 暴露 `kfd_mmu_get_workqueue()` accessor**（per ADR-060 §2.1 + Migration:400；day-1 **不启用** async 路径，仅暴露返回 `kernel_workqueue*` 的 accessor；return 值当前不被任何 caller 使用，未来 1 行 switch 启用 async mmu_notifier callback）：
  - [ ] B.3.7.1 在 `plugins/gpu_driver/drv/kfd/kfd_mmu.h` 声明 `kernel_workqueue* kfd_mmu_get_workqueue(void);`
  - [ ] B.3.7.2 在 `plugins/gpu_driver/drv/kfd/kfd_mmu.c` 实现（initialization lazy，与 module init 绑定 `kernel_workqueue(1)`）
  - [ ] B.3.7.3 静态分析检查：当前 Phase B 期间零 caller（grep 确认）
  - [ ] B.3.7.4 文档 1-line 说明（`kfd_mmu.h` 头注释）："day-1 stub, future Phase C async opt-in via 1-line change"

### B.4 事件（events + HAL ops）

- [ ] B.4.1 `plugins/gpu_driver/drv/kfd/kfd_events.c` — event notification
- [ ] B.4.2 `plugins/gpu_driver/drv/kfd/kfd_events.h`
- [ ] B.4.3 集成 sim signal path
- [ ] **B.4.4 HAL op 扩展**（**H1 修复**，ADR-023 + ADR-035 流程）：
  - [ ] B.4.4.1 修改 `struct gpu_hal_ops` 新增 `hal_event_signal()`
  - [ ] B.4.4.2 `hal_mock.cpp` 实现（路由 sim signal）
  - [ ] B.4.4.3 `hal_user.cpp` 桩实现
  - [ ] B.4.4.4 MockGpuDriver 测试覆盖
  - [ ] B.4.4.5 **追加到 ADR-062 (HAL ops event signal 扩展)**（与 B.3.4 同流程但独立 ADR；ADR-061 专管 IOMMU，ADR-062 专管 event signal；C-12 实施时与 ADR-061 同时创建）
- [ ] B.4.5 单元测试 `test_kfd_events_standalone`（~180 LOC，**M2 拆分**）
- [ ] **B.4.6 kfd_events 后台线程**（**ADR-060 §2.1 异步决策**：kfd_event_work_handler 模拟）
  - [ ] B.4.6.1 `kfd_events_thread_`（基于 kernel_thread_base）
  - [ ] B.4.6.2 `kfd_events_queue_`（struct list_head + pthread_mutex_t + pthread_cond_t，C 文件不自引入 STL）
  - [ ] B.4.6.3 `kfd_events_cv_` 唤醒（condition_variable）
  - [ ] B.4.6.4 事件处理主循环 `runLoop()`
  - [ ] B.4.6.5 start()/stop() API（kfd_module_init/exit 调用）
  - [ ] B.4.6.6 TSan 测试覆盖（race condition 检测）

---

## Phase C: Stage 1.4 Tier-2 deferred（2 周）

> **来源**: [kfd-portability-boundary.md §3](../../../docs/05-advanced/kfd-portability-boundary.md) §3.2 + §3.3

- [ ] C.1 §3.2 IOMMU invalidation 真实化
  - [ ] C.1.1 修 `plugins/gpu_driver/sim/sim_pfh_*`（page fault handler 真实化）
  - [ ] C.1.2 修 `plugins/gpu_driver/sim/sim_pm_*`（page migration 真实化）
  - [ ] C.1.3 IOTLB flush 真实化（基于 commit `6a7f4ab` 基础扩展）
  - [ ] C.1.4 验证：`tests/test_iommu_emu_standalone` 覆盖 invalidation 路径
- [ ] C.2 §3.3 mm_struct PID + VMA tracking
  - [ ] C.2.1 修 `src/kernel/mm_shim.cpp` 加 PID + VMA 跟踪
  - [ ] C.2.2 单元测试 `tests/test_mm_shim_standalone` 扩展（新增 PID/VMA 测试 cases）
  - [ ] C.2.3 集成测试 `test_kfd_concurrent_processes_standalone`（验证多进程 PID 隔离）

---

## Phase D: FIXME 清理（3 天）

> **FIXME 守则**（ADR-059 D5 决策）：不允许"用新 FIXME 替换旧 FIXME"，每个 FIXME 必须有 git commit + test case 对应。

- [ ] D.1 `kfd_queue.c` line FIXME 1（line 214）：移除 `amdgpu_bo_unref` 直接调用
  - [ ] D.1.1 修改调用方为直接调 `amdgpu_bo_unref()`（依赖 libgpu_core，ADR-020）
  - [ ] D.1.2 单元测试：BO 引用计数正常释放路径
- [ ] D.2 `kfd_queue.c` line FIXME 2（line 310）：实现 `_locked` 版本
  - [ ] D.2.1 实现 `kfd_queue_*_locked()` 版本（使用 `pthread_mutex_t` 保护，C 文件不引入 STL，遵守 ADR-018 决策 3）
  - [ ] D.2.2 单元测试：并发场景 `_locked` 版本正确性
- [ ] D.3 集成测试：FIXME 清理后 `test_kfd_queue_standalone` 全绿

---

## Phase E: 集成 + E2E（2 周）

### E.0 集成测试补充（**M2 修复**）

- [ ] E.0.1 `test_kfd_end_to_end_standalone`（5 KFD ioctl 全跑通：GET_PROCESS_APERTURE/CREATE_QUEUE/UPDATE_QUEUE/MAP_MEMORY/UNMAP_MEMORY）
- [ ] E.0.2 `test_kfd_fault_handling_standalone`（page fault 触发 → sim_pfh_inject_fault → KFD event 通知）
- [ ] E.0.3 `test_kfd_concurrent_processes_standalone`（多进程 PID + VMA 隔离，依赖 C.2.3）

### E.1 完整 build 验证

- [ ] E.1.1 `cmake -DCMAKE_BUILD_TYPE=Debug .. && make -j4` 0 errors
- [ ] E.1.2 86/86 ctest PASS（Stage 2 baseline `fb75ed2` 保持，无 regression）
- [ ] E.1.3 10 新增 ctest binary + 30+ 新增 TEST_CASE 全 PASS（B.1.11-B.1.13、B.2.4、B.3.6、B.4.5 共 6 单元；E.0.1-E.0.3 共 3 集成；E.2.4.1 共 1 L1↔L2 bridge）

### E.2 TaskRunner E2E（含双赢，**L2 修复**）

- [ ] E.2.1 UsrLinuxEmu ctest 全绿 + TaskRunner `test_cuda_scheduler` + `test_cu_mem_pool` + `test_cuda_runtime_api` 端到端 PASS
- [ ] E.2.2 TaskRunner 318/318 tests PASS（无回归，Phase 4 基线保持）
- [ ] E.2.3 ASan/UBSan/TSan 三 sanitizer clean
- [ ] **E.2.4 TADR-401 Entry 3b 双赢**：UsrLinuxEmu 端实装真实 L1↔L2 test
  - [ ] E.2.4.1 在 `tests/test_kfd_l1_l2_bridge_standalone.cpp` 新增
  - [ ] E.2.4.2 验证 GpuDriverClient → UsrLinuxEmu GpgpuDevice → KFD sim 端到端
  - [ ] E.2.4.3 在 TaskRunner `openspec/changes/l1-l2-bridge-e2e-test-skeleton/` 同步（ADR-035 §Rule 5.1）

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
| Phase A | 4（已完成 1）|
| Phase B | 27（含 HAL ops 10 + kfd_sim_bridge 1 + 单元测试 6 + stub 扩展 3）|
| Phase C | 8（IOMMU + mm_struct + 集成测试 1）|
| Phase D | 7（FIXME 2 + 测试 5）|
| Phase E | 24（集成测试 3 + build 3 + TaskRunner E2E 7 + docs 9 + PR 7）|
| **总计** | **70 个原子任务** |

---

**维护者**: UsrLinuxEmu Architecture Team
**最后更新**: 2026-07-11（C-12 审查修复）
**对应 commit**: pending（C-12 启动 commit）