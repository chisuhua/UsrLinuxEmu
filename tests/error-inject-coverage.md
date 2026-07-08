# Errno Coverage Matrix — IOCTL Handler Error Paths

> **Created**: 2026-07-08
> **Scope**: 32 ioctl handlers in `GpgpuDevice::kTable`
> **Audit method**: grep `gpgpu_device.cpp` + sim layer source review
> **Validation**: test_gpu_plugin.cpp (34→52 core) + test_error_inject_standalone (6 new) = 58 test cases

## Coverage per handler

| IOCTL | Handler | `-EFAULT` | `-EINVAL` | `-ENOMEM` | `-ENOSYS` | Sim-layer |
|-------|---------|:---------:|:---------:|:---------:|:---------:|:---------:|
| GET_DEVICE_INFO | handleGetDeviceInfo | ✅ null arg | n/a | n/a | n/a | n/a |
| ALLOC_BO | handleAllocBo | ✅ null arg | ✅ domain=0 | ✅ HAL inj (2x) | n/a | n/a |
| FREE_BO | handleFreeBo | ✅ null arg | ✅ handle=0 | n/a | n/a | n/a |
| MAP_BO | handleMapBo | ✅ null arg | ✅ handle invalid | n/a | n/a | n/a |
| PUSHBUFFER_SUBMIT_BATCH | handlePushbufferSubmitBatch | ✅ null arg | ✅ count=0 | n/a | n/a | n/a |
| WAIT_FENCE | handleWaitFence | ✅ null arg | n/a | n/a | n/a | ✅ timeout |
| CREATE_QUEUE | handleCreateQueue | ✅ null arg | ✅ type>max | n/a | n/a | n/a |
| DESTROY_QUEUE | handleDestroyQueue | ✅ null arg | n/a | n/a | n/a | ✅ ENOENT |
| MAP_QUEUE_RING | handleMapQueueRing | ✅ null arg | n/a | n/a | n/a | ✅ ENOENT |
| QUERY_QUEUE | handleQueryQueue | ✅ null arg | n/a | n/a | n/a | n/a |
| CREATE_VA_SPACE | handleCreateVASpace | ✅ null arg | ✅ page_size>1 | n/a | n/a | n/a |
| DESTROY_VA_SPACE | handleDestroyVASpace | ✅ null arg | n/a | n/a | n/a | ✅ ENOENT |
| REGISTER_GPU | handleRegisterGPU | ✅ null arg | n/a | n/a | n/a | n/a |
| STREAM_CAPTURE_BEGIN | handleStreamCaptureBegin | ✅ null arg | ✅ mode=0xFF | n/a | n/a | ⚠️ -1 for double-begin |
| STREAM_CAPTURE_END | handleStreamCaptureEnd | ✅ null arg | n/a | n/a | n/a | ⚠️ -1 for bogus stream |
| STREAM_CAPTURE_STATUS | handleStreamCaptureStatus | ✅ null arg | n/a | n/a | n/a | OK for bogus (returns NONE) |
| GRAPH_CREATE | handleGraphCreate | ✅ null arg | n/a | n/a | n/a | n/a |
| GRAPH_DESTROY | handleGraphDestroy | ✅ null arg | ✅ h=0 (via test) | n/a | n/a | ⚠️ -1 if not found |
| GRAPH_ADD_KERNEL_NODE | handleGraphAddKernelNode | ✅ null arg | ✅ h=0 (via test) | n/a | n/a | ⚠️ -1 if not found |
| GRAPH_ADD_MEMCPY_NODE | handleGraphAddMemcpyNode | ✅ null arg | ✅ h=0 (via test) | n/a | n/a | ⚠️ -1 if not found |
| GRAPH_INSTANTIATE | handleGraphInstantiate | ✅ null arg | ✅ h=0 (via test) | n/a | n/a | ⚠️ -1 if not found |
| GRAPH_LAUNCH | handleGraphLaunch | ✅ null arg | ✅ h=0 (via test) | n/a | n/a | `-EINVAL` if not found |
| GRAPH_DESTROY_EXEC | handleGraphDestroyExec | ✅ null arg | ✅ h=0 (via test) | n/a | n/a | ⚠️ -1 if not found |
| MEM_POOL_CREATE | handleMemPoolCreate | ✅ null arg | ✅ size=0 | n/a | n/a | `SIM_POOL_ERR_INVAL` (-3) |
| MEM_POOL_DESTROY | handleMemPoolDestroy | ✅ null arg | ✅ h=0 (via test) | n/a | n/a | `SIM_POOL_ERR_INV_H` (-1) |
| MEM_POOL_ALLOC | handleMemPoolAlloc | ✅ null arg | ✅ h=0 (via test) | n/a | n/a | `SIM_POOL_ERR_INV_H` (-1) |
| MEM_POOL_ALLOC_ASYNC | handleMemPoolAllocAsync | ✅ null arg | ✅ h=0 (via test) | ✅ (fence exhaustion) | n/a | ⚠️ -1 if not found |
| MEM_POOL_FREE_ASYNC | handleMemPoolFreeAsync | ✅ null arg | ✅ va=0 (via test) | ✅ (fence exhaustion) | n/a | ⚠️ -1 if not found |
| MEM_POOL_SET_ATTR | handleMemPoolSetAttr | ✅ null arg | n/a | n/a | ✅ attr=0xFF | `SIM_POOL_ERR_INV_H` |
| MEM_POOL_GET_ATTR | handleMemPoolGetAttr | ✅ null arg | n/a | n/a | ✅ attr=0xFF | `SIM_POOL_ERR_INV_H` |
| MEM_POOL_TRIM | handleMemPoolTrim | ✅ null arg | ✅ h=0 (via test) | n/a | n/a | `SIM_POOL_ERR_INV_H` |
| MEM_POOL_EXPORT | handleMemPoolExport | ✅ null arg | ✅ h=0 (via test) | n/a | n/a | ⚠️ -1 if not found |

_Legend: ✅ = covered by test, ⚠️ = sim returns raw -1 (non-standard errno, see Findings below), n/a = not applicable_

## Findings: Non-standard errno in sim layer

The sim layer (`plugins/gpu_driver/sim/`) uses non-standard error codes that are passed through to userspace without remapping:

| File | Return code | Standard equivalent |
|------|------------|-------------------|
| `stream_capture.cpp` | `return -1` (double begin, end for non-active, unreachable) | Should be `-EINVAL` |
| `graph.cpp` | `return -1` (destroy/add/instantiate/destroy_exec for invalid handle) | Should be `-EINVAL` or `-ENOENT` |
| `mem_pool.cpp` | `SIM_POOL_ERR_INVALID_HANDLE (-1)` | Should be `-EINVAL` or `-ENOENT` |
| `mem_pool.cpp` | `SIM_POOL_ERR_NOSPC (-2)` | Should be `-ENOSPC` |
| `mem_pool.cpp` | `SIM_POOL_ERR_INVAL (-3)` | Should be `-EINVAL` |
| `mem_pool.cpp` | `SIM_POOL_ERR_NOT_SUPPORTED (-4)` | Should be `-EOPNOTSUPP` |

These are **pre-existing** (prior to this change). `fc6f854` fixed the fence_id exhaustion paths to return `-ENOMEM` instead of `-1`, but left the invalid-handle paths. The graph layer `return -1` is also documented in `fc6f854` commit message as deliberately unchanged because `test_sim_graph_standalone.cpp` asserts `== -1`.

**Recommendation**: Future change to normalize sim-layer errno. Specifically:
- `-1` → `-EINVAL` for invalid handle cases
- `SIM_POOL_ERR_*` → standard Linux errno via a remapping function in the handlers

## Test counts

| Test suite | Before | After | Δ |
|-----------|--------|-------|---|
| `test_gpu_plugin` (case count) | 34 | 52 | +18 |
| `test_error_inject_standalone` | — | 6 | +6 |
| Total ctest binaries | 85 | 86 | +1 |
| Total assertions (approx) | — | — | +84 |
