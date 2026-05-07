# ADR-018: 驱动/仿真代码分离策略

**状态**: 已接受 (Accepted)

**日期**: 2026-05-06

**提案人**: Sisyphus (基于 ADR-015 架构分析与插件代码审查)

**评审者**: UsrLinuxEmu Architecture Team

**关联 ADR**: ADR-015 (GPU IOCTL Unification), ADR-019 (DRM/GEM/TTM 对齐), ADR-023 (仿真层接口契约 HAL)

---

## 背景

当前 `plugins/gpu_driver/plugin.cpp`（635 行单文件）混合了两类本质不同的代码：

**类型 A — 驱动代码（移植目标）**：
- `handle_pushbuffer_submit_batch()` — GPFIFO 分发
- `handle_wait_fence()` — fence 同步
- `handle_alloc_bo()` / `handle_free_bo()` / `handle_map_bo()` — 内存分配
- `handle_get_device_info()` — 设备信息查询

**类型 B — 仿真代码（仅仿真环境）**：
- `class BuddyAllocator` — 模拟 VRAM 分配器
- `fences_` map + 轮询等待 — 模拟 fence 硬件
- `registered_kernels_` map — 模拟 kernel 注册表
- `std::cout` / `std::this_thread::sleep_for` — 模拟日志和等待

这种混合导致三个问题：
1. **不可移植** — 驱动代码中夹杂 `std::thread`、`std::mutex`、`std::cout`，到内核必须全部替换
2. **职责不清** — 新开发者不清楚哪些代码可以动，哪些不可以动
3. **不可独立测试** — 仿真逻辑和驱动逻辑无法分开测试

---

## 决策

### 决策 1: 物理目录分离

`plugins/gpu_driver/` 拆分为三个子目录：

```
plugins/gpu_driver/
├── drv/             # 驱动代码（移植到真实内核的目标）
│   ├── gpgpu_device.cpp     # GpgpuDevice：ioctl handler、fops
│   ├── gpu_ioctl_handlers.cpp  # 各 ioctl 具体实现
│   ├── gpu_gem_object.cpp   # GEM object 生命周期（见 ADR-019）
│   ├── gpu_va_space.cpp     # VA Space 管理（见 ADR-017）
│   ├── gpu_queue.cpp        # Queue/Channel 管理（见 ADR-017）
│   └── gpu_fence.cpp        # Fence tracker（见 ADR-017）
│
├── sim/             # 仿真代码（纯仿真，不移植）
│   ├── buddy_allocator.cpp  # BuddyAllocator 模拟 VRAM
│   ├── fence_sim.cpp        # 模拟 fence 信号
│   ├── hardware_puller_emu.cpp  # GPFIFO 状态机（见 ADR-021）
│   └── gpu_core_emu.cpp     # GPU 计算单元仿真（见 ADR-022）
│
├── hal/             # 硬件抽象层接口（见 ADR-023）
│   ├── gpu_hal.h            # 驱动调用的硬件接口
│   └── hal_user.cpp         # 用户态实现（调用 sim/）
│
└── shared/          # Canonical 接口（与 TaskRunner 共享，ABI 契约）
    ├── gpu_ioctl.h
    ├── gpu_types.h
    ├── gpu_regs.h
    └── gpu_events.h
```

### 决策 2: 依赖方向 — 驱动 → HAL → 仿真

```
drv/  (驱动代码)  ──►  hal/  (抽象接口)  ──►  sim/  (仿真实现)
  │                      │                       │
  │  ioctl handler       │  gpu_hal.h 定义       │  buddy_allocator, puller
  │  调用 HAL 接口        │  硬件读/写/中断接口    │  HW 行为仿真
  │                      │                       │
  ▼                      ▼                       ▼
移植到内核后替换为真实硬件寄存器访问        删除或转为测试 mock
```

**关键规则**：
- `drv/` 不直接调用 `sim/` — 所有硬件访问通过 `hal/` 接口
- `sim/` 不依赖 `drv/` — 可独立编译和测试
- `shared/` 两者共用 — 是 ABI 契约，不属于任何一边

### 决策 3: 驱动代码使用可移植 C++ 子集

| 允许 | 禁止 |
|------|------|
| 基础 C++（类、继承、虚函数） | RTTI（`typeid`、`dynamic_cast`） |
| `linux_compat` 的链表/容器 | `std::vector`、`std::map`（真实内核无 STL） |
| Linux 错误码（`-EINVAL`、`-ENOMEM`） | C++ 异常（`throw`/`catch`） |
| 简单 RAII（`lock_guard` 等价） | `std::shared_ptr`、`std::unique_ptr` |
| `int`、`u32`、`u64`（linux_compat 类型） | `std::cout`、`std::this_thread` |

### 决策 4: 仿真代码作为永久测试环境

移植到真实内核后，`sim/` 目录保留：
- CI 单元测试中作为 mock 环境
- 内核驱动开发时作为用户态验证平台
- 不参与生产编译（条件编译 `#ifndef CONFIG_KERNEL_MODE`）

---

## 后果

### 正面后果
- ✅ 驱动代码可在用户态开发测试，然后移植到真实内核
- ✅ 仿真代码和驱动代码可独立演进
- ✅ 新人通过目录结构就能理解哪些代码"进内核"
- ✅ HAL 接口使得"用户态函数调用 ↔ 内核态寄存器操作"的映射清晰

### 负面后果
- ⚠️ 重构工作量：需要将现有 plugin.cpp（635 行）拆分为 ~8 个文件
- ⚠️ 接口抽象成本：HAL 层增加约 10% 的间接调用开销
- ⚠️ 开发期间需要在 3 个目录之间跳转

### 风险

| 风险 | 缓解措施 |
|------|---------|
| 目录拆分破坏现有构建 | 逐步拆分（先建目录/移动代码/保持编译），每次 PR 验证编译通过 |
| HAL 接口抽象过度 | 接口只包含真正需要抽象的 5-8 个函数（register read/write、memcpy、interrupt） |
| 开发效率短期下降 | 并行保留 `plugin.cpp` 到拆分完成，拆分期间不新增功能 |

---

## 实施步骤

1. 创建 `drv/`、`sim/`、`hal/` 三个目录和对应 `CMakeLists.txt`
2. 提取 `BuddyAllocator` → `sim/buddy_allocator.cpp`（见 ADR-020）
3. 提取 ioctl handler 和 GpgpuDevice 类框架 → `drv/gpgpu_device.cpp`
4. 定义 HAL 接口头文件 → `hal/gpu_hal.h`
5. 实现 HAL 用户态 → `hal/hal_user.cpp`
6. 调整 CMake 构建，确保 `drv/` 只依赖 `hal/`，不直接依赖 `sim/`
7. 删除旧的 `plugin.cpp`

---

**维护者**: UsrLinuxEmu Architecture Team

**最后更新**: 2026-05-06
