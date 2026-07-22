# 第一个 GPU 示例

> **最后验证**: 2026-07-22 (commit `ca561fc`)
>
> **状态**: ✅ 对齐 System C (Phase 2) 架构；编译运行实测通过
> **实测耗时**（不含 `git clone`，4 核 VM）：CMake 配置 ~2s，首次编译 ~6 min，运行示例 ~2s（预期 < 15 min）
>
> **SSOT**: [`docs/02_architecture/post-refactor-architecture.md`](../02_architecture/post-refactor-architecture.md) §1.3 / §1.4 / 附录 A

本教程带你跑通一个端到端 GPU 命令提交流程。从打开设备，到创建虚拟地址空间、命令队列、映射环形缓冲区、分配显存、提交 GPFIFO 条目、等待 fence，最后按相反顺序释放资源。整个流程跑完大约 15 分钟。

## 前提条件

在开始之前，确认你已经完成：

- [x] 已完成 [安装](installation.md)
- [x] 已完成 [构建](building.md)，`build/bin/` 目录有 `test_gpu_ioctl_standalone` 等二进制
- [x] 当前目录是项目根目录（加载插件使用相对路径 `plugins/`）

不需要 root 权限，不需要真实 GPU。

## 架构速览

UsrLinuxEmu 当前架构分四层，调用从用户态贯穿到硬件仿真：

```
用户应用
   ↓ ioctl(fd, GPU_IOCTL_*)
内核模拟框架 (VFS, ModuleLoader, kernel SHARED)
   ↓ dlopen("plugins/plugin_gpu_driver.so")
GPU 驱动 (GpgpuDevice + HAL)
   ↓ HAL ops
硬件仿真 (HardwarePullerEmu, GlobalScheduler, GpuQueueEmu)
```

我们关注的接口边界只有两条：用户态到框架的 `VFS::instance().open()` 与 `dev->fops->ioctl()`，框架到驱动的 ioctl 编号表。两边都遵循 Linux 风格错误码（`0` 成功，`-EINVAL`、`-EBUSY` 等表示失败）。

完整分层与数据流参考 [SSOT §1.2](../02_architecture/post-refactor-architecture.md#12-架构一张图) 和 [SSOT §1.3](../02_architecture/post-refactor-architecture.md#13-关键数据流phase-2-完整版)。

## 数据模型

Phase 2 引入了三个必须理解的对象，它们是任何 GPU 命令提交的前置：

```
VASpace (u64 handle)
  ├─ page_size: 0=4KB, 1=64KB
  └─ attached_queues: [queue_handle, ...]
        ↓
Queue (u64 handle)
  ├─ queue_type: COMPUTE=0 / COPY=1 / GRAPHICS=2
  ├─ priority: 0-100
  ├─ ring_size: entry 数, 最大 1024
  ├─ ring_buffer: gpu_ring_header (shm-backed, write_idx/read_idx 原子)
  └─ doorbell: mmap 偏移 0x10000 + h*0x1000
```

- `VASpace` 拥有 GPU 虚拟地址分配。`gpu_va` 在 BO 分配时由驱动从当前 VA Space 划出。
- `Queue` 必须挂在某个 `VASpace` 之下，孤立 Queue 不存在。
- `Ring Buffer` 是用户态与 Puller 之间的共享内存握手点，容量受 `GPU_MAX_RING_ENTRIES` 限制。

字段定义见 [`plugins/gpu_driver/shared/gpu_ioctl.h`](../../plugins/gpu_driver/shared/gpu_ioctl.h) 与 [`plugins/gpu_driver/shared/gpu_queue.h`](../../plugins/gpu_driver/shared/gpu_queue.h)。结构图参考 [SSOT §1.4](../02_architecture/post-refactor-architecture.md#14-数据模型va-space--queue--ring-buffer)。

## 完整示例: 端到端 GPU 命令提交

下面的程序覆盖 Phase 2 完整流程：先建 VA Space，再建 Queue，再分配显存，再提交 batch，最后等 fence 完成。所有结构体字段与 ioctl 编号都直接来自 `plugins/gpu_driver/shared/gpu_ioctl.h`。

### 源码

把下面内容存为 `/tmp/first_gpu_run.cpp`（这是临时文件，不要放进仓库。仓库内没有用户代码示例目录，编译时用绝对路径调用 `g++` 即可）：

```cpp
// /tmp/first_gpu_run.cpp, 端到端 Phase 2 GPU 命令提交示例
// 编译方法见本节"编译与运行"小节

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "gpu_driver/shared/gpu_ioctl.h"
#include "gpu_driver/shared/gpu_queue.h"
#include "gpu_driver/shared/gpu_types.h"
#include "kernel/file_ops.h"
#include "kernel/module_loader.h"
#include "kernel/vfs.h"

using namespace usr_linux_emu;

static int check(long ret, const char* what) {
  if (ret != 0) {
    std::fprintf(stderr, "[FAIL] %s: ret=%ld\n", what, ret);
    return 1;
  }
  std::printf("[ OK ] %s\n", what);
  return 0;
}

int main() {
  int rc = 0;

  // 1. 加载 GPU 插件（相对路径，必须从项目根目录运行）
  ModuleLoader::load_plugins("plugins");

  // 2. 打开设备
  auto dev = VFS::instance().open("/dev/gpgpu0", O_RDWR);
  if (!dev) { std::fprintf(stderr, "open /dev/gpgpu0 failed\n"); return 1; }
  int fd = 0;  // 模拟 fd；UsrLinuxEmu 当前是单 fd 设计

  // 3. 查询设备能力
  struct gpu_device_info info {};
  rc |= check(dev->fops->ioctl(fd, GPU_IOCTL_GET_DEVICE_INFO, &info),
              "GET_DEVICE_INFO");
  std::printf("       VRAM=%lu MB, CUs=%u, marketing_name=%s\n",
              info.vram_size / (1024 * 1024), info.compute_units,
              info.marketing_name);

  // 4. 创建 VA Space (Phase 2 强制前置)
  struct gpu_va_space_args va_args {};
  va_args.page_size = 1;  // 64KB
  va_args.flags = 0;
  rc |= check(dev->fops->ioctl(fd, GPU_IOCTL_CREATE_VA_SPACE, &va_args),
              "CREATE_VA_SPACE");
  gpu_va_space_handle_t va_handle = va_args.va_space_handle;

  // 5. 在 VA Space 下创建 Compute Queue
  struct gpu_queue_args q_args {};
  q_args.va_space_handle = va_handle;
  q_args.queue_type = GPU_QUEUE_COMPUTE;
  q_args.priority = 50;
  q_args.ring_buffer_size = 256;
  rc |= check(dev->fops->ioctl(fd, GPU_IOCTL_CREATE_QUEUE, &q_args),
              "CREATE_QUEUE");
  gpu_queue_handle_t queue = q_args.queue_handle;
  uint64_t doorbell_pgoff = q_args.doorbell_pgoff;
  std::printf("       queue=%lu doorbell_pgoff=0x%lx\n", queue, doorbell_pgoff);

  // 6. 准备共享内存 Ring Buffer 并 mmap
  constexpr size_t kRingBytes = sizeof(gpu_ring_header) + 256 * sizeof(gpu_gpfifo_entry);
  void* ring_shm = mmap(nullptr, kRingBytes, PROT_READ | PROT_WRITE,
                        MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  std::memset(ring_shm, 0, kRingBytes);
  struct gpu_queue_map_ring_args map_args { .queue_handle = queue,
                                            .ring_addr = reinterpret_cast<uint64_t>(ring_shm) };
  rc |= check(dev->fops->ioctl(fd, GPU_IOCTL_MAP_QUEUE_RING, &map_args),
              "MAP_QUEUE_RING");

  // 7. 分配显存 (BO)
  struct gpu_alloc_bo_args bo_args {
    .size = 4 * 1024 * 1024, .domain = GPU_MEM_DOMAIN_VRAM,
    .flags = GPU_BO_DEVICE_LOCAL, .handle = 0, .gpu_va = 0
  };
  rc |= check(dev->fops->ioctl(fd, GPU_IOCTL_ALLOC_BO, &bo_args), "ALLOC_BO");
  std::printf("       bo handle=%u gpu_va=0x%lx\n", bo_args.handle, bo_args.gpu_va);

  // 8. 提交一次 GPU_MEMCPY batch
  struct gpu_gpfifo_entry entry {};
  entry.valid = 1;
  entry.method = GPU_OP_MEMCPY;
  entry.payload[0] = 0x1000;             // src GPU VA
  entry.payload[1] = bo_args.gpu_va;     // dst
  entry.payload[2] = 4096;               // bytes

  struct gpu_pushbuffer_args pb_args {
    .stream_id = 0, .entries_addr = reinterpret_cast<uint64_t>(&entry),
    .count = 1, .flags = 0, .fence_id = 0
  };
  rc |= check(dev->fops->ioctl(fd, GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH, &pb_args),
              "PUSHBUFFER_SUBMIT_BATCH");
  uint64_t fence_id = pb_args.fence_id;

  // 9. 等待 fence
  struct gpu_wait_fence_args w_args { .fence_id = fence_id, .timeout_ms = 5000, .status = 0 };
  rc |= check(dev->fops->ioctl(fd, GPU_IOCTL_WAIT_FENCE, &w_args), "WAIT_FENCE");
  std::printf("       fence status=%u\n", w_args.status);

  // 10. 清理（顺序与创建相反）
  u32 bo_handle = bo_args.handle;
  dev->fops->ioctl(fd, GPU_IOCTL_FREE_BO, &bo_handle);
  dev->fops->ioctl(fd, GPU_IOCTL_DESTROY_QUEUE, &queue);
  dev->fops->ioctl(fd, GPU_IOCTL_DESTROY_VA_SPACE, &va_handle);
  munmap(ring_shm, kRingBytes);
  dev.reset();
  ModuleLoader::unload_plugins();
  return rc;
}
```

### 编译与运行

```bash
# 1. 确认已在项目根目录
cd /workspace/project/UsrLinuxEmu

# 2. 编译（直接 g++ 链接 kernel SHARED 与 VFS/ModuleLoader 头路径）
#    注：libkernel.so 位于 build/src/，--allow-shlib-undefined 允许运行时 dlopen 解析插件符号
g++ -std=c++17 -O0 -g \
    -I include \
    -I include/kernel \
    -I include/kernel/device \
    -I include/linux_compat \
    -I plugins \
    -L build/src \
    -Wl,-rpath,$(pwd)/build/src \
    -Wl,--allow-shlib-undefined \
    /tmp/first_gpu_run.cpp -o /tmp/first_gpu_run -lkernel

# 3. 运行（必须从项目根目录，插件路径是相对的）
/tmp/first_gpu_run
```

预期输出（数值随实现而变）：

```
[ OK ] GET_DEVICE_INFO
       VRAM=256 MB, CUs=8, marketing_name=UsrLinuxEmu GPGPU v1.0
[ OK ] CREATE_VA_SPACE
[ OK ] CREATE_QUEUE
       queue=1 doorbell_pgoff=0x10000
[ OK ] MAP_QUEUE_RING
[ OK ] ALLOC_BO
       bo handle=1 gpu_va=0x100000
[ OK ] PUSHBUFFER_SUBMIT_BATCH
[ OK ] WAIT_FENCE
       fence status=1
```

### 验证构建

`first_gpu_run.cpp` 跑通后，验证 Phase 2 完整约束：

```bash
# 1. 跑独立测试（覆盖 BO 分配 + PUSHBUFFER + WAIT）
cd /workspace/project/UsrLinuxEmu
./build/bin/test_gpu_ioctl_standalone

# 2. 跑 VA Space + Queue 链路测试
./build/bin/test_va_space_standalone

# 3. 跑全部 ctest（覆盖插件加载、ioctl 编号、fence 回调等）
cd /workspace/project/UsrLinuxEmu/build
ctest --output-on-failure
```

如果第 1、2 步单独可跑而你的 `first_gpu_run` 失败，差异基本只剩 ioctl 字段顺序或 `#include` 路径。逐项对照 [SSOT 附录 A](../02_architecture/post-refactor-architecture.md#附录-a完整-ioctl-编号表) 与 `plugins/gpu_driver/shared/gpu_ioctl.h` 检查。

## IOCTL 速查表

本示例用到 9 个 ioctl，完整编号与方向见下表（节选自 [SSOT 附录 A](../02_architecture/post-refactor-architecture.md#附录-a完整-ioctl-编号表)）：

| 编号 | 宏 | 方向 | 关键参数 | 何时调用 |
|------|----|------|----------|----------|
| `0x20` | `GPU_IOCTL_GET_DEVICE_INFO` | `_IOR` | `struct gpu_device_info*` | 打开设备后第一件事 |
| `0x30` | `GPU_IOCTL_CREATE_VA_SPACE` | `_IOWR` | `struct gpu_va_space_args*` | Phase 2 强制：建任何 Queue 之前 |
| `0x31` | `GPU_IOCTL_DESTROY_VA_SPACE` | `_IOW` | `gpu_va_space_handle_t*` | 所有 Queue 都销毁后 |
| `0x40` | `GPU_IOCTL_CREATE_QUEUE` | `_IOWR` | `struct gpu_queue_args*` | 必须传入有效 `va_space_handle` |
| `0x41` | `GPU_IOCTL_DESTROY_QUEUE` | `_IOW` | `gpu_queue_handle_t*` | 提交完后第一件事 |
| `0x42` | `GPU_IOCTL_MAP_QUEUE_RING` | `_IOWR` | `struct gpu_queue_map_ring_args*` | 写入 GPFIFO 之前 |
| `0x10` | `GPU_IOCTL_ALLOC_BO` | `_IOWR` | `struct gpu_alloc_bo_args*` | 提交命令前需要目标 buffer |
| `0x11` | `GPU_IOCTL_FREE_BO` | `_IOW` | `u32*`（handle） | 不再需要时 |
| `0x01` | `GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH` | `_IOW` | `struct gpu_pushbuffer_args*` | 真正干活的那一步 |
| `0x13` | `GPU_IOCTL_WAIT_FENCE` | `_IOW` | `struct gpu_wait_fence_args*` | 同步点，避免 race |

未列出的编号（`0x02` `MMU_EVENT_CB` / `0x03` `FIRMWARE_CB` / `0x12` `MAP_BO` / `0x32` `REGISTER_GPU` / `0x43` `QUERY_QUEUE`）是进阶主题，本快速开始不展开。完整字段定义直接读 [`plugins/gpu_driver/shared/gpu_ioctl.h`](../../plugins/gpu_driver/shared/gpu_ioctl.h) 与 [`plugins/gpu_driver/shared/gpu_queue.h`](../../plugins/gpu_driver/shared/gpu_queue.h)。

## 关键设计约束

- **VA Space 必现**。Phase 2 起任何 `CREATE_QUEUE` 若 `va_space_handle` 无效会返回 `-EINVAL`。没有 VA Space 时提交命令会失败。
- **销毁顺序对称**。先 `DESTROY_QUEUE` 才能 `DESTROY_VA_SPACE`，否则后者返回 `-EBUSY`。参见 `tests/test_va_space.cpp::test_cascade_destroy`。
- **Ring Buffer 容量上限**。`GPU_MAX_RING_ENTRIES = 1024`。`ring_buffer_size` 超过该值会被驱动截断或拒绝。
- **Magic number 一致**。`GPU_IOCTL_BASE = 'G'`。ioctl 编号在用户态与内核态必须严格一致，否则 EINVAL。
- **插件路径相对**。`ModuleLoader::load_plugins("plugins")` 依赖 `getcwd()`，所以必须从项目根目录运行，AGENTS.md 也有相同提醒。

## 常见错误

| 现象 | 原因 | 解决 |
|------|------|------|
| `Failed to open /dev/gpgpu0` | 没加载插件或路径错 | 确认 `plugins/plugin_gpu_driver.so` 存在；当前目录是项目根 |
| `ioctl ret=-EINVAL` | 结构体字段漏填 / Magic number 不一致 | 重新对照 `gpu_ioctl.h` 字段顺序 |
| `CREATE_QUEUE ret=-EINVAL` | `va_space_handle` 无效或没建 VA Space | 先 `CREATE_VA_SPACE` |
| `DESTROY_VA_SPACE ret=-EBUSY` | 仍有 Queue 挂载 | 先 `DESTROY_QUEUE` |
| `WAIT_FENCE status=0` | 超时 | 增大 `timeout_ms` 或检查 `fence_id` |
| `undefined reference to VFS::instance` | 链接时缺 `-lkernel` 或 `kernel` 不是 SHARED | `src/CMakeLists.txt` 中 `add_library(kernel SHARED ...)` 不可改为 STATIC（Issue #11） |
| 链接阶段报 `gpu_ioctl.h: No such file` | 缺 `-I plugins` | 参考"编译与运行"小节的命令 |

## 下一步

走通端到端流程后，你可以继续：

- 阅读 [架构总览（SSOT）](../02_architecture/post-refactor-architecture.md) §1.3 理解完整数据流：`PUSHBUFFER_SUBMIT_BATCH` → `HardwarePullerEmu` 状态机 → `GlobalScheduler` → fence 回调
- 阅读 [API 参考](../06-reference/api-reference.md) 了解 `VFS`、`ModuleLoader`、其他设备 API（该文档仍在重写中，参考 `include/kernel/*.h` 为准）
- 跑 [`test_va_space.cpp`](../../tests/test_va_space.cpp) 查看 VA Space 边界条件（无效 handle、cascade destroy）
- 接入 [TaskRunner 集成文档](../07-integration/) 学习真实任务调度路径

---

**相关源文件**：

- [`plugins/gpu_driver/shared/gpu_ioctl.h`](../../plugins/gpu_driver/shared/gpu_ioctl.h)，ioctl 编号与结构体定义
- [`plugins/gpu_driver/shared/gpu_queue.h`](../../plugins/gpu_driver/shared/gpu_queue.h)，Ring Buffer 与 Queue 参数
- [`plugins/gpu_driver/drv/gpgpu_device.h`](../../plugins/gpu_driver/drv/gpgpu_device.h)，`GpgpuDevice` 类与 handler 入口
- [`include/kernel/vfs.h`](../../include/kernel/vfs.h) 与 [`include/kernel/module_loader.h`](../../include/kernel/module_loader.h)，框架入口
- [`tests/test_gpu_ioctl.cpp`](../../tests/test_gpu_ioctl.cpp) 与 [`tests/test_va_space.cpp`](../../tests/test_va_space.cpp)，实际跑的测试，可直接对照

**最后更新**: 2026-07-22
**对应代码 commit**: `ca561fc`
