# Sim Primitives Reference (Stage 1.3 + Phase 3.1/3.2)

> **目的**: ③ 硬件模拟层对外 C-ABI 接口的速查手册
>
> **创建**: 2026-07-06 (Phase 3.1 + 3.2 实施时)
>
> **最后验证**: 2026-07-06 / commit 后续归档
>
> **权威来源**: 头文件本身 (`plugins/gpu_driver/sim/*.h`) — 本文档仅做导航与示例。
>
> **关联 SSOT**:
> - 3 区分架构: [ADR-036](../00_adr/adr-036-three-way-separation.md)
> - KFD 边界 v1.2: [kfd-portability-boundary.md §10](../05-advanced/kfd-portability-boundary.md)
> - IOCTL 编号表: [ADR-015](../00_adr/adr-015-gpu-ioctl-unification.md) + `plugins/gpu_driver/shared/gpu_ioctl.h`

---

## 1. Stage 1.3 UVM/HMM 引入的 10 个 sim 原语

### 1.1 `sim_pfh_*` — Page Fault Handler

头文件: `plugins/gpu_driver/sim/page_fault_handler.h` (3 个创建/销毁 + 5 个查询函数)

```c
struct sim_page_fault_handler *sim_pfh_create(struct mm_struct *mm);
void                           sim_pfh_destroy(struct sim_page_fault_handler *pfh);
int                            sim_pfh_get_fault_count(struct sim_page_fault_handler *pfh);
void                           sim_pfh_inject_fault(struct sim_page_fault_handler *pfh,
                                                    unsigned long addr,
                                                    unsigned long *pfn_out);
void                           sim_pfh_inject_fault_with_cause(...)
unsigned long                  sim_pfh_get_last_fault_addr(...);
int                            sim_pfh_get_last_fault_cause(...);
```

### 1.2 `sim_pm_*` — Page Migration

头文件: `plugins/gpu_driver/sim/page_migration.h` (5 个迁移 + 3 个查询函数)

---

## 2. Phase 3.1 新增 sim 原语

### 2.1 `sim_fence_id_*` — Sim 层 fence 分配器 (FIX-1 / Oracle H4)

头文件: `plugins/gpu_driver/sim/fence_id.h`

```c
int64_t sim_fence_id_alloc(void);            /* 返回 ≥ 1<<32 的 fence_id */
int     sim_fence_id_check(uint64_t, bool*); /* 检查是否触发 */
void    sim_fence_id_signal(uint64_t);       /* 触发 */
void    sim_fence_id_reset_for_test(void);
```

**范围不冲突**: driver 层 fence_id 范围 `[1, (1<<32)-1]`（由 `hal_fence_create()` 分配）；sim 层 fence_id 范围 `[(1<<32), INT64_MAX]`。`gpu_ioctl_wait_fence` handler 按 fence_id 值范围分发。

### 2.2 `sim_stream_capture_*` — Stream Capture State Machine

头文件: `plugins/gpu_driver/sim/stream_capture.h`

```c
int sim_stream_capture_begin(uint32_t stream_id, uint32_t mode);
int sim_stream_capture_end  (uint32_t stream_id, uint64_t* graph_handle_out);
int sim_stream_capture_status(uint32_t stream_id, sim_stream_capture_status_t*);
```

**state machine**:
- `NONE → begin() → ACTIVE`
- `ACTIVE → end() → NONE` + emits monotonic `graph_handle_out`
- `ACTIVE → begin() → INVALID` (Oracle P3-L1; double-begin)
- mode ≠ `SIM_CAPTURE_MODE_GLOBAL` returns `-EINVAL` (Fix-10)

### 2.3 `sim_graph_*` — CUDA Graph Metadata (Phase 3.1)

头文件: `plugins/gpu_driver/sim/graph.h`

7 函数: `sim_graph_create` / `sim_graph_destroy` / `sim_graph_add_kernel_node` /
`sim_graph_add_memcpy_node` / `sim_graph_instantiate` / `sim_graph_launch` /
`sim_graph_destroy_exec`.

**约束**:
- `instantiate` 验证所有 kernel node 的 `kernargs_bo_handle != 0`（PoC）
- `launch` 返回 sim fence_id (≥ 1<<32)，PoC 实现立即 signal
- 单线程调用，无锁

---

## 3. Phase 3.2 新增 sim 原语 (FIX-2 Option B - VA 子范围方案)

### 3.1 `sim_mem_pool_*` — Memory Pool

头文件: `plugins/gpu_driver/sim/mem_pool.h`

8 函数 (per Fix-3): `sim_mem_pool_create/destroy/alloc/alloc_async/free_async/set_attr/get_attr/trim`.

**返回约定** (Fix-9):
- int 函数: `SIM_POOL_ERR_*` (0 / -1/-2/-3/-4)
- int64_t 函数 (`alloc_async` / `free_async`): `< 0` 错误，`≥ 1<<32` 合法 fence_id

**算法**: `design.md §Pool VA 分配算法` — first-fit 4KB 对齐扫描。

**不修改**: `libgpu_core/gpu_buddy`（避免核心库污染 — Decision 4）。

---

## 4. 用法示例: 完整的 capture → graph → launch 流程

```c
#include "sim/stream_capture.h"
#include "sim/graph.h"
#include "sim/fence_id.h"

uint32_t stream_id = 0;
uint64_t graph_handle = 0;
uint64_t kernargs_bo = 1;  /* BO handle, 必须 != 0 */
uint64_t exec_handle = 0;

/* 1. 开始 capture */
sim_stream_capture_begin(stream_id, SIM_CAPTURE_MODE_GLOBAL);

/* 2. 模拟 GPU 操作进入 capture (Phase 3.1 PoC 不真正记录 — 由调用方直接调用 sim_graph_add_*_node 即可) */
/* (实际 CUDA 调用流程中, driver handler 在 ACTIVE 状态下记录到 graph metadata */

/* 3. 结束 capture, 拿到 graph_handle */
sim_stream_capture_end(stream_id, &graph_handle);

/* 4. 直接构造 graph (或复用 capture 自动记录的 graph) */
sim_graph_create(&graph_handle);  /* 已通过 end 拿到 handle, 此处只是示意 */
sim_graph_add_kernel_node(graph_handle, /*kernel_index=*/0,
                          /*grid=*/1,1,1, /*block=*/32,1,1,
                          &kernargs_bo);
sim_graph_instantiate(graph_handle, &exec_handle);

/* 5. Launch — 立即返回 sim fence_id (≥ 1<<32) */
int64_t fence = sim_graph_launch(exec_handle, stream_id);
bool signaled = false;
sim_fence_id_check((uint64_t)fence, &signaled);  /* signaled = true (PoC immediate) */
```

---

## 5. 错误码快查

| 模块 | 错误码宏 | 数值 | 含义 |
|------|---------|------|------|
| fence_id | (无) | -1 / 0 | check 返回 -1 越界 / 0 OK |
| stream_capture | (Linux errno) | -EINVAL | 非法模式 |
| graph | (Linux errno) | -EINVAL / -1 | 验证失败 / handle 未找到 |
| mem_pool | `SIM_POOL_ERR_OK` | 0 | 成功 |
| mem_pool | `SIM_POOL_ERR_INVALID_HANDLE` | -1 | handle 无效 |
| mem_pool | `SIM_POOL_ERR_NOSPC` | -2 | 池容量不足 |
| mem_pool | `SIM_POOL_ERR_INVAL` | -3 | 参数无效 |
| mem_pool | `SIM_POOL_ERR_NOT_SUPPORTED` | -4 | 未知 attr |

---

## 6. 相关测试 binary

| 测试 | 验证范围 |
|------|----------|
| `test_sim_stream_capture_standalone` | stream capture 9 cases |
| `test_sim_graph_standalone` | graph 14 cases |
| `test_sim_mem_pool_standalone` | mem_pool 14 cases |
| `test_fence_id_lifecycle_standalone` | 跨层 fence_id 8 cases |
| `test_kfd_portability_phase31_standalone` | 18 IOCTL #define 4 cases (编译期验证) |

合计 ≥49 cases (per design.md NP1-1: ≥47 required)。

---

**维护者**: UsrLinuxEmu sim layer maintainers
**最后更新**: 2026-07-06
