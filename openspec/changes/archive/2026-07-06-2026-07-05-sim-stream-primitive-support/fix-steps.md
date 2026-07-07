# Fix Steps: sim-stream-primitive-support

> **状态**: ✅ COMPLETED（2026-07-05，全部 14 项 fix 已应用；2026-07-08 转为 ACCEPTED 候选）
> **作者**: UsrLinuxEmu Architecture Team
> **适用范围**: `openspec/changes/2026-07-05-sim-stream-primitive-support/` 下的 4 个文件
> **关联审查**: 见审查报告（修复 3 HIGH + 8 MEDIUM + 5 LOW 共 16 项 → 14 项 fix 落地 + 2 项 Nice 留 Follow-ups）
> **关联反馈**: [`taskrunner-feedback.md`](taskrunner-feedback.md)
> **应用日期**: 2026-07-05（同步 TaskRunner 接收 ack 后 0 天）
> **TaskRunner 接收确认**: 2026-07-05 全部 11 项决议 ack

## 0. 修复优先级矩阵

| 优先级 | 项数 | 阻塞 merge | 阻塞实施 | 修复位置 |
|--------|------|-----------|----------|----------|
| 🔴 P0（必须） | 3 | ✓ | ✓ | UsrLinuxEmu 提案 |
| 🟡 P1（必须） | 6 | ✗ | ✓ | UsrLinuxEmu 提案 |
| 🟢 P2（建议） | 5 | ✗ | ✗ | UsrLinuxEmu 提案 |
| 🔵 P3（外部） | 4 | ✗ | 部分 | TaskRunner 反馈 |

**修复总工作量估算**: P0+P1 ≈ 4-6 小时编辑，P2 ≈ 2-3 小时编辑。

---

## 1. 🔴 P0 — 阻塞级修复（3 项，必须全部完成才能进入实施）

### Fix-1 (H1): 解决 fence_id 命名空间冲突

**问题回顾**: design.md 提出 SIM/DRIVER fence_id 范围划分方案，但实际所有 fence_id 由 `hal_fence_create()` 在 HAL 层分配（`plugins/gpu_driver/drv/gpu_drm_driver.cpp:212-218`），sim 层无 fence 概念。提案需明确迁移路径。

**修复步骤**:

**步骤 1.1** — 在 `design.md` 新增章节"§fence_id Lifecycle Migration Plan"（位置：在现有 §fence_id Lifecycle 之后，约 L363 之后追加）：

```markdown
### fence_id Lifecycle Migration Plan (Oracle H4 补充)

**决策**：本 change 采用 **Option A — 最小侵入式方案**。

**保留 HAL fence_id 不变**：
- 现有 `hal_fence_create()` (HAL 层) 继续作为 driver 层 fence_id 分配点
- driver 层 fence_id 范围保持不变（从 1 开始自增）

**新增 sim 层 fence_id 独立分配**：
- 新建 `plugins/gpu_driver/sim/fence_id.h` + `fence_id.cpp`
- 提供 `sim_fence_id_alloc()` 函数
- sim 层 fence_id 范围：`[1 << 32, INT64_MAX]`（避开 driver 层 1..1<<32-1）
- driver 层 fence_id 范围：`[1, (1 << 32) - 1]`（保持现有 70+ 测试通过）

**wait_fence 分发决策**（在 `gpu_ioctl_wait_fence` handler 中）：
- `fence_id < (1 << 32)` → 调用 `hal_fence_read()` 现有路径
- `fence_id >= (1 << 32)` → 调用 `sim_fence_id_check()` 新增路径
- 任一路径返回 signaled=true 即返回 0；否则阻塞等待

**测试覆盖**：新增 `tests/test_fence_id_lifecycle_standalone.cpp`（≥6 cases）：
1. sim 层 fence_id 分配单调递增
2. driver 层 fence_id 分配单调递增
3. 两层 fence_id 不冲突（unique 范围）
4. wait_fence(sim_fence_id) 正确等待 sim fence
5. wait_fence(driver_fence_id) 正确等待 driver fence
6. 跨层混合场景：sim fence + driver fence 顺序触发
```

**步骤 1.2** — 在 `tasks.md §5 测试覆盖` 中追加 task：

```markdown
### 5.6 fence_id lifecycle 测试 (Fix-1 补充)

- [ ] 5.6.1 新建 `tests/test_fence_id_lifecycle_standalone.cpp`（≥6 cases）
- [ ] 5.6.2 编辑 `plugins/gpu_driver/sim/CMakeLists.txt` 注册新 sim 库子目标（如未存在）
- [ ] 5.6.3 编辑 `tests/CMakeLists.txt` 注册新 test binary
- [ ] 5.6.4 验证：现有 70+ 测试全过（无 regression）
```

**步骤 1.3** — 在 `spec.md` 新增 Requirement：

```markdown
### REQ-9: fence_id 命名空间隔离

**SHALL**: sim 层 fence_id 与 driver 层 fence_id SHALL 分配在不相交的整数范围内，且 wait_fence SHALL 根据 fence_id 范围分发到对应处理路径。

#### Scenario 9.1: fence_id 范围划分
- sim 层 fence_id SHALL ≥ (1 << 32)
- driver 层 fence_id SHALL < (1 << 32)

#### Scenario 9.2: wait_fence 分发
- fence_id < (1 << 32) → 调 hal_fence_read
- fence_id >= (1 << 32) → 调 sim_fence_id_check
```

**步骤 1.4** — 验证命令：

```bash
# 验证 design.md 已包含迁移方案
grep -A 5 "Migration Plan" openspec/changes/2026-07-05-sim-stream-primitive-support/design.md
# 验证 tasks.md 已添加 §5.6
grep -A 3 "5.6 fence_id lifecycle" openspec/changes/2026-07-05-sim-stream-primitive-support/tasks.md
```

---

### Fix-2 (H2): 解决 Pool VA 范围架构空缺

**问题回顾**: tasks.md §1.6 列出"Pool VA 范围复用 vs 独立分配"待决策，但 proposal.md / design.md 给出模糊实现路径。`gpu_buddy_alloc()` 无 pool 边界概念。

**修复步骤**:

**步骤 2.1** — 在 `proposal.md §Capabilities` 替换模糊描述（位置：`sim-mem-pool` capability 段）：

```markdown
- `sim-mem-pool`: Memory pool 原语。
  - **决策（Fix-2）**：采用 **VA 子范围方案（Option B）**
  - pool 归属到 VA Space（通过 `va_space_handle`）
  - pool 创建时向 VA Space 申请一段连续 VA 子范围作为 pool 的 VA 区间
  - `sim_mem_pool_alloc` 在 pool VA 区间内调用 `alloc_bo`（libgpu_core/gpu_buddy）
  - **不**修改 libgpu_core/gpu_buddy（避免核心库污染）
  - pool 内部 bookkeeping：已分配 BO 列表 + 红黑树（按 VA 排序）
  - 超出 pool size 返回 -ENOSPC
```

**步骤 2.2** — 在 `design.md §sim_mem_pool.h` 段替换数据结构设计：

```markdown
#### sim_mem_pool.h (Fix-2 修订)

```c
// Pool properties (修订：va_subrange 由 pool 创建时分配)
typedef struct {
  uint64_t va_space_handle;  // 归属 VA Space
  uint64_t size;             // 池总大小（字节）
  uint64_t va_base;          // pool VA 子范围起始（pool 创建时分配）
  uint64_t va_limit;         // pool VA 子范围结束（va_base + size）
  uint32_t flags;            // GPU_MEM_POOL_*
} sim_mem_pool_props_t;

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

// 错误码扩展（Fix-2 新增）
#define SIM_POOL_ERR_OK              0
#define SIM_POOL_ERR_INVALID_HANDLE  -1   // pool handle 无效
#define SIM_POOL_ERR_NOSPC           -2   // pool 容量不足（Scenario 3.6）
#define SIM_POOL_ERR_INVAL           -3   // 参数无效
#define SIM_POOL_ERR_NOT_SUPPORTED   -4   // 不支持的 attr
```
```

**步骤 2.3** — 在 `design.md §详细技术设计` 后追加 "§Pool VA 分配算法 (Fix-2 新增)"：

```markdown
### Pool VA 分配算法 (Fix-2 新增)

```
sim_mem_pool_alloc(pool_handle, size, &va_out):
  1. 验证 pool_handle 有效
  2. 计算对齐后 size' = ALIGN(size, 4KB)
  3. 在 pool.props.allocated 中线性扫描查找 [va_base, va_limit) 内空闲区间
     - 起始搜索位置：pool.next_va_hint
     - 使用 first-fit 策略
  4. 若找到空闲区间：
     - 调 alloc_bo(size', pool.props.flags) 拿到 BO handle
     - 在 allocated 中插入 entry
     - 更新 next_va_hint = va + size'
     - 返回 va_out = entry.va
  5. 若未找到（剩余空间 < size'）：
     - 返回 SIM_POOL_ERR_NOSPC
```

**复杂度**：O(n)，n = pool 内已分配 BO 数量。Phase 3.2 初期 n 较小（<100），可接受。

**未来优化**（NG3 范围外）：Phase 3.x 引入红黑树 + 区间合并，O(log n) + 自动合并相邻空闲块。
```

**步骤 2.4** — 在 `tasks.md §1 准备工作` 修改 §1.6 决策项：

```markdown
- [ ] 1.6 决策：**Pool VA 范围 — 已决策为 VA 子范围方案（Option B，Fix-2 决议）**
  - 详见 `design.md §Pool VA 分配算法`
  - 不修改 libgpu_core/gpu_buddy
```

**步骤 2.5** — 在 `spec.md §REQ-3` 修订 Scenario 3.1 和 3.6：

```markdown
#### Scenario 3.1: Create pool (Fix-2 修订)
- WHEN 外部代码调用 `sim_mem_pool_create(&props, &pool_handle_out)`
- THEN sim SHALL:
  - 验证 `va_space_handle` 有效
  - 向 VA Space 申请 [props.va_base, props.va_base + props.size) 子范围
  - 分配 pool handle，记录 pool VA 区间
- AND `*pool_handle_out` SHALL 为有效 pool handle（≥1）

#### Scenario 3.6: Error — Alloc exceeding pool size (Fix-2 修订)
- WHEN 外部代码调用 `sim_mem_pool_alloc(pool_handle, size, &va_out)` 且 `size` 超过 pool 剩余容量
- THEN sim SHALL 返回 -2（即 SIM_POOL_ERR_NOSPC），**不**触发 alloc_bo
```

**步骤 2.6** — 验证命令：

```bash
grep -A 3 "VA 子范围方案" openspec/changes/2026-07-05-sim-stream-primitive-support/proposal.md
grep -A 5 "Pool VA 分配算法" openspec/changes/2026-07-05-sim-stream-primitive-support/design.md
```

---

### Fix-3 (H3): 修正 GpuQueueEmu API 名称失配

**问题回顾**: design.md 称 `sim_graph_launch` 通过"submit_batch 路径复用"，`GpuQueueEmu::enqueue` 不直接执行——但实际 `GpuQueueEmu` 只有 `submit(uint64_t, uint32_t)` 方法。

**修复步骤**:

**步骤 3.1** — 在 `design.md §与现有 sim 原语的集成` 替换整个表格：

```markdown
### 与现有 sim 原语的集成 (Fix-3 修订)

**GpuQueueEmu 实际 API**（基于 `plugins/gpu_driver/sim/gpu_queue_emu.h:113`）：

```c
class GpuQueueEmu {
  // 仅一个入口方法
  int submit(uint64_t gpfifo_addr, uint32_t entry_count);
  // ...
};
```

**集成策略**：

| 新原语 | 集成方式 | 是否需新增 GpuQueueEmu API |
|--------|---------|---------------------------|
| `sim_graph_launch` | 1. 调用 `GpuQueueEmu::submit(gpfifo_addr, entry_count)` 转发到硬件模拟器（与现有 submit_batch handler 一致）<br>2. **不**直接构造 gpfifo，由 sim 层将 graph node metadata 转为 gpfifo entries | ✗ 复用 |
| `sim_stream_capture_*` | capture mode 时**不**调用 `GpuQueueEmu::submit`，仅记录到 graph metadata；end 时才提交 | ✗ 复用 |
| `sim_mem_pool_alloc` | 调 `libgpu_core/gpu_buddy::gpu_buddy_alloc()`，**不**走 GpuQueueEmu | ✗ 复用 |
| `sim_mem_pool_alloc_async` / `sim_mem_pool_free_async` | 调 `GpuQueueEmu::submit()` + fence 返回 | ✗ 复用 |

**结论**：本 change **不修改** `GpuQueueEmu` 类定义，仅复用现有 `submit()` 方法。
```

**步骤 3.2** — 在 `spec.md §REQ-2 Scenario 2.5` 修订：

```markdown
#### Scenario 2.5: Launch instantiated graph (Fix-3 修订)

**WHEN** 外部代码调用 `sim_graph_launch(exec_handle, stream_id)`

**THEN** sim SHALL:
- 验证 exec_handle 有效
- 将 graph node metadata 转换为 gpfifo entries（kernel node → GPFIFO entry, memcpy node → no-op since 内联）
- 调用现有 `GpuQueueEmu::submit(gpfifo_addr, entry_count)` 路径
- 返回 fence_id（≥ (1 << 32)，见 REQ-9）
```

**步骤 3.3** — 在 `proposal.md §What Changes §1` 表格更新 "主要函数" 列：

```markdown
| Memory Pool | `plugins/gpu_driver/sim/mem_pool.h` | `plugins/gpu_driver/sim/mem_pool.cpp` | `sim_mem_pool_create/destroy/alloc/alloc_async/free_async/set_attr/get_attr/trim`（8 个函数）|
```

（删除原"10 个 graph/capture + 5 mempool"相关注释，确保 main 函数列表准确——8 而非 5）

**步骤 3.4** — 验证命令：

```bash
# 验证 design.md 已替换 submit_batch → submit
grep -c "submit_batch" openspec/changes/2026-07-05-sim-stream-primitive-support/design.md
# 应输出 ≤ 1（仅 spec.md Scenario 2.5 修订保留一处说明）
grep -c "GpuQueueEmu::submit" openspec/changes/2026-07-05-sim-stream-primitive-support/design.md
# 应输出 ≥ 3
```

---

## 2. 🟡 P1 — 必填修复（6 项，不阻塞 merge 但阻塞实施）

### Fix-4 (M1): 统一决策项数为 6

**修复步骤**:

**步骤 4.1** — `proposal.md L252` 修改：

```diff
- **LC1**: UsrLinuxEmu Architecture Team 接受本 proposal（含 §8 5 项决策）—— **待决策**
+ **LC1**: UsrLinuxEmu Architecture Team 接受本 proposal（含 design.md §Decisions 6 项决策）—— **待决策**
```

**步骤 4.2** — `tasks.md L235` 修改：

```diff
- 重新评估 §8 8 项决策（Oracle 扩展后从 5 项 → 8 项），调整后重新提 PR
+ 重新评估 design.md §Decisions 6 项决策，调整后重新提 PR
```

**步骤 4.3** — 验证：

```bash
grep -E "5 项决策|8 项决策" openspec/changes/2026-07-05-sim-stream-primitive-support/*.md
# 应无输出
```

---

### Fix-5 (M2): 修正 struct 计数分类

**修复步骤**:

`tasks.md L75` 修改：

```diff
- - [ ] 3.1 编辑 `plugins/gpu_driver/shared/gpu_ioctl.h`
-   - 新增 18 个 IOCTL 编号定义（0x50-0x67）
-   - 新增 **17 个 struct 定义**（7 原有 + 5 Oracle C3 补全 + 5 mempool — 见 design.md §IOCTL 结构体完整列表）
+ - [ ] 3.1 编辑 `plugins/gpu_driver/shared/gpu_ioctl.h`
+   - 新增 18 个 IOCTL 编号定义（0x50-0x67）
+   - 新增 **17 个 struct 定义**（2 stream + 7 graph + 8 mempool = 17 — 见 design.md §IOCTL 结构体完整列表）
```

**验证**：

```bash
grep -A 1 "2 stream + 7 graph + 8 mempool" openspec/changes/2026-07-05-sim-stream-primitive-support/tasks.md
```

---

### Fix-6 (M3): 增加线程安全小节

**修复步骤**:

在 `design.md` 新增章节"§Thread Safety（Fix-6 新增）"（位置：建议放在 §详细技术设计 之后）：

```markdown
### Thread Safety (Fix-6 新增)

**决策**：本 change **不**引入线程安全保证。

**理由**：
- UsrLinuxEmu 当前所有 GPU 路径均为单线程（`gpu_drm_driver.cpp` 由 DRM 主循环串行调用）
- TaskRunner GpuDriverClient 调用方由 `cmd_cuda.cpp` 串行调用
- Stage 1.4 Tier-2 已验证：现有 73/73 测试均为单线程

**全局表**：
- `StreamCaptureTable` / `GraphTable` / `ExecTable` / `PoolTable` 均为进程级单例
- **未加锁**，依赖单线程调用保证

**未来扩展**（如需多线程）：
- 加 `std::mutex table_mutex_` 保护每个表
- 或改为 thread_local（每个线程独立表）

**测试覆盖**：当前测试框架（doctest + Catch2）均为单线程，无需覆盖。
```

---

### Fix-7 (M5): 文档化 `gpu_mem_pool_attr_args.value[4]` 布局

**修复步骤**:

在 `design.md §IOCTL 结构体` 中 `gpu_mem_pool_attr_args` 后追加：

```markdown
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

---

### Fix-8 (M6): 文档化 `kernargs_bo_handle` 语义

**修复步骤**:

在 `spec.md §REQ-2 Scenario 2.2` 后追加：

```markdown
#### Scenario 2.2.1: kernargs_bo_handle 语义 (Fix-8)

- `kernargs_bo_handle == 0`：表示该 kernel 无参数（无 kernargs BO）
- `kernargs_bo_handle != 0`：表示该 kernel 的参数 BO handle（必须已分配）

**验证**：sim 层 SHALL 在 add_kernel_node 时检查 BO handle 有效性：
- 0 → 接受（标记为无 kernargs）
- 非 0 → 在 GraphTable 中查找对应 BO，存在则接受，不存在则返回 -EINVAL
```

---

### Fix-9 (M7): 明确 `int64_t` 返回约定

**修复步骤**:

在 `design.md §sim 原语接口` 顶部追加：

```markdown
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
```

---

## 3. 🟢 P2 — 建议改进（5 项，可在实施 Phase 2 后期补）

### Fix-10 (L1): spec.md 增加 capture mode 错误场景

在 `spec.md §REQ-1` 末尾追加：

```markdown
#### Scenario 1.6: Error — Unsupported capture mode (Fix-10)

**WHEN** 外部代码调用 `sim_stream_capture_begin(stream_id, mode)` 且 `mode` 不是 `SIM_CAPTURE_MODE_GLOBAL (0)`

**THEN** sim SHALL:
- 不修改 stream 状态
- 返回 -1（EINVAL）

**Phase 3.1 范围**：仅 `SIM_CAPTURE_MODE_GLOBAL` 受支持。`SIM_CAPTURE_MODE_THREAD_LOCAL` 和 `SIM_CAPTURE_MODE_RELAXED` 留待 Phase 3.x。
```

---

### Fix-11 (L2): 把 G1-G4 契约测试列入主测试列表

在 `tasks.md §5 测试覆盖 §5.1` 列表前追加：

```markdown
**前置回归（每个新 sim 原语实施后立即跑）**：
- `tests/test_uvm_drm_lifecycle_standalone`（G1-G2 边界）
- `tests/test_gpu_plugin`（G3-G4 集成）
- 现有 Stage 1.4 全部 73 个测试 binary
```

---

### Fix-12 (L3): ADR-015 范围切割

**修复步骤**:

在 `tasks.md §6.2.5` 修订：

```markdown
- [ ] 6.2.5 更新 ADR-015 IOCTL 编号表（Oracle H3 修正 + Fix-12 切割）：
  - **本 change 范围**：补 0x50-0x67 (新增 18 个 IOCTL) + 标注 0x70-0x7F 为 "reserved for future use"
  - **不在本 change 范围**：补 0x44-0x47 (KFD portability，Stage 1.4 已实施) → 移至 §10 Follow-ups
```

在 `tasks.md §10` 新增：

```markdown
## 10. Follow-ups（不阻塞本 change）

- [ ] F.1 补 ADR-015 中 0x44-0x47 (KFD) 编号表（Stage 1.4 遗留事项，独立小 PR）
- [ ] F.2 sim 原语多线程扩展（仅在 GpuDriverClient 多线程调用需求出现时）
- [ ] F.3 Pool VA 区间合并优化（NG3 范围外）
```

---

### Fix-13 (L4): 增加命名规范小节

在 `design.md §详细技术设计` 顶部追加：

```markdown
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
```

---

### Fix-14 (L5): 验证 TaskRunner 跨仓 PR 文档真实存在

**修复步骤**:

**步骤 14.1** — 在 terminal 验证 cross-repo PR 文档存在：

```bash
test -f external/TaskRunner/docs/superpowers/cross-repo-prs/2026-07-05-phase3-1-stream-mempool-coordination.md && echo "EXISTS" || echo "MISSING"
```

**步骤 14.2** — 若存在，在 `proposal.md` 关联 TaskRunner 文档段追加版本号：

```markdown
- **关联 TaskRunner 文档**:
  - `external/TaskRunner/docs/superpowers/cross-repo-prs/2026-07-05-phase3-1-stream-mempool-coordination.md`（协调请求 PR，2026-07-05, 444 行，已验证）
  - `external/TaskRunner/docs/superpowers/specs/2026-07-02-phase3-stream-capture-design.md`（Phase 3.1 设计稿，2026-07-02, 346 行）
  - `external/TaskRunner/docs/superpowers/specs/2026-07-02-phase3-mempool-design.md`（Phase 3.2 设计稿，2026-07-02, 226 行）
```

---

## 4. 🔵 P3 — 外部修复（4 项，依赖 TaskRunner 反馈）

详见 [`taskrunner-feedback.md`](taskrunner-feedback.md)：

- **Ext-1**: TaskRunner 侧的 IGpuDriver 15-方法扩展文档明确 fence_id 返回范围
- **Ext-2**: TaskRunner shim 层使用实际 API 名称（submit 而非 submit_batch）
- **Ext-3**: TaskRunner pool shim 层适配 VA 子范围方案（Fix-2 决议）
- **Ext-4**: TaskRunner capture mode 仅请求 `SIM_CAPTURE_MODE_GLOBAL`

---

## 5. 修复验收 Checklist

修复完成后，按以下顺序验证：

```bash
# 1. P0 修复验证（必须全过）
cd /workspace/project/UsrLinuxEmu

# 1.1 fence_id migration plan 存在
grep -c "Migration Plan" openspec/changes/2026-07-05-sim-stream-primitive-support/design.md
# 期望: ≥ 1

# 1.2 Pool VA 子范围方案明确
grep -c "VA 子范围方案" openspec/changes/2026-07-05-sim-stream-primitive-support/proposal.md
# 期望: ≥ 1

# 1.3 GpuQueueEmu::submit 引用（无 submit_batch）
grep -c "GpuQueueEmu::submit" openspec/changes/2026-07-05-sim-stream-primitive-support/design.md
grep -c "submit_batch" openspec/changes/2026-07-05-sim-stream-primitive-support/design.md
# 期望: 前者 ≥ 3, 后者 ≤ 1

# 2. P1 修复验证
grep "6 项决策" openspec/changes/2026-07-05-sim-stream-primitive-support/proposal.md
grep "2 stream + 7 graph + 8 mempool" openspec/changes/2026-07-05-sim-stream-primitive-support/tasks.md
grep "Thread Safety" openspec/changes/2026-07-05-sim-stream-primitive-support/design.md
grep "value_size 必须" openspec/changes/2026-07-05-sim-stream-primitive-support/design.md
grep "kernargs_bo_handle == 0" openspec/changes/2026-07-05-sim-stream-primitive-support/spec.md
grep "返回值约定" openspec/changes/2026-07-05-sim-stream-primitive-support/design.md

# 3. P2 修复验证
grep "Unsupported capture mode" openspec/changes/2026-07-05-sim-stream-primitive-support/spec.md
grep "命名规范" openspec/changes/2026-07-05-sim-stream-primitive-support/design.md

# 4. 文档审计
SKIP_DOCS_AUDIT=0 tools/docs-audit.sh --strict
```

---

## 6. 修复后状态

完成 P0 + P1 + P2 修复后，本 change 状态从 `🔄 PROPOSED` 升级为 `✅ ACCEPTED`，可进入实施阶段（Day 1 创建 worktree）。

**实施准入**：
- LC1: ✓ Architecture Team 接受（含 6 项决策）
- LC2: ✓ Stage 1.4 Tier-1/Tier-2 已 merge
- LC3: ⏳ TaskRunner IGpuDriver 15-方法扩展（依赖 P3 Ext-1）
- LC4: ✓ 70+/70+ 回归测试基线
- LC5: ⏳ worktree 创建（实施 Phase 1）

---

**修复负责人**: UsrLinuxEmu Architecture Team
**修复总工期**: 4-6 小时编辑 + 2 小时验证 + 1 小时 cross-doc audit = ~1 工作日
**下游依赖**: TaskRunner 团队接受 [`taskrunner-feedback.md`](taskrunner-feedback.md) 后才能进入实施