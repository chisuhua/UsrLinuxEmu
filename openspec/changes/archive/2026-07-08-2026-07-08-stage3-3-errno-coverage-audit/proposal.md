# Change: stage3-3-errno-coverage-audit

> **状态**: 📋 PROPOSED
> **优先级**: 🟢 P2
> **创建**: 2026-07-08
> **来源**: Issue #24 §3.3（Stage 3.3 error handling completeness）
> **依赖**: C-02 stage3-ioctl-dispatch-completeness
> **工作目录**: `openspec/changes/2026-07-08-stage3-3-errno-coverage-audit/`

## Why

Stage 3.3 要求所有路径返回 Linux 风格错误码。PR #20 + PR #26 + PR #27 共添加 **20 个新 IOCTL handler**（0x50-0x68），但 error path 的 errno 返回未必正确：

- `sim_graph_launch` / `sim_mem_pool_*_async` 早期返回 `-1`（已通过 `fc6f854` 修）
- 但其他新 handler 的 error path 未审计

## What Changes

### 1. 审计 20 个新 IOCTL handler

每个 handler 检查：
- 参数验证失败 → `-EINVAL` / `-EFAULT`
- 资源耗尽 → `-ENOMEM` / `-ENOSPC`
- 不支持操作 → `-ENOSYS` / `-EOPNOTSUPP`
- 中断/超时 → `-EINTR` / `-ETIMEDOUT`

### 2. 修复错误路径

每个 handler 改为返回标准 Linux errno。

### 3. 加测试 case

每个 error path 一个 test case。

## 涉及 Handler

| IOCTL # | Handler | 位置 |
|---------|---------|------|
| 0x50-0x59 | 10 stream+graph (PR #20) | sim/{stream_capture,graph}.cpp + drv |
| 0x60-0x67 | 8 mempool (PR #20) | sim/mem_pool.cpp + drv |
| 0x68 | 1 mempool_export (PR #27) | sim/mem_pool.cpp + drv |

共 19 个 + 1 = **20 个 handler**

## Acceptance

- [ ] 所有 20 个新 IOCTL handler 在 4 种 error 场景下返回正确 errno
- [ ] 加 test case 验证每种 errno（至少 80 个新 test assertions）
- [ ] `test_gpu_plugin` 测试集扩展（从 +18 → +36 或更多）
- [ ] docs-audit 无新 warning
- [ ] ctest 全绿

## 测试方法

```bash
cd build
ctest -R "test_gpu_plugin|test_gpu_driver_client_phase31"   # 现有
ctest -R "errno|error_path"                                  # 新增
ctest                                                       # 全量
```

## Cross-Repo 影响

TaskRunner 端需相应更新 expected_errno in test_cu_*.cpp（如有修改）。

## Dependencies

- **C-02** stage3-ioctl-dispatch-completeness（必须先合入，handler 才在 runtime 可达）
