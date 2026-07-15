# KFD Multi-File Integration (Stage 1.4 Sub-Project)

> **状态**: 📋 DESIGN（Phase A.1 文档化，C-12 启动）
> **最后更新**: 2026-07-11
> **Owner**: UsrLinuxEmu Architecture Team
> **关联 Change**: `openspec/changes/2026-08-15-stage1-4-kfd-multi-file-integration/`（C-12）
> **关联 ADR**: [ADR-059](../00_adr/adr-059-kfd-multi-file-integration.md)（KFD 多文件架构边界）
> **基础 SSOT**: [kfd-portability-boundary.md](kfd-portability-boundary.md) v1.2 + [stage-1-kernel-emu.md §1.4](../roadmap/stage-1-kernel-emu.md) + [post-refactor-architecture.md §1.10](../02_architecture/post-refactor-architecture.md)

---

## 1. 目标与背景

### 1.1 项目目标

把 UsrLinuxEmu 内单文件 KFD PoC（`kfd_queue.c` 520 行）扩展为**完整 ~50K LOC 多文件 KFD 子项目**，对齐真实 amdgpu KFD 驱动 ABI，实现：

1. **KFD 驱动代码可在 UsrLinuxEmu 内编译运行**（蓝图终态验收第 1 条）
2. **KFD 5 个核心 ioctl 在 UsrLinuxEmu 内跑通**（蓝图终态验收第 2 条）
3. **Stage 1.4 Tier-2 deferred §3.2 §3.3 落地**（IOMMU invalidation + mm_struct PID/VMA tracking）

### 1.2 起源与历史

| 里程碑 | 日期 | 关键发现 |
|--------|------|----------|
| Stage 1.4 PoC attempt | 2026-07-04 (commit `5341c3f`) | "real KFD transitively depends on 53+ amdgpu_* headers (~50K+ lines)" → **完整 KFD 多文件超出 Stage 1 范围** |
| Stage 1.4 Tier-1 交付 | 2026-07-04 (commit `80f6a44`) | 5 KFD IOCTL + 19 ioctl 派发表 + `kfd_queue.c` 编译通过（520 行）+ 3 设备节点 + 10 sim C 接口 |
| Stage 1.4 Tier-2 穿透 | 2026-07-05 (commit `6a7f4ab`) | 9 STUB_HANDLER 升级 + mmu_notifier 完整化 + IOTLB flush 真实化 |
| Stage 2 multi-device | 2026-07-05 (commit `fb75ed2`) | mm_shim.cpp 引入，Phase C.2 实施位置就绪 |
| README "后续子项目" | 2026-07-05 | "完整 KFD 多文件集成（独立子项目，~50K 行 amdgpu driver 移植）" |
| **C-12 PROPOSED** | **2026-08-15** | **本子项目正式立项（6-8 周 effort）** |

### 1.3 C-12 与现有成果的关系

```
C-12 (kfd-multi-file-integration)
   │
   ├─ 继承自 Stage 1.4 Tier-1（commit `80f6a44`）+ Tier-2 穿透（commit `6a7f4ab`）
   ├─ 吸收 Stage 1.4 Tier-2 deferred §3.2 §3.3
   ├─ 清理 `kfd_queue.c` 2 个 FIXME（line 214, 310）
   └─ 配合 TaskRunner Phase 4 完成（依赖已就绪）
```

---

## 2. 架构依据（ADR 引用）

> **设计原则**: 所有 KFD 多文件工作严格遵循 ADR-036（3 区分）+ ADR-018（dr/hal/sim 分离）+ ADR-035（治理）。

### 2.1 Tier 1：架构元决策（必读）

| ADR | 标题 | 状态 | 与本子项目关联 |
|-----|------|------|---------------|
| [ADR-036](../00_adr/adr-036-three-way-separation.md) | **3 区分架构原则** | ✅ Accepted | **架构判定基准**：KFD 仅在 ② 层，所有硬件访问通过 HAL |
| [ADR-018](../00_adr/adr-018-driver-sim-separation.md) | 驱动/仿真代码分离 | ✅ Accepted | **物理隔离基础**：`drv/kfd/` 独立目录，禁直接调 sim_* |
| [ADR-035](../00_adr/adr-035-governance-policy.md) | 治理规则 | ✅ Accepted | **本子项目的元规则**：新增 HAL ops 走 ADR；每个 PR 引用 ADR |
| [ADR-023](../00_adr/adr-023-hal-interface.md) | HAL 接口契约 | ✅ Accepted | **HAL 桥接规范**：`struct gpu_hal_ops` 11 个函数指针扩展策略 |

### 2.2 Tier 2：基础设施层（直接关联）

| ADR | 标题 | 状态 | 与本子项目关联 |
|-----|------|------|---------------|
| [ADR-008](../00_adr/adr-008-linux-api-compat.md) | Linux API 兼容层基础 | ✅ Accepted | 借鉴 KFD 风格、`drv/kfd/` 子目录下用 Linux kernel idioms 编写的 .c 文件零修改编译的基础 |
| [ADR-019](../00_adr/adr-019-drm-gem-ttm-alignment.md) | DRM/GEM/TTM 对齐 | ✅ Accepted | 现有 entries 基础（Stage 1.4 + Phase 3）|
| [ADR-027](../00_adr/adr-027-linux-compat-strategy.md) | Linux 兼容层扩展策略 | ✅ Accepted | **spec-driven 原则**：仅按 KFD 实际需要增量补 API |
| [ADR-037](../00_adr/adr-037-render-node-permissions.md) | VFS Device Permission Model | ✅ Accepted | 3 设备节点 `/dev/kfd` `/dev/dri/*` mode=0666 |
| [ADR-043](../00_adr/adr-043-cp-portability-boundary.md) | CP 可移植性边界 | ✅ Accepted | KFD 命令处理器边界判定 |

### 2.3 Tier 3：执行与调度层（间接关联）

| ADR | 标题 | 状态 | 与本子项目关联 |
|-----|------|------|---------------|
| [ADR-015](../00_adr/adr-015-gpu-ioctl-unification.md) | GPU IOCTL 接口统一 | ✅ Accepted | 5 个 KFD IOCTL 编号（0x40-0x47）基础 |
| [ADR-016](../00_adr/adr-016-gpu-memory-domain.md) | GPU Memory Domain | ✅ Accepted | KFD BO 域分配（VRAM/GTT/CPU）|
| [ADR-017](../00_adr/adr-017-gpfifo-queue-abstraction.md) | GPFIFO/Queue 抽象 | ✅ Accepted | `kfd_queue.c` 队列管理基础 |
| [ADR-020](../00_adr/adr-020-libgpu-core-extraction.md) | libgpu_core 算法核心 | ✅ Accepted | BO 分配后端（FIXME 1 依赖）|
| [ADR-021](../00_adr/adr-021-hardware-puller.md) | Hardware Puller FSM | ✅ Accepted | kfd_queue.c 状态转换基础 |
| [ADR-024](../00_adr/adr-024-user-mode-queue-submission.md) | 用户态队列提交 | ✅ Accepted | UMQ 与 KFD 正交（不在 1.3 UVM 范围）|
| [ADR-031](../00_adr/adr-031-ttm-migration-priority.md) | TTM 迁移优先级 | ✅ Accepted | KFD MMU migration 顺序 |
| [ADR-039](../00_adr/adr-039-mem-pool-export-ioctl.md) | MEM_POOL_EXPORT IOCTL (0x68) | ✅ Accepted | Phase E.2 TaskRunner E2E 验证 |
| [ADR-040](../00_adr/adr-040-puller-fence-completion.md) | Puller Fence Completion | ✅ Accepted | kfd_queue.c fence 机制 |
| [ADR-041](../00_adr/adr-041-graph-node-to-gpfifo-serialization.md) | Graph → GPFIFO 序列化 | ✅ Accepted | KFD graph launch 路径 |
| [ADR-058](../00_adr/adr-058-sim-mem-pool-real-va.md) | sim_mem_pool Real VA | 📋 PROPOSED | Phase C.2 mm_shim PID 跟踪相邻工作 |

### 2.4 Tier 4：跨仓镜像 TADR（TaskRunner）

| TADR | 标题 | 与本子项目关联 |
|------|------|---------------|
| [tadr-301](../external/TaskRunner/docs/shared/adr/tadr-301-igpu-driver-contract.md) | IGpuDriver 28→47 方法契约 | Phase E.2 TaskRunner E2E 接口 |
| [tadr-305](../external/TaskRunner/docs/shared/adr/tadr-305-mempool-export-shareable.md) | IGpuDriver::memPoolExportShareable | Phase E.2 MEM_POOL_EXPORT 验证 |
| [tadr-107](../external/TaskRunner/docs/shared/adr/tadr-107-shared-infrastructure-boundary.md) | shared 边界 | 跨仓同步协议 |

### 2.5 SSOT 文档引用

| 文档 | 引用位置 |
|------|---------|
| [post-refactor-architecture.md §1.10](../02_architecture/post-refactor-architecture.md) | 3 区分当前实现（KFD drv/hal/sim 物理隔离判定） |
| [kfd-portability-boundary.md](kfd-portability-boundary.md) v1.2 | KFD Tier-1/Tier-2 边界 SSOT |
| [kfd-portability-report.md](kfd-portability-report.md) | Tier-1 交付报告（commit `f41ace5`）|
| [tier2-runtime-penetration-report.md](tier2-runtime-penetration-report.md) | Tier-2 穿透报告（commit `6a7f4ab`）|
| [iommu-error-semantics.md](iommu-error-semantics.md) | Phase C.1 IOMMU invalidation 真实化依据 |
| [stage-1-kernel-emu.md §1.4](../roadmap/stage-1-kernel-emu.md) | Stage 1.4 集成验证（KFD 起源）|
| [blueprint.md](../roadmap/blueprint.md) §蓝图验收 | 第 1-2 条验收（C-12 直接目标）|
| [stage-2-multi-device.md](../roadmap/stage-2-multi-device.md) | mm_shim 来源 |

---

## 3. 6 个新模块职责划分（Phase B 核心）

> **架构原则**: KFD 多文件子项目严格遵循 [ADR-036](../00_adr/adr-036-three-way-separation.md)（3 区分），**所有新模块仅在 `plugins/gpu_driver/drv/kfd/` 目录内**，禁止跨越 ② 层。

### 3.1 模块依赖图

```
                ┌─────────────────────────┐
                │   kfd_module.c          │  ← 模块生命周期入口
                │   module_init/exit      │
                └──────────┬──────────────┘
                           │
              ┌────────────┼────────────────┐
              │            │                │
              ▼            ▼                ▼
     ┌────────────────┐  ┌──────────────┐  ┌──────────────────┐
     │ kfd_process.c  │  │kfd_dispatch.c│  │  kfd_events.c    │
     │ 进程 aperture  │  │ IOCTL 派发表 │  │ 事件通知机制     │
     └────────┬───────┘  └──────┬───────┘  └────────┬─────────┘
              │                 │                   │
              ▼                 ▼                   │
     ┌────────────────┐        │                   │
     │ kfd_pasid.c    │        │                   │
     │ PASID 分配管理 │        │                   │
     └────────┬───────┘        │                   │
              │                │                   │
              ▼                │                   │
     ┌────────────────┐        │                   │
     │ kfd_mmu.c      │        │                   │
     │ KFD-side MMU   │        │                   │
     └────────┬───────┘        │                   │
              │                │                   │
              ▼                ▼                   ▼
     ┌─────────────────────────────────────────────────┐
     │           HAL 接口（struct gpu_hal_ops）         │
     │  sim_pm_migrate_to_device/system               │
     │  sim_pfh_inject_fault_with_cause                │
     │  fence_create / doorbell_ring                   │
     └────────────────────┬────────────────────────────┘
                          │ (3 区分 ②→③ 桥)
                          ▼
              ┌──────────────────────────┐
              │   ③ Hardware Sim         │
              │  plugins/gpu_driver/sim/ │
              └──────────────────────────┘
```

### 3.2 模块职责详解

#### 3.2.1 `kfd_module.c` + `kfd_module.h` — 模块生命周期

| 维度 | 内容 |
|------|------|
| **职责** | Linux 风格的 module_init/module_exit 入口（与 `linux/module.h` 兼容）|
| **关键 API** | `module_init(fn)` / `module_exit(fn)` 宏 + `__init` / `__exit` 段标记 |
| **依赖** | `kfd_priv.h`（私有定义）|
| **不依赖** | 任何 sim 层或 HAL（确保 ② 层纯净）|
| **Phase** | B.1.1 |

#### 3.2.2 `kfd_process.c` + `kfd_process.h` — 进程 aperture 管理

| 维度 | 内容 |
|------|------|
| **职责** | 维护进程级 GPU aperture 表（对应 KFD `kfd_process_device_private_data`）|
| **关键 API** | `kfd_create_process()` / `kfd_destroy_process()` / `kfd_get_process_aperture()` |
| **数据结构** | `struct kfd_process { pid_t pid; uint64_t gpu_va_base; uint64_t gpu_va_limit; ... }` |
| **依赖** | `kfd_pasid.c`（获取 PASID）|
| **对应 IOCTL** | `GPU_IOCTL_GET_PROCESS_APERTURE` (0x44) |
| **Phase** | B.1.4 |

#### 3.2.3 `kfd_pasid.c` + `kfd_pasid.h` — PASID 管理

| 维度 | 内容 |
|------|------|
| **职责** | PASID 分配/释放（与 IOMMU page table 关联）|
| **关键 API** | `kfd_allocate_pasid()` / `kfd_free_pasid()` / `kfd_get_pasid_from_process()` |
| **数据结构** | `struct hlist_head pid_to_pasid_[PASID_HASH_SIZE]; pthread_mutex_t pasid_mutex_;`（C 文件不自引入 STL；hlist 来自 `<linux/list.h>` linux_compat wrapper）|
| **依赖** | `kfd_process.c`（按 PID 索引）|
| **HAL 扩展** | 如需新增 `hal_pasid_*` 走 ADR-027 spec-driven |
| **Phase** | B.1.3 |

#### 3.2.4 `kfd_dispatch.c` + `kfd_dispatch.h` — IOCTL 派发表

| 维度 | 内容 |
|------|------|
| **职责** | 保持 `drm_ioctl_desc[]` 已就位 entries（Stage 1.4 + Phase 3 stream/graph/mem_pool），C-12 不新增（除非后续走 ADR-023 + ADR-035 流程）|
| **关键 API** | `struct drm_ioctl_desc[]` 静态数组 + `kfd_ioctl_dispatch()` 入口 |
| **数据结构** | 参照 [ADR-019](../00_adr/adr-019-drm-gem-ttm-alignment.md) 决策 2 的 `drm_ioctl_desc` 表 |
| **依赖** | `gpu_drm_driver.cpp`（已有 dispatch）|
| **不依赖** | 不直接调 KFD 业务逻辑（保持薄派发层）|
| **Phase** | B.2.1 |

#### 3.2.5 `kfd_mmu.c` + `kfd_mmu.h` — KFD-side MMU

| 维度 | 内容 |
|------|------|
| **职责** | KFD-side IOMMU page table 管理（API 契约，对接 sim_pm_*）|
| **关键 API** | `kfd_mmu_map()` / `kfd_mmu_unmap()` / `kfd_mmu_invalidate_range()` |
| **HAL 集成** | 通过 `hal_iommu_map()` / `hal_iommu_unmap()` 桥接 `sim_pm_*`（**新增 HAL ops 走 ADR-023 + ADR-035**）|
| **对应 IOCTL** | `GPU_IOCTL_MAP_MEMORY` (0x46) / `GPU_IOCTL_UNMAP_MEMORY` (0x47) |
| **依赖** | `kfd_pasid.c`（PASID 索引 page table）|
| **Phase** | B.3.1 |

#### 3.2.6 `kfd_events.c` + `kfd_events.h` — 事件通知

| 维度 | 内容 |
|------|------|
| **职责** | KFD event 队列管理（page fault / queue eviction / hang 等）|
| **关键 API** | `kfd_event_create()` / `kfd_event_wait()` / `kfd_event_signal()` |
| **HAL 集成** | 通过 `hal_event_signal()` 桥接 sim signal path |
| **数据结构** | `struct list_head event_list_; pthread_mutex_t event_mutex_;`（C 文件不自引入 STL；list_head 来自 `<linux/list.h>` linux_compat wrapper）|
| **依赖** | `kfd_process.c`（事件绑定进程）|
| **Phase** | B.4.1 |

### 3.3 模块总览表

| 模块 | 新建文件 | LOC 估算 | Phase | 依赖 |
|------|---------|---------:|-------|------|
| `kfd_module` | `kfd_module.{c,h}` | ~80 | B.1.1 | kfd_priv |
| `kfd_process` | `kfd_process.{c,h}` | ~200 | B.1.4 | kfd_pasid |
| `kfd_pasid` | `kfd_pasid.{c,h}` | ~120 | B.1.3 | pthread_mutex_t |
| `kfd_dispatch` | `kfd_dispatch.{c,h}` | ~150 | B.2.1 | gpu_drm_driver |
| `kfd_mmu` | `kfd_mmu.{c,h}` | ~250 | B.3.1 | HAL + sim_pm |
| `kfd_events` | `kfd_events.{c,h}` | ~180 | B.4.1 | HAL event |
| **总计** | **12 个新文件** | **~980 LOC** | **B.1-B.4** | — |

---

## 4. 与现有 KFD 代码的关系

### 4.1 现有文件状态（来自 `plugins/gpu_driver/drv/kfd/`）

| 文件 | 当前状态 | C-12 后续 |
|------|---------|----------|
| `kfd_queue.c` (520 行) | 真实移植（Linux 6.12 LTS）| **保留 + FIXME 清理**（Phase D）|
| `kfd_priv.h` | Stage 1.2 PoC stub | **扩展为真实声明**（Phase B.1.2）|
| `kfd_topology.h` | Stage 1.2 PoC stub | **扩展为真实声明**（Phase B.1.4）|
| `kfd_svm.h` | Stage 1.2 PoC stub | **扩展为真实声明**（Phase B.3.1）|
| `CMakeLists.txt` | 编译 kfd_queue.c | **扩展编译 6 个新模块**（Phase B 末）|

### 4.2 FIXME 清理清单（Phase D）

**`kfd_queue.c` Line 214**:
```c
/* FIXME: remove this function, just call amdgpu_bo_unref directly */
```
- **现状**: KFD PoC 阶段保留的 wrapper 函数
- **清理策略**: 调用方改为直接调 `amdgpu_bo_unref()`（依赖 libgpu_core `gpu_bo_unref`，ADR-020）
- **验证**: D.3 单元测试 + 集成测试

**`kfd_queue.c` Line 310**:
```c
/* FIXME: make a _locked version of this that can be called before ... */
```
- **现状**: KFD PoC 阶段缺失锁版本
- **清理策略**: 实现 `kfd_queue_*_locked()` 版本，使用 `pthread_mutex_t` 保护（C 文件不引入 STL，遵守 ADR-018 决策 3）
- **验证**: D.3 单元测试并发场景

### 4.3 kfd_sim_bridge 既有层

Stage 1.4 Tier-1 已创建 `plugins/gpu_driver/drv/kfd_sim_bridge.{h,cpp}`（5 handler entries + 3 test entries），C-12 保留并扩展：
- `kfd_sim_handle_map_memory()` → 由 `kfd_mmu.c::kfd_mmu_map()` 内部调用
- `kfd_sim_handle_unmap_memory()` → 同上
- 其它 3 个 handler 保持现状

---

## 5. 实施策略（5 个 Phase）

### 5.1 Phase A: 文档化（2 天）— **本子项目 Phase A.1 已完成**

- ✅ A.1 `docs/05-advanced/kfd-multi-file.md` 设计文档（本文件）
- ⏳ A.2 与 amdgpu KFD driver 公开 ABI 对比分析（独立 report）
- ⏳ A.3 决定子项目目录结构（已决定：`drv/kfd/` 子目录）
- ⏳ A.4 README.md 更新 "后续子项目" 段

### 5.2 Phase B: 模块切分（2 周）

详细任务见 [tasks.md §Phase B](../../openspec/changes/2026-08-15-stage1-4-kfd-multi-file-integration/tasks.md)：
- B.1 基础设施（module/pasid/process）+ 单元测试
- B.2 派发（dispatch 表）+ 单元测试
- B.3 内存（mmu）+ 集成 `sim_pm_*`
- B.4 事件（events）+ 集成 sim signal path

### 5.3 Phase C: Stage 1.4 Tier-2 deferred（2 周）

- **C.1 §3.2 IOMMU invalidation 真实化**：
  - 修 `plugins/gpu_driver/sim/sim_pfh_*` + `sim_pm_*` 真实 IOMMU
  - 实现 `IOTLB flush`（已有 commit `6a7f4ab` 基础，扩展覆盖 KFD MMU 路径）
  - 验证：`tests/test_iommu_emu_standalone` 覆盖 invalidation 路径

- **C.2 §3.3 mm_struct PID + VMA tracking**：
  - 修 `src/kernel/mm_shim.cpp` 加 PID + VMA 跟踪
  - 单元测试：`tests/test_mm_shim_standalone` 扩展

### 5.4 Phase D: FIXME 清理（3 天）

详见 §4.2 FIXME 清理清单。

### 5.5 Phase E: 集成 + E2E（2 周）

- E.1 完整 build 验证（85+ ctest 全绿）
- E.2 全套 ctest + **TaskRunner E2E**（可吸收 TADR-401 Entry 3b）
- E.3 docs 更新（kfd-portability-boundary.md v1.3 + post-refactor-architecture.md §1.10）
- E.4 PR + merge + 归档

---

## 6. 测试策略

### 6.1 单元测试（每个模块独立测试）

| 测试二进制 | 覆盖范围 | LOC 估算 |
|-----------|---------|---------:|
| `test_kfd_module_standalone` | module init/exit 生命周期 | ~100 |
| `test_kfd_process_standalone` | process aperture + GPU_IOCTL_GET_PROCESS_APERTURE | ~200 |
| `test_kfd_pasid_standalone` | PASID 分配 + 回收 | ~150 |
| `test_kfd_dispatch_standalone` | drm_ioctl_desc 派发正确性 | ~180 |
| `test_kfd_mmu_standalone` | map/unmap + HAL 桥接 | ~250 |
| `test_kfd_events_standalone` | event queue + signal path | ~180 |
| **总计** | **6 个新 standalone** | **~1060 LOC** |

### 6.2 集成测试（端到端）

| 测试名称 | 范围 |
|---------|------|
| `test_kfd_end_to_end_standalone` | 加载 plugin → open `/dev/kfd` → 5 KFD ioctl 全跑通 |
| `test_kfd_fault_handling_standalone` | page fault 触发 → sim_pfh_inject_fault → KFD event 通知 |
| `test_kfd_concurrent_processes_standalone` | 多进程 PID + VMA 隔离验证（mm_shim）|

### 6.3 回归基线

- **保持 86/86 ctest PASS**（Stage 2 baseline `fb75ed2`）
- **保持 318/318 TaskRunner tests PASS**（Phase 4 基线）
- **新增 ~30 ctest cases**（6 unit + 3 integration + 回归）
- **ASan/UBSan/TSan 三 sanitizer clean**

---

## 7. Cross-Repo 影响

### 7.1 TaskRunner 仓（`external/TaskRunner/`）

| 影响 | 风险等级 | 缓解 |
|------|---------|------|
| `test_cu_mem_pool` 真实 KFD ABI 验证 | 🟢 低 | KFD ABI 变更不影响 TaskRunner 测试（MockGpuDriver 隔离）|
| `libcuda_shim` 新增 cuKFD* 桥接（如需要）| 🟡 中 | 走 ADR-023 + tadr-301 流程 |
| **双赢机会**：Phase E.2 实现 TADR-401 Entry 3b | 🟢 低 | UsrLinuxEmu 端实装真实 L1↔L2 test |

### 7.2 同步协议（ADR-035 §Rule 5.1）

按 4-step 协议执行（仅当 KFD ABI 变更时）：

1. TaskRunner 仓: PR + merge to main
2. UsrLinuxEmu 仓: bump submodule pointer
3. 如新增 TADR: 更新 `docs/00_adr/README.md` TaskRunner TADR mirror 表
4. 跨仓验证: ctest + docs-audit.sh 双绿

### 7.3 同步检查清单（每次 commit）

```bash
# 1. 检查 submodule 指针变更
cd /workspace/project/UsrLinuxEmu && git status -s external/TaskRunner

# 2. 检查 ADR-035 INDEX 是否需新增
grep -A 15 "TaskRunner TADR" docs/00_adr/README.md

# 3. 检查 openspec/ changes 是否有跨仓关联
cd /workspace/project/UsrLinuxEmu && git diff main HEAD --stat | grep external/TaskRunner
```

---

## 8. 风险与缓解

| 风险 | 概率 | 影响 | 缓解措施 |
|------|------|------|----------|
| KFD 代码量 ~50K LOC，scope 大 | 🟡 高 | 🟡 高 | Phase A 文档化先行；B.1-B.4 每个模块单测先行；按 Linux 6.12 LTS 增量 |
| 内核 API 覆盖不全 | 🟡 高 | 🟡 中 | ADR-027 spec-driven 原则：按 KFD 实际需要增量补 linux_compat/* |
| 真机部署兼容 | 🟡 中 | 🟡 中 | HAL 是桥（ADR-036），`hal_user.cpp` 持续维护 |
| IOMMU 子系统理解偏差 | 🟡 中 | 🟡 中 | 参考 Linux 6.6/6.12 LTS 头文件；ADR-027 决策 3（不承诺 ABI 一致，只对齐 API 签名）|
| mmu_notifier 路径复杂 | 🟡 中 | 🟡 高 | Phase C.2 内部做小范围 PoC（userfaultfd + mmap 共享触发）|
| HAL ops 扩展触发新 ADR | 🟢 低 | 🟢 低 | 按 ADR-023 + ADR-035 流程；spec-driven 避免过度设计 |
| 文档审计基线漂移 | 🟢 低 | 🟢 低 | pre-commit hook 自动跑 `tools/docs-audit.sh --strict` |

---

## 9. Acceptance Criteria（继承自 C-12 proposal.md + ADR-059）

### 9.1 功能验收

- [ ] 6 个新 KFD 模块全部编译通过（`cmake .. && make -j4` 0 errors）
- [ ] `kfd_queue.c` 2 个 FIXME 清理（line 214 + line 310）
- [ ] 5 个 KFD 核心 ioctl 端到端跑通（GET_PROCESS_APERTURE/CREATE_QUEUE/UPDATE_QUEUE/MAP_MEMORY/UNMAP_MEMORY）
- [ ] `drm_ioctl_desc[]` 派发表保持 ≥ 38 entries（含 5 KFD 0x40-0x47 已就位 + Stage 1.4 ~19 entries + Phase 3 stream/graph/mem_pool ~18 entries）。C-12 **不新增** dispatch entry，仅保持现有 entries + 完成 6 个新 KFD module 编译通过（如未来需要新增 ioctl，按 ADR-023 + ADR-035 流程单独走 ADR）。

### 9.2 测试验收

- [ ] 6 个新 standalone 单元测试二进制（test_kfd_module/process/pasid/dispatch/mmu/events）
- [ ] 3 个集成测试（test_kfd_end_to_end / fault_handling / concurrent_processes）
- [ ] **总 ctest 数 ≥ 116**（Stage 2 baseline 86 + 30 新增 ctests planned）
- [ ] TaskRunner E2E（Phase E.2 可吸收 TADR-401 Entry 3b）
- [ ] ASan/UBSan/TSan 三 sanitizer clean

### 9.3 架构验收

- [ ] KFD 代码严格在 `drv/kfd/` 子目录（无 ②→③ 直接调用）
- [ ] HAL 接口扩展走 ADR-023 + ADR-035 流程
- [ ] `libgpu_core/` 零修改（ADR-020 保持）
- [ ] 新增 HAL ops（如 `hal_iommu_*`）有 ADR 记录

### 9.4 文档验收

- [ ] 本文件（`docs/05-advanced/kfd-multi-file.md`）已创建
- [ ] ADR-059 已创建并 Accepted
- [ ] C-12 proposal.md 已补充 ADR 引用（修复治理缺陷）
- [ ] `docs/00_adr/README.md` 已更新索引
- [ ] `kfd-portability-boundary.md` v1.3 已更新（Tier-2 §3.2 §3.3 标注完成）
- [ ] `tools/docs-audit.sh --strict` 无 warning

### 9.5 跨仓验收

- [ ] TaskRunner tests 318/318 PASS（无回归）
- [ ] Issue #21/#22/#23（已关）后续不再 regress
- [ ] 跨仓同步协议（ADR-035 §Rule 5.1）已执行

---

## 10. 时间线与里程碑

```
Week 1  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ Phase A + Phase B.1
        │ ├─ A.1-A.4 文档化（已完成）       [2026-08-15]
        │ ├─ B.1.1 kfd_module              [Day 1-2]
        │ ├─ B.1.2 kfd_module.h            [Day 2]
        │ ├─ B.1.3 kfd_pasid               [Day 3-4]
        │ ├─ B.1.4 kfd_process             [Day 5-7]
        │ └─ B.1.5 单元测试                [并行]

Week 2  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ Phase B.2 + B.3
        │ ├─ B.2.1 kfd_dispatch            [Day 8-10]
        │ ├─ B.3.1 kfd_mmu                 [Day 11-14]
        │ └─ B.3.2 sim_pm_* 集成           [并行]

Week 3  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ Phase B.4 + Phase C.1
        │ ├─ B.4.1 kfd_events              [Day 15-17]
        │ ├─ B.4.2 sim signal path 集成    [并行]
        │ └─ C.1 §3.2 IOMMU invalidation    [Day 18-21]

Week 4  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ Phase C.2 + Phase D
        │ ├─ C.2 §3.3 mm_struct PID/VMA    [Day 22-25]
        │ ├─ C.2 单元测试                  [并行]
        │ ├─ D.1 FIXME line 214            [Day 26]
        │ └─ D.2 FIXME line 310            [Day 27]

Week 5-6 ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ Phase E
        │ ├─ E.1 完整 build 验证           [Week 5 Day 1-2]
        │ ├─ E.2 ctest + TaskRunner E2E    [Day 3-7]
        │ ├─ E.3 docs 更新                 [Day 8]
        │ └─ E.4 PR + merge + 归档         [Day 9-10]

Week 6-7 🏁 验收归档
        └─ [验收 + 归档 + ADR 升级]
```

**总工期**: 6-8 周（与 INDEX.md C-12 effort 估算一致）

---

## 11. 关联文档导航

| 类别 | 文档 |
|------|------|
| **架构 SSOT** | [post-refactor-architecture.md](../02_architecture/post-refactor-architecture.md), [kfd-portability-boundary.md](kfd-portability-boundary.md) |
| **Roadmap** | [stage-1-kernel-emu.md §1.4](../roadmap/stage-1-kernel-emu.md), [blueprint.md](../roadmap/blueprint.md) |
| **ADR** | [ADR-018](../00_adr/adr-018-driver-sim-separation.md), [ADR-023](../00_adr/adr-023-hal-interface.md), [ADR-035](../00_adr/adr-035-governance-policy.md), [ADR-036](../00_adr/adr-036-three-way-separation.md), [ADR-059](../00_adr/adr-059-kfd-multi-file-integration.md) |
| **变更记录** | [C-12 proposal.md](../../openspec/changes/2026-08-15-stage1-4-kfd-multi-file-integration/proposal.md), [C-12 tasks.md](../../openspec/changes/2026-08-15-stage1-4-kfd-multi-file-integration/tasks.md) |
| **TaskRunner 跨仓** | [tadr-301](../external/TaskRunner/docs/shared/adr/tadr-301-igpu-driver-contract.md), [tadr-305](../external/TaskRunner/docs/shared/adr/tadr-305-mempool-export-shareable.md), [TADR-401](../external/TaskRunner/docs/umd-evolution/adr/tadr-401-promote-umd-evolution-to-accepted.md) |
| **历史报告** | [kfd-portability-report.md](kfd-portability-report.md), [tier2-runtime-penetration-report.md](tier2-runtime-penetration-report.md) |

---

**维护者**: UsrLinuxEmu Architecture Team
**最后更新**: 2026-07-11（初版，C-12 Phase A.1）
**对应 commit**: pending（C-12 启动 commit）
**状态**: 📋 DESIGN（待 C-12 启动后升级为 ACTIVE）
