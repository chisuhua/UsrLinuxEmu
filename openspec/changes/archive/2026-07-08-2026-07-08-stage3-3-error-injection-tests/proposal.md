# Change: stage3-3-errno-and-error-injection

> **状态**: 📋 PROPOSED → 🔄 READY
> **优先级**: 🟢 P2
> **创建**: 2026-07-08
> **来源**: Issue #24 §3.3（Stage 3.3 error handling completeness）
> **融合**: C-06 (errno-coverage-audit) + C-07 (error-injection-tests) 合并
> **依赖**: C-02 stage3-ioctl-dispatch-completeness ✅ 已完成
> **工作目录**: `openspec/changes/2026-07-08-stage3-3-error-injection-tests/`

## Why

Stage 3.3 要求所有路径返回 Linux 风格错误码 + critical path error injection 测试覆盖 ≥ 80%。

### 现状

PR #20 + PR #26 + PR #27 共添加 **20 个新 IOCTL handler**（0x50-0x68）。现有测试情况：

| 阶段 | Handler | 已有 happy-path | 已有 error-path |
|------|---------|:-:|:-:|
| Baseline (13 handlers, 0x00-0x0C) | GET_DEVICE_INFO / ALLOC_BO / FREE_BO / MAP_BO / SUBMIT / WAIT_FENCE / CREATE/DESTROY_QUEUE / MAP_QUEUE_RING / QUERY_QUEUE / CREATE/DESTROY_VA_SPACE / REGISTER_GPU | ✅ 19 tests | ⚠️ 6 partial |
| Phase 3.1 (10 handlers, 0x50-0x59) | STREAM_CAPTURE_BEGIN/END/STATUS + GRAPH_CREATE/DESTROY/ADD_KERNEL_NODE/ADD_MEMCPY_NODE/INSTANTIATE/LAUNCH/DESTROY_EXEC | ✅ 10 tests | ❌ 0 tests |
| Phase 3.2 (8 handlers, 0x60-0x67) | MEM_POOL_CREATE/DESTROY/ALLOC/ALLOC_ASYNC/FREE_ASYNC/SET_ATTR/GET_ATTR/TRIM | ✅ 9 tests | ❌ 0 tests |
| Phase 4 (1 handler, 0x68) | MEM_POOL_EXPORT | ✅ 1 test (test_gpu_mempool_export) | ❌ 0 tests |

**Gap**: Phase 3/4 的 19 个 handler **零 error-path 测试**。

### 此前修复

- `sim_graph_launch` / `sim_mem_pool_*_async` 早期返回 `-1` — 已由 `fc6f854` 修复为返回标准 errno（`-ENOMEM`）
- 其余 20+ handler 的 error path 已有基础 errno 合规（`gpgpu_device.cpp` grep 确认为标准 Linux errno）—— C-06 审计发现无需额外修复

## What Changes

### 审计结果（C-06 合并）

全表 32 handler 的 error path 已返回标准 Linux errno，无需修复：

| errno | 出现频率 | 典型场景 |
|-------|---------|---------|
| `-EFAULT` | 13+ | null argp 检查（每个 handler 第一行） |
| `-EINVAL` | 12+ | 无效参数（domain=0, handle=0, queue_type>max, attr 枚举越界） |
| `-ENOMEM` | 8+ | handle 分配耗尽（`handles_.allocate()==0`）、HAL 内存分配失败 |
| `-ENOSYS` | 1 | `MEM_POOL_SET_ATTR` 未知 attr 类型 |
| `-ETIMEDOUT` | 0（隐式） | `WAIT_FENCE` 超时返回 `status=0` 而非负值 |

→ 无需修复代码，**直接进入测试覆盖**。

### 1. 扩展 test_gpu_plugin.cpp — 无效参数注入（通过真实 plugin）

现有 fixture `GpuPluginTestFixture` + real plugin，新增 error-path test cases：

| Handler | 注入方式 | 预期 errno |
|---------|---------|-----------|
| STREAM_CAPTURE_BEGIN | stream_id=0xFF | -EINVAL |
| STREAM_CAPTURE_END | 先 begin，后对 bogus stream_id 调用 end | -EINVAL |
| STREAM_CAPTURE_STATUS | stream_id=0 (never opened) | -EINVAL |
| GRAPH_CREATE | （无参数，仅 happy path） | — |
| GRAPH_DESTROY | graph_handle=0 | -EINVAL |
| GRAPH_ADD_KERNEL_NODE | graph_handle=0 | -EINVAL |
| GRAPH_ADD_MEMCPY_NODE | graph_handle=0 | -EINVAL |
| GRAPH_INSTANTIATE | graph_handle=0（无节点） | -EINVAL |
| GRAPH_LAUNCH | exec_handle=0 | -EINVAL |
| GRAPH_DESTROY_EXEC | exec_handle=0 | -EINVAL |
| MEM_POOL_CREATE | props.size=0 | -EINVAL |
| MEM_POOL_DESTROY | pool_handle=0 | -EINVAL |
| MEM_POOL_ALLOC | pool_handle=0 | -EINVAL |
| MEM_POOL_ALLOC_ASYNC | pool_handle=0 | -EINVAL |
| MEM_POOL_FREE_ASYNC | va=0（未分配） | -EINVAL |
| MEM_POOL_SET_ATTR | attr=0xFF（非法枚举） | -ENOSYS |
| MEM_POOL_GET_ATTR | attr=0xFF | -ENOSYS |
| MEM_POOL_TRIM | pool_handle=0 | -EINVAL |
| MEM_POOL_EXPORT | pool_handle=0 | -EINVAL |

**共 ~18 个新 test cases**（每个 1-3 个 REQUIRE）。

### 2. 新建 test_error_inject_standalone.cpp — HAL mock 注入

新增独立测试二进制，链接 `gpu_hal_mock`，构造 `GpgpuDevice` 时注入 mock HAL（不经过 real plugin）。

```cpp
// 模式：创建 GpgpuDevice + hal_mock_state，设置注入参数，调用 ioctl
struct gpu_hal_ops hal;
struct hal_mock_state state;
hal_mock_init(&hal, &state);

state.mem_alloc_result = -ENOMEM;  // 注入 HAL 分配失败

auto dev = std::make_shared<GpgpuDevice>(&hal);
// 通过 fops/ioctl 路径调用
struct gpu_alloc_bo_args args{};
args.size = 4096;
args.domain = GPU_MEM_DOMAIN_VRAM;
int ret = dev->ioctl(0, GPU_IOCTL_ALLOC_BO, &args);  // 直接调 ioctl 而非 VFS
REQUIRE(ret == -ENOMEM);
```

**注入矩阵**（HAL 函数 → 受影响 handler）：

| HAL 函数 | 注入 `*_result = -ENOMEM` | 受影响的 handler | handler 返回 |
|-----------|--------------------------|-----------------|-------------|
| `mem_alloc` | → 失败 | ALLOC_BO `handleAllocBo` | `-ENOMEM` |
| `mem_alloc` | → 失败 | MAP_BO（后端 mmap） | `-ENOMEM` |
| `mem_free` | → 失败 | FREE_BO | `-ENOMEM` |
| `fence_create` | → 失败 | PUSHBUFFER_SUBMIT_BATCH | `-ENOMEM` |
| `fence_create` | → 失败 | MAP_QUEUE_RING | `-ENOMEM` |
| `fence_read` | → 失败 | WAIT_FENCE | `-EINVAL` |

**共 ~6 个新 test cases**（HAL 注入路径）。

### 3. 覆盖率矩阵

目标：**32 handler × 4 error 维度 = 128 assertion tier-1 覆盖**

| 维度 | 覆盖方式 | case 数 | assertion 数 |
|------|---------|---------|-------------|
| `-EFAULT` (null arg) | test_gpu_plugin 扩展现有 | 32 | 32 |
| `-EINVAL` (无效参数) | test_gpu_plugin 扩展 | 18 | 36 |
| `-ENOMEM` (HAL 注入) | test_error_inject_standalone 新增 | 6 | 12 |
| `-ENOSYS` (不支持 attr) | test_gpu_plugin 扩展 | 2 | 4 |
| **总计** | | **58** | **84** |

实际实现约 **30-58 新 test cases**（满足 acceptance "≥ 20 cases"）。

## Acceptance

- [ ] errno 审计完成：所有 32 handler 的 error path 已确认使用标准 Linux errno（无需修复代码）
- [ ] test_gpu_plugin 新增 ≥ 18 error-path test cases（无效参数注入）
- [ ] test_error_inject_standalone 新增 ≥ 6 HAL mock injection cases
- [ ] ctest 全绿（新增 + 原有 85 tests 均通过）
- [ ] docs-audit 无新 warning
- [ ] 测试矩阵 doc 化：哪个 handler × 哪类 errno 已覆盖 / 待覆盖

## 测试方法

```bash
cd build && cmake .. && make -j4

# 新增 error injection 测试
./bin/test_error_inject_standalone

# 扩展后的 plugin 测试（含 error-path）
./bin/test_gpu_plugin --verbosity high

# 全量
ctest --output-on-failure
```

## Cross-Repo 影响

无。纯 UsrLinuxEmu 测试基础设施。

## Dependencies

- **C-02** stage3-ioctl-dispatch-completeness ✅

## 设计决策（ADR）

| 决策 | 选择 | 理由 |
|------|------|------|
| 注入机制 | `hal_mock_state` struct + 函数指针替换 | 不污染 prod 代码（无 `getenv()`），类型安全，测试隔离，已有先例 |
| 覆盖定义 | 32 handler × 4 errno 矩阵 = 128 assertion 目标 | 可度量、可追踪、每个 handler 的每个 error 维都显式测试 |
| 测试二进制 | test_gpu_plugin 扩展 + test_error_inject_standalone 新增 | 扩展利用已有 plugin 环境测 invalid-arg；新二进制测 HAL 失败注入 |
| CI 模式 | opt-in（`ctest -R error_inject`），不做 CI gating | 注入测试环境敏感，不阻塞 CI |
| 覆盖率目标 | 实际 58 cases / 84 assertions（≥ 20 验收下限） | 务实：覆盖全部 Phase 3 handler 的 2-3 种 error，core handler（ALLOC_BO 等）4 种 |
