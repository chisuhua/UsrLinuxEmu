# UsrLinuxEmu 审计后综合执行计划 (2026-05-08)

> **文档状态**: 活跃（基于 2026-05-08 审计结果）
> **创建日期**: 2026-05-08
> **维护者**: Prometheus (Plan Builder)
>
> 本计划整合 8 份现有计划文件、7 次审计提交记录和当前代码状态，
> 产出一份唯一、可执行的路线图。

---

## 一、现有计划状态总览

### 1.1 计划文件清单与状态

| 计划文件 | 状态 | 处理方式 | 原因 |
|---------|------|---------|------|
| `plans/master_plan_2026.md` | 已废弃 | **归档到 `docs/archive/planning/`** | 文档自述已废弃，被 `gpu_driver_portability_plan.md` + `docs/PRD.md` 取代 |
| `plans/phase1_implementation_plan.md` | 部分完成 | **保留并更新** | P0.5~P1.3 骨架已创建（commit 3），P1.1b 薄入口切换、P1.4~P1.6 待完成 |
| `plans/gpu_driver_portability_plan.md` | 活跃 | **保留为顶层路线图** | 三阶段规划仍有效，但需标注已完成项 |
| `plans/sync-plan.md` | 需刷新 | **保留并更新状态列** | S0/S1/S2 已完成（commit 2），S3/S4/S5 待更新 |
| `plans/linux_compat_plan.md` | 待 ADR-027 | **保留为技术参考** | 细节内容有效，顶层决策等 ADR-027 |
| `.sisyphus/plans/code-quality-improvement.md` | 部分完成 | **保留并更新** | Phase A(配置)、B(格式化) 已完成；Phase C(命名空间)、D(plugin 拆分)、E(清理) 待执行 |
| `.sisyphus/plans/fix-test-infra.md` | 已完成 | **归档标记** | 测试已全部通过（19/19），无需继续执行 |
| `.sisyphus/plans/doc-cleanup-plan.md` | 已完成 | **归档标记** | 文档目录结构已按此计划重组（docs/01-quickstart/ ~ 07-integration/） |

### 1.2 审计提交已实现的内容

| 提交 | 内容 | 覆盖的原计划任务 |
|------|------|-----------------|
| Commit 1 (d13b1ab) | `.clang-format`, `.clang-tidy`, `.editorconfig` | `code-quality-improvement.md` Phase A ✅ |
| Commit 2 (e9eff35) | 14 个测试文件迁移到 System C + `dev.reset()` + `entries_addr` | `fix-test-infra.md` Phase C/D/E ✅ |
| Commit 3 (d2399fb) | HAL/DRV/SIM 骨架 + `libgpu_core` + DRM compat 头文件 | `phase1_implementation_plan.md` P0.5+P1.1a+P1.2+P1.3 骨架 ✅ |
| Commit 4 (487f2b9) | ADR-018~023, PRD, sync-plan, Sisyphus plans | 文档产出 ✅ |
| Commit 5 (30daa4d) | TaskRunner submodule, ADR README, libgpu_core→CMake | 基础设施 ✅ |
| Commit 6 (a8d4bad) | `clang-format` 应用 103 文件 | `code-quality-improvement.md` Phase B ✅ |
| Commit 7 (8a42a13) | `.gitignore` 更新 | 杂项 ✅ |

---

## 二、综合任务表

### 2.1 短周期任务（1-2 天）

| # | 任务 | 优先级 | 状态 | 依赖 | 涉及文件 | 预计工时 |
|---|------|--------|------|------|---------|---------|
| T1 | **删除孤儿文件** `src/kernel/device/cuda_compat_ioctl.cpp` | 🔴 高 | 待执行 | 无 | `src/kernel/device/cuda_compat_ioctl.cpp` | 0.5h |
| T2 | **ServiceRegistry 决策与解耦** | 🔴 高 | 待决策 | 无 | `src/kernel/vfs.cpp`, `include/kernel/service_registry.h` | 2h |
| T3 | **更新 `sync-plan.md` 状态列** | 🟡 中 | 待执行 | 无 | `plans/sync-plan.md` | 0.5h |
| T4 | **归档已废弃/完成的计划文件** | 🟡 中 | 待执行 | 无 | `plans/master_plan_2026.md`, `.sisyphus/plans/*.md` | 0.5h |

### 2.2 中周期任务（1-2 周）

| # | 任务 | 优先级 | 状态 | 依赖 | 涉及文件 | 预计工时 |
|---|------|--------|------|------|---------|---------|
| T5 | **HAL 集成到 `plugin.cpp`**（ADR-023） | 🔴 高 | 待执行 | T2 完成（VFS 稳定） | `plugins/gpu_driver/plugin.cpp`, `plugins/gpu_driver/hal/hal_user.cpp`, `plugins/gpu_driver/drv/gpgpu_device.cpp` | 3-4d |
| T6 | **DRV 层薄入口切换**（P1.1b） | 🔴 高 | 待执行 | T5 完成 | `plugins/gpu_driver/plugin.cpp`, `plugins/CMakeLists.txt` | 1d |
| T7 | **SIM 层骨架填充**（buddy/fence/puller） | 🟡 中 | 待执行 | T5 完成 | `plugins/gpu_driver/sim/*.cpp` | 2-3d |
| T8 | **kernel 层命名空间迁移**（Phase C） | 🟡 中 | 待执行 | T6 完成（plugin 稳定后） | `include/kernel/*.h`, `src/kernel/*.cpp` | 2d |
| T9 | **DRM ioctl 表驱动**（P1.4） | 🟡 中 | 待执行 | T6 完成 | `plugins/gpu_driver/drv/gpu_drm_driver.cpp` | 1-2d |
| T10 | **Puller 状态机实现**（P1.5） | 🟢 低 | 待执行 | T7 完成 | `plugins/gpu_driver/sim/hardware/hardware_puller_emu.cpp` | 2-3d |

### 2.3 长周期任务（2-4 周，Phase 2 准备）

| # | 任务 | 优先级 | 状态 | 依赖 | 涉及文件 | 预计工时 |
|---|------|--------|------|------|---------|---------|
| T11 | **ADR-022/024/025/027 讨论与写入** | 🟡 中 | 待启动 | T9+T10 完成 | `docs/00_adr/` | 1-2w |
| T12 | **TaskRunner 端到端联调**（S4） | 🔴 高 | 待执行 | T6 完成 | `tests/`, TaskRunner CLI | 2-3d |
| T13 | **`test_portability.sh` 初始检查** | 🟡 中 | 待执行 | T8 完成 | `tools/test_portability.sh` | 1d |

---

## 三、详细任务说明

### T1: 删除孤儿文件 `cuda_compat_ioctl.cpp`

**现状**: `src/kernel/device/cuda_compat_ioctl.cpp` (9445 字节) 不在任何 `CMakeLists.txt` 中，零编译引用，零运行时引用。

**操作**:
```bash
git rm src/kernel/device/cuda_compat_ioctl.cpp
```

**验证**:
```bash
grep -r "cuda_compat_ioctl" CMakeLists.txt src/CMakeLists.txt plugins/CMakeLists.txt tests/CMakeLists.txt
# 预期：零匹配
git status
# 预期：显示删除记录
```

**风险**: 极低。文件未被任何构建系统引用。

---

### T2: ServiceRegistry 决策与解耦

**现状分析**:
```bash
grep -r "lookup_service\|register_service" src/ tests/ drivers/ plugins/ --include="*.cpp" --include="*.h"
```
结果：仅 `vfs.cpp` 调用 `register_service`/`unregister_service`，**`lookup_service` 零外部调用者**。

**决策**: 采用 **"保留空壳 + 完全去耦合"** 策略（选项 B）

**理由**:
1. `lookup_service` 无外部调用者 → 删除类无功能损失
2. 但完全删除类文件会引入不必要的 git 噪音和潜在的历史追溯困难
3. 保留空壳为未来可能的设备发现/服务网格功能预留扩展点

**操作步骤**:

1. **从 `vfs.cpp` 移除 ServiceRegistry 调用**:
   ```cpp
   // src/kernel/vfs.cpp 中 register_device() 方法
   // 删除：ServiceRegistry::instance().register_service(dev->name, dev);
   // unregister_device() 方法中删除对应 unregister_service 调用
   ```

2. **保留 `service_registry.h/.cpp` 文件**但标记为 "保留供将来使用"：
   ```cpp
   // 在 service_registry.h 文件头添加注释
   // NOTE: ServiceRegistry 当前未被 VFS 或其他组件使用。
   // 保留作为未来服务发现功能的扩展点。
   ```

3. **验证**:
   ```bash
   make -j4 && ctest --output-on-failure
   grep -r "ServiceRegistry" src/kernel/vfs.cpp
   # 预期：零匹配
   ```

**风险**: 低。仅删除交叉调用，不改变类定义。

---

### T3: 更新 `sync-plan.md`

**需要更新的状态**:

| 同步点 | 当前状态（文档写） | 实际状态（审计） | 更新内容 |
|--------|-------------------|-----------------|---------|
| S0 | ⬜ 待执行 | ✅ 已完成 | TaskRunner submodule 已添加，符号链接已确认 |
| S1 | ⬜ 待实现 | ✅ 已完成 | `GPU_IOCTL_GET_DEVICE_INFO` 已定义并实现 |
| S2 | ⬜ 待实现 | ✅ 已完成 | `GPU_IOCTL_ALLOC_BO/FREE_BO/MAP_BO` 已实现 |
| S3 | ⬜ 待实现 | 🔄 进行中 | `GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH` 已实现，但 `entries` 已改为 `entries_addr`，需记录 ABI 变更 |
| S4 | ⬜ 待实现 | ⏸️ 未开始 | 等待 T6（薄入口切换）完成后启动 |
| S5 | ⬜ 待实现 | ⏸️ 未开始 | 等待 Phase 2 |

---

### T4: 归档已废弃/完成的计划

**操作**:

1. **移动 `master_plan_2026.md` 到归档**:
   ```bash
   mkdir -p docs/archive/planning
   git mv plans/master_plan_2026.md docs/archive/planning/
   ```

2. **在 `plans/README.md` 更新索引**（添加归档区）。

3. **标记 `.sisyphus/plans/fix-test-infra.md` 为完成**:
   在文件头添加：
   ```markdown
   > **状态**: ✅ 已完成（2026-05-08）
   > 测试通过率：19/19（100%）
   ```

4. **标记 `.sisyphus/plans/doc-cleanup-plan.md` 为完成**:
   在文件头添加：
   ```markdown
   > **状态**: ✅ 已完成（2026-05-08）
   > 文档已按本计划重组到 docs/01-quickstart/ ~ 07-integration/
   ```

---

### T5: HAL 集成到 `plugin.cpp`（核心任务）

**现状**:
- `plugin.cpp` (656 行) 内联实现了 `BuddyAllocator` (339 行)、`HandleManager` (31 行)、`GpgpuDevice` (243 行)
- `hal_user.cpp` (145 行) 已实现完整的 10 个 HAL 函数，但**未连接到 plugin.cpp**
- `drv/gpgpu_device.cpp` 存在但几乎是空壳（364 字节）

**目标**: 让 `GpgpuDevice` 通过 HAL 接口访问硬件功能，而非直接内联实现。

**实施步骤**:

1. **在 `plugin.cpp` 中引入 HAL 头文件并构造 HAL 上下文**:
   ```cpp
   #include "hal/hal_user.h"
   
   // 在 GpgpuDevice 中添加 hal 成员
   struct gpu_hal_ops hal_;
   struct hal_user_context hal_ctx_;
   
   // 构造函数中初始化
   GpgpuDevice() {
     hal_user_init(&hal_, &hal_ctx_);
     // ... 其余初始化
   }
   ```

2. **将 `handle_alloc_bo` 改为通过 HAL 分配**:
   ```cpp
   long handle_alloc_bo(void* argp) {
     // ... 参数检查 ...
     uint64_t gpu_va = 0;
     int ret = hal_.mem_alloc(hal_.ctx, args->size, &gpu_va);
     if (ret != 0) return -ENOMEM;
     // ... 其余逻辑不变 ...
   }
   ```

3. **保留 `BuddyAllocator` 但标记为 legacy**，或移除并完全依赖 HAL 的 `gpu_buddy_*`。
   **推荐**: 保留 `BuddyAllocator` 作为 `sim/buddy_allocator.cpp` 的 C++ 封装，但让 `GpgpuDevice` 优先使用 HAL。

4. **验证**:
   ```bash
   make -j4 && ctest --output-on-failure
   # 所有 19 个测试通过
   # plugin.cpp 仍能通过 GPU_IOCTL_ALLOC_BO 等测试
   ```

**风险**: 中。涉及核心插件内存分配路径。建议保留 BuddyAllocator 作为 fallback。

---

### T6: DRV 层薄入口切换（P1.1b）

**目标**: 将 `plugin.cpp` 从 656 行内联实现变为 ~40 行薄入口，驱动逻辑迁移到 `drv/gpgpu_device.cpp`。

**前提**: T5 完成（HAL 集成验证通过）。

**操作**:

1. **将 `GpgpuDevice` 类完整迁移到 `drv/gpgpu_device.cpp/.h`**。
2. **将 `BuddyAllocator`、`HandleManager` 迁移到 `sim/` 对应文件**（或复用已有骨架）。
3. **重写 `plugin.cpp` 为薄入口**:
   ```cpp
   #include "drv/gpgpu_device.h"
   #include "hal/hal_user.h"
   #include "kernel/vfs.h"
   #include "kernel/module_loader.h"
   
   static std::shared_ptr<GpgpuDevice> g_device;
   
   static int plugin_init_internal() {
     g_device = std::make_shared<GpgpuDevice>();
     VFS::instance().register_device(
         std::make_shared<Device>(g_device->name(), 0, g_device, nullptr));
     return 0;
   }
   
   static void plugin_fini_internal() {
     VFS::instance().unregister_device("gpgpu0");
     g_device.reset();
   }
   
   extern "C" module mod = { /* ... */ };
   ```

4. **更新 `plugins/CMakeLists.txt`**：移除旧 `plugin.cpp` 的单独编译目标，改用新架构。

**验证**:
```bash
make -j4 && ctest --output-on-failure
wc -l plugins/gpu_driver/plugin.cpp
# 预期 < 50 行
```

---

### T7: SIM 层骨架填充

**现状**: `sim/` 目录已有文件但多为空壳或极简单实现：
- `buddy_allocator.cpp` (1145 字节) — 有内容，可能是 C++ 封装
- `fence_sim.cpp` (649 字节) — 有基础实现
- `doorbell_emu.cpp` (119 字节) — 空壳
- `hardware_puller_emu.cpp` (128 字节) — 空壳

**目标**: 让这些文件成为真正能工作的仿真组件。

**操作**:
1. 填充 `doorbell_emu.cpp`：实现寄存器布局（BASE=0x1000, STRIDE=0x40）和 `write()`/`poll()`。
2. 填充 `hardware_puller_emu.cpp`：实现 IDLE→FETCH→DECODE→ISSUE→COMPLETE 状态机骨架。
3. 验证 `buddy_allocator.cpp` 是否正确封装了 `libgpu_core` 的 C 接口。
4. 确保 `fence_sim.cpp` 能被 HAL 的 `fence_create`/`fence_read` 调用。

**与 T5 的关系**: T5 让 HAL 调用这些组件，T7 让这些组件真正有功能。

---

### T8: kernel 层命名空间迁移（code-quality Phase C）

**目标**: 将 `include/kernel/` 和 `src/kernel/` 的类封装到 `usr_linux_emu` 命名空间。

**操作**:
1. 在核心头文件添加 `namespace usr_linux_emu { ... }`。
2. 在实现文件添加对应命名空间。
3. 在 `plugin.cpp`、`tests/`、`drivers/` 等引用处添加 `using namespace usr_linux_emu;` 作为过渡。

**时机**: 建议在 T6（薄入口切换）完成后执行，因为此时 `plugin.cpp` 行数最少，变更冲突最少。

---

### T9: DRM ioctl 表驱动（P1.4）

**目标**: 将 `GpgpuDevice::ioctl()` 中的手写 `switch-case` 替换为 `drm_ioctl_desc` 数组驱动。

**前提**: T6 完成（`drv/gpgpu_device.cpp` 已独立）。

**操作**:
1. 创建 `drv/gpu_drm_driver.cpp`，定义 `gpu_ioctls[]` 数组。
2. 修改 `gpgpu_device.cpp`，移除 `switch`，调用 `drm_ioctl_wrapper()`。
3. 验证所有 ioctl 行为与修改前完全一致。

---

### T10: Puller 状态机实现（P1.5）

**目标**: 让 `hardware_puller_emu.cpp` 实现完整的状态转换。

**前提**: T7 完成（doorbell/poller 可用）。

**操作**:
1. 实现 `PullerState` 枚举和状态转换函数。
2. 在 `handle_pushbuffer_submit_batch` 中：分配 buffer → mem_write → fence_create → doorbell_ring → fence_read。
3. 创建 `sim/scheduler/global_scheduler.cpp` FIFO 调度器骨架。

---

### T11-T13: Phase 2 准备与联调

这些任务依赖 Phase 1 全部完成，详见 `gpu_driver_portability_plan.md` §3-4。

---

## 四、执行顺序与并行机会

### 4.1 依赖关系图

```
T1 (删除孤儿文件) ──┐
T2 (ServiceRegistry)─┤
T3 (更新 sync-plan)  │  可并行启动
T4 (归档计划)       ─┘
         │
         ▼
T5 (HAL 集成) ─────────┐
         │             │
         ▼             │
T6 (薄入口切换) ◄──────┘  T5 完成后
         │
    ┌────┴────┐
    ▼         ▼
T7 (SIM 填充)  T8 (命名空间)   可并行
    │         │
    ▼         ▼
T9 (DRM 表)   T12 (TaskRunner 联调)  T8 完成后可启动 T12
    │
    ▼
T10 (Puller 状态机)
    │
    ▼
T11 (ADR-022/024/025/027 讨论)
```

### 4.2 推荐执行时序

| 周次 | 主要任务 | 并行任务 |
|------|---------|---------|
| **Day 1** | T1 + T2 + T3 + T4（清理日） | 全部并行 |
| **Day 2-3** | T5（HAL 集成） | — |
| **Day 4** | T6（薄入口切换） | — |
| **Day 5-6** | T7（SIM 填充） | T8（命名空间） |
| **Day 7-8** | T9（DRM 表） | T12（TaskRunner 联调准备） |
| **Day 9-10** | T10（Puller 状态机） | T13（test_portability.sh） |
| **Week 3+** | T11（Phase 2 ADR 讨论） | — |

---

## 五、需要丢弃的内容

### 5.1 已完全过时的计划

| 内容 | 丢弃原因 | 替代来源 |
|------|---------|---------|
| `master_plan_2026.md` 全部 GPU 里程碑 | 被 `gpu_driver_portability_plan.md` + `docs/PRD.md` 取代 | `gpu_driver_portability_plan.md` |
| `fix-test-infra.md` 全部修复步骤 | 测试已全部通过（19/19） | 无需替代 |
| `doc-cleanup-plan.md` 全部重组步骤 | 文档已按此计划重组完成 | 无需替代 |
| `code-quality-improvement.md` Phase A+B | 已完成（clang-format 已应用） | 继续执行 Phase C~E |

### 5.2 建议归档的文件清单

```bash
# 执行 T4 时操作
git mv plans/master_plan_2026.md docs/archive/planning/

# 在以下文件头添加完成标记（不移动，保留在原地供参考）：
# .sisyphus/plans/fix-test-infra.md
# .sisyphus/plans/doc-cleanup-plan.md
```

---

## 六、需要合并/更新的内容

### 6.1 计划合并策略

| 源计划 | 目标 | 合并方式 |
|--------|------|---------|
| `phase1_implementation_plan.md` | `gpu_driver_portability_plan.md` | 将 P0.5~P1.6 作为 Phase 1 的子任务表嵌入，删除独立文件（或标记为"已合并到全局计划"） |
| `sync-plan.md` | `gpu_driver_portability_plan.md` | 在全局计划 §8 中维护同步点状态，sync-plan.md 保留为详细契约 |
| `code-quality-improvement.md` Phase C~E | 本执行计划 T8 | 将命名空间/plugin 拆分/清理任务纳入本计划任务表 |

### 6.2 建议的文件重组

```
plans/
├── README.md                           # 更新：添加本计划为"当前活跃主计划"
├── audit-execution-plan-2026.md        # ← 本文件
├── gpu_driver_portability_plan.md      # 保留：顶层三阶段路线图
├── sync-plan.md                        # 保留：TaskRunner 详细契约（更新状态列）
├── phase1_implementation_plan.md       # 标记为"已合并到 audit-execution-plan-2026.md"
├── linux_compat_plan.md                # 保留：技术参考（等 ADR-027）
└── phase0_env_setup.md                 # 已标记完成

.sisyphus/plans/
├── code-quality-improvement.md         # 更新：仅保留 Phase C~E
├── fix-test-infra.md                   # 标记完成
└── doc-cleanup-plan.md                 # 标记完成

docs/archive/planning/
└── master_plan_2026.md                 # 从 plans/ 移入
```

---

## 七、新发现项（原计划和审计未覆盖）

### 7.1 代码层面

| # | 发现 | 严重度 | 建议行动 |
|---|------|--------|---------|
| N1 | `plugin.cpp` 中 `GPU_OP_LAUNCH_CPU_TASK` 仍未处理（被 `default` 分支捕获） | 低 | 在 T5/T6 期间添加骨架处理分支，打印日志，Phase 2 实现回调 |
| N2 | `hal_user.cpp` 使用 `malloc`/`free`（第 125、143 行），与 ADR-020 "无 malloc/free" 约束冲突 | 中 | 改用 `new uint8_t[HAL_HEAP_SIZE]` / `delete[]`，或从 `libgpu_core` 申请堆内存 |
| N3 | `drv/gpgpu_device.cpp` 仅 364 字节，骨架完成度极低 | 低 | 在 T6 期间完整填充 |
| N4 | `sim/hardware_puller_emu.cpp` (128 字节) 和 `doorbell_emu.cpp` (119 字节) 几乎是空文件 | 低 | 在 T7 期间填充 |

### 7.2 计划层面

| # | 发现 | 建议 |
|---|------|------|
| N5 | 缺少 `plugin.cpp` → HAL → SIM 的集成测试 | 在 T5 完成后添加 `test_hal_integration.cpp`，验证 HAL 每个函数指针非空且可调用 |
| N6 | `test_portability.sh` 尚未创建 | T13 需要创建此脚本，初始检查 5 条规则（来自 `gpu_driver_portability_plan.md` §2.3） |
| N7 | ADR-022/024/025/027 无预定讨论时间表 | 建议在 T10 完成后（~Day 10）启动第一个 ADR-022 讨论 |

---

## 八、成功标准

本计划全部执行完毕时，项目应满足：

### 8.1 构建与测试
- [ ] 27/27 构建目标成功
- [ ] 19/19 测试通过
- [ ] `cuda_compat_ioctl.cpp` 不存在于仓库中
- [ ] `git grep "ServiceRegistry" src/kernel/vfs.cpp` 零匹配

### 8.2 架构质量
- [ ] `plugin.cpp` 行数 < 50（薄入口）
- [ ] `GpgpuDevice` 位于 `drv/gpgpu_device.cpp`
- [ ] HAL 10 个函数全部被 `plugin.cpp` → `GpgpuDevice` 调用链使用
- [ ] `sim/` 层组件（buddy/fence/doorbell/puller）有实质实现

### 8.3 文档与计划
- [ ] 所有计划文件状态标记正确
- [ ] `sync-plan.md` S0-S3 标记为完成
- [ ] 本计划 `audit-execution-plan-2026.md` 所有任务完成或移交到下一阶段计划

---

## 九、风险与缓解

| 风险 | 影响 | 概率 | 缓解措施 |
|------|------|------|---------|
| T5 HAL 集成破坏现有测试 | 高 | 中 | 每修改一个 HAL 函数就运行 `ctest -R gpu_ioctl`；保留 BuddyAllocator 作为 fallback |
| T6 薄入口切换引入符号可见性问题 | 高 | 中 | 确保 `plugin.cpp` 正确 `#include` 新头文件；检查 CMakeLists.txt 源文件列表 |
| T2 ServiceRegistry 移除后发现隐藏调用者 | 中 | 低 | 全局 grep 确认（已在审计中完成） |
| T11 ADR 讨论耗时过长 | 中 | 高 | 每个 ADR 限制 1 次讨论 + 1 次写入，超时按建议方案推进 |

---

## 十、附录：计划文件交叉索引

| 本计划任务 | 源计划参考 | 源计划章节 |
|-----------|-----------|-----------|
| T1 | `code-quality-improvement.md` | Phase E.1 |
| T2 | `code-quality-improvement.md` | Phase E.2 |
| T3 | `sync-plan.md` | §6.1 |
| T5 | `phase1_implementation_plan.md` | P1.3 |
| T6 | `phase1_implementation_plan.md` | P1.1b |
| T7 | `phase1_implementation_plan.md` | P1.2 + P1.5 |
| T8 | `code-quality-improvement.md` | Phase C |
| T9 | `phase1_implementation_plan.md` | P1.4 |
| T10 | `phase1_implementation_plan.md` | P1.5 |
| T11 | `gpu_driver_portability_plan.md` | §3.1 |
| T12 | `phase1_implementation_plan.md` | P1.6 |
| T13 | `gpu_driver_portability_plan.md` | P3.2 |

---

**审批**: Prometheus (Plan Builder)
**下次审查**: 每个任务完成后更新进度
