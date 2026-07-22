# Migration Guide: v0.x → v1.0

> **目标读者**: 从 UsrLinuxEmu 旧版本升级到 v1.0 的开发者
> **最后更新**: 2026-07-23

---

## 目录

1. [IOCTL 系统迁移: System B → System C](#1-ioctl-系统迁移-system-b--system-c)
2. [Kernel 库 SHARED 要求](#2-kernel-库-shared-要求)
3. [目录结构重组](#3-目录结构重组)
4. [测试框架变更: GTest → Catch2](#4-测试框架变更-gtest--catch2)
5. [HAL 接口升级](#5-hal-接口升级)
6. [API 命名空间变更](#6-api-命名空间变更)

---

## 1. IOCTL 系统迁移: System B → System C

### 背景

v0.x 使用 System B (`GPGPU_*`) 宏定义的 IOCTL 命令。v1.0 引入了 System C (`GPU_IOCTL_*`) 完整替换，具有更规范的编号分配和结构体定义。

### 映射表

| System B (旧) | System C (新) | 说明 |
|---------------|---------------|------|
| `GPGPU_GET_DEVICE_INFO` | `GPU_IOCTL_GET_DEVICE_INFO` (0x20) | 查询设备能力 |
| `GPGPU_ALLOC_MEM` | `GPU_IOCTL_ALLOC_BO` (0x10) | 分配 GPU buffer object |
| `GPGPU_FREE_MEM` | `GPU_IOCTL_FREE_BO` (0x11) | 释放 GPU buffer object |
| `GPGPU_SUBMIT_PACKET` | `GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH` (0x01) | 提交 pushbuffer 批次 |

**新增命令** (v1.0 only, 无 System B 对应):

| System C 命令 | 编号 | 功能 |
|---------------|------|------|
| `GPU_IOCTL_MAP_BO` | 0x12 | 映射 BO 到 GPU VA Space |
| `GPU_IOCTL_WAIT_FENCE` | 0x13 | 等待 fence 信号 |
| `GPU_IOCTL_CREATE_VA_SPACE` | 0x30 | 创建虚拟地址空间 |
| `GPU_IOCTL_DESTROY_VA_SPACE` | 0x31 | 销毁虚拟地址空间 |
| `GPU_IOCTL_REGISTER_GPU` | 0x32 | 注册 GPU 到 VA Space |
| `GPU_IOCTL_CREATE_QUEUE` | 0x40 | 创建命令队列 |
| `GPU_IOCTL_DESTROY_QUEUE` | 0x41 | 销毁命令队列 |
| `GPU_IOCTL_MAP_QUEUE_RING` | 0x42 | 映射 Ring Buffer |
| `GPU_IOCTL_QUERY_QUEUE` | 0x43 | 查询队列信息 |
| `GPU_IOCTL_GET_PROCESS_APERTURE` | 0x44 | 查询进程 GPU 地址窗口 |
| `GPU_IOCTL_UPDATE_QUEUE` | 0x45 | 运行时更新队列属性 |
| `GPU_IOCTL_MAP_MEMORY` | 0x46 | 映射系统内存到 GPU 地址 |

### 结构体变化

System B 结构体（如 `GpuDeviceInfo`、`GpuMemoryRequest`）已废弃。所有新结构体定义在:

```
plugins/gpu_driver/shared/gpu_ioctl.h
plugins/gpu_driver/shared/gpu_types.h
```

**示例迁移**:

```cpp
// ❌ System B (旧代码)
struct GpuMemoryRequest req;
req.size = 4096;
dev->fops->ioctl(fd, GPGPU_ALLOC_MEM, &req);

// ✅ System C (v1.0)
gpu_alloc_bo_args args{};
args.size = 4096;
args.domain = GPU_MEM_DOMAIN_VRAM;
dev->fops->ioctl(fd, GPU_IOCTL_ALLOC_BO, &args);
```

### 消除 System B 残留

`tools/docs-audit.sh` 的 §2.3 会检查 `GPGPU_*` 宏是否出现在非 archive 代码中。如果你在 `drivers/`、`src/`、`include/`、`tests/` 或 `plugins/` 中收到 `GPGPU_*` 审计失败，请替换为 `GPU_IOCTL_*`。

System B 的历史代码在 `archive/system_b_drivers/gpu/` 中仅供参考。

---

## 2. Kernel 库 SHARED 要求

### 问题 (Issue #11)

`VFS::instance()` 使用 Meyers 单例（函数内 `static` 局部变量）。如果 `kernel` 是 STATIC 库:

- 可执行文件拥有一个 VFS 单例副本
- 每个 `dlopen` 的插件拥有各自的 VFS 单例副本
- 结果：VFS 状态割裂 — 在可执行文件注册的设备在插件中不可见

### 解决方案

`src/CMakeLists.txt` 中的 `kernel` 库**必须**声明为 SHARED:

```cmake
# ✅ 正确
add_library(kernel SHARED ...)

# ❌ 错误 — 导致 VFS 单例割裂
add_library(kernel STATIC ...)
```

### 对你的影响

如果你的项目直接链接 `kernel`，确保使用动态链接（默认行为）。如果你的构建系统自定义了链接方式，请改为链接 `libkernel.so` 而不是直接将 `.o` 文件嵌入。

`tools/docs-audit.sh` §1.1 强制执行此约束。

---

## 3. 目录结构重组

### Phase 1.5 重组 (v0.1.7)

重构将 GPU 驱动拆分为物理分离的目录:

```
旧结构 (Phase 1 及之前):              新结构 (Phase 1.5+):
plugins/gpu_driver/                   plugins/gpu_driver/
├── gpgpu_device.cpp                  ├── drv/             ← 可移植驱动代码
├── gpu_scheduler.cpp                 │   └── gpgpu_device.cpp
├── hardware_puller_emu.cpp           ├── hal/             ← HAL 桥接
├── gpufifo_translator.cpp            │   ├── gpu_hal.h    (14 个函数指针)
└── shared/                           │   ├── hal_user.cpp
    ├── gpu_ioctl.h                   │   └── hal_mock.cpp
    └── gpu_types.h                   ├── sim/             ← 硬件仿真
                                      │   ├── scheduler/  (GlobalScheduler)
                                      │   └── hardware/   (HardwarePullerEmu)
                                      └── shared/          ← 公共定义
                                          ├── gpu_ioctl.h
                                          └── gpu_types.h
```

### Archive 目录

历史代码归档到 `archive/` 下:

| 路径 | 内容 |
|------|------|
| `archive/system_b_drivers/gpu/` | System B (`GPGPU_*`) 驱动代码 |
| `archive/orphaned_simulator/gpu/` | 旧 GPU 仿真器 |
| `archive/old_gpu_device/` | 旧 `GpuDevice` 基类 |
| `archive/historical-plans-2026-06-15/` | 历史项目路线图 |

### 关键路径映射

| 旧位置 | 新位置 |
|--------|--------|
| `plugins/gpu_driver/gpgpu_device.cpp` | `plugins/gpu_driver/drv/gpgpu_device.cpp` |
| `plugins/gpu_driver/gpu_scheduler.cpp` | `plugins/gpu_driver/sim/scheduler/` |
| `plugins/gpu_driver/hardware_puller_emu.cpp` | `plugins/gpu_driver/sim/hardware/` |
| `plugins/gpu_driver/shared/gpu_ioctl.h` | `plugins/gpu_driver/shared/gpu_ioctl.h` (不变) |
| `drivers/gpu/ioctl_gpgpu.h` | `archive/system_b_drivers/gpu/ioctl_gpgpu.h` (归档) |
| `simulator/gpu/` | `plugins/gpu_driver/sim/` (迁移后原目录清空) |

### 新增目录

| 路径 | 说明 |
|------|------|
| `plugins/net_driver/` | Stage 2: L2 Ethernet 网络驱动 (3 区分) |
| `plugins/storage_driver/` | Stage 2: 块存储设备驱动 |
| `plugins/gpu_driver/sim/` | 从 `simulator/` 迁移 |
| `libgpu_core/` | ADR-020: 纯 C buddy allocator |

---

## 4. 测试框架变更: GTest → Catch2

### 变更理由

v0.x 使用 Google Test (GTest)。v1.0 迁移到 **Catch2**，原因:

- 单文件 amalgamation (`tests/catch_amalgamated.{hpp,cpp}`) — 无需外部依赖
- Header-only 分发 — 更易 vendoring
- 更现代的 API (`TEST_CASE`、`SECTION`、`REQUIRE`/`CHECK`)

架构决策参见 [ADR-010](docs/00_adr/adr-010-gtest-migration.md) (状态: Proposed)。

### 语法对照

| GTest | Catch2 |
|-------|--------|
| `TEST(SuiteName, TestName)` | `TEST_CASE("description", "[tag]")` |
| `EXPECT_EQ(a, b)` | `CHECK(a == b)` |
| `ASSERT_EQ(a, b)` | `REQUIRE(a == b)` |
| `TEST_F(Fixture, TestName)` | `TEST_CASE("...")` + 嵌套 `SECTION` |
| `EXPECT_TRUE(expr)` | `CHECK(expr)` |
| `EXPECT_THROW(expr, exc)` | `CHECK_THROWS_AS(expr, exc)` |

### 测试运行

```bash
# 从项目根目录运行（重要：插件路径是相对路径）
cd /workspace/project/UsrLinuxEmu

# 运行所有测试
cd build && ctest --output-on-failure

# 运行单个测试二进制
./build/bin/test_gpu_ioctl_standalone
```

### 注意事项

- 测试二进制名称后缀为 `_standalone`（CMake conventions）
- 所有测试必须从**项目根目录**运行（`ModuleLoader::load_plugins("plugins")` 使用相对路径）
- `tools/docs-audit.sh` §5.5 检查 `catch_amalgamated.{hpp,cpp}` 是否存在且无 GTest `find_package`

---

## 5. HAL 接口升级

### HAL 函数指针表

v1.0 的 `struct gpu_hal_ops` 包含 **14 个函数指针**（v0.x 有 11 个）。新增 3 个（ADR-061/062）:

| 索引 | 函数指针 | 用途 | 备注 |
|------|---------|------|------|
| 1-11 | `fence_create` 等 | 基础 ops | v0.x 兼容 |
| 12 | `iommu_map` | IOMMU 页表映射 | v1.0 新增 |
| 13 | `iommu_unmap` | IOMMU 页表解映射 | v1.0 新增 |
| 14 | `event_signal` | KFD 事件信号 | v1.0 新增 |

**迁移**: 如果你的代码实现了自定义 HAL（即实现了 `struct gpu_hal_ops`），必须添加 3 个新的函数指针实现。参考 `hal_mock.cpp` 获取默认实现。

---

## 6. API 命名空间变更

v1.0 引入了 `usr_linux_emu::` 命名空间包裹核心类型。如果你的代码直接引用旧的无命名空间类型，需要更新:

```cpp
// ❌ 旧代码 (v0.x)
GpgpuDevice* dev = ...;

// ✅ v1.0
usr_linux_emu::GpgpuDevice* dev = ...;
```

建议在文件顶部使用 `using namespace usr_linux_emu;` 来最小化改动量。

---

## 升级检查清单

- [ ] 将所有 `GPGPU_*` IOCTL 调用替换为 `GPU_IOCTL_*`
- [ ] 确认 `kernel` 库链接为 SHARED (`libkernel.so`)
- [ ] 更新 include 路径以匹配新目录结构
- [ ] 将测试从 GTest 迁移到 Catch2
- [ ] 如果实现自定义 HAL，添加 3 个新函数指针
- [ ] 添加 `usr_linux_emu::` 命名空间前缀或 using 声明
- [ ] 运行 `tools/docs-audit.sh --strict` 确认无审计失败
- [ ] 运行 `ctest` 确认所有测试通过

---

## 获取帮助

- **架构文档**: [docs/02_architecture/post-refactor-architecture.md](docs/02_architecture/post-refactor-architecture.md)
- **ADR 索引**: [docs/00_adr/README.md](docs/00_adr/README.md)
- **Issue #11**: VFS 单例割裂问题
- **GitHub Issues**: https://github.com/chisuhua/UsrLinuxEmu/issues
