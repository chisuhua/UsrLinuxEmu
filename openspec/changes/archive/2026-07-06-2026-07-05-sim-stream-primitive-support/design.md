## Context

TaskRunner umd-evolution roadmap Phase 3（[`external/TaskRunner/docs/umd-evolution/roadmap/phase-3-deferred.md`](../../../external/TaskRunner/docs/umd-evolution/roadmap/phase-3-deferred.md)）的 **触发条件 1 已满足**：UsrLinuxEmu Stage 1.4（KFD portability Tier-1 + Tier-2 runtime penetration）于 2026-07-04 完成（commits `80f6a44` + `9378153`）。

TaskRunner 已发出 **跨仓协调请求** [`external/TaskRunner/docs/superpowers/cross-repo-prs/2026-07-05-phase3-1-stream-mempool-coordination.md`](../../../external/TaskRunner/docs/superpowers/cross-repo-prs/2026-07-05-phase3-1-stream-mempool-coordination.md)，要求 UsrLinuxEmu 提供 sim 原语支撑 Phase 3.1（Stream Capture + CUDA Graph）和 Phase 3.2（Memory Pool）的端到端验证。

本 change 设计围绕 **sim 原语 + IOCTL 编号 + GpuDriverClient 转发** 三件事展开。

### 现状盘点

| 模块 | 现有状态 | 缺失 |
|------|----------|------|
| sim 原语 | `sim_pfh_*`（Stage 1.3）/ `sim_pm_*`（Stage 1.3）| `sim_stream_*` / `sim_graph_*` / `sim_mem_pool_*` |
| IOCTL 编号 | 0x00-0x47 已使用 | 0x50-0x67 待预留 |
| GpuDriverClient | 31 个方法（H-2.5 + H-3）| **15 个方法**（Phase 3.1/3.2 — 10 graph/capture + 5 mempool）|
| 测试 | Stage 1.4 70+/70+ 通过 | 4 个新 sim standalone test binary + 1 个 KFD 派发测试 |
| KFD handler | 19 个 ioctl 派发表（Stage 1.4 Tier-1）| 新增 18 个 0x50-0x67 编号 |

### Oracle 评估 / 用户决策

> 本 change 涉及 sim 原语扩展（非 KFD 集成），相对低风险。关键决策点：
>
> | 决策 | 默认 | 理由 |
> |------|------|------|
> | sim 原语位置 | `plugins/gpu_driver/sim/`（与 `sim_pfh_*` 同级）| 风格一致 |
> | IOCTL 编号 | 0x50-0x67 顺序追加 | 不破坏现有 ABI |
> | GpuDriverClient 默认行为 | 转发到 sim 端 IOCTL | 与现有 14 个方法一致 |
> | 是否引入新 HAL op | 否 | sim 原语 + GpuDriverClient 转发，HAL 层无需扩展 |
> | 测试覆盖标准 | happy path + ≥1 error path per primitive | 与 Stage 1.4 Tier-2 一致 |

## Goals / Non-Goals

### Goals

1. **G1**: 3 个新 sim 原语模块（stream_capture / graph / mem_pool）完整实现，覆盖 TaskRunner Phase 3.1/3.2 全部所需接口
2. **G2**: 18 个新 IOCTL（0x50-0x67）在 `gpu_ioctl.h` 中定义，附完整 struct 定义
3. **G3**: GpuDriverClient 15 个新 forwarding 方法实现（在 TaskRunner 侧 `src/test_fixture/gpu_driver_client.cpp`，不在本 change scope — Oracle C2 修正），与 sim 原语一一映射
4. **G4**: 4 个新 sim standalone test binary + 1 个 KFD 派发测试，happy + error path 覆盖 ≥80%
5. **G5**: 现有 Stage 1.4 Tier-1/Tier-2 70+/70+ 回归测试零 regression（**每个 sim 原语添加后立即跑** G1-G4 契约测试 — Oracle M4 修正）
6. **G6**: 与 TaskRunner IGpuDriver **15-方法**扩展协调（默认 no-op + override 时转发）
7. **G7**: 边界契约 G1-G4 + Tier-2 runtime 路径不破坏
8. **G8**: ADR-015 IOCTL 编号表同步更新（补 0x44-0x47 + 0x50-0x67 + 标注 0x70-0x7F reserved — Oracle H3 修正）

### Non-Goals

1. **NG1**: 真实 CUDA Graph 执行（instantiate/launch 仅 record + fence 返回，不执行 DAG）
2. **NG2**: Memory pool 跨进程共享（`cuMemPoolExportToShareableHandle`）—— Phase 1.5+
3. **NG3**: Pool VA 范围回收策略的高级优化（release threshold 仅记录，不实际触发回收）
4. **NG4**: 新 HAL op 引入（沿用现有 HAL 11 函数指针表）
5. **NG5**: 多设备 pool（Phase 3.5 多设备范围）

## Decisions

### Decision 1: sim 原语位置（`plugins/gpu_driver/sim/` 选项 A）

**背景**: Stage 1.3 引入了 `sim_pfh_*` 和 `sim_pm_*`，均位于 `plugins/gpu_driver/sim/`，extern "C" 风格。
**方案**: 本 change 在同一目录新增 3 个原语（`sim_stream_*` / `sim_graph_*` / `sim_mem_pool_*`），保持 C 链接风格。
**何时创建**: 实施阶段（worktree 内），不在 change 启动阶段。
**理由**: 与现有 sim 原语风格一致，降低认知负担；CMake 增量更新简单（target_sources 即可）。

### Decision 2: IOCTL 编号（0x50-0x67 顺序追加）

**背景**: 现有 IOCTL 编号 0x00-0x47 已分配（VA Space 0x30-0x32，Queue 0x40-0x47）。
**方案**: 本 change 预留 0x50-0x67 范围，依次分配给 graph/capture（0x50-0x59）和 mempool（0x60-0x67）。
**理由**:
- 不修改现有编号（ABI 兼容）
- 预留范围（0x50-0x7F 共 48 个）足以覆盖 Phase 3 全部扩展
- 编号语义按功能分组（capture → graph → mempool），便于文档查阅

### Decision 3: GpuDriverClient 默认行为（直接转发到 IOCTL）

**背景**: 现有 GpuDriverClient 已实现 31 个方法，每个方法都是薄 wrapper（ioctl 系统调用）。
**方案**（**澄清 — 本决策不在本 change scope**）：原提案说"15 个新方法"，但这些方法在 **TaskRunner 侧** GpuDriverClient 实现，不在 UsrLinuxEmu。本 change 仅在 UsrLinuxEmu 侧提供 sim 原语 + IOCTL handler + 18 个 IOCTL #define。GpuDriverClient forwarding 在 TaskRunner 跨仓 PR §3.1.3 + §3.2.3 中实现（Step 3）。
**理由**: 与现有实现一致；不引入新的抽象层；sim 原语由 sim 端维护。

### Decision 4: 不引入新 HAL op

**背景**: Stage 1.4 Tier-2 决策 2：HAL ops 严格走 ADR 流程，不"顺手"添加。
**方案**: 本 change 不新增 HAL 函数指针表条目；新 sim 原语直接由 GpuDriverClient 转发到 IOCTL。
**理由**: 现有 HAL 11 函数指针表覆盖所有 GPU 驱动操作场景；新增原语属于"算法核心"层，不属于"驱动 ↔ 硬件"桥接层。
**审计**: PR review 时检查"是否新增了非 HAL 的 HAL op"。

### Decision 5: 测试覆盖标准（happy + ≥1 error path per primitive）

**背景**: Stage 1.4 Tier-2 测试覆盖遵循相同模式。
**方案**（**澄清 — 本决策不在本 change scope**）：每个 sim 原语至少 1 个 happy path + 1 个 error path。**GpuDriverClient forwarding 集成测试** 覆盖 15 个方法，由 TaskRunner 侧在 Step 3 阶段提供（不在本 change scope）。

### 本 change 测试 case 总数（NP1-1 修正）

| 测试 binary | Case 数 | scope |
|-------------|---------|-------|
| `test_sim_stream_capture_standalone` | ≥6 | 本 change |
| `test_sim_graph_standalone` | ≥12 | 本 change |
| `test_sim_mem_pool_standalone` | ≥11 | 本 change |
| `test_kfd_portability_phase31_standalone` | 18 | 本 change |
| **本 change 总计** | **≥47** | — |
| `test_gpu_driver_client_phase31_standalone` | 15 | TaskRunner 侧（不在本 change scope）|
| **端到端总计** | **≥62** | — |

### Decision 6: 回归测试零容忍

**背景**: Stage 1.4 Tier-2 完成后 Tier-1 全部测试 + G1-G4 边界契约测试均无 regression。
**方案**: 本 change 实施时每日跑 `tests/` 下全部 standalone test binary，任何 regression 立即修复。
**回归失败处理**: 回归测试失败 → 优先修复本 change 的 regression → 暂缓后续 sim 原语添加。

## 详细技术设计

### Thread Safety (Fix-6 新增)

**决策**：本 change **不**引入线程安全保证。

**理由**：
- UsrLinuxEmu 当前所有 GPU 路径均为单线程（`gpu_drm_driver.cpp` 由 DRM 主循环串行调用）
- TaskRunner GpuDriverClient 调用方由 `cmd_cuda.cpp` 串行调用
- Stage 1.4 Tier-2 已验证：现有 73/73 测试均为单线程

**全局表**：
- `StreamCaptureTable` / `GraphTable` / `ExecTable` / `PoolTable` 均为进程级单例
- **未加锁**，依赖单线程调用保证

**例外**：`sim_fence_id_alloc()` 使用 `std::atomic<uint64_t>` 保证 fence_id 分配的原子性（即使单线程调用，atomic 也保证未来扩展安全性）。

**未来扩展**（如需多线程）：
- 加 `std::mutex table_mutex_` 保护每个表
- 或改为 thread_local（每个线程独立表）

**测试覆盖**：当前测试框架（doctest + Catch2）均为单线程，无需覆盖。

### 命名规范 (Fix-13)

**sim 原语**：`sim_<feature>_<verb>`
- `sim_pfh_*` = page fault handler（Stage 1.3）
- `sim_pm_*` = page migration（Stage 1.3）
- `sim_stream_*` = stream（本 change）
- `sim_graph_*` = graph（本 change）
- `sim_mem_pool_*` = memory pool（本 change）

**handle 类型**：`sim_<feature>_handle_t`（typedef `uint64_t`）
- 例：`sim_graph_handle_t`, `sim_graph_exec_handle_t`, `sim_pool_handle_t`

**错误码**：`SIM_<FEATURE>_ERR_*` 大写宏
- 例：`SIM_POOL_ERR_OK`, `SIM_POOL_ERR_NOSPC`

### 返回值约定 (Fix-9)

| 原语 | C 返回类型 | 语义 |
|------|-----------|------|
| `sim_stream_capture_*` | `int` | 0 = 成功, -1 = SIM_ERR_GENERIC |
| `sim_graph_create/destroy/add_*/instantiate/destroy_exec` | `int` | 0 = 成功, -1/-EINVAL/-ENOSYS = 错误 |
| `sim_graph_launch` | `int64_t` | **失败**：负值 errno；**成功**：fence_id（≥ 1<<32）|
| `sim_mem_pool_create/destroy/alloc/set_attr/get_attr/trim` | `int` | 0 = 成功, 负值 = SIM_POOL_ERR_* |
| `sim_mem_pool_alloc_async` | `int64_t` | 失败：负值 errno；成功：fence_id |
| `sim_mem_pool_free_async` | `int64_t` | 失败：负值 errno；成功：fence_id |

**caller 必检**：所有 `int64_t` 返回值 SHALL 检查：
- < 0 → 错误
- ≥ (1 << 32) → 合法 fence_id

### sim 原语接口（C 链接）

#### sim_stream_capture.h

```c
// Stream Capture 状态机
typedef enum {
  SIM_STREAM_CAPTURE_NONE = 0,
  SIM_STREAM_CAPTURE_ACTIVE,
  SIM_STREAM_CAPTURE_INVALID
} sim_stream_capture_status_t;

// Begin capture on stream
int sim_stream_capture_begin(uint32_t stream_id, uint32_t mode);

// End capture, return graph handle
int sim_stream_capture_end(uint32_t stream_id, uint64_t* graph_handle_out);

// Query capture status
int sim_stream_capture_status(uint32_t stream_id,
                              sim_stream_capture_status_t* status_out);
```

#### sim_graph.h

```c
// Node types
typedef enum {
  SIM_GRAPH_NODE_KERNEL = 1,
  SIM_GRAPH_NODE_MEMCPY = 2
} sim_graph_node_type_t;

// Create empty graph
int sim_graph_create(uint64_t* graph_handle_out);

// Destroy graph
int sim_graph_destroy(uint64_t graph_handle);

// Add kernel node (record metadata)
int sim_graph_add_kernel_node(uint64_t graph_handle,
                               uint32_t kernel_index,
                               uint32_t grid_x, uint32_t grid_y, uint32_t grid_z,
                               uint32_t block_x, uint32_t block_y, uint32_t block_z,
                               uint64_t* kernargs_bo_handle);

// Add memcpy node
int sim_graph_add_memcpy_node(uint64_t graph_handle,
                               uint64_t src_va, uint64_t dst_va, uint64_t size,
                               int is_h2d);

// Instantiate graph (validation only)
int sim_graph_instantiate(uint64_t graph_handle, uint64_t* exec_handle_out);

// Launch graph (no-op, returns fence)
int64_t sim_graph_launch(uint64_t exec_handle, uint32_t stream_id);

// Destroy executable
int sim_graph_destroy_exec(uint64_t exec_handle);
```

#### sim_mem_pool.h (Fix-2 修订)

```c
// Pool properties (修订：va_subrange 由 pool 创建时分配)
typedef struct {
  uint64_t va_space_handle;  // 归属 VA Space
  uint64_t size;             // 池总大小（字节）
  uint64_t va_base;          // pool VA 子范围起始（pool 创建时分配，Fix-2 新增）
  uint64_t va_limit;         // pool VA 子范围结束 = va_base + size（Fix-2 新增）
  uint32_t flags;            // GPU_MEM_POOL_* (CU_MEMPOOL_*)
} sim_mem_pool_props_t;

// Pool attributes
typedef enum {
  SIM_MEM_POOL_ATTR_RELEASE_THRESHOLD = 1,
  SIM_MEM_POOL_ATTR_REUSE_FOLLOW_EVENT_DEPENDENCIES = 2
} sim_mem_pool_attr_t;

// 错误码扩展（Fix-2 新增）
#define SIM_POOL_ERR_OK              0
#define SIM_POOL_ERR_INVALID_HANDLE -1   // pool handle 无效
#define SIM_POOL_ERR_NOSPC          -2   // pool 容量不足（Scenario 3.6）
#define SIM_POOL_ERR_INVAL          -3   // 参数无效
#define SIM_POOL_ERR_NOT_SUPPORTED  -4   // 不支持的 attr

// Pool 内部 bookkeeping (C++ 内部使用，对外透明)
struct PoolInternalEntry {
  uint64_t va;               // 已分配 VA
  uint64_t size;             // 已分配大小
  uint64_t bo_handle;        // 对应 BO handle
};

// Pool 全局表项 (C++ 侧)
struct PoolTableEntry {
  sim_mem_pool_props_t props;
  std::map<uint64_t, PoolInternalEntry> allocated;  // VA → entry
  uint64_t next_va_hint;     // 下一次分配搜索起点
};

// Create pool
int sim_mem_pool_create(const sim_mem_pool_props_t* props, uint64_t* pool_handle_out);

// Destroy pool
int sim_mem_pool_destroy(uint64_t pool_handle);

// Sync alloc
int sim_mem_pool_alloc(uint64_t pool_handle, uint64_t size, uint64_t* va_out);

// Async alloc (returns fence)
int64_t sim_mem_pool_alloc_async(uint64_t pool_handle, uint64_t size,
                                  uint32_t stream_id, uint64_t* va_out);

// Async free (returns fence)
int64_t sim_mem_pool_free_async(uint64_t va, uint32_t stream_id);

// Set/get attribute
int sim_mem_pool_set_attr(uint64_t pool_handle, sim_mem_pool_attr_t attr,
                           const void* value, size_t value_size);
int sim_mem_pool_get_attr(uint64_t pool_handle, sim_mem_pool_attr_t attr,
                           void* value_out, size_t value_size);

// Trim pool to retain min_bytes
int sim_mem_pool_trim(uint64_t pool_handle, uint64_t min_bytes);
```

### IOCTL 结构体（`plugins/gpu_driver/shared/gpu_ioctl.h`）

```c
/* Stream Capture */
struct gpu_stream_capture_args {
  uint32_t stream_id;
  uint32_t mode;
  uint64_t graph_handle_out;  /* for END */
};

struct gpu_stream_capture_status_args {
  uint32_t stream_id;
  uint32_t status_out;        /* SIM_STREAM_CAPTURE_* */
};

/* Graph */
struct gpu_graph_create_args {
  uint64_t graph_handle_out;
};

struct gpu_graph_add_kernel_node_args {
  uint64_t graph_handle;
  uint32_t kernel_index;
  uint32_t grid_x, grid_y, grid_z;
  uint32_t block_x, block_y, block_z;
  uint64_t kernargs_bo_handle;
};

struct gpu_graph_add_memcpy_node_args {
  uint64_t graph_handle;
  uint64_t src_va, dst_va, size;
  uint32_t is_h2d;
};

struct gpu_graph_instantiate_args {
  uint64_t graph_handle;
  uint64_t exec_handle_out;
};

struct gpu_graph_launch_args {
  uint64_t exec_handle;
  uint32_t stream_id;
  int64_t  fence_id_out;       /* returns fence_id (>=1), MUST be _IOWR */
};

/* Missing structs (Oracle C3 — required for full IOCTL coverage) */

struct gpu_graph_destroy_args {
  uint64_t graph_handle;        /* _IOW — scalar arg sufficient */
};

struct gpu_graph_destroy_exec_args {
  uint64_t graph_exec_handle;   /* _IOW — scalar arg sufficient */
};

struct gpu_mem_pool_destroy_args {
  uint64_t pool_handle;         /* _IOW — scalar arg sufficient */
};

struct gpu_mem_pool_free_async_args {
  uint64_t va;                  /* VA to free */
  uint32_t stream_id;
  int64_t  fence_id_out;        /* returns fence_id (>=1), MUST be _IOWR */
};

struct gpu_mem_pool_trim_args {
  uint64_t pool_handle;
  uint64_t min_bytes;           /* retain at least this many bytes */
};

/* Memory Pool */
struct gpu_mem_pool_props {
  uint64_t va_space_handle;
  uint64_t size;
  uint32_t flags;
};

struct gpu_mem_pool_create_args {
  struct gpu_mem_pool_props props;
  uint64_t pool_handle_out;
};

struct gpu_mem_pool_alloc_args {
  uint64_t pool_handle;
  uint64_t size;
  uint64_t va_out;
};

struct gpu_mem_pool_alloc_async_args {
  uint64_t pool_handle;
  uint64_t size;
  uint32_t stream_id;
  uint64_t va_out;
  int64_t  fence_id_out;       /* returns fence_id (>=1) */
};

struct gpu_mem_pool_attr_args {  /* Fix-7: 布局文档化 */
  uint64_t pool_handle;
  uint32_t attr;                 /* SIM_MEM_POOL_ATTR_* */
  uint32_t _reserved;            /* 对齐填充，必须为 0 */
  uint64_t value[4];             /* 32 字节 in/out blob */
};

/* 字段布局映射（set_attr / get_attr）：
 *
 * attr == SIM_MEM_POOL_ATTR_RELEASE_THRESHOLD (1):
 *   - value[0]: uint64_t release_threshold（字节数）
 *   - value[1..3]: 保留（必须为 0）
 *   - value_size 必须 == 8
 *
 * attr == SIM_MEM_POOL_ATTR_REUSE_FOLLOW_EVENT_DEPENDENCIES (2):
 *   - value[0]: uint32_t enable（0 = false, 1 = true）
 *   - value[1..3]: 保留（必须为 0）
 *   - value_size 必须 == 4
 *
 * 错误：value_size > 32 → 返回 -EINVAL
 * 错误：未识别的 attr → 返回 -ENOSYS
 */
```

### IOCTL Directions Table（Oracle H2 — 避免 `_IOW` 误用导致静默数据丢失）

每个 IOCTL 必须**显式指定方向**，确保用户空间到内核空间的数据流向正确：

| IOCTL 编号 | 名称 | 方向 | 输出字段 | 错误后果（方向错时）|
|------------|------|------|----------|-------------------|
| 0x50 | `STREAM_CAPTURE_BEGIN` | `_IOW` | (none) | — |
| 0x51 | `STREAM_CAPTURE_END` | **`_IOWR`** | `graph_handle_out` | graph_handle 始终为 0 → shim 无法追踪 graph |
| 0x52 | `STREAM_CAPTURE_STATUS` | **`_IOWR`** | `status_out` | status 始终为 NONE → capture 状态丢失 |
| 0x53 | `GRAPH_CREATE` | **`_IOWR`** | `graph_handle_out` | graph_handle 始终为 0 → 无法创建 graph |
| 0x54 | `GRAPH_DESTROY` | `_IOW` | (none) | — |
| 0x55 | `GRAPH_ADD_KERNEL_NODE` | `_IOW` | (none) | — |
| 0x56 | `GRAPH_ADD_MEMCPY_NODE` | `_IOW` | (none) | — |
| 0x57 | `GRAPH_INSTANTIATE` | **`_IOWR`** | `exec_handle_out` | exec_handle 始终为 0 → 无法 launch graph |
| 0x58 | `GRAPH_LAUNCH` | **`_IOWR`** | **`fence_id_out`** | fence_id 始终为 -1 → 异步 launch 失效 |
| 0x59 | `GRAPH_DESTROY_EXEC` | `_IOW` | (none) | — |
| 0x60 | `MEM_POOL_CREATE` | **`_IOWR`** | `pool_handle_out` | pool_handle 始终为 0 |
| 0x61 | `MEM_POOL_DESTROY` | `_IOW` | (none) | — |
| 0x62 | `MEM_POOL_ALLOC` | **`_IOWR`** | `va_out` | VA 始终为 NULL |
| 0x63 | `MEM_POOL_ALLOC_ASYNC` | **`_IOWR`** | `va_out`, `fence_id_out` | 异步分配失效 |
| 0x64 | `MEM_POOL_FREE_ASYNC` | **`_IOWR`** | `fence_id_out` | 异步 free 失效 |
| 0x65 | `MEM_POOL_SET_ATTR` | `_IOW` | (none) | — |
| 0x66 | `MEM_POOL_GET_ATTR` | **`_IOWR`** | `value_out` | 属性值始终为 0 |
| 0x67 | `MEM_POOL_TRIM` | `_IOW` | (none) | — |

> **实施验证**：本表所有 `_IOWR` IOCTL 必须在 `gpu_drm_driver.cpp` handler 中检查 args 字段在 ioctl 返回后被正确填充，并写入 E2E 测试覆盖此行为。

### fence_id Lifecycle（Oracle H4 — sim 层与 driver 层 fence 协调）

**问题**：现有 `sim_pfh_*` / `sim_pm_*` 原语不涉及 fence。fence_id 由 `gpu_drm_driver.cpp` driver 层管理（通过 `hal_fence_create()` 在 HAL 层分配）。新原语 `sim_graph_launch` / `sim_mem_pool_alloc_async` / `sim_mem_pool_free_async` 返回 fence_id，意味着 sim 层也生成 fence_id。

**解决方案（Fix-1 / Oracle H4 已决策 — Option A 最小侵入式）**：

1. **保留 HAL fence_id 不变**：
   - 现有 `hal_fence_create()` (HAL 层) 继续作为 driver 层 fence_id 分配点
   - driver 层 fence_id 范围保持不变：**`[1, (1 << 32) - 1]`**
   - 现有 70+ Stage 1.4 测试不修改，全部继续通过

2. **新增 sim 层 fence_id 独立分配（Fix-1 修订）**：
   - 新建 `plugins/gpu_driver/sim/fence_id.h` + `fence_id.cpp`
   - 提供 `int64_t sim_fence_id_alloc()` 函数（C 链接，extern "C"）
   - sim 层 fence_id 范围：**`[(1 << 32), INT64_MAX]`**（避开 driver 层 `[1, 1<<32-1]`）
   - 三处返回 fence_id 的 sim 原语（`sim_graph_launch` / `sim_mem_pool_alloc_async` / `sim_mem_pool_free_async`）调 `sim_fence_id_alloc()` 拿 ID

3. **`GPU_IOCTL_WAIT_FENCE` handler 分发决策**（在 `gpu_drm_driver.cpp` 中实施）：
   ```c
   long gpu_ioctl_wait_fence(struct drm_device* dev, void* data, struct drm_file*) {
     auto* args = static_cast<struct gpu_wait_fence_args*>(data);
     uint64_t fence_id = args->fence_id;
     bool signaled = false;

     if (fence_id < (1ULL << 32)) {
       // driver 层 fence：调 HAL
       int ret = hal_fence_read(self->hal_, fence_id, &signaled);
       if (ret < 0) return ret;
     } else {
       // sim 层 fence：调新 sim_fence_id_check()
       int ret = sim_fence_id_check(fence_id, &signaled);
       if (ret < 0) return ret;
     }

     if (signaled) return 0;
     // ... 阻塞等待逻辑（与现有 fence wait 一致）
   }
   ```

4. **fence 触发时机**（与 Phase 2 一致）：
   - sim 层原语返回 fence_id 时，**不**立即触发 fence
   - fence 触发由 `GpuQueueEmu::submit()` 路径完成（与 Phase 2 一致）
   - sim 层仅承诺"未来某个时刻此 fence 会被触发"

5. **测试覆盖**（Fix-1 新增）：
   - `tests/test_fence_id_lifecycle_standalone.cpp`（≥6 cases，详见 tasks.md §5.6）

### fence_id Lifecycle Migration Plan（Fix-1 补充 — UsrLinuxEmu 实施路径）

**Step 1**: 新建 `plugins/gpu_driver/sim/fence_id.h`：
```c
#ifndef SIM_FENCE_ID_H
#define SIM_FENCE_ID_H
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// 分配下一个 sim 层 fence_id（范围 [(1<<32), INT64_MAX]）
// 返回值：>= (1<<32) 有效；< 0 错误（当前不会失败，预留）
int64_t sim_fence_id_alloc(void);

// 查询 sim 层 fence 是否已触发
// 返回值：0 = 已查询（signaled 写入），< 0 = 错误
int sim_fence_id_check(uint64_t fence_id, bool* signaled);

// 触发 sim 层 fence（由 submit_batch / submit_memcpy 路径调用）
void sim_fence_id_signal(uint64_t fence_id);

// 范围常量
#define SIM_FENCE_ID_BASE  (1ULL << 32)
#define SIM_FENCE_ID_MAX   INT64_MAX

#ifdef __cplusplus
}
#endif

#endif /* SIM_FENCE_ID_H */
```

**Step 2**: 新建 `plugins/gpu_driver/sim/fence_id.cpp`（C++ 实现，原子计数器 + 条件变量）：
- `std::atomic<uint64_t> next_sim_fence_id_{SIM_FENCE_ID_BASE}`
- `std::map<uint64_t, std::promise<bool>> sim_fence_table_`（driver-side helper）
- 线程安全（即使 §Thread Safety 章节说单线程，fence_id 分配本身必须原子）

**Step 3**: 在 `gpu_drm_driver.cpp` 修改 `gpu_ioctl_wait_fence` handler（按上面分发逻辑）

**Step 4**: 在 `gpu_ioctl_submit_batch` / `gpu_ioctl_submit_memcpy` 完成后调 `sim_fence_id_signal()`（如有 sim fence 引用）

**Step 5**: CMake 更新（`plugins/gpu_driver/sim/CMakeLists.txt` 添加 fence_id.cpp）

**兼容性保证**：
- 现有 driver 层 fence_id 范围 `[1, 1<<32-1]` 不变
- 现有 `test_gpu_fence_return_standalone` 等 70+ 测试不动
- 仅新增 sim 层 fence_id 路径 + wait_fence 分发逻辑

### Capture Mode 枚举（Oracle H5 — 避免 mode 参数语义未定义）

**问题**：`sim_stream_capture_begin(stream_id, mode)` 的 `mode` 参数对应 CUDA 的 `CUstreamCaptureMode`，但 sim 头文件未定义对应枚举。

**解决方案**：在 `sim/stream_capture.h` 中定义 `sim_capture_mode_t`：

```c
typedef enum {
  SIM_CAPTURE_MODE_GLOBAL         = 0,  /* CUDA CU_STREAM_CAPTURE_MODE_GLOBAL */
  SIM_CAPTURE_MODE_THREAD_LOCAL   = 1,  /* CUDA CU_STREAM_CAPTURE_MODE_THREAD_LOCAL */
  SIM_CAPTURE_MODE_RELAXED        = 2,  /* CUDA CU_STREAM_CAPTURE_MODE_RELAXED */
} sim_capture_mode_t;
```

Phase 3.1 初期**只识别**`SIM_CAPTURE_MODE_GLOBAL`（其他两种返回 `EINVAL`），后续 Phase 3.x 按需扩展 thread-local / relaxed 语义。

### GpuDriverClient forwarding 实现模板（**本节澄清 — 不在本 change scope**）

```cpp
// stream_capture
int GpuDriverClient::stream_capture_begin(uint32_t stream_id, uint32_t mode) {
  if (!is_open()) return -1;
  struct gpu_stream_capture_args args = {stream_id, mode, 0};
  if (ioctl(fd_, GPU_IOCTL_STREAM_CAPTURE_BEGIN, &args) < 0) return -1;
  return 0;
}

int GpuDriverClient::stream_capture_end(uint32_t stream_id, uint64_t* graph_handle_out) {
  if (!is_open() || !graph_handle_out) return -1;
  struct gpu_stream_capture_args args = {stream_id, 0, 0};
  if (ioctl(fd_, GPU_IOCTL_STREAM_CAPTURE_END, &args) < 0) return -1;
  *graph_handle_out = args.graph_handle_out;
  return 0;
}

// mem_pool
int GpuDriverClient::mem_pool_create(uint64_t va_space_handle, uint64_t size,
                                      uint32_t flags, uint64_t* pool_handle_out) {
  if (!is_open() || !pool_handle_out) return -1;
  struct gpu_mem_pool_create_args args = {{va_space_handle, size, flags}, 0};
  if (ioctl(fd_, GPU_IOCTL_MEM_POOL_CREATE, &args) < 0) return -1;
  *pool_handle_out = args.pool_handle_out;
  return 0;
}
```

### 与现有 sim 原语的集成（Fix-3 修订）

**GpuQueueEmu 实际 API**（基于 `plugins/gpu_driver/sim/gpu_queue_emu.h:113`）：

```c
class GpuQueueEmu {
  // 仅一个入口方法（Fix-3 验证：仅 submit 存在，submit_batch / enqueue 不存在）
  int submit(uint64_t gpfifo_addr, uint32_t entry_count);
  // ...
};
```

**集成策略（Fix-3 修订）**：

| 新原语 | 集成方式 | 是否需新增 GpuQueueEmu API |
|--------|---------|---------------------------|
| `sim_graph_launch` | 1. driver handler 把 `exec_handle + stream_id` 转 `gpfifo_addr + entry_count`<br>2. 调 `GpuQueueEmu::submit(gpfifo_addr, entry_count)` 转发到硬件模拟器<br>3. **不**直接构造 gpfifo（由 sim 层将 graph node metadata 转为 gpfifo entries） | ✗ 复用 |
| `sim_stream_capture_*` | capture mode 时**不**调 `GpuQueueEmu::submit`，仅记录到 graph metadata；end 时才提交 | ✗ 复用 |
| `sim_mem_pool_alloc` | 调 `libgpu_core/gpu_buddy::gpu_buddy_alloc()`，**不**走 GpuQueueEmu | ✗ 复用 |
| `sim_mem_pool_alloc_async` / `sim_mem_pool_free_async` | driver handler 转 `submit_memcpy` IOCTL → 内部调 `GpuQueueEmu::submit()` + fence 返回 | ✗ 复用 |

**结论**：本 change **不修改** `GpuQueueEmu` 类定义，仅复用现有 `submit(uint64_t, uint32_t)` 方法。

### Pool VA 分配算法 (Fix-2 新增)

**目标**：在 pool 的 VA 子范围 `[va_base, va_limit)` 内分配 BO，保证不超出 pool.size。

```c
// sim_mem_pool_alloc 主流程
int sim_mem_pool_alloc(uint64_t pool_handle, uint64_t size, uint64_t* va_out) {
  // 1. 验证 pool_handle 有效
  PoolTableEntry* pool = pool_table_.get(pool_handle);
  if (!pool) return SIM_POOL_ERR_INVALID_HANDLE;
  if (!va_out) return SIM_POOL_ERR_INVAL;

  // 2. 计算对齐后 size' = ALIGN(size, 4KB)
  uint64_t aligned_size = (size + 4095) & ~4095ULL;

  // 3. 在 pool.props.allocated 中线性扫描查找空闲区间
  //    使用 first-fit 策略，从 next_va_hint 开始
  uint64_t search_start = pool->next_va_hint;
  uint64_t found_va = find_free_range(pool, search_start, aligned_size);

  if (found_va == 0) {
    // 4. 未找到 → 返回 SIM_POOL_ERR_NOSPC（不调 alloc_bo）
    return SIM_POOL_ERR_NOSPC;
  }

  // 5. 调 alloc_bo 拿到 BO handle（注意：VA 由 pool 决定，非 buddy 决定）
  uint64_t bo_handle = alloc_bo_at_va(found_va, aligned_size, pool->props.flags);

  // 6. 在 allocated 中插入 entry
  pool->allocated[found_va] = {found_va, aligned_size, bo_handle};
  pool->next_va_hint = found_va + aligned_size;

  // 7. 返回 VA
  *va_out = found_va;
  return SIM_POOL_ERR_OK;
}
```

**复杂度**：O(n)，n = pool 内已分配 BO 数量。Phase 3.2 初期 n 较小（<100），可接受。

**未来优化**（NG3 范围外）：Phase 3.x 引入红黑树 + 区间合并，O(log n) + 自动合并相邻空闲块。

**与 Option A 区别**：Option A 让 `gpu_buddy_alloc` 直接分配，pool 仅记录"已分配 VA 列表"，无法强制 pool.size 上限；Option B 由 pool 控制 VA 范围，再调底层 buddy 实现 BO 分配。

### 与 GpuDriverClient 现有 31 方法的关系

| 现有方法 | 新方法复用 |
|----------|-----------|
| `submit_batch` | `sim_graph_launch` 复用 |
| `alloc_bo` | `sim_mem_pool_alloc` 复用 |
| `submit_memcpy` | `sim_mem_pool_alloc_async` / `sim_mem_pool_free_async` 复用 |
| `create_queue` | `sim_stream_capture_begin` 在指定 queue 上启动 capture |

## 风险与缓解

| 风险 | 概率 | 影响 | 缓解 |
|------|------|------|------|
| sim 原语与 Stage 1.4 Tier-2 runtime 冲突 | 中 | 中 | 复用现有 `sim_pfh_*` / `sim_pm_*` API；新原语只在其上层构建 |
| GpuDriverClient forwarding 增加 fd 系统调用次数 | 中 | 低 | `submit_graph` 单次调用 launch 整个 DAG（性能提升）|
| Pool 与 libgpu_core/gpu_buddy 集成复杂度 | 中 | 中 | Phase 3.2 初期仅实现"扁平 pool"（无释放回收策略），复用 alloc_bo 直接拿 BO |
| IOCTL 编号预留冲突 | 低 | 中 | 0x50-0x7F 范围足够（48 个），明确文档预留 |
| TaskRunner 侧 IGpuDriver **15-方法**扩展 vs UsrLinuxEmu 实施时序错配 | 中 | 高 | **4 步协调顺序**：Step 1 (TaskRunner IGpuDriver no-op) → Step 2 (UsrLinuxEmu sim + IOCTL，本 change) → Step 3 (TaskRunner GpuDriverClient forwarding) → Step 4 (submodule bump)。详见 §5 Step Sequence |

## Out of Scope（重申）

参见 [`proposal.md`](proposal.md) §"Out of Scope（显式排除）"段。