# ADR-076: GPGPU Kernel Module IOCTL（HAL Extension for PTX-EMU Image Executor 集成）

**状态**: 🚫 **Superseded by ADR-090**（2026-08-17 — 详见 [ADR-090](adr-090-ptxir-via-h2d-dma.md)；**已 ship 实施产物保留作为历史记录**，3 个 ioctl 0x27/0x28/0x29 + 3 个 HAL fn-ptr #66/#67/#68 + 144 ctest 全数保留，**退役路径见 ADR-090 §Migration**）
**日期**: 2026-08-09（v1 草案）→ 2026-08-13（v1 升 Accepted）→ 2026-08-15（v2 演进推迟声明）→ **2026-08-17（v3 退役声明 — Superseded by ADR-090）**
**提案人**: Sisyphus（基于 PTX-EMU ADR-0029 跨仓评审修订草案；canonical source per ADR-035 §R5.1 cross-repo 协议）
**评审者**:
- UsrLinuxEmu Architecture Team（owner 评审 — HAL append-only + ioctl 编号预留 39/40/41 可接受性）
- TaskRunner owner（consumer-lens 评审 — tadr-307 IGpuDriver 扩展对齐性）
- PTX-EMU Architecture Team（已批准 HAL 方案作为 Phase 2 集成路径；详见 [PTX-EMU ADR-0029 §D8](../external/PTX-EMU/docs/adr/ADR-0029-ptxemu-image-executor.md#d8-cp-端集成约定--hal-扩展方案usrlinuxemu--ptx-emu-跨仓契约)）

**关联 ADR**:
- [ADR-023](adr-023-hal-interface.md) ✅ HAL 接口契约（**本 ADR 是其扩展**，per Decision 4 spec-driven "追加不改"）
- [ADR-018](adr-018-driver-sim-separation.md) ✅ 驱动/仿真分离（HAL 仍是 ②③ 之间唯一桥）
- [ADR-036](adr-036-three-way-separation.md) ✅ 3 区分架构原则（HAL 扩展不破坏分层）
- [ADR-035](adr-035-governance-policy.md) ✅ 治理规则（本 ADR 走 §R5.1 cross-repo 流程）
- [**ADR-090**](adr-090-ptxir-via-h2d-dma.md) 🔄 **Superseding ADR（本 ADR 的 v2 修订替代）** — PTXIR Image Loading via CppTLM H2D DMA。**本 ADR 的已 ship 实施产物退役路径由 ADR-090 §Migration 接管**。本 ADR 文件保留作为历史决策记录与已 ship 实现说明。
- [ADR-039](adr-039-mem-pool-export-ioctl.md) ✅ MEM_POOL_EXPORT IOCTL 0x68（**姊妹变更**，同样跨仓扩展 IGpuDriver + ioctl）
- [ADR-061](adr-061-hal-iommu-extension.md) ✅ HAL IOMMU ops 扩展（**模式借鉴** — 本 ADR 采用类似 append-only 治理）

**关联 TADR**:
- [tadr-305](../external/TaskRunner/docs/shared/adr/tadr-305-mempool-export-shareable.md) ✅ IGpuDriver::memPoolExportShareable（**姊妹变更模板**，同 H-2.5 + tadr-301 模式）
- [tadr-307](../external/TaskRunner/docs/shared/adr/tadr-307-igpu-driver-kernel-module-extension.md) 📋 **本 ADR 的 consumer-side 对偶文档**

**关联外部 ADR**:
- [PTX-EMU ADR-0029 §D8](../external/PTX-EMU/docs/adr/ADR-0029-ptxemu-image-executor.md#d8-cp-端集成约定--hal-扩展方案usrlinuxemu--ptx-emu-跨仓契约) 📋 image executor in-memory Driver API（**驱动本 ADR 的需求源**）
- [PTX-EMU ADR-0024](../external/PTX-EMU/docs/adr/ADR-0024-ptxir-cubin-embed-extension.md) ✅ PTXIR-Embedded CUBIN 格式（image bytes 编码）
- [PTX-EMU ADR-0029 D3](../external/PTX-EMU/docs/adr/ADR-0029-ptxemu-image-executor.md#d3-image-bytes-私有保存--launch-时重-deserializea2-修复-mutation-bug) image bytes 私有保存 + per-launch re-deserialize（HAL 端需透传 image bytes）

**关联 Change（建议）**: `openspec/changes/2026-08-15-stage5-ptxemu-kernel-module-hal-extension/`（建议命名，待 UsrLinuxEmu owner 确认；**注**：2026-08-15 调整——本 change 实施已 ship，本 ADR 后续演进待 ADR-088 实施后评估）

**修订记录**:
- 2026-08-09 v1：初版草案（PTX-EMU ADR-0029 跨仓评审修订触发；canonical source per ADR-035 §R5.1 cross-repo 协议）
- 2026-08-13 v1 升 Accepted：HAL extension 完整实施（3 个 ioctl 0x27/0x28/0x29 + 3 个 HAL fn-ptr #66/#67/#68 + `hal_user.cpp` dlsym `libptxemu_device.so`）+ Oracle APPROVED-WITH-CONDITIONS + 144/145 ctest PASS + L1 portability check PASS
- 2026-08-15 v2 修订：**后续演进讨论推迟**。当前项目焦点为推进 [ADR-088（dGPU 参考设计 — 完整硬件子系统仿真）](adr-088-dgpu-complete-simulation.md)；本 ADR 已 ship 的实施产物（3 个 ioctl + 3 个 fn-ptrs + 144 ctest）保持 ✅ Accepted 状态不变。**待 ADR-088 升 Accepted 后再启动本 ADR 的演进讨论**（注：ADR-088 已于 2026-08-15 升 Accepted，推迟已退出），届时重点评估：
  - (a) 是否需要 v2 修订以支持 PTX-EMU 与 CppTLM backend 共存（环境变量 / dlopen 顺序 / C-ABI 命名空间）
  - (b) HAL fn-ptr 是否需要扩展以支持 CppTLM path 的 Kernel Module 加载路径
  - (c) TaskRunner [tadr-307](../external/TaskRunner/docs/shared/adr/tadr-307-igpu-driver-kernel-module-extension.md) 集成是否需要同步修订
  - (d) 跨仓契约澄清文档（per Sisyphus + Metis 双层审查 P1-C 偏差）

  **不冲突**：[ADR-088 §C2](adr-088-dgpu-complete-simulation.md) 明确"ADR-076 (PTX-EMU): **不被取代** — Kernel Module 仍走 PTX-EMU path"；两者**共存**：hal_user.cpp 可同时配置 PTX-EMU backend + CppTLM backend（env var 互不干扰）。本 v2 修订不撤销任何已 ship 实施，仅推迟**后续演进**讨论。

---

## Context

### C1: 现状 — PTX-EMU Image Executor 需要 CP 端集成入口

[PTX-EMU ADR-0029 §D8](../external/PTX-EMU/docs/adr/ADR-0029-ptxemu-image-executor.md#d8-cp-端集成约定--hal-扩展方案usrlinuxemu--ptx-emu-跨仓契约) 决定采用 **HAL 扩展方案**（2026-08-09 跨仓评审修订后的采纳方案）作为 CP 端集成路径。PTX-EMU 仓已 ship `libptxemu_device.so` + `cpptlm_module.h`（D1）作为 standalone image executor，但**集成到 UsrLinuxEmu + TaskRunner 软件栈**需要新的 HAL 接口与 System C ioctl 契约。

### C2: 需求 — 3 个 ioctl + 3 个 HAL fn-ptr

PTX-EMU `cpptlm_module.h` 暴露 5 个 ABI 函数：
- `ptxemu_image_load(image_bytes, size)` → opaque handle
- `ptxemu_image_kernel_name(handle, buf, size)`
- `ptxemu_image_execute(handle, grid, block, args, args_count, shared_mem)`
- `ptxemu_image_unload(handle)`
- `ptxemu_module_version()`

HAL 方案（[PTX-EMU ADR-0029 §D8.1-D8.4](../external/PTX-EMU/docs/adr/ADR-0029-ptxemu-image-executor.md#d8-cp-端集成约定--hal-扩展方案usrlinuxemu--ptx-emu-跨仓契约)）将其压缩为 3 个 ioctl：

| System C ioctl | 对应 PTX-EMU ABI | 说明 |
|----------------|------------------|------|
| `GPU_IOCTL_LOAD_KERNEL_MODULE` (0x27) | `ptxemu_image_load` + `ptxemu_image_kernel_name` | 上传 image bytes → 返回 module handle + 内部存 kernel name |
| `GPU_IOCTL_LAUNCH_KERNEL_MODULE` (0x28) | `ptxemu_image_execute` | 提交 launch（grid/block/args），同步等待完成 |
| `GPU_IOCTL_UNLOAD_KERNEL_MODULE` (0x29) | `ptxemu_image_unload` | 卸载 module；in-flight kernel 返回 busy |

**ioctl 编号 0x27/0x28/0x29**（注：原 PTX-EMU ADR-0029 §D8.3 提案用 39/40/41，本 ADR 修订为 System C magic `'G'` 的 8-bit 范围 `0x20-0x3F`，与现有 0x01~0x26 连续 — 见 D1）

### C3: HAL 扩展治理 — append-only per ADR-023

按 [ADR-023 Decision 4](adr-023-hal-interface.md) spec-driven 扩展规则，本 ADR 严格遵循：
- ✅ 仅在 `struct gpu_hal_ops` 末尾**追加** 3 个 fn-ptr（不修改现有 65 个）
- ✅ 现有 65 个 HAL ops 签名零修改
- ✅ 现有 `hal_user.cpp` / `hal_mock.cpp` 调用方零修改（仅 `.h` 文件需要更新）
- ✅ 移植到真机时新增 fn-ptr 留空不影响（真机路径通过 `hal_user.cpp` dlsym `libptxemu_device.so`）

### C4: 与 ADR-061 (HAL IOMMU extension) 模式对齐

[ADR-061](adr-061-hal-iommu-extension.md) 是最近一次 HAL 扩展（2026-07-15 ✅ Accepted，fn-ptrs 11→13 → 现已 65 per Stage 4 累积），采用：
- spec-driven 触发（KFD page migration 需求）
- 头文件追加 + inline wrapper 模式
- `hal_mock.cpp` 完整实现 + `hal_user.cpp` 桩实现 (-ENOSYS for 真机部署)
- 错误码语义与 `sim_*` 现有错误码一致

本 ADR 完全沿用该模式，区别仅在 `hal_user.cpp` 桩实现路径不同（不是 -ENOSYS，而是 **dlsym 加载 `libptxemu_device.so` 真正调用**——这是 PTX-EMU 作为 HAL backend 的关键区别）。

### C5: 跨仓协作触发条件

[ADR-035 §R5.1](adr-035-governance-policy.md) cross-repo 协议要求：跨仓 ABI 变更须先在主 ADR（本 ADR）记录 canonical 契约，consumer-side TADR mirror 后才能 ship。本 ADR 是 PTX-EMU ADR-0029 → UsrLinuxEmu → TaskRunner 的**主 ADR**（canonical source），[tadr-307](../external/TaskRunner/docs/shared/adr/tadr-307-igpu-driver-kernel-module-extension.md) 是 TaskRunner consumer-side mirror。

### C6: TaskRunner 当前状态

TaskRunner `cu_module.cpp` 当前实现（[H-5 ABI baseline](../external/TaskRunner/src/umd/libcuda_shim/cu_module.cpp)）：
- `cuModuleLoadData`（line 135）返回 `CUDA_ERROR_NOT_IMPLEMENTED` — 需替换为 HAL 方案
- `cuLaunchKernel`（`cu_launch.cpp:62`）走 `runtime()->launch_kernel(name, grid, block, ...)` → `CudaRuntimeApi` → `IGpuDriver::launch_kernel` → `GpuDriverClient` → `GPU_IOCTL_*`
- shim 已有 16 个 `cu_*.cpp` 文件 + `cuda_driver_accessor.hpp` 的 Meyers singleton 模式

HAL 方案下集成新增 3 个 `IGpuDriver` 纯虚方法，TaskRunner 端零 PTX-EMU 链接依赖。

---

## Decision

### D1: 3 个新 System C ioctl 编号 + 结构体

```c
// plugins/gpu_driver/shared/gpu_ioctl.h（追加，零修改现有）
// 注：ioctl 编号沿用 System C magic 'G' 0x20+ 范围（0x01~0x26 已占用至 Stage 4.6）

#define GPU_IOCTL_LOAD_KERNEL_MODULE    _IOWR('G', 0x27, gpu_load_kernel_module_args)
#define GPU_IOCTL_LAUNCH_KERNEL_MODULE  _IOWR('G', 0x28, gpu_launch_kernel_module_args)
#define GPU_IOCTL_UNLOAD_KERNEL_MODULE  _IOWR('G', 0x29, gpu_unload_kernel_module_args)

struct gpu_load_kernel_module_args {
    uint64_t image_ptr;            // 用户态 image buffer (PTXIR or PTXIR-Embedded CUBIN)
    uint64_t image_size;
    uint64_t out_module_handle;    // opaque handle (PTX-EMU 端生成)
    char     kernel_name[256];     // 输出：image 内 kernel 名（PTXIR v1 single-kernel 限制）
};

struct gpu_launch_kernel_module_args {
    uint64_t module_handle;
    uint32_t grid_x, grid_y, grid_z;
    uint32_t block_x, block_y, block_z;
    uint64_t shared_mem_bytes;
    uint64_t args_ptr;             // 用户态 void** kernel_args
    uint64_t args_count;
    int32_t  launch_status;        // 输出：0 成功；非 0 cudaError_t（PTX-EMU 错误码映射）
};

struct gpu_unload_kernel_module_args {
    uint64_t module_handle;
    int32_t  unload_status;        // 输出：0 成功；-EBUSY (in-flight); 其他错误码
};
```

**ioctl 编号 0x27/0x28/0x29 论证**：
- System C 现有 ioctl 范围 0x01~0x26（[gpu_ioctl.h](plugins/gpu_driver/shared/gpu_ioctl.h) Stage 4 累积）
- **0x27/0x28/0x29 位于 0x20 GET_DEVICE_INFO 与 0x30 CREATE_VA_SPACE 之间的编号 gap 之内**（不是连续追加，是填充既有 gap 的预留位置）
- 0x27/0x28/0x29 与现有连续（不跳号）
- 与原 PTX-EMU ADR-0029 §D8.3 提案的 39/40/41 不同——39/40/41 超过 8-bit 范围不可行（本 ADR 修订）
- 预留 0x2A~0x3F 给未来扩展（multi-kernel ADR-0028 + 其他 CP 端集成）

**`kernel_name[256]` 字段**：与 PTX-EMU `PtxEmuKernelNameMax` (256 字节) 对齐；v1 单 kernel per image（per PTX-EMU ADR-0029 D4）

### D2: 3 个新 HAL fn-ptr（#66/#67/#68）

```c
// plugins/gpu_driver/hal/gpu_hal.h（追加，零修改现有 65 个）
struct gpu_hal_ops {
    void *ctx;

    /* ... 现有 65 个 fn-ptr（ADR-023 + ADR-061/062 定义，本 ADR 零修改）... */

    /* --- ADR-076 扩展（2026-08-09, PTX-EMU Image Executor HAL backend）--- */
    int (*kernel_module_load)(void *ctx,
                              const uint8_t* image_bytes, size_t image_size,
                              uint64_t* out_module_handle,
                              char* out_kernel_name, size_t kernel_name_buf_size);
    int (*kernel_module_execute)(void *ctx,
                                 uint64_t module_handle,
                                 uint32_t grid_x, uint32_t grid_y, uint32_t grid_z,
                                 uint32_t block_x, uint32_t block_y, uint32_t block_z,
                                 size_t shared_mem_bytes,
                                 void** kernel_args, size_t args_count);
    int (*kernel_module_unload)(void *ctx, uint64_t module_handle);
};
```

**inline wrapper**（与 ADR-061/062 风格一致）：

```c
static inline int hal_kernel_module_load(struct gpu_hal_ops *hal,
                                         const uint8_t* image, size_t size,
                                         uint64_t* out_handle,
                                         char* out_name, size_t name_size) {
    return hal->kernel_module_load(hal->ctx, image, size, out_handle, out_name, name_size);
}
static inline int hal_kernel_module_execute(struct gpu_hal_ops *hal,
                                            uint64_t handle,
                                            uint32_t gx, uint32_t gy, uint32_t gz,
                                            uint32_t bx, uint32_t by, uint32_t bz,
                                            size_t shared_mem_bytes,
                                            void** args, size_t args_count) {
    return hal->kernel_module_execute(hal->ctx, handle, gx, gy, gz, bx, by, bz,
                                       shared_mem_bytes, args, args_count);
}
static inline int hal_kernel_module_unload(struct gpu_hal_ops *hal, uint64_t handle) {
    return hal->kernel_module_unload(hal->ctx, handle);
}
```

### D3: `Gpuool.h` ioctl 派发表扩展

```c
// plugins/gpu_driver/drv/gpgpu_device.cpp（追加，零修改现有 38 个 handler）
static const struct ioctl_handler g_gpu_ioctl_handlers[] = {
    /* ... 现有 38 个 handler（Stage 4 累积）... */
    [GPU_IOCTL_LOAD_KERNEL_MODULE]    = handle_load_kernel_module,
    [GPU_IOCTL_LAUNCH_KERNEL_MODULE]  = handle_launch_kernel_module,
    [GPU_IOCTL_UNLOAD_KERNEL_MODULE]  = handle_unload_kernel_module,
};
```

3 个新 handler 函数实现：

```c
// plugins/gpu_driver/drv/gpgpu_device.cpp（追加）
static int handle_load_kernel_module(struct GpgpuDevice* dev, void* args_ptr) {
    auto* args = static_cast<gpu_load_kernel_module_args*>(args_ptr);
    // 安全检查：image_ptr 用户态可读，image_size 不溢出
    if (!is_user_readable(args->image_ptr, args->image_size))
        return -EFAULT;
    if (args->image_size == 0 || args->image_size > MAX_KERNEL_IMAGE_SIZE)
        return -EINVAL;
    // HAL 调用（hal_user.cpp 内 dlsym libptxemu_device.so）
    int rc = hal_kernel_module_load(dev->hal_ops,
                                     reinterpret_cast<const uint8_t*>(args->image_ptr),
                                     args->image_size,
                                     &args->out_module_handle,
                                     args->kernel_name,
                                     sizeof(args->kernel_name));
    return (rc < 0) ? rc : 0;
}

static int handle_launch_kernel_module(struct GpuDevice* dev, void* args_ptr) {
    auto* args = static_cast<gpu_launch_kernel_module_args*>(args_ptr);
    // 安全检查：args_ptr 用户态可读
    if (args->args_count > 0 && !is_user_readable(args->args_ptr, args->args_count * sizeof(void*)))
        return -EFAULT;
    // HAL 调用（同步等待 kernel 执行结束）
    args->launch_status = hal_kernel_module_execute(dev->hal_ops,
                                                   args->module_handle,
                                                   args->grid_x, args->grid_y, args->grid_z,
                                                   args->block_x, args->block_y, args->block_z,
                                                   args->shared_mem_bytes,
                                                   reinterpret_cast<void**>(args->args_ptr),
                                                   args->args_count);
    return 0;  // launch_status 由调用方读
}

static int handle_unload_kernel_module(struct GpuDevice* dev, void* args_ptr) {
    auto* args = static_cast<gpu_unload_kernel_module_args*>(args_ptr);
    args->unload_status = hal_kernel_module_unload(dev->hal_ops, args->module_handle);
    return 0;
}
```

### D4: `hal_user.cpp` PTX-EMU 作为 HAL backend 实现

```c
// plugins/gpu_driver/hal/hal_user.cpp（追加 3 个 fn-ptr 实现）

// dlsym 缓存（一次性解析，进程内复用）
static struct {
    uint64_t (*image_load)(const uint8_t*, size_t);
    int      (*image_kernel_name)(uint64_t, char*, size_t);
    int      (*image_execute)(uint64_t,
                              uint32_t, uint32_t, uint32_t,
                              uint32_t, uint32_t, uint32_t,
                              size_t, void**, size_t);
    int      (*image_unload)(uint64_t);
    int      (*module_version)(void);
    void*    handle;  // libptxemu_device.so dlopen handle
} g_ptxemu_abi;

static int ensure_ptxemu_abi_loaded(void) {
    if (g_ptxemu_abi.image_load) return 0;
    // dlsym 加载路径优先级
    const char* root = getenv("PTXEMU_ROOT");
    if (!root) root = "/opt/ptxemu";      // 默认安装路径
    // ... 实际加载逻辑（参见 ADR-076 §D4.1）
    return -ENOSYS;  // 加载失败时
}

static int hal_user_kernel_module_load(void *ctx,
                                       const uint8_t* image_bytes, size_t image_size,
                                       uint64_t* out_module_handle,
                                       char* out_kernel_name, size_t kernel_name_buf_size) {
    (void)ctx;
    int rc = ensure_ptxemu_abi_loaded();
    if (rc < 0) return rc;
    uint64_t handle = g_ptxemu_abi.image_load(image_bytes, image_size);
    if (handle == 0) return -EINVAL;
    *out_module_handle = handle;
    if (g_ptxemu_abi.image_kernel_name(handle, out_kernel_name, kernel_name_buf_size) < 0)
        return -EINVAL;  // handle 已分配但 kernel name 读取失败
    return 0;
}

static int hal_user_kernel_module_execute(void *ctx, uint64_t module_handle,
                                          uint32_t grid_x, uint32_t grid_y, uint32_t grid_z,
                                          uint32_t block_x, uint32_t block_y, uint32_t block_z,
                                          size_t shared_mem_bytes,
                                          void** kernel_args, size_t args_count) {
    (void)ctx;
    if (!g_ptxemu_abi.image_execute) return -ENOSYS;
    return g_ptxemu_abi.image_execute(module_handle,
                                       grid_x, grid_y, grid_z,
                                       block_x, block_y, block_z,
                                       shared_mem_bytes,
                                       kernel_args, args_count);
}

static int hal_user_kernel_module_unload(void *ctx, uint64_t module_handle) {
    (void)ctx;
    if (!g_ptxemu_abi.image_unload) return -ENOSYS;
    return g_ptxemu_abi.image_unload(module_handle);
}
```

**关键设计**：
- `hal_user.cpp` 真正调 `libptxemu_device.so`（**不是 -ENOSYS 桩**，与 ADR-061 不同）
- `hal_mock.cpp` 提供 unit test 路径（返回 mock handle，可注入错误码）
- `libptxemu_device.so` 通过 `PTXEMU_ROOT` env 或 `/opt/ptxemu` 默认路径查找
- **跨版本检查**：调用前 `g_ptxemu_abi.module_version() == CPPTLM_MODULE_VERSION`（per PTX-EMU cpptlm_module.h）

### D4.1: `libptxemu_device.so` 加载路径优先级

```c
// 三级 fallback（ensure_ptxemu_abi_loaded 内）
1. getenv("PTXEMU_ROOT")           // 用户显式指定（最高优先）
2. "/opt/ptxemu/lib/"              // 系统级默认安装
3. NULL（dlsym RTLD_DEFAULT）       // 已在 LD_LIBRARY_PATH 中的全局查找
```

### D5: 错误码语义

| 错误码 | 触发场景 |
|--------|---------|
| `0` | 成功 |
| `-EINVAL` | image_size 为 0 或 > MAX；module_handle 无效；image_load 返回非 0 或 handle == 0；image_kernel_name 失败（rollback 路径后）|
| `-EFAULT` | image_ptr / args_ptr 用户态不可读 |
| `-ENOMEM` | PTX-EMU 端 GPU 状态满；CUDA_ERROR_OUT_OF_MEMORY → cuda_error_to_errno 映射 |
| `-EBUSY` | unload 时 in-flight kernel |
| `-ENOSYS` | `libptxemu_device.so` 未找到（dlsym 三级 fallback 全失败）；hal backend 未初始化 |
| `-EPROTO` | PTX-EMU ABI version < 1；`ptxemu_module_version` 符号缺失 |
| `-EIO` | PTX-EMU 内部错误（ANTLR parse / deserialize / execution 失败，cudaError_t 719 LAUNCH_FAILED） |
| `-EAGAIN` | CUDA_ERROR_ILLEGAL_STATE（700）— concurrent state mismatch |

**Canonical `cudaError_t → -errno` 映射表**（定义在 `hal_user.cpp::cuda_error_to_errno`）：

| cudaError_t | value | Linux errno |
|---|---|---|
| `CUDA_SUCCESS` | 0 | `0` |
| `CUDA_ERROR_OUT_OF_MEMORY` | 2 | `-ENOMEM` |
| `CUDA_ERROR_INVALID_VALUE` | 11 | `-EINVAL` |
| `CUDA_ERROR_INVALID_HANDLE` | 400 | `-EINVAL` |
| `CUDA_ERROR_LAUNCH_FAILED` | 719 | `-EIO` |
| `CUDA_ERROR_ILLEGAL_STATE` | 700 | `-EAGAIN` |
| (unknown) | other | `-EINVAL` (safe default) |

**Load-path collapse to -EINVAL**（修订自 [Oracle review C2](../../docs/architecture/adr-076-ptxemu-hal-backend-gap-analysis.md)）：
`ptxemu_image_load` 把 parse / deserialize / state-full 错误压成单个 `handle == 0` 返回；consumer 端无法区分三者。统一映射为 `-EINVAL`，**不再尝试反推错误类型**。

**Kernel-name failure rollback**（修订自 [Oracle review H3](../../docs/architecture/adr-076-ptxemu-hal-backend-gap-analysis.md)）：
`image_load` 成功后 `image_kernel_name` 失败路径必须先调 `image_unload(handle)` 再返回 `-EINVAL`，否则 PTX-EMU 端 handle 泄漏。

**`ptxemu_module_version` 符号缺失**（修订自 [Oracle review D8.4](../../docs/architecture/adr-076-ptxemu-hal-backend-gap-analysis.md)）：
判定为 `-EPROTO` 而非兼容路径——版本符号缺失即协议错误，不允许 fallback。

**Version floor `>= 1`**（修订自 [Oracle review C1](../../docs/architecture/adr-076-ptxemu-hal-backend-gap-analysis.md)）：
`module_version()` 返回 < 1 判 `-EPROTO`。PTX-EMU shipped v0.1.0 含 v1 ABI；v2+ 引入 multi-kernel 但 v1 符号子集稳定。前向兼容。

### D6: 同步 vs 异步语义

**v1 全部同步**：
- `kernel_module_load` 同步（用户态等待 PTX-EMU 解析完成）
- `kernel_module_execute` 同步（用户态等待 kernel 执行完成）
- `kernel_module_unload` 同步（立即释放或返回 -EBUSY）

**异步延后到 v2**（per PTX-EMU ADR-0029 §D6 SINGLE-GPU-INSTANCE 假设）：
- 异步 launch 需要 PTX-EMU 端引入 fence/callback 机制（不在 v1 范围）
- TaskRunner 端 `cuLaunchKernel` 当前实现也是同步阻塞（如 [cu_launch.cpp:89-91](../external/TaskRunner/src/umd/libcuda_shim/cu_launch.cpp)），无 v1 兼容性问题

---

## Consequences

### 正面后果

- ✅ **PTX-EMU ↔ UsrLinuxEmu 集成路径确立**：HAL 扩展方案（per PTX-EMU ADR-0029 D8）成为唯一集成入口
- ✅ **TaskRunner 零改动集成**：通过 `IGpuDriver` 扩展（[tadr-307](../external/TaskRunner/docs/shared/adr/tadr-307-igpu-driver-kernel-module-extension.md)）消费新 ioctl
- ✅ **GPU 状态统一**：所有 kernel 状态、device memory、module handle 在 UsrLinuxEmu 单例（`HardwarePullerEmu` + `GpgpuDevice`）；PTX-EMU 仅作为 ISA 执行 backend
- ✅ **3 区分架构保持**：HAL 仍是 ②③ 之间唯一桥（per ADR-036）；PTX-EMU `GPUContext` 不暴露到 UsrLinuxEmu ② 层
- ✅ **HAL append-only 治理延续**：65 → 68 fn-ptrs，不修改现有签名（per ADR-023 Decision 4）
- ✅ **跨仓契约清晰**：ioctl 编号 0x27/0x28/0x29、结构体字段、HAL fn-ptr 签名在本文档 canonical，TaskRunner tadr-307 + PTX-EMU ADR-0029 §D8 引用

### 负面后果

- ⚠️ `struct gpu_hal_ops` 65 → 68 fn-ptr；`gpu_hal.h` 增长 ~30 行；`gpgpu_device.cpp` ioctl 派发表 38 → 41 行
- ⚠️ `hal_user.cpp` 必须实现 3 个 fn-ptr（**不是桩**，是 dlsym 实际调用）—— `libptxemu_device.so` 缺失时返回 -ENOSYS 而非 -ENOSYS 桩
- ⚠️ `MockGpuDriver`（ADR-032 IGpuDriver 抽象层的 consumer）需要更新 3 个新 fn-ptr 字段
- ⚠️ 错误码映射需 PTX-EMU 端 `cudaError_t` → Linux errno 转换完整列表（per `cpptlm_module.h` 错误码约定）
- ⚠️ 跨仓同步：3 个仓 commit 必须按 §Migration 顺序串行（per ADR-035 §R5.1）

### 风险

| 风险 | 等级 | 缓解 |
|------|------|------|
| `libptxemu_device.so` 加载失败（系统未装 PTX-EMU）| 🟡 中 | 三级 fallback（env → /opt → RTLD_DEFAULT）+ `-ENOSYS` 错误码返回；TaskRunner 端可显式 `PTXEMU_ROOT` 指定 |
| `cpptlm_module.h` ABI version mismatch | 🟡 中 | 启动时 `module_version()` 检查 + `static_assert` 编译期断言；不匹配返回 `-EPROTO` |
| `g_gpu_ioctl_handlers` 表新增 3 行遗漏 | 🟢 低 | CMake 检查：handler 表 size == 已定义 ioctl 数量（per ADR-039 既有约束）|
| `MockGpuDriver` 字段更新遗漏 | 🟢 低 | ADR-032 既有约束：MockGpuDriver 与 IGpuDriver 字段数必须一致 |
| PTX-EMU 端 image bytes deep copy 与 UsrLinuxEmu 端 ① 用户态映射冲突 | 🟡 中 | HAL 层负责 image bytes 持有（`ptxemu_image_load` 内 deep copy per PTX-EMU ADR-0029 D3），② 仅持 handle；与现有 BO/VA Space 模型正交 |
| 跨仓 commit 顺序错位 | 🟡 中 | §Migration 提供严格 4 步 commit 顺序（per ADR-035 §R5.1）+ CI 校验 |
| v1 单 kernel 限制拖累集成 | 🟢 低 | `kernel_name[256]` 字段已预留；multi-kernel 解锁 per PTX-EMU ADR-0028 BLOCKING DEPENDENCY |

---

## Migration / 实施步骤 + 跨仓 commit 顺序

> **canonical source for cross-repo commit sync protocol**（per ADR-035 §R5.1 协议）

### Step 1: PTX-EMU 仓 ship `libptxemu_device.so`（per PTX-EMU ADR-0029 Phase 1）

```
仓库: /workspace/project/PTX-EMU
变更: PTX-EMU ADR-0029 Phase 1 实施
产出:
  - include/cudart/cpptlm_module.h (CPPTLM_MODULE_VERSION 1)
  - build/lib/libptxemu_device.so
  - tests/unit/cudart/test_cpptlm_module.cpp
验收: PTX-EMU 仓 5 gates (D7) 全部通过
commit: PTX-EMU 仓 commit + push，tag v0.1.0
```

### Step 2: UsrLinuxEmu 仓 ship HAL extension（本 ADR 实施）

```
仓库: /workspace/project/UsrLinuxEmu
变更: openspec/changes/2026-08-15-stage5-ptxemu-kernel-module-hal-extension/
产出:
  - plugins/gpu_driver/shared/gpu_ioctl.h
    (新增 GPU_ioctl_LOAD/LAUNCH/UNLOAD_KERNEL_MODULE 0x27/0x28/0x29 + 3 个结构体)
  - plugins/gpu_driver/hal/gpu_hal.h
    (struct gpu_hal_ops 追加 3 个 fn-ptr #66/#67/#68 + inline wrappers)
  - plugins/gpu_driver/hal/hal_user.cpp
    (实现 3 个 fn-ptr：dlsym libptxemu_device.so + 5 ABI 函数调用)
  - plugins/gpu_driver/hal/hal_mock.cpp
    (实现 3 个 fn-ptr：mock handle + 可注入错误码 for unit test)
  - plugins/gpu_driver/drv/gpgpu_device.cpp
    (ioctl 派发表追加 3 行 + 3 个 handler 实现)
  - tests/unit/hal/test_hal_kernel_module_*.cpp
    (mock test + dlsym failure test + version mismatch test)
  - docs/00_adr/adr-076-gpgpu-kernel-module-ioctl.md (本 ADR)
验收:
  - 现有 65 个 HAL fn-ptrs 零修改（per ADR-023 append-only）
  - 现有 38 个 ioctl handler 零修改
  - 现有测试全数通过（98/98 catch2 binaries）
  - 新增 3 个 HAL fn-ptr unit test 全数通过
  - cross-repo 同步：
    - ADR-035 §R5.1 mirror：本 ADR 添加到 UsrLinuxEmu docs/00_adr/README.md 索引表
    - TaskRunner 端：通过 submodule pointer bump 同步变更
commit: UsrLinuxEmu 仓 commit + push submodule pointer bump
```

### Step 3: TaskRunner 仓 ship `IGpuDriver` extension（[tadr-307](../external/TaskRunner/docs/shared/adr/tadr-307-igpu-driver-kernel-module-extension.md) 实施）

```
仓库: /workspace/project/UsrLinuxEmu/external/TaskRunner
变更: openspec/changes/igpu-driver-kernel-module-extension/
产出:
  - include/shared/igpu_driver.hpp
    (新增 3 个纯虚方法：load_kernel_module / launch_kernel_module / unload_kernel_module)
  - src/umd/cuda_runtime_api.cpp
    (实现 3 个方法：转发到 GpuDriverClient)
  - src/test_fixture/gpu_driver_client.cpp
    (实现 3 个 wrapper：构造 ioctl args + 调用 GPU_ioctl_*)
  - src/umd/libcuda_shim/cu_module.cpp
    (cuModuleLoadData 替换 CUDA_ERROR_NOT_IMPLEMENTED → 调 load_kernel_module)
  - src/umd/libcuda_shim/cu_launch.cpp
    (cuLaunchKernel 维持现状已走 launch_kernel 链路；新增 kernel_module launch 路径)
  - src/shared/mock_gpu_driver.hpp + .cpp
    (更新 MockGpuDriver：3 个新 fn-ptr mock 实现)
  - tests/umd/test_cuda_shim.cpp
    (新增 3 个 test case：load/launch/unload round-trip + in-flight unload busy)
  - docs/shared/adr/tadr-307-igpu-driver-kernel-module-extension.md
验收:
  - 现有 47 个 IGpuDriver 方法签名零修改（per tadr-301 现有契约）
  - 现有 shim 16 个 cu_*.cpp 全部继续编译
  - 新增 e2e 测试：PTX-EMU mock → shim → UsrLinuxEmu mock ioctl → handle 验证
  - cross-repo 同步：
    - tadr-307 添加到 TaskRunner docs/shared/adr/README.md 索引
    - UsrLinuxEmu 端：通过 mirror 同步添加 tadr-307 索引
commit: TaskRunner 仓 commit + push
```

### Step 4: UsrLinuxEmu 仓再次 bump submodule pointer + 集成 e2e

```
仓库: /workspace/project/UsrLinuxEmu
变更: integration verification
产出:
  - tests/e2e/test_ptxemu_kernel_module_e2e.cpp
    (mock libptxemu_device.so + IGpuDriver load → launch → unload)
  - ADR-076 状态升 Accepted（如本 ADR §Acceptance gate 全部通过）
  - TaskRunner submodule pointer bump
验收:
  - 三仓 e2e 集成测试通过
  - UsrLinuxEmu 仓 cross-repo 集成测试通过
  - ADR-076 状态从 PROPOSED → Accepted
commit: UsrLinuxEmu 仓 final integration commit + push
```

---

## 关联检查清单

- [x] ADR-023 Decision 4 spec-driven 扩展：本 ADR 严格"追加不改"
- [x] ADR-035 §R5.1 治理：跨仓 ABI 变更走 canonical ADR（本 ADR）+ consumer-side TADR（[tadr-307](../external/TaskRunner/docs/shared/adr/tadr-307-igpu-driver-kernel-module-extension.md)）
- [x] ADR-035 §R5.1 mirror：本 ADR 添加到 UsrLinuxEmu `docs/00_adr/README.md` 索引表；[tadr-307](../external/TaskRunner/docs/shared/adr/tadr-307-igpu-driver-kernel-module-extension.md) 添加到 TaskRunner `docs/shared/adr/README.md` 索引
- [x] ADR-036 三区分架构：HAL 仍是 ②③ 唯一桥；PTX-EMU 作为 HAL backend 不破坏分层
- [x] ADR-018 物理隔离：GPU 驱动代码 (`plugins/gpu_driver/drv/`) 仅调 HAL 函数，不 `#include "sim/*"` 或 `"hal/*"` 内部
- [x] ADR-061/062 模式对齐：append-only fn-ptrs + inline wrapper + hal_mock 完整实现 + hal_user dlsym 实现
- [ ] TaskRunner owner 评审通过 [tadr-307](../external/TaskRunner/docs/shared/adr/tadr-307-igpu-driver-kernel-module-extension.md)（CI gate）
- [ ] PTX-EMU owner 评审通过 ADR-0029 §D8（HAL 方案）→ Phase 0 Step 0 (amend ADR-0021) 通过
- [ ] C-12 状态分布：PROPOSED 5→5（如果 ADR-076 升 Accepted 改 PROPOSED→ACCEPTED 计数）
- [ ] UsrLinuxEmu 仓 cross-repo 集成测试通过

---

## Acceptance Gate 关系

本 ADR 由 Proposed → Accepted 必须满足两个前置 gate：

1. **PTX-EMU 仓 ship 确认 gate**（**HARD gate**，未通过 → ADR 退回 Proposed）：
   - PTX-EMU ADR-0029 Phase 0 Step 0 (amend ADR-0021) merged
   - PTX-EMU ADR-0029 Phase 1 (`libptxemu_device.so` + `cpptlm_module.h`) shipped + tagged v0.1.0+
2. **TaskRunner 仓评审确认 gate**（**SOFT gate**，未通过 → ADR 仍可 Accepted，但 TaskRunner 集成延后）：
   - [tadr-307](../external/TaskRunner/docs/shared/adr/tadr-307-igpu-driver-kernel-module-extension.md) 评审通过

✅ **当前状态（2026-08-13）**：HARD gate + SOFT gate 均已通过，ADR 升 Accepted。

---

## 后续演进推迟声明（v2 修订，2026-08-15）

> **状态演进（重要）**：本 ADR ✅ Accepted 状态**保持不变**。已 ship 的实施产物（3 个 ioctl 0x27/0x28/0x29 + 3 个 HAL fn-ptr #66/#67/#68 + 144/145 ctest + `hal_user.cpp` dlsym `libptxemu_device.so`）**全部有效**。
>
> **本节目的**：明确推迟**未来演进讨论**（不是撤销或修改已 ship 实施）。

### 1. 推迟原因

当前项目焦点为推进 [ADR-088（dGPU 参考设计 — 完整硬件子系统仿真）](adr-088-dgpu-complete-simulation.md)（Stage 5.5 子项目）。ADR-088 涉及 HAL in-place 改造 + dlopen `libcpptlm_emulator.so` + 23 个 C ABI（dGPU 板卡仿真），与本 ADR（ADR-076 PTX-EMU HAL Backend）使用相同的 dlopen + env var 集成模式。

为避免两 ADR 实施时出现冲突（env var 优先级 / dlopen 顺序 / C-ABI 命名空间 / HAL 静态变量共享），**本 ADR 的后续演进讨论统一推迟到 ADR-088 升 Accepted 之后**。

### 2. 何时重启演进讨论

**触发条件**：ADR-088 升 Accepted（per ADR-035 §R2 + §R6 lifecycle）。

届时启动本 ADR 的"演进评估"工作流：

| 评估项 | 范围 | Owner |
|--------|------|-------|
| (a) PTX-EMU 与 CppTLM backend 共存契约澄清 | env var 命名空间、优先级、dlopen 顺序 | UsrLinuxEmu Architecture Team |
| (b) HAL fn-ptr 是否需要扩展以支持 CppTLM path 的 Kernel Module 加载路径 | 复用现有 3 个 fn-ptrs（load/launch/unload）还是新增 CppTLM-specific | UsrLinuxEmu Architecture Team + CppTLM maintainer |
| (c) TaskRunner [tadr-307](../external/TaskRunner/docs/shared/adr/tadr-307-igpu-driver-kernel-module-extension.md) 集成是否需要同步修订 | consumer-side mirror | TaskRunner owner |
| (d) 跨仓契约澄清文档 | Sisyphus + Metis 双层审查 P1-C 偏差（ADR-076 vs ADR-088 跨仓契约冲突）| UsrLinuxEmu Architecture Team |

### 3. 与 ADR-088 共存关系（明确不变）

**[ADR-088 §C2](adr-088-dgpu-complete-simulation.md) 已明确**：

> "ADR-076 (PTX-EMU): **不被取代** — Kernel Module 仍走 PTX-EMU path"

两者**共存**，**不冲突**：

| 后端 | 触发 env var | dlopen 目标 | 用途 |
|------|--------------|-------------|------|
| **PTX-EMU**（本 ADR）| `PTXEMU_ROOT` 或 `/opt/ptxemu` 或 RTLD_DEFAULT | `libptxemu_device.so` | Kernel Module 加载/执行/卸载（IOCTL 0x27/0x28/0x29）|
| **CppTLM**（ADR-088）| `USR_LINUX_EMU_USE_CPPTLM=1` | `libcpptlm_emulator.so` | dGPU 板卡仿真（23 ABI；系统 IOMMU/CXL.mem 由 UsrLinuxEmu `src/system_hw/` 提供）|

两个后端通过不同 env var 触发，互不干扰；hal_user.cpp 可同时配置两个后端（per ADR-088 §C2 明确）。

### 4. 推迟期间的维护原则

- ✅ **不修改**：已 ship 的 3 个 ioctl 0x27/0x28/0x29、3 个 HAL fn-ptrs #66/#67/#68、144 个 ctest、PTX-EMU 集成路径
- ✅ **不撤销**：ADR 状态保持 ✅ Accepted
- ✅ **不阻塞**：ADR-088 实施进度（本 ADR 推迟不影响 ADR-088 实施）
- ⏸️ **不演进**：本 ADR 不接受新需求/扩展，直到 ADR-088 升 Accepted
- ⚠️ **必须记录**：如发现 PTX-EMU 集成 bug 或生产问题，按正常 issue 流程处理（不阻塞本推迟声明）

### 5. 退出条件

**退出"后续演进推迟"状态**（即重启演进讨论）需满足：

1. ~~ADR-088 升 Accepted~~ → **✅ 已触发**（2026-08-15 [ADR-088 升 ✅ Accepted](adr-088-dgpu-complete-simulation.md)，Oracle 二次评审通过）
2. ~~UsrLinuxEmu owner 启动 ADR-076 v2 修订工作流~~ → **✅ 已触发**（2026-08-15 同步执行）
3. ~~在本 ADR 创建"## 演进路线图"章节，列出 (a)-(d) 评估项的具体决策~~ → **✅ 已完成**（详见 §演进路线图）

**退出状态确认**（2026-08-15）：3 项退出条件全部满足，**本 ADR 已退出"后续演进推迟"状态**，进入正常演进轨道。后续演进讨论按 (a)-(d) 评估项的具体决策推进。

**注**：本节不阻塞 ADR-088 任何阶段（包括 PROPOSED → UNDER_REVIEW → ACTIVE → ACCEPTED）；ADR-088 升 Accepted 是本推迟声明的"重启触发条件"，而非"前置条件"。

---

## 演进路线图（**v3 修订 2026-08-15** — 退出条件 3 完成）

> **背景**：ADR-088 于 2026-08-15 升 ✅ Accepted（Oracle 二次评审通过），触发本 ADR §后续演进推迟声明 §5 退出条件。本节列出 (a)-(d) 4 项评估项的具体决策，作为后续演进讨论的工作框架。（注：ADR-088 早期多版本（v2/v3/v4/v5）演进期间的两次"重新校准"记录已随 ADR-088 单文件整合（2026-08-16）移除；(a)(b) 结论始终维持不变，(c)(d) 状态见下。）

### (a) PTX-EMU 与 CppTLM backend 共存契约澄清

**决策**（**已确定 2026-08-15**）：**共存关系维持**（不修改）。

**依据**：Oracle 二次评审通过 4 维无冲突验证：
- **库名**：`libptxemu_device.so`（本 ADR）vs `libcpptlm_emulator.so`（ADR-088）— 无冲突 ✓
- **Env var**：`USR_LINUX_EMU_USE_PTXEMU=1` vs `USR_LINUX_EMU_USE_CPPTLM=1` — 无冲突 ✓
- **C-ABI 命名空间**：`ptxemu_*` vs `cpptlm_emulator_*` — 独立命名空间 ✓
- **HAL 静态变量**：`hal_user_context.use_ptxemu_backend`（bool）vs `hal_user_context.use_cpptlm_backend`（bool）— 独立字段 ✓

**后续行动**：无需修改，per ADR-088 §C2 明确"ADR-076 (PTX-EMU): **不被取代** — Kernel Module  仍走 PTX-EMU path"。

### (b) HAL fn-ptr 是否需要扩展以支持 CppTLM path 的 Kernel Module 加载路径

**决策**（**已确定 2026-08-15**）：**不新增 fn-ptr，复用现有 3 个 kernel_module_* fn-ptrs**。

**依据**：
- ADR-088 维持 68 fn-ptrs 不变（in-place 替换不改 HAL 接口契约，per ADR-023 §D4 append-only 治理）
- `kernel_module_load/execute/unload` (#66/#67/#68) 设计为 ABI 通用，不限于 PTX-EMU backend
- CppTLM path 的 Kernel Module 加载路径可由 `libcpptlm_emulator.so` 通过 `cpptlm_emulator_*` 自行实现，不需 HAL 层扩展
- ADR-023 §D4 append-only 治理明确禁止在无新 spec-driven 需求时新增 fn-ptr

**后续行动**：无需修改，per ADR-076 D2 + ADR-088 §C2 共存关系。

### (c) TaskRunner [tadr-307](../external/TaskRunner/docs/shared/adr/tadr-307-igpu-driver-kernel-module-extension.md) 集成是否需要同步修订

**决策**（**待 TaskRunner owner 启动评估**）。

**依据**：ADR-088 升 Accepted 后，TaskRunner tadr-307 的 IGpuDriver kernel module extension 集成可能需要扩展（以支持 CppTLM path），但需 TaskRunner owner 评估：
- 是否需要在 IGpuDriver 接口层添加 mode 切换字段（PTX-EMU vs CppTLM）？
- tadr-307 的 3 个纯虚方法（load_kernel_module / launch_kernel_module / unload_kernel_module）是否需要按 backend 分流？
- 是否需要在 `cu_module.cpp::cuModuleLoadData` 添加 mode 选择逻辑？

**Owner**: TaskRunner owner
**预计启动时间**：ADR-088 实施 Gate 4（总集成完成）后
**跟踪 issue**: （待 TaskRunner owner 建 #tadr-307-followup）

### (d) 跨仓契约澄清文档

**决策**（**待文档化 2026-08-15 启动**）。

**依据**：ADR-088 Oracle 二次评审确认 4 维无冲突后，需编写独立的"ADR-076 vs ADR-088 跨仓契约澄清"文档，作为实施期间的权威依据：
- env var 优先级（同时设置 `USR_LINUX_EMU_USE_PTXEMU=1` + `USR_LINUX_EMU_USE_CPPTLM=1` 时的行为）
- dlopen 顺序（两个 .so 的加载顺序、依赖关系）
- C-ABI 命名空间互不干扰（已在 Oracle 评审中验证）
- HAL 静态变量共享（已在 Oracle 评审中验证）

**Owner**: UsrLinuxEmu Architecture Team
**预计创建时间**：ADR-088 实施阶段 1 启动前
**文档路径**：`docs/05-advanced/adr-076-vs-088-cross-backend-contract.md`（待建）

### 演进路线图时间表

| 时间点 | 事件 | 触发条件 | 备注 |
|--------|------|----------|------|
| 2026-08-15 | (a) 已确定 | Oracle 评审通过 4 维无冲突验证 | ✅ |
| 2026-08-15 | (b) 已确定 | ADR-023 §D4 append-only 治理 | ✅ |
| 2026-08-15 | (d) 文档化启动 | ADR-088 升 Accepted | ⏳ 进行中 |
| 2026-08-15+ | (c) TaskRunner owner 启动 | ADR-088 实施 Gate 4 | ⏳ 等待 |
| ADR-088 阶段 1 | (d) 完成 | libcpptlm_emulator.so 加载框架就绪 | ⏳ 待 |
| ADR-088 阶段 2a | (c) 进展 | MSI-X + DMA translate cb 验证 | ⏳ 待 |
| ADR-088 阶段 4 | 全部完成 | 23 ABI dispatch 到 CppTLM + system_hw 集成 | ⏳ 待 |

---

**维护者**: UsrLinuxEmu Architecture Team（canonical source）
**最后更新**: 2026-08-09（v1 草案）→ 2026-08-13（v1 升 Accepted）→ 2026-08-15（v2 演进推迟声明 → v3 退出推迟 + 创建演进路线图章节）→ **2026-08-16（ADR-088 单文件整合：演进路线图 v4/v5 重新校准记录移除，(a)(b) 结论维持，(c)(d) 状态见 §演进路线图；各修订均不涉及已 ship 实施）**
**关联 Issue**: 暂无（建议建 #76-gpgpu-kernel-module-ioctl）
**状态**: ✅ Accepted（保持不变；2026-08-15/2026-08-16 各修订均不涉及已 ship 实施）

---

## 附录 A: 与 PTX-EMU ADR-0029 §D8 字段对齐表

| 本 ADR 字段 | PTX-EMU ADR-0029 §D8 字段 | 状态 |
|------------|---------------------------|------|
| ioctl 编号 0x27/0x28/0x29 | ioctl 编号 39/40/41 | ⚠️ **本 ADR 修订**（39/40/41 超过 8-bit；0x27/0x28/0x29 与 System C 现有 0x01~0x26 连续）|
| `gpu_load_kernel_module_args` | §D8.3 struct | ✅ 一致 |
| `gpu_launch_kernel_module_args` | §D8.3 struct | ✅ 一致 |
| `gpu_unload_kernel_module_args` | §D8.3 struct | ✅ 一致 |
| HAL fn-ptr #66/#67/#68 | §D8.4 fn-ptr | ✅ 一致 |
| `hal_user_kernel_module_load/execute/unload` | §D8.4 dlsym 实现 | ✅ 一致 |
| 错误码语义 (D5) | §D8.5 错误码 | ✅ 一致 |
| 同步语义 (D6) | §D6 SINGLE-GPU-INSTANCE | ✅ 一致 |

---

## 附录 B: 与 TaskRunner tadr-307 字段对齐表

| 本 ADR 字段 | TaskRunner tadr-307 字段 | 状态 |
|------------|---------------------------|------|
| IGpuDriver::load_kernel_module(image, size) | tadr-307 §D1 方法签名 | ✅ 一致 |
| IGpuDriver::launch_kernel_module(handle, grid, block, args, args_count, shared_mem) | tadr-307 §D1 方法签名 | ✅ 一致 |
| IGpuDriver::unload_kernel_module(handle) | tadr-307 §D1 方法签名 | ✅ 一致 |
| 错误码语义 (D5) | tadr-307 §D2 错误码 | ✅ 一致 |
| 同步语义 (D6) | tadr-307 §D3 同步假设 | ✅ 一致 |
| cuModuleLoadData / cuLaunchKernel / cuModuleUnload 调用链 | tadr-307 §D4 shim 改动 | ✅ 一致 |