# Phase 1 实施计划 — 核心驱动框架

**关联文档**: `plans/gpu_driver_portability_plan.md` (§2)、`docs/PRD.md` (§5.1)
**对应 ADR**: `018`(代码分离) `019`(DRM骨架) `020`(libgpu_core) `021`(Puller) `023`(HAL)
**目标**: 可移植的驱动代码骨架跑通、TaskRunner 全链路连通

**更新记录**:
- 2026-05-07 v2: 拆分 P1.1 为 P1.1a+P1.1b，新增 P0.5 前置条件，修复依赖图（Oracle 审查发现）

---

## 阶段完成标准

```
□ 6 个 P0 ioctl 全部实现并通过测试
□ TaskRunner cuda_alloc → cuda_launch → cuda_wait 全链路成功
□ HAL 接口可通过构造注入切换 user/mock 两种实现
□ test_portability.sh 开始运行（初始检查 5 条核心规则）
□ ld 验证：ldd plugin.so 无 TaskRunner 依赖
```

---

## 前置条件

### P0.5: linux_compat/drm/ 头文件骨架

> **新增**（Oracle 审查发现：`drm_ioctl_desc`、`drm_driver`、`drm_gem_object` 等类型在代码库中不存在）

```
步骤：
0.5.1  创建 include/linux_compat/drm/drm_ioctl.h
       定义 struct drm_ioctl_desc { unsigned int cmd; u32 flags; int (*func)(...); }
       DRM_IOCTL_DEF_DRV() 宏

0.5.2  创建 include/linux_compat/drm/drm_driver.h
       定义 struct drm_driver（仅包含 ioctls 数组和 fops 字段，Phase 2 扩展）

0.5.3  创建 include/linux_compat/drm/drm_gem.h
       定义 struct drm_gem_object（基本生命周期：refcount、size、handle map）

0.5.4  验证
       - 模拟头文件可被 C++ 编译器正常解析
       - 与真实内核 drm/drm_*.h 的结构体布局兼容（字段顺序一致）

预计产出：3 个头文件，~300 行
```

---

## 任务分解

### P1.1a: 物理目录拆分（保留原 plugin.cpp 并行运行）

**核心思路**: 创建目录和 CMake，按功能分离代码到 drv/sim/hal，但保留原 plugin.cpp 作为活动编译目标。所有拆分后的驱动代码走"影子编译"——能编译但暂不链接到最终的 .so。

```
1.1a.1  创建目录结构
        mkdir -p plugins/gpu_driver/{drv,sim,hal,libgpu_core/{include,src,test}}

1.1a.2  创建 CMakeLists 框架
        - drv/CMakeLists.txt  → 编译为 drv 静态库（不直接依赖 sim）
        - sim/CMakeLists.txt  → 编译为 sim 静态库
        - hal/CMakeLists.txt  → 编译为 hal 静态库
        - libgpu_core/CMakeLists.txt → 编译为纯 C 静态库
        - 顶层 CMakeLists.txt: 整合子目录 + 保留原 plugin.cpp 编译
        - 所有 hal/ 头文件通过 PUBLIC 暴露给 drv/
        - sim/ 对 drv/ 不可见

1.1a.3  在 drv/ 创建 gpgpu_device.cpp（影子编译）
        从 plugin.cpp 复制并分解：
        - class GpgpuDevice（移除 BuddyAllocator，改为 HAL 依赖）
        - handle_*() ioctl handler（通过 HAL 调用，非直接调 buddy）
        - ioctl switch-case（仍保留，等 P1.1b 切到 DRM 表）

1.1a.4  在 sim/ 创建仿真类（影子编译）
        - sim/buddy_allocator.cpp → 封装 libgpu_core（P1.2 后填充）
        - sim/fence_sim.cpp → 模拟 fence
        - sim/hardware_puller_emu.cpp → 骨架
        - sim/doorbell_emu.cpp → 骨架

1.1a.5  创建 hal/ 头文件和实现（影子编译）
        - hal/gpu_hal.h（10 个函数指针，按 ADR-023 v2）
        - hal/hal_user.cpp → 调用 sim 组件
        - hal/hal_mock.cpp → mock 实现

1.1a.6  验证（影子编译）
        - cmake --build build/ 通过（新旧两套都编译）
        - ctest 全部通过（仍走原 plugin.cpp 路径）
        - drv/ + sim/ + hal/ 新代码编译无错误，但不影响运行
```

**预期产出**: 8 个新文件（影子编译），原 plugin.cpp 不变

---

### P1.1b: 切换薄入口（P1.2+P1.3+P1.4+P1.5 完成后才执行）

**核心思路**: 所有子任务就绪后，一次性从原 plugin.cpp 切换到薄入口。

```
1.1b.1  修改 plugin.cpp 为薄入口
        - 删除 600 行内联代码
        - 保留 ~40 行：
          extern "C" { module mod = { .init, .exit } }
          plugin_init_internal(): 创建 HAL → 构造 drv/GpgpuDevice → VFS 注册
          plugin_fini_internal(): VFS 注销 → 析构

1.1b.2  移除影子编译中的旧 plugin.cpp
        - 更新 CMakeLists.txt：只编译新架构，移除旧 plugin.cpp MODULE 目标

1.1b.3  验证
        - cmake --build build/ 通过
        - ctest 全部通过
        - ldd plugin.so 无额外依赖
```

**预期产出**: 1 个瘦 plugin.cpp + 旧代码完全退役

---

### P1.2: BuddyAllocator 提取为纯 C（对应 ADR-020）

**核心思路**: 将 `plugin.cpp` 中 ~150 行内联 C++ BuddyAllocator 改写为纯 C 零依赖，放入 `libgpu_core/`。

#### 步骤

```
1.2.1  创建 libgpu_core/include/gpu_buddy.h
       纯 C 接口：
       - void     gpu_buddy_init(struct gpu_buddy*, u64 base, u64 size)
       - int      gpu_buddy_alloc(struct gpu_buddy*, u64 size, u64 *out)
       - int      gpu_buddy_free(struct gpu_buddy*, u64 addr)
       - u64      gpu_buddy_free_size(const struct gpu_buddy*)
       - 无 malloc/free，无锁，无日志，无 C++ 特性

1.2.2  创建 libgpu_core/src/buddy.c
       将当前 BuddyAllocator 的算法逻辑移植为纯 C：
       - 移除 std::mutex（调用者负责同步）
       - 移除 std::cout（返回错误码）
       - 移除 std::map（用固定数组 + 链表）
       - 保留 buddy 算法（分块、合并、幂次分配）

1.2.3  创建 libgpu_core/test/test_buddy.c
       独立的 C 测试，不依赖 GpgpuDevice：
       - test_buddy_init_free()
       - test_buddy_alloc_small()
       - test_buddy_alloc_large()
       - test_buddy_coalesce()
       - test_buddy_oom()

1.2.4  创建 sim/buddy_allocator.cpp（封装层）
       C++ 包装类 SimBuddyAllocator：
       - 内部持有 struct gpu_buddy 实例
       - 调用 gpu_buddy_* C 函数
       - 提供锁保护（因为调用者不负责锁）
       - 提供日志输出（因为 C 核心不输出日志）

1.2.5  验证
       - libgpu_core 独立编译为静态库
       - test_buddy.c 全部通过
       - sim/buddy_allocator 通过 HAL 集成测试
```

**预期产出**: libgpu_core/ 4 个文件（.h/.c/test/CMakeLists），sim/ 封装 1 个

---

### P1.3: HAL 接口实现（对应 ADR-023）

**核心思路**: 实现 ADR-023 v2 定义的 10 个 HAL 接口的用户态和 mock 版本。

#### 步骤

```
1.3.1  创建 hal/gpu_hal.h
        按照 ADR-023 v2 写入（10 个函数指针 + inline wrapper）

1.3.2  创建 hal/hal_user.cpp
        用户态仿真实现：
        - register_read/write    → 调用 DoorbellEmu / PcieEmu
        - mem_read/write         → 调用 sim 管理的设备内存
        - mem_alloc/mem_free     → 调用 SimBuddyAllocator（P1.2）
        - doorbell_ring          → 触发 DoorbellEmu → Puller 状态机
        - interrupt_raise        → 调用 callback 函数
        - fence_create/fence_read → 调用 sim/fence_sim
        - time_wait              → std::this_thread::sleep_for

1.3.3  创建 hal/hal_mock.cpp
       单元测试 mock：
       - 记录每次调用参数（供测试断言）
       - 预设返回值
       - 无实际硬件操作

1.3.4  创建 hal/test_hal.cpp
       HAL 接口独立测试：
       - 验证 user 实现的基本功能
       - 验证 mock 实现的调用记录
       - 验证 inline wrapper 零开销编译

1.3.5  验证
       - 构造 GpgpuDevice 时可注入 user_hal 或 mock_hal
       - 两种实现下 ioctl 测试都能通过（mock 绕过仿真直接返回预设值）
```

**预期产出**: hal/ 3 个文件（.h / user.cpp / mock.cpp）+ 测试

---

### P1.4: DRM ioctl 表驱动（对应 ADR-019）

**核心思路**: 将手写 switch-case 替换为 DRM 风格的 `drm_ioctl_desc` 数组。

**前置条件**: P0.5 (linux_compat/drm/ 头文件) 必须已完成。

#### 步骤

```
1.4.1  创建 drv/gpu_drm_driver.cpp
        定义 + 实现 drm_ioctl_wrapper()：
        - 遍历 gpu_ioctls[] 数组匹配 cmd
        - 验证 flags（检查 DRM_RENDER_ALLOW）
        - 调用 handler(argp)
        - 返回 handler 的 long 结果
        定义：
        - DRM 风格 const struct drm_ioctl_desc gpu_ioctls[]
        - struct drm_driver gpu_drm_driver.fops 指向自己的 file_operations

1.4.2  创建 drv/gpu_gem_object.cpp
       GEM 对象骨架：
       - drm_gem_object 生命周期（alloc/free refcount）
       - GEM handle 管理（handle → object 映射）
       - mmap 回调（将模拟 VRAM 映射到用户空间）
       - prime_export/import 返回 -ENOSYS（Phase 2 实现）

1.4.3  修改 drv/gpgpu_device.cpp
       - 移除手写 switch(cmd) { case ... }
       - 改为调用 drm_ioctl_wrapper(gpu_ioctls, cmd, argp)
       - 保持每个 ioctl handler 的函数签名不变

1.4.4  验证
       - 所有 ioctl 行为与拆分前完全一致
       - drm_ioctl_desc 数组可以在复制到内核后直接使用
```

**预期产出**: drv/ 2 个新文件，1 个修改

---

### P1.5: Hardware Puller 状态机（对应 ADR-021）

**核心思路**: 实现 Puller 状态机的核心状态（IDLE → FETCH → DECODE → ISSUE → COMPLETE），替代当前的 switch 循环。

**关键约束**:
- GPFIFO entries 写到设备内存（通过 HAL mem_alloc 分配临时 buffer + mem_write 写入）
- fence_id 通过 HAL fence_create 预分配后再触发 doorbell
- 当前快速路径（DECODE→ISSUE→COMPLETE 直接完成，不调度 GPU Core Emu）

#### 步骤

```
1.5.1  创建 sim/hardware/doorbell_emu.h/.cpp
       Doorbell 寄存器仿真：
       - 寄存器布局（BASE=0x1000, STRIDE=0x40, MAX=32）
       - write(queue_id) — 通过 HAL register_write 触发
       - poll(queue_id) — Puller 轮询检测

1.5.2  创建 sim/hardware/hardware_puller_emu.h/.cpp
       Puller 状态机（初始实现 IDLE→FETCH→DECODE→ISSUE→COMPLETE）：
       enum class PullerState {
           IDLE,       // 等待 doorbell
           FETCH,      // 从设备内存读取 GPFIFO entry
           DECODE,     // 解析 method + payload
           ISSUE,      // 分发给执行引擎
           COMPLETE    // 完成 + 可选中断
       };
       - 运行在独立的仿真线程
       - 接收 doorbell 触发后开始状态转换
       - 当前 DECODE→ISSUE→COMPLETE 为快速路径（不调度 GPU Core Emu，直接返回完成）

1.5.3  创建 sim/scheduler/global_scheduler.h/.cpp（骨架）
       简单 FIFO 调度器：
       - 接收 Puller 分发的已完成 entry
       - 按引擎类型路由（当前只有 "空操作" 引擎）
       - Phase 2 扩展为真实调度

1.5.4  修改 drv/gpgpu_device.cpp handle_pushbuffer_submit_batch
        从"就地 switch 处理"改为：
        - 通过 HAL mem_alloc 分配设备内存 buffer
        - 将 entries 写入该 buffer（通过 HAL mem_write）
        - 通过 HAL fence_create 预分配 fence_id（立即返回）
        - 触发 doorbell（通过 HAL doorbell_ring）
        - 等待 fence 完成（通过 HAL fence_read + time_wait）

1.5.5  验证
       - PUSHBUFFER_SUBMIT_BATCH 走完 Puller 状态机全路径
       - 无 TaskRunner 功能回退
       - 当前测试全部通过
```

**预期产出**: sim/hardware/ 2 个 + sim/scheduler/ 1 个

---

### P1.6: TaskRunner 联调验证（对应 sync-plan）

**核心思路**: 验证 TaskRunner 能通过完整的 System C 路径与 UsrLinuxEmu 交互。

#### 步骤

```
1.6.1  验证符号链接
       - tools/verify_symlinks.sh PASS
       - TaskRunner 能正常编译 shared/ 头文件

1.6.2  构建联调环境
       - 编译 UsrLinuxEmu（Phase 1 版本）
       - 编译 TaskRunner CLI
       - usrlinuxemu/build/plugins/gpu_driver/libgpu_driver_plugin.so 存在

1.6.3  运行 TaskRunner CLI 命令
       - taskrunner cuda_alloc 4096 → 返回 GPU handle
       - taskrunner cuda_memcpy h2d <handle> 0 4096 → 返回 fence_id
       - taskrunner cuda_launch 0 1,1,1 1,1,1 → 返回 fence_id
       - taskrunner cuda_wait <fence_id> → 返回 0（成功）

1.6.4  验证零耦合
       - ldd libgpu_driver_plugin.so | grep taskrunner → 空
       - 断开符号链接 → cmake 报错退出

1.6.5  运行 test_portability.sh（初始版）
       - 先检查 5 条核心规则（1/2/3/6/8）
```

**预期产出**: 联调测试报告 + test_portability.sh v1

---

## 依赖关系

```
P0.5 (linux_compat/drm/) ← 前置条件
  │
  ├── P1.2 (libgpu_core)    ← 无外部依赖，可最先开始
  │
  ├── P1.1a (物理拆分)      ← 无外部依赖，可并行
  │
  ├── P1.3 (HAL)            ← 依赖 P1.2（mem_alloc/mem_free 需要 SimBuddyAllocator）
  │
  ├── P1.4 (DRM骨架)        ← 依赖 P0.5 + P1.3
  │
  └── P1.5 (Puller)         ← 依赖 P1.3（fence_create/doorbell/HAL 全部到位）
        │
        └── P1.6 (联调)     ← 依赖 P1.4 + P1.5

P1.1b (切换薄入口) ← 必须等 P1.2+P1.3+P1.4+P1.5 全部就绪
```

**关键变化**（对比 Oracle 审查前）：
- P1.1 拆为 P1.1a（物理拆分，随时可做）和 P1.1b（切换薄入口，必须等全部就绪）
- P0.5 新增为 P1.4 的前置条件
- P1.2 现在是 P1.3 的前置（HAL 的 mem_alloc/mem_free 需要 SimBuddyAllocator）

---

## 建议的实施顺序

```
Week 1-2         Week 3-4             Week 5-6
───────────      ───────────          ───────────
P0.5 drm 头文件   P1.3 HAL 实现         P1.5 Puller 状态机
P1.2 libgpu_core  P1.4 DRM 骨架         P1.1b 切换薄入口
P1.1a 物理拆分     └── 集成测试           P1.6 联调验证
                 │                      └── ADR-022/024/025/027 讨论
                 └── P1.1a 验证（影子编译）
```

**Week 1 可并行启动的任务**: P0.5、P1.2、P1.1a

---

**维护者**: UsrLinuxEmu Architecture Team

**最后更新**: 2026-05-07
