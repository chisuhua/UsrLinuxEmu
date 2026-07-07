# ADR-039: MEM_POOL_EXPORT IOCTL (0x68)

**状态**: ✅ 已接受 (Accepted)
**日期**: 2026-07-07
**提案人**: TaskRunner Phase 4 Team
**关联 ADR**: ADR-036 (3-way separation), ADR-015 (IOCTL unification), ADR-018 (driver-sim separation)
**关联 Change**: `openspec/changes/phase3-real-impl-bridge/` (Phase 4 MEM_POOL_EXPORT)
**关联 TaskRunner TADR**: tadr-302 (sync primitives — pending mempool export extension)

---

## Context

TaskRunner Phase 3 (`phase3-real-impl-bridge`) 的 Phase 4 需要实现 `cuMemPoolExportToShareableHandle` 支持。CUDA 语义允许将内存池导出为可共享句柄（POSIX FD），供跨进程或跨 API 共享。UsrLinuxEmu 需要新增一个 IOCTL 来模拟此能力。

当前 IOCTL 分配状态：0x60-0x67 已用于 mem_pool 系列（create/destroy/alloc/alloc_async/free_async/set_attr/get_attr/trim），0x68 是下一个可用编号。

### 约束

- 仅支持 `CU_MEM_HANDLE_TYPE_POSIX_FILE_DESCRIPTOR`（handle_type=1）
- Win32/Fabric 等 handle_type 延后至 Phase 5+
- 模拟实现使用 `pipe2(O_CLOEXEC)` 返回 POSIX FD
- flags 字段预留，Phase 4 必须为 0

---

## Decision

### D1: 新增 IOCTL 0x68

在 `gpu_ioctl.h` 新增 `GPU_IOCTL_MEM_POOL_EXPORT`，使用 `_IOWR` 宏以确保 `fd_out` 被正确写回用户态。

### D2: sim 层新增 sim_mem_pool_export_shareable

遵循 ADR-018 + ADR-036 三区分原则，sim 层新增 C-ABI 函数：
- `pool_handle` 无效 → `SIM_POOL_ERR_INVALID_HANDLE`
- `handle_type != 1` → `SIM_POOL_ERR_INVAL`
- `flags != 0` → `SIM_POOL_ERR_INVAL`
- 成功: `pipe2(O_CLOEXEC)` 创建 pipe，写 "POOL:<hex_handle>:0" 元数据 blob 到写端后关闭写端，返回读端 fd

### D3: DRM table 注册

遵循现有 IOCTL dispatch 模式，在 `gpu_drm_driver.cpp` 中新增 handler 和 DRM table entry。

### D4: 结构体 layout

```c
struct gpu_mem_pool_export_args {
  u64 pool_handle;   /* input */
  u32 handle_type;   /* input: 1 = POSIX FD */
  u32 flags;         /* input: reserved, must be 0 */
  s32 fd_out;        /* OUT: POSIX FD (>= 0) or -1 */
  u32 _pad;          /* alignment */
};
```

---

## Consequences

### 正面

- 为 TaskRunner Phase 4 提供 `cuMemPoolExportToShareableHandle` 的底层 IOCTL 支持
- 纯模拟实现，不修改 libgpu_core
- 与现有 IOCTL 模式完全一致

### 负面

- `pipe2(O_CLOEXEC)` 实现与真实 GPU 的 DMA-buf 导出语义不同（PoC 级别）
- 不支持 Win32/Fabric 句柄类型（Phase 5+ 扩展点）
- 无法跨进程真正共享内存池（pipe 是进程内对象）

### 迁移

Phase 5+ 如需真正跨进程共享，需升级为：
1. 基于 memfd 的实现（sealable, shareable via `/proc/<pid>/fd/<n>`）
2. DMA-buf 格式对齐（标准 Linux DRM prime 导出）

---

## 关联 TaskRunner TADR

TaskRunner `tadr-302` (sync primitives) 将引用本 ADR 的 IOCTL 编号和结构体定义，作为 `cuMemPoolExportToShareableHandle` Stub/C-ABI 层的基础。Phase 4 的 TaskRunner 实现（`phase3-real-impl-bridge-extended`）依赖于本 IOCTL 已合并到 UsrLinuxEmu main。

---

## 实施记录

| 文件 | 变更 |
|------|------|
| `plugins/gpu_driver/shared/gpu_ioctl.h` | 新增 `GPU_IOCTL_MEM_POOL_EXPORT (0x68)` + `struct gpu_mem_pool_export_args` |
| `plugins/gpu_driver/sim/mem_pool.h` | 新增 `sim_mem_pool_export_shareable` 声明 |
| `plugins/gpu_driver/sim/mem_pool.cpp` | 新增 `sim_mem_pool_export_shareable` 实现 |
| `plugins/gpu_driver/drv/gpu_drm_driver.cpp` | 新增 macro alias + handler + DRM table entry |
| `tests/test_gpu_mempool_export.cpp` | 5 个 standalone 测试用例 |
| `tests/CMakeLists.txt` | 注册 `test_gpu_mempool_export_standalone` |